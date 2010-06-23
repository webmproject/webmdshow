// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webmmuxfilter.hpp"
#include "cenumpins.hpp"
#include "graphutil.hpp"
#include "webmtypes.hpp"
#include <new>
#include <cassert>
#include <vfwmsgs.h>
#include <uuids.h>
#include <evcode.h>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif

using std::wstring;

namespace WebmMuxLib
{

HRESULT CreateInstance(
    IClassFactory* pClassFactory,
    IUnknown* pOuter,
    const IID& iid,
    void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    *ppv = 0;

    if ((pOuter != 0) && (iid != __uuidof(IUnknown)))
        return E_INVALIDARG;

    Filter* p = new (std::nothrow) Filter(pClassFactory, pOuter);

    if (p == 0)
        return E_OUTOFMEMORY;

    assert(p->m_nondelegating.m_cRef == 0);

    const HRESULT hr = p->m_nondelegating.QueryInterface(iid, ppv);

    if (SUCCEEDED(hr))
    {
        assert(*ppv);
        assert(p->m_nondelegating.m_cRef == 1);

        return S_OK;
    }

    assert(*ppv == 0);
    assert(p->m_nondelegating.m_cRef == 0);

    delete p;
    p = 0;

    return hr;
}


#pragma warning(disable:4355)  //'this' ptr in member init list
Filter::Filter(IClassFactory* pClassFactory, IUnknown* pOuter)
    : m_pClassFactory(pClassFactory),
      m_nondelegating(this),
      m_pOuter(pOuter ? pOuter : &m_nondelegating),
      m_state(State_Stopped),
      m_clock(0),
      m_inpin_video(this),
      m_inpin_audio(this),
      m_outpin(this)
{
    m_pClassFactory->LockServer(TRUE);

    const HRESULT hr = CLockable::Init();
    hr;
    assert(SUCCEEDED(hr));

    m_info.pGraph = 0;
    m_info.achName[0] = L'\0';

#ifdef _DEBUG
    odbgstream os;
    os << "mkvmux::ctor" << endl;
#endif
}
#pragma warning(default:4355)


Filter::~Filter()
{
#ifdef _DEBUG
    odbgstream os;
    os << "mkvmux::dtor" << endl;
#endif

    m_pClassFactory->LockServer(FALSE);
}



Filter::nondelegating_t::nondelegating_t(Filter* p)
    : m_pFilter(p),
      m_cRef(0)  //see CreateInstance
{
}


Filter::nondelegating_t::~nondelegating_t()
{
}


HRESULT Filter::nondelegating_t::QueryInterface(
    const IID& iid,
    void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == __uuidof(IUnknown))
    {
        pUnk = this;  //must be nondelegating
    }
    else if ((iid == __uuidof(IBaseFilter)) ||
             (iid == __uuidof(IMediaFilter)) ||
             (iid == __uuidof(IPersist)))
    {
        pUnk = static_cast<IBaseFilter*>(m_pFilter);
    }
    else if (iid == __uuidof(IMediaSeeking))
    {
        pUnk = static_cast<IMediaSeeking*>(m_pFilter);
    }
    else if (iid == __uuidof(IAMFilterMiscFlags))
    {
        pUnk = static_cast<IAMFilterMiscFlags*>(m_pFilter);
    }
    else if (iid == __uuidof(IWebmMux))
    {
        pUnk = static_cast<IWebmMux*>(m_pFilter);
    }
    else
    {
#if 0
        wodbgstream os;
        os << "mp3source::filter::QI: iid=" << IIDStr(iid) << std::endl;
#endif
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG Filter::nondelegating_t::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}


ULONG Filter::nondelegating_t::Release()
{
    if (LONG n = InterlockedDecrement(&m_cRef))
        return n;

    delete m_pFilter;
    return 0;
}


HRESULT Filter::QueryInterface(const IID& iid, void** ppv)
{
    return m_pOuter->QueryInterface(iid, ppv);
}


ULONG Filter::AddRef()
{
    return m_pOuter->AddRef();
}


ULONG Filter::Release()
{
    return m_pOuter->Release();
}


HRESULT Filter::GetClassID(CLSID* p)
{
    if (p == 0)
        return E_POINTER;

    *p = CLSID_WebmMux;
    return S_OK;
}



HRESULT Filter::Stop()
{
    //Stop is a synchronous operation: when it completes,
    //the filter is stopped.

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

#if 0
    odbgstream os;
    os << "mux::filter::stop (begin)" << endl;
#endif

    switch (m_state)
    {
        case State_Paused:
        case State_Running:

            //m_inpin_video.Stop();
            //m_inpin_audio.Stop();

            m_outpin.Final();  //close mkv file if req'd

            m_inpin_audio.Final();
            m_inpin_video.Final();

            break;

        case State_Stopped:
        default:
            break;
    }

    m_state = State_Stopped;

#if 0
    os << "mux::filter::stop (end)" << endl;
#endif

    return S_OK;
}


HRESULT Filter::Pause()
{
    //Unlike Stop(), Pause() can be asynchronous (that's why you have
    //GetState()).  We could use that here to build the samples index.

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

#if 0
    odbgstream os;
    os << "mux::filter::pause" << endl;
#endif

    switch (m_state)
    {
        case State_Stopped:
            m_inpin_video.Init();
            m_inpin_audio.Init();
            m_outpin.Init();
            break;

        case State_Running:
        case State_Paused:
        default:
            break;
    }

    m_state = State_Paused;
    return S_OK;
}


HRESULT Filter::Run(REFERENCE_TIME start)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

#if 0
    odbgstream os;
    os << "mux::filter::run" << endl;
#endif

