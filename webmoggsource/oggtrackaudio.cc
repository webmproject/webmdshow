#include <strmif.h>
#include "oggtrackaudio.hpp"
#include "vorbistypes.hpp"
#include "cmediatypes.hpp"
#include <new>
#include <cassert>
#include <uuids.h>
#if 0 //def _DEBUG
#include "odbgstream.hpp"
#include <iomanip>
using std::endl;
using std::fixed;
using std::setprecision;
#endif

using namespace oggparser;

//enum { cBuffers = 1024 };
//enum { cbBuffer = 1024 };

namespace WebmOggSource
{

HRESULT OggTrackAudio::Create(
    oggparser::OggStream* pStream,
    OggTrackAudio*& pTrack)
{
    if (pStream == 0)
        return E_INVALIDARG;

    pTrack = new (std::nothrow) OggTrackAudio(pStream, 1);

    if (pTrack == 0)
        return E_OUTOFMEMORY;

    HRESULT hr = pTrack->Init();

    if (SUCCEEDED(hr))
        return hr;

    delete pTrack;
    pTrack = 0;

    return hr;
}


OggTrackAudio::OggTrackAudio(
    oggparser::OggStream* pStream,
    ULONG id) :
    OggTrack(pStream, id),
    m_granule_pos(0),
    m_reftime(0),
    m_pfnGetSampleCount(0),
    m_pfnPopulateSamples(0)
{
}


OggTrackAudio::~OggTrackAudio()
{
}


HRESULT OggTrackAudio::Init()
{
    long result = m_pStream->Init(m_ident, m_comment, m_setup);

    if (result < 0)  //error
        return E_FAIL;   //TODO: refine this return value

    result = m_fmt.Read(m_pStream->m_pReader, m_ident);

    if (result < 0)
        return E_FAIL;

    return S_OK;
}


void OggTrackAudio::OnReset()
{
    m_granule_pos = 0;
    m_reftime = 0;
}


void OggTrackAudio::GetMediaTypes(CMediaTypes& mtv) const
{
    IOggReader* const pReader = m_pStream->m_pReader;

    const long ident_len = m_ident.GetLength();
    assert(ident_len == 30);

    const long comment_len = m_comment.GetLength();
    assert(comment_len >= 7);

    const long setup_len = m_setup.GetLength();
    assert(setup_len >= 7);

    const size_t hdr_len = ident_len + comment_len + setup_len;

    using VorbisTypes::VORBISFORMAT2;

    const size_t cb = sizeof(VORBISFORMAT2) + hdr_len;
    BYTE* const pb = (BYTE*)_malloca(cb);

    VORBISFORMAT2& fmt = (VORBISFORMAT2&)(*pb);

    fmt.channels = m_fmt.channels;
    fmt.samplesPerSec = m_fmt.sample_rate;
    fmt.bitsPerSample = 0;  //TODO: how do we synthesize this value?
    fmt.headerSize[0] = ident_len;
    fmt.headerSize[1] = comment_len;
    fmt.headerSize[2] = setup_len;

    BYTE* const dst_end = pb + cb;

    BYTE* dst = pb + sizeof(VORBISFORMAT2);
    assert(dst < dst_end);

    long result = m_ident.Copy(pReader, dst);
    assert(result == ident_len);

    dst += ident_len;
    assert(dst < dst_end);

    result = m_comment.Copy(pReader, dst);
    assert(result == comment_len);

    dst += comment_len;
    assert(dst < dst_end);

    result = m_setup.Copy(pReader, dst);
    assert(result == setup_len);

    dst += setup_len;
    assert(dst == dst_end);

    AM_MEDIA_TYPE mt;

    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = VorbisTypes::MEDIASUBTYPE_Vorbis2;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;
    mt.formattype = VorbisTypes::FORMAT_Vorbis2;
    mt.pUnk = 0;
    mt.cbFormat = static_cast<ULONG>(cb);
    mt.pbFormat = pb;

    mtv.Add(mt);

    mt.subtype = VorbisTypes::MEDIASUBTYPE_Vorbis2_Xiph_Lacing;
    mtv.Add(mt);
}


HRESULT OggTrackAudio::QueryAccept(const AM_MEDIA_TYPE* pmt) const
{
    if (pmt == 0)
        return E_INVALIDARG;

    const AM_MEDIA_TYPE& mt = *pmt;

    if (mt.majortype != MEDIATYPE_Audio)
        return S_FALSE;

    if (mt.subtype == VorbisTypes::MEDIASUBTYPE_Vorbis2)
        __noop;
    else if (mt.subtype == VorbisTypes::MEDIASUBTYPE_Vorbis2_Xiph_Lacing)
        __noop;
    else
        return S_FALSE;

    if (mt.formattype == GUID_NULL)
        return S_OK;  //for now, just ignore remaining items

    if (mt.formattype != VorbisTypes::FORMAT_Vorbis2)
        return S_FALSE;

    if (mt.pbFormat == 0)
        return S_FALSE;

    using VorbisTypes::VORBISFORMAT2;

    if (mt.cbFormat < sizeof(VORBISFORMAT2))
        return S_FALSE;

    const VORBISFORMAT2& fmt = (VORBISFORMAT2&)(*mt.pbFormat);

    if (fmt.channels != m_fmt.channels)
        return S_FALSE;

    if (fmt.samplesPerSec != m_fmt.sample_rate)
        return S_FALSE;

    //fmt.bitsPerSample = 0;  //TODO: how do we synthesize this value?

    const DWORD ident_len = fmt.headerSize[0];
    const DWORD comment_len = fmt.headerSize[1];
    const DWORD setup_len = fmt.headerSize[2];

    const DWORD hdr_len = ident_len + comment_len + setup_len;
    const DWORD cb = sizeof(VORBISFORMAT2) + hdr_len;

    if (mt.cbFormat < cb)
        return S_FALSE;

    //TODO: vet hdrs

    return S_OK;
}


HRESULT OggTrackAudio::SetConnectionMediaType(const AM_MEDIA_TYPE& mt)
{
    if (mt.subtype == VorbisTypes::MEDIASUBTYPE_Vorbis2)
    {
        m_pfnGetSampleCount = &OggTrackAudio::GetSampleCountVorbis2;
        m_pfnPopulateSamples = &OggTrackAudio::PopulateSamplesVorbis2;
    }
    else if (mt.subtype == VorbisTypes::MEDIASUBTYPE_Vorbis2_Xiph_Lacing)
    {
        m_pfnGetSampleCount =
            &OggTrackAudio::GetSampleCountVorbis2XiphLacing;

        m_pfnPopulateSamples =
            &OggTrackAudio::PopulateSamplesVorbis2XiphLacing;
    }
    else
        return E_FAIL;

    m_subtype = mt.subtype;
    return S_OK;
}


HRESULT OggTrackAudio::UpdateAllocatorProperties(
    ALLOCATOR_PROPERTIES& props) const
{
    long cBuffers;
    long cbBuffer;

    if (m_subtype == VorbisTypes::MEDIASUBTYPE_Vorbis2)
    {
        cBuffers = 256;
        cbBuffer = 8 * 1024;  //TODO: synthesize a better estimate
    }
    else
    {
        assert(m_subtype == VorbisTypes::MEDIASUBTYPE_Vorbis2_Xiph_Lacing);

        cBuffers = 1;
        cbBuffer = 64 * 1024;
    }

    if (props.cBuffers < cBuffers)
        props.cBuffers = cBuffers;

    if (props.cbBuffer < cbBuffer)
        props.cbBuffer = cbBuffer;

    if (props.cbAlign <= 0)
        props.cbAlign = 1;

    if (props.cbPrefix < 0)
        props.cbPrefix = 0;

    return S_OK;
}


//long OggTrackAudio::GetBufferSize() const
//{
//    const long blocks_per_frame = m_fmt.blocksize_1;  //?
//    const long bytes_per_block = sizeof(float) * m_fmt.channels;
//    const long bytes_per_frame = blocks_per_frame * bytes_per_block;
//
//    return bytes_per_frame;
//}


std::wostream& OggTrackAudio::GetKind(std::wostream& os) const
{
    return os << L"Audio";
}


std::wstring OggTrackAudio::GetCodecName() const
{
    return L"Vorbis";
}

#if 0
HRESULT OggTrackAudio::GetPackets(long& count)
{
    const long result = GetPackets();

    if (result < 0)
        return E_FAIL;

    if (m_packets.empty())
        return S_FALSE;

    count = (this->*m_pfnGetSampleCount)();

    if (count <= 0)
        return E_FAIL;

    return S_OK;
}


long OggTrackAudio::GetPackets()
{
    //m_granule_pos represents the trailing edge of the
    //audio we have already consumed.  It is therefore
    //the leading edge of the sample(s) we are about
    //to push downstream.

    //We need to consume packets until we get (at least)
    //one that has a non-negative granule pos -- that
    //packet is the last amond the packets we send downstream;
    //Its granule pos is the trailing edge of the current
    //media sample we're about to send, and that value
    //then becomes the leading edge of the next media sample.

    //If there are intervening packets, then we can either
    //estimate the granule pos of each one (perhaps ident.blocksize_{0|1}
    //can be used for this), or we can just pass packets downstream
    //without a media time and hope for the best.

    if (m_granule_pos < 0)
        return -1;

    if (!m_packets.empty())  //weird
    {
        const OggStream::Packet& pkt = m_packets.back();

        if (pkt.granule_pos >= 0)
        {
            assert(pkt.granule_pos > m_granule_pos);
            return 0;  //success
        }
    }

    for (;;)
    {
        OggStream::Packet pkt;

        const long result = m_pStream->GetPacket(pkt);

        if (result < 0)  //error (or EOF)
        {
            if (result != oggparser::E_END_OF_FILE)
                return result;

            if (m_packets.empty())
                return 0;

            if (m_packets.back().granule_pos < 0)
                return -1;

            return 0;  //done
        }

        m_packets.push_back(pkt);

        if (pkt.granule_pos >= 0)
        {
            assert(pkt.granule_pos > m_granule_pos);
            return 0;  //success
        }
    }
}
#else
HRESULT OggTrackAudio::GetPackets(long& count)
{
    if (!m_packets.empty())  //weird
    {
        const OggStream::Packet& pkt = m_packets.back();

        if (pkt.granule_pos >= 0)
        {
            assert(m_granule_pos >= 0);
            assert(pkt.granule_pos > m_granule_pos);

            count = (this->*m_pfnGetSampleCount)();
            assert(count > 0);

            return S_OK;
        }
    }

    for (;;)
    {
        OggStream::Packet pkt;

        const long result = m_pStream->GetPacket(pkt);

        if (result < 0)  //error (or EOF)
        {
            if (result != oggparser::E_END_OF_FILE)
                return E_FAIL;

            break;
        }

        m_packets.push_back(pkt);

        if (pkt.granule_pos >= 0)
        {
            assert(m_granule_pos >= 0);
            assert(pkt.granule_pos > m_granule_pos);

            count = (this->*m_pfnGetSampleCount)();
            assert(count > 0);

            return S_OK;
        }
    }

    //no more packets in stream

    if (m_packets.empty())
        return S_FALSE;  //EOS

    const OggStream::Packet& back = m_packets.back();

    if (back.granule_pos < 0)
        return E_FAIL;

    assert(m_granule_pos >= 0);
    assert(back.granule_pos > m_granule_pos);

    count = (this->*m_pfnGetSampleCount)();
    assert(count > 0);

    return S_OK;
}
#endif

long OggTrackAudio::GetSampleCountVorbis2() const
{
    const packets_t::size_type size = m_packets.size();
    return static_cast<long>(size);
}


long OggTrackAudio::GetSampleCountVorbis2XiphLacing() const
{
    return 1;
}


HRESULT OggTrackAudio::PopulateSamples(const samples_t& ss)
{
    return (this->*m_pfnPopulateSamples)(ss);
}


HRESULT OggTrackAudio::PopulateSamplesVorbis2(const samples_t& ss)
{
    if (m_packets.empty())
        return E_FAIL;

    const packets_t::size_type packet_count = m_packets.size();

    if (ss.size() != packet_count)
        return E_FAIL;

    if (m_granule_pos < 0)
        return E_FAIL;

    if (m_reftime < 0)
        return E_FAIL;

    const LONGLONG granule_pos = m_packets.back().granule_pos;

    if (granule_pos < 0)
        return E_FAIL;

    if (granule_pos <= m_granule_pos)
        return E_FAIL;

    float samples_per_packet;

    if (granule_pos < 0)  //EOS, and no terminating granule_pos
        samples_per_packet = 1024;  //?
    else
    {
        const LONGLONG total_samples = granule_pos - m_granule_pos;
        samples_per_packet = float(total_samples) / packet_count;
    }

    const float samples_per_sec = static_cast<float>(m_fmt.sample_rate);

    LONGLONG curr_samples = m_granule_pos;
    float curr_samples_ = static_cast<float>(curr_samples);

    LONGLONG curr_reftime = m_reftime;

    samples_t::const_iterator tgt_iter = ss.begin();

    while (m_packets.size() > 1)
    {
        const OggStream::Packet& pkt = m_packets.front();

        IMediaSample* const pSample = *tgt_iter++;
        assert(pSample);

        HRESULT hr = PopulateSample(pkt, pSample);

        if (FAILED(hr))
            return hr;

        m_bDiscontinuity = false;

        const float next_samples_ = curr_samples_ + samples_per_packet;
        LONGLONG next_samples = static_cast<LONGLONG>(next_samples_);

        const float next_sec = next_samples_ / samples_per_sec;
        const float next_reftime_ = next_sec * 10000000.0F;

        LONGLONG next_reftime = static_cast<LONGLONG>(next_reftime_);

        hr = pSample->SetMediaTime(&curr_samples, &next_samples);
        assert(SUCCEEDED(hr));

        hr = pSample->SetTime(&curr_reftime, &next_reftime);
        assert(SUCCEEDED(hr));

        curr_samples = next_samples;
        curr_samples_ = next_samples_;

        curr_reftime = next_reftime;

        m_packets.pop_front();
    }

    {
        const OggStream::Packet& pkt = m_packets.front();

        IMediaSample* const pSample = *tgt_iter++;
        assert(pSample);
        assert(tgt_iter == ss.end());

        HRESULT hr = PopulateSample(pkt, pSample);

        if (FAILED(hr))
            return hr;

        m_bDiscontinuity = false;

        if (granule_pos < 0)  //EOS, and no granule_pos on page
        {
            m_granule_pos = -1;  //EOS
            m_reftime = -1;

            hr = pSample->SetMediaTime(0, 0);
            assert(SUCCEEDED(hr));

            hr = pSample->SetTime(&curr_reftime, 0);
            assert(SUCCEEDED(hr));
        }
        else
        {
            m_granule_pos = granule_pos;  //next_samples

            const float next_samples_ = static_cast<float>(m_granule_pos);

            const float next_sec = next_samples_ / samples_per_sec;
            const float next_reftime_ = next_sec * 10000000.0F;

            m_reftime = static_cast<LONGLONG>(next_reftime_);  //next_reftime

            hr = pSample->SetMediaTime(&curr_samples, &m_granule_pos);
            assert(SUCCEEDED(hr));

            hr = pSample->SetTime(&curr_reftime, &m_reftime);
            assert(SUCCEEDED(hr));
        }

        m_packets.pop_front();
        assert(m_packets.empty());
    }

    return S_OK;
}


HRESULT OggTrackAudio::PopulateSample(
    const OggStream::Packet& pkt,
    IMediaSample* pSample) const
{
    assert(pSample);

    const long len = pkt.GetLength();
    assert(len > 0);

    BYTE* ptr;

    HRESULT hr = pSample->GetPointer(&ptr);
    assert(SUCCEEDED(hr));
    assert(ptr);

    const long size = pSample->GetSize();
    size;
    assert(size >= len);

    const long result = pkt.Copy(m_pStream->m_pReader, ptr);
    result;
    assert(result == len);
    assert((*ptr & 0x01) == 0);

    hr = pSample->SetActualDataLength(len);
    assert(SUCCEEDED(hr));

    hr = pSample->SetDiscontinuity(m_bDiscontinuity ? TRUE : FALSE);
    assert(SUCCEEDED(hr));

    //m_bDiscontinuity = false;

    hr = pSample->SetSyncPoint(TRUE);
    assert(SUCCEEDED(hr));

    hr = pSample->SetPreroll(FALSE);
    assert(SUCCEEDED(hr));

    //hr = pSample->SetMediaTime(&t.curr_samples, &t.next_samples);
    //assert(SUCCEEDED(hr));

    //hr = pSample->SetTime(&t.curr_reftime, &t.next_reftime);
    //assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT OggTrackAudio::PopulateSamplesVorbis2XiphLacing(const samples_t& ss)
{
    packets_t::size_type packet_count = m_packets.size();

    if (packet_count == 0)
        return E_FAIL;

    if (packet_count > 256)
        return E_FAIL;

    if (ss.size() != 1)
        return E_FAIL;

    if (m_granule_pos < 0)
        return E_FAIL;

    if (m_reftime < 0)
        return E_FAIL;

    const LONGLONG granule_pos = m_packets.back().granule_pos;

    if (granule_pos < 0)
        return E_FAIL;

    if (granule_pos <= m_granule_pos)
        return E_FAIL;

    IMediaSample* const pSample = ss.front();
    assert(pSample);

    BYTE* tgt_ptr;

    HRESULT hr = pSample->GetPointer(&tgt_ptr);
    assert(SUCCEEDED(hr));
    assert(tgt_ptr);

    const long tgt_size = pSample->GetSize();

    if (tgt_size <= 0)
        return E_FAIL;

    BYTE* const tgt_buf = tgt_ptr;
    BYTE* const tgt_end = tgt_buf + tgt_size;

    *tgt_ptr++ = static_cast<BYTE>(--packet_count);  //biased count

#if 0
    while (packet_count > 0)
    {
        assert(m_packets.size() > 1);
        const OggStream::Packet& pkt = m_packets.front();

        const long pkt_len = pkt.GetLength();

        if (pkt_len <= 0)
            return E_FAIL;

        long len = pkt_len;

        while (len >= 255)
        {
            if (tgt_ptr >= tgt_end)
                return E_FAIL;

            *tgt_ptr++ = 255;
            len -= 255;
        }

        {
            if (tgt_ptr >= tgt_end)
                return E_FAIL;

            *tgt_ptr++ = static_cast<BYTE>(len);
        }

        if ((tgt_ptr + pkt_len) >= tgt_end)
            return E_FAIL;

        const long result = pkt.Copy(m_pStream->m_pReader, tgt_ptr);
        result;
        assert(result == pkt_len);

        if (*tgt_ptr & 0x01)  //first byte of audio frame always even
            return E_FAIL;

        tgt_ptr += pkt_len;
        assert(tgt_ptr < tgt_end);

        m_packets.pop_front();
        assert(!m_packets.empty());

        --packet_count;
    }

    {
        assert(m_packets.size() == 1);
        const OggStream::Packet& pkt = m_packets.front();

        const long pkt_len = pkt.GetLength();

        if (pkt_len <= 0)
            return E_FAIL;

        if ((tgt_ptr + pkt_len) > tgt_end)
            return E_FAIL;

        const long result = pkt.Copy(m_pStream->m_pReader, tgt_ptr);
        result;
        assert(result == pkt_len);

        if (*tgt_ptr & 0x01)  //first byte for audio frame always even
            return E_FAIL;

        tgt_ptr += pkt_len;
        assert(tgt_ptr <= tgt_end);

        m_packets.pop_front();
        assert(m_packets.empty());
    }
#else
    typedef packets_t::iterator iter_t;

    iter_t iter = m_packets.begin();
    const iter_t iter_end = m_packets.end();

    while (packet_count > 0)
    {
        assert(iter != iter_end);
        const OggStream::Packet& pkt = *iter++;

        const long pkt_len = pkt.GetLength();

        if (pkt_len <= 0)
            return E_FAIL;

        long len = pkt_len;

        while (len >= 255)
        {
            if (tgt_ptr >= tgt_end)
                return E_FAIL;

            *tgt_ptr++ = 255;
            len -= 255;
        }

        {
            if (tgt_ptr >= tgt_end)
                return E_FAIL;

            *tgt_ptr++ = static_cast<BYTE>(len);
        }

        --packet_count;
    }

    assert(iter != iter_end);
    ++iter;
    assert(iter == iter_end);

    while (!m_packets.empty())
    {
        const OggStream::Packet& pkt = m_packets.front();

        const long pkt_len = pkt.GetLength();

        if (pkt_len <= 0)
            return E_FAIL;

        if ((tgt_ptr + pkt_len) > tgt_end)
            return E_FAIL;

        const long result = pkt.Copy(m_pStream->m_pReader, tgt_ptr);
        result;
        assert(result == pkt_len);

        if (*tgt_ptr & 0x01)  //first byte for audio frame always even
            return E_FAIL;

        tgt_ptr += pkt_len;
        assert(tgt_ptr <= tgt_end);

        m_packets.pop_front();
    }
#endif

    const long tgt_len = tgt_ptr - tgt_buf;
    assert(tgt_len <= tgt_size);

    hr = pSample->SetActualDataLength(tgt_len);
    assert(SUCCEEDED(hr));

    LONGLONG curr_samples = m_granule_pos;
    LONGLONG curr_reftime = m_reftime;

    m_granule_pos = granule_pos;  //next_samples

    const float next_samples_ = static_cast<float>(m_granule_pos);
    const float samples_per_sec = static_cast<float>(m_fmt.sample_rate);

    const float next_sec = next_samples_ / samples_per_sec;
    const float next_reftime_ = next_sec * 10000000.0F;

    m_reftime = static_cast<LONGLONG>(next_reftime_);  //next_reftime

    hr = pSample->SetMediaTime(&curr_samples, &m_granule_pos);
    assert(SUCCEEDED(hr));

    hr = pSample->SetTime(&curr_reftime, &m_reftime);
    assert(SUCCEEDED(hr));

    hr = pSample->SetDiscontinuity(m_bDiscontinuity ? TRUE : FALSE);
    assert(SUCCEEDED(hr));

    m_bDiscontinuity = false;

    hr = pSample->SetSyncPoint(TRUE);
    assert(SUCCEEDED(hr));

    hr = pSample->SetPreroll(FALSE);
    assert(SUCCEEDED(hr));

    return S_OK;
}

//REFERENCE_TIME OggTrackAudio::GetCurrTime() const
//{
//    assert(m_granule_pos >= 0);
//    assert(m_fmt.sample_rate > 0);
//
//    const double samples = static_cast<double>(m_granule_pos);
//    const double samples_per_sec = m_fmt.sample_rate;
//
//    const double sec = samples / samples_per_sec;
//
//    const double reftime_ = sec * 10000000.0;
//    const REFERENCE_TIME reftime = static_cast<REFERENCE_TIME>(reftime_);
//
//    return reftime;
//}


}  //end namespace WebmOggSource
