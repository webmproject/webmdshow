// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <string>
#include "iidstr.h"
#define NO_SHLWAPI_REG
#include "registry.h"
#include <strmif.h>
#include <malloc.h>
#include <ostream>

using std::wstring;

std::wostream& operator<<(std::wostream& os, const IIDStr& iidstr)
{
    wchar_t guidstr[CHARS_IN_GUID];

    StringFromGUID2(iidstr.m_iid, guidstr, CHARS_IN_GUID);

    const wstring subkey = wstring(L"Interface\\") + guidstr;

    const Registry::Key key(HKEY_CLASSES_ROOT, subkey);

    if (!key.is_open())
        return os << guidstr;

    wstring buf;

    if (key(buf))
        return os << buf;

    return os << guidstr;
}

