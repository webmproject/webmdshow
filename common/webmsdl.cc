// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <windowsx.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>

#include <new>

#include "debugutil.h"
#include "webmsdl.h"
#include "webmtypes.h"

namespace WebmSdl
{

SdlInstance::SdlInstance():
  ref_count_(0)
{
    DBGLOG("ctor");
}

SdlInstance::~SdlInstance()
{
    SDL_Quit();
    DBGLOG("dtor");
}

HRESULT SdlInstance::CreateInstance(SdlInstance** ptr_instance)
{
    SdlInstance* ptr_sdl_instance = new (std::nothrow) SdlInstance();
    if (!ptr_sdl_instance)
    {
        DBGLOG("ERROR, null SdlInstance, E_OUTOFMEMORY");
        return E_OUTOFMEMORY;
    }
    HRESULT hr = SDL_Init(SDL_INIT_EVERYTHING);
    if (FAILED(hr))
    {
        DBGLOG("SDL_Init failed, " << SDL_GetError());
        return hr;
    }
    ptr_sdl_instance->AddRef();
    *ptr_instance = ptr_sdl_instance;
    return S_OK;
}

ULONG SdlInstance::AddRef()
{
    return InterlockedIncrement(&ref_count_);
}

ULONG SdlInstance::Release()
{
    UINT ref_count = InterlockedDecrement(&ref_count_);
    if (ref_count == 0)
    {
        delete this;
    }
    return ref_count;
}

SdlPlayerBase::SdlPlayerBase():
  ptr_sdl_instance_(NULL)
{
    DBGLOG("ctor");
}

SdlPlayerBase::~SdlPlayerBase()
{
    if (ptr_sdl_instance_)
    {
        ptr_sdl_instance_->Release();
        ptr_sdl_instance_ = NULL;
    }
    DBGLOG("dtor");
}

HRESULT SdlPlayerBase::InitPlayer(SdlInstance* ptr_instance,
                                  IMFMediaType* ptr_media_type)
{
    if (!ptr_instance)
    {
        DBGLOG("ERROR, NULL SdlInstance, E_INVALIDARG");
        return E_INVALIDARG;
    }
    if (!ptr_media_type)
    {
        DBGLOG("ERROR, NULL IMFMediaType, E_INVALIDARG");
        return E_INVALIDARG;
    }
    GUID guid = GUID_NULL;
    HRESULT hr = ptr_media_type->GetGUID(MF_MT_MAJOR_TYPE, &guid);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, unable to obtain major type, MF_E_INVALIDMEDIATYPE");
        return MF_E_INVALIDMEDIATYPE;
    }
    if (MFMediaType_Audio != guid && MFMediaType_Video != guid)
    {
        DBGLOG("ERROR, unsupported major type, MF_E_INVALIDMEDIATYPE");
        return MF_E_INVALIDMEDIATYPE;
    }
    ptr_instance->AddRef();
    ptr_sdl_instance_ = ptr_instance;
    return S_OK;
}

SdlAudioPlayer::SdlAudioPlayer()
{
    DBGLOG("ctor");
}

SdlAudioPlayer::~SdlAudioPlayer()
{
    DBGLOG("dtor");
}

HRESULT SdlAudioPlayer::InitPlayer(SdlInstance* ptr_sdl_instance,
                                   IMFMediaType* ptr_media_type)
{
    HRESULT hr = SdlPlayerBase::InitPlayer(ptr_sdl_instance, ptr_media_type);
    if (FAILED(hr))
    {
        DBGLOG("SdlPlayerBase::InitPlayer failed" << HRLOG(hr));
        return hr;
    }
    GUID subtype = GUID_NULL;
    hr = ptr_media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (FAILED(hr))
    {
        DBGLOG("media type GetGUID MF_MT_SUBTYPE failed" << HRLOG(hr));
        return hr;
    }
    // TODO(tomfinegan): support MFAudioFormat_PCM-- we have to resample for
    //                   SDL anyway, so we might as well allow it to come in
    //                   that way.
    if (MFAudioFormat_Float != subtype)
    {
        DBGLOG("ERROR, unsupported media subtype, MF_E_INVALIDMEDIATYPE");
        return MF_E_INVALIDMEDIATYPE;
    }
    // Store the audio input format
    hr = StoreAudioInputFormat_(ptr_media_type);
    if (FAILED(hr))
    {
        DBGLOG("StoreAudioInputFormat_ failed" << HRLOG(hr));
        return hr;
    }
    return hr;
}

HRESULT SdlAudioPlayer::StoreAudioInputFormat_(IMFMediaType* ptr_media_type)
{
    // caller already checked ptr_media_type excessively
    HRESULT hr = ptr_media_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,
                                           &bits_per_sample_);
    if (FAILED(hr))
    {
        DBGLOG("unable to obtain sample size" << HRLOG(hr));
        return hr;
    }
    if (0 == bits_per_sample_)
    {
        DBGLOG("invalid sample size");
        return MF_E_INVALIDMEDIATYPE;
    }
    hr = ptr_media_type->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &block_align_);
    if (FAILED(hr))
    {
        DBGLOG("unable to obtain block alignment" << HRLOG(hr));
        return hr;
    }
    if (0 == block_align_)
    {
        DBGLOG("invalid block alignment");
        return MF_E_INVALIDMEDIATYPE;
    }
    hr = ptr_media_type->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
                                   &bytes_per_sec_);
    if (FAILED(hr))
    {
        DBGLOG("unable to obtain bytes per second" << HRLOG(hr));
        return hr;
    }
    if (0 == bytes_per_sec_)
    {
        DBGLOG("invalid bytes per sec");
        return MF_E_INVALIDMEDIATYPE;
    }
    hr = ptr_media_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels_);
    if (FAILED(hr))
    {
        DBGLOG("unable to obtain channel count" << HRLOG(hr));
        return hr;
    }
    if (0 == channels_)
    {
        DBGLOG("invalid channel count");
        return MF_E_INVALIDMEDIATYPE;
    }
    hr = ptr_media_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,
                                   &sample_rate_);
    if (FAILED(hr))
    {
        DBGLOG("unable to sample rate" << HRLOG(hr));
        return hr;
    }
    if (0 == sample_rate_)
    {
        DBGLOG("invalid sample rate");
        return MF_E_INVALIDMEDIATYPE;
    }
    return S_OK;
}

} // WebmSdl namespace
