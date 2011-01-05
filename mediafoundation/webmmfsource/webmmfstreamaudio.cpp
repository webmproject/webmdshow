#include "webmmfsource.hpp"
#include "webmmfstream.hpp"
#include "webmmfstreamaudio.hpp"
#include "vorbistypes.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <cassert>
#include <cmath>
#include <comdef.h>
#include <vfwmsgs.h>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif

_COM_SMARTPTR_TYPEDEF(IMFStreamDescriptor, __uuidof(IMFStreamDescriptor));
_COM_SMARTPTR_TYPEDEF(IMFMediaType, __uuidof(IMFMediaType));
_COM_SMARTPTR_TYPEDEF(IMFMediaTypeHandler, __uuidof(IMFMediaTypeHandler));
_COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, __uuidof(IMFMediaBuffer));
_COM_SMARTPTR_TYPEDEF(IMFSample, __uuidof(IMFSample));


namespace WebmMfSourceLib
{


HRESULT WebmMfStreamAudio::CreateStreamDescriptor(
    const mkvparser::Track* pTrack_,
    IMFStreamDescriptor*& pDesc)
{
    assert(pTrack_);
    assert(pTrack_->GetType() == 2);  //audio

    typedef mkvparser::AudioTrack AT;
    const AT* const pTrack = static_cast<const AT*>(pTrack_);

    pDesc = 0;

    const char* const codec = pTrack->GetCodecId();

    if (codec == 0)  //weird
        return E_FAIL;

    if (_stricmp(codec, "A_VORBIS") != 0)  //weird
        return E_FAIL;

    const LONGLONG id_ = pTrack->GetNumber();
    const DWORD id = static_cast<DWORD>(id_);

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

    if (channels_ <= 0)  //weird
        return E_FAIL;

    const UINT32 channels = static_cast<UINT32>(channels_);

    hr = pmt->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
    assert(SUCCEEDED(hr));

    const double rate = pTrack->GetSamplingRate();

    if (rate <= 0)
        return E_FAIL;

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

    if ((cp == 0) || (cp_size == 0))
        return E_FAIL;

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

    const UINT32 cbBufSize = static_cast<UINT32>(cb);
    hr = pmt->SetBlob(MF_MT_USER_DATA, pb, cbBufSize);
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
    const mkvparser::Track* pTrack_,
    WebmMfStream*& pStream)
{
    assert(pTrack_);
    assert(pTrack_->GetType() == 2);

    typedef mkvparser::AudioTrack AT;
    const AT* const pTrack = static_cast<const AT*>(pTrack_);

    pStream = new (std::nothrow) WebmMfStreamAudio(pSource, pSD, pTrack);
    assert(pStream);  //TODO

    //TODO: handle time

    return pStream ? S_OK : E_OUTOFMEMORY;
}


WebmMfStreamAudio::WebmMfStreamAudio(
    WebmMfSource* pSource,
    IMFStreamDescriptor* pDesc,
    const mkvparser::AudioTrack* pTrack) :
    WebmMfStream(pSource, pDesc, pTrack),
    m_pCurr(0)
{
}


WebmMfStreamAudio::~WebmMfStreamAudio()
{
}


#if 0
void WebmMfStreamAudio::SetCurrBlock(const mkvparser::BlockEntry* pCurr)
{
    m_pCurr = pCurr;
}
#endif


HRESULT WebmMfStreamAudio::Seek(
    const PROPVARIANT& var,
    const mkvparser::BlockEntry* pCurr,
    bool bStart)
{
    m_bDiscontinuity = true;

    m_pCurr = pCurr;

    const int status = m_pSource->m_file.LockPage(m_pCurr);
    status;
    assert(status == 0);

    //m_pNextCluster = 0;
    m_pNextBlock = 0;

    if (bStart)
        return OnStart(var);
    else
        return OnSeek(var);
}


bool WebmMfStreamAudio::IsEOS() const
{
    return ((m_pCurr == 0) || m_pCurr->EOS());
}


HRESULT WebmMfStreamAudio::GetSample(IUnknown* pToken)
{
    assert(m_pCurr);
    assert(!m_pCurr->EOS());
    assert(m_pLocked);
    assert(m_pLocked == m_pCurr);

    HRESULT hr;

    if (m_pNextBlock == 0)
    {
        hr = GetNextBlock(m_pCurr);

        if (FAILED(hr))  //no next entry found on current cluster
            return hr;
    }

    IMFSamplePtr pSample;

    hr = MFCreateSample(&pSample);
    assert(SUCCEEDED(hr));  //TODO
    assert(pSample);

    if (pToken)
    {
        hr = pSample->SetUnknown(MFSampleExtension_Token, pToken);
        assert(SUCCEEDED(hr));
    }

    const mkvparser::Block* const pCurrBlock = m_pCurr->GetBlock();
    assert(pCurrBlock);
    assert(pCurrBlock->GetTrackNumber() == m_pTrack->GetNumber());

    const mkvparser::Cluster* const pCurrCluster = m_pCurr->GetCluster();
    assert(pCurrCluster);

    const __int64 curr_ns = pCurrBlock->GetTime(pCurrCluster);
    assert(curr_ns >= 0);

    const int frame_count = pCurrBlock->GetFrameCount();
    assert(frame_count > 0);  //TODO

    mkvparser::IMkvReader* const pReader = pCurrCluster->m_pSegment->m_pReader;

    for (int i = 0; i < frame_count; ++i)
    {
        const mkvparser::Block::Frame& f = pCurrBlock->GetFrame(i);

        const long cbBuffer = f.len;
        assert(cbBuffer > 0);

        IMFMediaBufferPtr pBuffer;

        hr = MFCreateMemoryBuffer(cbBuffer, &pBuffer);
        assert(SUCCEEDED(hr));
        assert(pBuffer);

        BYTE* ptr;
        DWORD cbMaxLength;

        hr = pBuffer->Lock(&ptr, &cbMaxLength, 0);
        assert(SUCCEEDED(hr));
        assert(ptr);
        assert(cbMaxLength >= DWORD(cbBuffer));

        const long status = f.Read(pReader, ptr);
        assert(status == 0);

        hr = pBuffer->SetCurrentLength(cbBuffer);
        assert(SUCCEEDED(hr));

        hr = pBuffer->Unlock();
        assert(SUCCEEDED(hr));

        hr = pSample->AddBuffer(pBuffer);
        assert(SUCCEEDED(hr));
    }

    hr = pSample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
    assert(SUCCEEDED(hr));

    if (m_bDiscontinuity)
    {
        //TODO: resolve whether to set this for first of the preroll samples,
        //or wait until last of preroll samples has been pushed downstream.

        hr = pSample->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
        assert(SUCCEEDED(hr));

        m_bDiscontinuity = false;  //TODO: must set back to true during a seek
    }

    const LONGLONG sample_time = curr_ns / 100;

    hr = pSample->SetSampleTime(sample_time);
    assert(SUCCEEDED(hr));

    //TODO: this might not be accurate, if there are gaps in the
    //audio blocks.  (How do we detect gaps?)

    const mkvparser::Segment* const pSegment = m_pTrack->m_pSegment;
    const mkvparser::SegmentInfo* const pInfo = pSegment->GetInfo();

    LONGLONG duration_ns = -1;  //duration of sample

    const mkvparser::BlockEntry* const pNextEntry = m_pNextBlock;  //TODO

    if ((pNextEntry != 0) && !pNextEntry->EOS())
    {
        const mkvparser::Block* const b = pNextEntry->GetBlock();
        assert(b);

        const mkvparser::Cluster* const c = pNextEntry->GetCluster();
        assert(c);

        const __int64 next_ns = b->GetTime(c);
        assert(next_ns >= curr_ns);

        duration_ns = (next_ns - curr_ns);
        assert(duration_ns >= 0);
    }
    else if (pInfo)
    {
        const LONGLONG next_ns = pInfo->GetDuration();

        if ((next_ns >= 0) && (next_ns > curr_ns))  //have duration
            duration_ns = next_ns - curr_ns;
    }

    if (duration_ns < 0)
    {
        const LONGLONG ns_per_frame = 10000000;  //10ms
        duration_ns = LONGLONG(frame_count) * ns_per_frame;
    }

    if (duration_ns >= 0)
    {
        const LONGLONG sample_duration = duration_ns / 100;
        assert(sample_duration >= 0);

        hr = pSample->SetSampleDuration(sample_duration);
        assert(SUCCEEDED(hr));
    }

    MkvReader& f = m_pSource->m_file;

    f.UnlockPage(m_pLocked);
    m_pLocked = 0;

    m_pCurr = m_pNextBlock;

    const int status = f.LockPage(m_pCurr);
    assert(status == 0);

    if (status == 0)
        m_pLocked = m_pCurr;

    m_pNextBlock = 0;

    return ProcessSample(pSample);
}


#if 0
HRESULT WebmMfStreamAudio::PopulateSample(IMFSample* pSample)
{
    assert(pSample);

    if ((m_pCurr == 0) || m_pCurr->EOS())
        return S_FALSE;

    const mkvparser::BlockEntry* pNextEntry;

    const long result = m_pTrack->GetNext(m_pCurr, pNextEntry);

    if (result == mkvparser::E_BUFFER_NOT_FULL)
        return VFW_E_BUFFER_UNDERFLOW;

    assert(result >= 0);
    assert(pNextEntry);

    const mkvparser::Block* const pCurrBlock = m_pCurr->GetBlock();
    assert(pCurrBlock);
    assert(pCurrBlock->GetTrackNumber() == m_pTrack->GetNumber());

    const mkvparser::Cluster* const pCurrCluster = m_pCurr->GetCluster();
    assert(pCurrCluster);

    const __int64 curr_ns = pCurrBlock->GetTime(pCurrCluster);
    assert(curr_ns >= 0);

    HRESULT hr;

    const int frame_count = pCurrBlock->GetFrameCount();
    assert(frame_count > 0);  //TODO

    mkvparser::IMkvReader* const pReader = pCurrCluster->m_pSegment->m_pReader;

    for (int i = 0; i < frame_count; ++i)
    {
        const mkvparser::Block::Frame& f = pCurrBlock->GetFrame(i);

        const long cbBuffer = f.len;
        assert(cbBuffer > 0);

        IMFMediaBufferPtr pBuffer;

        hr = MFCreateMemoryBuffer(cbBuffer, &pBuffer);
        assert(SUCCEEDED(hr));
        assert(pBuffer);

        BYTE* ptr;
        DWORD cbMaxLength;

        hr = pBuffer->Lock(&ptr, &cbMaxLength, 0);
        assert(SUCCEEDED(hr));
        assert(ptr);
        assert(cbMaxLength >= DWORD(cbBuffer));

        const long status = f.Read(pReader, ptr);
        assert(status == 0);

        hr = pBuffer->SetCurrentLength(cbBuffer);
        assert(SUCCEEDED(hr));

        hr = pBuffer->Unlock();
        assert(SUCCEEDED(hr));

        hr = pSample->AddBuffer(pBuffer);
        assert(SUCCEEDED(hr));
    }

    hr = pSample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
    assert(SUCCEEDED(hr));

    if (m_bDiscontinuity)
    {
        //TODO: resolve whether to set this for first of the preroll samples,
        //or wait until last of preroll samples has been pushed downstream.

        hr = pSample->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
        assert(SUCCEEDED(hr));

        m_bDiscontinuity = false;  //TODO: must set back to true during a seek
    }

    const LONGLONG sample_time = curr_ns / 100;

    hr = pSample->SetSampleTime(sample_time);
    assert(SUCCEEDED(hr));

    //TODO: this might not be accurate, if there are gaps in the
    //audio blocks.  (How do we detect gaps?)

    const mkvparser::Segment* const pSegment = m_pTrack->m_pSegment;
    const mkvparser::SegmentInfo* const pInfo = pSegment->GetInfo();

    LONGLONG duration_ns = -1;  //duration of sample

    if ((pNextEntry != 0) && !pNextEntry->EOS())
    {
        const mkvparser::Block* const b = pNextEntry->GetBlock();
        assert(b);

        const mkvparser::Cluster* const c = pNextEntry->GetCluster();
        assert(c);

        const __int64 next_ns = b->GetTime(c);
        assert(next_ns >= curr_ns);

        duration_ns = (next_ns - curr_ns);
        assert(duration_ns >= 0);
    }
    else if (pInfo)
    {
        const LONGLONG next_ns = pInfo->GetDuration();

        if ((next_ns >= 0) && (next_ns > curr_ns))  //have duration
            duration_ns = next_ns - curr_ns;
    }

    if (duration_ns < 0)
    {
        const LONGLONG ns_per_frame = 10000000;  //10ms
        duration_ns = LONGLONG(frame_count) * ns_per_frame;
    }

    if (duration_ns >= 0)
    {
        const LONGLONG sample_duration = duration_ns / 100;
        assert(sample_duration >= 0);

        hr = pSample->SetSampleDuration(sample_duration);
        assert(SUCCEEDED(hr));
    }

    MkvReader& f = m_pSource->m_file;

    f.UnlockPage(m_pCurr);
    m_pCurr = pNextEntry;
    f.LockPage(m_pCurr);

    return S_OK;
}
#endif


void WebmMfStreamAudio::OnStop()
{
    MkvReader& f = m_pSource->m_file;

    f.UnlockPage(m_pLocked);
    m_pLocked = 0;
}


const mkvparser::BlockEntry* WebmMfStreamAudio::GetCurrBlock() const
{
    return m_pCurr;
}


void WebmMfStreamAudio::SetRate(BOOL, float)
{
    //TODO
}


int WebmMfStreamAudio::LockCurrBlock()
{
    assert(m_pCurr);
    assert(!m_pCurr->EOS());
    assert(m_pLocked == 0);

    MkvReader& f = m_pSource->m_file;

    const int status = f.LockPage(m_pCurr);
    assert(status == 0);

    if (status)  //should never happen
        return status;

    m_pLocked = m_pCurr;
    return 0;  //succeeded
}

}  //end namespace WebmMfSourceLib
