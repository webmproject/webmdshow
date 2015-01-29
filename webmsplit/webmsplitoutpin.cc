// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include <comdef.h>
#include <uuids.h>
#include "mkvparserstream.h"
#include "webmsplitfilter.h"
#include "webmsplitoutpin.h"
#include "cmediasample.h"
#include "mkvparser.hpp"
#include <vfwmsgs.h>
#include <cassert>
#include <sstream>
#include <iomanip>
#include <process.h>
#ifdef _DEBUG
#include "odbgstream.h"
#include "iidstr.h"
using std::endl;
using std::dec;
using std::hex;
#endif


namespace WebmSplit
{


Outpin::Outpin(
    Filter* pFilter,
    mkvparser::Stream* pStream) :
    Pin(pFilter, PINDIR_OUTPUT, pStream->GetId().c_str()),
    m_pStream(pStream),
    m_hThread(0),
    m_cRef(0)
{
    m_pStream->GetMediaTypes(m_preferred_mtv);

    m_hStop = CreateEvent(0, 0, 0, 0);
    assert(m_hStop);  //TODO

    m_hNewCluster = CreateEvent(0, 0, 0, 0);
    assert(m_hNewCluster);  //TODO
}


Outpin::~Outpin()
{
    //odbgstream os;
    //os << "WebmSplit::outpin::dtor" << endl;

    assert(m_hThread == 0);
    assert(!bool(m_pAllocator));
    assert(!bool(m_pInputPin));

    Final();
}


Outpin* Outpin::Create(Filter* f, mkvparser::Stream* s)
{
    Outpin* const p = new (std::nothrow) Outpin(f, s);
    assert(p);  //TODO

    p->AddRef();

    return p;
}


ULONG Outpin::Destroy()
{
    Final();
    return Release();
}


HRESULT Outpin::Start()  //transition from stopped
{
    assert(m_hThread == 0);

    if (m_pPinConnection == 0)
        return S_FALSE;  //nothing we need to do

    assert(bool(m_pAllocator));
    assert(bool(m_pInputPin));

    const HRESULT hr = m_pAllocator->Commit();
    hr;
    assert(SUCCEEDED(hr));  //TODO

    StartThread();

    return S_OK;
}


void Outpin::Stop()  //transition to stopped
{
    if (m_pPinConnection == 0)
        return;  //nothing was done

    assert(bool(m_pAllocator));
    assert(bool(m_pInputPin));

    const HRESULT hr = m_pAllocator->Decommit();
    hr;
    assert(SUCCEEDED(hr));

    StopThread();

    if (m_pStream)
        m_pStream->Stop();
}


void Outpin::Final()
{
    //odbgstream os;
    //os << "WebmSplit::outpin::final" << endl;

    m_preferred_mtv.Clear();

    if (m_hStop)
    {
        const BOOL b = CloseHandle(m_hStop);
        b;
        assert(b);

        m_hStop = 0;
    }

    if (m_hNewCluster)
    {
        const BOOL b = CloseHandle(m_hNewCluster);
        b;
        assert(b);

        m_hNewCluster = 0;
    }

    delete m_pStream;
    m_pStream = 0;
}


void Outpin::StartThread()
{
    assert(m_hThread == 0);

    BOOL b = ResetEvent(m_hStop);
    assert(b);

    b = ResetEvent(m_hNewCluster);
    assert(b);

    const uintptr_t h = _beginthreadex(
                            0,  //security
                            0,  //stack size
                            &Outpin::ThreadProc,
                            this,
                            0,   //run immediately
                            0);  //thread id

    m_hThread = reinterpret_cast<HANDLE>(h);
    assert(m_hThread);
}


void Outpin::StopThread()
{
    if (m_hThread == 0)
        return;

    assert(bool(m_pPinConnection));

    BOOL b = SetEvent(m_hStop);
    assert(b);

    HRESULT hr = m_pPinConnection->BeginFlush();
    assert(SUCCEEDED(hr));

    const DWORD dw = WaitForSingleObject(m_hThread, INFINITE);
    dw;
    assert(dw == WAIT_OBJECT_0);

    b = CloseHandle(m_hThread);
    assert(b);

    m_hThread = 0;

    hr = m_pPinConnection->EndFlush();
    assert(SUCCEEDED(hr));

    b = ResetEvent(m_hStop);
    assert(b);
}


void Outpin::OnNewCluster()
{
    if (m_hThread == 0)
        return;

    const BOOL b = SetEvent(m_hNewCluster);
    b;
    assert(b);
}


HRESULT Outpin::QueryInterface(const IID& iid, void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == __uuidof(IUnknown))
        pUnk = static_cast<IPin*>(this);

