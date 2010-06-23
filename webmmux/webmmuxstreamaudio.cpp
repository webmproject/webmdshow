// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "webmmuxcontext.hpp"
#include <cstdlib>
#include <cassert>
#include <algorithm>


namespace WebmMuxLib
{


StreamAudio::AudioFrame::AudioFrame()
{
}



StreamAudio::StreamAudio(
    Context& ctx,
    const BYTE* pb,
    ULONG cb) :
    Stream(ctx),
    m_pFormat(malloc(cb)),
    m_cFormat(cb)
{
    assert(m_pFormat);
    memcpy(m_pFormat, pb, cb);
}


StreamAudio::~StreamAudio()
{
    free(m_pFormat);
}


bool StreamAudio::Wait() const
{
    return m_context.WaitAudio();
}


const void* StreamAudio::GetFormat(ULONG& cb) const
{
    cb = m_cFormat;
    return m_pFormat;
}


void StreamAudio::WriteTrackType()
{
    EbmlIO::File& f = m_context.m_file;

    f.WriteID1(0x83);     //TrackType ID
    f.Write1UInt(1);
    f.Serialize1UInt(2);  //2=audio
}


void StreamAudio::WriteTrackSettings()
{
    EbmlIO::File& f = m_context.m_file;

    f.WriteID1(0xE1);  //Audio settings

    //allocate 2 bytes of storage for size of settings
    const __int64 begin_pos = f.SetPosition(2, STREAM_SEEK_CUR);

    //const WAVEFORMATEX& wfx = GetFormat();
    const ULONG samples_per_sec_ = GetSamplesPerSec();
    assert(samples_per_sec_ > 0);

    const float samples_per_sec = static_cast<float>(samples_per_sec_);

    f.WriteID1(0xB5);  //SamplingFrequency ID
    f.Write1UInt(4);
    f.Serialize4Float(samples_per_sec);

    const BYTE channels = GetChannels();
    assert(channels > 0);

    f.WriteID1(0x9F);  //Channels ID
    f.Write1UInt(2);
    f.Serialize2UInt(channels);

    const __int64 end_pos = f.GetPosition();

    const __int64 size_ = end_pos - begin_pos;
    assert(size_ <= USHRT_MAX);

    const USHORT size = static_cast<USHORT>(size_);

    f.SetPosition(begin_pos - 2);
    f.Write2UInt(size);

    f.SetPosition(end_pos);
}


#if 0
void StreamAudio::AudioFrame::Write(
    const Stream& s,
    ULONG cluster_timecode) const
{
    EbmlIO::File& file = s.m_context.m_file;

   //block = 1 byte ID + 4 byte size + f->size
   //block duration = 1 byte ID + 1 byte size + 1 byte value
   //reference block = 1 byte ID + 2 byte size + 2(?) byte signed value

   const ULONG block_size = 1 + 2 + 1 + GetSize();     //tn, tc, flg, f
   const ULONG block_group_size = 1 + 4 + block_size; //block id, size, payload

   file.WriteID1(0xA0);                //block group ID
   file.Write4UInt(block_group_size);  //size of payload for this block group

#ifdef _DEBUG
   const __int64 pos = file.GetPosition();
#endif

   //begin block

   file.WriteID1(0xA1);  //Block ID
   file.Write4UInt(block_size);

   const int tn_ = s.GetTrackNumber();
   assert(tn_ > 0);
   assert(tn_ <= 127);

   const BYTE tn = static_cast<BYTE>(tn_);

   file.Write1UInt(tn);   //track number

   const ULONG ft = GetTimecode();

   {
      const LONG tc_ = LONG(ft) - LONG(cluster_timecode);
      assert(tc_ >= SHRT_MIN);
      assert(tc_ <= SHRT_MAX);

      const SHORT tc = static_cast<SHORT>(tc_);

      file.Serialize2SInt(tc);       //relative timecode
   }

   const BYTE flags = 0;
   file.Write(&flags, 1);   //written as binary, not uint

   file.Write(GetData(), GetSize());  //frame

   //end block

#ifdef _DEBUG
   const __int64 newpos = file.GetPosition();
   assert((newpos - pos) == block_group_size);
#endif

}
#endif


bool StreamAudio::AudioFrame::IsKey() const
{
    return true;
}


StreamAudio::frames_t& StreamAudio::GetFrames()
{
    return m_frames;
}

void StreamAudio::Flush()
{
    m_context.FlushAudio(this);
}


}  //end namespace WebmMuxLib
