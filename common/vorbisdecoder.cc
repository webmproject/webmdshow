// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <cassert>
#include <cmath>
#include <vector>

#ifdef _WIN32
#include "mferror.h"
#include "windows.h"
#include "mmreg.h"
#else
// note: need SPEAKER_X_Y definitions w/o mmreg.h
//
// Define HRESULT and MF_E_TRANSFORM_NEED_MORE_INPUT in a very very lame
// first step toward making VorbisDecoder cross platform:
typedef long HRESULT;
#define _HRESULT_TYPEDEF_(_sc) ((HRESULT)_sc)
#define MF_E_TRANSFORM_NEED_MORE_INPUT   _HRESULT_TYPEDEF_(0xC00D6D72L)
#endif

#include "debugutil.h"
#include "vorbisdecoder.h"

namespace WebmMfVorbisDecLib
{

VorbisDecoder::VorbisDecoder() :
  m_ogg_packet_count(0),
  m_bytes_per_sample(sizeof(float))
{
    ::memset(&m_vorbis_info, 0, sizeof vorbis_info);
    ::memset(&m_vorbis_comment, 0, sizeof vorbis_comment);
    ::memset(&m_vorbis_state, 0, sizeof vorbis_dsp_state);
    ::memset(&m_vorbis_block, 0, sizeof vorbis_block);
    ::memset(&m_ogg_packet, 0, sizeof ogg_packet);
}

VorbisDecoder::~VorbisDecoder()
{
    DestroyDecoder();
}

int VorbisDecoder::NextOggPacket_(const BYTE* ptr_packet, DWORD packet_size)
{
    if (!ptr_packet || packet_size == 0)
        return E_INVALIDARG;

    m_ogg_packet.b_o_s = (m_ogg_packet_count == 0);
    m_ogg_packet.bytes = packet_size;

    // TODO(tomfinegan): implement End Of Stream handling
    m_ogg_packet.e_o_s = 0;
    m_ogg_packet.granulepos = 0;
    m_ogg_packet.packet = const_cast<BYTE*>(ptr_packet);
    m_ogg_packet.packetno = m_ogg_packet_count++;

    return S_OK;
}

int VorbisDecoder::CreateDecoder(const BYTE** const ptr_headers,
                                 const DWORD* const header_lengths,
                                 unsigned int num_headers)
{
    assert(ptr_headers);
    assert(header_lengths);
    assert(num_headers == 3);

    if (3 != num_headers)
        return E_INVALIDARG;

    vorbis_info_init(&m_vorbis_info);
    vorbis_comment_init(&m_vorbis_comment);

    int status;

    // feed the ident and comment headers into libvorbis
    for (BYTE header_num = 0; header_num < 3; ++header_num)
    {
        assert(header_lengths[header_num] > 0);

        // create an ogg packet in m_ogg_packet with current header for data
        status = NextOggPacket_(ptr_headers[header_num],
                                header_lengths[header_num]);
        if (FAILED(status))
            return E_INVALIDARG;
        assert(m_ogg_packet.packetno == header_num);
        status = vorbis_synthesis_headerin(&m_vorbis_info, &m_vorbis_comment,
                                           &m_ogg_packet);
        if (status < 0)
            return E_INVALIDARG;
    }

    // final init steps, setup decoder state...
    status = vorbis_synthesis_init(&m_vorbis_state, &m_vorbis_info);
    if (status != 0)
        return E_INVALIDARG;

    // ... and vorbis block structs
    status = vorbis_block_init(&m_vorbis_state, &m_vorbis_block);
    if (status != 0)
        return E_INVALIDARG;

    assert(m_vorbis_info.rate > 0);
    assert(m_vorbis_info.channels > 0);

    return S_OK;
}

int VorbisDecoder::CreateDecoderFromBuffer(const BYTE* const ptr_buffer,
                                           UINT size)
{
    const BYTE* ptr_vorbis_headers = ptr_buffer;
    const BYTE* const end = ptr_vorbis_headers + size;

    // read the id and comment header lengths
    const DWORD id_len = *ptr_vorbis_headers++;
    const DWORD comments_len = *ptr_vorbis_headers++;
    // |ptr_vorbis_headers| points to first header, set full private data
    // length:
    const INT64 total_len_ = end - ptr_vorbis_headers;
    const DWORD total_len = (DWORD)total_len_;
    // and calculate the length of the setup header
    const DWORD setup_len = total_len - id_len + comments_len;
    // set the pointer to each vorbis header
    const BYTE* const ptr_id = ptr_vorbis_headers;
    const BYTE* const ptr_comments = ptr_id + id_len;
    const BYTE* const ptr_setup = ptr_comments + comments_len;

    // store the header pointers and lengths for CreateDecoder's use
    const BYTE* header_ptrs[3] = {ptr_id, ptr_comments, ptr_setup};
    const DWORD header_lengths[3] = {id_len, comments_len, setup_len};

    return CreateDecoder(header_ptrs, header_lengths,
                         VORBIS_SETUP_HEADER_COUNT);
}

void VorbisDecoder::DestroyDecoder()
{
    m_ogg_packet_count = 0;

    vorbis_block_clear(&m_vorbis_block);
    vorbis_dsp_clear(&m_vorbis_state);
    vorbis_comment_clear(&m_vorbis_comment);

    // note, from vorbis decoder sample: vorbis_info_clear must be last call
    vorbis_info_clear(&m_vorbis_info);

    m_output_samples.clear();
}

int VorbisDecoder::Decode(BYTE* ptr_samples, UINT32 length)
{
    int status = NextOggPacket_(ptr_samples, length);
    if (FAILED(status))
        return E_FAIL;

    // start decoding the chunk of vorbis data we just wrapped in an ogg packet
    status = vorbis_synthesis(&m_vorbis_block, &m_ogg_packet);
    assert(status == 0);
    if (status != 0)
      return E_FAIL;

    status = vorbis_synthesis_blockin(&m_vorbis_state, &m_vorbis_block);
    assert(status == 0);
    if (status != 0)
      return E_FAIL;

    // Consume all PCM samples from libvorbis
    // Notes:
    // - channel reordering is performed only when necessary
    // - all streams w/>2 channels require inteleaving
    return ReorderAndInterleave_();
}

int VorbisDecoder::GetOutputSamplesAvailable(UINT32* ptr_num_samples_available)
{
    if (!ptr_num_samples_available)
        return E_INVALIDARG;

    const pcm_samples_t::size_type samples_size = m_output_samples.size();
    const UINT32 total_samples_available = static_cast<UINT32>(samples_size);
    const UINT32 channels = m_vorbis_info.channels;

    if (channels > 1)
    {
        // caller wants the total samples, not the size of the sample vector
        *ptr_num_samples_available =
            (total_samples_available + (channels - 1)) / channels;
    }
    else
    {
        // for mono the size of the samples vector is the number of samples
        *ptr_num_samples_available = total_samples_available;
    }

    return S_OK;
}

int VorbisDecoder::ConsumeOutputSamples(float* ptr_out_sample_buffer,
                                        UINT32 blocks_to_consume)
{
    if (!ptr_out_sample_buffer || !blocks_to_consume)
        return E_INVALIDARG;

    if (m_output_samples.empty())
        return MF_E_TRANSFORM_NEED_MORE_INPUT;

    const UINT32 samples_to_consume = blocks_to_consume *
                                      m_vorbis_info.channels;

    const UINT32 bytes_to_copy = samples_to_consume * m_bytes_per_sample;
    ::memcpy(ptr_out_sample_buffer, &m_output_samples[0], bytes_to_copy);

    typedef pcm_samples_t::const_iterator pcm_iterator;
    pcm_iterator pcm_begin = m_output_samples.begin();

    assert(samples_to_consume <= m_output_samples.size());
    const pcm_iterator pcm_end = pcm_begin + samples_to_consume;

    m_output_samples.erase(pcm_begin, pcm_end);

    return S_OK;
}

void VorbisDecoder::Flush()
{
    vorbis_synthesis_restart(&m_vorbis_state);
    m_output_samples.clear();
}

void VorbisDecoder::ReorderAndInterleaveBlock_(float** ptr_blocks, int sample)
{
    const int vorbis_channels = m_vorbis_info.channels;
    assert(vorbis_channels > 0);

    // On channel ordering, from the vorbis spec:
    // http://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9
    // one channel
    //   the stream is monophonic
    // two channels
    //   the stream is stereo. channel order: left, right
    // three channels
    //   the stream is a 1d-surround encoding. channel order: left, center,
    //   right
    // four channels
    //   the stream is quadraphonic surround. channel order: front left, front
    //   right, rear left, rear right
    // five channels
    //   the stream is five-channel surround. channel order: front left,
    //   center, front right, rear left, rear right
    // six channels
    //   the stream is 5.1 surround. channel order: front left, center,
    //   front right, rear left, rear right, LFE
    // seven channels
    //   the stream is 6.1 surround. channel order: front left, center,
    //   front right, side left, side right, rear center, LFE
    // eight channels
    //   the stream is 7.1 surround. channel order: front left, center,
    //   front right, side left, side right, rear left, rear right, LFE
    // greater than eight channels
    //   channel use and order is defined by the application

    switch (vorbis_channels)
    {
        case 3:
            m_output_samples.push_back(ptr_blocks[0][sample]); // FL
            m_output_samples.push_back(ptr_blocks[2][sample]); // FR
            m_output_samples.push_back(ptr_blocks[1][sample]); // FC
            break;
        case 5:
            m_output_samples.push_back(ptr_blocks[0][sample]); // FL
            m_output_samples.push_back(ptr_blocks[2][sample]); // FR
            m_output_samples.push_back(ptr_blocks[1][sample]); // FC
            m_output_samples.push_back(ptr_blocks[3][sample]); // BL
            m_output_samples.push_back(ptr_blocks[4][sample]); // BR
            break;
        case 6:
            // WebM Vorbis decode multi-channel ordering
            // 5.1 Vorbis to PCM (Decoding)
            // Vorbis                PCM
            // 0 Front Left   => 0 Front Left
            // 1 Front Center => 2 Front Right
            // 2 Front Right  => 1 Front Center
            // 3 Back Left    => 5 LFE
            // 4 Back Right   => 3 Back Left
            // 5 LFE          => 4 Back Right
            m_output_samples.push_back(ptr_blocks[0][sample]); // FL
            m_output_samples.push_back(ptr_blocks[2][sample]); // FR
            m_output_samples.push_back(ptr_blocks[1][sample]); // FC
            m_output_samples.push_back(ptr_blocks[5][sample]); // LFE
            m_output_samples.push_back(ptr_blocks[3][sample]); // BL
            m_output_samples.push_back(ptr_blocks[4][sample]); // BR
            break;
        case 7:
            m_output_samples.push_back(ptr_blocks[0][sample]); // FL
            m_output_samples.push_back(ptr_blocks[2][sample]); // FR
            m_output_samples.push_back(ptr_blocks[1][sample]); // FC
            m_output_samples.push_back(ptr_blocks[6][sample]); // LFE
            m_output_samples.push_back(ptr_blocks[5][sample]); // BC
            m_output_samples.push_back(ptr_blocks[3][sample]); // SL
            m_output_samples.push_back(ptr_blocks[4][sample]); // SR
            break;
        case 8:
            // 7.1 Vorbis to PCM (Decoding)
            // Vorbis             PCM
            // 0 Front Left   => 0 Front Left
            // 1 Front Center => 2 Front Right
            // 2 Front Right  => 1 Front Center
            // 3 Side Left    => 7 LFE
            // 4 Side Right   => 5 Back Left
            // 5 Back Left    => 6 Back Right
            // 6 Back Right   => 3 Side Left
            // 7 LFE          => 4 Side Right
            m_output_samples.push_back(ptr_blocks[0][sample]); // FL
            m_output_samples.push_back(ptr_blocks[2][sample]); // FR
            m_output_samples.push_back(ptr_blocks[1][sample]); // FC
            m_output_samples.push_back(ptr_blocks[7][sample]); // LFE
            m_output_samples.push_back(ptr_blocks[5][sample]); // BL
            m_output_samples.push_back(ptr_blocks[6][sample]); // BR
            m_output_samples.push_back(ptr_blocks[3][sample]); // SL
            m_output_samples.push_back(ptr_blocks[4][sample]); // SR
            break;
        case 1:
        case 2:
        case 4:
        default:
            // For mono/stereo/quadrophonic stereo/>8 channels: output in the
            // order libvorbis uses.  It's correct for the formats named, and
            // at present the Vorbis spec says streams w/>8 channels have user
            // defined channel order.
            for (int channel = 0; channel < vorbis_channels; ++channel)
                m_output_samples.push_back(ptr_blocks[channel][sample]);
    }


}

int VorbisDecoder::ReorderAndInterleave_()
{
    int samples = 0;
    float** pp_pcm;
    vorbis_dsp_state* const ptr_state = &m_vorbis_state;
    while ((samples = vorbis_synthesis_pcmout(ptr_state, &pp_pcm)) > 0)
    {
        for (int sample = 0; sample < samples; ++sample)
            ReorderAndInterleaveBlock_(pp_pcm, sample);

        vorbis_synthesis_read(ptr_state, samples);
    }
    return S_OK;
}

UINT32 VorbisDecoder::GetChannelMask() const
{
    assert(m_vorbis_info.channels > 0);
    const int vorbis_channels = m_vorbis_info.channels;

    UINT32 mask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    switch (vorbis_channels)
    {
        case 2:
            break;
        case 1:
            mask = SPEAKER_FRONT_CENTER;
            break;
        case 3:
            mask |= SPEAKER_FRONT_CENTER;
            break;
        case 4:
            mask |= SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
            break;
        case 5:
            mask |= SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT |
                    SPEAKER_BACK_RIGHT;
            break;
        case 6:
            mask |= SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
            break;
        case 7:
            mask |= SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                    SPEAKER_BACK_CENTER | SPEAKER_SIDE_LEFT |
                    SPEAKER_SIDE_RIGHT;
            break;
        case 8:
            mask |= SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
                    SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
            break;
        default:
            mask = 0;
    }

    return mask;
}

} // end namespace WebmMfVorbisDecLib
