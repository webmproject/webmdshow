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
#include "vp8encoderfilter.hpp"
#include "vp8encoderoutpinvideo.hpp"
#include "cvp8sample.hpp"
#include "mediatypeutil.hpp"
#include "webmtypes.hpp"
#include <vfwmsgs.h>
#include <amvideo.h>
#include <dvdmedia.h>
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
    Outpin(pFilter, L"output")
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


HRESULT OutpinVideo::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    if (pmt == 0)
        return E_INVALIDARG;
        
    const AM_MEDIA_TYPE& mt = *pmt;
    
    if (mt.majortype != MEDIATYPE_Video)
        return S_FALSE;
    
    if (mt.subtype != WebmTypes::MEDIASUBTYPE_VP80)
        return S_FALSE;
        
    if (mt.pbFormat == 0)
        return S_FALSE;

    const BITMAPINFOHEADER* pbmih;
    
    if (mt.formattype == FORMAT_VideoInfo)
    {
        if (mt.cbFormat < sizeof(VIDEOINFOHEADER))
            return S_FALSE;
        
        const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
        const BITMAPINFOHEADER& bmih = vih.bmiHeader;
        
        pbmih = &bmih;
    }
    else if (mt.formattype == FORMAT_VideoInfo2)
    {
        if (mt.cbFormat < sizeof(VIDEOINFOHEADER2))
            return S_FALSE;
        
        const VIDEOINFOHEADER2& vih = (VIDEOINFOHEADER2&)(*mt.pbFormat);
        const BITMAPINFOHEADER& bmih = vih.bmiHeader;
        
        pbmih = &bmih;
    }
    else
        return S_FALSE;
        
    assert(pbmih);
    const BITMAPINFOHEADER& bmih = *pbmih;
    
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


HRESULT OutpinVideo::GetAllocator(IMemInputPin*, IMemAllocator** pp) const
{
    return CVP8Sample::CreateAllocator(pp);
}


void OutpinVideo::GetSubtype(GUID& subtype) const
{
    subtype = WebmTypes::MEDIASUBTYPE_VP80;
}


}  //end namespace VP8EncoderLib
