// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma warning(disable:4505)  //unreferenced local function has been removed
#include "webmccfilter.hpp"
#include "webmccoutpin.hpp"
#include "mediatypeutil.hpp"
#include "graphutil.hpp"
#include "webmtypes.hpp"
#include <vfwmsgs.h>
#include <uuids.h>
#include <cassert>
#include <amvideo.h>
#include <evcode.h>
#ifdef _DEBUG
#include <iomanip>
#include "odbgstream.hpp"
using std::endl;
using std::hex;
using std::dec;
#endif

namespace WebmColorConversion
{

Inpin::Inpin(Filter* p) :
    Pin(p, PINDIR_INPUT, L"input"),
    m_bEndOfStream(false),
    m_bFlush(false),
    m_bDone(false)
{
    AM_MEDIA_TYPE mt;

    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_RGB24;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;
    mt.formattype = GUID_NULL;
    mt.pUnk = 0;
    mt.cbFormat = 0;
    mt.pbFormat = 0;

    m_preferred_mtv.Add(mt);

    mt.subtype = MEDIASUBTYPE_RGB32;
    m_preferred_mtv.Add(mt);

    m_hSamples = CreateEvent(0, 0, 0, 0);
    assert(m_hSamples);
}


Inpin::~Inpin()
{
    const BOOL b = CloseHandle(m_hSamples);
    assert(b);
}


HRESULT Inpin::QueryInterface(const IID& iid, void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == __uuidof(IUnknown))
        pUnk = static_cast<IPin*>(this);

    else if (iid == __uuidof(IPin))
        pUnk = static_cast<IPin*>(this);

