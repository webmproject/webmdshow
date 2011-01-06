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

#include <string>

#include "debugutil.hpp"
#include "comdllwrapper.hpp"
#include "eventutil.hpp"
#include "gtest/gtest.h"
#include "mfsrcwrap.hpp"
#include "mftranswrap.hpp"
#include "mfutil.hpp"
#include "tests/mfdllpaths.hpp"
#include "webmsdl.hpp"
#include "webmtypes.hpp"

extern wchar_t* g_test_input_file;

using WebmSdl::SdlInstance;
using WebmSdl::SdlAudioPlayer;
using WebmTypes::CLSID_WebmMfByteStreamHandler;
using WebmTypes::CLSID_WebmMfVorbisDec;
using WebmMfUtil::MfByteStreamHandlerWrapper;
using WebmMfUtil::MfTransformWrapper;
using WebmMfUtil::mf_startup;
using WebmMfUtil::mf_shutdown;

TEST(WebmSdl, Init)
{
    SdlInstance* ptr_sdl_instance = NULL;
    ASSERT_EQ(S_OK, SdlInstance::CreateInstance(&ptr_sdl_instance));
    ASSERT_TRUE(NULL != ptr_sdl_instance);
    ptr_sdl_instance->Release();
}

TEST(WebmSdl, InitAudioPlayer)
{
    SdlInstance* ptr_sdl_instance = NULL;
    ASSERT_EQ(S_OK, SdlInstance::CreateInstance(&ptr_sdl_instance));
    ASSERT_TRUE(NULL != ptr_sdl_instance);
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    if (ptr_mf_bsh->GetAudioStreamCount() > 0)
    {
        _COM_SMARTPTR_TYPEDEF(IMFMediaType, IID_IMFMediaType);
        IMFMediaTypePtr ptr_type;
        ASSERT_EQ(S_OK, ptr_mf_bsh->GetAudioMediaType(&ptr_type));
        GUID major_type = GUID_NULL;
        ASSERT_EQ(S_OK, ptr_type->GetMajorType(&major_type));
        ASSERT_EQ(TRUE, MFMediaType_Audio == major_type);
        MfTransformWrapper* ptr_transform = NULL;
        ASSERT_EQ(S_OK,
            MfTransformWrapper::CreateInstance(VORBISDEC_PATH,
                                               CLSID_WebmMfVorbisDec,
                                               &ptr_transform));
        ASSERT_EQ(S_OK, ptr_transform->SetInputType(ptr_type));
        // empty IMFMediaTypePtr means use first available output type
        ptr_type = 0;
        ASSERT_EQ(S_OK, ptr_transform->SetOutputType(ptr_type));
        ASSERT_EQ(S_OK, ptr_transform->GetOutputType(&ptr_type));
        SdlAudioPlayer audio_player;
        // TODO(tomfinegan): This test is incomplete.  SdlAudioPlayer doesn't
        //                   actually configure SDL-- it only validates the
        //                   media type.
        ASSERT_EQ(S_OK, audio_player.InitPlayer(ptr_sdl_instance, ptr_type));
        ptr_transform->Release();
    }
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());

    ptr_sdl_instance->Release();
}