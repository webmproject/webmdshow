// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <windowsx.h>

#include <string>

#include "debugutil.hpp"
#include "eventutil.hpp"
#include "gtest/gtest.h"
#include "mfplayerutil.hpp"
#include "player.hpp"
#include "player_tests.hpp"
#include "threadutil.hpp"

// TODO(tomfinegan): destroy player between tests

namespace
{
    // TODO(tomfinegan): get rid of this global window handle... all it will
    //                   take to get rid of it is learning how to pass data to
    //                   tests.
    HWND g_hwnd_player = NULL;
}

CPlayer* get_player(HWND hwnd)
{
    CPlayer* ptr_player = NULL;
    LONG_PTR ptr_userdata = GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (ptr_userdata)
    {
        ptr_player = reinterpret_cast<CPlayer*>(ptr_userdata);
    }
    return ptr_player;
}

HRESULT destroy_player(HWND hwnd)
{
    if (!hwnd)
        return E_INVALIDARG;

    CPlayer* ptr_player = get_player(hwnd);

    HRESULT hr = E_FAIL;
    if (ptr_player)
    {
        ptr_player->Shutdown();
        ptr_player->Release();
        hr = S_OK;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, NULL);
    }
    return hr;
}

void start_webmmf_test_thread(HWND hwnd_player)
{
    g_hwnd_player = hwnd_player;
    WebmMfUtil::SimpleThread simple_test_thread;
    LPVOID ptr_player_hwnd = reinterpret_cast<LPVOID>(hwnd_player);
    simple_test_thread.Run(test_thread, ptr_player_hwnd);
}

DWORD WINAPI test_thread(LPVOID ptr_thread_data)
{
    ptr_thread_data;
    int argc = 1;
    wchar_t* argv = L"mfplay_test_thread";

    // TODO(tomfinegan): open a console window for test output

    testing::InitGoogleTest(&argc, &argv);
    RUN_ALL_TESTS();
    return 0;
}

PlayerController::PlayerController():
  hwnd_player_(NULL),
  waiting_for_state_(0)
{

}

PlayerController::~PlayerController()
{
}

HRESULT PlayerController::Create(HWND hwnd_player)
{
    if (!hwnd_player)
        return E_INVALIDARG;

    hwnd_player_ = hwnd_player;
    return S_OK;
}

HRESULT PlayerController::Play(std::wstring file_to_play)
{

    CPlayer* player = get_player(hwnd_player_);

    if (!player || file_to_play.length() == 0)
        return E_INVALIDARG;

    using WebmMfUtil::MfPlayerCallback;
    HRESULT hr;
    hr = MfPlayerCallback::CreateInstance(OnPlayerStateChange,
                                          reinterpret_cast<UINT_PTR>(this),
                                          &ptr_player_callback_);
    if (FAILED(hr) || ptr_player_callback_ == NULL)
        return E_OUTOFMEMORY;

    hr = player_state_change_waiter_.Create();
    if (FAILED(hr))
        return E_OUTOFMEMORY;


    // TODO(tomfinegan): make player control thread safe-- control CPlayer
    //                   with PostMessage instead of doing evil things with
    //                   a global pointer to an object created in another
    //                   thread that's doing things asynchronously.

    player->SetStateCallback(ptr_player_callback_);

    waiting_for_state_ = OpenPending;

    hr = player->OpenURL(file_to_play.c_str());

    if (SUCCEEDED(hr))
    {
        DBGLOG("Waiting for OpenPending");
        hr = player_state_change_waiter_.Wait();
        DBGLOG("OpenPending wait result = " << hr);
    }

    if (SUCCEEDED(hr))
    {
        waiting_for_state_ = Started;
        DBGLOG("Waiting for Started");
        hr = player_state_change_waiter_.Wait();
        DBGLOG("Started wait result = " << hr);
    }

    return hr;
}

void PlayerController::OnPlayerStateChange(UINT_PTR ptr_this, int state)
{
    ASSERT_NE(ptr_this, 0UL);
    PlayerController* ptr_pc = reinterpret_cast<PlayerController*>(ptr_this);

    if (state == ptr_pc->waiting_for_state_)
    {
        ptr_pc->player_state_change_waiter_.Set();
    }
}


TEST(WebmMfPlayTest, PlayFile)
{
    PlayerController player_controller;
    player_controller.Create(g_hwnd_player);
    std::wstring file = L"D:\\src\\media\\webm\\fg.webm";

    HRESULT hr_play = player_controller.Play(file);
    ASSERT_TRUE(SUCCEEDED(hr_play));
}