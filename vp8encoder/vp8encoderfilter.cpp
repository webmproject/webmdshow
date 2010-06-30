// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include <uuids.h>
#include "vp8encoderfilter.hpp"
#include "vp8encoderidl.h"
#include "cenumpins.hpp"
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

namespace VP8EncoderLib
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
      m_outpin_video(this),
      m_outpin_preview(this),
      m_bDirty(false),
      m_bForceKeyframe(false)
{
    m_pClassFactory->LockServer(TRUE);

    const HRESULT hr = CLockable::Init();
    assert(SUCCEEDED(hr));

    m_info.pGraph = 0;
    m_info.achName[0] = L'\0';

    m_cfg.Init();

#ifdef _DEBUG
    odbgstream os;
    os << "vp8enc::filter::ctor" << endl;
#endif
}
#pragma warning(default:4355)


Filter::~Filter()
{
#ifdef _DEBUG
    odbgstream os;
    os << "vp8enc::filter::dtor" << endl;
#endif

    m_pClassFactory->LockServer(FALSE);
}


void Filter::Config::Init()
{
    deadline = -1;
    threads = -1;
    error_resilient = -1;
    lag_in_frames = -1;
    target_bitrate = -1;
    min_quantizer = -1;
    max_quantizer = -1;
    undershoot_pct = -1;
    overshoot_pct = -1;
    decoder_buffer_size = -1;
    decoder_buffer_initial_size = -1;
    decoder_buffer_optimal_size = -1;
    keyframe_mode = -1;
    keyframe_min_interval = -1;
    keyframe_max_interval = -1;
    dropframe_thresh = -1;
    resize_allowed = -1;
    resize_up_thresh = -1;
    resize_down_thresh = -1;
    end_usage = -1;
    token_partitions = -1;
    pass_mode = -1;
    two_pass_stats_buf = 0;
    two_pass_stats_buflen = -1;
    two_pass_vbr_bias_pct = -1;
    two_pass_vbr_minsection_pct = -1;
    two_pass_vbr_maxsection_pct = -1;
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
    else if (iid == __uuidof(IPersistStream))
    {
        pUnk = static_cast<IPersistStream*>(m_pFilter);
    }
    else if (iid == __uuidof(IVP8Encoder))
    {
        pUnk = static_cast<IVP8Encoder*>(m_pFilter);
    }
    else
    {
#if 0
        wodbgstream os;
        os << "vp8enc::filter::QI: iid=" << IIDStr(iid) << std::endl;
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

    *p = CLSID_VP8Encoder;
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
            hr = OnStart();  //commit outpin's allocator

            if (FAILED(hr))
                return hr;

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
            hr = OnStart();  //commit outpin's allocator

            if (FAILED(hr))
                return hr;

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

    IPin* pins[3] =
    {
        &m_inpin,
        &m_outpin_video,
        &m_outpin_preview
    };

    return CEnumPins::CreateInstance(pins, 3, pp);
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

    Pin* pins[3] =
    {
        &m_inpin,
        &m_outpin_video,
        &m_outpin_preview
    };

    Pin** iter = pins;

    for (int i = 0; i < 3; ++i)
    {
        Pin* const pin = *iter++;

        const wstring& id2_ = pin->m_id;
        const wchar_t* const id2 = id2_.c_str();

        if (wcscmp(id1, id2) == 0)  //case-sensitive
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


HRESULT Filter::ApplySettings()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_state == State_Stopped)
        return S_FALSE;

    return m_inpin.OnApplySettings();
}


HRESULT Filter::ResetSettings()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.Init();
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::SetDeadline(int deadline)
{
    if (deadline < 0)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.deadline = deadline;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetDeadline(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.deadline;

    return S_OK;
}



HRESULT Filter::SetThreadCount(int count)
{
    if (count < 0)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.threads = count;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetThreadCount(int* pCount)
{
    if (pCount == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pCount = m_cfg.threads;

    return S_OK;
}


HRESULT Filter::SetErrorResilient(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.error_resilient = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetErrorResilient(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.error_resilient;

    return S_OK;
}


HRESULT Filter::SetDropframeThreshold(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.dropframe_thresh = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetDropframeThreshold(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.dropframe_thresh;
    return S_OK;
}


HRESULT Filter::SetResizeAllowed(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.resize_allowed = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetResizeAllowed(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.resize_allowed;
    return S_OK;
}


HRESULT Filter::SetResizeUpThreshold(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.resize_up_thresh = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetResizeUpThreshold(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.resize_up_thresh;
    return S_OK;
}


HRESULT Filter::SetResizeDownThreshold(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.resize_down_thresh = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetResizeDownThreshold(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.resize_down_thresh;
    return S_OK;
}


HRESULT Filter::SetEndUsage(VP8EndUsage val_)
{
    const int val = val_;

    switch (val)
    {
        case kEndUsageVBR:
        case kEndUsageCBR:
        case kEndUsageDefault:
            break;

        default:
            return E_INVALIDARG;
    }

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.end_usage = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetEndUsage(VP8EndUsage* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = static_cast<VP8EndUsage>(m_cfg.end_usage);

    return S_OK;
}


HRESULT Filter::SetLagInFrames(int LagInFrames)
{
    if (LagInFrames < 0)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.lag_in_frames = LagInFrames;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetLagInFrames(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.lag_in_frames;

    return S_OK;
}


HRESULT Filter::SetTokenPartitions(int val)
{
    if (val > 3)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    m_cfg.token_partitions = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetTokenPartitions(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.token_partitions;
    return S_OK;
}


HRESULT Filter::SetTargetBitrate(int value)
{
    if (value < 0)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.target_bitrate = value;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetTargetBitrate(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.target_bitrate;
    return S_OK;
}


HRESULT Filter::SetMinQuantizer(int val)
{
    if (val > 63)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.min_quantizer = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetMinQuantizer(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.min_quantizer;
    return S_OK;
}


HRESULT Filter::SetMaxQuantizer(int val)
{
    if (val > 63)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.max_quantizer = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetMaxQuantizer(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.max_quantizer;
    return S_OK;
}


HRESULT Filter::SetUndershootPct(int val)
{
    if (val > 100)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.undershoot_pct = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetUndershootPct(int* pval)
{
    if (pval == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pval = m_cfg.undershoot_pct;
    return S_OK;
}


HRESULT Filter::SetOvershootPct(int val)
{
    if (val > 100)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.overshoot_pct = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetOvershootPct(int* pval)
{
    if (pval == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pval = m_cfg.overshoot_pct;
    return S_OK;
}


HRESULT Filter::SetDecoderBufferSize(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.decoder_buffer_size = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetDecoderBufferSize(int* pval)
{
    if (pval == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pval = m_cfg.decoder_buffer_size;
    return S_OK;
}


HRESULT Filter::SetDecoderBufferInitialSize(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.decoder_buffer_initial_size = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetDecoderBufferInitialSize(int* pval)
{
    if (pval == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pval = m_cfg.decoder_buffer_initial_size;
    return S_OK;
}


HRESULT Filter::SetDecoderBufferOptimalSize(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.decoder_buffer_optimal_size = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetDecoderBufferOptimalSize(int* pval)
{
    if (pval == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pval = m_cfg.decoder_buffer_optimal_size;
    return S_OK;
}


HRESULT Filter::SetKeyframeMode(VP8KeyframeMode m)
{
    switch (m)
    {
        case kKeyframeModeDefault:
        case kKeyframeModeDisabled:
        case kKeyframeModeAuto:
            break;

        default:
            return E_INVALIDARG;
    }

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.keyframe_mode = m;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetKeyframeMode(VP8KeyframeMode* pm)
{
    if (pm == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pm = static_cast<VP8KeyframeMode>(m_cfg.keyframe_mode);
    return S_OK;
}


HRESULT Filter::SetKeyframeMinInterval(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.keyframe_min_interval = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetKeyframeMinInterval(int* pval)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pval = m_cfg.keyframe_min_interval;
    return S_OK;
}


HRESULT Filter::SetKeyframeMaxInterval(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.keyframe_max_interval = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetKeyframeMaxInterval(int* pval)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pval = m_cfg.keyframe_max_interval;
    return S_OK;
}


HRESULT Filter::SetPassMode(VP8PassMode m)
{
    switch (m)
    {
        //case kPassModeDefault:  //TODO
        case kPassModeOnePass:
        case kPassModeFirstPass:
        case kPassModeLastPass:
            break;

        default:
            return E_INVALIDARG;
    }

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;


    //TODO: for now, don't serialize pass mode


    Config::int32_t& tgt = m_cfg.pass_mode;

    if (m == tgt)  //no need for any other checks
        return S_OK;

    OutpinVideo& outpin = m_outpin_video;

    hr = outpin.OnSetPassMode(m);

    if (FAILED(hr))
        return hr;

    tgt = m;
    return S_OK;
}


HRESULT Filter::GetPassMode(VP8PassMode* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_cfg.pass_mode < 0)
        *p = kPassModeOnePass;
    else
        *p = static_cast<VP8PassMode>(m_cfg.pass_mode);

    return S_OK;
}


HRESULT Filter::SetTwoPassStatsBuf(const BYTE* buf, LONGLONG len)
{
    if ((len < 0) || ((len > 0) && (buf == 0)))
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    m_cfg.two_pass_stats_buf = buf;
    m_cfg.two_pass_stats_buflen = len;

    return S_OK;
}


HRESULT Filter::GetTwoPassStatsBuf(const BYTE** pbuf, LONGLONG* plen)
{
    if (pbuf)
        *pbuf = 0;

    if (plen)
        *plen = 0;

    if ((pbuf == 0) || (plen == 0))
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *pbuf = m_cfg.two_pass_stats_buf;

    const LONGLONG src = m_cfg.two_pass_stats_buflen;
    *plen = (src <= 0) ? 0 : src;

    return S_OK;
}


HRESULT Filter::SetTwoPassVbrBiasPct(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.two_pass_vbr_bias_pct = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetTwoPassVbrBiasPct(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.two_pass_vbr_bias_pct;
    return S_OK;
}


HRESULT Filter::SetTwoPassVbrMinsectionPct(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.two_pass_vbr_minsection_pct = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetTwoPassVbrMinsectionPct(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.two_pass_vbr_minsection_pct;
    return S_OK;
}


HRESULT Filter::SetTwoPassVbrMaxsectionPct(int val)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_cfg.two_pass_vbr_maxsection_pct = val;
    m_bDirty = true;

    return S_OK;
}


HRESULT Filter::GetTwoPassVbrMaxsectionPct(int* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    *p = m_cfg.two_pass_vbr_maxsection_pct;
    return S_OK;
}


HRESULT Filter::SetForceKeyframe()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_bForceKeyframe = true;
    return S_OK;
}


HRESULT Filter::ClearForceKeyframe()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_bForceKeyframe = false;
    return S_OK;
}


HRESULT Filter::IsDirty()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    return m_bDirty ? S_OK : S_FALSE;
}


HRESULT Filter::Load(IStream* pStream)
{
    if (pStream == 0)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    unsigned __int32 size;
    ULONG cbRead;

    hr = pStream->Read(&size, 4, &cbRead);

    if (FAILED(hr))
        return hr;

    if (cbRead != 4)
        return E_FAIL;

    if (size != sizeof(Config))
        return E_FAIL;

    Config cfg;

    hr = pStream->Read(&cfg, size, &cbRead);

    if (FAILED(hr))
        return hr;

    if (cbRead != size)
        return E_FAIL;

    m_cfg = cfg;

    m_cfg.pass_mode = -1;
    m_cfg.two_pass_stats_buf = 0;
    m_cfg.two_pass_stats_buflen = -1;

    return S_OK;
}


HRESULT Filter::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == 0)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    typedef unsigned __int32 uint32_t;

    const uint32_t size = static_cast<uint32_t>(sizeof(Config));

    ULONG cbWritten;

    hr = pStm->Write(&size, 4, &cbWritten);

    if (FAILED(hr))
        return hr;

    if (cbWritten != 4)
        return E_FAIL;  //?

    hr = pStm->Write(&m_cfg, size, &cbWritten);

    if (FAILED(hr))
        return hr;

    if (cbWritten != size)
        return E_FAIL;

    if (fClearDirty)
        m_bDirty = false;

    return S_OK;
}


HRESULT Filter::GetSizeMax(ULARGE_INTEGER* pcb)
{
    if (pcb == 0)
        return E_POINTER;

    ULARGE_INTEGER& cb = *pcb;

    cb.QuadPart = 4 + sizeof(Config);

    return S_OK;
}



HRESULT Filter::OnStart()
{
    HRESULT hr = m_inpin.Start();

    if (FAILED(hr))
        return hr;

    hr = m_outpin_video.Start();

    if (FAILED(hr))
    {
        m_inpin.Stop();
        return hr;
    }

    hr = m_outpin_preview.Start();

    if (FAILED(hr))
    {
        m_outpin_video.Stop();
        m_inpin.Stop();

        return hr;
    }

    return S_OK;
}


void Filter::OnStop()
{
    m_outpin_preview.Stop();
    m_outpin_video.Stop();
    m_inpin.Stop();
}


VP8PassMode Filter::GetPassMode() const
{
    const Config::int32_t m = m_cfg.pass_mode;

    if (m < 0)
        return kPassModeOnePass;  //default

    return static_cast<VP8PassMode>(m);
}



}  //end namespace VP8EncoderLib
