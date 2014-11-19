// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <windowsx.h>

#include <cassert>

#include "debugutil.hpp"
#include "consoleutil.hpp"
#include "gtest/gtest.h"
#include "windowutil.hpp"

void OnClose(HWND hwnd)
{
    hwnd;
    PostQuitMessage(0);
}

void OnCreate(HWND hwnd)
{
    hwnd;
}

void OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = 0;

    hdc = BeginPaint(hwnd, &ps);

    EndPaint(hwnd, &ps);
}

void OnSize(HWND hwnd, UINT state, int /*cx*/, int /*cy*/)
{
    hwnd; state;
}

void OnKeyDown(HWND hwnd, UINT vk, BOOL /*fDown*/, int /*cRepeat*/,
               UINT /*flags*/)
{
    switch (vk)
    {
    case VK_SPACE:
        break;
    }
    hwnd;
}

LRESULT CALLBACK window_handler(HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_CLOSE,   OnClose);
        HANDLE_MSG(hwnd, WM_KEYDOWN, OnKeyDown);
        HANDLE_MSG(hwnd, WM_PAINT,   OnPaint);
        HANDLE_MSG(hwnd, WM_SIZE,    OnSize);

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

HINSTANCE g_instance;

int sdlplay_main(int argc, WCHAR* argv[])
{
    if (argc < 1)
    {
        DBGLOG(L"No file specified" << E_FAIL);
        return EXIT_FAILURE;
    }

    const DWORD co_init_flags =
        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE;

    HRESULT hr = CoInitializeEx(NULL, co_init_flags);
    if (FAILED(hr))
    {
        DBGLOG(L"COM init failed" << E_FAIL);
        return EXIT_FAILURE;
    }

    std::wstring url_str = argv[0];

    WebmMfUtil::WebmMfWindow mfwindow(window_handler);
    hr = mfwindow.Create(g_instance);
    assert(SUCCEEDED(hr));

    if (FAILED(hr))
    {
        DBGLOG(L"mfwindow.Create failed!" << hr);
        return EXIT_FAILURE;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();

    return EXIT_SUCCESS;
}

wchar_t* g_test_input_file = NULL;
int test_main(int argc, WCHAR* argv[])
{
    WebmMfUtil::ConsoleWindow console_window;
    console_window.Create();
    g_test_input_file = argv[0];
    testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS();
    return 0;
}

INT WINAPI wWinMain(HINSTANCE,HINSTANCE instance, LPWSTR ptr_cmdline,INT)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(ptr_cmdline, &argc);
    g_instance = instance;
    test_main(argc, argv);
    int result = sdlplay_main(argc, argv);
    LocalFree(argv);
    return result;
}
