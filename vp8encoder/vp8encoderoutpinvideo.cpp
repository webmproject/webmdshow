// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license and patent
// grant that can be found in the LICENSE file in the root of the source
// tree. All contributing project authors may be found in the AUTHORS
// file in the root of the source tree.

#include <strmif.h>
#include <comdef.h>
#include <uuids.h>
#include "vp8encoderfilter.hpp"
#include "vp8encoderoutpinvideo.hpp"
#include "cvp8sample.hpp"
#include "mediatypeutil.hpp"
#include "webmtypes.hpp"
#include <vfwmsgs.h>
#include <amvideo.h>
#include <cassert>
#ifdef _DEBUG
#include "odbgstream.hpp"
#include "iidstr.hpp"
using std::endl;
using std::dec;
using std::hex;
#endif

using std::wstring;

namespace VP8EncoderLib
{

OutpinVideo::OutpinVideo(Filter* pFilter) :
    Outpin(pFilter, L"video")
{
    SetDefaultMediaTypes();
}


OutpinVideo::~OutpinVideo()
{
}


HRESULT OutpinVideo::QueryInterface(const IID& iid, void** ppv)
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
        pUnk = 0;
        return E_NOINTERFACE;
    }
    
    pUnk->AddRef();
    return S_OK;
}



ULONG OutpinVideo::AddRef()
{
    return m_pFilter->AddRef();
}


ULONG OutpinVideo::Release()
{
    return m_pFilter->Release();
}


#if 0  //TODO
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
        
    if (m_pFilter->m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;
        
    if (bool(m_pPinConnection))
        return VFW_E_ALREADY_CONNECTED;
        
    if (!bool(m_pFilter->m_inpin.m_pPinConnection))
        return VFW_E_NO_TYPES;  //VFW_E_NOT_CONNECTED?
        
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
                break;
                        
            ++i;
        }
        
        if (i >= j)
            return VFW_E_NO_ACCEPTABLE_TYPES;
            
        const AM_MEDIA_TYPE& mt = m_preferred_mtv[i];

        m_connection_mtv.Add(mt);
    }

    GraphUtil::IMemAllocatorPtr pAllocator;
    
    hr = CVP8Sample::CreateAllocator(&pAllocator);
    
    if (FAILED(hr))
        return VFW_E_NO_ALLOCATOR;

    assert(bool(pAllocator));    

    ALLOCATOR_PROPERTIES props, actual;

    props.cBuffers = -1;    //number of buffers
    props.cbBuffer = -1;    //size of each buffer, excluding prefix
    props.cbAlign = -1;     //applies to prefix, too
    props.cbPrefix = -1;    //imediasample::getbuffer does NOT include prefix

    hr = pInputPin->GetAllocatorRequirements(&props);

    if (props.cBuffers <= 0)
        props.cBuffers = 1;
        
    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    assert(mt.formattype == FORMAT_VideoInfo);  //TODO
    assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER));
    assert(mt.pbFormat);

    const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
    const BITMAPINFOHEADER& bmih = vih.bmiHeader;
    
    LONG w = bmih.biWidth;
    assert(w > 0);
    
    LONG h = labs(bmih.biHeight);
    assert(h > 0);

    if (w % 16)
        w = 16 * ((w + 15) / 16);
        
    if (h % 16)
        h = 16 * ((h + 15) / 16);
        
    const long cbBuffer = w*h + 2*(w/2)*(h/2);

    if (props.cbBuffer < cbBuffer)
        props.cbBuffer = cbBuffer;
    
    if (props.cbAlign <= 0)
        props.cbAlign = 1;
        
    if (props.cbPrefix < 0)
        props.cbPrefix = 0;
    
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
#endif  //TODO



HRESULT OutpinVideo::QueryAccept(const AM_MEDIA_TYPE* pmt)
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
        
    //TODO: compare these to preferred media type
    //(or to width and height of inpin)
        
    if (bmih.biWidth <= 0)
        return S_FALSE;
        
    if (bmih.biHeight <= 0)
        return S_FALSE;
        
    if (bmih.biCompression != mt.subtype.Data1)
        return S_FALSE;
        
    return S_OK;
}


HRESULT OutpinVideo::GetCapabilities(DWORD* pdw)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;
        
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->GetCapabilities(pdw);
    }
            
    if (pdw == 0)
        return E_POINTER;
        
    DWORD& dw = *pdw;
    dw = 0;
    
    return S_OK;  //?
}


HRESULT OutpinVideo::CheckCapabilities(DWORD* pdw)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->CheckCapabilities(pdw);
    }

    if (pdw == 0)
        return E_POINTER;
        
    DWORD& dw = *pdw;

    const DWORD dwRequested = dw;
    
    if (dwRequested == 0)
        return E_INVALIDARG;
    
    return E_FAIL;
}


