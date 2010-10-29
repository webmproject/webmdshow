#include <cassert>
#include <cmath>
#include <vector>

#ifdef _WIN32
#include "mferror.h"
#include "windows.h"
#include "mmsystem.h"
#include "mmreg.h"
#endif

#include "vorbisdecoder.hpp"

#ifdef _DEBUG
#include "odbgstream.hpp"
#include "iidstr.hpp"
using std::endl;

// keep the compiler quiet about do/while(0)'s used in log macros
#pragma warning(disable:4127)

#define DBGLOG(X) \
do { \
    wodbgstream wos; \
    wos << "["__FUNCTION__"] " << X << endl; \
} while(0)

#else
#define DBGLOG(X)
#endif

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
    int samples = 0;
    float** pp_pcm;

    while ((samples = vorbis_synthesis_pcmout(&m_vorbis_state, &pp_pcm)) > 0)
    {
        for (int sample = 0; sample < samples; ++sample)
        {
            for (int channel = 0; channel < m_vorbis_info.channels; ++channel)
            {
                m_output_samples.push_back(pp_pcm[channel][sample]);
            }
        }
        vorbis_synthesis_read(&m_vorbis_state, samples);
    }

    return S_OK;
}

int VorbisDecoder::GetOutputSamplesAvailable(UINT32* ptr_num_samples_available)
{
    if (!ptr_num_samples_available)
        return E_INVALIDARG;

    const UINT32 total_samples_available = m_output_samples.size();
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

} // end namespace WebmMfVorbisDecLib
