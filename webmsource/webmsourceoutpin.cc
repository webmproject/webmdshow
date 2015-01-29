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
#include "webmsourcefilter.h"
#include "mkvparserstream.h"
#include "webmsourceoutpin.h"
//#include "cmemallocator.h"
#include "cmediasample.h"
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
using std::boolalpha;
#endif


namespace WebmSource
{


Outpin::Outpin(
    Filter* pFilter,
    mkvparser::Stream* pStream) :
    Pin(pFilter, PINDIR_OUTPUT, pStream->GetId().c_str()),
    m_pStream(pStream),
    m_hThread(0)
{
    m_pStream->GetMediaTypes(m_preferred_mtv);
}


Outpin::~Outpin()
{
    assert(m_hThread == 0);
    assert(!bool(m_pAllocator));
    assert(!bool(m_pInputPin));

    delete m_pStream;
}


void Outpin::Init()  //transition from stopped
{
    assert(m_hThread == 0);

    if (m_connection == 0)
        return;  //nothing we need to do

    assert(bool(m_pAllocator));
    assert(bool(m_pInputPin));

    const HRESULT hr = m_pAllocator->Commit();
    assert(SUCCEEDED(hr));  //TODO

    StartThread();
}


void Outpin::Final()  //transition to stopped
{
    if (m_connection == 0)
        return;  //nothing was done

    assert(bool(m_pAllocator));
    assert(bool(m_pInputPin));

    const HRESULT hr = m_pAllocator->Decommit();
    assert(SUCCEEDED(hr));

    StopThread();
    m_pStream->Init();
}


void Outpin::StartThread()
{
    assert(m_hThread == 0);

    const uintptr_t h = _beginthreadex(
                            0,  //security
                            0,  //stack size
                            &Outpin::ThreadProc,
                            this,
                            0,   //run immediately
                            0);  //thread id

    m_hThread = reinterpret_cast<HANDLE>(h);
    assert(m_hThread);

#ifdef _DEBUG
    wodbgstream os;
    os << "webmsource::Outpin[" << m_id << "]::StartThread: hThread=0x"
       << hex << h << dec
       << endl;
#endif
}


void Outpin::StopThread()
{
    if (m_hThread == 0)
        return;

#ifdef _DEBUG
    wodbgstream os;
    os << "webmsource::Outpin[" << m_id << "]::StopThread: hThread=0x"
       << hex << uintptr_t(m_hThread) << dec
       << endl;
#endif


    assert(m_connection);

    HRESULT hr = m_connection->BeginFlush();

    const DWORD dw = WaitForSingleObject(m_hThread, 5000);
    assert(dw == WAIT_OBJECT_0);

    const BOOL b = CloseHandle(m_hThread);
    assert(b);

    m_hThread = 0;

    hr = m_connection->EndFlush();
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
        os << "webmsource::outpin::QI: iid=" << IIDStr(iid) << std::endl;
#endif
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG Outpin::AddRef()
{
    return m_pFilter->AddRef();
}


ULONG Outpin::Release()
{
    return m_pFilter->Release();
}


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

    assert(m_pFilter->m_file.IsOpen());

    if (m_pFilter->m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    if (m_connection)
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

    m_pAllocator = pAllocator;

    m_connection = pin;  //TODO: use com smartptr here
    m_connection->AddRef();

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


HRESULT Outpin::QueryInternalConnections(IPin**, ULONG* pn)
{
    if (pn == 0)
        return E_POINTER;

    *pn = 0;
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
    assert(SUCCEEDED(hr)); hr;
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

    using namespace mkvparser;

    Segment* const pSegment = m_pStream->m_pTrack->m_pSegment;
    assert(pSegment);

    LONGLONG duration_ns = pSegment->GetDuration();

    if (duration_ns >= 0)  //actually have a duration in file
    {
        reftime = duration_ns / 100;
        return S_OK;
    }

    if (pSegment->DoneParsing())
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

    if (const Cues* pCues = pSegment->GetCues())
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

    const long status = pSegment->LoadCluster();  //force progress
    assert(status >= 0);

    {
        const Cluster* const pCluster = pSegment->GetLast();  //best we can do

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

#if 0 //def _DEBUG
    wodbgstream os;
    os << "\nwebmsource::Outpin[" << m_id << "]::SetPos(begin): pCurr="
       << dec << (pCurr ? *pCurr : -1)
       << " dwCurr=0x"
       << hex << dwCurr_
       << " pStop="
       << dec << (pStop ? *pStop : -1)
       << " dwStop=0x"
       << hex << dwStop_
       << "; STATE=" << m_pFilter->m_state
       << endl;
#endif

    if (m_connection == 0)
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
                    tStop = 0;  //??
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

    assert(pCurr);  //vetted above
    LONGLONG& tCurr = *pCurr;

    if (tCurr == Filter::kNoSeek)
        return E_INVALIDARG;  //we need a nonce value

    if (m_pFilter->m_state != State_Stopped)
    {
#if 0 //def _DEBUG
        os << "webmsource::Outpin[" << m_id << "]::SetPos(cont'd): pCurr="
           << dec << (pCurr ? tCurr : -1)
           << " dwCurr=0x"
           << hex << dwCurr_
           << " pStop="
           << dec << (pStop ? *pStop : -1)
           << " dwStop=0x"
           << hex << dwStop_
           << "; BEGIN FLUSH: releasing filter lock; STATE="
           << m_pFilter->m_state
           << endl;
#endif

        lock.Release();

#if 0 //def _DEBUG
        os << "webmsource::Outpin[" << m_id << "]::SetPos(cont'd): pCurr="
           << dec << (pCurr ? tCurr : -1)
           << " dwCurr=0x"
           << hex << dwCurr_
           << " pStop="
           << dec << (pStop ? *pStop : -1)
           << " dwStop=0x"
           << hex << dwStop_
           << "; BEGIN FLUSH: released filter lock; "
           << "connection->calling BeginFlush"
           << endl;
#endif

        hr = m_connection->BeginFlush();

#if 0 //def _DEBUG
        os << "webmsource::Outpin[" << m_id << "]::SetPos(cont'd): pCurr="
           << dec << (pCurr ? tCurr : -1)
           << " dwCurr=0x"
           << hex << dwCurr_
           << " pStop="
           << dec << (pStop ? *pStop : -1)
           << " dwStop=0x"
           << hex << dwStop_
           << "; BEGIN FLUSH: released filter lock; "
           << "connection->called BeginFlush; "
           << "waiting for thread termination"
           << endl;
#endif

        assert(m_hThread);

        const DWORD dw = WaitForSingleObject(m_hThread, 5000);
        //assert(dw == WAIT_OBJECT_0);
        if (dw == WAIT_TIMEOUT)
            return VFW_E_TIMEOUT;

        const BOOL b = CloseHandle(m_hThread);
        assert(b);

        m_hThread = 0;

#if 0 //def _DEBUG
        os << "webmsource::Outpin[" << m_id << "]::SetPos(cont'd): pCurr="
           << dec << (pCurr ? tCurr : -1)
           << " dwCurr=0x"
           << hex << dwCurr_
           << " pStop="
           << dec << (pStop ? *pStop : -1)
           << " dwStop=0x"
           << hex << dwStop_
           << "; END FLUSH: calling connection->EndFlush"
           << endl;
#endif

        hr = m_connection->EndFlush();

#if 0 //def _DEBUG
        os << "webmsource::Outpin[" << m_id << "]::SetPos(cont'd): pCurr="
           << dec << (pCurr ? tCurr : -1)
           << " dwCurr=0x"
           << hex << dwCurr_
           << " pStop="
           << dec << (pStop ? *pStop : -1)
           << " dwStop=0x"
           << hex << dwStop_
           << "; END FLUSH: called connection->EndFlush; seizing filter lock"
           << endl;
#endif

        hr = lock.Seize(m_pFilter);
        assert(SUCCEEDED(hr));  //TODO

#if 0 //def _DEBUG
        os << "webmsource::Outpin[" << m_id << "]::SetPos(cont'd): pCurr="
           << dec << (pCurr ? tCurr : -1)
           << " dwCurr=0x"
           << hex << dwCurr_
           << " pStop="
           << dec << (pStop ? *pStop : -1)
           << " dwStop=0x"
           << hex << dwStop_
           << "; END FLUSH: seized filter lock"
           << endl;
#endif
    }


#if 0 //def _DEBUG
    os << "webmsource::Outpin[" << m_id << "]::SetPos(cont'd): pCurr="
       << dec << (pCurr ? tCurr : -1)
       << " pStop="
       << dec << (pStop ? *pStop : -1)
       << "; SET CURR POSN (begin)"
       << endl;
#endif

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

#if 0 //def _DEBUG
    os << "webmsource::Outpin[" << m_id << "]::SetPos(cont'd): pCurr="
       << dec << (pCurr ? tCurr : -1)
       << " pStop="
       << dec << (pStop ? *pStop : -1)
       << "; SET CURR POSN (end)"
       << endl;
#endif

    if (dwCurr_ & AM_SEEKING_ReturnTime)
    {
        tCurr = m_pStream->GetCurrTime();

        if (tCurr < 0)  //means "use duration"
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

        if (*pStop < 0)  //means "use duration"
        {
            hr = GetDuration(pStop);

            if (FAILED(hr) || (*pStop < 0))
                *pStop = 0;  //?
        }
    }

    if (m_pFilter->m_state != State_Stopped)
        StartThread();

#if 0 //def _DEBUG
    os << "webmsource::Outpin[" << m_id << "]::SetPos(end): pCurr="
       << dec << (pCurr ? tCurr : -1)
       << " pStop="
       << dec << (pStop ? *pStop : -1)
       << "; DONE\n"
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
    assert(m_connection);
    assert(bool(m_pInputPin));

    //TODO: we need duration to send NewSegment
    //HRESULT hr = m_connection->NewSegment(st, sp, 1);

    typedef mkvparser::Stream::samples_t samples_t;
    samples_t samples;

    for (;;)
    {
        HRESULT hr = PopulateSamples(samples);

        if (FAILED(hr))
            break;

        if (hr != S_OK)  //EOS
        {
            hr = m_connection->EndOfStream();
            break;
        }

        assert(!samples.empty());

        IMediaSample** const pSamples = &samples[0];

        const samples_t::size_type nSamples_ = samples.size();
        const long nSamples = static_cast<long>(nSamples_);

        long nProcessed;

        hr = m_pInputPin->ReceiveMultiple(pSamples, nSamples, &nProcessed);

        //TODO: there is a potential problem here.  If the upstream decoder
        //rejects the sample (problem with bitstream, etc), then this
        //terminates this streaming thread, but the filter isn't in the
        //Stopped state.
        //Now say the use notices that the window isn't displaying any video.
        //He closes the window to stop play, but this causes the FGM to
        //call IMediaSeeking::SetPosition to reset the position back to 0.
        //But since we weren't stopped when that happened, we restart the
        //thread, thinking that a play had been interrupted by a seek request,
        //but that's not the case, because the thread had already been
        //interrupted much earlier, because the bitstream failed to decode.
        //We probably need a stronger test: instead of testing whether we
        //were stopped or not stopped, we need to test whether we we not
        //stopped and thread wasn't already terminated.

        if (hr != S_OK)
            break;

        mkvparser::Stream::Clear(samples);
        Sleep(0);       //better way to do this?
    }

    mkvparser::Stream::Clear(samples);
    m_pStream->Stop();

    return 0;
}


HRESULT Outpin::PopulateSamples(mkvparser::Stream::samples_t& samples)
{
    mkvparser::Segment* const pSegment = m_pStream->m_pTrack->m_pSegment;

    for (;;)
    {
        assert(samples.empty());

        Filter::Lock lock;

        HRESULT hr = lock.Seize(m_pFilter);

        if (FAILED(hr))
            return hr;

        long count;

        for (;;)
        {
            hr = m_pStream->GetSampleCount(count);

            if (SUCCEEDED(hr))
                break;

            if (hr != VFW_E_BUFFER_UNDERFLOW)
                return hr;

            const long status = pSegment->LoadCluster();
            assert(status >= 0);
        }

        if (hr != S_OK)      //EOS
            return S_FALSE;  //report EOS

        hr = lock.Release();
        assert(SUCCEEDED(hr));

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

        for (;;)
        {
            hr = m_pStream->PopulateSamples(samples);

            if (SUCCEEDED(hr))
                break;

            if (hr != VFW_E_BUFFER_UNDERFLOW)
                return hr;

            const long status = pSegment->LoadCluster();
            assert(status >= 0);
        }

        if (hr != 2)
            return hr;  //either have samples, or EOS

        hr = lock.Release();
        assert(SUCCEEDED(hr));

        mkvparser::Stream::Clear(samples);
    }
}


} //end namespace WebmSource