HRESULT OutpinVideo::IsFormatSupported(const GUID* p)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->IsFormatSupported(p);
    }

    if (p == 0)
        return E_POINTER;
        
    const GUID& g = *p;
    
    if (g == TIME_FORMAT_MEDIA_TIME)
        return S_OK;
        
    return S_FALSE;
}



HRESULT OutpinVideo::QueryPreferredFormat(GUID* p)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->QueryPreferredFormat(p);
    }

    if (p == 0)
        return E_POINTER;
        
    *p = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
}


HRESULT OutpinVideo::GetTimeFormat(GUID* p)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->GetTimeFormat(p);
    }

    if (p == 0)
        return E_POINTER;
        
    *p = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
}


HRESULT OutpinVideo::IsUsingTimeFormat(const GUID* p)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->IsUsingTimeFormat(p);
    }

    if (p == 0)
        return E_INVALIDARG;
        
    return (*p == TIME_FORMAT_MEDIA_TIME) ? S_OK : S_FALSE;
}


HRESULT OutpinVideo::SetTimeFormat(const GUID* p)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->SetTimeFormat(p);
    }
        
    if (p == 0)
        return E_INVALIDARG;
        
    if (*p == TIME_FORMAT_MEDIA_TIME)
        return S_OK;
        
    return E_INVALIDARG;
}
    

HRESULT OutpinVideo::GetDuration(LONGLONG* p)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->GetDuration(p);
    }

    if (p == 0)
        return E_POINTER;
        
    return E_FAIL;
}



HRESULT OutpinVideo::GetStopPosition(LONGLONG* p)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->GetStopPosition(p);
    }

    if (p == 0)
        return E_POINTER;
        
    return E_FAIL;
}


HRESULT OutpinVideo::GetCurrentPosition(LONGLONG* p)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->GetCurrentPosition(p);
    }

    if (p == 0)
        return E_POINTER;
        
    return E_FAIL;
}


HRESULT OutpinVideo::ConvertTimeFormat( 
    LONGLONG* ptgt,
    const GUID* ptgtfmt,
    LONGLONG src,
    const GUID* psrcfmt)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->ConvertTimeFormat(ptgt, ptgtfmt, src, psrcfmt);
    }

    if (ptgt == 0)
        return E_POINTER;
        
    LONGLONG& tgt = *ptgt;
        
    const GUID& tgtfmt = ptgtfmt ? *ptgtfmt : TIME_FORMAT_MEDIA_TIME;
    const GUID& srcfmt = psrcfmt ? *psrcfmt : TIME_FORMAT_MEDIA_TIME;
    
    if (tgtfmt != TIME_FORMAT_MEDIA_TIME)
        return E_INVALIDARG;
        
    if (srcfmt != TIME_FORMAT_MEDIA_TIME)
        return E_INVALIDARG;
        
    tgt = src;
    return S_OK;                
}


HRESULT OutpinVideo::SetPositions( 
    LONGLONG* pCurr_,
    DWORD dwCurr_,
    LONGLONG* pStop_,
    DWORD dwStop_)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        
        LONGLONG curr;
        LONGLONG* const pCurr = pCurr_ ? pCurr_ : &curr;
        
        const DWORD dwCurr = pCurr_ ? dwCurr_ : AM_SEEKING_NoPositioning;
        
        LONGLONG stop;
        LONGLONG* const pStop = pStop_ ? pStop_ : &stop;
        
        const DWORD dwStop = pStop_ ? dwStop_ : AM_SEEKING_NoPositioning;

        return pSeek->SetPositions(pCurr, dwCurr, pStop, dwStop);
    }
        
    return E_FAIL;
}


HRESULT OutpinVideo::GetPositions( 
    LONGLONG* pCurrPos,
    LONGLONG* pStopPos)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->GetPositions(pCurrPos, pStopPos);
    }
        
    return E_FAIL;
}


HRESULT OutpinVideo::GetAvailable( 
    LONGLONG* pEarliest,
    LONGLONG* pLatest)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->GetAvailable(pEarliest, pLatest);
    }
        
    return E_FAIL;
}


HRESULT OutpinVideo::SetRate(double r)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->SetRate(r);
    }

    return E_FAIL;
}


HRESULT OutpinVideo::GetRate(double* p)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->GetRate(p);
    }

    return E_FAIL;
}


HRESULT OutpinVideo::GetPreroll(LONGLONG* p)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
    {
        lock.Release();
        return pSeek->GetPreroll(p);
    }

    return E_FAIL;
}


std::wstring OutpinVideo::GetName() const
{
    return L"VP80";
}


