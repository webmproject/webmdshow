// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "comreg.h"

#include <malloc.h>

#include <cassert>
#include <sstream>

#include "registry.h"

using std::wstring;
using std::wostringstream;

GUID ComReg::GUIDFromString(const wchar_t* const_str) {
  wchar_t* const str = const_cast<wchar_t*>(const_str);

  GUID guid;

  const HRESULT hr = CLSIDFromString(str, &guid);
  assert(SUCCEEDED(hr));  // TODO
  hr;

  return guid;
}

HRESULT ComReg::ComRegGetModuleFileName(HMODULE h, std::wstring& result) {
  DWORD n = 256;  // number of TCHARS

  for (;;) {
    void* const pv = _alloca(n * sizeof(wchar_t));
    wchar_t* const buf = (wchar_t*)pv;

    const DWORD dw = GetModuleFileNameW(h, buf, n);

    if (dw == 0) {  // error

      result.clear();

      const DWORD e = GetLastError();
      return HRESULT_FROM_WIN32(e);
    }

    if (dw >= n)  // truncation
    {
      n *= 2;
      continue;
    }

    result = buf;
    return S_OK;
  }
}

HRESULT ComReg::RegisterCustomFileType(const wchar_t* ext, const GUID& filter,
                                       const GUID& mediatype,
                                       const GUID& subtype) {
  Registry::Key parent, key;

  LONG e = parent.open(HKEY_CLASSES_ROOT, L"Media Type\\Extensions",
                       KEY_CREATE_SUB_KEY);

  if (e)
    return HRESULT_FROM_WIN32(e);

  e = key.create<wchar_t>(parent, ext, 0, 0, KEY_SET_VALUE, 0);

  if (e)
    return HRESULT_FROM_WIN32(e);

  wchar_t buf[guid_buflen];

  int n = StringFromGUID2(filter, buf, guid_buflen);
  assert(n == guid_buflen);

  e = key.set(L"Source Filter", buf);

  if (e)
    return HRESULT_FROM_WIN32(e);

  if (mediatype != GUID_NULL) {
    n = StringFromGUID2(mediatype, buf, guid_buflen);
    assert(n == guid_buflen);

    e = key.set(L"Media Type", buf);

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  if (subtype != GUID_NULL) {
    n = StringFromGUID2(subtype, buf, guid_buflen);
    assert(n == guid_buflen);

    e = key.set(L"Subtype", buf);

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  return S_OK;
}

HRESULT ComReg::UnRegisterCustomFileType(const wchar_t* ext,
                                         const GUID& filter) {
  Registry::Key parent, key;

  LONG e = parent.open(HKEY_CLASSES_ROOT, L"Media Type\\Extensions",
                       KEY_ALL_ACCESS);
  if (e == ERROR_FILE_NOT_FOUND)
    return S_FALSE;

  if (e)
    return HRESULT_FROM_WIN32(e);

  e = key.open(parent, ext, KEY_ALL_ACCESS);

  if (e == ERROR_FILE_NOT_FOUND)  // normal
    return S_FALSE;  // not an error if key does not already exist

  if (e)  // indicates a deeper problem
    return HRESULT_FROM_WIN32(e);

  wstring clsidstr;

  e = key.query(L"Source Filter", clsidstr);

  if (e)
    return HRESULT_FROM_WIN32(e);

  const wchar_t* const const_str = clsidstr.c_str();
  wchar_t* const str = const_cast<wchar_t*>(const_str);

  CLSID clsid;

  const HRESULT hr = CLSIDFromString(str, &clsid);
  hr;
  assert(SUCCEEDED(hr));

  if (clsid != filter)
    return S_FALSE;

  key.close();

  e = Registry::DeleteKey(parent, ext);

  if (e)
    return HRESULT_FROM_WIN32(e);

  return S_OK;
}

HRESULT ComReg::RegisterCustomFileType(
    const wchar_t* const* argv,  // array of check-byte strings
    const GUID& filter, const GUID& mediatype, const GUID& subtype) {
  assert(argv);
  assert(*argv);
  assert(filter != GUID_NULL);
  assert(mediatype != GUID_NULL);
  assert(subtype != GUID_NULL);

  Registry::Key parent, key1, key2;

  LONG e = parent.open(HKEY_CLASSES_ROOT, L"Media Type", KEY_CREATE_SUB_KEY);

  if (e)
    return HRESULT_FROM_WIN32(e);

  wchar_t buf[guid_buflen];

  int n = StringFromGUID2(mediatype, buf, guid_buflen);
  assert(n == guid_buflen);

  e = key1.create<wchar_t>(parent, buf, 0, 0, KEY_SET_VALUE, 0);

  if (e)
    return HRESULT_FROM_WIN32(e);

  n = StringFromGUID2(subtype, buf, guid_buflen);
  assert(n == guid_buflen);

  e = key2.create<wchar_t>(key1, buf, 0, 0, KEY_SET_VALUE, 0);

  if (e)
    return HRESULT_FROM_WIN32(e);

  const wchar_t* arg = *argv;
  assert(arg);

  int i = 0;

  for (;;) {
    wostringstream os;
    os << i;

    const wstring name_ = os.str();
    const wchar_t* const name = name_.c_str();

    e = key2.set(name, arg);

    if (e)
      return HRESULT_FROM_WIN32(e);

    arg = *++argv;

    if (arg == 0)
      break;

    ++i;
  }

  n = StringFromGUID2(filter, buf, guid_buflen);
  assert(n == guid_buflen);

  e = key2.set(L"Source Filter", buf);

  if (e)
    return HRESULT_FROM_WIN32(e);

  return S_OK;
}

static HRESULT CreateClsidKey(
    const wstring& clsid_keyname, const wchar_t* friendlyname,
    const wchar_t* inprocserver, const wchar_t* versionindependentprogid,
    const wchar_t* progid, bool insertable, bool control,
    ComReg::ThreadingModel threadingmodel, const GUID& typelib,
    const wchar_t* version, int toolboxbitmap) {
  Registry::Key clsid_key;

  LONG e = clsid_key.create(HKEY_CLASSES_ROOT, clsid_keyname);

  if (e)
    return HRESULT_FROM_WIN32(e);

  if (friendlyname) {
    e = clsid_key.set(friendlyname);

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  if (insertable) {
    Registry::Key subkey;

    e = subkey.create(clsid_key, L"Insertable");

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  if (control) {
    Registry::Key subkey;

    e = subkey.create(clsid_key, L"Control");

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  if (versionindependentprogid) {
    Registry::Key subkey;

    e = subkey.create(clsid_key, L"VersionIndependentProgID");

    if (e)
      return HRESULT_FROM_WIN32(e);

    e = subkey.set(versionindependentprogid);

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  if (progid) {
    Registry::Key subkey;

    e = subkey.create(clsid_key, L"ProgID");

    if (e)
      return HRESULT_FROM_WIN32(e);

    e = subkey.set(progid);

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  if (inprocserver) {
    Registry::Key subkey;

    e = subkey.create(clsid_key, L"InprocServer32");

    if (e)
      return HRESULT_FROM_WIN32(e);

    e = subkey.set(inprocserver);

    if (e)
      return HRESULT_FROM_WIN32(e);

    if (threadingmodel != ComReg::kSingleThreading) {
      const wchar_t* const a[3] = {L"Apartment", L"Free", L"Both"};
      const wchar_t* const val = a[threadingmodel];

      e = subkey.set(L"ThreadingModel", val);

      if (e)
        return HRESULT_FROM_WIN32(e);
    }
  }

  if (typelib != GUID_NULL) {
    wchar_t guid_str[ComReg::guid_buflen];

    const int n = StringFromGUID2(typelib, guid_str, ComReg::guid_buflen);
    assert(n == ComReg::guid_buflen);
    n;

    Registry::Key subkey;

    e = subkey.create(clsid_key, L"TypeLib");

    if (e)
      return HRESULT_FROM_WIN32(e);

    e = subkey.set(guid_str);

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  if (version) {
    Registry::Key subkey;

    e = subkey.create(clsid_key, L"Version");

    if (e)
      return HRESULT_FROM_WIN32(e);

    e = subkey.set(version);

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  if ((toolboxbitmap > 0) && (inprocserver != 0)) {
    Registry::Key subkey;

    e = subkey.create(clsid_key, L"ToolBoxBitmap32");

    if (e)
      return HRESULT_FROM_WIN32(e);

    std::wostringstream os;
    os << inprocserver << L", " << toolboxbitmap;

    e = subkey.set(os.str().c_str());  // TODO: fix registry.h

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  return S_OK;
}

static HRESULT CreateProgidKey(
    const wstring& clsid_str, const wchar_t* friendlyname,
    const wchar_t* progid,
    const wchar_t* curver,  // only for versionindependentprogid
    bool insertable) {
  assert(progid);

  Registry::Key progid_key;

  LONG e = progid_key.create(HKEY_CLASSES_ROOT, progid);

  if (e)
    return HRESULT_FROM_WIN32(e);

  if (friendlyname) {
    e = progid_key.set(friendlyname);

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  Registry::Key subkey;

  e = subkey.create(progid_key, L"CLSID");

  if (e)
    return HRESULT_FROM_WIN32(e);

  e = subkey.set<wchar_t>(0, clsid_str);  // TODO: fix registry.h

  if (insertable) {
    e = subkey.create(progid_key, L"Insertable");

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  if (curver) {
    e = subkey.create(progid_key, L"CurVer");

    if (e)
      return HRESULT_FROM_WIN32(e);

    e = subkey.set(curver);

    if (e)
      return HRESULT_FROM_WIN32(e);
  }

  return S_OK;
}

HRESULT ComReg::RegisterCoclass(const GUID& clsid, const wchar_t* friendlyname,
                                const wchar_t* inprocserver,
                                const wchar_t* versionindependentprogid,
                                const wchar_t* progid, bool insertable,
                                bool control, ThreadingModel threadingmodel,
                                const GUID& typelib, const wchar_t* version,
                                int toolboxbitmap) {
  wchar_t clsid_str[guid_buflen];

  const int n = StringFromGUID2(clsid, clsid_str, guid_buflen);
  assert(n == guid_buflen);
  n;

  const wstring clsid_keyname = wstring(L"CLSID\\") + clsid_str;

  HRESULT hr =
      CreateClsidKey(clsid_keyname, friendlyname, inprocserver,
                     versionindependentprogid, progid, insertable, control,
                     threadingmodel, typelib, version, toolboxbitmap);

  if (hr != S_OK)
    return hr;

  if (versionindependentprogid) {
    hr = CreateProgidKey(clsid_str, friendlyname, versionindependentprogid,
                         progid,
                         insertable);  // TODO: pass false here?

    if (hr != S_OK)
      return hr;
  }

  if (progid) {
    hr = CreateProgidKey(clsid_str, friendlyname, progid,
                         0,  // no CurVer for progid
                         insertable);

    if (hr != S_OK)
      return hr;
  }

  return S_OK;
}

HRESULT ComReg::UnRegisterCoclass(const GUID& clsid) {
  enum { clsid_strlen = 39 };
  wchar_t clsid_str[clsid_strlen];

  const int n = StringFromGUID2(clsid, clsid_str, clsid_strlen);
  assert(n == clsid_strlen);
  n;

  const wstring clsid_keyname = wstring(L"CLSID\\") + clsid_str;

  Registry::Key clsid_key(HKEY_CLASSES_ROOT, clsid_keyname);
  // TODO: KEY_QUERY_VALUE | KEY_SET_VALUE);

  if (!clsid_key.is_open())
    return S_OK;

  Registry::Key subkey(clsid_key, L"VersionIndependentProgID");  // open

  if (subkey.is_open()) {
    wstring val;

    if (subkey(val)) {
      const DWORD dw = Registry::SHDeleteKey(HKEY_CLASSES_ROOT, val);
      assert((dw == ERROR_SUCCESS) || (dw == ERROR_FILE_NOT_FOUND));
      dw;

      // TODO: delete subkey from clsid_key
    }
  }

  subkey.open(clsid_key, L"ProgID");

  if (subkey.is_open()) {
    wstring val;

    if (subkey(val)) {
      const DWORD dw = Registry::SHDeleteKey(HKEY_CLASSES_ROOT, val);
      assert((dw == ERROR_SUCCESS) || (dw == ERROR_FILE_NOT_FOUND));
      dw;

      // TODO: delete subkey
    }
  }

  subkey.close();
  clsid_key.close();

  const DWORD dw = Registry::SHDeleteKey(HKEY_CLASSES_ROOT, clsid_keyname);
  assert((dw == ERROR_SUCCESS) || (dw == ERROR_FILE_NOT_FOUND));
  dw;

  return S_OK;
}

HRESULT ComReg::RegisterTypeLibResource(const wchar_t* fullpath_,
                                        const wchar_t* helpdir_) {
  ITypeLib* pTypeLib;

  HRESULT hr = LoadTypeLib(fullpath_, &pTypeLib);
  // Does not register if filename is a full path, which is what we want.

  if (FAILED(hr))
    return hr;

  assert(pTypeLib);

  wchar_t* const fullpath = const_cast<wchar_t*>(fullpath_);
  wchar_t* const helpdir = const_cast<wchar_t*>(helpdir_);

  hr = RegisterTypeLib(pTypeLib, fullpath, helpdir);

  pTypeLib->Release();
  pTypeLib = 0;

  return hr;
}

HRESULT ComReg::UnRegisterTypeLibResource(const wchar_t* fullpath) {
  ITypeLib* pTypeLib;

  HRESULT hr = LoadTypeLib(fullpath, &pTypeLib);
  // Does not register if filename is a full path, which is what we want.

  if (FAILED(hr))
    return hr;

  assert(pTypeLib);

  TLIBATTR* pLibAttr;

  hr = pTypeLib->GetLibAttr(&pLibAttr);

  if (FAILED(hr)) {
    pTypeLib->Release();
    pTypeLib = 0;

    return hr;
  }

  assert(pLibAttr);
  const TLIBATTR a(*pLibAttr);

  pTypeLib->ReleaseTLibAttr(pLibAttr);

  pTypeLib->Release();
  pTypeLib = 0;

  hr = UnRegisterTypeLib(a.guid, a.wMajorVerNum, a.wMinorVerNum, a.lcid,
                         a.syskind);

  // TYPE_E_REGISTRYACCESS is returned when the type lib was not registered.
  // Since DShow filters are supposed to call DllUnregisterServer() from
  // DllRegisterServer(), and DllUnregisterServer() must (attempt to) unregister
  // type libraries, suppress this error because it's going to be returned every
  // time a filter is registered for the first time on a system.
  if (hr == TYPE_E_REGISTRYACCESS)
    hr = S_FALSE;

  return hr;
}

HRESULT ComReg::GetTypeLibAttr(const wchar_t* fullpath, TLIBATTR& a) {
  ITypeLib* pTypeLib;

  HRESULT hr = LoadTypeLib(fullpath, &pTypeLib);
  // Does not register if filename is a full path, which is what we want.

  if (FAILED(hr))
    return hr;

  assert(pTypeLib);

  TLIBATTR* pLibAttr;

  hr = pTypeLib->GetLibAttr(&pLibAttr);

  if (FAILED(hr)) {
    pTypeLib->Release();
    pTypeLib = 0;

    return hr;
  }

  assert(pLibAttr);
  a = *pLibAttr;

  pTypeLib->ReleaseTLibAttr(pLibAttr);

  pTypeLib->Release();
  pTypeLib = 0;

  return S_OK;
}

#if (_WIN32_WINNT >= 0x0601)
HRESULT ComReg::RegisterByteStreamHandler(const wchar_t* ext, const GUID& clsid,
                                          const wchar_t* friendly_name) {
  const wchar_t parent_subkey[] =
      L"Software\\Microsoft\\Windows Media Foundation\\ByteStreamHandlers";
  const wstring subkey = wstring(parent_subkey) + L"\\" + ext;

  Registry::Key key;

  LONG e = key.create<wchar_t>(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, 0,
                               KEY_WRITE, 0);

  if (e)
    return HRESULT_FROM_WIN32(e);

  wchar_t buf[guid_buflen];
  const int n = StringFromGUID2(clsid, buf, guid_buflen);
  n;
  assert(n == guid_buflen);

  e = key.set(buf, friendly_name);

  if (e)
    return HRESULT_FROM_WIN32(e);

  return S_OK;
}

HRESULT ComReg::UnRegisterByteStreamHandler(const wchar_t* ext,
                                            const GUID& clsid) {
  Registry::Key parent, key;
  LONG e = parent.open(HKEY_LOCAL_MACHINE,
                       L"Software\\Microsoft"
                       L"\\Windows Media Foundation\\ByteStreamHandlers",
                       KEY_ALL_ACCESS);

  if (e == ERROR_FILE_NOT_FOUND)  // weird
    return S_FALSE;

  if (e)
    return HRESULT_FROM_WIN32(e);

  e = key.open(parent, ext, KEY_ALL_ACCESS);

  if (e == ERROR_FILE_NOT_FOUND)  // normal
    return S_FALSE;  // not an error if key does not already exist

  if (e)  // indicates a deeper problem
    return HRESULT_FROM_WIN32(e);

  wchar_t clsidstr[guid_buflen];  // name

  const int n = StringFromGUID2(clsid, clsidstr, guid_buflen);
  n;
  assert(n == guid_buflen);

  wstring value;

  e = key.query(clsidstr, value);

  if (e == ERROR_FILE_NOT_FOUND)
    return S_FALSE;

  if (e)
    return HRESULT_FROM_WIN32(e);

  key.close();

  e = Registry::DeleteKey(parent, ext);

  if (e)
    return HRESULT_FROM_WIN32(e);

  return S_OK;
}
#endif

HRESULT ComReg::RegisterProtocolSource(const wchar_t* protocol,
                                       const wchar_t* ext, const GUID& filter) {
  if (protocol == 0)
    return E_INVALIDARG;

  if (ext == 0)
    return E_INVALIDARG;

  if (filter == GUID_NULL)
    return E_INVALIDARG;

  Registry::Key pk;  // protocol

  LONG e = pk.open(HKEY_CLASSES_ROOT, protocol, KEY_READ | KEY_WRITE);

  if (e)
    return HRESULT_FROM_WIN32(e);

  Registry::Key ek;  // extensions

  e = ek.open(pk, L"Extensions", KEY_READ | KEY_WRITE);

  if (e)
    return HRESULT_FROM_WIN32(e);

  wchar_t buf[guid_buflen];

  const int n = StringFromGUID2(filter, buf, guid_buflen);
  assert(n == guid_buflen);
  n;

  e = ek.set(ext, buf);

  if (e)
    return HRESULT_FROM_WIN32(e);

  return S_OK;
}

HRESULT ComReg::UnRegisterProtocolSource(const wchar_t* protocol,
                                         const wchar_t* ext,
                                         const GUID& filter) {
  if (protocol == 0)
    return E_INVALIDARG;

  if (ext == 0)
    return E_INVALIDARG;

  if (filter == GUID_NULL)
    return E_INVALIDARG;

  Registry::Key pk;  // protocol

  LONG e = pk.open(HKEY_CLASSES_ROOT, protocol, KEY_READ | KEY_WRITE);

  if (e == ERROR_FILE_NOT_FOUND)
    return S_FALSE;

  if (e)
    return HRESULT_FROM_WIN32(e);

  Registry::Key ek;  // extensions

  e = ek.open(pk, L"Extensions", KEY_READ | KEY_WRITE);

  if (e == ERROR_FILE_NOT_FOUND)
    return S_FALSE;

  if (e)
    return HRESULT_FROM_WIN32(e);

  wstring val;

  e = ek.query(ext, val);

  if (e == ERROR_FILE_NOT_FOUND)
    return S_FALSE;

  if (e)
    return HRESULT_FROM_WIN32(e);

  GUID guid;

  const HRESULT hr = ::CLSIDFromString(val.c_str(), &guid);

  if (FAILED(hr))
    return S_FALSE;

  if (guid != filter)
    return S_FALSE;

  e = ::RegDeleteValue(ek, ext);

  if (e)
    return HRESULT_FROM_WIN32(e);

  return S_OK;
}