    switch (m_state)
    {
        case State_Stopped:
            m_inpin_video.Init();
            m_inpin_audio.Init();
            m_outpin.Init();
            break;

        case State_Paused:
        case State_Running:
        default:
            m_inpin_video.Run();
            m_inpin_audio.Run();
            break;
    }

    m_start = start;
    m_state = State_Running;

    return S_OK;
}


HRESULT Filter::GetState(
    DWORD /* timeout */ ,
    FILTER_STATE* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_state;

    return S_OK;
}



HRESULT Filter::SetSyncSource(
    IReferenceClock* clock)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_clock)
        m_clock->Release();

    m_clock = clock;

    if (m_clock)
        m_clock->AddRef();

    return S_OK;
}


HRESULT Filter::GetSyncSource(
    IReferenceClock** pclock)
{
    if (pclock == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    IReferenceClock*& clock = *pclock;

    clock = m_clock;

    if (clock)
        clock->AddRef();

    return S_OK;
}


HRESULT Filter::EnumPins(IEnumPins** pp)
{
    IPin* const pa[3] = { &m_inpin_video, &m_inpin_audio, &m_outpin };

    return CEnumPins::CreateInstance(pa, 3, pp);
}



HRESULT Filter::FindPin(
    LPCWSTR id,
    IPin** pp)
{
    if (pp == 0)
        return E_POINTER;

    IPin*& p = *pp;
    p = 0;

    if (id == 0)
        return E_INVALIDARG;

    enum { n = 3 };

    Pin* const pins[n] =
    {
        &m_inpin_video,
        &m_inpin_audio,
        &m_outpin
    };

    for (int i = 0; i < n; ++i)
    {
        Pin* const pin = pins[i];

        if (wcscmp(id, pin->m_id) == 0)
        {
            p = pin;
            p->AddRef();

            return S_OK;
        }
    }

    return VFW_E_NOT_FOUND;
}



HRESULT Filter::QueryFilterInfo(FILTER_INFO* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    enum { size = sizeof(p->achName)/sizeof(WCHAR) };
    const errno_t e = wcscpy_s(p->achName, size, m_info.achName);
    assert(e == 0);
    e;

    p->pGraph = m_info.pGraph;

    if (p->pGraph)
        p->pGraph->AddRef();

    return S_OK;
}


HRESULT Filter::JoinFilterGraph(
    IFilterGraph *pGraph,
    LPCWSTR name)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //NOTE:
    //No, do not adjust reference counts here!
    //Read the docs for the reasons why.
    //ENDNOTE.

    m_info.pGraph = pGraph;

    if (name == 0)
        m_info.achName[0] = L'\0';
    else
    {
        enum { size = sizeof(m_info.achName)/sizeof(WCHAR) };
        const errno_t e = wcscpy_s(m_info.achName, size, name);
        e;
        assert(e == 0);  //TODO
    }

    return S_OK;
}


HRESULT Filter::QueryVendorInfo(LPWSTR* pstr)
{
    if (pstr == 0)
        return E_POINTER;

    wchar_t*& str = *pstr;

    str = 0;
    return E_NOTIMPL;
}


