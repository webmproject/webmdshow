// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma warning(disable:4505)  //unreferenced local function has been removed
#include "webmvorbisencoderfilter.hpp"
#include "webmvorbisencoderoutpin.hpp"
#include "mediatypeutil.hpp"
#include "graphutil.hpp"
#include "webmtypes.hpp"
#include "vorbistypes.hpp"
#include "vorbis/vorbisenc.h"
#include "vorbis/codec.h"  //in libvorbis
#include <vfwmsgs.h>
#include <uuids.h>
#include <mmreg.h>
#include <cassert>
//#include <amvideo.h>
//#include <evcode.h>
#ifdef _DEBUG
#include <iomanip>
#include "odbgstream.hpp"
using std::endl;
using std::hex;
using std::dec;
using std::fixed;
using std::setprecision;
#endif

namespace WebmVorbisEncoderLib
{

Inpin::Inpin(Filter* pFilter) :
    Pin(PINDIR_INPUT, L"input"),
    m_pFilter(pFilter),
    m_bEndOfStream(false),
    m_bFlush(false),
    m_bDone(false),
    m_bStopped(true),
    m_first_reftime(-1),
    m_start_reftime(-1),
    m_start_samples(-1)
{
    AM_MEDIA_TYPE mt;

    //This is essentially a "partial media type".  That's all we
    //can say right now without being connected.

    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = MEDIASUBTYPE_IEEE_FLOAT;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;
    mt.formattype = GUID_NULL;
    mt.pUnk = 0;
    mt.cbFormat = 0;
    mt.pbFormat = 0;

    m_preferred_mtv.Add(mt);

    m_info.channels = 0;  //means "not initialized"

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
    assert(mt.formattype == FORMAT_WaveFormatEx);
    assert(mt.cbFormat >= sizeof(WAVEFORMATEX));
    assert(mt.pbFormat);

    hr = m_connection_mtv.Add(mt);

    if (FAILED(hr))
        return hr;

    const WAVEFORMATEX& wfx = (WAVEFORMATEX&)(*mt.pbFormat);
    assert(wfx.nChannels > 0);
    assert(wfx.nChannels <= 2);  //TODO: proper channel mapping
    assert(wfx.nSamplesPerSec > 0);

    //Initialize vorbis encoder library, in order to generate
    //the 3 header packets, and thus the output media type.

    assert(m_info.channels <= 0);

    vorbis_info_init(&m_info);

    int result = vorbis_encode_init_vbr(
                    &m_info,
                    wfx.nChannels,
                    wfx.nSamplesPerSec,
                    1.0);  //?

    if (result != 0)  //error
        return E_FAIL;

    assert(m_info.channels > 0);
    assert(m_info.channels == wfx.nChannels);

    result = vorbis_analysis_init(&m_dsp_state, &m_info);
    assert(result == 0);

    vorbis_comment comment_hdr;

    vorbis_comment_init(&comment_hdr);

    ogg_packet ident, comment_pkt, code;

    result = vorbis_analysis_headerout(
                &m_dsp_state,
                &comment_hdr,
                &ident,
                &comment_pkt,
                &code);

    if (result != 0)
    {
        vorbis_info_clear(&m_info);
        m_info.channels = 0;

        return E_FAIL;
    }

    result = vorbis_block_init(&m_dsp_state, &m_block);
    assert(result == 0);

    m_pPinConnection = pin;

    OnConnect(wfx, ident, comment_pkt, code);

    //Encoding workflow:
    //http://xiph.org/vorbis/doc/libvorbis/overview.html
    //http://xiph.org/vorbis/doc/vorbisenc/overview.html
    //http://xiph.org/vorbis/doc/vorbisenc/examples.html

    return S_OK;
}


void Inpin::OnConnect(
    const WAVEFORMATEX& wfx,
    const ogg_packet& ident,
    const ogg_packet& comment,
    const ogg_packet& code)
{
    typedef VorbisTypes::VORBISFORMAT2 FMT;

    size_t cb = sizeof(FMT);

    assert(ident.packetno == 0);
    assert(ident.packet);
    assert(ident.bytes >= 7);
    assert(memcmp(ident.packet, "\x01vorbis", 7) == 0);

    cb += ident.bytes;

    assert(comment.packetno == 1);
    assert(comment.packet);
    assert(comment.bytes >= 7);
    assert(memcmp(comment.packet, "\x03vorbis", 7) == 0);

    cb += comment.bytes;

    assert(code.packetno == 2);
    assert(code.packet);
    assert(code.bytes >= 7);
    assert(memcmp(code.packet, "\x05vorbis", 7) == 0);

    cb += code.bytes;

    AM_MEDIA_TYPE mt;

    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = VorbisTypes::MEDIASUBTYPE_Vorbis2;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;
    mt.formattype = VorbisTypes::FORMAT_Vorbis2;
    mt.pUnk = 0;

    mt.cbFormat = cb;
    mt.pbFormat = (BYTE*)_malloca(cb);

    BYTE* pb = mt.pbFormat;

    FMT& fmt = (FMT&)(*pb);

    fmt.channels = wfx.nChannels;
    fmt.samplesPerSec = wfx.nSamplesPerSec;
    fmt.bitsPerSample = 0;  //TODO
    fmt.headerSize[0] = ident.bytes;
    fmt.headerSize[1] = comment.bytes;
    fmt.headerSize[2] = code.bytes;

    pb += sizeof(FMT);

    memcpy(pb, ident.packet, ident.bytes);
    pb += ident.bytes;

    memcpy(pb, comment.packet, comment.bytes);
    pb += comment.bytes;

    memcpy(pb, code.packet, code.bytes);
    pb += code.bytes;
    assert((pb - mt.pbFormat) == ptrdiff_t(mt.cbFormat));

    m_pFilter->m_outpin.OnInpinConnect(mt);
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
    os << "webmvorbisencoder::inpin::EOS" << endl;
#endif

    if (m_bFlush)
        return S_FALSE;  //?

    m_bEndOfStream = true;

    const int status = vorbis_analysis_wrote(&m_dsp_state, 0);  //means "EOS"
    status;
    assert(status == 0);

    hr = lock.Release();
    assert(SUCCEEDED(hr));

    if (FAILED(hr))
        return hr;

    return PopulateSamples();
}


HRESULT Inpin::BeginFlush()
{
#ifdef _DEBUG
    odbgstream os;
    os << "webmvorbisencoder::inpin::beginflush(begin)" << endl;
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
        os << "webmvorbisencoder::inpin::beginflush:"
           << " flushing downstream filter"
           << endl;
#endif

        hr = pPin->BeginFlush();  //safe to do this here, while holding lock?

#ifdef _DEBUG
        os << "webmvorbisencoder::inpin::beginflush:"
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
    os << "webmvorbisencoder::inpin::beginflush(end #2): thread terminated"
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
    os << "webmvorbisencoder::inpin::endflush(begin)" << endl;
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

    Outpin& outpin = m_pFilter->m_outpin;

    if (IPin* const pPin = outpin.m_pPinConnection)
    {
#ifdef _DEBUG
        os << "webmvorbisencoder::inpin::endflush: calling endflush on"
           << " downstream filter"
           << endl;
#endif

        hr = pPin->EndFlush();  //safe to call this without releasing lock?

        if (m_pFilter->m_state != State_Stopped)
        {
#ifdef _DEBUG
            os << "webmvorbisencoder::inpin::endflush: state != stopped,"
               << " so starting thread"
               << endl;
#endif

            outpin.StartThread();

#ifdef _DEBUG
            os << "webmvorbisencoder::inpin::endflush: started thread"
               << endl;
#endif
        }
    }

#ifdef _DEBUG
    os << "webmvorbisencoder::inpin::endflush(end)" << endl;
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

    if (mt.subtype != MEDIASUBTYPE_IEEE_FLOAT)
        return S_FALSE;

    if (mt.formattype != FORMAT_WaveFormatEx)
        return S_FALSE;

    if (mt.pbFormat == 0)
        return S_FALSE;

    if (mt.cbFormat < 18)  //WAVEFORMATEX
        return S_FALSE;

    const WAVEFORMATEX& wfx = (WAVEFORMATEX&)(*mt.pbFormat);

    if (wfx.wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
        return S_FALSE;

    if (wfx.cbSize > 0)  //weird
        return S_FALSE;

    if (wfx.nChannels == 0)
        return S_FALSE;

    if (wfx.nChannels > 2)  //TODO
        return S_FALSE;

    if (wfx.nSamplesPerSec == 0)
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
    os << "webmvorbisencoder::inpin::NotifyAllocator: props.cBuffers="
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

    if (m_bStopped)  //weird
        return VFW_E_WRONG_STATE;

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

        if (m_first_reftime < 0)  //TODO: Vorbis can handle this case
            return S_OK;

        m_start_reftime = m_first_reftime;
        m_start_samples = 0;

#ifdef DEBUG_RECEIVE
        odbgstream os;
        os << std::fixed << std::setprecision(3);

        os << "\nwebmvorbisdec::Inpin::Receive: RESET FIRST REFTIME;"
           << " st=" << m_start_reftime
           << " st[sec]=" << (double(m_start_reftime) / 10000000)
           << endl;
#endif

#if 0  //TODO
        const int status = vorbis_synthesis_restart(&m_dsp_state);
        status;
        assert(status == 0);  //success
#endif

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

    Encode(pInSample);

    hr = lock.Release();
    assert(SUCCEEDED(hr));

    if (FAILED(hr))
        return hr;

    return PopulateSamples();
}


void Inpin::Encode(IMediaSample* s)
{
    assert(s);

    const long len = s->GetActualDataLength();

    if (len <= 0)
        return;

    const int channels = m_info.channels;
    assert(channels > 0);

    const long block_align = sizeof(float) * channels;
    assert(len % block_align == 0);

    const long block_count = len / block_align;
    assert(block_count > 0);  //distinguished value 0 means "end of stream"

    const long sample_count = len / sizeof(float);
    assert(sample_count > 0);

    BYTE* buf;

    HRESULT hr = s->GetPointer(&buf);
    assert(SUCCEEDED(hr));
    assert(buf);

    const float* const src_begin = reinterpret_cast<float*>(buf);
    const float* src = src_begin;

    const float* const src_end = src_begin + sample_count;
    src_end;

    float** dst = vorbis_analysis_buffer(&m_dsp_state, block_count);
    assert(dst);

    //TODO: verify left-right channel order issue
    //TODO: extend to channels > 2

    for (int block = 0; block < block_count; ++block)
        for (int channel = 0; channel < channels; ++channel)
        {
            memcpy(dst[channel] + block, src, sizeof(float));
            ++src;
        }

    assert(src == src_end);
    assert((src_end - src_begin) / channels == block_count);

    const int status = vorbis_analysis_wrote(&m_dsp_state, block_count);
    status;
    assert(status == 0);
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

        if (m_bStopped)  //weird
            return VFW_E_WRONG_STATE;

        //if (m_bEndOfStream)
        //  return VFW_SAMPLE_REJECTED_EOS;

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

        int status = vorbis_analysis_blockout(&m_dsp_state, &m_block);

        if (status < 0)  //error
            return E_FAIL;

        if (status == 0)  //no block available
        {
#if 0  //see test below
            //NOTE: commenting out this branch assumes
            //that if we request EOS, then the encoder pushes
            //out at least one packet, with the EOS indication
            //specified.

            if (m_bEndOfStream)
            {
                m_buffers.push_back(0);

                const BOOL b = SetEvent(m_hSamples);
                b;
                assert(b);
            }
#endif
            return S_OK;
        }

        //status=1 means "success, more blocks available"

        ogg_packet pkt;

        status = vorbis_analysis(&m_block, &pkt);
        assert(status == 0);
        //TODO: vet seq no.

        PopulateSample(pOutSample, pkt);

        m_buffers.push_back(pOutSample.Detach());

        if (pkt.e_o_s)
            m_buffers.push_back(0);

        const BOOL b = SetEvent(m_hSamples);
        b;
        assert(b);

        if (pkt.e_o_s)
            return S_OK;
    }
}


void Inpin::PopulateSample(
    IMediaSample* pOutSample,
    const ogg_packet& pkt)
{
    assert(m_start_samples >= 0);
    assert(m_start_reftime >= 0);

    assert(pOutSample);
    assert(pkt.packet);
    assert(pkt.bytes > 0);
    assert(pkt.granulepos >= 0);
    assert(pkt.granulepos >= m_start_samples);

    const long size = pOutSample->GetSize();
    assert(size >= 0);
    assert(size >= pkt.bytes);

    BYTE* dst;

    HRESULT hr = pOutSample->GetPointer(&dst);
    assert(SUCCEEDED(hr));
    assert(dst);

    memcpy(dst, pkt.packet, pkt.bytes);

    hr = pOutSample->SetActualDataLength(pkt.bytes);
    assert(SUCCEEDED(hr));

    const double stop_samples = static_cast<double>(pkt.granulepos);
    const double samples_per_sec = m_info.rate;

    const double stop_secs = stop_samples / samples_per_sec;

    const double stop_ticks_ = stop_secs * 10000000;
    const LONGLONG stop_ticks = static_cast<LONGLONG>(stop_ticks_);

    LONGLONG stop_reftime = m_first_reftime + stop_ticks;
    assert(stop_reftime >= m_start_reftime);

#if 0  //TODO: determine whether we need this
    if (stop_reftime <= m_start_reftime)
    {
#ifdef _DEBUG
        odbgstream os;
        os << "WebmVorbisEnc::Inpin::PopulateSample: start=stop"
           << " pkt.granulepos=" << pkt.granulepos
           << " pkt.pkt_no=" << pkt.packetno
           << " st=" << m_start_reftime
           << " sp=" << stop_reftime
           << " sp-st=" << (stop_reftime - m_start_reftime)
           << " stop_secs=" << fixed << setprecision(3) << stop_secs
           << endl;
#endif

        stop_reftime = m_start_reftime + 1;  //use 1 tick or 1 ms?
    }
#endif

    hr = pOutSample->SetTime(&m_start_reftime, &stop_reftime);
    assert(SUCCEEDED(hr));

    m_start_samples = pkt.granulepos;
    m_start_reftime = stop_reftime;

    hr = pOutSample->SetSyncPoint(TRUE);  //?
    assert(SUCCEEDED(hr));

    hr = pOutSample->SetPreroll(FALSE);
    assert(SUCCEEDED(hr));

    hr = pOutSample->SetDiscontinuity(m_bDiscontinuity ? TRUE : FALSE);
    assert(SUCCEEDED(hr));

    m_bDiscontinuity = false;

    hr = pOutSample->SetMediaTime(0, 0);
    assert(SUCCEEDED(hr));
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

        if (m_bStopped)
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
    m_pFilter->m_outpin.OnInpinDisconnect();

    if (m_info.channels > 0)
    {
        vorbis_block_clear(&m_block);
        vorbis_dsp_clear(&m_dsp_state);
        vorbis_info_clear(&m_info);

        m_info.channels = 0;  //means "not initialized"
    }

    return S_OK;
}


HRESULT Inpin::GetName(PIN_INFO& info) const
{
    const wchar_t name[] = L"IEEE Float";

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

HRESULT Inpin::Seize(CLockable::Lock& lock)
{
    return lock.Seize(m_pFilter);
}

FILTER_STATE Inpin::GetState()
{
    return m_pFilter->m_state;
}

IBaseFilter* Inpin::GetFilter()  //no addref here
{
    return m_pFilter;  //no addref here!
}

void Inpin::Start()
{
    assert(m_buffers.empty());

    m_bEndOfStream = false;
    m_bFlush = false;
    m_bDone = false;
    m_bStopped = false;
    m_first_reftime = -1;
    m_start_reftime = -1;
    m_start_samples = -1;

    if (m_info.channels > 0)  //connected
    {
        int result = vorbis_block_clear(&m_block);
        assert(result == 0);

        vorbis_dsp_clear(&m_dsp_state);

        result = vorbis_analysis_init(&m_dsp_state, &m_info);
        assert(result == 0);

        result = vorbis_block_init(&m_dsp_state, &m_block);
        assert(result == 0);
    }
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

    m_bStopped = true;

    //TODO: we really should wait here for the output pin thread
    //to terminate, in order to ensure that we transition to the
    //bDone state immediately.
}

}  //end namespace WebmVorbisEncoderLib