    else if (iid == __uuidof(IMemInputPin))
        pUnk = static_cast<IMemInputPin*>(this);

    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG Inpin::AddRef()
{
    return m_pFilter->AddRef();
}


ULONG Inpin::Release()
{
    return m_pFilter->Release();
}


HRESULT Inpin::Connect(IPin*, const AM_MEDIA_TYPE*)
{
    return E_UNEXPECTED;  //for output pins only
}


HRESULT Inpin::QueryInternalConnections(
    IPin** pa,
    ULONG* pn)
{
    if (pn == 0)
        return E_POINTER;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    const ULONG m = 1;  //number of output pins

    ULONG& n = *pn;

    if (n == 0)
    {
        if (pa == 0)  //query for required number
        {
            n = m;
            return S_OK;
        }

        return S_FALSE;  //means "insufficient number of array elements"
    }

    if (n < m)
    {
        n = 0;
        return S_FALSE;  //means "insufficient number of array elements"
    }

    if (pa == 0)
    {
        n = 0;
        return E_POINTER;
    }

    IPin*& pin = pa[0];

    pin = &m_pFilter->m_outpin;
    pin->AddRef();

    n = m;
    return S_OK;
}


HRESULT Inpin::ReceiveConnection(
    IPin* pin,
    const AM_MEDIA_TYPE* pmt)
{
    if (pin == 0)
        return E_INVALIDARG;

    if (pmt == 0)
        return E_INVALIDARG;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_pFilter->m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    if (bool(m_pPinConnection))
        return VFW_E_ALREADY_CONNECTED;

    m_connection_mtv.Clear();

    hr = QueryAccept(pmt);

    if (hr != S_OK)
        return VFW_E_TYPE_NOT_ACCEPTED;

    const AM_MEDIA_TYPE& mt = *pmt;

    hr = m_connection_mtv.Add(mt);

    if (FAILED(hr))
        return hr;

    m_pPinConnection = pin;

    m_pFilter->m_outpin.OnInpinConnect(mt);

    return S_OK;
}


HRESULT Inpin::EndOfStream()
{
    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

#ifdef _DEBUG
    odbgstream os;
    os << "webmcc::inpin::EOS" << endl;
#endif

    if (m_bFlush)
        return S_FALSE;  //?

    m_bEndOfStream = true;

    m_samples.push_back(0);

    const BOOL b = SetEvent(m_hSamples);
    assert(b);

    return S_OK;
}


HRESULT Inpin::BeginFlush()
{
#ifdef _DEBUG
    odbgstream os;
    os << "webmcc::inpin::beginflush(begin)" << endl;
#endif

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    //IPin::BeginFlush
    //In a flush operation, a filter discards whatever data it was processing.
    //It rejects new data until the flush is completed. The flush is
    //completed when the upstream pin calls the IPin::EndFlush method.
    //Flushing enables the filter graph to be more responsive when events
    //alter the normal data flow. For example, flushing occurs during a seek.
    //
    //When BeginFlush is called, the filter performs the following steps:
    //
    //(1) Passes the IPin::BeginFlush call downstream.
    //
    //(2) Sets an internal flag that causes all data-streaming methods to
    //    fail, such as IMemInputPin::Receive.
    //
    //(3) Returns from any blocked calls to the Receive method.
    //
    //
    //When the BeginFlush notification reaches a renderer filter,
    //the renderer frees any samples that it holds.
    //
    //After BeginFlush is called, the pin rejects all samples from upstream,
    //with a return value of S_FALSE, until the IPin::EndFlush method is
    //called.

    //IPin::EndOfStream
    //The IPin::BeginFlush method flushes any queued end-of-stream
    //notifications. This is intended for input pins only.

    m_bFlush = true;

    Outpin& outpin = m_pFilter->m_outpin;

    if (IPin* const pPin = outpin.m_pPinConnection)
    {
#ifdef _DEBUG
        os << "webmcc::inpin::beginflush:"
           << " flushing downstream filter"
           << endl;
#endif

        hr = pPin->BeginFlush();  //safe to do this here, while holding lock?

#ifdef _DEBUG
        os << "webmcc::inpin::beginflush:"
           << " called BeginFlush: hr=0x" << hex << hr << dec
           << "\nwebmcc::inpin::beginflush: waiting for thread"
           << "to terminate"
           << endl;
#endif

        const BOOL b = SetEvent(m_hSamples);  //to terminate thread
        assert(b);

        hr = lock.Release();
        assert(SUCCEEDED(hr));

        //The thread might not exist yet, if we have been connected but not
        //started.  In which case, attempting to stop the thread is benign.

        outpin.StopThread();
    }

#ifdef _DEBUG
    os << "webmcc::inpin::beginflush(end #2): thread terminated"
       << endl;
#endif

    return S_OK;
}


HRESULT Inpin::EndFlush()
{
    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

#ifdef _DEBUG
    odbgstream os;
    os << "webmcc::inpin::endflush(begin)" << endl;
#endif

    //IPin::EndFlush
    //When this method is called, the filter performs the following actions:
    //
    //(1) Waits for all queued samples to be discarded.
    //
    //(2) Frees any buffered data, including any pending end-of-stream
    //notifications.
    //
    //(3) Clears any pending EC_COMPLETE notifications.
    //
    //(4) Calls EndFlush downstream.
    //
    //When the method returns, the pin can accept new samples.

    m_bFlush = false;
    m_bEndOfStream = false;
    m_bDone = false;

    while (!m_samples.empty())
    {
        IMediaSample* const pSample = m_samples.front();
        m_samples.pop_front();

        if (pSample)
            pSample->Release();
    }

    Outpin& outpin = m_pFilter->m_outpin;

    if (IPin* const pPin = outpin.m_pPinConnection)
    {
#ifdef _DEBUG
        os << "webmcc::inpin::endflush: calling endflush on"
           << " downstream filter"
           << endl;
#endif

        hr = pPin->EndFlush();  //safe to call this without releasing lock?

        if (m_pFilter->m_state != State_Stopped)
        {
#ifdef _DEBUG
            os << "webmcc::inpin::endflush: state != stopped,"
               << " so starting thread"
               << endl;
#endif

            outpin.StartThread();

#ifdef _DEBUG
            os << "webmcc::inpin::endflush: started thread"
               << endl;
#endif
        }
    }

#ifdef _DEBUG
    os << "webmcc::inpin::endflush(end)" << endl;
#endif

    return S_OK;
}


void Inpin::OnCompletion()
{
    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);
    assert(SUCCEEDED(hr));

    while (!m_samples.empty())
    {
        IMediaSample* const pSample = m_samples.front();

        if (pSample == 0)  //EOS notification
            break;         //don't throw EOS notification away

        m_samples.pop_front();
        pSample->Release();
    }

    m_bDone = true;
}


HRESULT Inpin::NewSegment(
    REFERENCE_TIME st,
    REFERENCE_TIME sp,
    double r)
{
    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    if (IPin* pPin = m_pFilter->m_outpin.m_pPinConnection)
    {
        lock.Release();

        const HRESULT hr = pPin->NewSegment(st, sp, r);
        return hr;
    }

    return S_OK;
}


