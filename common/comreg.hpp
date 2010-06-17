// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
//#ifndef _WINDOWS_
//#include <windows.h>
//#endif
#include <oaidl.h>
#include <string>

namespace ComReg
{
    enum { guid_buflen = 39 };  //includes braces and terminating NUL
    //CHARS_IN_GUID  //see strmif.h

    GUID GUIDFromString(const wchar_t*);

    HRESULT ComRegGetModuleFileName(HMODULE, std::wstring&);

    enum ThreadingModel
    {
        kSingleThreading = -1, //no threading model
        kApartment = 0,
        kFree,
        kBoth
    };

    HRESULT RegisterCoclass(
                const GUID& clsid,
                const wchar_t* friendlyname,
                const wchar_t* inprocserver,
                const wchar_t* versionindependentprogid,
                const wchar_t* progid,
                bool insertable,
                bool control,   //TODO: add category support
                ThreadingModel,
                const GUID& typelib,
                const wchar_t* version,
                int toolboxbitmap32);

    HRESULT UnRegisterCoclass(const GUID&);

    HRESULT RegisterTypeLibResource(
                const wchar_t* fullpath,
                const wchar_t* helpdir);

    HRESULT UnRegisterTypeLibResource(const wchar_t* fullpath);

    HRESULT GetTypeLibAttr(const wchar_t*, TLIBATTR&);

    HRESULT RegisterCustomFileType(
                const wchar_t* ext,
                const GUID& filter,
                const GUID& mediatype,
                const GUID& subtype);

    HRESULT UnRegisterCustomFileType(
                const wchar_t* ext,
                const GUID& filter);

    HRESULT RegisterCustomFileType(
                const wchar_t* const* argv,  //array of check-byte strings
                const GUID& filter,
                const GUID& mediatype,
                const GUID& subtype);
}
