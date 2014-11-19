// Copyright (c) 2013 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include <comdef.h>
#include <strmif.h>
#include <uuids.h>

#include <cassert>

#include "cfactory.h"
#include "comreg.h"
#include "graphutil.h"
#include "vp9decoderidl.h"
#include "webmtypes.h"

HMODULE g_hModule;
static ULONG s_cLock;

namespace VP9DecoderLib {
HRESULT CreateInstance(IClassFactory*, IUnknown*, const IID&, void**);

}  // namespace VP9DecoderLib

static CFactory s_factory(&s_cLock, &VP9DecoderLib::CreateInstance);

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID) {
  switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      g_hModule = hModule;
      break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
    default:
      break;
  }

  return TRUE;
}

STDAPI DllCanUnloadNow() { return s_cLock ? S_FALSE : S_OK; }

STDAPI DllGetClassObject(const CLSID& clsid, const IID& iid, void** ppv) {
  if (clsid == CLSID_VP9Decoder)
    return s_factory.QueryInterface(iid, ppv);

  return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllUnregisterServer() {
  const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
  assert(bool(pMapper));

  HRESULT hr = pMapper->UnregisterFilter(&CLSID_LegacyAmFilterCategory, 0,
                                         CLSID_VP9Decoder);
  assert(SUCCEEDED(hr));
  if (FAILED(hr))
    return hr;

  hr = ComReg::UnRegisterCoclass(CLSID_VP9Decoder);
  assert(SUCCEEDED(hr));
  if (FAILED(hr))
    return hr;

  std::wstring filename;

  hr = ComReg::ComRegGetModuleFileName(g_hModule, filename);
  assert(SUCCEEDED(hr));
  assert(!filename.empty());
  if (FAILED(hr) || filename.empty())
    return hr;

  hr = ComReg::UnRegisterTypeLibResource(filename.c_str());
  assert(SUCCEEDED(hr));
  if (FAILED(hr))
    return hr;

  return S_OK;  // TODO
}

STDAPI DllRegisterServer() {
  std::wstring filename_;

  HRESULT hr = ComReg::ComRegGetModuleFileName(g_hModule, filename_);
  assert(SUCCEEDED(hr));
  assert(!filename_.empty());
  if (FAILED(hr))
    return hr;

  const wchar_t* const filename = filename_.c_str();

#if _DEBUG
  const wchar_t friendlyname[] = L"WebM VP9 Decoder Filter (Debug)";
#else
  const wchar_t friendlyname[] = L"WebM VP9 Decoder Filter";
#endif

  hr = DllUnregisterServer();
  assert(SUCCEEDED(hr));
  if (FAILED(hr))
    return hr;

  hr = ComReg::RegisterTypeLibResource(filename, 0);
  assert(SUCCEEDED(hr));
  if (FAILED(hr))
    return hr;

  hr = ComReg::RegisterCoclass(
      CLSID_VP9Decoder, friendlyname, filename, L"Webm.VP9Decoder",
      L"Webm.VP9Decoder.1",
      false,  // not insertable
      false,  // not a control
      ComReg::kBoth,  // DShow filters must support "both"
      LIBID_VP9DecoderLib,  // typelib
      0,  // no version specified
      0);  // no toolbox bitmap

  assert(SUCCEEDED(hr));
  if (FAILED(hr))
    return hr;

  const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
  assert(pMapper != NULL);
  if (pMapper == NULL)
    return E_FAIL;


  // Describe the input and output pins.
  enum { cPins = 2 };
  REGFILTERPINS pins[cPins];

  REGFILTERPINS& inpin = pins[0];

  enum { nInpinMediaTypes = 1 };
  const REGPINTYPES inpinMediaTypes[nInpinMediaTypes] = {
      {&MEDIATYPE_Video, &WebmTypes::MEDIASUBTYPE_VP90}};

  inpin.strName = 0;  // obsolete
  inpin.bRendered = FALSE;  // TODO: ?
  inpin.bOutput = FALSE;
  inpin.bZero = FALSE;
  inpin.bMany = FALSE;
  inpin.clsConnectsToFilter = 0;  // obsolete
  inpin.strConnectsToPin = 0;  // obsolete
  inpin.nMediaTypes = nInpinMediaTypes;
  inpin.lpMediaType = inpinMediaTypes;

  REGFILTERPINS& outpin = pins[1];

  enum { nOutpinMediaTypes = 7 };
  const REGPINTYPES outpinMediaTypes[nOutpinMediaTypes] = {
      {&MEDIATYPE_Video, &MEDIASUBTYPE_NV12},
      {&MEDIATYPE_Video, &MEDIASUBTYPE_YV12},
      {&MEDIATYPE_Video, &WebmTypes::MEDIASUBTYPE_I420},
      {&MEDIATYPE_Video, &MEDIASUBTYPE_UYVY},
      {&MEDIATYPE_Video, &MEDIASUBTYPE_YUY2},
      {&MEDIATYPE_Video, &MEDIASUBTYPE_YUYV},
      {&MEDIATYPE_Video, &MEDIASUBTYPE_YVYU}};

  outpin.strName = 0;  // obsolete
  outpin.bRendered = FALSE;  // always FALSE for outpins
  outpin.bOutput = TRUE;
  outpin.bZero = FALSE;
  outpin.bMany = FALSE;
  outpin.clsConnectsToFilter = 0;  // obsolete
  outpin.strConnectsToPin = 0;  // obsolete
  outpin.nMediaTypes = nOutpinMediaTypes;
  outpin.lpMediaType = outpinMediaTypes;

  // Register the filter.
  REGFILTER2 filter = {0};
  filter.dwVersion = 1;
  filter.dwMerit = MERIT_NORMAL;
  filter.cPins = cPins;
  filter.rgPins = pins;

  return pMapper->RegisterFilter(CLSID_VP9Decoder, friendlyname, 0,
                                 &CLSID_LegacyAmFilterCategory, 0, &filter);
}
