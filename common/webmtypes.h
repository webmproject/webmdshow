// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once

namespace WebmTypes
{
    extern const GUID MEDIASUBTYPE_WEBM;
    extern const GUID MEDIASUBTYPE_VP80;
    extern const GUID MEDIASUBTYPE_VP90;
    extern const GUID MEDIASUBTYPE_I420;
    extern const GUID MEDIASUBTYPE_VP8_STATS;

    //extern const CLSID CLSID_WebmMux;
    extern const CLSID CLSID_WebmSource;  //DirectShow
    extern const CLSID CLSID_WebmSplit;
    extern const CLSID CLSID_WebmVorbisDecoder;
    extern const CLSID CLSID_WebmVorbisEncoder;
    extern const CLSID CLSID_WebmOggSource;
    extern const CLSID CLSID_WebmColorConversion;


    extern const GUID APPID_WebmMf;         //Media Foundation Application ID
    extern const CLSID CLSID_WebmMfSource;  //Media Foundation
    extern const CLSID CLSID_WebmMfByteStreamHandler;

    extern const CLSID CLSID_WebmMfVp8Dec;  //Media Foundation
    extern const GUID WebMSample_Preroll;

    extern const CLSID CLSID_WebmMfVorbisDec; //Media Foundation
}
