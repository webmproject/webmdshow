#include "webmmfsource.hpp"
#include "webmmfstream.hpp"
#include "webmmfstreamaudio.hpp"
//#include "webmtypes.hpp"
#include "vorbistypes.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <cassert>
#include <cmath>
#include <comdef.h>

_COM_SMARTPTR_TYPEDEF(IMFStreamDescriptor, __uuidof(IMFStreamDescriptor));
_COM_SMARTPTR_TYPEDEF(IMFMediaType, __uuidof(IMFMediaType));
_COM_SMARTPTR_TYPEDEF(IMFMediaTypeHandler, __uuidof(IMFMediaTypeHandler));
_COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, __uuidof(IMFMediaBuffer));


namespace WebmMfSourceLib
{


HRESULT WebmMfStreamAudio::CreateStreamDescriptor(
    mkvparser::Track* pTrack_,
    IMFStreamDescriptor*& pDesc)
{
    assert(pTrack_);
    assert(pTrack_->GetType() == 2);  //audio

    using mkvparser::AudioTrack;
    AudioTrack* const pTrack = static_cast<AudioTrack*>(pTrack_);

    const char* const codec = pTrack->GetCodecId();
    assert(codec);

    if (_stricmp(codec, "A_VORBIS") != 0)  //weird
    {
        pDesc = 0;
        return E_FAIL;
    }

    const DWORD id = pTrack->GetNumber();

    IMFMediaTypePtr pmt;

    HRESULT hr = MFCreateMediaType(&pmt);
    assert(SUCCEEDED(hr));
    assert(pmt);

    hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    assert(SUCCEEDED(hr));

    hr = pmt->SetGUID(MF_MT_SUBTYPE, VorbisTypes::MEDIASUBTYPE_Vorbis2);
    assert(SUCCEEDED(hr));

    //TODO: set this too?
    //hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, FALSE);
    //assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_COMPRESSED, TRUE);
    assert(SUCCEEDED(hr));

    const __int64 channels_ = pTrack->GetChannels();
    assert(channels_ > 0);

    const UINT32 channels = static_cast<UINT32>(channels_);

    hr = pmt->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
    assert(SUCCEEDED(hr));

    const double rate = pTrack->GetSamplingRate();
    assert(rate > 0);

    hr = pmt->SetDouble(MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND, rate);
    assert(SUCCEEDED(hr));

    double int_rate_;

    const double frac_rate = modf(rate, &int_rate_);
    frac_rate;
    assert(frac_rate == 0);

    const UINT32 int_rate = static_cast<UINT32>(int_rate_);

    hr = pmt->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, int_rate);
    assert(SUCCEEDED(hr));

    const __int64 bits_per_sample_ = pTrack->GetBitDepth();
    UINT32 bits_per_sample;

    if (bits_per_sample_ <= 0)
        bits_per_sample = 0;
    else
    {
        assert((bits_per_sample_ % 8) == 0);
        bits_per_sample = static_cast<UINT32>(bits_per_sample_);

        hr = pmt->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bits_per_sample);
        assert(SUCCEEDED(hr));
    }

    size_t cp_size;
    const BYTE* const cp = pTrack->GetCodecPrivate(cp_size);
    assert(cp);
    assert(cp_size);

    const BYTE* const begin = cp;
    const BYTE* const end = begin + cp_size;

    const BYTE* p = begin;
    assert(p < end);

    const BYTE n = *p++;
    n;
    assert(n == 2);
    assert(p < end);

    const ULONG id_len = *p++;  //TODO: don't assume < 255
    assert(id_len < 255);
    assert(id_len > 0);
    assert(p < end);

    const ULONG comment_len = *p++;  //TODO: don't assume < 255
    assert(comment_len < 255);
    assert(comment_len > 0);
    assert(p < end);

    //p points to first header

    const BYTE* const id_hdr = p;
    id_hdr;

    const BYTE* const comment_hdr = id_hdr + id_len;
    comment_hdr;

    const BYTE* const setup_hdr = comment_hdr + comment_len;
    setup_hdr;
    assert(setup_hdr < end);

    const ptrdiff_t setup_len_ = end - setup_hdr;
    assert(setup_len_ > 0);

    const DWORD setup_len = static_cast<DWORD>(setup_len_);

    const size_t hdr_len = id_len + comment_len + setup_len;

    using VorbisTypes::VORBISFORMAT2;

    const size_t cb = sizeof(VORBISFORMAT2) + hdr_len;
    BYTE* const pb = (BYTE*)_malloca(cb);

    VORBISFORMAT2& fmt = (VORBISFORMAT2&)(*pb);

    fmt.channels = channels;
    fmt.samplesPerSec = int_rate;
    fmt.bitsPerSample = bits_per_sample;
    fmt.headerSize[0] = id_len;
    fmt.headerSize[1] = comment_len;
    fmt.headerSize[2] = setup_len;

    assert(p < end);
    assert(size_t(end - p) == hdr_len);

    BYTE* const dst = pb + sizeof(VORBISFORMAT2);
    memcpy(dst, p, hdr_len);

    //TODO: not sure whether this is correct;
    //following behavior described here:

    //See MFCreateMediaTypeFromRepresentation
    //ms-help://MS.VSCC.v90/MS.MSDNQTR.v90.en/medfound/html/
    //  5d85c47e-2e40-45f2-8f17-52f642652112.htm

    hr = pmt->SetGUID(MF_MT_AM_FORMAT_TYPE, VorbisTypes::FORMAT_Vorbis2);
    assert(SUCCEEDED(hr));

    hr = pmt->SetBlob(MF_MT_USER_DATA, pb, cb);
    assert(SUCCEEDED(hr));

    IMFMediaType* mtv[1] = { pmt };

    hr = MFCreateStreamDescriptor(id, 1, mtv, &pDesc);
    assert(SUCCEEDED(hr));
    assert(pDesc);

    IMFMediaTypeHandlerPtr ph;

    hr = pDesc->GetMediaTypeHandler(&ph);
    assert(SUCCEEDED(hr));
    assert(ph);

    hr = ph->SetCurrentMediaType(pmt);
    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfStreamAudio::CreateStream(
    IMFStreamDescriptor* pSD,
    WebmMfSource* pSource,
    mkvparser::Track* pTrack_,
    WebmMfStream*& pStream)
{
    assert(pTrack_);
    assert(pTrack_->GetType() == 2);

    using mkvparser::AudioTrack;
    AudioTrack* const pTrack = static_cast<AudioTrack*>(pTrack_);

    pStream = new (std::nothrow) WebmMfStreamAudio(pSource, pSD, pTrack);
    assert(pStream);  //TODO

    //TODO: handle time

    return pStream ? S_OK : E_OUTOFMEMORY;
}


