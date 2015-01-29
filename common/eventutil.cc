// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <windowsx.h>

#include "debugutil.h"
#include "eventutil.h"

namespace WebmMfUtil
{

DWORD kTIMEOUT = 500;

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
    return infinite_cowait(event_handle_);
}

HRESULT EventWaiter::ZeroWait()
{
    return zero_cowait(event_handle_);
}

HRESULT EventWaiter::MessageWait()
{
    HRESULT hr = E_FAIL;
    for (;;)
    {
        MSG msg;
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        DWORD wr = MsgWaitForMultipleObjects(1, &event_handle_, TRUE, kTIMEOUT,
                                             QS_ALLEVENTS);
        if (wr == WAIT_OBJECT_0)
        {
            hr = S_OK;
            break;
        }
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

HRESULT infinite_cowait(HANDLE hndl)
{
    DWORD wr;
    HRESULT hr = CoWaitForMultipleHandles(COWAIT_WAITALL, INFINITE, 1,
                                          &hndl, &wr);
    if (wr != WAIT_OBJECT_0 || FAILED(hr))
    {
        DBGLOG(L"event wait failed" << hr);
        hr = E_FAIL;
    }
    return hr;
}

HRESULT zero_cowait(HANDLE hndl)
{
    DWORD wr;
    HRESULT hr = CoWaitForMultipleHandles(COWAIT_WAITALL, 0, 1, &hndl, &wr);
    if (SUCCEEDED(hr) && WAIT_OBJECT_0 == wr)
    {
        hr = S_OK;
    }
    else
    {
        hr = E_FAIL;
    }
    return hr;
}

} // WebmMfUtil namespace
