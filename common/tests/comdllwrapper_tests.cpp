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
#include <string>

#include "debugutil.hpp"
#include "comreg.hpp"
#include "comdllwrapper.hpp"
#include "gtest/gtest.h"

TEST(ComDllWrapperBasic, FailPathDoesNotExist)
{
    WebmMfUtil::ComDllWrapper dll_wrapper;
    GUID guid = GUID_NULL;
    std::wstring fake_dll_path = L"FakeDllName.dll";
    ASSERT_EQ(E_INVALIDARG, dll_wrapper.LoadDll(fake_dll_path, guid));
}

TEST(ComDllWrapperBasic, FailGuidNull)
{
    WebmMfUtil::ComDllWrapper dll_wrapper;
    GUID guid = GUID_NULL;
    // abuse the DShow DLL for a quick test
    std::wstring dll_path = L"C:\\Windows\\System32\\quartz.dll";
    ASSERT_EQ(CLASS_E_CLASSNOTAVAILABLE, dll_wrapper.LoadDll(dll_path, guid));
}

TEST(ComDllWrapperBasic, CreateAsyncFileSource)
{
    WebmMfUtil::ComDllWrapper dll_wrapper;
    // abuse the DShow DLL for a quick test
    std::wstring dll_path = L"C:\\Windows\\System32\\quartz.dll";
    // use the async file source filter as test fodder
    const wchar_t* async_src_str = L"{E436EBB5-524F-11CE-9F53-0020AF0BA770}";
    GUID async_src_clsid = ComReg::GUIDFromString(async_src_str);
    ASSERT_EQ(S_OK, dll_wrapper.LoadDll(dll_path, async_src_clsid));
}
