// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_CONSOLEUTIL_HPP__
#define __WEBMDSHOW_COMMON_CONSOLEUTIL_HPP__

namespace WebmMfUtil
{

class ConsoleWindow
{
public:
    ConsoleWindow();
    ~ConsoleWindow();
    HRESULT Create();

private:
    int stderr_handle_;
    int stdout_handle_;

    DISALLOW_COPY_AND_ASSIGN(ConsoleWindow);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_CONSOLEUTIL_HPP__