HRESULT Filter::GetCapabilities(DWORD* pdw)
{
    if (pdw == 0)
        return E_POINTER;

    //enum AM_SEEKING_SeekingCapabilities
    //   { AM_SEEKING_CanSeekAbsolute = 0x1,
       // AM_SEEKING_CanSeekForwards = 0x2,
       // AM_SEEKING_CanSeekBackwards = 0x4,
       // AM_SEEKING_CanGetCurrentPos = 0x8,
       // AM_SEEKING_CanGetStopPos = 0x10,
       // AM_SEEKING_CanGetDuration = 0x20,
       // AM_SEEKING_CanPlayBackwards = 0x40,
       // AM_SEEKING_CanDoSegments = 0x80,
       // AM_SEEKING_Source = 0x100
    //    } AM_SEEKING_SEEKING_CAPABILITIES;

    DWORD& dwResult = *pdw;

    dwResult = AM_SEEKING_CanGetCurrentPos |
               AM_SEEKING_CanGetDuration;

#if 0
    Lock lock;

    HRESULT hr = lock.Enter(this);

    if (FAILED(hr))
        return hr;

    //if (m_state == State_Stopped)
    //    dwResult |= AM_SEEKING_CanSeekAbsolute;

    if (IPin* pin = m_inpin_video.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();

            DWORD dw;
            hr = pSeek->GetCapabilities(&dw);

            if (SUCCEEDED(hr))
                dwResult |= dw;

            return S_OK;
        }
    }

    if (IPin* pin = m_inpin_audio.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();

            DWORD dw;
            hr = pSeek->GetCapabilities(&dw);

            if (SUCCEEDED(hr))
                dwResult |= dw;

            return S_OK;
        }
    }
#endif

    return S_OK;
}


HRESULT Filter::CheckCapabilities(DWORD* pdw)
{
    if (pdw == 0)
        return E_POINTER;

    DWORD& dw = *pdw;

    const DWORD dwRequested = dw;

    if (dwRequested == 0)
        return E_INVALIDARG;

    DWORD dwActual;

    HRESULT hr = GetCapabilities(&dwActual);

    if (FAILED(hr))
        return hr;

    if (dwActual == 0)
        return E_FAIL;

    dw &= dwActual;

    if (dw == 0)
        return E_FAIL;

    return (dw == dwRequested) ? S_OK : S_FALSE;
}


HRESULT Filter::IsFormatSupported(const GUID* p)
{
#if 0
    //See my notes for other IMediaSeeking member functions.
    //Because we handle GetCurrentPosition here, by this
    //filter directly, I don't think it's meaningful to
    //attempt to support other time formats besides MEDIA_TIME.

    Lock lock;

    HRESULT hr = lock.Enter(this);

    if (FAILED(hr))
        return hr;

    if (IPin* pin = m_inpin_video.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->IsFormatSupported(p);
        }
    }

    if (IPin* pin = m_inpin_audio.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->IsFormatSupported(p);
        }
    }
#endif

    if (p == 0)
        return E_POINTER;

    const GUID& fmt = *p;

    if (fmt == TIME_FORMAT_MEDIA_TIME)
        return S_OK;

    return S_FALSE;
}


HRESULT Filter::QueryPreferredFormat(GUID* p)
{
#if 0
    Lock lock;

    HRESULT hr = lock.Enter(this);

    if (FAILED(hr))
        return hr;

    if (IPin* pin = m_inpin_video.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->QueryPreferredFormat(p);
        }
    }

    if (IPin* pin = m_inpin_audio.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->QueryPreferredFormat(p);
        }
    }
#endif

    if (p == 0)
        return E_POINTER;

    *p = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
}


HRESULT Filter::GetTimeFormat(GUID* p)
{
#if 0
    Lock lock;

    HRESULT hr = lock.Enter(this);

    if (FAILED(hr))
        return hr;

    if (IPin* pin = m_inpin_video.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->GetTimeFormat(p);
        }
    }

    if (IPin* pin = m_inpin_audio.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->GetTimeFormat(p);
        }
    }
#endif

    if (p == 0)
        return E_POINTER;

    *p = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
}


HRESULT Filter::IsUsingTimeFormat(const GUID* p)
{
#if 0
    Lock lock;

    HRESULT hr = lock.Enter(this);

    if (FAILED(hr))
        return hr;

    if (IPin* pin = m_inpin_video.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->IsUsingTimeFormat(p);
        }
    }

    if (IPin* pin = m_inpin_audio.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->IsUsingTimeFormat(p);
        }
    }
