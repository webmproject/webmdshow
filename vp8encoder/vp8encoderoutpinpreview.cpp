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
#include "vp8encoderoutpinpreview.hpp"
#include "cmediasample.hpp"
#include "mediatypeutil.hpp"
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

OutpinPreview::OutpinPreview(Filter* pFilter) :
    Outpin(pFilter, L"preview")
{
    SetDefaultMediaTypes();
}


OutpinPreview::~OutpinPreview()
{
}


HRESULT OutpinPreview::QueryInterface(const IID& iid, void** ppv)
{
    if (ppv == 0)
        return E_POINTER;
        
    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);
    
    if (iid == __uuidof(IUnknown))
        pUnk = static_cast<IPin*>(this);
        
    else if (iid == __uuidof(IPin))
        pUnk = static_cast<IPin*>(this);
        
    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }
    
    pUnk->AddRef();
    return S_OK;
}



ULONG OutpinPreview::AddRef()
{
    return m_pFilter->AddRef();
}


ULONG OutpinPreview::Release()
{
    return m_pFilter->Release();
}


HRESULT OutpinPreview::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    if (pmt == 0)
        return E_INVALIDARG;
        
    const AM_MEDIA_TYPE& mt = *pmt;
    
    if (mt.majortype != MEDIATYPE_Video)
        return S_FALSE;
        
    if (mt.subtype != MEDIASUBTYPE_YV12)
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
        
    //if (bmih.biHeight <= 0)
    //    return S_FALSE;
        
    if (bmih.biCompression != mt.subtype.Data1)
        return S_FALSE;
        
    return S_OK;
}


std::wstring OutpinPreview::GetName() const
{
    return L"Preview";
}


void OutpinPreview::SetDefaultMediaTypes()
{
    m_preferred_mtv.Clear();
    
    AM_MEDIA_TYPE mt;
    
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_YV12;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;
    mt.formattype = GUID_NULL;
    mt.pUnk = 0;
    mt.cbFormat = 0;
    mt.pbFormat = 0;
    
    m_preferred_mtv.Add(mt);
}


HRESULT OutpinPreview::GetAllocator(IMemInputPin* pInputPin, IMemAllocator** pp) const
{
    HRESULT hr = pInputPin->GetAllocator(pp);
    
    if (FAILED(hr))
        hr = CMediaSample::CreateAllocator(pp);
        
    return hr;
}


void OutpinPreview::Render(
    CLockable::Lock& lock,
    const vpx_image_t* img)
{
    assert(img);
    
    if (!bool(m_pPinConnection))
        return;
        
    if (!bool(m_pInputPin))
        return;
        
    if (!bool(m_pAllocator))
        return;
        
    HRESULT hr = lock.Release();
    assert(SUCCEEDED(hr));
    
    GraphUtil::IMediaSamplePtr pOutSample;
    
    const HRESULT hrGetBuffer = m_pAllocator->GetBuffer(
                                    &pOutSample, 
                                    0, 
                                    0, 
                                    AM_GBF_NOWAIT);
    
    hr = lock.Seize(m_pFilter);
    assert(SUCCEEDED(hr));  //TODO

    if (FAILED(hrGetBuffer))
        return;

    assert(bool(pOutSample));
    
    AM_MEDIA_TYPE* pmt;
    
    hr = pOutSample->GetMediaType(&pmt);
    
    if (SUCCEEDED(hr) && (pmt != 0))
    {
        assert(QueryAccept(pmt) == S_OK);
        m_connection_mtv.Clear();
        m_connection_mtv.Add(*pmt);
        
        MediaTypeUtil::Free(pmt);
        pmt = 0;
    }
    
    unsigned int wIn = img->d_w;
    unsigned int hIn = img->d_h;

    const BITMAPINFOHEADER& bmih = GetBMIH();
    
    LONG strideOut = bmih.biWidth;
    assert(strideOut);
    assert((strideOut % 2) == 0);
    
    //Y
    
    const BYTE* pInY = img->planes[PLANE_Y];
    assert(pInY);
    
    BYTE* pOutBuf;
    
    hr = pOutSample->GetPointer(&pOutBuf);
    assert(SUCCEEDED(hr));
    assert(pOutBuf);
    
    BYTE* pOut = pOutBuf;
    
    const int strideInY = img->stride[PLANE_Y];
    
    for (unsigned int y = 0; y < hIn; ++y)
    {
        memcpy(pOut, pInY, wIn);
        pInY += strideInY;
        pOut += strideOut;
    }
    
    strideOut /= 2;
    
    wIn = (wIn + 1) / 2;    
    hIn = (hIn + 1) / 2;
    
    const BYTE* pInV = img->planes[PLANE_V];
    assert(pInV);
    
    const int strideInV = img->stride[PLANE_V];
    
    const BYTE* pInU = img->planes[PLANE_U];
    assert(pInU);
    
    const int strideInU = img->stride[PLANE_U];
    
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

    hr = pOutSample->SetTime(0, 0);
    assert(SUCCEEDED(hr));

    hr = pOutSample->SetSyncPoint(TRUE);
    assert(SUCCEEDED(hr));
    
    hr = pOutSample->SetPreroll(FALSE);
    assert(SUCCEEDED(hr));
    
    const ptrdiff_t lenOut_ = pOut - pOutBuf;
    const long lenOut = static_cast<long>(lenOut_);
    
    hr = pOutSample->SetActualDataLength(lenOut);
    assert(SUCCEEDED(hr));
    
    hr = pOutSample->SetDiscontinuity(FALSE);  //?
    
    hr = pOutSample->SetMediaTime(0, 0);
    
    hr = lock.Release();
    assert(SUCCEEDED(hr));
    
    hr = m_pInputPin->Receive(pOutSample);
    
    hr = lock.Seize(m_pFilter);
    assert(SUCCEEDED(hr));  //TODO
}


void OutpinPreview::GetSubtype(GUID& subtype) const
{
    subtype = MEDIASUBTYPE_YV12;
}

}  //end namespace VP8EncoderLib
