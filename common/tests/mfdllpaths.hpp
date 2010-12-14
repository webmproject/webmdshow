// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#if defined _DEBUG
    #if defined _M_X64
        #define WEBMMF_DLL_PATH L"..\\..\\..\\..\\dll\\webmmf\\x64\\Debug\\"
    #else
        #define WEBMMF_DLL_PATH L"..\\..\\..\\..\\dll\\webmmf\\Win32\\Debug\\"
    #endif
#else
    #if defined _M_X64
        #define WEBMMF_DLL_PATH L"..\\..\\..\\..\\dll\\webmmf\\x64\\Release\\"
    #else
        #define WEBMMF_DLL_PATH L"..\\..\\..\\..\\dll\\webmmf\\Win32\\Release\\"
    #endif
#endif

#if defined _M_X64
    #define WEBM_SOURCE_PATH WEBMMF_DLL_PATH L"webmmfsource64.dll"
    #define VORBISDEC_PATH WEBMMF_DLL_PATH L"webmmfvorbisdec64.dll"
    #define VP8DEC_PATH WEBMMF_DLL_PATH L"webmmfvp8dec64.dll"
#else
    #define WEBM_SOURCE_PATH WEBMMF_DLL_PATH L"webmmfsource32.dll"
    #define VORBISDEC_PATH WEBMMF_DLL_PATH L"webmmfvorbisdec32.dll"
    #define VP8DEC_PATH WEBMMF_DLL_PATH L"webmmfvp8dec32.dll"
#endif