#endif

    if (p == 0)
        return E_INVALIDARG;

    return (*p == TIME_FORMAT_MEDIA_TIME) ? S_OK : S_FALSE;
}


HRESULT Filter::SetTimeFormat(const GUID* p)
{
#if 0
    //TODO: see my comments below in GetCurrentPosition.
    //We might have to NOT pass this request downstream,
    //because we handle GetCurrentPosition locally, and
    //all the filters need to agree on what the curent
    //time format is.

    Lock lock;

    HRESULT hr = lock.Enter(this);

    if (FAILED(hr))
        return hr;

    if (IPin* pin = m_inpin_video.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->SetTimeFormat(p);
        }
    }

    if (IPin* pin = m_inpin_audio.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->SetTimeFormat(p);
        }
    }
#endif

    if (p == 0)
        return E_INVALIDARG;

    const GUID& g = *p;

    if (g == TIME_FORMAT_MEDIA_TIME)
        return S_OK;

    return E_INVALIDARG;
}


HRESULT Filter::GetDuration(LONGLONG* p)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (IPin* pin = m_inpin_video.m_pPinConnection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Release();

            hr = pSeek->GetDuration(p);

            //if (SUCCEEDED(hr))
            //{
            //    odbgstream os;
            //    os << "mkvmux::filter::GetDuration: vt=" << *p << endl;
            //}

            return hr;
        }
    }

    if (IPin* pin = m_inpin_audio.m_pPinConnection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Release();

            hr = pSeek->GetDuration(p);

            //if (SUCCEEDED(hr))
            //{
            //    odbgstream os;
            //    os << "mkvmux::filter::GetDuration: at=" << *p << endl;
            //}

            return hr;
        }
    }

    return E_FAIL;
}


HRESULT Filter::GetStopPosition(LONGLONG* p)
{
    if (p == 0)
        return E_POINTER;

    LONGLONG& t = *p;
    t = _I64_MAX;  //?

    return S_OK;
}


HRESULT Filter::GetCurrentPosition(LONGLONG* p)
{
    if (p == 0)
        return E_POINTER;

    LONGLONG& reftime = *p;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //muxers always return their own position, without
    //forwarding this request upstream.  Apps should
    //always call the filter directly to get the
    //current position, instead of calling the FGM.
    //
    //The only snag here is if the app changes the
    //time format.  We forward that change upstream.
    //But if the app calls GetCurrentPosition, it's
    //handled here, by this filter, which ignores any
    //change in time format and always returns MEDIA_TIME
    //units.
    //
    //Maybe this isn't a real problem, though.  It might
    //be the case that GetCurrentPosition is always
    //filter-specific.  If the app wants to know the
    //"current position" then he might have to call a specific
    //filter directly.  But if that's true (and it is
    //for this muxer filter), then if this muxer were
    //to support other time formats (e.g FRAME), then
    //it would have to cache the requested time format.
    //This does get weird, though, if this filter supported
    //formats different from the upstead filters, then
    //you'd have different parts of the graph using
    //different time formats.  That probably wouldn't
    //work.
    //
    //Maybe the solution is to simply reject all requests
    //for media formats that's aren't for MEDIA_TIME.  This
    //is a muxer, after all, so we're very time-based
    //anyway.  It would be nice, however, to support
    //frame-based requests, allowing the client to seek
    //to, say, the 100th frame, and to request what is the
    //number of the frame mostly recently written
    //(a request that would be handled by this filter).
    //Another possibility is to take the intersection
    //of the formats supported by both this muxer
    //and the upstream filter.

    return m_outpin.GetCurrentPosition(reftime);
}


HRESULT Filter::ConvertTimeFormat(
    LONGLONG* ptgt,
    const GUID* ptgtfmt,
    LONGLONG src,
    const GUID* psrcfmt)
{
#if 0
    Lock lock;

    HRESULT hr = lock.Enter(this);

    if (FAILED(hr))
        return hr;

    if (IPin* pin = m_inpin_video.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->ConvertTimeFormat(ptgt, ptgtfmt, src, psrcfmt);
        }
    }

    if (IPin* pin = m_inpin_audio.m_connection)
    {
        const GraphUtil::IMediaSeekingPtr pSeek(pin);

        if (bool(pSeek))
        {
            lock.Leave();
            return pSeek->ConvertTimeFormat(ptgt, ptgtfmt, src, psrcfmt);
        }
    }
