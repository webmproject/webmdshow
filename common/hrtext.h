// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once

#ifndef __ERRORS__
#include <errors.h>
#pragma comment(lib, "Quartz.lib")
#endif

struct hrtext
{
    const HRESULT hr;

    hrtext(HRESULT hr_) : hr(hr_) {}

private:

    hrtext& operator=(const hrtext&);

};


inline std::ostream& operator<<(std::ostream& os, const hrtext& val)
{
    char buf[MAX_ERROR_TEXT_LEN];

    const DWORD n = AMGetErrorTextA(val.hr, buf, MAX_ERROR_TEXT_LEN);

    if (n == 0)
        return os << "<notext>";

    char* str = buf + n;

    for (;;)
    {
        if (*str >= ' ')
            break;

        *str = '\0';

        if (str == buf)
            break;

        --str;
    }

    return os << buf;
}


inline std::wostream& operator<<(std::wostream& os, const hrtext& val)
{
    wchar_t buf[MAX_ERROR_TEXT_LEN];

    const DWORD n = AMGetErrorTextW(val.hr, buf, MAX_ERROR_TEXT_LEN);

    if (n == 0)
        return os << L"<notext>";

    wchar_t* str = buf + n;

    for (;;)
    {
        if (*str >= L' ')
            break;

        *str = L'\0';

        if (str == buf)
            break;

        --str;
    }

    return os << buf;
}
