// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
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
    EbmlIO::File& f = m_context.m_file;

    f.WriteID1(0xAE);  //TrackEntry ID (level 2)

    //allocate 2 bytes for track entry size field
    const __int64 begin_pos = f.SetPosition(2, STREAM_SEEK_CUR);

    WriteTrackNumber(tn);
    WriteTrackUID();
    WriteTrackType();
    WriteTrackName();
    WriteTrackCodecID();
    WriteTrackCodecPrivate();
    WriteTrackCodecName();
    WriteTrackSettings();

    const __int64 end_pos = f.GetPosition();

    const __int64 size_ = end_pos - begin_pos;
    assert(size_ <= USHRT_MAX);

    const USHORT size = static_cast<USHORT>(size_);

    f.SetPosition(begin_pos - 2);
    f.Write2UInt(size);

    f.SetPosition(end_pos);
}


void Stream::WriteTrackNumber(int tn_)
{
    EbmlIO::File& f = m_context.m_file;

    assert(tn_ > 0);
    assert(tn_ < 128);

    m_trackNumber = tn_;

    const BYTE tn = static_cast<BYTE>(tn_);

    f.WriteID1(0xD7);     //track number ID  //1101 0111
    f.Write1UInt(1);
    f.Serialize1UInt(tn);
}


void Stream::WriteTrackUID()
{
    EbmlIO::File& f = m_context.m_file;

    const TrackUID_t uid = CreateTrackUID();

    f.WriteID2(0x73C5);    //TrackUID ID
    f.Write1UInt(8);
    f.Serialize8UInt(uid);
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

    file.WriteID1(0xA0);  //block group
    file.Write4UInt(block_group_size);

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

        file.WriteID1(0xFB);  //ReferenceBlock ID
        file.Write1UInt(2);
        file.Serialize2SInt(val);
    }

    if (duration > 0)
    {
        file.WriteID1(0x9B);  //BlockDuration ID
        file.Write1UInt(4);
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
    file.Write4UInt(block_size);

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

    file.Write(&flags, 1);   //written as binary, not uint

    file.Write(GetData(), GetSize());  //frame

    //end block

#ifdef _DEBUG
    const __int64 newpos = file.GetPosition();
    assert((newpos - pos) == block_size);
#endif
}


}  //end namespace WebmMuxLib
