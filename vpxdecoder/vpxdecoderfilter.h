// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMDSHOW_VPXDECODER_VPXDECODERFILTER_H_
#define WEBMDSHOW_VPXDECODER_VPXDECODERFILTER_H_

#include <strmif.h>

#include <string>

#include "clockable.h"
#include "vpxdecoderidl.h"
#include "vpxdecoderinpin.h"
#include "vpxdecoderoutpin.h"

namespace VPXDecoderLib {

class Filter : public IBaseFilter, public IVP8PostProcessing, public CLockable {
 public:
  struct Config {
    int flags;
    int deblock;
    int noise;
  };

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

  // IVP8PostProcessing
  HRESULT STDMETHODCALLTYPE SetFlags(int);
  HRESULT STDMETHODCALLTYPE SetDeblockingLevel(int);
  HRESULT STDMETHODCALLTYPE SetNoiseLevel(int);
  HRESULT STDMETHODCALLTYPE GetFlags(int*);
  HRESULT STDMETHODCALLTYPE GetDeblockingLevel(int*);
  HRESULT STDMETHODCALLTYPE GetNoiseLevel(int*);
  HRESULT STDMETHODCALLTYPE ApplyPostProcessing();

  // local classes and methods
  FILTER_STATE GetStateLocked() const;
  HRESULT OnDecodeFailureLocked();
  void OnDecodeSuccessLocked(bool is_key);

  // public members
  FILTER_INFO m_info;
  Inpin m_inpin;
  Outpin m_outpin;
  Config m_cfg;

 private:
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

  enum State {
    kStateStopped,
    kStatePausedWaitingForKeyframe,
    kStatePaused,
    kStateRunning,
    kStateRunningWaitingForKeyframe
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

}  // namespace VPXDecoderLib

#endif  // WEBMDSHOW_VPXDECODER_VPXDECODERFILTER_H_