// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <objbase.h>
#include "webmtypes.hpp"

const GUID WebmTypes::MEDIASUBTYPE_WEBM =
{ /* ED3110EB-5211-11DF-94AF-0026B977EEAA */
    0xED3110EB,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const GUID WebmTypes::MEDIASUBTYPE_VP8_STATS =
{ /* ED3110EC-5211-11DF-94AF-0026B977EEAA */
    0xED3110EC,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


// 30385056-0000-0010-8000-00AA00389B71 'VP80'
const GUID WebmTypes::MEDIASUBTYPE_VP80 =
{
    0x30385056,
    0x0000,
    0x0010,
    { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};

// 30395056-0000-0010-8000-00AA00389B71 'VP90'
const GUID WebmTypes::MEDIASUBTYPE_VP90 =
{
    0x30395056,
    0x0000,
    0x0010,
    { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};

// 30323449-0000-0010-8000-00AA00389B71 'I420'
const GUID WebmTypes::MEDIASUBTYPE_I420 =
{
    0x30323449,
    0x0000,
    0x0010,
    { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};

//now defined in type library
//const CLSID WebmTypes::CLSID_WebmMux =
//{ /* ED3110F0-5211-11DF-94AF-0026B977EEAA */
//    0xED3110F0,
//    0x5211,
//    0x11DF,
//    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
//};


const CLSID WebmTypes::CLSID_WebmSource =
{ /* ED3110F7-5211-11DF-94AF-0026B977EEAA */
    0xED3110F7,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const GUID WebmTypes::CLSID_WebmSplit =
{ /* ED3110F8-5211-11DF-94AF-0026B977EEAA */
    0xED3110F8,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const CLSID WebmTypes::CLSID_WebmMfSource =
{ /* ED311110-5211-11DF-94AF-0026B977EEAA */
    0xED311110,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const CLSID WebmTypes::CLSID_WebmMfByteStreamHandler =
{ /* ED311111-5211-11DF-94AF-0026B977EEAA */
    0xED311111,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const CLSID WebmTypes::CLSID_WebmMfVp8Dec =
{  /* ED311120-5211-11DF-94AF-0026B977EEAA */
    0xED311120,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const GUID WebmTypes::WebMSample_Preroll =
{  /* ED311121-5211-11DF-94AF-0026B977EEAA */
    0xED311121,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const CLSID WebmTypes::CLSID_WebmMfVorbisDec =
{ /* ED311130-5211-11DF-94AF-0026B977EEAA */
    0xED311130,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const CLSID WebmTypes::CLSID_WebmVorbisDecoder =
{  /* ED311103-5211-11DF-94AF-0026B977EEAA */
    0xED311103,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const CLSID WebmTypes::CLSID_WebmVorbisEncoder =
{ /* ED311107-5211-11DF-94AF-0026B977EEAA */
    0xED311107,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const CLSID WebmTypes::CLSID_WebmOggSource =
{ /* ED311104-5211-11DF-94AF-0026B977EEAA */
    0xED311104,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const GUID WebmTypes::APPID_WebmMf =
{ /* ED3112D0-5211-11DF-94AF-0026B977EEAA */
    0xED3112D0,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


const CLSID WebmTypes::CLSID_WebmColorConversion =
{ /* ED311140-5211-11DF-94AF-0026B977EEAA */
    0xED311140,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};