HRESULT Inpin::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    if (pmt == 0)
        return E_INVALIDARG;

    const AM_MEDIA_TYPE& mt = *pmt;

    if (mt.majortype != MEDIATYPE_Video)
        return S_FALSE;

    if (mt.subtype == MEDIASUBTYPE_RGB24)
        __noop;
    else if (mt.subtype == MEDIASUBTYPE_RGB32)
        __noop;
    else
        return S_FALSE;

    if (mt.pbFormat == 0)
        return S_FALSE;

    const BITMAPINFOHEADER* pbmih;  //TODO: make a utility for this

    if (mt.formattype == FORMAT_VideoInfo)
    {
        if (mt.cbFormat < sizeof(VIDEOINFOHEADER))
            return S_FALSE;

        const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);

        //if (vih.AvgTimePerFrame <= 0)
        //    return S_FALSE;

        pbmih = &vih.bmiHeader;
    }
#if 0  //TODO
    else if (mt.formattype == FORMAT_VideoInfo2)
    {
        if (mt.cbFormat < sizeof(VIDEOINFOHEADER2))
            return S_FALSE;

        const VIDEOINFOHEADER2& vih = (VIDEOINFOHEADER2&)(*mt.pbFormat);

        if (vih.AvgTimePerFrame <= 0)
            return S_FALSE;

        pbmih = &vih.bmiHeader;
    }
#endif
    else
        return S_FALSE;

    assert(pbmih);
    const BITMAPINFOHEADER& bmih = *pbmih;

    if (bmih.biSize != sizeof(BITMAPINFOHEADER))  //TODO: liberalize
        return S_FALSE;

    if (bmih.biWidth <= 0)
        return S_FALSE;

    if (bmih.biWidth % 2)  //TODO
        return S_FALSE;

    if (bmih.biHeight == 0)
        return S_FALSE;

    if (bmih.biHeight % 2)  //TODO
        return S_FALSE;

    if (bmih.biCompression != mt.subtype.Data1)
        return S_FALSE;

    return S_OK;
}


HRESULT Inpin::GetAllocator(IMemAllocator** p)
{
    if (p)
        *p = 0;

    return VFW_E_NO_ALLOCATOR;
}



HRESULT Inpin::NotifyAllocator(
    IMemAllocator* pAllocator,
    BOOL)
{
    if (pAllocator == 0)
        return E_INVALIDARG;

    ALLOCATOR_PROPERTIES props;

    const HRESULT hr = pAllocator->GetProperties(&props);
    hr;
    assert(SUCCEEDED(hr));

#ifdef _DEBUG
    wodbgstream os;
    os << "webmcc::inpin::NotifyAllocator: props.cBuffers="
       << props.cBuffers
       << " cbBuffer="
       << props.cbBuffer
       << " cbAlign="
       << props.cbAlign
       << " cbPrefix="
       << props.cbPrefix
       << endl;
#endif

    return S_OK;
}


HRESULT Inpin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pp)
{
    if (pp == 0)
        return E_POINTER;

    ALLOCATOR_PROPERTIES& p = *pp;
    p;

    return S_OK;
}


HRESULT Inpin::Receive(IMediaSample* pInSample)
{
    if (pInSample == 0)
        return E_INVALIDARG;

    HRESULT hr;

//#define DEBUG_RECEIVE
#undef DEBUG_RECEIVE

#ifdef DEBUG_RECEIVE
    __int64 start_reftime_, stop_reftime_;
    hr = pInSample->GetTime(&start_reftime_, &stop_reftime_);
#endif

    Filter::Lock lock;

    hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

//#ifdef DEBUG_RECEIVE
//    wodbgstream os;
//    os << L"vp8dec::inpin::Receive: THREAD=0x"
//       << std::hex << GetCurrentThreadId() << std::dec
//       << endl;
//#endif

    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    Outpin& outpin = m_pFilter->m_outpin;

    if (!bool(outpin.m_pPinConnection))
        return S_FALSE;

    if (!bool(outpin.m_pAllocator))  //should never happen
        return VFW_E_NO_ALLOCATOR;

    if (m_pFilter->m_state == State_Stopped)
        return VFW_E_NOT_RUNNING;

    if (m_bEndOfStream)
        return VFW_E_SAMPLE_REJECTED_EOS;

    if (m_bFlush)
        return S_FALSE;

    if (m_bDone)
        return S_FALSE;

#ifdef DEBUG_RECEIVE
    {
        odbgstream os;
        os << "webmvorbisdec::inpin::receive: ";

        os << std::fixed << std::setprecision(3);

        if (hr == S_OK)
            os << "start[sec]="
               << double(start_reftime_) / 10000000
               << "; stop[sec]="
               << double(stop_reftime_) / 10000000
               << "; dt[ms]="
               << double(stop_reftime_ - start_reftime_) / 10000;

        else if (hr == VFW_S_NO_STOP_TIME)
            os << "start[sec]=" << double(start_reftime_) / 10000000;

        os << endl;
    }
#endif

    pInSample->AddRef();
    m_samples.push_back(pInSample);

    const BOOL b = SetEvent(m_hSamples);
    assert(b);

    return S_OK;
}


