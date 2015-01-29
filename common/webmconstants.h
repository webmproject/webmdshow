// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_WEBMCONSTANTS_HPP__
#define __WEBMDSHOW_COMMON_WEBMCONSTANTS_HPP__

#pragma once

namespace WebmUtil
{
    enum EbmlID
    {
        kEbmlAudioSettingsID = 0xE1,
        kEbmlBlockGroupID = 0xA0,
        kEbmlBlockDurationID = 0x9B,
        kEbmlChannelsID = 0x9F,
        kEbmlClusterID = 0x1F43B675,
        kEbmlCodecIDID = 0x86,
        kEbmlCodecNameID = 0x258688,
        kEbmlCodecPrivateID = 0x63A2,
        kEbmlCrc32ID = 0xC3,
        kEbmlCuesID = 0x1C53BB6B,
        kEbmlDocTypeID = 0x4282,
        kEbmlDocTypeVersionID = 0x4287,
        kEbmlDocTypeReadVersionID = 0x4285,
        kEbmlDurationID = 0x4489,
        kEbmlID = 0x1A45DFA3,
        kEbmlMaxIDLengthID = 0x42F2,
        kEbmlMaxSizeLengthID = 0x42F3,
        kEbmlMuxingAppID = 0x4D80,
        kEbmlReadVersionID = 0x42F7,
        kEbmlReferenceBlockID = 0xFB,
        kEbmlSamplingFrequencyID = 0xB5,
        kEbmlSeekEntryID = 0x4DBB,
        kEbmlSeekHeadID = 0x114D9B74,
        kEbmlSeekIDID = 0x53AB,
        kEbmlSeekPositionID = 0x53AC,
        kEbmlSegmentID = 0x18538067,
        kEbmlSegmentInfoID = 0x1549A966,
        kEbmlTimeCodeID = 0xE7,
        kEbmlTimeCodeScaleID = 0x2AD7B1,
        kEbmlTrackEntryID = 0xAE,
        kEbmlTrackNumberID = 0xD7,
        kEbmlTrackTypeID = 0x83,
        kEbmlTrackUIDID = 0x73C5,
        kEbmlTracksID = 0x1654AE6B,
        kEbmlVersionID = 0x4286,
        kEbmlVideoSettingsID = 0xE0,
        kEbmlVideoHeight = 0xBA,
        kEbmlVideoWidth = 0xB0,
        kEbmlVideoFrameRate = 0x2383E3,
        kEbmlVoidID = 0xEC,
        kEbmlWritingAppID = 0x5741,
    };

    enum EbmlLimits
    {
        kEbmlMaxID1 = 0xFE,
        kEbmlMaxID2 = 0x7FFE,
        kEbmlMaxID3 = 0x3FFFFE,
        kEbmlMaxID4 = 0x1FFFFFFE,
    };

    enum EbmlTrackType
    {
        kEbmlTrackTypeVideo = 1,
        kEbmlTrackTypeAudio = 2,
    };
}

#endif // __WEBMDSHOW_COMMON_WEBMCONSTANTS_HPP__
