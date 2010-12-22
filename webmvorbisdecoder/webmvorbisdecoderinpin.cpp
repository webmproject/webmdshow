// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma warning(disable:4505)  //unreferenced local function has been removed
#include "webmvorbisdecoderfilter.hpp"
#include "webmvorbisdecoderoutpin.hpp"
#include "mediatypeutil.hpp"
#include "graphutil.hpp"
#include "webmtypes.hpp"
#include "vorbistypes.hpp"
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

namespace WebmVorbisDecoderLib
{

Inpin::Inpin(Filter* p) :
    Pin(p, PINDIR_INPUT, L"input"),
    m_bEndOfStream(false),
    m_bFlush(false)
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

    //TODO: init decompressor here?

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

    m_bEndOfStream = true;

    if (IPin* pPin = m_pFilter->m_outpin.m_pPinConnection)
    {
        lock.Release();

#ifdef _DEBUG
        odbgstream os;
        os << "webmvorbisdecoder::inpin::EOS: calling pin->EOS" << endl;
#endif

        const HRESULT hr = pPin->EndOfStream();

#ifdef _DEBUG
        os << "webmvorbisdecoder::inpin::EOS: called pin->EOS; hr=0x"
           << hex << hr << dec
           << endl;
#endif

        return hr;
    }

    return S_OK;
}


