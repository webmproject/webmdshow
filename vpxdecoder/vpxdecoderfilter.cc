// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "vpxdecoderfilter.h"

#include <strmif.h>
#include <uuids.h>
#include <vfwmsgs.h>

#include <cassert>
#include <new>

#include "cenumpins.h"
#include "vpxdecoderidl.h"
#include "webmtypes.h"

#ifdef _DEBUG
#include "iidstr.h"
#include "odbgstream.h"
using std::endl;
using std::hex;
using std::dec;
#endif

using std::wstring;

namespace VPXDecoderLib {

HRESULT CreateInstance(IClassFactory* pClassFactory, IUnknown* pOuter,
                       const IID& iid, void** ppv) {
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

  if (SUCCEEDED(hr)) {
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

//
// Filter::CNondelegating
//
HRESULT Filter::CNondelegating::QueryInterface(const IID& iid, void** ppv) {
  if (ppv == 0)
    return E_POINTER;

  IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

  if (iid == __uuidof(IUnknown)) {
    pUnk = this;  // must be nondelegating
  } else if (iid == __uuidof(IBaseFilter) ||
             iid == __uuidof(IMediaFilter) ||
             iid == __uuidof(IPersist)) {
    pUnk = static_cast<IBaseFilter*>(m_pFilter);
  } else if (iid == __uuidof(IVP8PostProcessing)) {
    pUnk = static_cast<IVP8PostProcessing*>(m_pFilter);
  } else {
#if _DEBUG
    wodbgstream os;
    os << "vpxdec::filter::QI: iid=" << IIDStr(iid) << std::endl;
#endif

    pUnk = 0;
    return E_NOINTERFACE;
  }

  pUnk->AddRef();
  return S_OK;
}

ULONG Filter::CNondelegating::AddRef() {
  return InterlockedIncrement(&m_cRef);
}

ULONG Filter::CNondelegating::Release() {
  const LONG n = InterlockedDecrement(&m_cRef);

#if _DEBUG
  odbgstream os;
  os << "vpxdec::filter::Release: n=" << n << endl;
#endif

  if (n > 0)
    return n;

  delete m_pFilter;
  return 0;
}

//
// Filter
//
#pragma warning(disable : 4355)  // 'this' ptr in member init list
Filter::Filter(IClassFactory* pClassFactory, IUnknown* pOuter)
    : m_pClassFactory(pClassFactory),
      m_nondelegating(this),
      m_pOuter(pOuter ? pOuter : &m_nondelegating),
      m_clock(0),
      m_state(kStateStopped),
      m_inpin(this),
      m_outpin(this) {
  m_pClassFactory->LockServer(TRUE);

  const HRESULT hr = CLockable::Init();
  hr;
  assert(SUCCEEDED(hr));

  m_info.pGraph = 0;
  m_info.achName[0] = L'\0';

  // TODO: these need to be read from and written to the .grf stream.
  m_cfg.flags = 0;
  m_cfg.deblock = 0;
  m_cfg.noise = 0;

#ifdef _DEBUG
  odbgstream os;
  os << "vpxdec::filter::ctor" << endl;
#endif
}
#pragma warning(default : 4355)

Filter::~Filter() {
#ifdef _DEBUG
  odbgstream os;
  os << "vpxdec::filter::dtor" << endl;
#endif

  m_pClassFactory->LockServer(FALSE);
}

HRESULT Filter::QueryInterface(const IID& iid, void** ppv) {
  return m_pOuter->QueryInterface(iid, ppv);
}

ULONG Filter::AddRef() {
  return m_pOuter->AddRef();
}

ULONG Filter::Release() {
  return m_pOuter->Release();
}

HRESULT Filter::GetClassID(CLSID* p) {
  if (p == 0)
    return E_POINTER;

  *p = CLSID_VPXDecoder;
  return S_OK;
}

HRESULT Filter::Stop() {
  // Stop is a synchronous operation: when it completes,
  // the filter is stopped.
  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  // odbgstream os;
  // os << "mkvsplit::Filter::Stop" << endl;

  switch (m_state) {
    case kStatePaused:
    case kStatePausedWaitingForKeyframe:
    case kStateRunning:
    case kStateRunningWaitingForKeyframe:
      m_state = kStateStopped;
      OnStop();  // decommit outpin's allocator
      break;

    case kStateStopped:
      break;

    default:
      assert(false);
      break;
  }

  return S_OK;
}

HRESULT Filter::Pause() {
  // Unlike Stop(), Pause() can be asynchronous (that's why you have
  // GetState()).
  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  // odbgstream os;
  // os << "mkvsplit::Filter::Pause" << endl;

  switch (m_state) {
    case kStateStopped:
      OnStart();  // commit outpin's allocator
      m_state = kStatePausedWaitingForKeyframe;
      break;

    case kStateRunning:
      m_state = kStatePaused;
      break;

    case kStateRunningWaitingForKeyframe:
      m_state = kStatePausedWaitingForKeyframe;
      break;

    case kStatePausedWaitingForKeyframe:
    case kStatePaused:
      break;

    default:
      assert(false);
      break;
  }

  return S_OK;
}

HRESULT Filter::Run(REFERENCE_TIME start) {
  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  // odbgstream os;
  // os << "mkvsplit::Filter::Run" << endl;

  switch (m_state) {
    case kStateStopped:
      OnStart();
      m_state = kStateRunningWaitingForKeyframe;
      break;

    case kStatePausedWaitingForKeyframe:
      m_state = kStateRunningWaitingForKeyframe;
      break;

    case kStatePaused:
      m_state = kStateRunning;
      break;

    case kStateRunningWaitingForKeyframe:
    case kStateRunning:
      break;

    default:
      assert(false);
      break;
  }

  m_start = start;
  return S_OK;
}

HRESULT Filter::GetState(DWORD, FILTER_STATE* p) {
  if (p == 0)
    return E_POINTER;

  Lock lock;

  const HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  *p = GetStateLocked();
  return S_OK;
}

HRESULT Filter::SetSyncSource(IReferenceClock* clock) {
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

HRESULT Filter::GetSyncSource(IReferenceClock** pclock) {
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

HRESULT Filter::EnumPins(IEnumPins** pp) {
  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  IPin* pins[2];

  pins[0] = &m_inpin;
  pins[1] = &m_outpin;

  return CEnumPins::CreateInstance(pins, 2, pp);
}

HRESULT Filter::FindPin(LPCWSTR id1, IPin** pp) {
  if (pp == 0)
    return E_POINTER;

  IPin*& p = *pp;
  p = 0;

  if (id1 == 0)
    return E_INVALIDARG;

  Pin* pPin = &m_inpin;

  if (wcscmp(id1, pPin->m_id.c_str()) == 0) {
    p = pPin;
    p->AddRef();

    return S_OK;
  }

  pPin = &m_outpin;

  if (wcscmp(id1, pPin->m_id.c_str()) == 0) {
    p = pPin;
    p->AddRef();

    return S_OK;
  }

  return VFW_E_NOT_FOUND;
}

HRESULT Filter::QueryFilterInfo(FILTER_INFO* p) {
  if (p == 0)
    return E_POINTER;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  enum { size = sizeof(p->achName) / sizeof(WCHAR) };
  const errno_t e = wcscpy_s(p->achName, size, m_info.achName);
  assert(e == 0);
  if (e != 0)
    return E_FAIL;

  p->pGraph = m_info.pGraph;

  if (p->pGraph)
    p->pGraph->AddRef();

  return S_OK;
}

HRESULT Filter::JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR name) {
  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  // NOTE:
  // No, do not adjust reference counts here!
  // Read the docs for the reasons why.
  // ENDNOTE.

  m_info.pGraph = pGraph;

  if (name == 0) {
    m_info.achName[0] = L'\0';
  } else {
    enum { size = sizeof(m_info.achName) / sizeof(WCHAR) };
    const errno_t e = wcscpy_s(m_info.achName, size, name);
    e;
    assert(e == 0);  // TODO
  }

  return S_OK;
}

HRESULT Filter::QueryVendorInfo(LPWSTR* pstr) {
  if (pstr == 0)
    return E_POINTER;

  wchar_t*& str = *pstr;

  str = 0;
  return E_NOTIMPL;
}

HRESULT Filter::SetFlags(int Flags) {
  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  if (Flags & ~0x07)
    return E_INVALIDARG;

  m_cfg.flags = Flags;

  return S_OK;
}

HRESULT Filter::SetDeblockingLevel(int level) {
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

HRESULT Filter::SetNoiseLevel(int level) {
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

HRESULT Filter::GetFlags(int* pFlags) {
  if (pFlags == 0)
    return E_POINTER;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  *pFlags = m_cfg.flags;

  return S_OK;
}

HRESULT Filter::GetDeblockingLevel(int* pLevel) {
  if (pLevel == 0)
    return E_POINTER;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  *pLevel = m_cfg.deblock;

  return S_OK;
}

HRESULT Filter::GetNoiseLevel(int* pLevel) {
  if (pLevel == 0)
    return E_POINTER;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  *pLevel = m_cfg.noise;

  return S_OK;
}

HRESULT Filter::ApplyPostProcessing() {
  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  if (m_state != State_Paused)
    return VFW_E_NOT_PAUSED;

  return m_inpin.OnApplyPostProcessing();
}

void Filter::OnStart() {
  HRESULT hr = m_inpin.Start();
  assert(SUCCEEDED(hr));  // TODO

  hr = m_outpin.Start();
  assert(SUCCEEDED(hr));  // TODO
}

void Filter::OnStop() {
  m_outpin.Stop();
  m_inpin.Stop();
}

FILTER_STATE Filter::GetStateLocked() const {
  switch (m_state) {
    case kStateStopped:
      return State_Stopped;

    case kStateRunning:
    case kStateRunningWaitingForKeyframe:
      return State_Running;

    case kStatePaused:
    case kStatePausedWaitingForKeyframe:
      return State_Paused;

    default:
      assert(false);
      return State_Stopped;
  }
}

HRESULT Filter::OnDecodeFailureLocked() {
  switch (m_state) {
    case kStateRunning:
      m_state = kStateRunningWaitingForKeyframe;
      break;

    case kStatePaused:
      m_state = kStatePausedWaitingForKeyframe;
      break;

    default:
      break;
  }

  return S_OK;  // continue accepting frames
}

void Filter::OnDecodeSuccessLocked(bool is_key) {
  switch (m_state) {
    case kStateRunningWaitingForKeyframe:
      if (is_key)
        m_state = kStateRunning;
      break;

    case kStatePausedWaitingForKeyframe:
      if (is_key)
        m_state = kStatePaused;
      break;

    default:
      break;
  }
}

}  // namespace VPXDecoderLib
