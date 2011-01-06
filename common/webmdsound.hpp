// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_WEBDSOUND_HPP__
#define __WEBMDSHOW_COMMON_WEBDSOUND_HPP__

#include <dsound.h>

namespace WebmDirectSound
{

class AudioPlaybackDevice
{
public:
    AudioPlaybackDevice();
    ~AudioPlaybackDevice();
    HRESULT Open(HWND hwnd, const WAVEFORMATEXTENSIBLE* const ptr_wfx);
private:
    HRESULT CreateBuffer_(const WAVEFORMATEXTENSIBLE* const ptr_wfx);
    IDirectSound8* ptr_dsound_;
    IDirectSoundBuffer8* ptr_dsound_buf_;

    HWND hwnd_;
    DISALLOW_COPY_AND_ASSIGN(AudioPlaybackDevice);
};

} // WebmDirectSound

#endif // __WEBMDSHOW_COMMON_WEBDSOUND_HPP__