    else if (iid == __uuidof(IPin))
        pUnk = static_cast<IPin*>(this);

    else if (iid == __uuidof(IMediaSeeking))
        pUnk = static_cast<IMediaSeeking*>(this);

    else
    {
#if 0
        wodbgstream os;
        os << "mkvsource::outpin::QI: iid=" << IIDStr(iid) << std::endl;
#endif
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG Outpin::AddRef()
{
    const ULONG cFilter = m_pFilter->AddRef();
    cFilter;

    const LONG cPin = InterlockedIncrement((LONG*)&m_cRef);

    //wodbgstream os;
    //os << "WebmSplit::outpin[" << m_id << "]::addref: cFilter=" << cFilter
    //   << " cOutpin=" << cPin
    //   << endl;

    return cPin;
}


ULONG Outpin::Release()
{
    //Case I:
    //If this is the last reference to the outpin and filter,
    //and the outpin ref and filter count will both be 1.
    //The filter will have already released its reference, and
    //it's just waiting to be deleted by whatever object holds
    //a reference to (just) the outpin.  Releasing the filter
    //will delete the filter object.
    //
    //Case II:
    //References to the filter exist, but this outpin is dormant.
    //Releasing the filter won't do anything at all.  But since
    //this is the last reference to the outpin, the outpin will
    //be deleted.
    //
    //Case III:
    //References to the filter exists, and the filter has its own
    //reference to the outpin (because the outpin is active, not
    //dormant).  This will delete neither the outpin nor the filter.

    assert(m_cRef > 0);

    //wodbgstream os;
    //os << "WebmSplit::outpin[" << m_id << "]::release(begin): cRef="
    //   << m_cRef
    //   << endl;

    //Here we (potentially) delete the outpin, before (potentially)
    //deleting the filter, so that filter resources will be available
    //to the outpin in its dtor.  (As of this writing we don't touch
    //the filter object in this way, but it doesn't hurt anything to
    //delete the outpin before deleting the filter.)
    //
    //This means that that once we decrement the outpin's refcount,
    //we cannot refer to it or any of its components.  In particular
    //that means we must copy the filter pointer onto the stack,
    //before we do anything else.

    Filter* const pFilter = m_pFilter;

    const LONG cPin = InterlockedDecrement((LONG*)&m_cRef);

    if (cPin == 0)
        delete this;

    //os << "WebmSplit::outpin::release(cont'd): releasing filter" << endl;

    const ULONG cFilter = pFilter->Release();
    cFilter;

    //os << "WebmSplit::outpin::release(end): cFilter="
    //   << cFilter
    //   << endl;

    return cPin;
}


#if 0
ULONG Outpin::Destroy()
{
    wodbgstream os;
    os << "WebmSplit::outpin[" << m_id << "]::destroy: cRef="
       << m_cRef
       << endl;

    Final();

    //NOTE: this negates the contribution to cRef made in ctor
    const LONG cPin = InterlockedDecrement((LONG*)&m_cRef);

    if (cPin > 0)
        return cPin;

    delete this;
    return 0;
}
#endif


HRESULT Outpin::Connect(
    IPin* pin,
    const AM_MEDIA_TYPE* pmt)
{
    if (pin == 0)
        return E_POINTER;

    GraphUtil::IMemInputPinPtr pInputPin;

    HRESULT hr = pin->QueryInterface(&pInputPin);

    if (hr != S_OK)
        return hr;

    Filter::Lock lock;

    hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_pStream == 0)
        return E_FAIL;

    if (m_pFilter->m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    if (bool(m_pPinConnection))
        return VFW_E_ALREADY_CONNECTED;

    m_connection_mtv.Clear();

    if (pmt)
    {
        hr = QueryAccept(pmt);

        if (hr != S_OK)
            return VFW_E_TYPE_NOT_ACCEPTED;

        hr = pin->ReceiveConnection(this, pmt);

        if (FAILED(hr))
            return hr;

        const AM_MEDIA_TYPE& mt = *pmt;

        hr = m_pStream->SetConnectionMediaType(mt);

        if (FAILED(hr))
            return VFW_E_TYPE_NOT_ACCEPTED;

        m_connection_mtv.Add(mt);
    }
    else
    {
        ULONG i = 0;
        const ULONG j = m_preferred_mtv.Size();

        while (i < j)
        {
            const AM_MEDIA_TYPE& mt = m_preferred_mtv[i];

            hr = pin->ReceiveConnection(this, &mt);

            if (SUCCEEDED(hr))
            {
                hr = m_pStream->SetConnectionMediaType(mt);

                if (SUCCEEDED(hr))
                    break;
            }

            ++i;
        }

        if (i >= j)
            return VFW_E_NO_ACCEPTABLE_TYPES;

        const AM_MEDIA_TYPE& mt = m_preferred_mtv[i];

        m_connection_mtv.Add(mt);
    }

    GraphUtil::IMemAllocatorPtr pAllocator;

    hr = pInputPin->GetAllocator(&pAllocator);

    if (FAILED(hr))
    {
        //hr = CMemAllocator::CreateInstance(&pAllocator);
        hr = CMediaSample::CreateAllocator(&pAllocator);

        if (FAILED(hr))
            return VFW_E_NO_ALLOCATOR;
    }

    assert(bool(pAllocator));

    ALLOCATOR_PROPERTIES props, actual;

    props.cBuffers = -1;    //number of buffers
    props.cbBuffer = -1;    //size of each buffer, excluding prefix
    props.cbAlign = -1;     //applies to prefix, too
    props.cbPrefix = -1;    //imediasample::getbuffer does NOT include prefix

    hr = pInputPin->GetAllocatorRequirements(&props);

    m_pStream->UpdateAllocatorProperties(props);

    hr = pAllocator->SetProperties(&props, &actual);

    if (FAILED(hr))
        return hr;

    hr = pInputPin->NotifyAllocator(pAllocator, 0);  //allow writes

    if (FAILED(hr) && (hr != E_NOTIMPL))
        return hr;

    m_pPinConnection = pin;
    m_pAllocator = pAllocator;
    m_pInputPin = pInputPin;

    return S_OK;
}


HRESULT Outpin::OnDisconnect()
{
    m_pInputPin = 0;
    m_pAllocator = 0;

    return S_OK;
}


HRESULT Outpin::ReceiveConnection(
    IPin*,
    const AM_MEDIA_TYPE*)
{
    return E_UNEXPECTED;  //for input pins only
}


HRESULT Outpin::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    return m_pStream->QueryAccept(pmt);
}


HRESULT Outpin::QueryInternalConnections(IPin** pa, ULONG* pn)
{
    if (pn == 0)
        return E_POINTER;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    //odbgstream os;
    //os << "WebmSplit::outpin: QueryInternalConnections" << endl;

    ULONG& n = *pn;

    if (n == 0)
    {
        if (pa == 0)  //query for required number
        {
            n = 1;
            return S_OK;
        }

        return S_FALSE;  //means "insufficient number of array elements"
    }

    if (n < 1)
    {
        n = 0;
        return S_FALSE;  //means "insufficient number of array elements"
    }

    if (pa == 0)
    {
        n = 0;
        return E_POINTER;
    }

    IPin*& pPin = pa[0];

    pPin = &m_pFilter->m_inpin;
    pPin->AddRef();

    n = 1;
    return S_OK;
}


HRESULT Outpin::EndOfStream()
{
    return E_UNEXPECTED;  //for inpins only
}


HRESULT Outpin::NewSegment(
    REFERENCE_TIME,
    REFERENCE_TIME,
    double)
{
    return E_UNEXPECTED;
}


HRESULT Outpin::BeginFlush()
{
    return E_UNEXPECTED;
}


HRESULT Outpin::EndFlush()
{
    return E_UNEXPECTED;
}


HRESULT Outpin::GetCapabilities(DWORD* pdw)
{
    if (pdw == 0)
        return E_POINTER;

    DWORD& dw = *pdw;

    dw = AM_SEEKING_CanSeekAbsolute
           | AM_SEEKING_CanSeekForwards
           | AM_SEEKING_CanSeekBackwards
           | AM_SEEKING_CanGetCurrentPos
           | AM_SEEKING_CanGetStopPos
           | AM_SEEKING_CanGetDuration;
           //AM_SEEKING_CanPlayBackwards
           //AM_SEEKING_CanDoSegments
           //AM_SEEKING_Source

    return S_OK;
}


HRESULT Outpin::CheckCapabilities(DWORD* pdw)
{
    if (pdw == 0)
        return E_POINTER;

    DWORD& dw = *pdw;

    const DWORD dwRequested = dw;

    if (dwRequested == 0)
        return E_INVALIDARG;

    DWORD dwActual;

    const HRESULT hr = GetCapabilities(&dwActual);
    hr;
    assert(SUCCEEDED(hr));
    assert(dw);

    dw &= dwActual;

    if (dw == 0)
        return E_FAIL;

    return (dw == dwRequested) ? S_OK : S_FALSE;
}


HRESULT Outpin::IsFormatSupported(const GUID* p)
{
    if (p == 0)
        return E_POINTER;

    const GUID& fmt = *p;

    if (fmt == TIME_FORMAT_MEDIA_TIME)
        return S_OK;

    //TODO
    //if (fmt != TIME_FORMAT_FRAME)
    //    return S_FALSE;

    return S_FALSE;
}


HRESULT Outpin::QueryPreferredFormat(GUID* p)
{
    if (p == 0)
        return E_POINTER;

    *p = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
}


HRESULT Outpin::GetTimeFormat(GUID* p)
{
    if (p == 0)
        return E_POINTER;

    *p = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
}


HRESULT Outpin::IsUsingTimeFormat(const GUID* p)
{
    if (p == 0)
        return E_INVALIDARG;

    const GUID& g = *p;

    if (g == TIME_FORMAT_MEDIA_TIME)
        return S_OK;

    return S_FALSE;
}


HRESULT Outpin::SetTimeFormat(const GUID* p)
{
    if (p == 0)
        return E_INVALIDARG;

    const GUID& g = *p;

    if (g == TIME_FORMAT_MEDIA_TIME)
        return S_OK;

    return E_INVALIDARG;
}


HRESULT Outpin::GetDuration(LONGLONG* p)
{
    if (p == 0)
        return E_POINTER;

    LONGLONG& reftime = *p;
    reftime = -1;

    Filter::Lock lock;

    const HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_pStream == 0)
        return E_FAIL;

