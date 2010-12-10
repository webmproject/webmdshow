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

// TODO(tomfinegan): this is just pure nasty... fix me!
extern CPlayer* g_pPlayer;

// TODO(tomfinegan): destroy player between tests

void start_test_thread()
{
    WebmMfUtil::SimpleThread simple_test_thread;
    simple_test_thread.Run(test_thread, NULL);
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
  ptr_player_(NULL),
  waiting_for_state_(0)
{

}

PlayerController::~PlayerController()
{
}

HRESULT PlayerController::Play(CPlayer* player, std::wstring file_to_play)
{

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

    ptr_player_ = player;
    ptr_player_->SetStateCallback(ptr_player_callback_);

    waiting_for_state_ = OpenPending;

    hr = ptr_player_->OpenURL(file_to_play.c_str());

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
    std::wstring file = L"D:\\src\\media\\webm\\fg.webm";

    HRESULT hr_play = player_controller.Play(g_pPlayer, file);
    ASSERT_TRUE(SUCCEEDED(hr_play));
}