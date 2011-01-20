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
#include <vector>

#include "debugutil.hpp"
#include "eventutil.hpp"
#include "comreg.hpp"
#include "comdllwrapper.hpp"
#include "gtest/gtest.h"
#include "memutil.hpp"
#include "mfsrcwrap.hpp"
#include "mftranswrap.hpp"
#include "mfutil.hpp"
#include "tests/mfdllpaths.hpp"
#include "webmtypes.hpp"

using WebmTypes::CLSID_WebmMfByteStreamHandler;
using WebmTypes::CLSID_WebmMfVp8Dec;
using WebmTypes::CLSID_WebmMfVorbisDec;
using WebmMfUtil::MfByteStreamHandlerWrapper;
using WebmMfUtil::MfTransformWrapper;
using WebmMfUtil::mf_startup;
using WebmMfUtil::mf_shutdown;

extern wchar_t* g_test_input_file;

TEST(MfByteStreamHandlerWrapper, CreateMfByteStreamHandlerWrapper)
{
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(0, ptr_mf_bsh->Release());
}

TEST(MfByteStreamHandlerWrapper, LoadFile)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, LoadMediaStreams)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, GetAudioSample)
{
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
        _COM_SMARTPTR_TYPEDEF(IMFSample, IID_IMFSample);
        IMFSamplePtr ptr_sample;
        ASSERT_EQ(S_OK, ptr_mf_bsh->GetAudioSample(&ptr_sample));
        DWORD buffer_count = 0;
        ASSERT_EQ(S_OK, ptr_sample->GetBufferCount(&buffer_count));
        ASSERT_GT(buffer_count, 0UL);
        ASSERT_EQ(true, ptr_sample != NULL);
    }
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, GetVideoSample)
{
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
        _COM_SMARTPTR_TYPEDEF(IMFSample, IID_IMFSample);
        IMFSamplePtr ptr_sample;
        ASSERT_EQ(S_OK, ptr_mf_bsh->GetVideoSample(&ptr_sample));
        DWORD buffer_count = 0;
        ASSERT_EQ(S_OK, ptr_sample->GetBufferCount(&buffer_count));
        ASSERT_GT(buffer_count, 0UL);
        ASSERT_EQ(true, ptr_sample != NULL);
    }
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, Start)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, StartAt10Seconds)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(true, 100000000LL));
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, StartPause)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ASSERT_EQ(S_OK, ptr_mf_bsh->Pause());
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, StartStop)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ASSERT_EQ(S_OK, ptr_mf_bsh->Stop());
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, StartPauseStop)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ASSERT_EQ(S_OK, ptr_mf_bsh->Pause());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Stop());
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, StartPauseStopStartStop)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ASSERT_EQ(S_OK, ptr_mf_bsh->Pause());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Stop());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ASSERT_EQ(S_OK, ptr_mf_bsh->Stop());
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, StartPauseStart)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ASSERT_EQ(S_OK, ptr_mf_bsh->Pause());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, StartSeekTo10Seconds)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(true, 100000000LL));
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfByteStreamHandlerWrapper, StartSeekTo20SecondsPauseStart)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(true, 200000000LL));
    ASSERT_EQ(S_OK, ptr_mf_bsh->Pause());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfTransformWrapper, CreateVp8Decoder)
{
    MfTransformWrapper* ptr_transform = NULL;
    ASSERT_EQ(S_OK, MfTransformWrapper::CreateInstance(VP8DEC_PATH,
                                                       CLSID_WebmMfVp8Dec,
                                                       &ptr_transform));
    ptr_transform->Release();
}

TEST(MfTransformWrapper, CreateVorbisDecoder)
{
    MfTransformWrapper* ptr_transform = NULL;
    ASSERT_EQ(S_OK, MfTransformWrapper::CreateInstance(VORBISDEC_PATH,
                                                       CLSID_WebmMfVorbisDec,
                                                       &ptr_transform));
    ptr_transform->Release();
}

TEST(MfTransformWrapper, SetAudioInputType)
{
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

        ptr_transform->Release();
    }
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfTransformWrapper, SetAudioOutputType)
{
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
        ptr_transform->Release();
    }
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfTransformWrapper, SetVideoInputType)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    if (ptr_mf_bsh->GetVideoStreamCount() > 0)
    {
        _COM_SMARTPTR_TYPEDEF(IMFMediaType, IID_IMFMediaType);
        IMFMediaTypePtr ptr_type;
        ASSERT_EQ(S_OK, ptr_mf_bsh->GetVideoMediaType(&ptr_type));
        GUID major_type = GUID_NULL;
        ASSERT_EQ(S_OK, ptr_type->GetMajorType(&major_type));
        ASSERT_EQ(TRUE, MFMediaType_Video == major_type);
        MfTransformWrapper* ptr_transform = NULL;
        ASSERT_EQ(S_OK, MfTransformWrapper::CreateInstance(VP8DEC_PATH,
                                                           CLSID_WebmMfVp8Dec,
                                                           &ptr_transform));
        ASSERT_EQ(S_OK, ptr_transform->SetInputType(ptr_type));
        ptr_transform->Release();
    }
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(MfTransformWrapper, SetVideoOutputType)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    if (ptr_mf_bsh->GetVideoStreamCount() > 0)
    {
        _COM_SMARTPTR_TYPEDEF(IMFMediaType, IID_IMFMediaType);
        IMFMediaTypePtr ptr_type;
        ASSERT_EQ(S_OK, ptr_mf_bsh->GetVideoMediaType(&ptr_type));
        GUID major_type = GUID_NULL;
        ASSERT_EQ(S_OK, ptr_type->GetMajorType(&major_type));
        ASSERT_EQ(TRUE, MFMediaType_Video == major_type);
        MfTransformWrapper* ptr_transform = NULL;
        ASSERT_EQ(S_OK, MfTransformWrapper::CreateInstance(VP8DEC_PATH,
                                                           CLSID_WebmMfVp8Dec,
                                                           &ptr_transform));
        ASSERT_EQ(S_OK, ptr_transform->SetInputType(ptr_type));
        // empty IMFMediaTypePtr means use first available output type
        ptr_type = 0;
        ASSERT_EQ(S_OK, ptr_transform->SetOutputType(ptr_type));
        ptr_transform->Release();
    }
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(BSHBasicFuzz, PauseWithoutStart)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(MF_E_INVALID_STATE_TRANSITION, ptr_mf_bsh->Pause());
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(BSHBasicFuzz, StopWithoutStart)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(MF_E_INVALID_STATE_TRANSITION, ptr_mf_bsh->Stop());
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}

TEST(BSHBasicFuzz, StartStopPauseWithoutStart)
{
    ASSERT_EQ(S_OK, mf_startup());
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    ASSERT_EQ(S_OK,
        MfByteStreamHandlerWrapper::Create(WEBM_SOURCE_PATH,
                                           CLSID_WebmMfByteStreamHandler,
                                           &ptr_mf_bsh));
    ASSERT_EQ(S_OK, ptr_mf_bsh->OpenURL(g_test_input_file));
    ASSERT_EQ(S_OK, ptr_mf_bsh->LoadMediaStreams());
    ASSERT_EQ(S_OK, ptr_mf_bsh->Start(false, 0LL));
    ASSERT_EQ(S_OK, ptr_mf_bsh->Stop());
    ASSERT_EQ(MF_E_INVALID_STATE_TRANSITION, ptr_mf_bsh->Pause());
    ptr_mf_bsh->Release();
    ASSERT_EQ(S_OK, mf_shutdown());
}