    using namespace mkvparser;

    Segment* const pSegment = m_pStream->m_pTrack->m_pSegment;
    assert(pSegment);

    LONGLONG duration_ns = pSegment->GetDuration();

    if (duration_ns >= 0)  //actually have a duration in file
    {
        reftime = duration_ns / 100;
        return S_OK;
    }

    const Cues* const pCues = pSegment->GetCues();

    if (pSegment->DoneParsing() || !m_pFilter->InCache() || (pCues == 0))
    {
        const Cluster* const pCluster = pSegment->GetLast();

        if ((pCluster == 0) || pCluster->EOS())
        {
            reftime = 0;
            return S_OK;
        }

        duration_ns = pCluster->GetLastTime();
        assert(duration_ns >= 0);

        reftime = duration_ns / 100;

        return S_OK;
    }

    {
        while (!pCues->DoneParsing())
            pCues->LoadCuePoint();

        const CuePoint* const pCP = pCues->GetLast();
        assert(pCP);  //TODO

        const Tracks* const pTracks = pSegment->GetTracks();
        const ULONG count = pTracks->GetTracksCount();

        for (ULONG idx = 0; idx < count; ++idx)
        {
            const Track* const pTrack = pTracks->GetTrackByIndex(idx);

            if (pTrack == 0)
                continue;

            const CuePoint::TrackPosition* const pTP = pCP->Find(pTrack);

            if (pTP == 0)
                continue;

            const BlockEntry* const pBE = pCues->GetBlock(pCP, pTP);

            if ((pBE == 0) || pBE->EOS())
                continue;

            const Cluster* pCluster = pBE->GetCluster();
            assert(pCluster);
            assert(!pCluster->EOS());

            if (pCluster->GetIndex() >= 0)  //loaded
            {
                const Cluster* const p = pSegment->GetLast();
                assert(p);
                assert(p->GetIndex() >= 0);

                pCluster = p;
            }
            else //pre-loaded
            {
                for (int i = 0; i < 10; ++i)
                {
                    const Cluster* const p = pSegment->GetNext(pCluster);

                    if ((p == 0) || p->EOS())
                        break;

                    pCluster = p;
                }
            }

            duration_ns = pCluster->GetLastTime();
            assert(duration_ns >= 0);

            reftime = duration_ns / 100;  //reftime

            return S_OK;
        }
    }

