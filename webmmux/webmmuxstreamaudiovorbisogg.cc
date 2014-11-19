// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include <uuids.h>
#include <vfwmsgs.h>

#include <cassert>

#include "cmediatypes.h"
#include "vorbistypes.h"
#include "webmconstants.h"
#include "webmmuxcontext.h"
#include "webmmuxstreamaudiovorbisogg.h"

#ifdef _DEBUG
#include <odbgstream.h>
#include <iomanip>
using std::endl;
using std::hex;
using std::dec;
using std::setfill;
using std::setw;
#endif

namespace WebmMuxLib
{

// |StreamAudioVorbisOgg::WriteTrackCodecPrivate| and
// |StreamAudioVorbisOgg::FinalizeTrackCodecPrivate| use
// kPRIVATE_DATA_BYTES_RESERVED to preallocate and write (respectively) the
// track's codec private data block.
enum { kPRIVATE_DATA_BYTES_RESERVED = 8000 };


void StreamAudioVorbisOgg::GetMediaTypes(CMediaTypes& mtv)
{
    AM_MEDIA_TYPE mt;

    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = VorbisTypes::MEDIASUBTYPE_Vorbis;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;
    mt.formattype = GUID_NULL;
    mt.pUnk = 0;
    mt.cbFormat = 0;
    mt.pbFormat = 0;

    mtv.Add(mt);
}


bool StreamAudioVorbisOgg::QueryAccept(const AM_MEDIA_TYPE& mt)
{
    if (mt.majortype != MEDIATYPE_Audio)
        return false;

    if (mt.formattype != VorbisTypes::FORMAT_Vorbis)
        return false;

    if (mt.pbFormat == 0)
        return false;

    using VorbisTypes::VORBISFORMAT;

    if (mt.cbFormat < sizeof(VORBISFORMAT))
        return false;

    const VORBISFORMAT& fmt = (VORBISFORMAT&)(*mt.pbFormat);

    if (fmt.numChannels == 0)
        return false;

    if (fmt.samplesPerSec == 0)
        return false;

    return true;
}


StreamAudio* StreamAudioVorbisOgg::CreateStream(
    Context& ctx,
    const AM_MEDIA_TYPE& mt)
{
    assert(QueryAccept(mt));

    const BYTE* const pb = mt.pbFormat;
    const ULONG cb = mt.cbFormat;

    return new (std::nothrow) StreamAudioVorbisOgg(ctx, pb, cb);
}


#if 0
StreamAudioVorbisOgg::VorbisFrame::VorbisFrame(
    IMediaSample* pSample,
    ULONG sample_rate,
    ULONG timecode_scale) :
    m_pSample(pSample),
    m_timecode(CalculateTimecode(pSample, sample_rate, timecode_scale))
{
    m_pSample->AddRef();
}


ULONG StreamAudioVorbisOgg::VorbisFrame::CalculateTimecode(
    IMediaSample* pSample,
    ULONG sample_rate,
    ULONG timecode_scale)
{
    ULONG timecode = 0;

    assert(pSample);

    REFERENCE_TIME sampletime_start, sampletime_end;

    HRESULT status = pSample->GetTime(&sampletime_start, &sampletime_end);
    assert(SUCCEEDED(status));

    double time_in_seconds = static_cast<double>(sampletime_start);

    const double dsample_rate = sample_rate;
    time_in_seconds /= dsample_rate;

    const double nanoseconds_per_second = 1000000000.0; // 10^9

    const double time_in_nanoseconds =
        time_in_seconds * nanoseconds_per_second;

    const double dtimecode_scale = timecode_scale;
    assert(timecode_scale > 0);

    const double dtimecode = time_in_nanoseconds / dtimecode_scale;
    assert(timecode <= ULONG_MAX);

    timecode = static_cast<ULONG>(dtimecode);

#if 0 //def _DEBUG
    odbgstream dbg;
    dbg << "["__FUNCTION__"] "
        << " sampletime_start=" << sampletime_start
        << " sampletime_end=" << sampletime_end
        << " duration[samples]=" << sampletime_end - sampletime_start
        << endl
        << "   timecode=" << timecode
        << " time[seconds]=" << time_in_seconds
        << " time[nanoseconds]=" << time_in_nanoseconds
        << endl;
#endif

    return timecode;
}


StreamAudioVorbisOgg::VorbisFrame::~VorbisFrame()
{
    const ULONG n = m_pSample->Release();
    n;
}
#else
StreamAudioVorbisOgg::VorbisFrame::VorbisFrame(
    IMediaSample* pSample,
    StreamAudioVorbisOgg* pStream)
{
    __int64 st, sp;  //this is actually samples, not reftime

    HRESULT hr = pSample->GetTime(&st, &sp);
    assert(SUCCEEDED(hr));
    assert(st >= 0);

    double samples = double(st);

    const ULONG samples_per_sec_ = pStream->GetSamplesPerSec();
    assert(samples_per_sec_ > 0);

    const double samples_per_sec = double(samples_per_sec_);

    //secs [=] samples / samples/sec
    double secs = samples / samples_per_sec;

    double ns = secs * 1000000000.0;

    const Context& ctx = pStream->m_context;
    const ULONG scale = ctx.GetTimecodeScale();
    assert(scale >= 1);

    double tc = ns / scale;
    assert(tc <= ULONG_MAX);

    m_timecode = static_cast<ULONG>(tc);

    if ((hr == VFW_S_NO_STOP_TIME) || (sp <= st))
        m_duration = 0;
    else
    {
        samples = double(sp - st);
        secs = samples / samples_per_sec;
        ns = secs * 1000000000.0;
        tc = ns / scale;
        m_duration = static_cast<ULONG>(tc);
    }

    const long size = pSample->GetActualDataLength();
    assert(size > 0);

    m_size = size;

    BYTE* ptr;

    hr = pSample->GetPointer(&ptr);
    assert(SUCCEEDED(hr));
    assert(ptr);

    m_data = new (std::nothrow) BYTE[m_size];
    assert(m_data);  //TODO

    memcpy(m_data, ptr, m_size);
}


StreamAudioVorbisOgg::VorbisFrame::~VorbisFrame()
{
    delete[] m_data;
}
#endif


const VorbisTypes::VORBISFORMAT&
StreamAudioVorbisOgg::GetFormat() const
{
    using VorbisTypes::VORBISFORMAT;

    ULONG cb;
    const void* const pv = StreamAudio::GetFormat(cb);
    assert(pv);
    assert(cb >= sizeof(VORBISFORMAT));

    const VORBISFORMAT* const pfmt = static_cast<const VORBISFORMAT*>(pv);
    assert(pfmt);

    const VORBISFORMAT& fmt = *pfmt;
    return fmt;
}


ULONG StreamAudioVorbisOgg::GetSamplesPerSec() const
{
    const VorbisTypes::VORBISFORMAT& fmt = GetFormat();
    assert(fmt.samplesPerSec > 0);

    return fmt.samplesPerSec;
}



BYTE StreamAudioVorbisOgg::GetChannels() const
{
    const VorbisTypes::VORBISFORMAT& fmt = GetFormat();
    assert(fmt.numChannels > 0);

    return fmt.numChannels;
}


ULONG StreamAudioVorbisOgg::VorbisFrame::GetTimecode() const
{
    return m_timecode;
}


ULONG StreamAudioVorbisOgg::VorbisFrame::GetDuration() const
{
    return m_duration;
}


ULONG StreamAudioVorbisOgg::VorbisFrame::GetSize() const
{
#if 0
   const long result = m_pSample->GetActualDataLength();
   assert(result >= 0);

   return result;
#else
    return m_size;
#endif
}


const BYTE* StreamAudioVorbisOgg::VorbisFrame::GetData() const
{
#if 0
    BYTE* ptr;

    const HRESULT hr = m_pSample->GetPointer(&ptr);
    assert(SUCCEEDED(hr));
    assert(ptr);

    return ptr;
#else
    return m_data;
#endif
}


StreamAudioVorbisOgg::StreamAudioVorbisOgg(
    Context& ctx,
    const BYTE* pb,
    ULONG cb) :
    StreamAudio(ctx, pb, cb)
{
}

void StreamAudioVorbisOgg::WriteTrackCodecID()
{
    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;

    buf.WriteID1(WebmUtil::kEbmlCodecIDID);
    buf.Write1String("A_VORBIS");
}


void StreamAudioVorbisOgg::WriteTrackCodecName()
{
    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;

    buf.WriteID3(WebmUtil::kEbmlCodecNameID);
    buf.Write1UTF8(L"VORBIS");
}


void StreamAudioVorbisOgg::WriteTrackCodecPrivate()
{
    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;

    // Note: this enables buffering for all streams!!
    m_context.BufferData();
    m_codec_private_data_pos = buf.GetBufferLength();

    const uint16 size = kPRIVATE_DATA_BYTES_RESERVED - 3;

    buf.WriteID1(WebmUtil::kEbmlVoidID);
    buf.Write2UInt(size);
    buf.Fill(0, size);
}


HRESULT StreamAudioVorbisOgg::FinalizeTrackCodecPrivate()
{
    if (m_ident.empty() || m_comment.empty() || m_setup.empty())
        return S_OK;

    const uint32 ident_len = static_cast<const uint32>(m_ident.size());
    assert(ident_len > 0);
    assert(ident_len <= 255);

    const uint32 comment_len = static_cast<const uint32>(m_comment.size());
    assert(comment_len > 0);
    assert(comment_len <= 255);

    const uint32 setup_len = static_cast<const uint32>(m_setup.size());
    assert(setup_len > 0);

    const uint32 hdr_len = ident_len + comment_len + setup_len;

    // 1 byte to store header count (total headers - 1)
    // 1 byte each for vorbis ident and comment headers
    // - len of setup_len is implied by total len
    const uint32 codec_private_length = 1 + 1 + 1 + hdr_len;

    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;
    uint64 rewrite_offset = m_codec_private_data_pos;

    rewrite_offset += buf.RewriteID(rewrite_offset,
                                    WebmUtil::kEbmlCodecPrivateID,
                                    sizeof(uint16));
    rewrite_offset += buf.RewriteUInt(rewrite_offset, codec_private_length,
                                      sizeof(uint32));

    uint8 val = 2;  //number of headers - 1
    rewrite_offset += buf.Rewrite(rewrite_offset, &val, sizeof(uint8));

    // write ident length
    val = static_cast<uint8>(ident_len);
    rewrite_offset += buf.Rewrite(rewrite_offset, &val, sizeof(uint8));

    // write comment length
    val = static_cast<uint8>(comment_len);
    rewrite_offset += buf.Rewrite(rewrite_offset, &val, sizeof(uint8));

    // write ident data
    rewrite_offset += buf.Rewrite(rewrite_offset, &m_ident[0], ident_len);

    // write comment data
    rewrite_offset += buf.Rewrite(rewrite_offset, &m_comment[0], comment_len);

    // write vorbis decoder setup data
    rewrite_offset += buf.Rewrite(rewrite_offset, &m_setup[0], setup_len);

    // Fill any remaining reserved space with a proper EBML Void element
    const uint64 private_len = rewrite_offset - m_codec_private_data_pos;
    if (private_len < kPRIVATE_DATA_BYTES_RESERVED)
    {
        uint64 void_len = kPRIVATE_DATA_BYTES_RESERVED - private_len;

        // assert that we have more than 3 bytes remaining-- I don't know how
        // parsers will handle a void element with a size of 0 and no contents
        assert(void_len > 3); // 3 = kEbmlVoidID + uint16

        // subtract void element id (1) and element size (2)
        void_len -= 3;

        rewrite_offset += buf.RewriteID(rewrite_offset, WebmUtil::kEbmlVoidID,
                                        sizeof(uint8));
        rewrite_offset += buf.RewriteUInt(rewrite_offset, void_len,
                                          sizeof(uint16));
        const uint8 fill_val = 0;
        for (int i = 0; i < void_len; ++i)
        {
            rewrite_offset += buf.Rewrite(rewrite_offset, &fill_val,
                                          sizeof(uint8));
        }

        const uint64 private_end =
            kPRIVATE_DATA_BYTES_RESERVED + m_codec_private_data_pos;
        assert(rewrite_offset == private_end);
        private_end;
    }

    // Normal file writing can proceed; flush the data we've had Context
    // buffering now that we've written the vorbis decoder setup data.
    m_context.FlushBufferedData();

    return S_OK;
}


HRESULT StreamAudioVorbisOgg::Receive(IMediaSample* pSample)
{
    if (pSample == 0)
        return E_INVALIDARG;

    EbmlIO::File& file = m_context.m_file;

    BYTE* buf;

    HRESULT hr = pSample->GetPointer(&buf);
    assert(SUCCEEDED(hr));
    assert(buf);

    const long len = pSample->GetActualDataLength();

    if (len < 0)
        return E_INVALIDARG;

    if (len == 0)
        return S_OK;  //?

    BYTE* const buf_end = buf + len;

    if (m_ident.empty())
    {
        assert(len >= 7);
        assert(buf[0] == 1);
        assert(memcmp(buf + 1, "vorbis", 6) == 0);
        m_ident.assign(buf, buf_end);

        return S_OK;
    }

    if (m_comment.empty())
    {
        assert(len >= 7);
        assert(buf[0] == 3);
        assert(memcmp(buf + 1, "vorbis", 6) == 0);
        m_comment.assign(buf, buf_end);

        return S_OK;
    }

    if (m_setup.empty())
    {
        assert(len >= 7);
        assert(buf[0] == 5);
        assert(memcmp(buf + 1, "vorbis", 6) == 0);
        m_setup.assign(buf, buf_end);

        if (file.GetStream())
        {
            const HRESULT hr = FinalizeTrackCodecPrivate();
            hr;
            SUCCEEDED(hr);
        }

        return S_OK;
    }

    if (file.GetStream() == 0)
        return S_OK;

    //In order to construct a frame, we need to have
    //both the start and stop times, so we check it
    //here before calling the ctor.

    __int64 st, sp;

    hr = pSample->GetTime(&st, &sp);

    if (FAILED(hr))
        return VFW_E_SAMPLE_TIME_NOT_SET;

    if (hr != S_OK)  //do not have stop time
        return E_INVALIDARG;

    if (st >= sp)
        return S_OK;  //throw away this sample

    VorbisFrame* const pFrame = new (std::nothrow) VorbisFrame(pSample, this);
    assert(SUCCEEDED(hr));  //TODO

    m_context.NotifyAudioFrame(this, pFrame);

    return S_OK;
}


int StreamAudioVorbisOgg::EndOfStream()
{
    return m_context.NotifyAudioEOS(this);
}

}  //end namespace WebmMuxLib
