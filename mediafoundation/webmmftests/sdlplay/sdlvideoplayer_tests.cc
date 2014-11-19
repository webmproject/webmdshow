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
#include <cmath>
#include <queue>
#include <vector>

#include "debugutil.h"
#include "gtest/gtest.h"
#include "SDLVideoPlayer.h"

TEST(SDLVideoPlayer, Init)
{
    SDLVideoPlayer sdl_player;
    ASSERT_EQ(0, sdl_player.Init());
}
