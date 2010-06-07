// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma warning(disable:4505)  //unreferenced local function has been removed
#include "vp8decoderfilter.hpp"
#include "vp8decoderoutpin.hpp"
#include "mediatypeutil.hpp"
#include "vp8dx.h"
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

namespace VP8DecoderLib
{
    
Inpin::Inpin(Filter* p) :
    Pin(p, PINDIR_INPUT, L"input"),
    m_bEndOfStream(false),
    m_bFlush(false)
{
    AM_MEDIA_TYPE mt;
    
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = WebmTypes::MEDIASUBTYPE_VP80;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = TRUE;
    mt.lSampleSize = 0;
    mt.formattype = GUID_NULL;
    mt.pUnk = 0;
    mt.cbFormat = 0;
    mt.pbFormat = 0;
    
    m_preferred_mtv.Add(mt);
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
        os << "vp8decoder::inpin::EOS: calling pin->EOS" << endl;
#endif

        const HRESULT hr = pPin->EndOfStream();
        
#ifdef _DEBUG
        os << "vp8decoder::inpin::EOS: called pin->EOS; hr=0x"
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
    
#ifdef _DEBUG
    odbgstream os;
    os << "vp8decoder::inpin::beginflush" << endl;
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

#ifdef _DEBUG
    odbgstream os;
    os << "vp8decoder::inpin::endflush" << endl;
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
    
    if (mt.majortype != MEDIATYPE_Video)
        return S_FALSE;
        
    if (mt.subtype != WebmTypes::MEDIASUBTYPE_VP80)
        return S_FALSE;
        
    if (mt.formattype != FORMAT_VideoInfo)  //TODO: liberalize
        return S_FALSE;
        
    if (mt.pbFormat == 0)
        return S_FALSE;

    if (mt.cbFormat < sizeof(VIDEOINFOHEADER))
        return S_FALSE;
        
    const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
    const BITMAPINFOHEADER& bmih = vih.bmiHeader;
    
    if (bmih.biSize != sizeof(BITMAPINFOHEADER))  //TODO: liberalize
        return S_FALSE;
        
    if (bmih.biWidth <= 0)
        return S_FALSE;
        
    if (bmih.biHeight <= 0)
        return S_FALSE;
        
    if (bmih.biCompression != WebmTypes::MEDIASUBTYPE_VP80.Data1)  //"VP80"
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
    os << "vp8dec::inpin::NotifyAllocator: props.cBuffers="
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

//#define DEBUG_RECEIVE

#ifdef DEBUG_RECEIVE
    {
        __int64 start_reftime, stop_reftime;
        const HRESULT hr = pInSample->GetTime(&start_reftime, &stop_reftime);

        odbgstream os;
        os << "vp8dec::inpin::receive: ";
        
        os << std::fixed << std::setprecision(3);
        
        if (hr == S_OK)
            os << "start[ms]=" << double(start_reftime) / 10000
               << "; stop[ms]=" << double(stop_reftime) / 10000
               << "; dt[ms]=" << double(stop_reftime - start_reftime) / 10000;

        else if (hr == VFW_S_NO_STOP_TIME)
            os << "start[ms]=" << double(start_reftime) / 10000;

        os << endl;
    }
#endif

    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
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
        
    BYTE* buf;
    
    hr = pInSample->GetPointer(&buf);
    assert(SUCCEEDED(hr));
    assert(buf);
    
    const long len = pInSample->GetActualDataLength();
    assert(len >= 0);
    
    const vpx_codec_err_t err = vpx_codec_decode(&m_ctx, buf, len, 0, 0);

    if (err != VPX_CODEC_OK)
    {
        GraphUtil::IMediaEventSinkPtr pSink(m_pFilter->m_info.pGraph);
        
        if (bool(pSink))
        {
            lock.Release();
            
            hr = pSink->Notify(EC_STREAM_ERROR_STOPPED, VFW_E_SAMPLE_REJECTED, err);
        }
                
        m_bEndOfStream = true;  //clear this out when we stop and then start again
        return S_FALSE;
    }
        
    if (pInSample->IsPreroll() == S_OK)
        return S_OK;
        
    lock.Release();
    
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

    vpx_codec_iter_t iter = 0;
    
    vpx_image_t* const f = vpx_codec_get_frame(&m_ctx, &iter);
    
    if (f == 0)
        return S_OK;
            
    AM_MEDIA_TYPE* pmt;
    
    hr = pOutSample->GetMediaType(&pmt);
    
    if (SUCCEEDED(hr) && (pmt != 0))
    {
        assert(outpin.QueryAccept(pmt) == S_OK);
        outpin.m_connection_mtv.Clear();
        outpin.m_connection_mtv.Add(*pmt);
        
        MediaTypeUtil::Free(pmt);
        pmt = 0;
    }
    
    const AM_MEDIA_TYPE& mt = outpin.m_connection_mtv[0];
    assert(mt.formattype == FORMAT_VideoInfo);
    assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER));
    assert(mt.pbFormat);
    
    const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
    const BITMAPINFOHEADER& bmih = vih.bmiHeader;    
    
    //Y
    
    const BYTE* pInY = f->planes[PLANE_Y];
    assert(pInY);
    
    unsigned int wIn = f->d_w;
    unsigned int hIn = f->d_h;
    
    BYTE* pOutBuf;
    
    hr = pOutSample->GetPointer(&pOutBuf);
    assert(SUCCEEDED(hr));
    assert(pOutBuf);
    
    BYTE* pOut = pOutBuf;
    
    const int strideInY = f->stride[PLANE_Y];
    
    LONG strideOut = bmih.biWidth;
    assert(strideOut);
    assert((strideOut % 2) == 0);  //?
    
    for (unsigned int y = 0; y < hIn; ++y)
    {
        memcpy(pOut, pInY, wIn);
        pInY += strideInY;
        pOut += strideOut;
    }
    
    strideOut /= 2;
    
    wIn = (wIn + 1) / 2;    
    hIn = (hIn + 1) / 2;
    
    const BYTE* pInV = f->planes[PLANE_V];
    assert(pInV);
    
    const int strideInV = f->stride[PLANE_V];
    
    const BYTE* pInU = f->planes[PLANE_U];
    assert(pInU);
    
    const int strideInU = f->stride[PLANE_U];
    
    if (mt.subtype == MEDIASUBTYPE_YV12)
    {
        //V
        
        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInV, wIn);
            pInV += strideInV;
            pOut += strideOut;
        }
        
        //U
        
        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInU, wIn);
            pInU += strideInU;
            pOut += strideOut;
        }
    }
    else
    {
        //U 
        
        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInU, wIn);
            pInU += strideInU;
            pOut += strideOut;
        }
        
        //V

        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInV, wIn);
            pInV += strideInV;
            pOut += strideOut;
        }        
    }
    
    __int64 st, sp;
    
    hr = pInSample->GetTime(&st, &sp);
    
    if (FAILED(hr))
    {
        hr = pOutSample->SetTime(0, 0);
        assert(SUCCEEDED(hr));
    }        
    else if (hr == S_OK)
    {
        hr = pOutSample->SetTime(&st, &sp);
        assert(SUCCEEDED(hr));
    }        
    else
    {
        hr = pOutSample->SetTime(&st, 0);
        assert(SUCCEEDED(hr));
    }
        
    hr = pOutSample->SetSyncPoint(TRUE);
    assert(SUCCEEDED(hr));
    
    hr = pOutSample->SetPreroll(FALSE);
    assert(SUCCEEDED(hr));
    
    const ptrdiff_t lenOut_ = pOut - pOutBuf;
    const long lenOut = static_cast<long>(lenOut_);
    
    hr = pOutSample->SetActualDataLength(lenOut);
    assert(SUCCEEDED(hr));
    
    hr = pInSample->IsDiscontinuity();
    hr = pOutSample->SetDiscontinuity(hr == S_OK);
    
    hr = pOutSample->SetMediaTime(0, 0);
                
    lock.Release();
    
    return outpin.m_pInputPin->Receive(pOutSample);
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
    const wchar_t name[] = L"VP80";
    
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

    vpx_codec_iface_t& vp8 = vpx_codec_vp8_dx_algo;
    
    const int flags = VPX_CODEC_USE_POSTPROC;
    
    const vpx_codec_err_t err = vpx_codec_dec_init(
                                    &m_ctx, 
                                    &vp8, 
                                    0, 
                                    flags);
    
    if (err == VPX_CODEC_MEM_ERROR)
        return E_OUTOFMEMORY;
        
    if (err != VPX_CODEC_OK)
        return E_FAIL;

    const HRESULT hr = OnApplyPostProcessing();

    if (FAILED(hr))
    {
        Stop();
        return hr;
    }
 
    return S_OK;
}


void Inpin::Stop()
{
    const vpx_codec_err_t err = vpx_codec_destroy(&m_ctx);
    err;
    assert(err == VPX_CODEC_OK);
}


HRESULT Inpin::OnApplyPostProcessing()
{
    const Filter::Config& src = m_pFilter->m_cfg;
    vp8_postproc_cfg_t tgt;
    
    tgt.post_proc_flag = src.flags;
    tgt.deblocking_level = src.deblock;
    tgt.noise_level = src.noise;

    const vpx_codec_err_t err = vpx_codec_control(&m_ctx, VP8_SET_POSTPROC, &tgt);

    return (err == VPX_CODEC_OK) ? S_OK : E_FAIL;
}


}  //end namespace VP8DecoderLib