    {
        const Cluster* const pCluster = pSegment->GetLast();

        if ((pCluster == 0) || pCluster->EOS())
        {
            reftime = 0;
            return S_OK;
        }

        duration_ns = pCluster->GetLastTime();
        assert(duration_ns >= 0);

        reftime = duration_ns / 100;

        return S_OK;
    }
}


HRESULT Outpin::GetStopPosition(LONGLONG* p)
{
    if (p == 0)
        return E_POINTER;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_pStream == 0)
        return E_FAIL;

    LONGLONG& pos = *p;
    pos = m_pStream->GetStopTime();

    if (pos < 0)  //means "use duration"
    {
        hr = GetDuration(&pos);

        if (FAILED(hr) || (pos < 0))
            return E_FAIL;  //?
    }

    return S_OK;
}


HRESULT Outpin::GetCurrentPosition(LONGLONG* p)
{
    if (p == 0)
        return E_POINTER;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_pStream == 0)
        return E_FAIL;

    LONGLONG& pos = *p;
    pos = m_pStream->GetCurrTime();

    if (pos < 0)  //means "use duration"
    {
        hr = GetDuration(&pos);

        if (FAILED(hr) || (pos < 0))
            return E_FAIL;
    }

    return S_OK;
}


HRESULT Outpin::ConvertTimeFormat(
    LONGLONG* ptgt,
    const GUID* ptgtfmt,
    LONGLONG src,
    const GUID* psrcfmt)
{
    if (ptgt == 0)
        return E_POINTER;

    LONGLONG& tgt = *ptgt;

    const GUID& tgtfmt = ptgtfmt ? *ptgtfmt : TIME_FORMAT_MEDIA_TIME;
    const GUID& srcfmt = psrcfmt ? *psrcfmt : TIME_FORMAT_MEDIA_TIME;

    if (tgtfmt != TIME_FORMAT_MEDIA_TIME)
        return E_INVALIDARG;

    if (srcfmt != TIME_FORMAT_MEDIA_TIME)
        return E_INVALIDARG;

    if (src < 0)
        return E_INVALIDARG;

    tgt = src;
    return S_OK;
}


