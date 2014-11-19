// Copyright (c) 2013 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMDSHOW_VP9DECODER_VP9DECODERFILTER_HPP_
#define WEBMDSHOW_VP9DECODER_VP9DECODERFILTER_HPP_

#include <strmif.h>

#include <string>

#include "clockable.h"
#include "vp9decoderinpin.h"
#include "vp9decoderoutpin.h"

namespace VP9DecoderLib {

class Filter : public IBaseFilter, public CLockable {
 public:
  // IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  // IBaseFilter
  HRESULT STDMETHODCALLTYPE GetClassID(CLSID*);
  HRESULT STDMETHODCALLTYPE Stop();
  HRESULT STDMETHODCALLTYPE Pause();
  HRESULT STDMETHODCALLTYPE Run(REFERENCE_TIME);
  HRESULT STDMETHODCALLTYPE GetState(DWORD, FILTER_STATE*);
  HRESULT STDMETHODCALLTYPE SetSyncSource(IReferenceClock*);
  HRESULT STDMETHODCALLTYPE GetSyncSource(IReferenceClock**);
  HRESULT STDMETHODCALLTYPE EnumPins(IEnumPins**);
  HRESULT STDMETHODCALLTYPE FindPin(LPCWSTR, IPin**);
  HRESULT STDMETHODCALLTYPE QueryFilterInfo(FILTER_INFO*);
  HRESULT STDMETHODCALLTYPE JoinFilterGraph(IFilterGraph*, LPCWSTR);
  HRESULT STDMETHODCALLTYPE QueryVendorInfo(LPWSTR*);

  FILTER_STATE GetStateLocked() const;
  HRESULT OnDecodeFailureLocked();
  void OnDecodeSuccessLocked(bool is_key);

  FILTER_INFO m_info;
  Inpin m_inpin;
  Outpin m_outpin;

 private:
  enum State {
    kStateStopped,
    kStatePausedWaitingForKeyframe,
    kStatePaused,
    kStateRunning,
    kStateRunningWaitingForKeyframe
  };

  class CNondelegating : public IUnknown {
   public:
    explicit CNondelegating(Filter* f) : m_pFilter(f), m_cRef(0) {}
    virtual ~CNondelegating() {}

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    Filter* const m_pFilter;
    LONG m_cRef;

   private:
    CNondelegating(const CNondelegating&);
    CNondelegating& operator=(const CNondelegating&);
  };

  friend HRESULT CreateInstance(IClassFactory*, IUnknown*, const IID&, void**);

  void OnStop();
  void OnStart();

  Filter(IClassFactory*, IUnknown*);
  virtual ~Filter();
  Filter(const Filter&);
  Filter& operator=(const Filter&);

  IClassFactory* const m_pClassFactory;
  CNondelegating m_nondelegating;
  IUnknown* const m_pOuter;
  REFERENCE_TIME m_start;
  IReferenceClock* m_clock;
  State m_state;
};

}  // namespace VP9DecoderLib

#endif  // WEBMDSHOW_VP9DECODER_VP9DECODERFILTER_HPP_
