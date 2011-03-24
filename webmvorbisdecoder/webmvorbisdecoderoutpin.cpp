// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
//#include <comdef.h>
#include "webmvorbisdecoderfilter.hpp"
#include "webmvorbisdecoderoutpin.hpp"
#include "cmediasample.hpp"
#include "mediatypeutil.hpp"
#include "webmtypes.hpp"
#include "vorbistypes.hpp"
#include <vfwmsgs.h>
#include <mmreg.h>
#include <uuids.h>
#include <cassert>
#include <process.h>
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

namespace WebmVorbisDecoderLib
{


Outpin::Outpin(Filter* pFilter) :
    Pin(pFilter, PINDIR_OUTPUT, L"output"),
    m_hThread(0)
{
    SetDefaultMediaTypes();

    //m_hQuit = CreateEvent(0, 0, 0, 0);
    //assert(m_hQuit);
}


Outpin::~Outpin()
{
    assert(m_hThread == 0);
    assert(!bool(m_pAllocator));
    assert(!bool(m_pInputPin));

    //const BOOL b = CloseHandle(m_hQuit);
    //assert(b);
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

    //m_bDone = true;
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

    const long cBuffers = 3;

    if (props.cBuffers < cBuffers)
        props.cBuffers = cBuffers;

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    assert(mt.formattype == FORMAT_WaveFormatEx);
    assert(mt.cbFormat >= 18);
    assert(mt.pbFormat);

    const WAVEFORMATEX& wfx = (WAVEFORMATEX&)(*mt.pbFormat);

    const DWORD target_count = wfx.nSamplesPerSec / Pin::kSampleRateDivisor;
    const long cbBuffer = target_count * wfx.nBlockAlign;

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

    if (mtOut.majortype != MEDIATYPE_Audio)
        return S_FALSE;

    if (mtOut.subtype != MEDIASUBTYPE_IEEE_FLOAT)
        return S_FALSE;

    if (mtOut.formattype != FORMAT_WaveFormatEx)
        return S_FALSE;

    if (mtOut.pbFormat == 0)
        return S_FALSE;

    if (mtOut.cbFormat < 18)
        return S_FALSE;

    const WAVEFORMATEX& wfxOut = (WAVEFORMATEX&)(*mtOut.pbFormat);

    if (wfxOut.wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
        return S_FALSE;

    if (wfxOut.cbSize > 0)
        return S_FALSE;

    const AM_MEDIA_TYPE& mtIn = inpin.m_connection_mtv[0];
    const WAVEFORMATEX& wfxIn = (WAVEFORMATEX&)(*mtIn.pbFormat);

    if (wfxOut.nChannels != wfxIn.nChannels)
        return S_FALSE;

    if (wfxOut.nSamplesPerSec != wfxIn.nSamplesPerSec)
        return S_FALSE;

    if (wfxOut.nBlockAlign != wfxIn.nBlockAlign)
        return S_FALSE;

    //TODO: check bits/sample

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
    const wchar_t* const name_ = L"PCM (IEEE Float)";

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
    typedef VorbisTypes::VORBISFORMAT2 FMT;
    const FMT& fmt = (FMT&)(*mtIn.pbFormat);

    m_preferred_mtv.Clear();

    AM_MEDIA_TYPE mt;
    WAVEFORMATEX wfx;

    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = MEDIASUBTYPE_IEEE_FLOAT;
    mt.bFixedSizeSamples = TRUE;
    mt.bTemporalCompression = FALSE;
    //mt.lSampleSize
    mt.formattype = FORMAT_WaveFormatEx;
    mt.pUnk = 0;
    mt.cbFormat = 18;
    mt.pbFormat = (BYTE*)&wfx;

    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx.nChannels = static_cast<WORD>(fmt.channels);
    wfx.nSamplesPerSec = fmt.samplesPerSec;

    const size_t bytesPerSample = sizeof(float);
    const size_t bitsPerSample = 8 * bytesPerSample;

    wfx.wBitsPerSample = static_cast<WORD>(bitsPerSample);

    wfx.nBlockAlign = bytesPerSample * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
    wfx.cbSize = 0;

    mt.lSampleSize = wfx.nBlockAlign;

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

    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = MEDIASUBTYPE_IEEE_FLOAT;
    mt.bFixedSizeSamples = TRUE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;
    mt.formattype = GUID_NULL;
    mt.pUnk = 0;
    mt.cbFormat = 0;
    mt.pbFormat = 0;

    m_preferred_mtv.Add(mt);
}


const WAVEFORMATEX* Outpin::GetFormat() const
{
    if (!bool(m_pPinConnection))
        return 0;

    if (m_connection_mtv.Empty())
        return 0;

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    assert(mt.formattype == FORMAT_WaveFormatEx);
    assert(mt.cbFormat >= sizeof(WAVEFORMATEX));
    assert(mt.pbFormat);

    const WAVEFORMATEX& wfx = (WAVEFORMATEX&)(*mt.pbFormat);

    return &wfx;
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
    os << "webmvorbisdec::Outpin[" << m_id << "]::StartThread: hThread=0x"
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
    os << "webmvorbisdec::Outpin[" << m_id << "]::StopThread: hThread=0x"
       << hex << uintptr_t(m_hThread) << dec
       << endl;
#endif

    //BOOL b = SetEvent(m_hQuit);
    //assert(b);

    const DWORD dw = WaitForSingleObject(m_hThread, 5000);
    assert(dw == WAIT_OBJECT_0);

    const BOOL b = CloseHandle(m_hThread);
    assert(b);

    m_hThread = 0;
}


//bool Outpin::Done() const
//{
//    if (m_hThread == 0)
//        return true;
//
//    const DWORD dw = WaitForSingleObject(m_hThread, 0);
//
//    if (dw == WAIT_TIMEOUT)
//        return false;
//
//    assert(dw == WAIT_OBJECT_0);
//
//    return true;
//}


unsigned Outpin::ThreadProc(void* pv)
{
    Outpin* const pPin = static_cast<Outpin*>(pv);
    assert(pPin);

#ifdef _DEBUG
    wodbgstream os;
    os << "webmvorbisdec::Outpin["
       << pPin->m_id
       << "]::ThreadProc(begin): hThread=0x"
       << hex << uintptr_t(pPin->m_hThread) << dec
       << endl;
#endif

    pPin->Main();

#ifdef _DEBUG
    os << "webmvorbisdec::Outpin["
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
        GraphUtil::IMediaSamplePtr pSample;

        Filter::Lock lock;

        HRESULT hrLock = lock.Seize(m_pFilter);
        assert(SUCCEEDED(hrLock));

        if (FAILED(hrLock))
            return 0;  //TODO: signal error

        const int status = inpin.GetSample(&pSample);

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
            os << "webmvorbisdec::outpin::EOS: calling pin->EOS" << endl;
#endif

            const HRESULT hrEOS = m_pPinConnection->EndOfStream();
            hrEOS;

#ifdef _DEBUG
            os << "webmvorbisdec::outpin::EOS: called pin->EOS; hr=0x"
               << hex << hrEOS << dec
               << endl;
#endif
        }
        else if (status > 0)  //have payload to send downstream
        {
#if 0
            LONGLONG st, sp;
            const HRESULT hr = pSample->GetTime(&st, &sp);
            assert(SUCCEEDED(hr));

            odbgstream os;
            os << "A: "
               << fixed
               << setprecision(3)
               << (double(st)/10000000.0)
               << endl;
#endif

            const HRESULT hrReceive = m_pInputPin->Receive(pSample);

            if (hrReceive == S_OK)
                continue;

            inpin.OnCompletion();
        }

        const DWORD dw = WaitForSingleObject(hSamples, INFINITE);

        if (dw == WAIT_FAILED)
            return 0;  //signal error to FGM

        assert(dw == WAIT_OBJECT_0);
    }
}


}  //end namespace WebmVorbisDecoderLib
