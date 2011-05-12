// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "webmconstants.hpp"
#include "webmmuxcontext.hpp"
#include "webmmuxstreamaudiovorbis.hpp"
#include "vorbistypes.hpp"
#include "cmediatypes.hpp"
#include <cassert>
#include <uuids.h>
#include <vfwmsgs.h>
#if 0 //def _DEBUG
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


void StreamAudioVorbis::GetMediaTypes(CMediaTypes& mtv)
{
    AM_MEDIA_TYPE mt;

    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = VorbisTypes::MEDIASUBTYPE_Vorbis2;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;
    mt.formattype = GUID_NULL;
    mt.pUnk = 0;
    mt.cbFormat = 0;
    mt.pbFormat = 0;

    mtv.Add(mt);

    mt.subtype = VorbisTypes::MEDIASUBTYPE_Vorbis2_Xiph_Lacing;
    mtv.Add(mt);
}


bool StreamAudioVorbis::QueryAccept(const AM_MEDIA_TYPE& mt)
{
    if (mt.majortype != MEDIATYPE_Audio)
        return false;

    if (mt.subtype == VorbisTypes::MEDIASUBTYPE_Vorbis2)
        __noop;
    else if (mt.subtype == VorbisTypes::MEDIASUBTYPE_Vorbis2_Xiph_Lacing)
        __noop;
    else
        return false;

    if (mt.formattype != VorbisTypes::FORMAT_Vorbis2)
        return false;

    using VorbisTypes::VORBISFORMAT2;

    if (mt.cbFormat < sizeof(VORBISFORMAT2))
        return false;

    if (mt.pbFormat == 0)
        return false;

    const VORBISFORMAT2& fmt = (VORBISFORMAT2&)(*mt.pbFormat);

    if (fmt.channels == 0)
        return false;

    if (fmt.channels > 255)
        return false;

    if (fmt.samplesPerSec == 0)
        return false;

    //TODO: fmt.dwBitsPerSample

    const DWORD ident_len = fmt.headerSize[0];

    if (ident_len != 30)
        return false;

    const DWORD comments_len = fmt.headerSize[1];

    if (comments_len == 0)  //TODO: req'd?
        return false;

    const DWORD setup_len = fmt.headerSize[2];

    if (setup_len == 0)
        return false;

    const DWORD hdr_len = ident_len + comments_len + setup_len;
    const DWORD cbFormat = sizeof(VORBISFORMAT2) + hdr_len;

    if (mt.cbFormat < cbFormat)
        return false;

    return true;
}


#if 0
HRESULT StreamAudioVorbis::GetAllocatorRequirements(
    const AM_MEDIA_TYPE&,
    ALLOCATOR_PROPERTIES&)
{
    //Here we assume a Vorbis audio packet is 20ms in duration,
    //which makes 50 packets per one second of audio.  We then buffer
    //3 seconds worth of audio.
    //TODO: determine what the duration of a Vorbis packet actually is.
    //UPDATE: James Z. said that Vorbis packets have powers-of-two
    //number of samples, from 64 - 8192.  The number of buffers
    //could be calculated as:
    //   number of buffers [=] samples/sec / 64 samples/buffer
    //which is about 750 buffers/sec.  We could either choose to
    //fix the number of buffers we allocated, which determines
    //how much we're able to buffer, or fix how much we want to
    //buffer, and then copy the audio data from the media samples
    //into a local buffer.  (The latter approach is less likely
    //to stall the graph, but it's not as efficient.)

    props.cBuffers = 3 * 50;
    props.cbBuffer = 0;
    props.cbAlign = 0;
    props.cbPrefix = 0;

    return S_OK;
}
#endif


StreamAudio* StreamAudioVorbis::CreateStream(
    Context& ctx,
    const AM_MEDIA_TYPE& mt)
{
    assert(QueryAccept(mt));
    return new (std::nothrow) StreamAudioVorbis(ctx, mt);
}



