#include <cassert>
#include <cmath>
#include <vector>

#ifdef _WIN32
#include "mferror.h"
#include "windows.h"
#include "mmsystem.h"
#include "mmreg.h"
#endif

#include "VorbisDecoder.hpp"

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
#define DBGLOG(X) do {} while(0)
#endif

namespace WebmMfVorbisDecLib
{

VorbisDecoder::VorbisDecoder() :
  m_ogg_packet_count(0),
  m_output_format_tag(WAVE_FORMAT_IEEE_FLOAT),
  m_output_bits_per_sample(sizeof(float))
{
    ::memset(&m_vorbis_info, 0, sizeof vorbis_info);
    ::memset(&m_vorbis_comment, 0, sizeof vorbis_comment);
    ::memset(&m_vorbis_state, 0, sizeof vorbis_dsp_state);
    ::memset(&m_vorbis_block, 0, sizeof vorbis_block);
    ::memset(&m_ogg_packet, 0, sizeof ogg_packet);
    ::memset(&m_wave_format, 0, sizeof WAVEFORMATEX);
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

    SetDecodedWaveFormat_();

    assert(m_vorbis_info.rate > 0);
    assert(m_vorbis_info.channels > 0);

#ifdef _DEBUG
    m_pcm_writer.Open(&m_wave_format);
#endif

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

void VorbisDecoder::SetDecodedWaveFormat_()
{
    m_wave_format.nChannels = static_cast<WORD>(m_vorbis_info.channels);
    m_wave_format.nSamplesPerSec = static_cast<WORD>(m_vorbis_info.rate);
    m_wave_format.wBitsPerSample = sizeof(float) * 8;
    m_wave_format.nBlockAlign = m_wave_format.nChannels * sizeof(float);
    m_wave_format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    m_wave_format.nAvgBytesPerSec = m_wave_format.nBlockAlign *
                                    m_wave_format.nSamplesPerSec;
}

int VorbisDecoder::GetWaveFormat(WAVEFORMATEX* ptr_out_wave_format)
{
    if (!ptr_out_wave_format)
        return E_INVALIDARG;
    assert(m_wave_format.cbSize == 0);
    memcpy(ptr_out_wave_format, &m_wave_format, sizeof WAVEFORMATEX);
    return S_OK;
}

int VorbisDecoder::Decode(BYTE* ptr_samples, UINT32 length)
{
    int status = NextOggPacket_(ptr_samples, length);
    if (FAILED(status))
        return E_FAIL;

    // start decoding the chunk of vorbis data we just wrapped in an ogg packet
    status = vorbis_synthesis(&m_vorbis_block, &m_ogg_packet);
    // TODO(tomfinegan): will vorbis_synthesis ever return non-zero?
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

int VorbisDecoder::ConsumeOutputSamples(BYTE* ptr_out_sample_buffer,
                                        UINT32 buffer_limit_in_bytes,
                                        UINT32* ptr_output_bytes_written,
                                        UINT32* ptr_output_sample_count)
{
    if (!ptr_out_sample_buffer || !ptr_output_bytes_written ||
        !ptr_output_sample_count)
        return E_INVALIDARG;

    if (m_output_samples.empty())
        return MF_E_TRANSFORM_NEED_MORE_INPUT;

    const UINT32 bytes_per_sample = m_wave_format.wBitsPerSample >> 3;
    const UINT32 bytes_available = m_output_samples.size() * bytes_per_sample;
    UINT32 bytes_to_copy = bytes_available > buffer_limit_in_bytes ?
                           buffer_limit_in_bytes : bytes_available;

    if (m_output_format_tag == WAVE_FORMAT_PCM)
    {
        return ConvertOutputSamplesS16_(ptr_out_sample_buffer,
                                        buffer_limit_in_bytes,
                                        ptr_output_bytes_written,
                                        ptr_output_sample_count);
    }

    ::memcpy(ptr_out_sample_buffer, &m_output_samples[0], bytes_to_copy);

    const UINT32 samples_consumed =
        static_cast<UINT32>((double)bytes_to_copy / (double)bytes_per_sample);

    typedef pcm_samples_t::const_iterator pcm_iterator;
    pcm_iterator pcm_iter = m_output_samples.begin();
    m_output_samples.erase(pcm_iter, pcm_iter+samples_consumed);

    *ptr_output_sample_count = static_cast<UINT32>(
        (double)samples_consumed / (double)m_vorbis_info.channels);

    DBGLOG("bytes_available=" << bytes_available);
    DBGLOG("buffer_limit_in_bytes=" << buffer_limit_in_bytes);
    DBGLOG("bytes_to_copy=" << bytes_to_copy);
    DBGLOG("samples_consumed=" << samples_consumed);
    DBGLOG("out_sample_count=" << *ptr_output_sample_count);

    *ptr_output_bytes_written = bytes_to_copy;

#ifdef _DEBUG
    m_pcm_writer.Write(ptr_out_sample_buffer, bytes_to_copy);
#endif

    return S_OK;
}

namespace
{
    INT16 clip16(int val)
    {
        if (val > 32767)
            val = 32767;
        else if (val < -32768)
            val = -32768;

        return static_cast<INT16>(val);
    }
} // end anon namespace

int VorbisDecoder::ConvertOutputSamplesS16_(BYTE* ptr_out_sample_buffer,
                                            UINT32 buffer_limit_in_bytes,
                                            UINT32* ptr_output_bytes_written,
                                            UINT32* ptr_output_sample_count)
{

    assert(m_output_bits_per_sample == 16);
    assert(m_output_format_tag == WAVE_FORMAT_PCM);

    typedef pcm_samples_t::const_iterator pcm_iterator;
    pcm_iterator pcm_iter = m_output_samples.begin();
    pcm_iterator pcm_end = m_output_samples.end();

    UINT32 bytes_per_sample = m_output_bits_per_sample >> 3;
    UINT32 max_samples = static_cast<UINT32>(
        (double)buffer_limit_in_bytes / (double)bytes_per_sample);

    UINT32 sample;
    INT16* ptr_out_sample_buffer_s16 = reinterpret_cast<INT16*>(
        ptr_out_sample_buffer);

    for (sample = 0; pcm_iter != pcm_end && sample < max_samples;
         ++pcm_iter, ++sample)
    {
        ptr_out_sample_buffer_s16[sample] = static_cast<INT16>(
            clip16((int)floor(*pcm_iter * 32767.f + .5f)));
    }

    if (pcm_iter != pcm_end)
        m_output_samples.erase(m_output_samples.begin(), pcm_iter);
    else
        m_output_samples.clear();

    *ptr_output_bytes_written = sample * bytes_per_sample;
    *ptr_output_sample_count = static_cast<UINT32>(
        (double)sample / (double)m_vorbis_info.channels);

    return S_OK;
}

void VorbisDecoder::Flush()
{
    vorbis_synthesis_restart(&m_vorbis_state);
    m_output_samples.clear();
}

int VorbisDecoder::SetOutputWaveFormat(int format_tag, int bits_per_sample)
{
    assert(format_tag == WAVE_FORMAT_IEEE_FLOAT ||
           format_tag == WAVE_FORMAT_PCM);
    if (format_tag != WAVE_FORMAT_IEEE_FLOAT && format_tag == WAVE_FORMAT_PCM)
        return E_INVALIDARG;

    const int bits_ieee = sizeof(float) * 8;
    const int bits_s16 = sizeof(INT16) * 8;
    assert(bits_per_sample == bits_ieee || bits_s16 == sizeof(INT16));
    if (bits_ieee != sizeof(float) && bits_s16 != sizeof(INT16))
        return E_INVALIDARG;

    m_output_format_tag = format_tag;
    m_output_bits_per_sample = bits_per_sample;

    return S_OK;
}

} // end namespace WebmMfVorbisDecLib
