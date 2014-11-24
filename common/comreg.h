// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMDSHOW_COMMON_COMREG_H_
#define WEBMDSHOW_COMMON_COMREG_H_

#include <oaidl.h>
#include <strmif.h>

#include <string>

namespace ComReg {
  enum { guid_buflen = CHARS_IN_GUID };  // includes braces and terminating NUL

GUID GUIDFromString(const wchar_t*);

HRESULT ComRegGetModuleFileName(HMODULE, std::wstring&);

enum ThreadingModel {
  kSingleThreading = -1,  // no threading model
  kApartment = 0,
  kFree,
  kBoth
};

HRESULT RegisterCoclass(const GUID& clsid, const wchar_t* friendlyname,
                        const wchar_t* inprocserver,
                        const wchar_t* versionindependentprogid,
                        const wchar_t* progid, bool insertable,
                        bool control,  // TODO: add category support
                        ThreadingModel, const GUID& typelib,
                        const wchar_t* version, int toolboxbitmap32);

HRESULT UnRegisterCoclass(const GUID&);

HRESULT RegisterTypeLibResource(const wchar_t* fullpath,
                                const wchar_t* helpdir);

HRESULT UnRegisterTypeLibResource(const wchar_t* fullpath);

HRESULT GetTypeLibAttr(const wchar_t*, TLIBATTR&);

// DirectShow
HRESULT RegisterCustomFileType(const wchar_t* ext, const GUID& filter,
                               const GUID& mediatype, const GUID& subtype);

HRESULT UnRegisterCustomFileType(const wchar_t* ext, const GUID& filter);

HRESULT RegisterCustomFileType(
    const wchar_t* const* argv,  // array of check-byte strings
    const GUID& filter, const GUID& mediatype, const GUID& subtype);

HRESULT RegisterProtocolSource(const wchar_t* protocol, const wchar_t* ext,
                               const GUID& filter);

HRESULT UnRegisterProtocolSource(const wchar_t* protocol, const wchar_t* ext,
                                 const GUID& filter);

// Media Foundation
#if (_WIN32_WINNT >= 0x0601)
HRESULT RegisterByteStreamHandler(const wchar_t* ext, const GUID& clsid,
                                  const wchar_t* friendly_name);

HRESULT UnRegisterByteStreamHandler(const wchar_t* ext, const GUID& clsid);
#endif
}

#endif  // WEBMDSHOW_COMMON_COMREG_H_
