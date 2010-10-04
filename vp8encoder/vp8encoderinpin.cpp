// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma warning(disable:4505)  //unreferenced local function has been removed
#include "vp8encoderfilter.hpp"
#include "vp8encoderoutpin.hpp"
#include "mediatypeutil.hpp"
#include "webmtypes.hpp"
#include "vpx/vp8cx.h"
#include <vfwmsgs.h>
#include <uuids.h>
#include <cassert>
#include <amvideo.h>   //VideoInfoHeader
#include <dvdmedia.h>  //VideoInfoHeader2
#ifdef _DEBUG
#include "odbgstream.hpp"
#include <iomanip>
using std::endl;
using std::hex;
using std::dec;
#endif

namespace VP8EncoderLib
{

Inpin::Inpin(Filter* p) :
    Pin(p, PINDIR_INPUT, L"input"),
    m_bEndOfStream(false),
    m_bFlush(false),
    m_bDiscontinuity(true),
    m_buf(0),
    m_buflen(0)
{
    AM_MEDIA_TYPE mt;

    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_YV12;
    mt.bFixedSizeSamples = TRUE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;
    mt.formattype = GUID_NULL;
    mt.pUnk = 0;
    mt.cbFormat = 0;
    mt.pbFormat = 0;

    m_preferred_mtv.Add(mt);

    mt.subtype = WebmTypes::MEDIASUBTYPE_I420;
    m_preferred_mtv.Add(mt);

    mt.subtype = MEDIASUBTYPE_YUY2;
    m_preferred_mtv.Add(mt);

    mt.subtype = MEDIASUBTYPE_YUYV;
    m_preferred_mtv.Add(mt);
}


Inpin::~Inpin()
{
    PurgePending();

    delete[] m_buf;
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

    const ULONG m = 2;  //number of output pins

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

    pa[0] = &m_pFilter->m_outpin_video;
    pa[0]->AddRef();

    pa[1] = &m_pFilter->m_outpin_preview;
    pa[1]->AddRef();

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

    m_pFilter->m_outpin_video.OnInpinConnect();
    m_pFilter->m_outpin_preview.OnInpinConnect();

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

    if (m_pFilter->m_state == State_Stopped)
        return VFW_E_NOT_RUNNING;

    if (m_bFlush)
        return S_FALSE;  //TODO: correct return value here?

    if (m_bEndOfStream)
        return S_FALSE;

    m_bEndOfStream = true;

    const vpx_codec_err_t err = vpx_codec_encode(&m_ctx, 0, 0, 0, 0, 0);
    err;
    assert(err == VPX_CODEC_OK);  //TODO

    const VP8PassMode m = m_pFilter->GetPassMode();

    OutpinVideo& outpin = m_pFilter->m_outpin_video;

    vpx_codec_iter_t iter = 0;

    for (;;)
    {
        const vpx_codec_cx_pkt_t* const pkt =
            vpx_codec_get_cx_data(&m_ctx, &iter);

        if (pkt == 0)
            break;

        switch (pkt->kind)
        {
            case VPX_CODEC_CX_FRAME_PKT:
                assert(m != kPassModeFirstPass);
                AppendFrame(pkt);
                break;

            case VPX_CODEC_STATS_PKT:
                assert(m == kPassModeFirstPass);
                outpin.WriteStats(pkt);
                break;

            default:
                assert(false);
                return E_FAIL;
        }
    }

    if (m != kPassModeFirstPass)
    {
        while (!m_pending.empty())
        {
            if (!bool(outpin.m_pAllocator))
                break;

            lock.Release();

            GraphUtil::IMediaSamplePtr pOutSample;

            const HRESULT hrGetBuffer =
                outpin.m_pAllocator->GetBuffer(&pOutSample, 0, 0, 0);

            hr = lock.Seize(m_pFilter);

            if (FAILED(hr))
                return hr;

            if (FAILED(hrGetBuffer))
                break;

            assert(bool(pOutSample));

            PopulateSample(pOutSample);  //consume pending frame

            if (!bool(outpin.m_pInputPin))
                break;

            lock.Release();

            const HRESULT hrReceive = outpin.m_pInputPin->Receive(pOutSample);

            hr = lock.Seize(m_pFilter);

            if (FAILED(hr))
                return hr;

            if (hrReceive != S_OK)
                break;
        }
    }

    //We hold the lock.

    if (IPin* pPin = m_pFilter->m_outpin_preview.m_pPinConnection)
    {
        lock.Release();

        hr = pPin->EndOfStream();

        hr = lock.Seize(m_pFilter);
        assert(SUCCEEDED(hr));  //TODO
    }

    //We hold the lock.

    if (IPin* pPin = outpin.m_pPinConnection)
    {
        lock.Release();
        hr = pPin->EndOfStream();
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

    //TODO: need this?
    //if (m_bFlush)
    //    return S_FALSE;

    m_bFlush = true;

    //We hold the lock

    if (IPin* pPin = m_pFilter->m_outpin_preview.m_pPinConnection)
    {
        lock.Release();

        hr = pPin->BeginFlush();

        hr = lock.Seize(m_pFilter);
        assert(SUCCEEDED(hr));  //TODO
    }

    //We hold the lock

    if (IPin* pPin = m_pFilter->m_outpin_video.m_pPinConnection)
    {
        lock.Release();
        hr = pPin->BeginFlush();
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

    m_bFlush = false;

    //We hold the lock

    if (IPin* pPin = m_pFilter->m_outpin_preview.m_pPinConnection)
    {
        lock.Release();

        hr = pPin->EndFlush();

        hr = lock.Seize(m_pFilter);
        assert(SUCCEEDED(hr));  //TODO
    }

    //We hold the lock

    if (IPin* pPin = m_pFilter->m_outpin_video.m_pPinConnection)
    {
        lock.Release();
        hr = pPin->EndFlush();
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

    //We hold the lock

    if (IPin* pPin = m_pFilter->m_outpin_preview.m_pPinConnection)
    {
        lock.Release();

        hr = pPin->NewSegment(st, sp, r);

        hr = lock.Seize(m_pFilter);
        assert(SUCCEEDED(hr));  //TODO
    }

    //We hold the lock

    if (IPin* pPin = m_pFilter->m_outpin_video.m_pPinConnection)
    {
        lock.Release();
        hr = pPin->NewSegment(st, sp, r);
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

    if (mt.subtype == MEDIASUBTYPE_YV12)
        __noop;

    else if (mt.subtype == WebmTypes::MEDIASUBTYPE_I420)
        __noop;

    else if (mt.subtype == MEDIASUBTYPE_YUY2)
        __noop;

    else if (mt.subtype == MEDIASUBTYPE_YUYV)
        __noop;

    else
        return S_FALSE;

    if (mt.pbFormat == 0)
        return S_FALSE;

    const BITMAPINFOHEADER* pbmih;

    if (mt.formattype == FORMAT_VideoInfo)
    {
        if (mt.cbFormat < sizeof(VIDEOINFOHEADER))
            return S_FALSE;

        const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);

        if (vih.AvgTimePerFrame <= 0)
            return S_FALSE;

        pbmih = &vih.bmiHeader;
    }
    else if (mt.formattype == FORMAT_VideoInfo2)
    {
        if (mt.cbFormat < sizeof(VIDEOINFOHEADER2))
            return S_FALSE;

        const VIDEOINFOHEADER2& vih = (VIDEOINFOHEADER2&)(*mt.pbFormat);

        if (vih.AvgTimePerFrame <= 0)
            return S_FALSE;

        pbmih = &vih.bmiHeader;
    }
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
    os << "vp8enc::inpin::NotifyAllocator: props.cBuffers="
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

    if (pInSample->IsPreroll() == S_OK)  //bogus for encode
        return S_OK;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    if (m_pFilter->m_state == State_Stopped)
        return VFW_E_NOT_RUNNING;

    if (m_bEndOfStream)
        return VFW_E_SAMPLE_REJECTED_EOS;

    if (m_bFlush)
        return S_FALSE;

    const BITMAPINFOHEADER& bmih = GetBMIH();

    const LONG w = bmih.biWidth;
    assert(w > 0);
    assert((w % 2) == 0);  //TODO

    const LONG h = labs(bmih.biHeight);
    assert(h > 0);
    assert((h % 2) == 0);  //TODO

    const long len = pInSample->GetActualDataLength();
    len;
    assert(len >= 0);

    img_fmt_t fmt;

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];

    if (mt.subtype == MEDIASUBTYPE_YV12)
        fmt = IMG_FMT_YV12;

    else if (mt.subtype == WebmTypes::MEDIASUBTYPE_I420)
        fmt = IMG_FMT_I420;

    else if ((mt.subtype == MEDIASUBTYPE_YUY2) ||
             (mt.subtype == MEDIASUBTYPE_YUYV))
    {
        fmt = IMG_FMT_YUY2;
    }
    else
    {
        assert(false);
        return E_FAIL;
    }

    BYTE* inbuf;

    hr = pInSample->GetPointer(&inbuf);
    assert(SUCCEEDED(hr));
    assert(inbuf);

    BYTE* imgbuf;

    switch (fmt)
    {
        case IMG_FMT_YV12:
        case IMG_FMT_I420:
        {
            assert(len == (w*h + 2*((w+1)/2)*((h+1)/2)));

            imgbuf = inbuf;
            assert(imgbuf);

            break;
        }
        case IMG_FMT_YUY2:
        {
            assert(len == ((2*w) * h));

            fmt = IMG_FMT_YV12;

            imgbuf = ConvertYUY2ToYV12(inbuf, w, h);
            assert(imgbuf);

            break;
        }
        default:
            assert(false);
            return E_FAIL;
    }

    vpx_image_t img_;
    vpx_image_t* const img = vpx_img_wrap(&img_, fmt, w, h, 1, imgbuf);
    assert(img);
    assert(img == &img_);

    //TODO: set this based on vih.rcSource
    const int status = vpx_img_set_rect(img, 0, 0, w, h);
    status;
    assert(status == 0);

    m_pFilter->m_outpin_preview.Render(lock, img);

    OutpinVideo& outpin = m_pFilter->m_outpin_video;

    if (!bool(outpin.m_pPinConnection))
        return S_OK;

    __int64 st, sp;

    hr = pInSample->GetTime(&st, &sp);

    if (FAILED(hr))
        return hr;

    assert(st >= 0);
    assert(st >= m_start_reftime);

    m_start_reftime = st;

    const __int64 duration_ = GetAvgTimePerFrame();
    assert(duration_ > 0);

    const unsigned long d = static_cast<unsigned long>(duration_);

    hr = pInSample->IsDiscontinuity();
    const bool bDiscontinuity = (hr == S_OK);

    vpx_enc_frame_flags_t f = 0;

    if (m_pFilter->m_bForceKeyframe || bDiscontinuity || (st <= 0))
    {
        f |= VPX_EFLAG_FORCE_KF;
        m_pFilter->m_bForceKeyframe = false;
    }

    const Filter::Config::int32_t deadline_ = m_pFilter->m_cfg.deadline;
    const ULONG dl = (deadline_ >= 0) ? deadline_ : kDeadlineGoodQuality;

    const vpx_codec_err_t err = vpx_codec_encode(&m_ctx, img, st, d, f, dl);
    err;
    assert(err == VPX_CODEC_OK);  //TODO

    const VP8PassMode m = m_pFilter->GetPassMode();

    vpx_codec_iter_t iter = 0;

    for (;;)
    {
        const vpx_codec_cx_pkt_t* const pkt =
            vpx_codec_get_cx_data(&m_ctx, &iter);

        if (pkt == 0)
            break;

        switch (pkt->kind)
        {
            case VPX_CODEC_CX_FRAME_PKT:
                assert(m != kPassModeFirstPass);
                AppendFrame(pkt);
                break;

            case VPX_CODEC_STATS_PKT:
                assert(m == kPassModeFirstPass);
                outpin.WriteStats(pkt);
                break;

            default:
                assert(false);
                return E_FAIL;
        }
    }

    if (m == kPassModeFirstPass)
        return S_OK;  //nothing else to do

    while (!m_pending.empty())
    {
        if (!bool(outpin.m_pAllocator))
            return VFW_E_NO_ALLOCATOR;

        lock.Release();

        GraphUtil::IMediaSamplePtr pOutSample;

        hr = outpin.m_pAllocator->GetBuffer(&pOutSample, 0, 0, 0);

        if (FAILED(hr))
            return hr;

        assert(bool(pOutSample));

        hr = lock.Seize(m_pFilter);

        if (FAILED(hr))
            return hr;

        PopulateSample(pOutSample);  //consume pending frame

        if (!bool(outpin.m_pInputPin))
            return S_FALSE;

        lock.Release();

        hr = outpin.m_pInputPin->Receive(pOutSample);

        if (hr != S_OK)
            return hr;

        hr = lock.Seize(m_pFilter);

        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}


void Inpin::PopulateSample(IMediaSample* p)
{
    assert(p);
    assert(!m_pending.empty());

#ifdef _DEBUG
    {
        AM_MEDIA_TYPE* pmt;

        const HRESULT hr = p->GetMediaType(&pmt);
        assert(FAILED(hr) || (pmt == 0));
    }
#endif

    _COM_SMARTPTR_TYPEDEF(IVP8Sample, __uuidof(IVP8Sample));

    const IVP8SamplePtr pSample(p);
    assert(bool(pSample));

    IVP8Sample::Frame& f = pSample->GetFrame();
    assert(f.buf == 0);  //should have already been reclaimed

    f = m_pending.front();
    assert(f.buf);

    m_pending.pop_front();

    HRESULT hr = p->SetPreroll(FALSE);
    assert(SUCCEEDED(hr));

    hr = p->SetDiscontinuity(m_bDiscontinuity ? TRUE : FALSE);
    assert(SUCCEEDED(hr));

    m_bDiscontinuity = false;
}


void Inpin::AppendFrame(const vpx_codec_cx_pkt_t* pkt)
{
    assert(pkt);
    assert(pkt->kind == VPX_CODEC_CX_FRAME_PKT);

    IVP8Sample::Frame f;

    const HRESULT hr = m_pFilter->m_outpin_video.GetFrame(f);
    assert(SUCCEEDED(hr));
    assert(f.buf);

    const size_t len_ = pkt->data.frame.sz;
    const long len = static_cast<long>(len_);

    const long size = f.buflen - f.off;
    assert(size >= len);

    BYTE* tgt = f.buf + f.off;
    //assert(intptr_t(tgt - props.cbPrefix) % props.cbAlign == 0);

    memcpy(tgt, pkt->data.frame.buf, len);

    f.len = len;

    f.start = pkt->data.frame.pts;
    assert(f.start >= 0);

#if 0
    f.stop = f.start + pkt->data.frame.duration;
    assert(f.stop > f.start);
#else
    f.stop = -1;  //don't send stop time
#endif

    const uint32_t bKey = pkt->data.frame.flags & VPX_FRAME_IS_KEY;

    f.key = bKey ? true : false;

    m_pending.push_back(f);

#if 0 //def _DEBUG
    odbgstream os;
    os << "vp8encoder::inpin::appendframe: pending.size="
       << m_pending.size()
       << endl;
#endif
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

    if (IMemInputPin* pPin = m_pFilter->m_outpin_video.m_pInputPin)
    {
        lock.Release();
        return pPin->ReceiveCanBlock();
    }

    return S_FALSE;
}


HRESULT Inpin::OnDisconnect()
{
    HRESULT hr = m_pFilter->m_outpin_preview.OnInpinDisconnect();
    assert(SUCCEEDED(hr));

    hr = m_pFilter->m_outpin_video.OnInpinDisconnect();
    assert(SUCCEEDED(hr));

    return S_OK;
}


std::wstring Inpin::GetName() const
{
    return L"YUV";
}


void Inpin::PurgePending()
{
    while (!m_pending.empty())
    {
        IVP8Sample::Frame& f = m_pending.front();
        assert(f.buf);

        delete[] f.buf;

        m_pending.pop_front();
    }
}


HRESULT Inpin::Start()
{
    m_bDiscontinuity = true;
    m_bEndOfStream = false;
    m_bFlush = false;
    m_start_reftime = 0;

    PurgePending();

    const BITMAPINFOHEADER& bmih = GetBMIH();

    const LONG w = bmih.biWidth;
    assert(w > 0);
    assert((w % 2) == 0);  //TODO

    const LONG h = labs(bmih.biHeight);
    assert(h > 0);
    assert((h % 2) == 0);  //TODO

    vpx_codec_iface_t& vp8 = vpx_codec_vp8_cx_algo;

    vpx_codec_enc_cfg_t& tgt = m_cfg;

    vpx_codec_err_t err = vpx_codec_enc_config_default(&vp8, &tgt, 0);

    if (err != VPX_CODEC_OK)
        return E_FAIL;

    tgt.g_w = w;
    tgt.g_h = h;
    tgt.g_timebase.num = 1;
    tgt.g_timebase.den = 10000000;  //100-ns ticks

    SetConfig();

    err = vpx_codec_enc_init(&m_ctx, &vp8, &tgt, 0);

    if (err != VPX_CODEC_OK)
    {
#ifdef _DEBUG
        const char* str = vpx_codec_error_detail(&m_ctx);
        str;
#endif

        return E_FAIL;
    }

    err = SetTokenPartitions();

    if (err != VPX_CODEC_OK)
    {
        const vpx_codec_err_t err = vpx_codec_destroy(&m_ctx);
        err;
        assert(err == VPX_CODEC_OK);

        return E_FAIL;
    }

    return S_OK;
}

void Inpin::Stop()
{
    const vpx_codec_err_t err = vpx_codec_destroy(&m_ctx);
    err;
    assert(err == VPX_CODEC_OK);

    memset(&m_ctx, 0, sizeof m_ctx);
}


HRESULT Inpin::OnApplySettings(std::wstring& msg)
{
    SetConfig();

    const vpx_codec_err_t err = vpx_codec_enc_config_set(&m_ctx, &m_cfg);

    if (err == VPX_CODEC_OK)
    {
        msg.clear();
        return S_OK;
    }

    const char* const str = vpx_codec_error_detail(&m_ctx);

    const int cch = MultiByteToWideChar(
                        CP_UTF8,
                        0,  //TODO: MB_ERR_INVALID_CHARS
                        str,
                        -1,  //include NUL terminator in result
                        0,
                        0);  //request length

    assert(cch > 0);

    const size_t cb = cch * sizeof(wchar_t);
    wchar_t* const wstr = (wchar_t*)_alloca(cb);

    const int cch2 = MultiByteToWideChar(
                        CP_UTF8,
                        0,  //TODO: MB_ERR_INVALID_CHARS
                        str,
                        -1,
                        wstr,
                        cch);

    cch2;
    assert(cch2 > 0);
    assert(cch2 == cch);

    msg.assign(wstr);
    return E_FAIL;
}


void Inpin::SetConfig()
{
    const Filter::Config& src = m_pFilter->m_cfg;
    vpx_codec_enc_cfg_t& tgt = m_cfg;

    if (src.threads >= 0)
        tgt.g_threads = src.threads;

    if (src.error_resilient >= 0)
        tgt.g_error_resilient = src.error_resilient;

    switch (src.pass_mode)
    {
        default:
            break;

        case kPassModeOnePass:
            tgt.g_pass = VPX_RC_ONE_PASS;
            break;

        case kPassModeFirstPass:
            tgt.g_pass = VPX_RC_FIRST_PASS;
            break;

        case kPassModeLastPass:
            tgt.g_pass = VPX_RC_LAST_PASS;

            if (src.two_pass_stats_buflen >= 0)
            {
                vpx_fixed_buf& stats = tgt.rc_twopass_stats_in;

                stats.buf = const_cast<BYTE*>(src.two_pass_stats_buf);
                stats.sz = static_cast<size_t>(src.two_pass_stats_buflen);
            }

            break;
    }

    if (src.two_pass_vbr_bias_pct >= 0)
        tgt.rc_2pass_vbr_bias_pct = src.two_pass_vbr_bias_pct;

    if (src.two_pass_vbr_minsection_pct >= 0)
        tgt.rc_2pass_vbr_minsection_pct = src.two_pass_vbr_minsection_pct;

    if (src.two_pass_vbr_maxsection_pct >= 0)
        tgt.rc_2pass_vbr_maxsection_pct = src.two_pass_vbr_maxsection_pct;

    if (src.lag_in_frames >= 0)
        tgt.g_lag_in_frames = src.lag_in_frames;

    if (src.dropframe_thresh < 0)
        tgt.rc_dropframe_thresh = 0;  //disable by default
    else
        tgt.rc_dropframe_thresh = src.dropframe_thresh;

    if (src.resize_allowed >= 0)
        tgt.rc_resize_allowed = src.resize_allowed;

    if (src.resize_up_thresh >= 0)
        tgt.rc_resize_up_thresh = src.resize_up_thresh;

    if (src.resize_down_thresh >= 0)
        tgt.rc_resize_down_thresh = src.resize_down_thresh;

    switch (src.end_usage)
    {
        default:
            break;

        case kEndUsageVBR:
            tgt.rc_end_usage = VPX_VBR;
            break;

        case kEndUsageCBR:
            tgt.rc_end_usage = VPX_CBR;
            break;
    }

    if (src.target_bitrate >= 0)
        tgt.rc_target_bitrate = src.target_bitrate;

    if (src.min_quantizer >= 0)
        tgt.rc_min_quantizer = src.min_quantizer;

    if (src.max_quantizer >= 0)
        tgt.rc_max_quantizer = src.max_quantizer;

    if (src.undershoot_pct >= 0)
        tgt.rc_undershoot_pct = src.undershoot_pct;

    if (src.overshoot_pct >= 0)
        tgt.rc_overshoot_pct = src.overshoot_pct;

    if (src.decoder_buffer_size >= 0)
        tgt.rc_buf_sz = src.decoder_buffer_size;

    if (src.decoder_buffer_initial_size >= 0)
        tgt.rc_buf_initial_sz = src.decoder_buffer_initial_size;

    if (src.decoder_buffer_optimal_size >= 0)
        tgt.rc_buf_optimal_sz = src.decoder_buffer_optimal_size;

    switch (src.keyframe_mode)
    {
        default:
            break;

        case kKeyframeModeDisabled:
            tgt.kf_mode = VPX_KF_DISABLED;
            break;

        case kKeyframeModeAuto:
            tgt.kf_mode = VPX_KF_AUTO;
            break;
    }

    if (src.keyframe_min_interval >= 0)
        tgt.kf_min_dist = src.keyframe_min_interval;

    if (src.keyframe_max_interval >= 0)
        tgt.kf_max_dist = src.keyframe_max_interval;
}


vpx_codec_err_t Inpin::SetTokenPartitions()
{
    const Filter::Config& src = m_pFilter->m_cfg;

    if (src.token_partitions < 0)
        return VPX_CODEC_OK;

    const vp8e_token_partitions token_partitions =
        static_cast<vp8e_token_partitions>(src.token_partitions);

    return vpx_codec_control(
        &m_ctx, VP8E_SET_TOKEN_PARTITIONS, token_partitions);
}



BYTE* Inpin::ConvertYUY2ToYV12(
    const BYTE* srcbuf,
    ULONG w,
    ULONG h)
{
    assert(srcbuf);
    assert((w % 2) == 0);  //TODO
    assert((h % 2) == 0);  //TODO

    const ULONG len = w*h + 2*((w+1)/2)*((h+1)/2);

    if (m_buflen < len)
    {
        delete[] m_buf;

        m_buf = 0;
        m_buflen = 0;

        m_buf = new (std::nothrow) BYTE[len];
        assert(m_buf);  //TODO

        m_buflen = len;
    }

    BYTE* dst_y = m_buf;
    BYTE* dst_v = dst_y + w*h;
    BYTE* dst_u = dst_v + ((w/2)*(h/2));

    const ULONG src_stride = 2*w;

    const ULONG dst_y_stride = w;
    const ULONG dst_uv_stride = w/2;

    const BYTE* src = srcbuf;

    for (ULONG i = 0; i < h; ++i)
    {
        const BYTE* src0_y = src;
        src = src0_y + src_stride;

        BYTE* dst0_y = dst_y;
        dst_y = dst0_y + dst_y_stride;

        for (ULONG j = 0; j < dst_y_stride; ++j)
        {
            *dst0_y = *src0_y;

            src0_y += 2;
            ++dst0_y;
        }
    }

    src = srcbuf;

    for (ULONG i = 0; i < h; i += 2)
    {
        const BYTE* src0_u = src + 1;
        const BYTE* src1_u = src0_u + src_stride;

        const BYTE* src0_v = src + 3;
        const BYTE* src1_v = src0_v + src_stride;

        src += 2 * src_stride;

        BYTE* dst0_u = dst_u;
        dst_u += dst_uv_stride;


        BYTE* dst0_v = dst_v;
        dst_v += dst_uv_stride;

        for (ULONG j = 0; j < dst_uv_stride; ++j)
        {
            const UINT u0 = *src0_u;
            const UINT u1 = *src1_u;
            const UINT src_u = (u0 + u1) / 2;  //?

            const UINT v0 = *src0_v;
            const UINT v1 = *src1_v;
            const UINT src_v = (v0 + v1) / 2;  //?

            *dst0_u = static_cast<BYTE>(src_u);
            *dst0_v = static_cast<BYTE>(src_v);

            src0_u += 4;
            src1_u += 4;

            src0_v += 4;
            src1_v += 4;

            ++dst0_u;
            ++dst0_v;
        }
    }

    return m_buf;
}


}  //end namespace VP8EncoderLib