HRESULT Outpin::SetPositions(
    LONGLONG* pCurr,
    DWORD dwCurr_,
    LONGLONG* pStop,
    DWORD dwStop_)
{
    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_pStream == 0)
        return E_FAIL;

#if 0 //def _DEBUG
    odbgstream os;
    os << "Outpin::SetPos(begin): pCurr="
       << dec << (pCurr ? *pCurr : -1)
       << " dwCurr=0x"
       << hex << dwCurr_
       << " pStop="
       << dec << (pStop ? *pStop : -1)
       << " dwStop=0x"
       << hex << dwStop_
       << endl;
#endif

    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const DWORD dwCurrPos = dwCurr_ & AM_SEEKING_PositioningBitsMask;
    const DWORD dwStopPos = dwStop_ & AM_SEEKING_PositioningBitsMask;

    if (dwCurrPos == AM_SEEKING_NoPositioning)
    {
        if (dwCurr_ & AM_SEEKING_ReturnTime)
        {
            if (pCurr == 0)
                return E_POINTER;

            *pCurr = m_pStream->GetCurrTime();

            if (*pCurr < 0)  //means "use duration"
            {
                hr = GetDuration(pCurr);

                if (FAILED(hr) || (*pCurr < 0))
                    *pCurr = 0;  //?
            }
        }

        if (dwStopPos == AM_SEEKING_NoPositioning)
        {
            if (dwStop_ & AM_SEEKING_ReturnTime)
            {
                if (pStop == 0)
                    return E_POINTER;

                *pStop = m_pStream->GetStopTime();

                if (*pStop < 0) //means "use duration"
                {
                    hr = GetDuration(pStop);

                    if (FAILED(hr) || (*pStop < 0))
                        *pStop = 0;  //?
                }
            }

            return S_FALSE;  //no position change
        }

        if (pStop == 0)
            return E_INVALIDARG;

        LONGLONG& tStop = *pStop;

        //TODO:
        //It makes sense to be able to adjust this during stop.
        //However, if we're paused/running, then the thread is either
        //still sending frames, or it has already sent EOS.  In the
        //former case, it makes sense to be able to adjust where
        //the running thread will stop.  But in the latter case,
        //the thread has already terminated, and it wouldn't
        //make any sense to restart the thread because there
        //would be a large time gap.

        m_pStream->SetStopPosition(tStop, dwStop_);

        if (dwStop_ & AM_SEEKING_ReturnTime)
        {
            tStop = m_pStream->GetStopTime();

            if (tStop < 0)  //means "use duration"
            {
                hr = GetDuration(&tStop);

                if (FAILED(hr) || (tStop < 0))
                    tStop = 0;  //?
            }
        }

        //TODO: You're supposed to return S_FALSE if there has
        //been no change in position.  Does changing only the stop
        //position count has having changed the position?

        return S_OK;
    }

    //Check for errors first, before changing any state.

    if (pCurr == 0)
        return E_INVALIDARG;

    switch (dwCurrPos)
    {
        case AM_SEEKING_IncrementalPositioning:
        default:
            return E_INVALIDARG;  //applies only to stop pos

        case AM_SEEKING_AbsolutePositioning:
        case AM_SEEKING_RelativePositioning:
            break;
    }

    if (dwStopPos == AM_SEEKING_NoPositioning)
    {
        if (((dwStop_ & AM_SEEKING_ReturnTime) != 0) && (pStop == 0))
            return E_POINTER;
    }
    else if (pStop == 0)
        return E_INVALIDARG;

    //TODO: this check is incorrect.  We have to allow a seek
    //to happen no matter what the filter state is.  We have to
    //something like this:
    // (1) m_connection->BeginFlush() and set hStop
    // (2) release filter lock and wait for thread to terminate
    // (3) seize filter lock and set posn(s)
    // (4) restart thread
    // (5) release filter lock
    //
    //Note that the streaming thread doesn't ever acquire the
    //filter lock -- that is only the case for the worker thread.
    //We just need to kill the streaming thread.  If it's blocked
    //on GetBuffer than BeginFlush will release samples.  If it's
    //blocked on Receive then Flush will reject receipt of sample.
    //So eventually the thread will terminate.  We can't hold
    //the filter lock while we're waiting for thread to terminate.
    //It's OK to release the filter lock, because it's the
    //FGM thread that is the caller.  The only thing we have to
    //worry about is the action of the worker thread.
    //
    //Stream::SetCurr(Stop)Position doesn't change the state
    //of what we have cached.  It's only the worker thread that
    //changes cache state (by loading clusters).  The problem occurs
    //when this sequence happens:
    //  (1) satisfy this set position, calculating base time
    //  (2) worker thread satisfies populate request from another
    //      running streaming thread, changing cache state
    //  (3) seek happens on next pin, but base time is different
    //      from the value calculated for this pin.
    //
    //Assuming this analysis is correct, it would be nice to kill
    //all of the streaming threads, seek all of the pins, and then
    //restart the pins.  Maybe what we can do is:
    //  (1) first pin to receive seek request kills all streaming threads
    //  (2) set position on all pins
    //  (3) restart streaming thread for this pin
    //  (4) as other pins are told to seek, they notice that a seek
    //      as occurred and re-starts its own streaming thread
    //
    //Note that is the filter state is stopped, there's nothing special
    //we need to do.  We're only figuring what to do when the filter
    //state is paused/running, when there are active streaming threads.
    //
    //How does a pin know whether it has been seeked?  If filter
    //state is paused/running, and there's no streaming thread active,
    //it would simply restart the thread.  The problem is that
    //a thread can be terminated for other reasons, such as having
    //reached end-of-stream.  So if the first pin to be seeked happens
    //to have its thread already terminated, we would falsely assume
    //that it had already been seeked and all we need to is restart
    //its streaming thread.
    //
    //The problem is that we want to guarantee that all pins have
    //the same base time.  We have been assuming that if the worker
    //thread manipulated the file after we release the filter lock
    //here, then that would change the base time.  The problem is
    //when Segment:::GetCluster is called -- whether or not it
    //returns NULL depends on how much is loaded.  Maybe we could
    //decide that:
    //  if segment->unparsed <= 0 then
    //     worker thread can't change cache state (because we're fully loaded)
    //     go ahead and return NULL when seek time >= duration
    //  else
    //     Never return NULL.  If time is large relative to existing cache,
    //     return value of last cluster loaded.
    //  end if
    //
    //In that case, there's nothing special we would need to do wrt
    //the threads.
    //
    //Another possibility is to have Segement::GetCluster return
    //a pseudo-cluster value (non-NULL) when the time is large relative
    //to the cache (or to the duration when fully loaded).  This
    //nonce-value would always mean "beyond and of file".
    //
    //The important point is that all pins get this same cluster
    //value, no matter what the worker thread might have done in between
    //seeks on different pins.  The same cluster vlaue means the
    //same base time, which is necessary to keep all the streams in sync.
    //
    //We want to be able to skip ahead in the file during a seek,
    //so clusters are able to be load out-of-order, but maybe the
    //solution is to just navigate among the clusters to find
    //their location and size, but don't actually load the frames.
    //That would mean there's nothiing special we needs to do wrt
    //find a cluster that corresponds to a time.  But to get the time
    //for a cluster, we can't assume that it's the first sub-element
    //of the cluster.  But we could have a roaming posn ptr for
    //each cluster, as we do for the segment, to allow us to
    //determine how much of the cluster has been parsed.  We
    //could stop loading as soon as we find the cluster's time
    //(only is pathological cases would the time be not right up
    //at the front of the cluster anyway).
    //
    //But then again, none of this will work in the splitter case
    //anway, and we can't do any pre-loading anyway (we don't even
    //have access to the seekhead, which is at the end of the file).
    //The problem we're having with clusters (attempting to seek
    //to a time beyond what's actually loaded) is that we're allowing
    //large times, and GetCluster returns 0 -- but that can change
    //if more clusters get loaded.  If we throttle the requested seek
    //time (that's what IMediaSeeking::Available is supposed to do),
    //then returning NULL can't happen.  But then does this just
    //defer the problem, from Segment::GetCluster to
    //IMediaSeeking::Available?
    //
    //But then again, this is the source filter case, not the splitter
    //case, so we should be able to seek.  We could bring back load:
    //  pSegment->Load(time)
    //which would load clusters until it loads a cluster whose time
    //is t >= time.  That would guarantee that Segment::GetCluster
    //returns non-NULL, and it would also mean that there's nothing
    //the worker thread could do to change cache state in a harmful
    //way (because that cluster has already been load, so if the worker
    //thread happens to load another than it wouldn't matter, since
    //that wouldn't affect the result of Segment::GetCluster).
    //
    //Actually, a cluster wouldn't really need its own roaming
    //posn ptr.  If the cluster has a non-zero size, and its block
    //group array is empty, that means we skipped loading the blocks
    //during our initial pass (e.g. a seek) and so we can load the
    //blocks on the fly when GetNextBlock is called.

    assert(pCurr);  //vetted above
    LONGLONG& tCurr = *pCurr;

    if (tCurr == Filter::kNoSeek)
        return E_INVALIDARG;  //need nonce value

    if (m_pFilter->m_state != State_Stopped)
    {
        lock.Release();

        StopThread();

        hr = lock.Seize(m_pFilter);
        assert(SUCCEEDED(hr));  //TODO
    }

    m_pFilter->SetCurrPosition(tCurr, dwCurr_, this);

    if (dwStopPos == AM_SEEKING_NoPositioning)
    {
        //TODO: I still haven't figured what should happen to the
        //stop position if the user doesn't seek the stop time
        //too.  For now I assume that that user wants to play
        //the entire remainder of the stream starting from the
        //seek time.

        m_pStream->SetStopPositionEOS();
    }
    else
    {
        assert(pStop);  //vetted above
        m_pStream->SetStopPosition(*pStop, dwStop_);
    }

    if (dwCurr_ & AM_SEEKING_ReturnTime)
    {
        tCurr = m_pStream->GetCurrTime();

        if (tCurr < 0)
        {
            hr = GetDuration(&tCurr);

            if (FAILED(hr) || (tCurr < 0))
                tCurr = 0;  //?
        }
    }

    if (dwStop_ & AM_SEEKING_ReturnTime)
    {
        assert(pStop);  //we checked this above
        *pStop = m_pStream->GetStopTime();

        if (*pStop < 0)
        {
            hr = GetDuration(pStop);

            if (FAILED(hr) || (*pStop < 0))
                *pStop = 0;  //?
        }
    }

    if (m_pFilter->m_state != State_Stopped)
        StartThread();

