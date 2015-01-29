// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.


#include <windows.h>
#include <windowsx.h>
#include <io.h>

#include <iostream>

#include "debugutil.h"
#include "consoleutil.h"

namespace WebmMfUtil
{

ConsoleWindow::ConsoleWindow():
  stderr_handle_(-1),
  stdout_handle_(-1)
{
}

ConsoleWindow::~ConsoleWindow()
{
    if (-1 != stderr_handle_)
        _close(stderr_handle_);
    if (-1 != stdout_handle_)
        _close(stdout_handle_);
    FreeConsole();
}

HRESULT ConsoleWindow::Create()
{
    // TODO(tomfinegan): do nothing when running within a console
    AllocConsole();
    stderr_handle_ =
        _open_osfhandle((intptr_t)GetStdHandle(STD_ERROR_HANDLE), 0);
    stdout_handle_ =
        _open_osfhandle((intptr_t)GetStdHandle(STD_OUTPUT_HANDLE), 0);

    stderr->_file = stderr_handle_;
    stdout->_file = stdout_handle_;

    HRESULT hr = E_OUTOFMEMORY;

    if (-1 != stderr_handle_ && -1 != stdout_handle_)
        hr = S_OK;

    return hr;
}

} // WebmMfUtil namespace
