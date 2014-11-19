// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "makewebmapp.hpp"
#include <cassert>

static int CoMain(int, wchar_t*[]);

HANDLE g_hQuit;

static BOOL __stdcall ConsoleCtrlHandler(DWORD type)
{
    if (type != CTRL_C_EVENT)
        return FALSE;  //no, not handled here

    const BOOL b = SetEvent(g_hQuit);
    b;
    assert(b);

    return TRUE;  //yes, handled here
}


int wmain(int argc, wchar_t* argv[])
{
    g_hQuit = CreateEvent(0, 1, 0, 0);  //manual-reset event, non-signalled
    assert(g_hQuit);

    const BOOL b = SetConsoleCtrlHandler(&ConsoleCtrlHandler, TRUE);
    assert(b); b;

    const HRESULT hr = CoInitialize(0);

    if (FAILED(hr))
        return 1;

    const int status = CoMain(argc, argv);

    CoUninitialize();

    return status;
}


static int CoMain(int argc, wchar_t* argv[])
{
    App app;
    return app(argc, argv);
}