#if 0 //def _DEBUG
    os << "Outpin::SetPos(end): pCurr="
       << dec << (pCurr ? *pCurr : -1)
       << " pStop="
       << dec << (pStop ? *pStop : -1)
       << endl;
#endif

    return S_OK;
}


HRESULT Outpin::GetPositions(
    LONGLONG* pCurrPos,
    LONGLONG* pStopPos)
{
    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_pStream == 0)
        return E_FAIL;

    if (pCurrPos)
        hr = GetCurrentPosition(pCurrPos);

    if (pStopPos)
        hr = GetStopPosition(pStopPos);

    return S_OK;
}


HRESULT Outpin::GetAvailable(
    LONGLONG* pEarliest,
    LONGLONG* pLatest)
{
    if (pEarliest)
        *pEarliest = 0;

    return GetDuration(pLatest);
}



HRESULT Outpin::SetRate(double r)
{
    if (r == 1)
        return S_OK;

    if (r <= 0)
        return E_INVALIDARG;

    return E_NOTIMPL;  //TODO: better return here?
}


HRESULT Outpin::GetRate(double* p)
{
    if (p == 0)
        return E_POINTER;

    *p = 1;
    return S_OK;
}


HRESULT Outpin::GetPreroll(LONGLONG* p)
{
    if (p == 0)
        return E_POINTER;

    *p = 0;
    return S_OK;
}


