// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include <uuids.h>
#include "vp8decoderfilter.hpp"
#include "vp8decoderidl.h"
#include "cenumpins.hpp"
#include "webmtypes.hpp"
#include <new>
#include <cassert>
#include <vfwmsgs.h>
#ifdef _DEBUG
#include "iidstr.hpp"
#include "odbgstream.hpp"
using std::endl;
using std::hex;
using std::dec;
#endif

using std::wstring;

namespace VP8DecoderLib
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
      m_inpin(this),
      m_outpin(this)
{
    m_pClassFactory->LockServer(TRUE);

    const HRESULT hr = CLockable::Init();
    hr;
    assert(SUCCEEDED(hr));

    m_info.pGraph = 0;
    m_info.achName[0] = L'\0';

    //TODO: these need to be read from and written to the .grf stream.
    m_cfg.flags = 0;
    m_cfg.deblock = 0;
    m_cfg.noise = 0;

#ifdef _DEBUG
    odbgstream os;
    os << "vp8dec::filter::ctor" << endl;
#endif
}
#pragma warning(default:4355)


Filter::~Filter()
{
#ifdef _DEBUG
    odbgstream os;
    os << "vp8dec::filter::dtor" << endl;
#endif

    m_pClassFactory->LockServer(FALSE);
}


Filter::CNondelegating::CNondelegating(Filter* p)
    : m_pFilter(p),
      m_cRef(0)  //see CreateInstance
{
}


Filter::CNondelegating::~CNondelegating()
{
}


HRESULT Filter::CNondelegating::QueryInterface(
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
    else if (iid == __uuidof(IVP8PostProcessing))
    {
        pUnk = static_cast<IVP8PostProcessing*>(m_pFilter);
    }
    else
    {
#if 0
        wodbgstream os;
        os << "vp8dec::filter::QI: iid=" << IIDStr(iid) << std::endl;
#endif
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG Filter::CNondelegating::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}


ULONG Filter::CNondelegating::Release()
{
    const LONG n = InterlockedDecrement(&m_cRef);

    //odbgstream os;
    //os << "Filter::Release: n=" << n << endl;

    if (n > 0)
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

    *p = CLSID_VP8Decoder;
    return S_OK;
}



HRESULT Filter::Stop()
{
    //Stop is a synchronous operation: when it completes,
    //the filter is stopped.

    //odbgstream os;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //odbgstream os;
    //os << "mkvsplit::Filter::Stop" << endl;

    switch (m_state)
    {
        case State_Paused:
        case State_Running:
            m_state = State_Stopped;
            OnStop();    //decommit outpin's allocator
            break;

        case State_Stopped:
        default:
            break;
    }

    return S_OK;
}


HRESULT Filter::Pause()
{
    //Unlike Stop(), Pause() can be asynchronous (that's why you have
    //GetState()).

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //odbgstream os;
    //os << "mkvsplit::Filter::Pause" << endl;

    switch (m_state)
    {
        case State_Stopped:
            OnStart();  //commit outpin's allocator
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

    //odbgstream os;
    //os << "mkvsplit::Filter::Run" << endl;

    switch (m_state)
    {
        case State_Stopped:
            OnStart();
            break;

        case State_Paused:
        case State_Running:
        default:
            break;
    }

    m_start = start;
    m_state = State_Running;

    return S_OK;
}


HRESULT Filter::GetState(
    DWORD,
    FILTER_STATE* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    const HRESULT hr = lock.Seize(this);

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
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    IPin* pins[2];

    pins[0] = &m_inpin;
    pins[1] = &m_outpin;

    return CEnumPins::CreateInstance(pins, 2, pp);
}



HRESULT Filter::FindPin(
    LPCWSTR id1,
    IPin** pp)
{
    if (pp == 0)
        return E_POINTER;

    IPin*& p = *pp;
    p = 0;

    if (id1 == 0)
        return E_INVALIDARG;

    {
        Pin* const pPin = &m_inpin;

        const wstring& id2_ = pPin->m_id;
        const wchar_t* const id2 = id2_.c_str();

        if (wcscmp(id1, id2) == 0)  //case-sensitive
        {
            p = pPin;
            p->AddRef();

            return S_OK;
        }
    }

    {
        Pin* const pPin = &m_outpin;

        const wstring& id2_ = pPin->m_id;
        const wchar_t* const id2 = id2_.c_str();

        if (wcscmp(id1, id2) == 0)  //case-sensitive
        {
            p = pPin;
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
    e;
    assert(e == 0);

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


HRESULT Filter::SetFlags(int Flags)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (Flags & ~0x07)
        return E_INVALIDARG;

    m_cfg.flags = Flags;

    return S_OK;
}


HRESULT Filter::SetDeblockingLevel(int level)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (level < 0)
        return E_INVALIDARG;

    if (level > 16)
        return E_INVALIDARG;

    m_cfg.deblock = level;

    return S_OK;
}


HRESULT Filter::SetNoiseLevel(int level)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (level < 0)
        return E_INVALIDARG;

    if (level > 16)
        return E_INVALIDARG;

    m_cfg.noise = level;

    return S_OK;
}


HRESULT Filter::GetFlags(int* pFlags)
{
    if (pFlags == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pFlags = m_cfg.flags;

    return S_OK;
}


HRESULT Filter::GetDeblockingLevel(int* pLevel)
{
    if (pLevel == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pLevel = m_cfg.deblock;

    return S_OK;
}


HRESULT Filter::GetNoiseLevel(int* pLevel)
{
    if (pLevel == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pLevel = m_cfg.noise;

    return S_OK;
}


HRESULT Filter::ApplyPostProcessing()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_state != State_Paused)
        return VFW_E_NOT_PAUSED;

    return m_inpin.OnApplyPostProcessing();
}


void Filter::OnStart()
{
    HRESULT hr = m_inpin.Start();
    assert(SUCCEEDED(hr));  //TODO

    hr = m_outpin.Start();
    assert(SUCCEEDED(hr));  //TODO
}


void Filter::OnStop()
{
    m_outpin.Stop();
    m_inpin.Stop();
}


}  //end namespace VP8DecoderLib
