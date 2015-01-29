// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <objbase.h>
#include "vorbistypes.h"

// {8D2FD10B-5841-4a6b-8905-588FEC1ADED9}
const GUID VorbisTypes::MEDIASUBTYPE_Vorbis2 =
{
    0x8D2FD10B,
    0x5841,
    0x4a6b,
    { 0x89, 0x05, 0x58, 0x8F, 0xEC, 0x1A, 0xDE, 0xD9 }
};


const GUID VorbisTypes::MEDIASUBTYPE_Vorbis2_Xiph_Lacing =
{ /* ED311105-5211-11DF-94AF-0026B977EEAA */
    0xED311105,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


// {B36E107F-A938-4387-93C7-55E966757473}
const GUID VorbisTypes::FORMAT_Vorbis2 =
{
    0xB36E107F,
    0xA938,
    0x4387,
    { 0x93, 0xC7, 0x55, 0xE9, 0x66, 0x75, 0x74, 0x73 }
};

//See vorbisdecoderdllstuff.h (in the oggdsf source tree from Xiph.org):
// {60891713-C24F-4767-B6C9-6CA05B3338FC}
const GUID VorbisTypes::MEDIATYPE_OggPacketStream =
{
    0x60891713,
    0xc24f,
    0x4767,
    { 0xb6, 0xc9, 0x6c, 0xa0, 0x5b, 0x33, 0x38, 0xfc }
};

// {95388704-162C-42a9-8149-C3577C12AAF9}
const GUID VorbisTypes::FORMAT_OggIdentHeader =
{
    0x95388704,
    0x162c,
    0x42a9,
    { 0x81, 0x49, 0xc3, 0x57, 0x7c, 0x12, 0xaa, 0xf9 }
};

// {8A0566AC-42B3-4ad9-ACA3-93B906DDF98A}
const GUID VorbisTypes::MEDIASUBTYPE_Vorbis =
{
    0x8a0566ac,
    0x42b3,
    0x4ad9,
    { 0xac, 0xa3, 0x93, 0xb9, 0x6, 0xdd, 0xf9, 0x8a }
};

// {44E04F43-58B3-4de1-9BAA-8901F852DAE4}
const GUID VorbisTypes::FORMAT_Vorbis =
{
    0x44e04f43,
    0x58b3,
    0x4de1,
    { 0x9b, 0xaa, 0x89, 0x1, 0xf8, 0x52, 0xda, 0xe4 }
};