HRESULT Outpin::GetName(PIN_INFO& i) const
{
    const std::wstring name = m_pStream->GetName();

    const size_t buflen = sizeof(i.achName)/sizeof(WCHAR);

    const errno_t e = wcscpy_s(i.achName, buflen, name.c_str());
    e;
    assert(e == 0);

    return S_OK;
}


unsigned Outpin::ThreadProc(void* pv)
{
    Outpin* const pPin = static_cast<Outpin*>(pv);
    assert(pPin);

    return pPin->Main();
}


unsigned Outpin::Main()
{
    assert(bool(m_pAllocator));
    assert(bool(m_pPinConnection));
    assert(bool(m_pInputPin));
    assert(m_pStream);

    //TODO: we need duration to send NewSegment
    //HRESULT hr = m_connection->NewSegment(st, sp, 1);
    //UPDATE: but if IMediaSeeking::GetCapabilities does NOT indicate
    //that it supports segments, then do we still need to send NewSegment?

    typedef mkvparser::Stream::samples_t samples_t;
    samples_t samples;

    for (;;)
    {
        HRESULT hr = PopulateSamples(samples);

        if (FAILED(hr))
            break;

        if (hr != S_OK)  //EOS
        {
            hr = m_pPinConnection->EndOfStream();
            break;
        }

        assert(!samples.empty());

        IMediaSample** const pSamples = &samples[0];

        const samples_t::size_type nSamples_ = samples.size();
        const long nSamples = static_cast<long>(nSamples_);

        long nProcessed;

        hr = m_pInputPin->ReceiveMultiple(pSamples, nSamples, &nProcessed);

        if (hr != S_OK)  //downstream filter says we're done
            break;

        mkvparser::Stream::Clear(samples);
        Sleep(0);
    }

    mkvparser::Stream::Clear(samples);
    return 0;
}


