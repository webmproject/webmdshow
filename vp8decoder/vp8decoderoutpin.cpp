// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
//#include <comdef.h>
#include "vp8decoderfilter.hpp"
#include "vp8decoderoutpin.hpp"
//#include "cmemallocator.hpp"
#include "cmediasample.hpp"
#include "mediatypeutil.hpp"
#include "webmtypes.hpp"
#include <vfwmsgs.h>
#include <amvideo.h>  //VIDEOINFOHEADER
#include <uuids.h>
#include <cassert>
#ifdef _DEBUG
#include "odbgstream.hpp"
#include "iidstr.hpp"
using std::endl;
using std::dec;
using std::hex;
#endif

using std::wstring;

namespace VP8DecoderLib
{


Outpin::Outpin(Filter* pFilter) :
    Pin(pFilter, PINDIR_OUTPUT, L"output")
{
    SetDefaultMediaTypes();
}


Outpin::~Outpin()
{
    assert(!bool(m_pAllocator));
    assert(!bool(m_pInputPin));
}


HRESULT Outpin::Start()  //transition from stopped
{
    if (m_pPinConnection == 0)
        return S_FALSE;  //nothing we need to do
        
    assert(bool(m_pAllocator));
    assert(bool(m_pInputPin));
    
    const HRESULT hr = m_pAllocator->Commit();
    hr;
    assert(SUCCEEDED(hr));  //TODO
    
    return S_OK;
}


void Outpin::Stop()  //transition to stopped
{
    if (m_pPinConnection == 0)
        return;  //nothing was done
        
    assert(bool(m_pAllocator));
    assert(bool(m_pInputPin));
    
    const HRESULT hr = m_pAllocator->Decommit();
    hr;
    assert(SUCCEEDED(hr));
}


HRESULT Outpin::QueryInterface(const IID& iid, void** ppv)
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
#if 0
        wodbgstream os;
        os << "vp8dec::outpin::QI: iid=" << IIDStr(iid) << std::endl;
#endif        
        pUnk = 0;
        return E_NOINTERFACE;
    }
    
    pUnk->AddRef();
    return S_OK;
}


ULONG Outpin::AddRef()
{
    return m_pFilter->AddRef();
}


ULONG Outpin::Release()
{
    return m_pFilter->Release();
}


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
    
    hr = pInputPin->GetAllocator(&pAllocator);
    
    if (FAILED(hr))
    {        
        //hr = CMemAllocator::CreateInstance(&m_sample_factory, &pAllocator);
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

    //if (w % 16)
    //    w = 16 * ((w + 15) / 16);
    //    
    //if (h % 16)
    //    h = 16 * ((h + 15) / 16);
        
    const long cbBuffer = w*h + 2*((w+1)/2 * (h+1)/2);

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


HRESULT Outpin::OnDisconnect()
{
    m_pInputPin = 0;
    m_pAllocator = 0;
    
    return S_OK;
}


HRESULT Outpin::ReceiveConnection( 
    IPin*,
    const AM_MEDIA_TYPE*)
{
    return E_UNEXPECTED;  //for input pins only
}


HRESULT Outpin::QueryAccept(const AM_MEDIA_TYPE* pmt)
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
    else
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
        
    //if (bmih.biHeight <= 0)
    //    return S_FALSE;
        
    if (bmih.biCompression != mt.subtype.Data1)
        return S_FALSE;
        
    return S_OK;
}


HRESULT Outpin::QueryInternalConnections(IPin** pa, ULONG* pn)
{
    if (pn == 0)
        return E_POINTER;
        
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    ULONG& n = *pn;
    
    if (n == 0)
    {
        if (pa == 0)  //query for required number
        {
            n = 1;
            return S_OK;
        }
        
        return S_FALSE;  //means "insufficient number of array elements"
    }
    
    if (n < 1)
    {
        n = 0;
        return S_FALSE;  //means "insufficient number of array elements"
    }
        
    if (pa == 0)
    {
        n = 0;
        return E_POINTER;
    }
    
    IPin*& pPin = pa[0];

    pPin = &m_pFilter->m_inpin;
    pPin->AddRef();
        
    n = 1;    
    return S_OK;        
}

        
HRESULT Outpin::EndOfStream()
{
    return E_UNEXPECTED;  //for inpins only
}


HRESULT Outpin::NewSegment(
    REFERENCE_TIME,
    REFERENCE_TIME,
    double)
{
    return E_UNEXPECTED;
}


HRESULT Outpin::BeginFlush()
{
    return E_UNEXPECTED;
}


HRESULT Outpin::EndFlush()
{
    return E_UNEXPECTED;
}


HRESULT Outpin::GetCapabilities(DWORD* pdw)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->GetCapabilities(pdw);
            
    if (pdw == 0)
        return E_POINTER;
        
    DWORD& dw = *pdw;
    dw = 0;
    
    return S_OK;  //?
}


HRESULT Outpin::CheckCapabilities(DWORD* pdw)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->CheckCapabilities(pdw);

    if (pdw == 0)
        return E_POINTER;
        
    DWORD& dw = *pdw;

    const DWORD dwRequested = dw;
    
    if (dwRequested == 0)
        return E_INVALIDARG;
    
    return E_FAIL;
}


HRESULT Outpin::IsFormatSupported(const GUID* p)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->IsFormatSupported(p);

    if (p == 0)
        return E_POINTER;
        
    const GUID& g = *p;
    
    if (g == TIME_FORMAT_MEDIA_TIME)
        return S_OK;
        
    return S_FALSE;
}


