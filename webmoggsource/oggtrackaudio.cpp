#include <strmif.h>
#include "oggtrackaudio.hpp"
#include "vorbistypes.hpp"
#include "cmediatypes.hpp"
#include <new>
#include <cassert>
#include <uuids.h>

using namespace oggparser;

enum { cBuffers = 1024 };
enum { cbBuffer = 1024 };

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
    m_granule_pos(0)
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
}


HRESULT OggTrackAudio::QueryAccept(const AM_MEDIA_TYPE* pmt) const
{
    if (pmt == 0)
        return E_INVALIDARG;

    const AM_MEDIA_TYPE& mt = *pmt;

    if (mt.majortype != MEDIATYPE_Audio)
        return S_FALSE;

    if (mt.subtype != VorbisTypes::MEDIASUBTYPE_Vorbis2)
        return S_FALSE;

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


HRESULT OggTrackAudio::UpdateAllocatorProperties(
    ALLOCATOR_PROPERTIES& props) const
{
    if (props.cBuffers <= 0)
        props.cBuffers = cBuffers;

#if 0
    const long size = GetBufferSize();
    assert(size > 0);

    if (props.cbBuffer < size)
        props.cbBuffer = size;
#else
    if (props.cbBuffer < cbBuffer)
        props.cbBuffer = cbBuffer;
#endif

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


HRESULT OggTrackAudio::GetSampleCount(long& count)
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
        return E_FAIL;

    if (!m_packets.empty())
    {
        const OggStream::Packet& pkt = m_packets.back();

        if (pkt.granule_pos >= 0)
        {
            assert(pkt.granule_pos > m_granule_pos);

            count = m_packets.size();
            assert(count <= cBuffers);

            return S_OK;
        }
    }

#if 0
    count = 0;

    typedef packets_t::const_iterator iter_t;

    iter_t i = m_packets.begin();
    const iter_t j = m_packets.end();

    while (i != j)
    {
        ++count;

        const OggStream::Packet& pkt = *i;

        if (pkt.granule_pos >= 0)
        {
            assert(pkt.granule_pos > m_granule_pos);
            break;
        }
    }

    if (i != j)  //found an eligible packet
        return S_OK;

    assert(count == m_packets.size());
#endif

    for (;;)
    {
        OggStream::Packet pkt;

        while (m_pStream->GetPacket(pkt) < 1)  //not found
        {
            const long result = m_pStream->Parse();

            if (result < 0)  //error
            {
                if (result != oggparser::E_END_OF_FILE)
                    return E_FAIL;

                count = m_packets.size();
                assert(count <= cBuffers);

                return (count <= 0) ? S_FALSE : S_OK;
            }
        }

        assert(pkt.GetLength() <= cbBuffer);

        m_packets.push_back(pkt);

        if (pkt.granule_pos >= 0)
        {
            assert(pkt.granule_pos > m_granule_pos);

            count = m_packets.size();
            assert(count <= cBuffers);

            return S_OK;
        }
    }
}


HRESULT OggTrackAudio::PopulateSamples(const samples_t& ss)
{
    if (m_packets.empty())
        return E_FAIL;

    if (ss.size() != m_packets.size())
        return E_FAIL;

    if (m_granule_pos < 0)
        return E_FAIL;

    const LONGLONG granule_pos = m_packets.back().granule_pos;
    //assert(granule_pos > m_granule_pos);

    samples_t::const_iterator tgt_iter = ss.begin();

    {
        IMediaSample* const pSample = *tgt_iter++;
        assert(pSample);

        const OggStream::Packet& pkt = m_packets.front();

        HRESULT hr = PopulateSample(pkt, pSample);

        if (FAILED(hr))
            return hr;

        m_packets.pop_front();

        REFERENCE_TIME reftime = GetCurrTime();
        assert(reftime >= 0);

        hr = pSample->SetTime(&reftime, 0);
        assert(SUCCEEDED(hr));

        hr = pSample->SetDiscontinuity(m_bDiscontinuity ? TRUE : FALSE);
        assert(SUCCEEDED(hr));

        m_bDiscontinuity = false;
    }

    while (!m_packets.empty())
    {
        const OggStream::Packet& pkt = m_packets.front();
        IMediaSample* const pSample = *tgt_iter++;

        const HRESULT hr = PopulateSample(pkt, pSample);

        if (FAILED(hr))
            return hr;

        m_packets.pop_front();
    }

    assert(tgt_iter == ss.end());

    m_granule_pos = granule_pos;

    return S_OK;
}


HRESULT OggTrackAudio::PopulateSample(
    const OggStream::Packet& pkt,
    IMediaSample* pSample) const
{
    assert(pSample);

    const long len = pkt.GetLength();
    assert(len > 0);
    assert(len <= cbBuffer);

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

    hr = pSample->SetSyncPoint(TRUE);
    assert(SUCCEEDED(hr));

    hr = pSample->SetPreroll(FALSE);
    assert(SUCCEEDED(hr));

    hr = pSample->SetMediaTime(0, 0);
    assert(SUCCEEDED(hr));

    //discontinuity

    return S_OK;
}


REFERENCE_TIME OggTrackAudio::GetCurrTime() const
{
    assert(m_granule_pos >= 0);
    assert(m_fmt.sample_rate > 0);

    const double samples = static_cast<double>(m_granule_pos);
    const double samples_per_sec = m_fmt.sample_rate;

    const double sec = samples / samples_per_sec;

    const double reftime_ = sec * 10000000.0;
    const REFERENCE_TIME reftime = static_cast<REFERENCE_TIME>(reftime_);

    return reftime;
}


}  //end namespace WebmOggSource
