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
#include "webmmuxstreamvideovpx.hpp"
#include "webmtypes.hpp"
#include <climits>
#include <cassert>
#include <vfwmsgs.h>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
using std::boolalpha;
using std::hex;
using std::dec;
#endif

namespace WebmMuxLib
{

StreamVideoVPx::VPxFrame::VPxFrame(
    IMediaSample* pSample,
    StreamVideoVPx* pStream) :
    m_pSample(pSample)
{
    assert(m_pSample);
    m_pSample->AddRef();

    __int64 st, sp;  //reftime units

    const HRESULT hr = m_pSample->GetTime(&st, &sp);
    assert(SUCCEEDED(hr));
    assert(st >= 0);

    __int64 ns = st * 100;  //nanoseconds

    const Context& ctx = pStream->m_context;
    const ULONG scale = ctx.GetTimecodeScale();
    assert(scale >= 1);

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
}


StreamVideoVPx::VPxFrame::~VPxFrame()
{
    const ULONG n = m_pSample->Release();
    n;
}


ULONG StreamVideoVPx::VPxFrame::GetTimecode() const
{
    return m_timecode;
}


ULONG StreamVideoVPx::VPxFrame::GetDuration() const
{
    return m_duration;
}


bool StreamVideoVPx::VPxFrame::IsKey() const
{
    return (m_pSample->IsSyncPoint() == S_OK);
}


ULONG StreamVideoVPx::VPxFrame::GetSize() const
{
    const long result = m_pSample->GetActualDataLength();
    assert(result >= 0);

    return result;
}


const BYTE* StreamVideoVPx::VPxFrame::GetData() const
{
    BYTE* ptr;

    const HRESULT hr = m_pSample->GetPointer(&ptr);
    assert(SUCCEEDED(hr));
    assert(ptr);

    return ptr;
}



StreamVideoVPx::StreamVideoVPx(
    Context& c,
    const AM_MEDIA_TYPE& mt) :
    StreamVideo(c, mt)
{
}


//void StreamVideoVPx::WriteTrackName()
//{
//    EbmlIO::File& f = m_context.m_file;
//
//    f.WriteID2(0x536E);   //name
//    f.Write1UTF8(L"VP8 video stream");  //TODO
//}



void StreamVideoVPx::WriteTrackCodecID()
{
    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;

    buf.WriteID1(WebmUtil::kEbmlCodecIDID);

    if (m_mt.subtype == WebmTypes::MEDIASUBTYPE_VP80)
        buf.Write1String("V_VP8");
    else
    {
        assert(m_mt.subtype == WebmTypes::MEDIASUBTYPE_VP90);
        buf.Write1String("V_VP9");
    }
}


void StreamVideoVPx::WriteTrackCodecName()
{
    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;

    buf.WriteID3(WebmUtil::kEbmlCodecNameID);

    if (m_mt.subtype == WebmTypes::MEDIASUBTYPE_VP80)
        buf.Write1UTF8(L"VP8");
    else
    {
        assert(m_mt.subtype == WebmTypes::MEDIASUBTYPE_VP90);
        buf.Write1UTF8(L"VP9");
    }
}


void StreamVideoVPx::WriteTrackSettings()
{
    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;

    // we need the starting length of |buf| to properly calculate
    // |num_bytes_to_ignore|
    const uint64 buf_start_len = buf.GetBufferLength();

    buf.WriteID1(WebmUtil::kEbmlVideoSettingsID);

    // store length offset for patching later
    const uint64 len_offset = buf.GetBufferLength();

    // We must exclude |num_bytes_to_ignore| from the size we obtain from
    // |buf|.  The value from |buf| includes the bytes storing all preceding
    // data in the track entry -- using it as-is would result in an invalid
    // tracks element.
    const uint64 num_bytes_to_ignore = buf_start_len + 1 + sizeof(uint16);

    // reserve 2 bytes for patching in the size...
    buf.Serialize2UInt(0);

    const BITMAPINFOHEADER& bmih = GetBitmapInfoHeader();
    assert(bmih.biSize >= sizeof(BITMAPINFOHEADER));
    assert(bmih.biWidth > 0);
    assert(bmih.biWidth <= kuint16max);
    assert(bmih.biHeight > 0);
    assert(bmih.biHeight <= kuint16max);

    const uint16 width = static_cast<uint16>(bmih.biWidth);
    const uint16 height = static_cast<uint16>(bmih.biHeight);

    buf.WriteID1(WebmUtil::kEbmlVideoWidth);
    buf.Write1UInt(2);
    buf.Serialize2UInt(width);

    buf.WriteID1(WebmUtil::kEbmlVideoHeight);
    buf.Write1UInt(2);
    buf.Serialize2UInt(height);

    const float framerate = GetFramerate();

    if (framerate > 0)
    {
        buf.WriteID3(WebmUtil::kEbmlVideoFrameRate);
        buf.Write1UInt(4);
        buf.Serialize4Float(framerate);
    }

    const uint64 video_len = buf.GetBufferLength() - num_bytes_to_ignore;
    buf.RewriteUInt(len_offset, video_len, sizeof(uint16));
}


HRESULT StreamVideoVPx::Receive(IMediaSample* pSample)
{
    assert(pSample);

#if 0
    __int64 st, sp;
    const HRESULT hrTime = pSample->GetTime(&st, &sp);

    odbgstream os;

    os << "webmmux::vpx::receive: hrTime="
       << hex << hrTime << dec;

    if (SUCCEEDED(hrTime))
    {
        os << " st=" << st
           << " st.ms=" << double(st) / 10000;

        if (hrTime == S_OK)
            os << " sp="
               << sp
               << " sp.ms="
               << double(sp) / 10000
               << " dt.ms="
               << (double(sp-st) / 10000);
    }

    //os << " frame.GetTimecode="
    //   << pFrame->GetTimecode();

    os << endl;
#endif

    EbmlIO::File& file = m_context.m_file;

    if (file.GetStream() == 0)
        return S_OK;

    VPxFrame* const pFrame = new (std::nothrow) VPxFrame(pSample, this);
    assert(pFrame);  //TODO

    assert(!m_vframes.empty() || pFrame->IsKey());

    m_vframes.push_back(pFrame);

    m_context.NotifyVideoFrame(this, pFrame);

    return S_OK;
}


int StreamVideoVPx::EndOfStream()
{
    return m_context.NotifyVideoEOS(this);
}


LONG StreamVideoVPx::GetLastTimecode() const
{
    if (m_vframes.empty())
        return -1;

    VideoFrame* const pFrame = m_vframes.back();
    assert(pFrame);

    return pFrame->GetTimecode();
}


}  //end namespace WebmMuxLib

