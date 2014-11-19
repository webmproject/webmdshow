// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma warning(disable:4505)  //unreferenced local function has been removed
#include "webmvorbisdecoderfilter.h"
#include "webmvorbisdecoderoutpin.h"
#include "mediatypeutil.h"
#include "graphutil.h"
#include "webmtypes.h"
#include "vorbistypes.h"
#include <vfwmsgs.h>
#include <uuids.h>
#include <cassert>
#include <amvideo.h>
#include <evcode.h>
#ifdef _DEBUG
#include <iomanip>
#include "odbgstream.h"
using std::endl;
using std::hex;
using std::dec;
#endif

namespace WebmVorbisDecoderLib
{

Inpin::Inpin(Filter* p) :
    Pin(p, PINDIR_INPUT, L"input"),
    m_bEndOfStream(false),
    m_bFlush(false),
    m_bDone(false)
{
    AM_MEDIA_TYPE mt;

    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = VorbisTypes::MEDIASUBTYPE_Vorbis2;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;
    mt.formattype = VorbisTypes::FORMAT_Vorbis2;
    mt.pUnk = 0;
    mt.cbFormat = 0;
    mt.pbFormat = 0;

    m_preferred_mtv.Add(mt);

    m_packet.packetno = -1;

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
    os << "webmvorbisdecoder::inpin::EOS" << endl;
#endif

    if (m_bFlush)
        return S_FALSE;  //?

    m_bEndOfStream = true;

    m_buffers.push_back(0);

    const BOOL b = SetEvent(m_hSamples);
    assert(b);

    return S_OK;
}


HRESULT Inpin::BeginFlush()
{
#ifdef _DEBUG
    odbgstream os;
    os << "webmvorbisdecoder::inpin::beginflush(begin)" << endl;
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
        os << "webmvorbisdecoder::inpin::beginflush:"
           << " flushing downstream filter"
           << endl;
#endif

        hr = pPin->BeginFlush();  //safe to do this here, while holding lock?

#ifdef _DEBUG
        os << "webmvorbisdecoder::inpin::beginflush:"
           << " called BeginFlush: hr=0x" << hex << hr << dec
           << "\nwebmvorbisdecoder::inpin::beginflush: waiting for thread"
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
    os << "webmvorbisdecoder::inpin::beginflush(end #2): thread terminated"
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
    os << "webmvorbisdecoder::inpin::endflush(begin)" << endl;
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
    m_first_reftime = -1;
    m_bDone = false;

    while (!m_buffers.empty())
    {
        IMediaSample* const pSample = m_buffers.front();
        m_buffers.pop_front();

        if (pSample)
            pSample->Release();
    }

    typedef channels_t::iterator iter_t;

    iter_t i = m_channels.begin();
    const iter_t j = m_channels.end();

    while (i != j)
    {
        samples_t& ss = *i++;
        ss.clear();
    }

    Outpin& outpin = m_pFilter->m_outpin;

    if (IPin* const pPin = outpin.m_pPinConnection)
    {
#ifdef _DEBUG
        os << "webmvorbisdecoder::inpin::endflush: calling endflush on"
           << " downstream filter"
           << endl;
#endif

        hr = pPin->EndFlush();  //safe to call this without releasing lock?

        if (m_pFilter->m_state != State_Stopped)
        {
#ifdef _DEBUG
            os << "webmvorbisdecoder::inpin::endflush: state != stopped,"
               << " so starting thread"
               << endl;
#endif

            outpin.StartThread();

#ifdef _DEBUG
            os << "webmvorbisdecoder::inpin::endflush: started thread"
               << endl;
#endif
        }
    }

#ifdef _DEBUG
    os << "webmvorbisdecoder::inpin::endflush(end)" << endl;
#endif

    return S_OK;
}


void Inpin::OnCompletion()
{
    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);
    assert(SUCCEEDED(hr));

    while (!m_buffers.empty())
    {
        IMediaSample* const pSample = m_buffers.front();

        if (pSample == 0)  //EOS notification
            break;         //don't throw EOS notification away

        m_buffers.pop_front();
        pSample->Release();
    }

    typedef channels_t::iterator iter_t;

    iter_t i = m_channels.begin();
    const iter_t j = m_channels.end();