void OutpinVideo::OnInpinConnect(const AM_MEDIA_TYPE& mtIn)
{
    assert(mtIn.cbFormat >= sizeof(VIDEOINFOHEADER));
    assert(mtIn.pbFormat);
    
    const VIDEOINFOHEADER& vihIn = (VIDEOINFOHEADER&)(*mtIn.pbFormat);
    const BITMAPINFOHEADER& bmihIn = vihIn.bmiHeader;
    
    m_preferred_mtv.Clear();
    
    AM_MEDIA_TYPE mt;
    
    VIDEOINFOHEADER vih;
    BITMAPINFOHEADER& bmih = vih.bmiHeader;
    
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = WebmTypes::MEDIASUBTYPE_VP80;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = TRUE;
    mt.lSampleSize = 0;
    mt.formattype = FORMAT_VideoInfo;
    mt.pUnk = 0;
    mt.cbFormat = sizeof vih;
    mt.pbFormat = (BYTE*)&vih;
    
    SetRectEmpty(&vih.rcSource);  //TODO
    SetRectEmpty(&vih.rcTarget);  //TODO
    vih.dwBitRate = 0;
    vih.dwBitErrorRate = 0;
    vih.AvgTimePerFrame = vihIn.AvgTimePerFrame;
    
    const LONG ww = bmihIn.biWidth;
    assert(ww > 0);
    
    const LONG hh = bmihIn.biHeight;
    assert(hh > 0);
    
    bmih.biSize = sizeof bmih;
    bmih.biWidth = ww;
    bmih.biHeight = hh;
    bmih.biPlanes = 1;  //because Microsoft says so
    bmih.biBitCount = 12;  //?
    bmih.biCompression = mt.subtype.Data1;

    //TODO: does this really need to be a conditional expr?
    const LONG w = (ww % 16) ? 16 * ((ww + 15) / 16) : ww;
    const LONG h = (hh % 16) ? 16 * ((hh + 15) / 16) : hh;        
    const LONG cbBuffer = w*h + 2*(w/2)*(h/2);

    bmih.biSizeImage = cbBuffer;
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;
    
    m_preferred_mtv.Add(mt);
}


void OutpinVideo::SetDefaultMediaTypes()
{
    m_preferred_mtv.Clear();
    
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


HRESULT OutpinVideo::GetFrame(IVP8Sample::Frame& f)
{
    IMemAllocator* const pAlloc_ = m_pAllocator;
    assert(pAlloc_);
    
    CMemAllocator* const pAlloc = static_cast<CMemAllocator*>(pAlloc_);
    
    return CVP8Sample::GetFrame(pAlloc, f);
}


HRESULT OutpinVideo::GetAllocator(IMemInputPin* pInputPin)
{
    GraphUtil::IMemAllocatorPtr pAllocator;
    
    HRESULT hr = CVP8Sample::CreateAllocator(&pAllocator);
    
    if (FAILED(hr))
        return VFW_E_NO_ALLOCATOR;

    assert(bool(pAllocator));    

    ALLOCATOR_PROPERTIES props, actual;

    props.cBuffers = -1;    //number of buffers
    props.cbBuffer = -1;    //size of each buffer, excluding prefix
    props.cbAlign = -1;     //applies to prefix, too
    props.cbPrefix = -1;    //imediasample::getbuffer does NOT include prefix

    hr = pInputPin->GetAllocatorRequirements(&props);

    if (props.cBuffers <= 0)
        props.cBuffers = 1;
        
    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    assert(mt.formattype == FORMAT_VideoInfo);  //TODO
    assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER));
    assert(mt.pbFormat);

    const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
    const BITMAPINFOHEADER& bmih = vih.bmiHeader;
    
    LONG w = bmih.biWidth;
    assert(w > 0);
    
    LONG h = labs(bmih.biHeight);
    assert(h > 0);

    if (w % 16)
        w = 16 * ((w + 15) / 16);
        
    if (h % 16)
        h = 16 * ((h + 15) / 16);
        
    const long cbBuffer = w*h + 2*(w/2)*(h/2);

    if (props.cbBuffer < cbBuffer)
        props.cbBuffer = cbBuffer;
    
    if (props.cbAlign <= 0)
        props.cbAlign = 1;
        
    if (props.cbPrefix < 0)
        props.cbPrefix = 0;
    
    hr = pAllocator->SetProperties(&props, &actual);
    
    if (FAILED(hr))
        return hr;
        
    hr = pInputPin->NotifyAllocator(pAllocator, 0);  //allow writes
    
    if (FAILED(hr) && (hr != E_NOTIMPL))
        return hr;
        
    m_pAllocator = pAllocator;
    return S_OK;  //success
}


}  //end namespace VP8EncoderLib
