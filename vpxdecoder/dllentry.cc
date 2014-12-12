// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
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
#include "vpxdecoderidl.h"
#include "webmtypes.h"

HMODULE g_hModule;
static ULONG s_cLock;

namespace VPXDecoderLib {

HRESULT CreateInstance(IClassFactory*, IUnknown*, const IID&, void**);

}  // namespace VPXDecoderLib

namespace {

CFactory factory(&s_cLock, &VPXDecoderLib::CreateInstance);

// Remove the specified DirectShow filter CLSID from the registry.
HRESULT UnregisterFilterCLSID(CLSID clsid) {
  const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
  if (pMapper == NULL) {
    assert(pMapper != NULL);
    return E_FAIL;
  }

  HRESULT hr =
      pMapper->UnregisterFilter(&CLSID_LegacyAmFilterCategory, 0, clsid);
  if (FAILED(hr)) {
    return hr;
  }

  hr = ComReg::UnRegisterCoclass(CLSID_VPXDecoder);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

// Unregister the VP8 and VP9 filters.
void UnregisterVPxFilters() {
  LPCOLESTR vp8f_olestr = OLESTR("{ED3110F3-5211-11DF-94AF-0026B977EEAA}");
  LPCOLESTR vp9f_olestr = OLESTR("{ED31110A-5211-11DF-94AF-0026B977EEAA}");

  CLSID vp8f_clsid;
  HRESULT hr_vp8 = CLSIDFromString(vp8f_olestr, &vp8f_clsid);

  CLSID vp9f_clsid;
  HRESULT hr_vp9 = CLSIDFromString(vp9f_olestr, &vp9f_clsid);

  if (FAILED(hr_vp8) || FAILED(hr_vp9)) {
    assert(hr_vp8 == S_OK && "VP8Decoder CLSID string conversion failed.");
    assert(hr_vp9 == S_OK && "VP9Decoder CLSID string conversion failed.");
    return;
  }

  hr_vp8 = UnregisterFilterCLSID(vp8f_clsid);
  hr_vp9 = UnregisterFilterCLSID(vp9f_clsid);

  if (FAILED(hr_vp8) || FAILED(hr_vp9)) {
    assert(hr_vp8 == S_OK && "VP8Decoder unregistration failed.");
    assert(hr_vp9 == S_OK && "VP9Decoder unregistration failed.");
  }
}

}  // namespace

BOOL APIENTRY DllMain(HINSTANCE hModule,
                      DWORD dwReason,
                      LPVOID /*lpReserved*/) {
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
  if (clsid == CLSID_VPXDecoder)
    return factory.QueryInterface(iid, ppv);

  return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllUnregisterServer() {
  const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
  if (pMapper == NULL) {
    assert(pMapper != NULL);
    return E_FAIL;
  }

  HRESULT hr = pMapper->UnregisterFilter(&CLSID_LegacyAmFilterCategory, 0,
                                         CLSID_VPXDecoder);
  if (FAILED(hr)) {
    assert(SUCCEEDED(hr));
    return hr;
  }

  hr = ComReg::UnRegisterCoclass(CLSID_VPXDecoder);
  if (FAILED(hr)) {
    assert(SUCCEEDED(hr));
    return hr;
  }

  std::wstring filename_;
  hr = ComReg::ComRegGetModuleFileName(g_hModule, filename_);
  if (FAILED(hr)) {
    assert(SUCCEEDED(hr));
    return hr;
  }
  if (filename_.empty()) {
    assert(!filename_.empty());
    return E_FAIL;
  }

  hr = ComReg::UnRegisterTypeLibResource(filename_.c_str());
  if (FAILED(hr)) {
    assert(SUCCEEDED(hr));
  }

  return hr;
}

STDAPI DllRegisterServer() {
  std::wstring filename_;
  HRESULT hr = ComReg::ComRegGetModuleFileName(g_hModule, filename_);
  if (FAILED(hr)) {
    assert(SUCCEEDED(hr));
    return hr;
  }
  if (filename_.empty()) {
    assert(!filename_.empty());
    return E_FAIL;
  }

#if _DEBUG
  const wchar_t friendlyname[] = L"WebM VPx Decoder Filter (Debug)";
#else
  const wchar_t friendlyname[] = L"WebM VPx Decoder Filter";
#endif

  hr = DllUnregisterServer();
  if (FAILED(hr)) {
    assert(SUCCEEDED(hr));
    return hr;
  }

  // Unregister the VP8 and VP9 filters.
  UnregisterVPxFilters();

  hr = ComReg::RegisterTypeLibResource(filename_.c_str(), 0);
  if (FAILED(hr)) {
    assert(SUCCEEDED(hr));
    return hr;
  }

  hr = ComReg::RegisterCoclass(
      CLSID_VPXDecoder, friendlyname, filename_.c_str(), L"Webm.VPXDecoder",
      L"Webm.VPXDecoder.1",
      false,  // not insertable
      false,  // not a control
      ComReg::kBoth,  // DShow filters must support "both"
      LIBID_VPXDecoderLib,  // typelib
      0,  // no version specified
      0);  // no toolbox bitmap

  if (FAILED(hr)) {
    assert(SUCCEEDED(hr));
    return hr;
  }

  const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
  if (pMapper == NULL) {
    assert(pMapper != NULL);
    return E_FAIL;
  }

  enum { cPins = 2 };
  REGFILTERPINS pins[cPins];

  REGFILTERPINS& inpin = pins[0];

  enum { nInpinMediaTypes = 2 };
  const REGPINTYPES inpinMediaTypes[nInpinMediaTypes] = {
      {&MEDIATYPE_Video, &WebmTypes::MEDIASUBTYPE_VP80},
      {&MEDIATYPE_Video, &WebmTypes::MEDIASUBTYPE_VP90}};

  inpin.strName = 0;
  inpin.bRendered = FALSE;
  inpin.bOutput = FALSE;
  inpin.bZero = FALSE;
  inpin.bMany = FALSE;
  inpin.clsConnectsToFilter = 0;
  inpin.strConnectsToPin = 0;
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

  outpin.strName = 0;
  outpin.bRendered = FALSE;
  outpin.bOutput = TRUE;
  outpin.bZero = FALSE;
  outpin.bMany = FALSE;
  outpin.clsConnectsToFilter = 0;
  outpin.strConnectsToPin = 0;
  outpin.nMediaTypes = nOutpinMediaTypes;
  outpin.lpMediaType = outpinMediaTypes;

  REGFILTER2 filter;
  filter.dwVersion = 1;
  filter.dwMerit = MERIT_NORMAL;
  filter.cPins = cPins;
  filter.rgPins = pins;

  hr = pMapper->RegisterFilter(CLSID_VPXDecoder, friendlyname, 0,
                               &CLSID_LegacyAmFilterCategory, 0, &filter);
  if (FAILED(hr)) {
    assert(SUCCEEDED(hr));
  }

  return hr;
}