WebmMfStreamAudio::WebmMfStreamAudio(
    WebmMfSource* pSource,
    IMFStreamDescriptor* pDesc,
    mkvparser::AudioTrack* pTrack) :
    WebmMfStream(pSource, pDesc, pTrack)
{
}


WebmMfStreamAudio::~WebmMfStreamAudio()
{
}


HRESULT WebmMfStreamAudio::OnPopulateSample(
    const mkvparser::BlockEntry* pNextEntry,
    IMFSample* pSample)
{
    assert(pSample);
    assert(m_pBaseCluster);
    assert(!m_pBaseCluster->EOS());
    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    const mkvparser::Block* const pCurrBlock = m_pCurr->GetBlock();
    assert(pCurrBlock);
    assert(pCurrBlock->GetTrackNumber() == m_pTrack->GetNumber());

    mkvparser::Cluster* const pCurrCluster = m_pCurr->GetCluster();
    assert(pCurrCluster);

    const __int64 curr_ns = pCurrBlock->GetTime(pCurrCluster);
    assert(curr_ns >= 0);
    //assert((start_ns % 100) == 0);

    const __int64 basetime_ns = m_pBaseCluster->GetFirstTime();
    assert(basetime_ns >= 0);
    //assert((basetime_ns % 100) == 0);
    assert(curr_ns >= basetime_ns);

    const long cbBuffer = pCurrBlock->GetSize();
    assert(cbBuffer >= 0);

    IMFMediaBufferPtr pBuffer;

    HRESULT hr = MFCreateMemoryBuffer(cbBuffer, &pBuffer);
    assert(SUCCEEDED(hr));
    assert(pBuffer);

    BYTE* ptr;
    DWORD cbMaxLength;

    hr = pBuffer->Lock(&ptr, &cbMaxLength, 0);
    assert(SUCCEEDED(hr));
    assert(ptr);
    assert(cbMaxLength >= DWORD(cbBuffer));

    mkvparser::IMkvReader* const pReader = pCurrCluster->m_pSegment->m_pReader;

    long status = pCurrBlock->Read(pReader, ptr);
    assert(status == 0);  //all bytes were read

    hr = pBuffer->SetCurrentLength(cbBuffer);
    assert(SUCCEEDED(hr));

    hr = pBuffer->Unlock();
    assert(SUCCEEDED(hr));

    hr = pSample->AddBuffer(pBuffer);
    assert(SUCCEEDED(hr));

#if 0  //TODO

    hr = pSample->SetPreroll(FALSE);
    assert(SUCCEEDED(hr));

#endif //TODO

    hr = pSample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
    assert(SUCCEEDED(hr));

    const LONGLONG sample_time = (curr_ns - basetime_ns) / 100;

    hr = pSample->SetSampleTime(sample_time);
    assert(SUCCEEDED(hr));

    //TODO: we can better here: synthesize duration of last block
    //in stream from the duration of the stream

    //TODO: this might not be accurate, if there are gaps in the
    //audio blocks.  (How do we detect gaps?)

    if ((pNextEntry != 0) && !pNextEntry->EOS())
    {
        const mkvparser::Block* const pNextBlock = pNextEntry->GetBlock();
        assert(pNextBlock);

        mkvparser::Cluster* const pNextCluster = pNextEntry->GetCluster();

        const __int64 next_ns = pNextBlock->GetTime(pNextCluster);
        assert(next_ns >= curr_ns);

        const LONGLONG sample_duration = (next_ns - curr_ns) / 100;

        hr = pSample->SetSampleDuration(sample_duration);
        assert(SUCCEEDED(hr));
    }

    return S_OK;
}


}  //end namespace WebmMfSourceLib
