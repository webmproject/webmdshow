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
#include "mfobjwrapper.hpp"
#include "tests/mfdllpaths.hpp"
#include "webmtypes.hpp"

TEST(MfObjWrapperBasic, CreateMfByteStreamHandlerWrapper)
{
    WebmMfUtil::MfByteStreamHandlerWrapper mf_bsh;
    ASSERT_EQ(S_OK, mf_bsh.Create(WEBM_SOURCE_PATH,
                                  WebmTypes::CLSID_WebmMfByteStreamHandler));
}

TEST(MfObjWrapperBasic, CreateMfTransformWrapper1)
{
    WebmMfUtil::MfTransformWrapper mf_transform;
    ASSERT_EQ(S_OK, mf_transform.Create(VP8DEC_PATH,
                                        WebmTypes::CLSID_WebmMfVp8Dec));
}

TEST(MfObjWrapperBasic, CreateMfTransformWrapper2)
{
    WebmMfUtil::MfTransformWrapper mf_transform;
    ASSERT_EQ(S_OK, mf_transform.Create(VORBISDEC_PATH,
                                        WebmTypes::CLSID_WebmMfVorbisDec));
}
