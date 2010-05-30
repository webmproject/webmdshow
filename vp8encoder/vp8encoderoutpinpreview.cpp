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
#include "vp8encoderoutpinpreview.hpp"
#include "cmediasample.hpp"
#include "mediatypeutil.hpp"
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


void OutpinPreview::OnInpinConnect(const AM_MEDIA_TYPE& mtIn)
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
    mt.subtype = MEDIASUBTYPE_YV12;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;  //TODO
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


HRESULT OutpinPreview::GetAllocator(IMemInputPin* pInputPin)
{
    GraphUtil::IMemAllocatorPtr pAllocator;
    
    HRESULT hr = pInputPin->GetAllocator(&pAllocator);
    
    if (FAILED(hr))
    {        
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


void OutpinPreview::Render(
    CLockable::Lock& lock,
    const BYTE* img,  //YV12
    ULONG wIn,
    ULONG hIn)  
{
    assert(wIn > 0);
    assert((wIn % 2) == 0);  //TODO
    assert(hIn > 0);
    assert((hIn % 2) == 0);  //TODO
    
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

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    assert(mt.formattype == FORMAT_VideoInfo);
    assert(mt.subtype == MEDIASUBTYPE_YV12);
    assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER));
    assert(mt.pbFormat);
    
    const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
    const BITMAPINFOHEADER& bmih = vih.bmiHeader;    
    
    //Y
    
    const BYTE* pInY = img;
    assert(pInY);
    
    BYTE* pOutBuf;
    
    hr = pOutSample->GetPointer(&pOutBuf);
    assert(SUCCEEDED(hr));
    assert(pOutBuf);
    
    BYTE* pOut = pOutBuf;
    
    const int strideInY = wIn;
    
    LONG strideOut = bmih.biWidth;
    assert(strideOut);
    assert((strideOut % 2) == 0);
    
    for (ULONG y = 0; y < hIn; ++y)
    {
        memcpy(pOut, pInY, wIn);
        pInY += strideInY;
        pOut += strideOut;
    }
    
    strideOut /= 2;
    
    wIn = (wIn + 1) / 2;    
    hIn = (hIn + 1) / 2;
    
    const BYTE* pInV = pInY;
    assert(pInV);
    
    const int strideInV = wIn;
    
    //V
    
    for (ULONG y = 0; y < hIn; ++y)
    {
        memcpy(pOut, pInV, wIn);
        pInV += strideInV;
        pOut += strideOut;
    }
    
    const BYTE* pInU = pInV;
    assert(pInU);
    
    const int strideInU = wIn;
    
    //U
    
    for (ULONG y = 0; y < hIn; ++y)
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


}  //end namespace VP8EncoderLib
