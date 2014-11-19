// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <windowsx.h>
#include <comdef.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mmreg.h>

#include <cassert>

#include "clockable.h"
#include "debugutil.h"
#include "eventutil.h"
#include "gtest/gtest.h"
#include "threadutil.h"
#include "webmdsound.h"

extern wchar_t* g_test_input_file;

using WebmDirectX::AudioPlaybackDevice;

void init_wfextensible(WORD format_tag, WORD channels, DWORD sample_rate,
                       WORD bits_per_sample, WORD reserved, DWORD channel_mask,
                       GUID sub_fmt, WAVEFORMATEXTENSIBLE* ptr_ext_wav)
{
    if (!ptr_ext_wav)
        return;
    WAVEFORMATEX* ptr_wfx = &ptr_ext_wav->Format;
    ptr_wfx->wFormatTag = format_tag;
    ptr_wfx->nChannels = channels;
    ptr_wfx->nSamplesPerSec = sample_rate;
    ptr_wfx->wBitsPerSample = bits_per_sample;
    if (WAVE_FORMAT_PCM  == format_tag || WAVE_FORMAT_IEEE_FLOAT == format_tag)
    {
        ptr_wfx->nBlockAlign =
            (ptr_wfx->nChannels * ptr_wfx->wBitsPerSample + 7) / 8;
        ptr_wfx->nAvgBytesPerSec =
            ptr_wfx->nSamplesPerSec * ptr_wfx->nBlockAlign;
    }
    ptr_ext_wav->Samples.wReserved = reserved;
    ptr_ext_wav->dwChannelMask = channel_mask;
    ptr_ext_wav->SubFormat = sub_fmt;
}

TEST(WebmDirectSound, AudioPlaybackDevice_InitPCM)
{
    AudioPlaybackDevice apd;
    WAVEFORMATEXTENSIBLE wfx = {0};
    init_wfextensible(WAVE_FORMAT_PCM, 1, 44100, 16, 0, 0, GUID_NULL, &wfx);
    ASSERT_EQ(S_OK, apd.Open(NULL, &wfx));
}

TEST(WebmDirectSound, AudioPlaybackDevice_InitIEEEFloat)
{
    AudioPlaybackDevice apd;
    WAVEFORMATEXTENSIBLE wfx = {0};
    init_wfextensible(WAVE_FORMAT_IEEE_FLOAT, 1, 44100, sizeof(float)*8, 0, 0,
                      GUID_NULL, &wfx);
    ASSERT_EQ(S_OK, apd.Open(NULL, &wfx));
}