HRESULT Outpin::PopulateSamples(mkvparser::Stream::samples_t& samples)
{
    for (;;)
    {
        assert(samples.empty());

        Filter::Lock lock;

        HRESULT hr = lock.Seize(m_pFilter);

        if (FAILED(hr))
            return hr;

        long count;

        hr = m_pStream->GetSampleCount(count);

        if (SUCCEEDED(hr))
        {
            if (hr != S_OK)  //EOS
                return hr;

            hr = lock.Release();
            assert(SUCCEEDED(hr));

            //We have a count.  Now get some (empty) buffers.

            samples.reserve(count);

            for (long idx = 0; idx < count; ++idx)
            {
                IMediaSample* sample;

                hr = m_pAllocator->GetBuffer(&sample, 0, 0, 0);

                if (hr != S_OK)
                    return E_FAIL;  //we're done

                samples.push_back(sample);
            }

            hr = lock.Seize(m_pFilter);

            if (FAILED(hr))
                return hr;

            //We have buffers.  Now populate them.

            hr = m_pStream->PopulateSamples(samples);

            if (SUCCEEDED(hr))
            {
                if (hr != 2)
                    return hr;

                hr = lock.Release();
                assert(SUCCEEDED(hr));

                mkvparser::Stream::Clear(samples);
                continue;
            }
            else
            {
                assert(SUCCEEDED(hr));
                return hr;
            }
        }

        if (hr != VFW_E_BUFFER_UNDERFLOW)
            return hr;

        m_pFilter->OnStarvation(m_pStream->GetClusterCount());

        hr = lock.Release();
        assert(SUCCEEDED(hr));

        enum { nh = 2 };
        const HANDLE hh[nh] = { m_hStop, m_hNewCluster };

        const DWORD dw = WaitForMultipleObjects(nh, hh, 0, INFINITE);
        assert(dw >= WAIT_OBJECT_0);
        assert(dw < (WAIT_OBJECT_0 + nh));

        if (dw == WAIT_OBJECT_0)  //hStop
            return E_FAIL;  //NOTE: this return here is not an error

        assert(dw == (WAIT_OBJECT_0 + 1));  //hNewCluster
    }
}


mkvparser::Stream* Outpin::GetStream() const
{
    return m_pStream;
}


} //end namespace WebmSplit
