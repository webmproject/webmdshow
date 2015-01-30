// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

//#pragma warning(disable:4514)  //unref'd inline
//#pragma warning(disable:4710)  //not inlined
//#pragma warning(push, 3)
#include <windows.h>
#include "versionhandling.h"
#include <cassert>
#include <cstdlib>  //_wpgmptr, _get_wpgmptr
#include <malloc.h>   //_alloca
#include <ostream>
//#pragma warning(pop)

#pragma comment(lib, "version.lib")

void VersionHandling::GetVersion(
    const wchar_t* fname,
    WORD& major,
    WORD& minor,
    WORD& revision,
    WORD& build)
{
#if 0
    char fname[_MAX_PATH];

    const DWORD gmfnStatus =
        GetModuleFileName(0, fname, sizeof fname);

    assert(gmfnStatus);
#elif 0
    //_wpgmptr
    wchar_t* fname;

    const errno_t e = _get_wpgmptr(&fname);
    assert(e == 0);
#endif

    DWORD handle;

    const DWORD size = GetFileVersionInfoSize(fname, &handle);
    assert(size);

    BYTE* const buf = (BYTE*)_malloca(size);

    BOOL b = GetFileVersionInfo(fname, handle, size, buf);
    assert(b);

    VS_FIXEDFILEINFO* p;
    UINT len;

    b = VerQueryValue(buf, L"\\", (LPVOID*)&p, &len);
    assert(b);
    assert(p);
    assert(len);

    major = WORD(p->dwProductVersionMS >> 16);
    minor = WORD(p->dwProductVersionMS);

    revision = WORD(p->dwProductVersionLS >> 16);
    build = WORD(p->dwProductVersionLS);
}


void VersionHandling::GetVersion(const wchar_t* fname, std::wostream& os)
{
    WORD major, minor, revision, build;
    VersionHandling::GetVersion(fname, major, minor, revision, build);

    os << major
       << L'.'
       << minor
       << L'.'
       << revision
       << L'.'
       << build;
}



#if 0
const std::string VersionHandling::version(bool full)
{
    DWORD ms, ls;

    version(ms, ls);

    const DWORD ms_major = ms >> 16;

    char buf[33];

    _ultoa(ms_major, buf, 10);

    string value = buf;

    const DWORD ms_minor = ms & 0x0000FFFF;

    _ultoa(ms_minor, buf, 10);

    value += '.';
    value += buf;

    if (full)
    {
        const DWORD ls_major = ls >> 16;

        _ultoa(ls_major, buf, 10);

        value += '.';
        value += buf;

        const DWORD ls_minor = ls & 0x0000FFFF;

        _ultoa(ls_minor, buf, 10);

        value += '.';
        value += buf;
    }

    return value;
}
#endif
