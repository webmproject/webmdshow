// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_PLAYER_TESTS_HPP__
#define __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_PLAYER_TESTS_HPP__

class PlayerController
{
public:
    PlayerController();
    ~PlayerController();

    HRESULT Play(CPlayer* player, std::wstring file_to_play);
    HRESULT Pause();
    HRESULT Stop();
private:
    static void OnPlayerStateChange(UINT_PTR ptr_this, int state);
    CPlayer* ptr_player_;
    int waiting_for_state_;
    WebmMfUtil::MfPlayerCallback* ptr_player_callback_;
    WebmMfUtil::EventWaiter player_state_change_waiter_;

};

#endif // __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_PLAYER_TESTS_HPP__