HRESULT Outpin::QueryPreferredFormat(GUID* p)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->QueryPreferredFormat(p);

    if (p == 0)
        return E_POINTER;
        
    *p = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
}


HRESULT Outpin::GetTimeFormat(GUID* p)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->GetTimeFormat(p);

    if (p == 0)
        return E_POINTER;
        
    *p = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
}


HRESULT Outpin::IsUsingTimeFormat(const GUID* p)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->IsUsingTimeFormat(p);

    if (p == 0)
        return E_INVALIDARG;
        
    return (*p == TIME_FORMAT_MEDIA_TIME) ? S_OK : S_FALSE;
}


HRESULT Outpin::SetTimeFormat(const GUID* p)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->SetTimeFormat(p);
        
    if (p == 0)
        return E_INVALIDARG;
        
    if (*p == TIME_FORMAT_MEDIA_TIME)
        return S_OK;
        
    return E_INVALIDARG;
}
    

HRESULT Outpin::GetDuration(LONGLONG* p)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->GetDuration(p);

    if (p == 0)
        return E_POINTER;
        
    return E_FAIL;
}


HRESULT Outpin::GetStopPosition(LONGLONG* p)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->GetStopPosition(p);

    if (p == 0)
        return E_POINTER;
        
    return E_FAIL;
}


HRESULT Outpin::GetCurrentPosition(LONGLONG* p)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->GetCurrentPosition(p);

    if (p == 0)
        return E_POINTER;
        
    return E_FAIL;
}


HRESULT Outpin::ConvertTimeFormat( 
    LONGLONG* ptgt,
    const GUID* ptgtfmt,
    LONGLONG src,
    const GUID* psrcfmt)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->ConvertTimeFormat(ptgt, ptgtfmt, src, psrcfmt);

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


HRESULT Outpin::SetPositions( 
    LONGLONG* pCurr,
    DWORD dwCurr,
    LONGLONG* pStop,
    DWORD dwStop)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->SetPositions(pCurr, dwCurr, pStop, dwStop);
        
    return E_FAIL;
}


HRESULT Outpin::GetPositions( 
    LONGLONG* pCurrPos,
    LONGLONG* pStopPos)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->GetPositions(pCurrPos, pStopPos);
        
    return E_FAIL;
}


HRESULT Outpin::GetAvailable( 
    LONGLONG* pEarliest,
    LONGLONG* pLatest)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->GetAvailable(pEarliest, pLatest);
        
    return E_FAIL;
}


HRESULT Outpin::SetRate(double r)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->SetRate(r);

    return E_FAIL;
}


HRESULT Outpin::GetRate(double* p)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->GetRate(p);

    return E_FAIL;
}


HRESULT Outpin::GetPreroll(LONGLONG* p)
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);
        
    if (bool(pSeek))
        return pSeek->GetPreroll(p);

    return E_FAIL;
}


HRESULT Outpin::GetName(PIN_INFO& info) const
{
    wstring name;
    
    if (!bool(m_pPinConnection))
        name = L"YUV";
    else
    {
        const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
        const char* p = (const char*)&mt.subtype.Data1;
        const char* const q = p + 4;
        
        while (p != q)
        {
            const char c = *p++;
            name += wchar_t(c);  //?
        }
    }
    
    const wchar_t* const name_ = name.c_str();
    
#if _MSC_VER >= 1400
    enum { namelen = sizeof(info.achName) / sizeof(WCHAR) };
    const errno_t e = wcscpy_s(info.achName, namelen, name_);
    e;
    assert(e == 0);
#else
    wcscpy(info.achName, name_);
#endif

    return S_OK;
}


void Outpin::OnInpinConnect(const AM_MEDIA_TYPE& mtIn)
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
    mt.bFixedSizeSamples = TRUE;
    mt.bTemporalCompression = FALSE;
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
    
    const LONG w = bmihIn.biWidth;
    assert(w > 0);
    
    const LONG h = bmihIn.biHeight;
    assert(h > 0);
    
    bmih.biSize = sizeof bmih;
    bmih.biWidth = w;
    bmih.biHeight = h;
    bmih.biPlanes = 1;  //because Microsoft says so
    bmih.biBitCount = 12;  //?
    bmih.biCompression = mt.subtype.Data1;
    bmih.biSizeImage = w*h + 2*((w+1)/2 * (h+1)/2);
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;
    
    m_preferred_mtv.Add(mt);

    mt.subtype = WebmTypes::MEDIASUBTYPE_I420;
    bmih.biCompression = mt.subtype.Data1;  //"I420"

    m_preferred_mtv.Add(mt);
}


HRESULT Outpin::OnInpinDisconnect()
{
    if (bool(m_pPinConnection))
    {
        IFilterGraph* const pGraph = m_pFilter->m_info.pGraph;
        assert(pGraph);
        
        HRESULT hr = pGraph->Disconnect(m_pPinConnection);
        assert(SUCCEEDED(hr));
        
        hr = pGraph->Disconnect(this);
        assert(SUCCEEDED(hr));

        assert(!bool(m_pPinConnection));
    }
    
    SetDefaultMediaTypes();

    return S_OK;
}


void Outpin::SetDefaultMediaTypes()
{
    m_preferred_mtv.Clear();
    
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
}


}  //end namespace VP8DecoderLib