#if 0  //TODO


void Inpin::PopulateSample(
    IMediaSample* pOutSample,
    long target_count,
    const WAVEFORMATEX& wfx)
{
    assert(pOutSample);
    assert(target_count >= 1);

    const DWORD channels = wfx.nChannels;
    assert(channels > 0);
    assert(channels <= 2);  //TODO
    assert(channels == m_channels.size());

    const long block_align = wfx.nBlockAlign;
    assert(size_t(block_align) == (channels * sizeof(float)));

    //ALLOCATOR_PROPERTIES props;

    //hr = outpin.m_pAllocator->GetProperties(&props);
    //assert(SUCCEEDED(hr));

    //if (FAILED(hr))
    //    return hr;

    //const long cbBuffer = props.cbBuffer;
    //assert(cbBuffer >= block_align);

    const long size = pOutSample->GetSize();
    assert(size >= 0);
    assert(size >= block_align);

    const long samples_ = size / block_align;  //max samples in buffer
    assert(samples_ >= 1);

    const long samples = (samples_ < target_count) ? samples_ : target_count;

    const long len_out = samples * block_align;
    assert(len_out <= size);

    BYTE* dst;

    HRESULT hr = pOutSample->GetPointer(&dst);
    assert(SUCCEEDED(hr));
    assert(dst);

    BYTE* const dst_end = dst + len_out;
    dst_end;

    for (long i = 0; i < samples; ++i)
    {
        for (DWORD j = 0; j < channels; ++j)
        {
            samples_t& ss = m_channels[j];
            assert(!ss.empty());

            const float& s = ss.front();

            memcpy(dst, &s, sizeof(float));  //TODO: proper channel mapping

            dst += sizeof(float);
            assert(dst <= dst_end);

            ss.pop_front();  //OK for deque (horrible for vector)
        }
    }

    assert(dst == dst_end);

    hr = pOutSample->SetActualDataLength(len_out);
    assert(SUCCEEDED(hr));

    m_samples += samples;

    const double secs = m_samples / double(wfx.nSamplesPerSec);
    const double ticks = secs * 10000000;

    LONGLONG stop_reftime = m_first_reftime + static_cast<LONGLONG>(ticks);

    hr = pOutSample->SetTime(&m_start_reftime, &stop_reftime);
    assert(SUCCEEDED(hr));

    m_start_reftime = stop_reftime;

    hr = pOutSample->SetSyncPoint(TRUE);  //?
    assert(SUCCEEDED(hr));

    hr = pOutSample->SetPreroll(FALSE);
    assert(SUCCEEDED(hr));

#if 0
    hr = pInSample->IsDiscontinuity();
    assert(SUCCEEDED(hr));

    hr = pOutSample->SetDiscontinuity(BOOL(hr == S_OK));
    assert(SUCCEEDED(hr));
#else
    hr = pOutSample->SetDiscontinuity(m_bDiscontinuity ? TRUE : FALSE);
    assert(SUCCEEDED(hr));

    m_bDiscontinuity = false;
#endif

    hr = pOutSample->SetMediaTime(0, 0);
    assert(SUCCEEDED(hr));
}


