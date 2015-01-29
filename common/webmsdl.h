// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_WEBMSDL_HPP__
#define __WEBMDSHOW_COMMON_WEBMSDL_HPP__

#pragma warning(push)
// disable member alignment sensitive to packing warning: we know SDL is 4
// byte aligned, and we're fine with that
#pragma warning(disable:4121)
#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_audio.h"
#include "SDL_timer.h"
#pragma warning(pop)

// forward declare MediaFoundation Media Type
struct IMFMediaType;

namespace WebmSdl
{

class SdlInstance
{
public:
    static HRESULT CreateInstance(SdlInstance** ptr_instance);
    ULONG AddRef();
    ULONG Release();
private:
    SdlInstance();
    ~SdlInstance();

    ULONG ref_count_;

    DISALLOW_COPY_AND_ASSIGN(SdlInstance);
};

class SdlPlayerBase
{
public:
    SdlPlayerBase();
    virtual ~SdlPlayerBase();
    virtual HRESULT InitPlayer(SdlInstance*, IMFMediaType*);
private:
    SdlInstance* ptr_sdl_instance_;
    DISALLOW_COPY_AND_ASSIGN(SdlPlayerBase);
};

class SdlAudioPlayer : public SdlPlayerBase
{
    // TODO(tomfinegan): use private ctor/dtor here too?
public:
    SdlAudioPlayer();
    virtual ~SdlAudioPlayer();
    virtual HRESULT InitPlayer(SdlInstance* ptr_sdl_instance,
                               IMFMediaType* ptr_media_type);
private:
    UINT32 bits_per_sample_;
    UINT32 block_align_;
    UINT32 bytes_per_sec_;
    UINT32 channels_;
    UINT32 sample_rate_;

    HRESULT StoreAudioInputFormat_(IMFMediaType* ptr_media_type);
    DISALLOW_COPY_AND_ASSIGN(SdlAudioPlayer);
};

} // WebmSdl namespace

#endif // __WEBMDSHOW_COMMON_WEBMSDL_HPP__
