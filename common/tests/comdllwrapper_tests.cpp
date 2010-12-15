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

#include <comdef.h>
#include <string>

#include "debugutil.hpp"
#include "comreg.hpp"
#include "comdllwrapper.hpp"
#include "gtest/gtest.h"
#include "tests/mfdllpaths.hpp"
#include "webmtypes.hpp"

using WebmTypes::CLSID_WebmMfVorbisDec;
using WebmMfUtil::ComDllWrapper;

TEST(ComDllWrapperBasic, FailPathDoesNotExist)
{
    ComDllWrapper* ptr_dll_wrapper = NULL;
    GUID guid = GUID_NULL;
    std::wstring fake_dll_path = L"FakeDllName.dll";
    ASSERT_EQ(E_INVALIDARG,
              ComDllWrapper::Create(fake_dll_path, guid, &ptr_dll_wrapper));
}

TEST(ComDllWrapperBasic, FailClassNotAvailable)
{
    ComDllWrapper* ptr_dll_wrapper = NULL;
    GUID guid = GUID_NULL;
    ASSERT_EQ(CLASS_E_CLASSNOTAVAILABLE,
              ComDllWrapper::Create(VORBISDEC_PATH, guid, &ptr_dll_wrapper));
}

TEST(ComDllWrapperBasic, CreateVorbisDec)
{
    ComDllWrapper* ptr_dll_wrapper = NULL;
    ASSERT_EQ(S_OK,
              ComDllWrapper::Create(VORBISDEC_PATH, CLSID_WebmMfVorbisDec,
                                    &ptr_dll_wrapper));
    IMFTransform* ptr_mftransform = NULL;
    void* ptr_transform = reinterpret_cast<void*>(ptr_mftransform);
    ASSERT_EQ(S_OK, ptr_dll_wrapper->CreateInstance(IID_IMFTransform,
                                                    &ptr_transform));
    if (ptr_mftransform)
    {
        ptr_mftransform->Release();
    }
    ptr_dll_wrapper->Release();
}