HRESULT Inpin::PopulateSamples()
{
    //Filter is NOT locked

    const Outpin& outpin = m_pFilter->m_outpin;

    for (;;)
    {
        GraphUtil::IMediaSamplePtr pOutSample;

        HRESULT hr = outpin.m_pAllocator->GetBuffer(&pOutSample, 0, 0, 0);

        if (FAILED(hr))
            return hr;

        Filter::Lock lock;

        hr = lock.Seize(m_pFilter);
        assert(SUCCEEDED(hr));

        if (FAILED(hr))
            return hr;

        if (m_pFilter->m_state == State_Stopped)
            return VFW_E_NOT_RUNNING;

        if (m_bEndOfStream)
            return VFW_E_SAMPLE_REJECTED_EOS;

        if (m_bFlush)
            return S_FALSE;

        if (m_bDone)
            return S_FALSE;

        if (!bool(outpin.m_pPinConnection))  //weird
            return S_FALSE;

        if (!bool(outpin.m_pInputPin))  //weird
            return S_FALSE;

        //if (!bool(outpin.m_pAllocator))  //weird
        //    return S_FALSE;

        const WAVEFORMATEX* const pwfx = outpin.GetFormat();
        assert(pwfx);
        assert(pwfx->nChannels > 0);
        assert(pwfx->nChannels == m_channels.size());

        const long actual = m_channels[0].size();
        const long target = pwfx->nSamplesPerSec / Pin::kSampleRateDivisor;

        if (actual < target)
            return S_OK;

        PopulateSample(pOutSample, target, *pwfx);

        m_buffers.push_back(pOutSample.Detach());

        const BOOL b = SetEvent(m_hSamples);
        assert(b);
    }
}

#endif  //TODO


int Inpin::GetSample(IMediaSample** ppSample)
{
    assert(ppSample);

    IMediaSample*& pSample = *ppSample;

    if (m_bFlush)
    {
        pSample = 0;
        return -2;  //terminate
    }

    if (m_samples.empty())
    {
        pSample = 0;

#if 0  //TODO: need another way to handle this
        if (m_packet.packetno < 0)  //stopped
            return -2;  //terminate
#endif

        assert(m_pFilter->m_state != State_Stopped);
        return -1;  //wait
    }

    pSample = m_samples.front();
    m_samples.pop_front();

    if (pSample)
        return 1;

    assert(m_samples.empty());
    return 0;  //EOS
}


HRESULT Inpin::ReceiveMultiple(
    IMediaSample** pSamples,
    long n,    //in
    long* pm)  //out
{
    if (pm == 0)
        return E_POINTER;

    long& m = *pm;    //out
    m = 0;

    if (n <= 0)
        return S_OK;  //weird

    if (pSamples == 0)
        return E_INVALIDARG;

    for (long i = 0; i < n; ++i)
    {
        IMediaSample* const pSample = pSamples[i];
        assert(pSample);

        const HRESULT hr = Receive(pSample);

        if (hr != S_OK)
            return hr;

        ++m;
    }

    return S_OK;
}


HRESULT Inpin::ReceiveCanBlock()
{
    return S_FALSE;
}


HRESULT Inpin::OnDisconnect()
{
    return m_pFilter->m_outpin.OnInpinDisconnect();
}


HRESULT Inpin::GetName(PIN_INFO& info) const
{
    const wchar_t name[] = L"RGB";  //TODO: support YUV-to-YUV too?

#if _MSC_VER >= 1400
    enum { namelen = sizeof(info.achName) / sizeof(WCHAR) };
    const errno_t e = wcscpy_s(info.achName, namelen, name);
    e;
    assert(e == 0);
#else
    wcscpy(info.achName, name);
#endif

    return S_OK;
}


HRESULT Inpin::Start()
{
    m_bEndOfStream = false;
    m_bFlush = false;
    m_bDone = false;

    if (!bool(m_pPinConnection))
        return S_FALSE;

    const Outpin& outpin = m_pFilter->m_outpin;

    if (!bool(outpin.m_pPinConnection))
        return S_FALSE;

    assert(m_samples.empty());

    //TODO: based on input and output media types,
    //select a color conversion function.

    return S_OK;
}

void Inpin::Stop()
{
    while (!m_samples.empty())
    {
        IMediaSample* const pSample = m_samples.front();
        m_samples.pop_front();

        if (pSample)
            pSample->Release();
    }

    const BOOL b = SetEvent(m_hSamples);  //tell thread to terminate
    assert(b);
}

}  //end namespace WebmColorConversion