    while (i != j)
    {
        samples_t& ss = *i++;
        ss.clear();
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

    if (mt.majortype != MEDIATYPE_Audio)
        return S_FALSE;

    if (mt.subtype != VorbisTypes::MEDIASUBTYPE_Vorbis2)
        return S_FALSE;

    if (mt.formattype != VorbisTypes::FORMAT_Vorbis2)
        return S_FALSE;

    if (mt.pbFormat == 0)
        return S_FALSE;

    typedef VorbisTypes::VORBISFORMAT2 FMT;

    if (mt.cbFormat < sizeof(FMT))
        return S_FALSE;

    const FMT& fmt = (FMT&)(*mt.pbFormat);

    if (fmt.channels == 0)
        return S_FALSE;

    if (fmt.channels > 2)  //TODO: handle up to 8
        return S_FALSE;

    if (fmt.samplesPerSec == 0)
        return S_FALSE;

    //check bitsPerSample?

    const ULONG id_len = fmt.headerSize[0];

    if (id_len == 0)
        return S_FALSE;

    const ULONG comment_len = fmt.headerSize[1];

    if (comment_len == 0)
        return S_FALSE;

    const ULONG setup_len = fmt.headerSize[2];

    if (setup_len == 0)
        return S_FALSE;

    const ULONG hdr_len = id_len + comment_len + setup_len;

    if (mt.cbFormat < (sizeof(FMT) + hdr_len))
        return S_FALSE;

    //TODO: vet headers

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
    os << "webmvorbisdecoder::inpin::NotifyAllocator: props.cBuffers="
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

    if (m_first_reftime < 0)
    {
        LONGLONG sp;

        hr = pInSample->GetTime(&m_first_reftime, &sp);

        if (FAILED(hr))
            return S_OK;

        if (m_first_reftime < 0)
            return S_OK;

        m_start_reftime = m_first_reftime;
        m_samples = 0;

#ifdef DEBUG_RECEIVE
        odbgstream os;
        os << std::fixed << std::setprecision(3);

        os << "\nwebmvorbisdec::Inpin::Receive: RESET FIRST REFTIME;"
           << " st=" << m_start_reftime
           << " st[sec]=" << (double(m_start_reftime) / 10000000)
           << endl;
#endif

        const int status = vorbis_synthesis_restart(&m_dsp_state);
        status;
        assert(status == 0);  //success

        m_bDiscontinuity = true;
    }

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

    Decode(pInSample);

    hr = lock.Release();
    assert(SUCCEEDED(hr));

    if (FAILED(hr))
        return hr;

    return PopulateSamples();
}


void Inpin::Decode(IMediaSample* pInSample)
{
    BYTE* buf_in;

    HRESULT hr = pInSample->GetPointer(&buf_in);
    assert(SUCCEEDED(hr));
    assert(buf_in);

    const long len_in = pInSample->GetActualDataLength();
    assert(len_in >= 0);

    ogg_packet& pkt = m_packet;

    pkt.packet = buf_in;
    pkt.bytes = len_in;
    ++pkt.packetno;

    int status = vorbis_synthesis(&m_block, &pkt);
    assert(status == 0);  //TODO

    status = vorbis_synthesis_blockin(&m_dsp_state, &m_block);
    assert(status == 0);  //TODO

    typedef VorbisTypes::VORBISFORMAT2 FMT;

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    assert(mt.cbFormat > sizeof(FMT));
    assert(mt.pbFormat);

    const FMT& fmt = (const FMT&)(*mt.pbFormat);
    assert(fmt.channels > 0);
    assert(fmt.samplesPerSec > 0);

    float** sv;

    const int pcmout_count = vorbis_synthesis_pcmout(&m_dsp_state, &sv);

    if (pcmout_count <= 0)
        return;

    assert(sv);

    for (DWORD i = 0; i < fmt.channels; ++i)
    {
        const float* const first = sv[i];
        const float* const last = first + pcmout_count;

        samples_t& ss = m_channels[i];
        ss.insert(ss.end(), first, last);
    }

    sv = 0;

    status = vorbis_synthesis_read(&m_dsp_state, pcmout_count);
    assert(status == 0);
}


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


int Inpin::GetSample(IMediaSample** ppSample)
{
    assert(ppSample);

    IMediaSample*& pSample = *ppSample;

    if (m_bFlush)
    {
        pSample = 0;
        return -2;  //terminate
    }

    if (m_buffers.empty())
    {
        pSample = 0;

        if (m_packet.packetno < 0)  //stopped
            return -2;  //terminate

        assert(m_pFilter->m_state != State_Stopped);
        return -1;  //wait
    }

    pSample = m_buffers.front();
    m_buffers.pop_front();

    if (pSample)
        return 1;

    assert(m_buffers.empty());
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
    return S_OK;  //because we wait for output sample
}


HRESULT Inpin::OnDisconnect()
{
    return m_pFilter->m_outpin.OnInpinDisconnect();
}


HRESULT Inpin::GetName(PIN_INFO& info) const
{
    const wchar_t name[] = L"Vorbis";

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

    ogg_packet& pkt = m_packet;

    assert(pkt.packetno < 0);

    if (!bool(m_pPinConnection))
        return S_FALSE;

    const Outpin& outpin = m_pFilter->m_outpin;

    if (!bool(outpin.m_pPinConnection))
        return S_FALSE;

    typedef VorbisTypes::VORBISFORMAT2 FMT;

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    assert(mt.cbFormat > sizeof(FMT));
    assert(mt.pbFormat);

    BYTE* pb = mt.pbFormat;
    BYTE* const pb_end = pb + mt.cbFormat;

    const FMT& fmt = (const FMT&)(*pb);

    pb += sizeof(FMT);
    assert(pb < pb_end);

    const ULONG id_len = fmt.headerSize[0];
    assert(id_len > 0);

    const ULONG comment_len = fmt.headerSize[1];
    assert(comment_len > 0);

    const ULONG setup_len = fmt.headerSize[2];
    assert(setup_len > 0);

    BYTE* const id_buf = pb;
    assert(id_buf < pb_end);

    BYTE* const comment_buf = pb += id_len;
    assert(comment_buf < pb_end);

    BYTE* const setup_buf = pb += comment_len;
    assert(setup_buf < pb_end);

    pb += setup_len;
    assert(pb == pb_end);

    pkt.packet = id_buf;
    pkt.bytes = id_len;
    pkt.b_o_s = 1;
    pkt.e_o_s = 0;
    pkt.granulepos = 0;
    pkt.packetno = 0;

    int status = vorbis_synthesis_idheader(&pkt);
    assert(status == 1);  //TODO

    vorbis_info& info = m_info;
    vorbis_info_init(&info);

    vorbis_comment& comment = m_comment;
    vorbis_comment_init(&comment);

    status = vorbis_synthesis_headerin(&info, &comment, &pkt);
    assert(status == 0);
    assert((info.channels >= 0) && (DWORD(info.channels) == fmt.channels));
    assert((info.rate >= 0) && (DWORD(info.rate) == fmt.samplesPerSec));

    pkt.packet = comment_buf;
    pkt.bytes = comment_len;
    pkt.b_o_s = 0;
    ++pkt.packetno;

    status = vorbis_synthesis_headerin(&info, &comment, &pkt);
    assert(status == 0);

    pkt.packet = setup_buf;
    pkt.bytes = setup_len;
    ++pkt.packetno;

    status = vorbis_synthesis_headerin(&info, &comment, &pkt);
    assert(status == 0);

    status = vorbis_synthesis_init(&m_dsp_state, &info);
    assert(status == 0);

    status = vorbis_block_init(&m_dsp_state, &m_block);
    assert(status == 0);

    m_first_reftime = -1;
    //m_start_reftime
    //m_samples

    m_channels.clear();
    m_channels.resize(fmt.channels);

    assert(m_buffers.empty());

    return S_OK;
}


void Inpin::Stop()
{
    while (!m_buffers.empty())
    {
        IMediaSample* const pSample = m_buffers.front();
        m_buffers.pop_front();

        if (pSample)
            pSample->Release();
    }

    const BOOL b = SetEvent(m_hSamples);  //tell thread to terminate
    assert(b);

    m_channels.clear();
    m_first_reftime = -1;

    if (m_packet.packetno < 0)
        return;

    vorbis_block_clear(&m_block);
    vorbis_dsp_clear(&m_dsp_state);
    vorbis_comment_clear(&m_comment);
    vorbis_info_clear(&m_info);

    m_packet.packetno = -1;
}


}  //end namespace WebmVorbisDecoderLib
