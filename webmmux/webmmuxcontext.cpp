// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include <comdef.h>
#include "webmmuxcontext.hpp"
#include "versionhandling.hpp"
#include "comreg.hpp"
#include <cassert>
#include <ctime>
#include <sstream>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif

using std::wstring;
using std::wostringstream;

namespace WebmMuxLib
{

extern HMODULE s_hModule;

Context::Context() :
   m_pVideo(0),
   m_pAudio(0),
   m_timecode_scale(1000000)  //TODO
{
    //Seed the random number generator, which is needed
    //for creation of unique TrackUIDs.

    const time_t time_ = time(0);
    const unsigned seed = static_cast<unsigned>(time_);
    srand(seed);
}


Context::~Context()
{
   assert(m_pVideo == 0);
   assert(m_pAudio == 0);
   assert(m_file.GetStream() == 0);
}


void Context::SetVideoStream(StreamVideo* pVideo)
{
   assert((pVideo == 0) || (m_pVideo == 0));
   assert((pVideo == 0) || (pVideo->GetFrames().empty()));
   assert((pVideo == 0) || (pVideo->GetKeyFrames().empty()));
   assert(m_clusters.empty());

   m_pVideo = pVideo;
}


void Context::SetAudioStream(StreamAudio* pAudio)
{
   assert((pAudio == 0) || (m_pAudio == 0));
   assert((pAudio == 0) || (pAudio->GetFrames().empty()));
   assert(m_clusters.empty());

   m_pAudio = pAudio;  //TODO: generlize this to many audio streams
}



void Context::Open(IStream* pStream)
{
    assert(m_file.GetStream() == 0);
    assert((m_pVideo == 0) || (m_pVideo->GetFrames().empty()));
    assert((m_pVideo == 0) || (m_pVideo->GetKeyFrames().empty()));
    assert((m_pAudio == 0) || (m_pAudio->GetFrames().empty()));
    assert(m_clusters.empty());

    m_max_timecode = 0;        //to keep track of duration
    m_cEOS = 0;
    m_bEOSVideo = false;  //means we haven't seen EOS yet (from either
    m_bEOSAudio = false;  //the stream itself, or because of stop)

    int tn = 0;

    if (m_pVideo)
    {
        m_pVideo->SetTrackNumber(++tn);
        ++m_cEOS;
    }

    if (m_pAudio)
    {
        m_pAudio->SetTrackNumber(++tn);
        ++m_cEOS;
    }

    if (pStream)
    {
        m_file.SetStream(pStream);

#if 0   //TODO: parameterize this (with default of 0)
        const __int64 One_GB = 1024i64 * 1024i64 * 1024i64;
        __int64 size = 8 * One_GB;

        while (size > 0)
        {
            const HRESULT hr = m_file.SetSize(size);

            if (SUCCEEDED(hr))
                break;

            size /= 2;
        }
#endif

        m_file.SetPosition(0);
        WriteEbmlHeader();
        InitSegment();
    }
}


void Context::Close()
{
    if (m_pVideo)
        NotifyVideoEOS(0);

    if (m_pAudio)
        NotifyAudioEOS(0);

    Final();
}


void Context::Final()
{
    if (m_file.GetStream())
    {
        if (m_pVideo)
            m_pVideo->Final();  //grant last wishes

        if (m_pAudio)
            m_pAudio->Final();  //grant last wishes

        FinalSegment();
        m_file.SetStream(0);
    }

    assert(m_clusters.empty());
    assert((m_pVideo == 0) || (m_pVideo->GetFrames().empty()));
    assert((m_pVideo == 0) || (m_pVideo->GetKeyFrames().empty()));
    assert((m_pAudio == 0) || (m_pAudio->GetFrames().empty()));
}


ULONG Context::GetTimecodeScale() const
{
   return m_timecode_scale;
}


ULONG Context::GetTimecode() const
{
   return m_max_timecode;
}


void Context::WriteEbmlHeader()
{
    m_file.WriteID4(0x1A45DFA3);

    //Allocate 1 byte of storage for Ebml header size.
    const __int64 start_pos = m_file.SetPosition(1, STREAM_SEEK_CUR);

    //EBML Version

    m_file.WriteID2(0x4286);
    m_file.Write1UInt(1);
    m_file.Serialize1UInt(1);  //EBML Version = 1

    //EBML Read Version

    m_file.WriteID2(0x42F7);
    m_file.Write1UInt(1);
    m_file.Serialize1UInt(1);  //EBML Read Version = 1

    //EBML Max ID Length

    m_file.WriteID2(0x42F2);
    m_file.Write1UInt(1);
    m_file.Serialize1UInt(4);  //EBML Max ID Length = 4

    //EBML Max Size Length

    m_file.WriteID2(0x42F3);
    m_file.Write1UInt(1);
    m_file.Serialize1UInt(8);  //EBML Max Size Length = 8

    //Doc Type

    m_file.WriteID2(0x4282);

    //Some parsers don't like embedded NULs in the stream,
    //so this little trick won't work:
    //m_file.Write1String("webm", strlen("matroska"));
    //So do this instead:
    m_file.Write1String("webm");

    m_file.WriteID1(0xEC);  //Void element
    m_file.Write1UInt(9);
    m_file.SetPosition(9, STREAM_SEEK_CUR);

    //Doc Type Version

    m_file.WriteID2(0x4287);
    m_file.Write1UInt(1);
    m_file.Serialize1UInt(2);  //Doc Type Version

    //Doc Type Read Version

    m_file.WriteID2(0x4285);
    m_file.Write1UInt(1);
    m_file.Serialize1UInt(2);  //Doc Type Read Version

    const __int64 stop_pos = m_file.GetPosition();

    const __int64 size_ = stop_pos - start_pos;
    assert(size_ <= 126);  //1-byte EBML u-int type

    const BYTE size = static_cast<BYTE>(size_);

    m_file.SetPosition(start_pos - 1);
    m_file.Write1UInt(size);

    m_file.SetPosition(stop_pos);
}


void Context::InitSegment()
{
    m_segment_pos = m_file.GetPosition();

    m_file.WriteID4(0x18538067);  //Segment ID
    m_file.Write8UInt(0);         //will need to be filled in later

    InitFirstSeekHead();  //Meta Seek
    InitInfo();      //Segment Info
    WriteTrack();
}


void Context::FinalSegment()
{
    m_cues_pos = m_file.GetPosition();  //end of clusters
    WriteCues();

#if 0
    m_second_seekhead_pos = m_file.GetPosition();  //end of cues
    WriteSecondSeekHead();
#else
    m_clusters.clear();
#endif

    const __int64 maxpos = m_file.GetPosition();
    m_file.SetSize(maxpos);

    const __int64 size = maxpos - m_segment_pos - 12;
    assert(size >= 0);

    m_file.SetPosition(m_segment_pos);

    const ULONG id = m_file.ReadID4();
    assert(id == 0x18538067);  //Segment ID
    id;

    m_file.Write8UInt(size);  //total size of the segment

    FinalFirstSeekHead();
    FinalInfo();
    //FinalClusters(m_cues_pos);
}


void Context::FinalInfo()
{
    m_file.SetPosition(m_duration_pos + 2 + 1);  //2 byte ID + 1 byte size

    const float duration = static_cast<float>(m_max_timecode);

    m_file.Serialize4Float(duration);
}


void Context::InitFirstSeekHead()
{
    m_first_seekhead_pos = m_file.GetPosition();

    //The SeekID is 2 + 1 + 4 = 7 bytes.
    //The SeekPos is 2 + 1 + 8 = 11 bytes.
    //Total payload for a seek entry is 7 + 11 = 18 bytes.
    //The Seek entry is 2 + 1 + 18 = 21 bytes.

    //(first seek head)
    //SegmentInfo (1/4)
    //Track (2/4)
    //(clusters)
    //Cues (3/4)
    //2nd SeekHead (4/4)

    const BYTE size = /* 4 */ 3 * 21;

    m_file.WriteID4(0x114D9B74);  //Seek Head
    m_file.Write1UInt(size);

    m_file.SetPosition(size, STREAM_SEEK_CUR);
}



void Context::FinalFirstSeekHead()
{
    const __int64 start_pos = m_file.SetPosition(m_first_seekhead_pos + 4 + 1);

    WriteSeekEntry(0x1549A966, m_info_pos);   //SegmentInfo  (1/4)
    WriteSeekEntry(0x1654AE6B, m_track_pos);  //Track  (2/4)
    WriteSeekEntry(0x1C53BB6B, m_cues_pos);   //Cues (3/4)
    //WriteSeekEntry(0x114D9B74, m_second_seekhead_pos);   //2nd SeekHead (4/4)

    const __int64 stop_pos = m_file.GetPosition();

    const __int64 size = stop_pos - start_pos;
    size;
    assert(size == ( /* 4 */ 3 * 21));
}


#if 0
void Context::WriteSecondSeekHead()
{
    m_file.WriteID4(0x114D9B74);  //Seek Head

    //start_pos = start of payload
    const __int64 start_pos = m_file.SetPosition(4, STREAM_SEEK_CUR);

    clusters_t& cc = m_clusters;

#if 0
    typedef clusters_t::const_iterator iter_t;

    iter_t i = cc.begin();
    const iter_t j = cc.end();

    while (i != j)
    {
        const Cluster& c = *i++;
        WriteSeekEntry(0x1F43B675, c.m_pos);
    }
#else
    while (!cc.empty())
    {
        const Cluster& c = cc.front();
        WriteSeekEntry(0x1F43B675, c.m_pos);
        cc.pop_front();
    }
#endif

    const __int64 stop_pos = m_file.GetPosition();

    const __int64 size_ = stop_pos - start_pos;
    assert(size_ <= ULONG_MAX);

    const ULONG size = static_cast<ULONG>(size_);

    m_file.SetPosition(start_pos - 4);
    m_file.Write4UInt(size);

    m_file.SetPosition(stop_pos);
}
#endif


void Context::WriteSeekEntry(ULONG id, __int64 pos_)
{
    //The SeekID is 2 + 1 + 4 = 7 bytes.
    //The SeekPos is 2 + 1 + 8 = 11 bytes.
    //Total payload for a seek entry is 7 + 11 = 18 bytes.

    //The Seek entry is 2 + 1 + 18 = 21 bytes.
    //Total payload for SeekHead is 20000 * 21 = 420000 bytes.

#ifdef _DEBUG
    const __int64 start_pos = m_file.GetPosition();
#endif

    m_file.WriteID2(0x4DBB);  //Seek Entry ID
    m_file.Write1UInt(18);    //payload size of this Seek Entry

    m_file.WriteID2(0x53AB);  //SeekID ID
    m_file.Write1UInt(4);     //payload size is 4 bytes
    m_file.WriteID4(id);

    const __int64 pos = pos_ - m_segment_pos - 12;
    assert(pos >= 0);

    m_file.WriteID2(0x53AC);     //SeekPos ID
    m_file.Write1UInt(8);        //payload size is 8 bytes
    m_file.Serialize8UInt(pos);  //payload

#ifdef _DEBUG
    const __int64 stop_pos = m_file.GetPosition();
    assert((stop_pos - start_pos) == 21);
#endif
}


void Context::InitInfo()
{
    m_info_pos = m_file.GetPosition();

    m_file.WriteID4(0x1549A966);  //Segment Info ID

    //allocate 2 bytes of storage for size
    const __int64 pos = m_file.SetPosition(2, STREAM_SEEK_CUR);

    m_file.WriteID3(0x2AD7B1);                //TimeCodeScale ID
    m_file.Write1UInt(4);                     //payload size
    m_file.Serialize4UInt(m_timecode_scale);

    m_duration_pos = m_file.GetPosition();  //remember where duration is

    m_file.WriteID2(0x4489);         //Duration ID
    m_file.Write1UInt(4);            //payload size
    m_file.Serialize4Float(0.0);     //set value again during close

    //MuxingApp

    m_file.WriteID2(0x4D80);  //MuxingApp ID

    wstring fname;
    ComReg::ComRegGetModuleFileName(s_hModule, fname);

    wostringstream os;
    os << L"webmmux-";
    VersionHandling::GetVersion(fname.c_str(), os);

    m_file.Write1UTF8(os.str().c_str());  //writes both EBML size and payload

    //WritingApp

    if (!m_writing_app.empty())
    {
        m_file.WriteID2(0x5741);  //WritingApp ID
        m_file.Write1UTF8(m_writing_app.c_str());
    }

    const __int64 newpos = m_file.GetPosition();

    const __int64 size_ = newpos - pos;
    const USHORT size = static_cast<USHORT>(size_);

    m_file.SetPosition(pos - 2);
    m_file.Write2UInt(size);

    m_file.SetPosition(newpos);
}


void Context::WriteTrack()
{
    m_track_pos = m_file.GetPosition();

    m_file.WriteID4(0x1654AE6B);  //Tracks element (level 1)

    //allocate 2 bytes of storage for size of Tracks element (level 1)
    const __int64 begin_pos = m_file.SetPosition(2, STREAM_SEEK_CUR);

    int tn = 0;

    if (m_pVideo)
        m_pVideo->WriteTrackEntry(++tn);

    if (m_pAudio)  //TODO: allow for multiple audio streams
        m_pAudio->WriteTrackEntry(++tn);

    const __int64 end_pos = m_file.GetPosition();

    const __int64 size_ = end_pos - begin_pos;
    assert(size_ <= USHRT_MAX);

    const USHORT size = static_cast<USHORT>(size_);

    m_file.SetPosition(begin_pos - 2);
    m_file.Write2UInt(size);

    m_file.SetPosition(end_pos);
}


#if 0
void Context::Cluster::Final(
    Context& ctx,
    ULONG& prev,
    const Cluster& next)
{
    const __int64 size_ = next.m_pos - m_pos;
    assert(size_ >= 8);
    assert(size_ <= ULONG_MAX);

    const ULONG new_prev = static_cast<ULONG>(size_);
    const ULONG size = new_prev - 8;

    EbmlIO::File& f = ctx.m_file;

    f.SetPosition(m_pos);

    const ULONG id = f.ReadID4();
    assert(id == 0x1F43B675);
    id;

    f.Write4UInt(size);

    prev = new_prev;
}
#endif


void Context::WriteCuePoints(const Cluster& c)
{
    //cue point container = 1 + size len(2) + payload len
    //  time = 1 + size len(1) + payload len(4)
    //  track posns container = 1 + size len + payload len
    //     track = 1 + size len + payload len (track number val)
    //     cluster pos = 1 + size len + payload len (pos val)
    //     block num = 2 + size len + payload len (block num val)

    //TODO: for now just write video keyframes
    //Do we even need audio here?
    //We would need something, if this is an audio-only mux.

    //TODO: write this at beginning of file

    const Cluster::keyframes_t& kk = c.m_keyframes;
    assert(kk.size() <= 255);

    typedef Cluster::keyframes_t::const_iterator iter_t;

    iter_t i = kk.begin();
    const iter_t j = kk.end();

    while (i != j)
    {
        const Keyframe& k = *i++;
        WriteCuePoint(c, k);
    }
}


Context::Keyframe::Keyframe() :
   m_timecode(0),
   m_block(0)
{
}


Context::Keyframe::Keyframe(
   ULONG t,
   ULONG n) :
   m_timecode(t),
   m_block(n)
{
}


void Context::WriteCuePoint(
    const Cluster& c,
    const Keyframe& k)
{
    assert(m_pVideo);

    EbmlIO::File& f = m_file;

    f.WriteID1(0xBB);  //CuePoint ID
    f.Write1UInt(28);  //payload size

#ifdef _DEBUG
    const __int64 start_pos = f.GetPosition();
#endif

    f.WriteID1(0xB3);                //CueTime ID
    f.Write1UInt(4);                 //payload len is 4
    f.Serialize4UInt(k.m_timecode);  //payload

    f.WriteID1(0xB7);  //CueTrackPositions
    f.Write1UInt(20);  //payload size

#ifdef _DEBUG
    const __int64 start_track_pos = f.GetPosition();
#endif

    const int tn_ = m_pVideo->GetTrackNumber();
    assert(tn_ > 0);
    assert(tn_ <= 255);

    const BYTE tn = static_cast<BYTE>(tn_);

    f.WriteID1(0xF7);        //CueTrack ID
    f.Write1UInt(1);         //payload size is 1 byte
    f.Serialize1UInt(tn);    //payload

    const __int64 off = c.m_pos - m_segment_pos - 12;
    assert(off >= 0);

    f.WriteID1(0xF1);        //CueClusterPosition ID
    f.Write1UInt(8);         //payload size is 8 bytes
    f.Serialize8UInt(off);   //payload

    //TODO: Keyframe::m_block_number is a 4-byte
    //number, and we serialize all 4 bytes.  However,
    //it's unlikely we'll have block numbers that large
    //(because we create a new cluster every second).
    //Right now we always decide statically how many
    //bytes to serialize (we're using 4 bytes of storage,
    //so we serialize all 4 bytes, irrespective of the
    //value at run-time), but in the future we
    //could decide to check at run-time how large a value
    //we have, and then only serialize the minimum number
    //of bytes required for that value.

    f.WriteID2(0x5378);            //CueBlockNumber
    f.Write1UInt(4);               //payload size
    f.Serialize4UInt(k.m_block);   //payload  //TODO: don't need 4 bytes

#ifdef _DEBUG
    const __int64 stop_pos = f.GetPosition();
    assert((stop_pos - start_track_pos) == 20);
    assert((stop_pos - start_pos) == 28);
#endif
}



void Context::WriteCues()
{
    m_file.WriteID4(0x1C53BB6B);   //Cues ID

    //allocate 4 bytes of storage for size of cues element
    const __int64 start_pos = m_file.SetPosition(4, STREAM_SEEK_CUR);

    typedef clusters_t::const_iterator iter_t;

    iter_t i = m_clusters.begin();
    const iter_t j = m_clusters.end();

    while (i != j)
    {
        const Cluster& c = *i++;
        WriteCuePoints(c);
    }

    const __int64 stop_pos = m_file.GetPosition();

    const __int64 size_ = stop_pos - start_pos;
    assert(size_ <= ULONG_MAX);

    const ULONG size = static_cast<ULONG>(size_);

    m_file.SetPosition(start_pos - 4);
    m_file.Write4UInt(size);

    m_file.SetPosition(stop_pos);
}


void Context::NotifyVideoFrame(
    StreamVideo* pVideo,
    StreamVideo::VideoFrame* pFrame)
{
    pVideo;
    assert(pVideo);
    assert(pVideo == m_pVideo);
    assert(pFrame);
    assert(m_file.GetStream());  //TODO

    StreamVideo::frames_t& vframes = pVideo->GetFrames();
    assert(!vframes.empty());
    assert(vframes.back() == pFrame);

    const ULONG vt = pFrame->GetTimecode();

    StreamVideo::frames_t& rframes = m_pVideo->GetKeyFrames();

    if (rframes.empty())
    {
        rframes.push_back(pFrame);
        return;
    }

    if (pFrame->IsKey())
        rframes.push_back(pFrame);
    else
    {
        const StreamVideo::VideoFrame* const pvf0 = rframes.back();
        assert(pvf0);

        const ULONG vt0 = pvf0->GetTimecode();
        assert(vt >= vt0);

        const LONGLONG dt = LONGLONG(vt) - LONGLONG(vt0);
        assert(dt >= 0);

        const LONGLONG scale = GetTimecodeScale();
        assert(scale >= 1);

        const LONGLONG ns = scale * dt;

        //TODO: allow this to be parameterized
        if (ns <= 1000000000)  //1 sec
            return;

        rframes.push_back(pFrame);
    }

    //At this point, we have at least 2 rframes, which means
    //at least one cluster is potentially available to be written
    //to the file.  (Here the constraints that the video stream
    //needs to satisfy have been satisified.  We might still have
    //to wait for the audio stream to satisfy its constraints.)

    if ((m_pAudio == 0) || m_bEOSAudio)
    {
        CreateNewCluster(pFrame);
        return;
    }

    const StreamAudio::frames_t& aframes = m_pAudio->GetFrames();

    if (aframes.empty())
        return;

    const StreamAudio::AudioFrame* const paf = aframes.back();
    assert(paf);

    const ULONG at = paf->GetTimecode();

    if (at < vt)
        return;

    CreateNewCluster(pFrame);
}


void Context::NotifyAudioFrame(
    StreamAudio* pAudio,
    StreamAudio::AudioFrame* pFrame)
{
    pAudio;
    assert(pAudio);
    assert(pAudio == m_pAudio);
    assert(pFrame);
    assert(m_file.GetStream());

    StreamAudio::frames_t& aframes = pAudio->GetFrames();
    aframes.push_back(pFrame);

    const ULONG at = pFrame->GetTimecode();

    if ((m_pVideo == 0) || (m_pVideo->GetFrames().empty() && m_bEOSVideo))
    {
        const StreamAudio::AudioFrame* const paf = aframes.front();
        assert(paf);

        const ULONG at0 = paf->GetTimecode();
        assert(at >= at0);

        const LONG dt = LONG(at) - LONG(at0);

        //TODO: THIS ASSUMES TIMECODE HAS MS RESOLUTION!
        //THIS IS WRONG AND NEEDS TO BE FIXED
        if (dt >= 1000)
            CreateNewClusterAudioOnly();

        return;
    }

    StreamVideo::frames_t& vframes = m_pVideo->GetFrames();

    if (vframes.empty())
        return;

    if (m_bEOSVideo)
    {
        const LONG vt_ = m_pVideo->GetLastTimecode();
        assert(vt_ >= 0);

        const ULONG vt = static_cast<ULONG>(vt_);

        if (at >= vt)
            CreateNewCluster(0);  //NULL means deque all video

        return;  //not enough audio yet
    }

    StreamVideo::frames_t& rframes = m_pVideo->GetKeyFrames();

    if (rframes.empty())
        return;

    typedef StreamVideo::frames_t::const_iterator iter_t;

    iter_t i = rframes.begin();
    const iter_t j = rframes.end();

    const StreamVideo::VideoFrame* const pvf0 = *i++;  //1st rframe
    assert(pvf0);

    const ULONG vt0 = pvf0->GetTimecode();  //1st rframe
    vt0;

    if (i == j)  //only one rframe in queue
        return;

    const StreamVideo::VideoFrame* const pvf = *i;  //2nd rframe
    assert(pvf);

    const ULONG vt = pvf->GetTimecode();  //2nd rframe
    assert(vt >= vt0);

    if (at < vt)
        return;  //not enough audio

    CreateNewCluster(pvf);  //2nd rframe
}


bool Context::WaitVideo() const
{
    if (m_file.GetStream() == 0)
        return false;

    if (m_pVideo == 0)
        return false;

    if (m_bEOSVideo)
        return false;

    if (m_pAudio == 0)
        return false;

    if (m_bEOSAudio)
        return false;

    StreamVideo::frames_t& rframes = m_pVideo->GetKeyFrames();

    if (rframes.size() <= 1)
        return false;

    const StreamVideo::VideoFrame* const pvf0 = rframes.front();
    assert(pvf0);

    const StreamVideo::VideoFrame* const pvf = rframes.back();
    assert(pvf);

    const ULONG vt0 = pvf0->GetTimecode();
    const ULONG vt = pvf->GetTimecode();

    const LONG dt = LONG(vt) - LONG(vt0);
    assert(dt >= 0);

    //TODO: THIS ASSUMES TIMECODE HAS MS RESOLUTION!
    //THIS IS WRONG AND NEEDS TO BE FIXED
    if (dt < 1000)
        return false;

    const StreamAudio::frames_t& aframes = m_pAudio->GetFrames();

    if (aframes.empty())
        return true;

    const StreamAudio::AudioFrame* const paf = aframes.back();
    assert(paf);

    const ULONG at = paf->GetTimecode();

    if (vt <= at)
        return false;

    //TODO: THIS ASSUMES TIMECODE HAS MS RESOLUTION!
    //THIS IS WRONG AND NEEDS TO BE FIXED
    if ((vt - at) <= 1000)
        return false;

    return true;
}


bool Context::WaitAudio() const
{
    if (m_file.GetStream() == 0)
        return false;

    if (m_pAudio == 0)
        return false;

    if (m_bEOSAudio)
        return false;

    if (m_pVideo == 0)
        return false;

    if (m_bEOSVideo)
        return false;

    const StreamAudio::frames_t& aframes = m_pAudio->GetFrames();

    if (aframes.empty())
        return false;

    const StreamVideo::frames_t& rframes = m_pVideo->GetKeyFrames();

    if (rframes.empty())
        return true;  //wait for some video

    const StreamAudio::AudioFrame* const paf = aframes.back();
    assert(paf);

    const ULONG at = paf->GetTimecode();

    const StreamVideo::VideoFrame* const pvf = rframes.back();
    assert(pvf);

    const ULONG vt0 = pvf->GetTimecode();

    if (at <= vt0)
        return false;

    const LONG vt_ = m_pVideo->GetLastTimecode();
    assert(vt_ >= 0);

    const ULONG vt = static_cast<ULONG>(vt_);
    assert(vt >= vt0);

    if (at <= vt)
        return false;

    //TODO: THIS ASSUMES TIMECODE HAS MS RESOLUTION!
    //THIS IS WRONG AND NEEDS TO BE FIXED
    if ((at - vt) <= 1000)
        return false;

    return true;
}



int Context::NotifyVideoEOS(StreamVideo* pSource)
{
    if (m_bEOSVideo)
        return 0;

#if 0
    odbgstream os;
    os << "mux::eosvideo" << endl;
#endif

    m_bEOSVideo = true;

    if (m_file.GetStream() == 0)
        __noop;
    else if ((m_pAudio == 0) || m_bEOSAudio)
    {
        for (;;)
        {
            if ((m_pVideo != 0) && !m_pVideo->GetFrames().empty())
                CreateNewCluster(0);
            else if ((m_pAudio != 0) && !m_pAudio->GetFrames().empty())
                CreateNewClusterAudioOnly();
            else
                break;
        }
    }

    return EOS(pSource);
}


int Context::NotifyAudioEOS(StreamAudio* pSource)
{
    if (m_bEOSAudio)
        return 0;

#if 0
    odbgstream os;
    os << "mux::eosaudio" << endl;
#endif

    m_bEOSAudio = true;

    if (m_file.GetStream() == 0)
        __noop;
    else if ((m_pVideo == 0) || m_bEOSVideo)
    {
        for (;;)
        {
            if ((m_pVideo != 0) && !m_pVideo->GetFrames().empty())
                CreateNewCluster(0);
            else if ((m_pAudio != 0) && !m_pAudio->GetFrames().empty())
                CreateNewClusterAudioOnly();
            else
                break;
        }
    }

    return EOS(pSource);
}


int Context::EOS(Stream*)
{
    assert(m_cEOS > 0);
    --m_cEOS;

    if (m_cEOS > 0)
        return 0;

    Final();

    return 1;  //signal done
}


void Context::CreateNewCluster(
    const StreamVideo::VideoFrame* pvf_stop)
{
#if 0
    odbgstream os;
    os << "\nCreateNewCluster: pvf_stop=";

    if (pvf_stop == 0)
        os << "NULL";
    else
        os << pvf_stop->GetTimecode();

    os << endl;
#endif

    assert(m_pVideo);

    const StreamVideo::frames_t& vframes = m_pVideo->GetFrames();
    assert(!vframes.empty());

    clusters_t& cc = m_clusters;

    cc.push_back(Cluster());
    Cluster& c = cc.back();

    c.m_pos = m_file.GetPosition();

    {
        const StreamVideo::VideoFrame* const pvf = vframes.front();
        assert(pvf);
        assert(pvf != pvf_stop);

        const ULONG vt = pvf->GetTimecode();

        if ((m_pAudio == 0) || m_pAudio->GetFrames().empty())
            c.m_timecode = vt;
        else
        {
            const StreamAudio::frames_t& aframes = m_pAudio->GetFrames();
            const StreamAudio::AudioFrame* const paf = aframes.front();
            const ULONG at = paf->GetTimecode();

            c.m_timecode = (at <= vt) ? at : vt;
        }
    }

    m_file.WriteID4(0x1F43B675);  //Cluster ID

#if 0
    m_file.Write4UInt(0);         //patch size later, during close
#else
    m_file.SetPosition(4, STREAM_SEEK_CUR);
#endif

    m_file.WriteID1(0xE7);
    m_file.Write1UInt(4);
    m_file.Serialize4UInt(c.m_timecode);

    ULONG cFrames = 0;

    StreamVideo::frames_t& rframes = m_pVideo->GetKeyFrames();

    while (!vframes.empty())
    {
        const StreamVideo::VideoFrame* const pvf = vframes.front();
        assert(pvf);

        if (pvf == pvf_stop)
            break;

        const ULONG vt = pvf->GetTimecode();
        assert(vt >= c.m_timecode);
        assert((pvf_stop == 0) || (vt < pvf_stop->GetTimecode()));

        if ((m_pAudio == 0) || m_pAudio->GetFrames().empty())
        {
            if (!rframes.empty() && (pvf == rframes.front()))
                rframes.pop_front();

            WriteVideoFrame(c, cFrames);
            continue;
        }

        const StreamAudio::frames_t& aframes = m_pAudio->GetFrames();
        typedef StreamAudio::frames_t::const_iterator iter_t;

        iter_t i = aframes.begin();
        const iter_t j = aframes.end();

        const StreamAudio::AudioFrame* const paf = *i++;  //1st audio frame
        assert(paf);

        const ULONG at = paf->GetTimecode();
        assert(at >= c.m_timecode);

        if (vt < at)
        {
            if (!rframes.empty() && (pvf == rframes.front()))
                rframes.pop_front();

            WriteVideoFrame(c, cFrames);

            continue;
        }

        //At this point, we have (at least) one audio frame,
        //and (at least) one video frame.  They could have an
        //equal timecode, or that audio might be smaller than
        //the video.  Our desire is that the largest audio
        //frame less than the pvf_stop go on the next cluster,
        //which means any video frames greater than the audio
        //frame will also go on the next cluster.

        if (pvf_stop == 0)  //means write all extant frames
        {
            //We know that this audio frame is less or equal to
            //the video frame, so write it now.

            WriteAudioFrame(c, cFrames);
            continue;
        }

        //At this point, we still have an audio frame and a
        //video frame, neigther of which has been written yet.

        const ULONG vt_stop = pvf_stop->GetTimecode();

        if (at >= vt_stop)  //weird
            break;

        if (i == j)  //weird
            break;

        const StreamAudio::AudioFrame* const paf_stop = *i;  //2nd audio frame
        assert(paf_stop);

        const ULONG at_stop = paf_stop->GetTimecode();

        if (at_stop >= vt_stop)
            break;

        WriteAudioFrame(c, cFrames);   //write 1st audio frame
    }

    const __int64 pos = m_file.GetPosition();

    const __int64 size_ = pos - c.m_pos - 8;
    assert(size_ <= ULONG_MAX);

    const ULONG size = static_cast<ULONG>(size_);

    m_file.SetPosition(c.m_pos + 4);
    m_file.Write4UInt(size);

    m_file.SetPosition(pos);
}


void Context::CreateNewClusterAudioOnly()
{
    assert(m_pAudio);

    const StreamAudio::frames_t& aframes = m_pAudio->GetFrames();
    assert(!aframes.empty());

    const StreamAudio::AudioFrame* const paf_first = aframes.front();
    assert(paf_first);

    const StreamAudio::AudioFrame& af_first = *paf_first;

    const ULONG af_first_time = af_first.GetTimecode();

    clusters_t& cc = m_clusters;
    assert(cc.empty() || (af_first_time > cc.back().m_timecode));

    cc.push_back(Cluster());
    Cluster& c = cc.back();

    c.m_pos = m_file.GetPosition();
    c.m_timecode = af_first_time;

    m_file.WriteID4(0x1F43B675);  //Cluster ID

#if 0
    m_file.Write4UInt(0);         //patch size later, during close
#else
    m_file.SetPosition(4, STREAM_SEEK_CUR);
#endif

    m_file.WriteID1(0xE7);
    m_file.Write1UInt(4);
    m_file.Serialize4UInt(c.m_timecode);

    ULONG cFrames = 0;   //TODO: must write cues for audio

    while (!aframes.empty())
    {
        const StreamAudio::AudioFrame* const paf = aframes.front();
        assert(paf);

        const ULONG t = paf->GetTimecode();
        assert(t >= c.m_timecode);

        const LONG dt = LONG(t) - LONG(c.m_timecode);

        if (dt > 1000)
            break;

        WriteAudioFrame(c, cFrames);
    }

    const __int64 pos = m_file.GetPosition();

    const __int64 size_ = pos - c.m_pos - 8;
    assert(size_ <= ULONG_MAX);

    const ULONG size = static_cast<ULONG>(size_);

    m_file.SetPosition(c.m_pos + 4);
    m_file.Write4UInt(size);

    m_file.SetPosition(pos);
}


void Context::WriteVideoFrame(Cluster& c, ULONG& cFrames)
{
    assert(m_pVideo);
    StreamVideo& s = *m_pVideo;

    StreamVideo::frames_t& vframes = s.GetFrames();
    assert(!vframes.empty());

    StreamVideo::VideoFrame* const pf = vframes.front();
    assert(pf);

    StreamVideo::frames_t& rframes = m_pVideo->GetKeyFrames();
    rframes;  //already popped
    assert(rframes.empty() || (pf != rframes.front()));

    assert(cFrames < ULONG_MAX);
    ++cFrames;

    pf->Write(s, c.m_timecode);

    const ULONG ft = pf->GetTimecode();

    if (pf->IsKey())
    {
        Cluster::keyframes_t& kk = c.m_keyframes;
        const Keyframe k(ft, cFrames);

        kk.push_back(k);
    }

    if (ft > m_max_timecode)
       m_max_timecode = ft;

    vframes.pop_front();
    pf->Release();

#if 0
    odbgstream os;
    os << "mux::context::writevideoframe: t=" << ft
       << " vframes.size=" << vframes.size()
       << " rframes.size=" << rframes.size()
       << endl;
#endif
}


void Context::WriteAudioFrame(Cluster& c, ULONG& cFrames)
{
   assert(m_pAudio);
   StreamAudio& s = *m_pAudio;

   StreamAudio::frames_t& aframes = s.GetFrames();
   assert(!aframes.empty());

   StreamAudio::AudioFrame* const pf = aframes.front();
   assert(pf);

   assert(cFrames < ULONG_MAX);
   ++cFrames;

   pf->Write(s, c.m_timecode);

   const ULONG ft = pf->GetTimecode();

   if (ft > m_max_timecode)
      m_max_timecode = ft;

   aframes.pop_front();
   pf->Release();

#if 0
    odbgstream os;
    os << "mux::context::writeaudioframe: t=" << ft
       << " aframes.size=" << aframes.size()
       << endl;
#endif
}


void Context::FlushVideo(StreamVideo* pVideo)
{
    assert(pVideo);
    assert(pVideo == m_pVideo);

    StreamVideo::frames_t& vframes = pVideo->GetFrames();

    if (m_file.GetStream() == 0)
    {
        assert(vframes.empty());
        return;
    }

    while (!vframes.empty())
        CreateNewCluster(0);
}


void Context::FlushAudio(StreamAudio* pAudio)
{
    assert(pAudio);
    assert(pAudio == m_pAudio);

    const StreamAudio::frames_t& aframes = pAudio->GetFrames();

    if (m_file.GetStream() == 0)
    {
        assert(aframes.empty());
        return;
    }

    while (!aframes.empty())
    {
        if ((m_pVideo != 0) && !m_pVideo->GetFrames().empty())
            CreateNewCluster(0);
        else
            CreateNewClusterAudioOnly();
    }
}


}  //end namespace WebmMuxLib