#if 0
StreamAudioVorbis::VorbisFrame::VorbisFrame(
    IMediaSample* pSample,
    StreamAudioVorbis* pStream) :
    m_pSample(pSample)
{
    m_pSample->AddRef();

    __int64 st, sp;  //reftime units

    const HRESULT hr = m_pSample->GetTime(&st, &sp);
    assert(SUCCEEDED(hr));
    assert(st >= 0);

    const __int64 ns = st * 100;  //nanoseconds

    const Context& ctx = pStream->m_context;
    const ULONG scale = ctx.GetTimecodeScale();
    assert(scale >= 1);

    //TODO: verify this when scale equals audio sampling rate
    const __int64 tc = ns / scale;
    assert(tc <= ULONG_MAX);

    m_timecode = static_cast<ULONG>(tc);
}
#else
StreamAudioVorbis::VorbisFrame::VorbisFrame(
    IMediaSample* pSample,
    StreamAudioVorbis* pStream)
{
    __int64 st, sp;  //reftime units

    HRESULT hr = pSample->GetTime(&st, &sp);
    assert(SUCCEEDED(hr));
    assert(st >= 0);

    __int64 ns = st * 100;  //nanoseconds

    const Context& ctx = pStream->m_context;
    const ULONG scale = ctx.GetTimecodeScale();
    assert(scale >= 1);

    //TODO: verify this when scale equals audio sampling rate
    __int64 tc = ns / scale;
    assert(tc <= ULONG_MAX);

    m_timecode = static_cast<ULONG>(tc);

    if ((hr == VFW_S_NO_STOP_TIME) || (sp <= st))
        m_duration = 0;
    else
    {
        ns = (sp - st) * 100;  //duration (ns units)
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

    const GUID& g = pStream->m_subtype;

    if (g == VorbisTypes::MEDIASUBTYPE_Vorbis2_Xiph_Lacing)
        m_lacing = 1;
    else
        m_lacing = 0;
}
#endif


StreamAudioVorbis::VorbisFrame::~VorbisFrame()
{
#if 0
    const ULONG n = m_pSample->Release();
    n;
#else
    delete[] m_data;
#endif
}


const VorbisTypes::VORBISFORMAT2&
StreamAudioVorbis::GetFormat() const
{
    using VorbisTypes::VORBISFORMAT2;

    ULONG cb;
    const void* const pv = StreamAudio::GetFormat(cb);
    assert(pv);
    assert(cb >= sizeof(VORBISFORMAT2));

    const VORBISFORMAT2* const pfmt = static_cast<const VORBISFORMAT2*>(pv);
    assert(pfmt);

    const VORBISFORMAT2& fmt = *pfmt;
    return fmt;
}


int StreamAudioVorbis::VorbisFrame::GetLacing() const
{
    return m_lacing;
}


ULONG StreamAudioVorbis::GetSamplesPerSec() const
{
    const VorbisTypes::VORBISFORMAT2& fmt = GetFormat();
    assert(fmt.samplesPerSec > 0);

    return fmt.samplesPerSec;
}


BYTE StreamAudioVorbis::GetChannels() const
{
    const VorbisTypes::VORBISFORMAT2& fmt = GetFormat();
    assert(fmt.channels > 0);
    assert(fmt.channels <= 255);

    return static_cast<BYTE>(fmt.channels);
}



ULONG StreamAudioVorbis::VorbisFrame::GetTimecode() const
{
    return m_timecode;
}


ULONG StreamAudioVorbis::VorbisFrame::GetDuration() const
{
    return m_duration;
}


ULONG StreamAudioVorbis::VorbisFrame::GetSize() const
{
#if 0
   const long result = m_pSample->GetActualDataLength();
   assert(result >= 0);

   return result;
#else
    return m_size;
#endif
}


const BYTE* StreamAudioVorbis::VorbisFrame::GetData() const
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


StreamAudioVorbis::StreamAudioVorbis(
    Context& ctx,
    const AM_MEDIA_TYPE& mt) :
    StreamAudio(ctx, mt.pbFormat, mt.cbFormat),
    m_subtype(mt.subtype)
{
}


void StreamAudioVorbis::WriteTrackCodecID()
{
    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;

    buf.WriteID1(WebmUtil::kEbmlCodecIDID);
    buf.Write1String("A_VORBIS");
}


void StreamAudioVorbis::WriteTrackCodecName()
{
    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;

    buf.WriteID3(WebmUtil::kEbmlCodecNameID);
    buf.Write1UTF8(L"VORBIS");
}


void StreamAudioVorbis::WriteTrackCodecPrivate()
{
    typedef VorbisTypes::VORBISFORMAT2 F;

    ULONG cb;
    const void* const pv = StreamAudio::GetFormat(cb);
    assert(cb >= sizeof(F));
    assert(pv);

    const F* const pf = static_cast<const F*>(pv);
    const F& f = *pf;
    assert(f.channels > 0);
    assert(f.samplesPerSec > 0);

    const DWORD ident_len = f.headerSize[0];
    assert(ident_len == 30);

    const DWORD comment_len = f.headerSize[1];
    assert(comment_len > 0);

    const DWORD setup_len = f.headerSize[2];
    assert(setup_len > 0);

    const DWORD hdr_len = ident_len + comment_len + setup_len;
    assert(cb == (sizeof(F) + hdr_len));

    const BYTE* const pb = static_cast<const BYTE*>(pv);
    const BYTE* const hdr_ptr = pb + sizeof(F);

    ULONG len = 1 + 1;
    //1 byte = number of headers - 1 (always 2)
    //1 byte = ident len (always 30)

    ULONG n = comment_len;
    while (n >= 255)
    {
        ++len;
        n -= 255;
    }

    ++len;
    len += hdr_len;

    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;
    buf.WriteID2(WebmUtil::kEbmlCodecPrivateID);
    buf.WriteUInt(len, 0);

    uint8 val = 2;  //number of headers - 1
    buf.Write(&val, 1);

    val = static_cast<uint8>(ident_len);
    buf.Write(&val, 1);

    n = comment_len;
    while (n >= 255)
    {
        val = 255;
        buf.Write(&val, 1);

        n -= 255;
    }

    val = static_cast<uint8>(n);
    buf.Write(&val, 1);

    buf.Write(hdr_ptr, hdr_len);
}


HRESULT StreamAudioVorbis::Receive(IMediaSample* pSample)
{
    assert(pSample);

    EbmlIO::File& file = m_context.m_file;

    if (file.GetStream() == 0)
        return S_OK;

    VorbisFrame* const pFrame = new (std::nothrow) VorbisFrame(pSample, this);
    assert(pFrame);  //TODO

    m_context.NotifyAudioFrame(this, pFrame);

    return S_OK;
}


int StreamAudioVorbis::EndOfStream()
{
    return m_context.NotifyAudioEOS(this);
}


}  //end namespace WebmMuxLib
