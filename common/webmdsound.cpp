// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <mmreg.h>

#include "debugutil.hpp"
#include "webmdsound.hpp"

namespace WebmDirectSound
{

template <typename COMOBJ>
ULONG safe_rel(COMOBJ*& comobj)
{
    ULONG refcnt = 0;
    if (comobj)
    {
        refcnt = comobj->Release();
        comobj = NULL;
    }
    return refcnt;
}

HRESULT AudioPlaybackDevice::Open(HWND hwnd,
                                  const WAVEFORMATEXTENSIBLE* const ptr_wfx)
{
    HRESULT hr;
    CHK(hr, DirectSoundCreate8(NULL /* same as DSDEVID_DefaultPlayback */,
                               &ptr_dsound_, NULL));
    if (FAILED(hr))
    {
        return hr;
    }
    if (!hwnd)
    {
        HWND desktop_hwnd = GetDesktopWindow();
        // TODO(tomfinegan): using |desktop_hwnd| is wrong, we need our own
        //                   window here.  Using the desktop window means that
        //                   users are stuck hearing our audio when the desktop
        //                   window is active, and might not be able to hear
        //                   anything when our own window is active.
        hwnd_ = desktop_hwnd;
    }

    CHK(hr, ptr_dsound_->SetCooperativeLevel(hwnd_, DSSCL_PRIORITY));
    if (FAILED(hr))
    {
        return hr;
    }
    CHK(hr, CreateBuffer_(ptr_wfx));
    if (FAILED(hr))
    {
        return hr;
    }
    return hr;
}

AudioPlaybackDevice::AudioPlaybackDevice():
  hwnd_(NULL),
  ptr_dsound_(NULL),
  ptr_dsound_buf_(NULL)
{
}

AudioPlaybackDevice::~AudioPlaybackDevice()
{
    safe_rel(ptr_dsound_);
    safe_rel(ptr_dsound_buf_);
}

HRESULT AudioPlaybackDevice::CreateBuffer_(
    const WAVEFORMATEXTENSIBLE* const ptr_wfx)
{
    if (!ptr_wfx)
    {
        DBGLOG("NULL WAVEFORMATEX!");
        return E_INVALIDARG;
    }
    if (!ptr_dsound_)
    {
        DBGLOG("called without valid IDirectSound pointer!");
        return E_UNEXPECTED;
    }
    const WORD fmt_tag = ptr_wfx->Format.wFormatTag;
    if (WAVE_FORMAT_PCM != fmt_tag && WAVE_FORMAT_IEEE_FLOAT != fmt_tag)
    {
        DBGLOG("unsupported format tag!");
        return E_INVALIDARG;
    }
    DSBUFFERDESC aud_buffer_desc = {0};
    aud_buffer_desc.dwSize = sizeof DSBUFFERDESC;
    aud_buffer_desc.guid3DAlgorithm = DS3DALG_DEFAULT;
    aud_buffer_desc.lpwfxFormat = (WAVEFORMATEX*)ptr_wfx;
    aud_buffer_desc.dwBufferBytes = ptr_wfx->Format.nAvgBytesPerSec;
    aud_buffer_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
    // Obtain our IDirectSoundBuffer8 interface pointer, |ptr_dsound_buf_|, via
    // creation of an IDirectSoundBuffer, and then use QI on the
    // IDirectSoundBuffer instance to obtain the IDirectSoundBuffer8 instance.
    HRESULT hr;
    IDirectSoundBuffer* ptr_dsbuf;
    CHK(hr, ptr_dsound_->CreateSoundBuffer(&aud_buffer_desc, &ptr_dsbuf, NULL));
    if (FAILED(hr) || !ptr_dsbuf)
    {
        return hr;
    }
    void* ptr_dsound_buf8 = reinterpret_cast<void*>(ptr_dsound_buf_);
    CHK(hr, ptr_dsbuf->QueryInterface(IID_IDirectSoundBuffer8,
                                      &ptr_dsound_buf8));
    if (FAILED(hr))
    {
        return hr;
    }
    return hr;
}

} // WebmDirectSound namespace
