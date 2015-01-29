// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef _MEDIAFOUNDATION_WEBMMFVORBISDEC_VORBISDECODER_HPP_
#define _MEDIAFOUNDATION_WEBMMFVORBISDEC_VORBISDECODER_HPP_

#include "vorbis/codec.h"

namespace WebmMfVorbisDecLib
{

const UINT32 VORBIS_SETUP_HEADER_COUNT = 3;

class VorbisDecoder
{
public:
    VorbisDecoder();
    ~VorbisDecoder();
    int CreateDecoder(const BYTE** const ptr_headers,
                      const DWORD* const header_lengths,
                      unsigned int num_headers /* must be == 3 */);
    int CreateDecoderFromBuffer(const BYTE* const ptr_buffer, UINT size);

    void DestroyDecoder();

    int Decode(BYTE* ptr_samples, UINT32 length);

    int GetOutputSamplesAvailable(UINT32* ptr_num_samples_available);
    int ConsumeOutputSamples(float* ptr_out_sample_buffer,
                             UINT32 blocks_to_consume);
    void Flush();

    int GetVorbisRate() const
    {
        return m_vorbis_info.rate;
    };
    int GetVorbisChannels() const
    {
        return m_vorbis_info.channels;
    };

    UINT32 GetChannelMask() const;

private:
    int NextOggPacket_(const BYTE* ptr_packet, DWORD packet_size);

    void ReorderAndInterleaveBlock_(float** ptr_blocks, int sample);
    int ReorderAndInterleave_();

    ogg_packet m_ogg_packet;
    DWORD m_ogg_packet_count;

    vorbis_info m_vorbis_info; // contains static bitstream settings
    vorbis_comment m_vorbis_comment; // contains user comments
    vorbis_dsp_state m_vorbis_state; // decoder state
    vorbis_block m_vorbis_block; // working space for packet->PCM decode

    const int m_bytes_per_sample;

    WAVEFORMATEX m_wave_format;

    typedef std::vector<float> pcm_samples_t;
    pcm_samples_t m_output_samples;

    // disallow copy and assign
    DISALLOW_COPY_AND_ASSIGN(VorbisDecoder);
};

} // namespace WebmMfVorbisDecLib

#endif // _MEDIAFOUNDATION_WEBMMFVORBISDEC_VORBISDECODER_HPP_
