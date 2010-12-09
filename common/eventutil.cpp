// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <windowsx.h>

#include "debugutil.hpp"
#include "eventutil.hpp"

namespace WebmMfUtil
{

EventWaiter::EventWaiter() :
  event_handle_(INVALID_HANDLE_VALUE)
{
}

EventWaiter::~EventWaiter()
{
    if (INVALID_HANDLE_VALUE != event_handle_)
    {
        CloseHandle(event_handle_);
        event_handle_ = INVALID_HANDLE_VALUE;
    }
}

HRESULT EventWaiter::Create()
{
    event_handle_ = CreateEvent(NULL, FALSE, FALSE, L"mfplayer_event");
    if (INVALID_HANDLE_VALUE == event_handle_)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

HRESULT EventWaiter::Wait()
{
    DWORD wr = MsgWaitForMultipleObjects(1, &event_handle_, TRUE, INFINITE,
                                         QS_ALLEVENTS);
    HRESULT hr = S_OK;
    if (wr != WAIT_OBJECT_0)
    {
        DBGLOG(L"event wait failed" << hr);
        hr = E_FAIL;
    }
    return hr;
}

HRESULT EventWaiter::Set()
{
    HRESULT hr = S_OK;
    if (!SetEvent(event_handle_))
    {
        hr = E_FAIL;
    }
    return hr;
}

} // WebmMfUtil namespace
