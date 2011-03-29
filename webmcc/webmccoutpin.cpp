// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
//#include <comdef.h>
#include "webmccfilter.hpp"
#include "webmccoutpin.hpp"
#include "cmediasample.hpp"
#include "mediatypeutil.hpp"
#include "webmtypes.hpp"
#include <vfwmsgs.h>
#include <uuids.h>
#include <cassert>
#include <process.h>
//#include <amvideo.h>
#ifdef _DEBUG
#include "odbgstream.hpp"
#include <iomanip>
#include "iidstr.hpp"
using std::endl;
using std::dec;
using std::hex;
using std::fixed;
using std::setprecision;
#endif

using std::wstring;

namespace WebmColorConversion
{


Outpin::Outpin(Filter* pFilter) :
    Pin(pFilter, PINDIR_OUTPUT, L"output"),
    m_hThread(0),
    m_rgb_to_yuv(0)
{
    SetDefaultMediaTypes();
}


Outpin::~Outpin()
{
    assert(m_hThread == 0);
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

    StartThread();

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

    StopThread();
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

        LONG idx = -1;

        const ULONG n = m_preferred_mtv.Size();

        for (ULONG i = 0; i < n; ++i)
        {
            const AM_MEDIA_TYPE& mt = m_preferred_mtv[i];

            if (mt.subtype == mt.subtype)
            {
                idx = i;
                break;
            }
        }

        if (idx < 0)  //weird
            return VFW_E_TYPE_NOT_ACCEPTED;

        const AM_MEDIA_TYPE& mt = m_preferred_mtv[idx];

        hr = pin->ReceiveConnection(this, &mt);

        if (FAILED(hr))
            return hr;

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

    const long cBuffers = 2;

    if (props.cBuffers < cBuffers)
        props.cBuffers = cBuffers;

    const VIDEOINFOHEADER* const pvih = this->GetVideoInfo();
    assert(pvih);

    const BITMAPINFOHEADER& bmih = pvih->bmiHeader;

    const LONG w = bmih.biWidth;
    const LONG h = labs(bmih.biHeight);

    const LONG y = w * h;
    const LONG uv = (w/2) * (h/2);

    const LONG cbBuffer = y + 2 * uv;

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

    Filter::Lock lock;

    const HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    const Inpin& inpin = m_pFilter->m_inpin;

    if (!bool(inpin.m_pPinConnection))
        return VFW_E_NO_TYPES;

    const AM_MEDIA_TYPE& mtOut = *pmt;

    if (mtOut.majortype != MEDIATYPE_Video)
        return S_FALSE;

    if (mtOut.subtype != MEDIASUBTYPE_YV12)
        return S_FALSE;

    if (mtOut.formattype == GUID_NULL)
        return S_OK;

    if (mtOut.formattype != FORMAT_VideoInfo)
        return S_FALSE;

    if (mtOut.pbFormat == 0)
        return S_FALSE;

    if (mtOut.cbFormat < sizeof(VIDEOINFOHEADER))
        return S_FALSE;

    const VIDEOINFOHEADER& vihOut = (VIDEOINFOHEADER&)(*mtOut.pbFormat);
    const BITMAPINFOHEADER& bmihOut = vihOut.bmiHeader;
    bmihOut;

    //TODO: vet vih and bmih

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
    const wchar_t* const name_ = L"YUV";

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
    assert(mtIn.formattype == FORMAT_VideoInfo);  //TODO: liberalize
    assert(mtIn.pbFormat);
    assert(mtIn.cbFormat >= sizeof(VIDEOINFOHEADER));

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
    mt.lSampleSize = 0;  //TODO
    mt.formattype = FORMAT_VideoInfo;
    mt.pUnk = 0;
    mt.cbFormat = sizeof vih;
    mt.pbFormat = (BYTE*)&vih;

    SetRectEmpty(&vih.rcSource);
    SetRectEmpty(&vih.rcTarget);
    vih.dwBitRate = 0;
    vih.dwBitErrorRate = 0;
    vih.AvgTimePerFrame = vihIn.AvgTimePerFrame;

    bmih.biSize = sizeof(BITMAPINFOHEADER);  //40
    bmih.biWidth = bmihIn.biWidth;
    bmih.biHeight = bmihIn.biHeight;
    bmih.biPlanes = 1;
    bmih.biBitCount = 12;
    memcpy(&bmih.biCompression, "YV12", 4);
    bmih.biSizeImage = 0;  //TODO
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;

    m_preferred_mtv.Add(mt);

    img_fmt_t src;

    if (mtIn.subtype == MEDIASUBTYPE_RGB24)
        src = IMG_FMT_RGB24;
    else
    {
        assert(mtIn.subtype == MEDIASUBTYPE_RGB32);
        src = IMG_FMT_RGB32;
    }

    const img_fmt_t dst = IMG_FMT_YV12;  //TODO: liberalize

    m_rgb_to_yuv = on2_get_rgb_to_yuv(dst, src);
    assert(m_rgb_to_yuv);
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
}


void Outpin::StartThread()
{
    assert(m_hThread == 0);

    const uintptr_t h = _beginthreadex(
                            0,  //security
                            0,  //stack size
                            &Outpin::ThreadProc,
                            this,
                            0,   //run immediately
                            0);  //thread id

    m_hThread = reinterpret_cast<HANDLE>(h);
    assert(m_hThread);

#ifdef _DEBUG
    wodbgstream os;
    os << "webmcc::Outpin[" << m_id << "]::StartThread: hThread=0x"
       << hex << h << dec
       << endl;
#endif
}


void Outpin::StopThread()
{
    if (m_hThread == 0)
        return;

#ifdef _DEBUG
    wodbgstream os;
    os << "webmcc::Outpin[" << m_id << "]::StopThread: hThread=0x"
       << hex << uintptr_t(m_hThread) << dec
       << endl;
#endif

    const DWORD dw = WaitForSingleObject(m_hThread, 5000);
    assert(dw == WAIT_OBJECT_0);

    const BOOL b = CloseHandle(m_hThread);
    assert(b);

    m_hThread = 0;
}


unsigned Outpin::ThreadProc(void* pv)
{
    Outpin* const pPin = static_cast<Outpin*>(pv);
    assert(pPin);

#ifdef _DEBUG
    wodbgstream os;
    os << "webmcc::Outpin["
       << pPin->m_id
       << "]::ThreadProc(begin): hThread=0x"
       << hex << uintptr_t(pPin->m_hThread) << dec
       << endl;
#endif

    pPin->Main();

#ifdef _DEBUG
    os << "webmcc::Outpin["
       << pPin->m_id << "]::ThreadProc(end): hThread=0x"
       << hex << uintptr_t(pPin->m_hThread) << dec
       << endl;
#endif

    return 0;
}


unsigned Outpin::Main()
{
    assert(bool(m_pPinConnection));
    assert(bool(m_pInputPin));

    Inpin& inpin = m_pFilter->m_inpin;
    const HANDLE hSamples = inpin.m_hSamples;

    for (;;)
    {
        Filter::Lock lock;

        HRESULT hrLock = lock.Seize(m_pFilter);
        assert(SUCCEEDED(hrLock));

        if (FAILED(hrLock))
            return 0;  //TODO: signal error

        GraphUtil::IMediaSamplePtr pInSample;

        const int status = inpin.GetSample(&pInSample);

        hrLock = lock.Release();
        assert(SUCCEEDED(hrLock));

        if (FAILED(hrLock))
            return 0;

        if (status < -1)  //terminate thread
            return 0;

        if (status == 0)  //EOS (no payload)
        {
#ifdef _DEBUG
            odbgstream os;
            os << "webmcc::outpin::EOS: calling pin->EOS" << endl;
#endif

            const HRESULT hrEOS = m_pPinConnection->EndOfStream();
            hrEOS;

#ifdef _DEBUG
            os << "webmcc::outpin::EOS: called pin->EOS; hr=0x"
               << hex << hrEOS << dec
               << endl;
#endif
        }
        else if (status > 0)  //have payload to send downstream
        {
            GraphUtil::IMediaSamplePtr pOutSample;

            HRESULT hr = m_pAllocator->GetBuffer(&pOutSample, 0, 0, 0);

            if (hr == S_OK)
            {
                PopulateSample(pInSample, pOutSample);

                hr = m_pInputPin->Receive(pOutSample);

                if (hr == S_OK)
                    continue;
            }

            pInSample = 0;
            inpin.OnCompletion();
        }

        const DWORD dw = WaitForSingleObject(hSamples, INFINITE);

        if (dw == WAIT_FAILED)
            return 0;  //signal error to FGM

        assert(dw == WAIT_OBJECT_0);
    }
}


void Outpin::PopulateSample(
    IMediaSample* pIn,
    IMediaSample* pOut)
{
    assert(pIn);
    assert(pOut);

    //input

    const Inpin& inpin = m_pFilter->m_inpin;

    const VIDEOINFOHEADER* const pvih_in = inpin.GetVideoInfo();
    assert(pvih_in);

    const RECT& rc_in = pvih_in->rcSource;

    const LONG rc_in_w = rc_in.right - rc_in.left;
    assert(rc_in_w >= 0);
    assert(rc_in_w % 2 == 0);  //TODO

    //BITMAPINFOHEADER Structure
    //http://msdn.microsoft.com/en-us/library/dd183376%28v=vs.85%29.aspx

    const BITMAPINFOHEADER& bmih_in = pvih_in->bmiHeader;

    const LONG w_in = bmih_in.biWidth;  //stride
    assert(w_in > 0);
    assert(w_in % 2 == 0);  //TODO
    //assert(w_in == rc_in_w);

    const LONG hh_in = bmih_in.biHeight;
    assert(hh_in);

    const LONG h_in = labs(hh_in);
    assert(h_in % 2 == 0);  //TODO

    BYTE* buf_in;

    HRESULT hr = pIn->GetPointer(&buf_in);
    assert(SUCCEEDED(hr));
    assert(buf_in);

    const AM_MEDIA_TYPE* const pmt_in = inpin.GetMediaType();
    assert(pmt_in);

    const GUID& subtype_in = pmt_in->subtype;

    ULONG bytes_per_pixel;

    if (subtype_in == MEDIASUBTYPE_RGB24)
        bytes_per_pixel = 3;
    else
    {
        assert(subtype_in == MEDIASUBTYPE_RGB32);
        bytes_per_pixel = 4;
    }

    const ULONG stride_in = w_in * bytes_per_pixel;
    const ULONG size_in = stride_in * h_in;  //calculated

    const long actual_size_in = pIn->GetActualDataLength();
    assert(actual_size_in >= 0);
    assert(ULONG(actual_size_in) == size_in);

    //output

    const VIDEOINFOHEADER* const pvih_out = this->GetVideoInfo();
    assert(pvih_out);

    const RECT& rc_out = pvih_out->rcSource;

    const LONG rc_out_w = rc_out.right - rc_out.left;
    assert(rc_out_w >= 0);
    assert(rc_out_w % 2 == 0);  //TODO

    const BITMAPINFOHEADER& bmih_out = pvih_out->bmiHeader;

    const LONG w_out = bmih_out.biWidth;  //stride
    assert(w_out > 0);
    assert(w_out % 2 == 0);  //TODO

    const LONG hh_out = bmih_out.biHeight;
    assert(hh_out);

    const LONG h_out = labs(hh_out);
    assert(h_out % 2 == 0);  //TODO

    BYTE* buf_out;

    hr = pOut->GetPointer(&buf_out);
    assert(SUCCEEDED(hr));
    assert(buf_out);

    const AM_MEDIA_TYPE* const pmt_out = this->GetMediaType();
    assert(pmt_out);
    assert(pmt_out->subtype == MEDIASUBTYPE_YV12);

    BYTE* const y = buf_out;

    const ULONG y_stride = w_out;
    const ULONG y_height = h_out;

    const ULONG y_size = y_stride * y_height;

    const ULONG uv_stride = y_stride / 2;
    const ULONG uv_height = y_height / 2;

    const ULONG uv_size = uv_stride * uv_height;

    const ULONG size_out = y_size + 2 * uv_size;  //calculated

    const long actual_size_out = pOut->GetSize();
    assert(actual_size_out >= 0);
    assert(ULONG(actual_size_out) >= size_out);

    BYTE* const u = y + uv_size;
    BYTE* const v = u + uv_size;

    assert(m_rgb_to_yuv);
    (*m_rgb_to_yuv)(buf_in, w_in, h_in, y, u, v, stride_in, y_stride);

    hr = pOut->SetActualDataLength(size_out);
    assert(SUCCEEDED(hr));

    REFERENCE_TIME st, sp;

    hr = pIn->GetTime(&st, &sp);

    if (hr == S_OK)
    {
        hr = pOut->SetTime(&st, &sp);
        assert(SUCCEEDED(hr));
    }
    else if (SUCCEEDED(hr))
    {
        hr = pOut->SetTime(&st, 0);
        assert(SUCCEEDED(hr));
    }
    else
    {
        hr = pOut->SetTime(0, 0);
        assert(SUCCEEDED(hr));
    }

    hr = pOut->SetSyncPoint(TRUE);
    assert(SUCCEEDED(hr));

    hr = pOut->SetPreroll(FALSE);  //TODO
    assert(SUCCEEDED(hr));

    hr = pOut->SetMediaType(0);
    assert(SUCCEEDED(hr));

    const HRESULT hrDiscontinuity = pIn->IsDiscontinuity();
    assert(SUCCEEDED(hrDiscontinuity));

    const bool bDiscontinuity = (hrDiscontinuity == S_OK);

    hr = pOut->SetDiscontinuity(bDiscontinuity ? TRUE : FALSE);
    assert(SUCCEEDED(hr));

    hr = pOut->SetMediaTime(0, 0);
    assert(SUCCEEDED(hr));
}


}  //end namespace WebmColorConversion
