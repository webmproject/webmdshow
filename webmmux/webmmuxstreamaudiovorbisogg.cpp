// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "webmmuxcontext.hpp"
#include "webmmuxstreamaudiovorbisogg.hpp"
#include "vorbistypes.hpp"
#include "cmediatypes.hpp"
#include <cassert>
#include <uuids.h>
#include <vfwmsgs.h>
#ifdef _DEBUG
#include <odbgstream.hpp>
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

    const double samples = double(st);

    const ULONG samplesPerSec = pStream->GetSamplesPerSec();
    assert(samplesPerSec > 0);

    //secs [=] samples / samples/sec
    const double secs = samples / double(samplesPerSec);

    const double ns = secs * 1000000000.0;

    const Context& ctx = pStream->m_context;
    const ULONG scale = ctx.GetTimecodeScale();
    assert(scale >= 1);

    const double tc = ns / scale;
    assert(tc <= ULONG_MAX);

    m_timecode = static_cast<ULONG>(tc);

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


void StreamAudioVorbisOgg::Final()
{
    const HRESULT hr = FinalizeTrackCodecPrivate();
    hr;
    SUCCEEDED(hr);
}



void StreamAudioVorbisOgg::WriteTrackCodecID()
{
    EbmlIO::File& f = m_context.m_file;

    f.WriteID1(0x86);  //Codec ID
    f.Write1String("A_VORBIS");
}


void StreamAudioVorbisOgg::WriteTrackCodecName()
{
    EbmlIO::File& f = m_context.m_file;

    f.WriteID3(0x258688);  //Codec Name
    f.Write1UTF8(L"VORBIS");
}


void StreamAudioVorbisOgg::WriteTrackCodecPrivate()
{
    EbmlIO::File& file = m_context.m_file;

    m_codec_private_data_pos = file.GetPosition();

    file.SetPosition(kPRIVATE_DATA_BYTES_RESERVED, STREAM_SEEK_CUR);

#if 0 //def _DEBUG
    odbgstream ods;
    ods << "["__FUNCTION__"] " << "RESERVED SPACE! m_codec_private_data_pos="
        << m_codec_private_data_pos << " kPRIVATE_DATA_BYTES_RESERVED="
        << kPRIVATE_DATA_BYTES_RESERVED << std::endl;
#endif
}


HRESULT StreamAudioVorbisOgg::FinalizeTrackCodecPrivate()
{
    EbmlIO::File& file = m_context.m_file;

    const __int64 old_pos = file.GetPosition();
    file.SetPosition(m_codec_private_data_pos);

    if (m_ident.empty() || m_comment.empty() || m_setup.empty())
    {
        //debit allocated size by Void type (1) and length (2)
        const int bytes_to_write_ = kPRIVATE_DATA_BYTES_RESERVED - 1 - 2;
        assert(bytes_to_write_ <= USHRT_MAX);

        const USHORT bytes_to_write = static_cast<USHORT>(bytes_to_write_);

        file.WriteID1(0xEC); // Void
        file.Write2UInt(bytes_to_write);

        file.SetPosition(old_pos);

        return S_OK;
    }

    const DWORD ident_len = static_cast<const DWORD>(m_ident.size());
    assert(ident_len > 0);
    assert(ident_len <= 255);

    const DWORD comment_len = static_cast<const DWORD>(m_comment.size());
    assert(comment_len > 0);
    assert(comment_len <= 255);

    const DWORD setup_len = static_cast<const DWORD>(m_setup.size());
    assert(setup_len > 0);

    const DWORD hdr_len = ident_len + comment_len + setup_len;

    const ULONG len = 1 + 1 + 1 + hdr_len;
    //1 byte = number of headers - 1
    //1 byte = ident len
    //1 byte = comment len
    //(len of setup_len is implied by total len)

    file.WriteID2(0x63A2);  //Codec Private
    file.Write4UInt(len);

    BYTE val = 2;  //number of headers - 1
    file.Write(&val, 1);

    val = static_cast<BYTE>(ident_len);
    file.Write(&val, 1);

    val = static_cast<BYTE>(comment_len);
    file.Write(&val, 1);
    file.Write(&m_ident[0], ident_len);

    // TODO(tomfinegan): if |PRIVATE_DATA_BYTES_RESERVED| did not allow for
    // enough storage, we could write an empty comment header to avoid
    // trouble. (assuming that buys us enough space)
    file.Write(&m_comment[0], comment_len);
    file.Write(&m_setup[0], setup_len);

    // fill in any remaining space with a Void element
    const __int64& private_begin_pos = m_codec_private_data_pos;
    const __int64 private_end_pos =
        private_begin_pos + kPRIVATE_DATA_BYTES_RESERVED;

    if (file.GetPosition() < private_end_pos)
    {
        __int64 llbytes_to_write = private_end_pos - file.GetPosition();

        assert(llbytes_to_write >= 3); // 1 for id, 2 for len
        assert(llbytes_to_write < kPRIVATE_DATA_BYTES_RESERVED);

        USHORT bytes_to_write = static_cast<USHORT>(llbytes_to_write);

        bytes_to_write = bytes_to_write - 1 - 2; // Void type, length

        // create a void element and set its size (no need to fill)
        file.WriteID1(0xEC); // Void
        file.Write2UInt(bytes_to_write);
    }

    const __int64 actual_bytes_written =
        file.GetPosition() - private_begin_pos;

    actual_bytes_written;
    assert(actual_bytes_written <= kPRIVATE_DATA_BYTES_RESERVED);

    file.SetPosition(old_pos);

    return S_OK;
}


HRESULT StreamAudioVorbisOgg::Receive(IMediaSample* pSample)
{
    if (pSample == 0)
        return E_INVALIDARG;

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

        return S_OK;
    }

    EbmlIO::File& file = m_context.m_file;

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