#endif

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


HRESULT Filter::SetPositions(
    LONGLONG* pCurr,
    DWORD dwCurr_,
    LONGLONG* pStop,
    DWORD dwStop_)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    const DWORD dwCurrPos = dwCurr_ & AM_SEEKING_PositioningBitsMask;
    const DWORD dwStopPos = dwStop_ & AM_SEEKING_PositioningBitsMask;

    if (dwCurrPos == AM_SEEKING_NoPositioning)
    {
        if (dwCurr_ & AM_SEEKING_ReturnTime)
        {
            if (pCurr == 0)
                return E_POINTER;

            hr = m_outpin.GetCurrentPosition(*pCurr);

            if (FAILED(hr))
                return hr;
        }

        if (dwStopPos == AM_SEEKING_NoPositioning)
        {
            if (dwStop_ & AM_SEEKING_ReturnTime)
            {
                if (pStop == 0)
                    return E_POINTER;

                LONGLONG& tStop = *pStop;
                tStop = _I64_MAX;  //TODO
            }

            return S_FALSE;  //no position change
        }

        if (pStop == 0)
            return E_INVALIDARG;

        LONGLONG& tStop = *pStop;
        tStop;

        return E_FAIL;  //TODO: support setting stop pos
    }

    //Check for errors first, before changing any state.

    if (pCurr == 0)
        return E_INVALIDARG;

    switch (dwCurrPos)
    {
        case AM_SEEKING_IncrementalPositioning:
        default:
            return E_INVALIDARG;  //applies only to stop pos

        case AM_SEEKING_AbsolutePositioning:
        case AM_SEEKING_RelativePositioning:
            break;
    }

    switch (dwStopPos)
    {
        case AM_SEEKING_NoPositioning:
            if (((dwStop_ & AM_SEEKING_ReturnTime) != 0) && (pStop == 0))
                return E_POINTER;

            break;

        case AM_SEEKING_AbsolutePositioning:
        case AM_SEEKING_RelativePositioning:
        case AM_SEEKING_IncrementalPositioning:
            return E_FAIL;
    }

    //Args have now been vetted.

    assert(pCurr);  //vetted above
    LONGLONG& tCurr = *pCurr;

    if (m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    switch (dwCurrPos)
    {
        case AM_SEEKING_IncrementalPositioning:  //applies only to stop pos
        default:
            assert(false);
            return E_FAIL;

        case AM_SEEKING_AbsolutePositioning:
        {
            if (tCurr > 0)
                return E_INVALIDARG;

            break;
        }
        case AM_SEEKING_RelativePositioning:
        {
            //We're stopped, so we assume "curr time" is 0.

            if (tCurr > 0)
                return E_INVALIDARG;

            break;
        }
    }

    hr = m_inpin_video.ResetPosition();

    if (FAILED(hr))
        return hr;

    hr = m_inpin_audio.ResetPosition();

    if (FAILED(hr))
        return hr;

    if (dwCurr_ & AM_SEEKING_ReturnTime)
        tCurr = 0;

    if (dwStop_ & AM_SEEKING_ReturnTime)
    {
        assert(pStop);  //we checked this above
        LONGLONG& tStop = *pStop;

        tStop = _I64_MAX;  //TODO
    }

    return S_OK;
}


HRESULT Filter::GetPositions(
    LONGLONG* pCurrPos,
    LONGLONG* pStopPos)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (pCurrPos == 0)
        return E_POINTER;

    LONGLONG& reftime = *pCurrPos;

    hr = m_outpin.GetCurrentPosition(reftime);

    if (FAILED(hr))
        return hr;

    if (pStopPos)
        *pStopPos = _I64_MAX;  //?

    return S_OK;
}


HRESULT Filter::GetAvailable(
    LONGLONG* pEarliest,
    LONGLONG* pLatest)
{
    if ((pEarliest == 0) || (pLatest == 0))
        return E_POINTER;

    *pEarliest = 0;
    *pLatest = 0;  //?

    return S_OK;
}


HRESULT Filter::SetRate(double r)
{
    //TODO: still need to look at MKV support for this

    if (r == 1)
        return S_OK;

    if (r <= 0)
        return E_INVALIDARG;

    return E_NOTIMPL;  //TODO: better return here?
}


