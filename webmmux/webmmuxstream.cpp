// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "webmconstants.hpp"
#include "webmmuxstream.hpp"
#include "webmmuxcontext.hpp"
#include <cassert>
#include <climits>


namespace WebmMuxLib
{

Stream::Frame::Frame()
{
}


Stream::Frame::~Frame()
{
}


void Stream::Frame::Release()
{
   delete this;
}



Stream::Stream(Context& c) :
    m_context(c),
    m_trackNumber(0)
{
}


Stream::~Stream()
{
}


void Stream::Final()
{
}


void Stream::SetTrackNumber(int tn)
{
   m_trackNumber = tn;
}


int Stream::GetTrackNumber() const
{
   return m_trackNumber;
}


void Stream::WriteTrackEntry(int tn)
{
    WebmUtil::EbmlScratchBuf& entry_buf = m_context.m_buf;

    // we need the starting length of |entry_buf| to properly calculate
    // |num_bytes_to_ignore|
    const uint64 buf_start_len = entry_buf.GetBufferLength();

    entry_buf.WriteID1(WebmUtil::kEbmlTrackEntryID);

    // store entry offset for patching later
    const uint64 entry_len_offset = entry_buf.GetBufferLength();

    // We must exclude |num_bytes_to_ignore| from the size we obtain from
    // |entry_buf|.  The value from |entry_buf| includes the bytes storing
    // all preceding data in the track entry -- using it as-is would result
    // in an invalid tracks element.
    const uint64 num_bytes_to_ignore = buf_start_len + 1 + sizeof(uint16);

    // reserve 2 bytes for patching in the size...
    entry_buf.Serialize2UInt(0);

    WriteTrackNumber(tn);
    WriteTrackUID();
    WriteTrackType();
    WriteTrackName();
    WriteTrackCodecID();
    WriteTrackCodecPrivate();
    WriteTrackCodecName();
    WriteTrackSettings();

    const uint64 entry_len = entry_buf.GetBufferLength() - num_bytes_to_ignore;
    entry_buf.RewriteUInt(entry_len_offset, entry_len, sizeof(uint16));
}


void Stream::WriteTrackNumber(int tn_)
{
    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;

    assert(tn_ > 0);
    assert(tn_ < 128);

    m_trackNumber = tn_;
    const uint8 track_num = static_cast<uint8>(tn_);

    buf.WriteID1(WebmUtil::kEbmlTrackNumberID);
    buf.Write1UInt(1);
    buf.Serialize1UInt(track_num);
}


void Stream::WriteTrackUID()
{
    WebmUtil::EbmlScratchBuf& buf = m_context.m_buf;

    const TrackUID_t uid = CreateTrackUID();

    buf.WriteID2(WebmUtil::kEbmlTrackUIDID);
    buf.Write1UInt(8);
    buf.Serialize8UInt(uid);
}


void Stream::WriteTrackName()
{
}


void Stream::WriteTrackCodecPrivate()
{
}


void Stream::WriteTrackSettings()
{
}


Stream::TrackUID_t Stream::CreateTrackUID()
{
    TrackUID_t result;

    //TODO: Do we need to do this?
    //NOTE: The TrackUID is serialized in the normal way (the same
    //as for any other integer that is the payload of an EBML tag),
    //but the Matroska spec does say that this is an unsigned
    //integer.  In order to allow this integer value to be used
    //as an EBML varying-size integer, we restrict its value so
    //that it satifies the constraints for a varying size integer
    //that is streamed out using 8 bytes.  That means the upper
    //byte (the first in the stream) is 0 (the upper byte in the
    //stream is reserved for indicating that this integer
    //occupies 8 bytes total in the stream), and the low order
    //byte (the last in the stream) is even, which prevents
    //the integer from ever having a value with all bits set
    //(because then it would look like a signed integer).

    BYTE* p = reinterpret_cast<BYTE*>(&result);
    BYTE* const q = p + 7;

    {
        const int n = rand();

        BYTE& b0 = *p++;

        b0 = static_cast<BYTE>(n >> 4); //throw away low-order bits

        b0 &= 0xFE;  //ensure low order bit is not set
    }

    while (p != q)
    {
        const int n = rand();
        *p++ = static_cast<BYTE>(n >> 4); //throw away low-order bits
    }

    *p = 0;

    return result;
}


int Stream::Frame::GetLacing() const
{
    return 0;
}


ULONG Stream::Frame::GetBlockSize() const
{
    const ULONG result = 1 + 2 + 1 + GetSize();  //tn, tc, flg, f
    return result;
}

void Stream::Frame::WriteSimpleBlock(
    const Stream& s,
    ULONG cluster_tc) const
{
    WriteBlock(s, cluster_tc, true, GetBlockSize());  //SimpleBlock
}

void Stream::Frame::WriteBlockGroup(
    const Stream& s,
    ULONG cluster_tc,
    LONG prev_tc,
    ULONG duration) const
{
    EbmlIO::File& file = s.m_context.m_file;

    const ULONG block_size = GetBlockSize();
    ULONG block_group_size = 5 + block_size;

    const bool bKey = IsKey();

    if (!bKey)
        block_group_size += 1 + 1 + 2;

    if (duration > 0)
        block_group_size += 1 + 1 + 4;

    //begin block group

    file.WriteID1(WebmUtil::kEbmlBlockGroupID);
    file.WriteUInt(block_group_size);

#ifdef _DEBUG
    const __int64 pos = file.GetPosition();
#endif

    WriteBlock(s, cluster_tc, false, block_size);

    if (!bKey)
    {
        assert(prev_tc >= 0);

        const ULONG curr_tc = GetTimecode();
        assert(curr_tc <= LONG_MAX);

        const LONG tc = prev_tc - LONG(curr_tc);
        assert(tc < 0);
        assert(tc >= SHRT_MIN);

        const SHORT val = static_cast<SHORT>(tc);

        file.WriteID1(WebmUtil::kEbmlReferenceBlockID);
        file.Write1UInt(2);
        file.Serialize2SInt(val);
    }

    if (duration > 0)
    {
        file.WriteID1(WebmUtil::kEbmlBlockDurationID);
        file.Write1UInt(4);  //TODO: use min size
        file.Serialize4UInt(duration);
    }

    //end block group

#ifdef _DEBUG
    const __int64 newpos = file.GetPosition();
    assert((newpos - pos) == block_group_size);
#endif
}


void Stream::Frame::WriteBlock(
    const Stream& s,
    ULONG cluster_timecode,
    bool simple_block,
    ULONG block_size) const
{
    EbmlIO::File& file = s.m_context.m_file;

    //begin block

    const BYTE id = simple_block ? 0xA3 : 0xA1;  //SimpleBlock vs. Block

    file.WriteID1(id);
    file.WriteUInt(block_size);

#ifdef _DEBUG
    const __int64 pos = file.GetPosition();
#endif

    const int tn_ = s.GetTrackNumber();
    assert(tn_ > 0);
    assert(tn_ <= 255);

    const BYTE tn = static_cast<BYTE>(tn_);

    file.Write1UInt(tn);   //track number

    {
        const ULONG ft = GetTimecode();
        assert(ft <= LONG_MAX);

        const LONG tc_ = LONG(ft) - LONG(cluster_timecode);
        assert(tc_ >= SHRT_MIN);
        assert(tc_ <= SHRT_MAX);

        const SHORT tc = static_cast<SHORT>(tc_);

        file.Serialize2SInt(tc);       //relative timecode
    }

    BYTE flags = 0;

    if (simple_block & IsKey())
        flags |= BYTE(1 << 7);

    const int lacing = GetLacing();
    assert(lacing >= 0);
    assert(lacing <= 3);

    const BYTE fLacing = static_cast<BYTE>(lacing << 1);
    flags |= fLacing;

    file.Write(&flags, 1);   //written as binary, not uint

    file.Write(GetData(), GetSize());  //frame

    //end block

#ifdef _DEBUG
    const __int64 newpos = file.GetPosition();
    assert((newpos - pos) == block_size);
#endif
}


}  //end namespace WebmMuxLib
