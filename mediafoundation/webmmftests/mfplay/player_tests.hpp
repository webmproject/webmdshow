// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_PLAYER_TESTS_HPP__
#define __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_PLAYER_TESTS_HPP__

const static UINT WM_APP_PLAYER_COMMAND = WM_APP + 2;

enum PlayerCommand
{
    PLAYER_CMD_BASE = WM_APP_PLAYER_COMMAND,
    PLAYER_CMD_OPEN,
    PLAYER_CMD_PLAY,
};

struct PlayerCommandMessage
{
    PlayerCommand command;
    const void* ptr_command_data;
};

class PlayerController
{
public:
    PlayerController();
    ~PlayerController();

    HRESULT Create(HWND hwnd_player);
    HRESULT Destroy();
    HRESULT Play(std::wstring file_to_play);
    HRESULT Pause();
    HRESULT Stop();
private:
    static void OnPlayerStateChange(UINT_PTR ptr_this, int state);
    HWND hwnd_player_;
    int waiting_for_state_;
    WebmMfUtil::MfPlayerCallback* ptr_player_callback_;
    WebmMfUtil::EventWaiter player_state_change_waiter_;
};

void start_webmmf_test_thread(HWND hwnd_player);

DWORD WINAPI test_thread(LPVOID ptr_thread_data);

CPlayer* get_player(HWND hwnd);

#endif // __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_PLAYER_TESTS_HPP__