HRESULT Filter::GetRate(double* p)
{
    //TODO: still need to look at MKV support for this

    if (p == 0)
        return E_POINTER;

    *p = 1;
    return S_OK;
}


HRESULT Filter::GetPreroll(LONGLONG* p)
{
    if (p == 0)
        return E_POINTER;

    *p = 0;  //?
    return S_OK;
}


ULONG Filter::GetMiscFlags()
{
    return AM_FILTER_MISC_FLAGS_IS_RENDERER;
}


HRESULT Filter::SetWritingApp(const wchar_t* str)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (str == 0)
        m_ctx.m_writing_app.clear();

    else if (wcslen(str) > 126)  //1-byte u-int max
        return E_INVALIDARG;

    else
        m_ctx.m_writing_app = str;

    return S_OK;
}


HRESULT Filter::GetWritingApp(wchar_t** p)
{
    if (p == 0)
        return E_POINTER;

    wchar_t*& str = *p;
    str = 0;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    const wstring& app = m_ctx.m_writing_app;

    if (app.empty())
        return S_OK;

    const size_t len = app.length();   //wchar strlen
    const size_t buflen = len + 1;               //wchar strlen + wchar null
    const size_t cb = buflen * sizeof(wchar_t);  //total bytes

    str = (wchar_t*)CoTaskMemAlloc(cb);

    if (str == 0)
        return E_OUTOFMEMORY;

    const errno_t e = wcscpy_s(str, buflen, app.c_str());
    e;
    assert(e == 0);

    return S_OK;
}



HRESULT Filter::OnEndOfStream()
{
#if 1
    //NOTE: it appears that you must do both of send EOS downstream
    //to the file writer, and act as renderer (as described below):
    if (IPin* pin = m_outpin.m_pPinConnection)
    {
        const HRESULT hr = pin->EndOfStream();
        hr;
        assert(SUCCEEDED(hr));
    }

    //"End-of-Stream Notifications"
    //http://msdn.microsoft.com/en-us/library/dd375604(VS.85).aspx

    //The key to getting this to work is to treat this muxer as
    //as renderer.  We do that by supporting both IMediaSeeking and
    //IAMFilterMiscFlags.  As the API states:
    //
    //(begin quote)
    //To determine the number of streams, the Filter Graph Manager
    //counts the number of filters that support seeking (through
    //IMediaSeeking or IMediaPosition) and have a rendered input pin,
    //which is defined as an input pin with no corresponding outputs.
    //The Filter Graph Manager determines whether a pin is rendered
    //in one of two ways:
    //
    //(1)  The pin's IPin::QueryInternalConnections method returns zero
    //     in the nPin parameter.
    //(2)  The filter exposes the IAMFilterMiscFlags interface and
    //     returns the AM_FILTER_MISC_FLAGS_IS_RENDERER flag.
    //(end quote)
    //
    //See also:
    //"IPin::QueryInternalConnections Method"
    //http://msdn.microsoft.com/en-us/library/dd390431(VS.85).aspx
    //
    //(begin quote)
    //This method has another use that is now deprecated: The Filter
    //Graph Manager treats a filter as being a renderer filter if at
    //least one input pin implements this method but returns zero in nPin.
    //If you are writing a new renderer filter, however, you should
    //implement the IAMFilterMiscFlags interface instead of using this
    //method to indicate that the filter is a renderer.
    //(end quote)

#if 0
    odbgstream os;
    os << "muxer::filter::OnEndOfStream: sending EC_COMPLETE" << endl;
#endif

    _COM_SMARTPTR_TYPEDEF(IMediaEventSink, __uuidof(IMediaEventSink));

    const IMediaEventSinkPtr pSink(m_info.pGraph);
    assert(bool(pSink));

    const HRESULT hr = pSink->Notify(EC_COMPLETE, S_OK, 0);
    hr;
    assert(SUCCEEDED(hr));

#else
    //TODO: I thought that sending EOS should be enough, but apparently
    //the EC_COMPLETE is not being delivered to the main loop.
    //I don't know why.  As a workaround, just manually send
    //a user event, so we know something gets delivered.

    _COM_SMARTPTR_TYPEDEF(IMediaEventSink, __uuidof(IMediaEventSink));

    const IMediaEventSinkPtr pSink(m_info.pGraph);
    assert(bool(pSink));

    const HRESULT hr = pSink->Notify(EC_USER + 0x100, 0, 0);
    hr;
#endif

    return S_OK;
}


} //end namespace WebmMuxLib
