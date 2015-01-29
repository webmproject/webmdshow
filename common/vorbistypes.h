// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once

namespace VorbisTypes
{
    struct VORBISFORMAT  //xiph.org
    {
        DWORD vorbisVersion;
        DWORD samplesPerSec;
        DWORD minBitsPerSec;
        DWORD avgBitsPerSec;
        DWORD maxBitsPerSec;
        BYTE numChannels;
    };

    struct VORBISFORMAT2  //matroska.org
    {
        DWORD channels;
        DWORD samplesPerSec;
        DWORD bitsPerSample;
        DWORD headerSize[3];
    };

    //Matroska/FFmpeg:
    extern const GUID MEDIASUBTYPE_Vorbis2;
    extern const GUID MEDIASUBTYPE_Vorbis2_Xiph_Lacing;
    extern const GUID FORMAT_Vorbis2;

    //Xiph/Ogg Decoder:
    extern const GUID MEDIATYPE_OggPacketStream;
    extern const GUID FORMAT_OggIdentHeader;

    //Xiph/Ogg Encoder:
    extern const GUID MEDIASUBTYPE_Vorbis;
    extern const GUID FORMAT_Vorbis;

}  //end namespace VorbisTypes
