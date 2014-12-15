// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <cassert>

#include "playwebmapp.h"

static HANDLE s_hQuit;

static BOOL __stdcall ConsoleCtrlHandler(DWORD type) {
  if (type != CTRL_C_EVENT)
    return FALSE;  // no, not handled here

  const BOOL b = SetEvent(s_hQuit);
  assert(b);
  b;

  return TRUE;  // yes, handled here
}

static int CoMain(int argc, wchar_t* argv[]) {
  App app(s_hQuit);
  return app(argc, argv);
}

int wmain(int argc, wchar_t* argv[]) {
  s_hQuit = CreateEvent(0, 0, 0, 0);
  assert(s_hQuit);

  const BOOL b = SetConsoleCtrlHandler(&ConsoleCtrlHandler, TRUE);
  assert(b);
  b;

  const HRESULT hr = CoInitialize(0);

  if (FAILED(hr))
    return 1;

  const int status = CoMain(argc, argv);

  CoUninitialize();

  return status;
}