HRESULT Inpin::BeginFlush()
{
    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    //TODO:
    //if (m_bFlush)
    //    return S_FALSE;

#if 0 //def _DEBUG
    odbgstream os;
    os << "webmvorbisdecoder::inpin::beginflush" << endl;
#endif

    m_bFlush = true;

    if (IPin* pPin = m_pFilter->m_outpin.m_pPinConnection)
    {
        lock.Release();

        const HRESULT hr = pPin->BeginFlush();
        return hr;
    }

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

#if 0 //def _DEBUG
    odbgstream os;
    os << "webmvorbisdecoder::inpin::endflush" << endl;
#endif

    m_bFlush = false;
    m_bEndOfStream = false;

    if (IPin* pPin = m_pFilter->m_outpin.m_pPinConnection)
    {
        lock.Release();

        const HRESULT hr = pPin->EndFlush();
        return hr;
    }

    return S_OK;
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

    //TODO: check for max channels value?

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

    __int64 start_reftime_, stop_reftime_;
    HRESULT hr = pInSample->GetTime(&start_reftime_, &stop_reftime_);

#define DEBUG_RECEIVE

#ifdef DEBUG_RECEIVE
    {
        odbgstream os;
        os << "webmvorbisdec::inpin::receive: ";

        os << std::fixed << std::setprecision(3);

        if (hr == S_OK)
            os << "start[ms]="
               << double(start_reftime_) / 10000
               << "; stop[ms]="
               << double(stop_reftime_) / 10000
               << "; dt[ms]="
               << double(stop_reftime_ - start_reftime_) / 10000;

        else if (hr == VFW_S_NO_STOP_TIME)
            os << "start[ms]=" << double(start_reftime_) / 10000;

        os << endl;
    }
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

    if (m_first_reftime < 0)
    {
        LONGLONG sp;

        hr = pInSample->GetTime(&m_first_reftime, &sp);

        if (FAILED(hr))
            return hr;

        if (m_first_reftime < 0)
            return S_OK;

        m_start_reftime = m_first_reftime;
        m_samples = 0;
    }

    BYTE* buf_in;

    hr = pInSample->GetPointer(&buf_in);
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

#if 0 //TODO
    if (pInSample->IsPreroll() == S_OK)
        return S_OK;
#endif

    hr = lock.Release();
    assert(SUCCEEDED(hr));

    GraphUtil::IMediaSamplePtr pOutSample;

    hr = outpin.m_pAllocator->GetBuffer(&pOutSample, 0, 0, 0);

    if (FAILED(hr))
        return S_FALSE;

    assert(bool(pOutSample));

    hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_pFilter->m_state == State_Stopped)
        return VFW_E_NOT_RUNNING;

    if (!bool(outpin.m_pPinConnection))  //should never happen
        return S_FALSE;

    if (!bool(outpin.m_pInputPin))  //should never happen
        return S_FALSE;

    typedef VorbisTypes::VORBISFORMAT2 FMT;

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    assert(mt.cbFormat > sizeof(FMT));
    assert(mt.pbFormat);

    const FMT& fmt = (const FMT&)(*mt.pbFormat);
    assert(fmt.channels > 0);
    assert(fmt.samplesPerSec > 0);

    const long size = pOutSample->GetSize();
    assert(size >= 0);

    const long max_samples = size / (fmt.channels * sizeof(float));
    assert(max_samples > 0);

    BYTE* dst;

    hr = pOutSample->GetPointer(&dst);
    assert(SUCCEEDED(hr));
    assert(dst);

    BYTE* const dst_end = dst + size;
    dst_end;

    long total_samples = 0;

    while (total_samples < max_samples)
    {
        float** sv;  //samples vector

        const int count = vorbis_synthesis_pcmout(&m_dsp_state, &sv);

        if (count < 0)  //error?
            return S_FALSE;

        if (count == 0)
            break;

        long samples = max_samples - total_samples;

        if (samples > count)
            samples = count;

        for (long i = 0; i < samples; ++i)
        {
            for (DWORD j = 0; j < fmt.channels; ++j)
            {
                const float* const src = sv[j] + i;
                assert(src);

                memcpy(dst, src, sizeof(float));
                dst += sizeof(float);
            }
        }

        assert(dst <= dst_end);

        status = vorbis_synthesis_read(&m_dsp_state, samples);
        assert(status == 0);

        total_samples += samples;
        assert(total_samples <= max_samples);
    }

    if (total_samples <= 0)
        return S_OK;

    const long len_out = total_samples * fmt.channels * sizeof(float);
    assert(len_out <= size);

    hr = pOutSample->SetActualDataLength(len_out);
    assert(SUCCEEDED(hr));

    m_samples += total_samples;

    const double secs = m_samples / double(fmt.samplesPerSec);
    const double ticks = secs * 10000000;

    LONGLONG stop_reftime = m_first_reftime + static_cast<LONGLONG>(ticks);

    hr = pOutSample->SetTime(&m_start_reftime, &stop_reftime);
    //hr = pOutSample->SetTime(&start_reftime_, 0);
    assert(SUCCEEDED(hr));

    odbgstream os;
    os << "webmvorbisdec::Inpin::Receive: total_samples="
       << total_samples
       << " samples="
       << m_samples
       << " secs="
       << secs
       << " st=" << m_start_reftime
       << " st[ms]=" << (double(m_start_reftime) / 10000)
       << " sp=" << stop_reftime
       << " sp[ms]=" << (double(stop_reftime) / 10000)
       << " dt[ms]=" << (double(stop_reftime - m_start_reftime) / 10000)
       << endl;

    m_start_reftime = stop_reftime;

    hr = pOutSample->SetSyncPoint(TRUE);  //?
    assert(SUCCEEDED(hr));

    hr = pOutSample->SetPreroll(FALSE);
    assert(SUCCEEDED(hr));

    hr = pInSample->IsDiscontinuity();
    hr = pOutSample->SetDiscontinuity(hr == S_OK);

    hr = pOutSample->SetMediaTime(0, 0);

    hr = lock.Release();
    assert(SUCCEEDED(hr));

    os << "webmvorbisdec::Inpin::Receive: calling downstream pin (before)"
       << endl;

    hr = outpin.m_pInputPin->Receive(pOutSample);

    os << "webmvorbisdec::Inpin::Receive: called downstream pin (after); "
       << "hr=0x" << hex << hr << dec
       << endl;

    return hr;
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
    Filter::Lock lock;

    const HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return S_OK;  //?

    if (IMemInputPin* pPin = m_pFilter->m_outpin.m_pInputPin)
    {
        lock.Release();
        return pPin->ReceiveCanBlock();
    }

    return S_FALSE;
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

    ogg_packet& pkt = m_packet;

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

    return S_OK;
}


void Inpin::Stop()
{
    if (m_packet.packetno < 0)
        return;

    vorbis_block_clear(&m_block);
    vorbis_dsp_clear(&m_dsp_state);
    vorbis_comment_clear(&m_comment);
    vorbis_info_clear(&m_info);

    m_packet.packetno = -1;
}


}  //end namespace WebmVorbisDecoderLib
