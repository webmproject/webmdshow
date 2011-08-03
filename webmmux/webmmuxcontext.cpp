// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <cassert>
#include <ctime>
#include <sstream>

#include <strmif.h>
#include <comdef.h>

#include "comreg.hpp"
#include "scratchbuf.hpp"
#include "versionhandling.hpp"
#include "webmconstants.hpp"
#include "webmmuxcontext.hpp"

#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif

using std::wstring;
using std::wostringstream;

enum { kAudioClusterSizeInTimeMs = 5000 };  //TODO: parameterize this

namespace WebmMuxLib
{

extern HMODULE s_hModule;

Context::Context() :
   m_bLiveMux(false),
   m_bBufferData(false),
   m_pVideo(0),
   m_pAudio(0),
   m_timecode_scale(1000000),  //TODO
   m_info_pos(0),
   m_seekhead_pos(0),
   m_segment_pos(0)
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

        ResetBuffer();

        if (!m_bLiveMux)
            m_file.SetPosition(0);
        else
            assert(m_file.GetPosition() == 0);

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
    m_buf.WriteID4(WebmUtil::kEbmlID);

    // store the header size offset for later correction
    const uint64 offset_header_size = m_buf.GetBufferLength();
    m_buf.Write1UInt(0xF); // temp size

    // We must exclude |num_bytes_to_ignore| from the size we obtain from
    // |m_buf|.  The value from |m_buf| includes the bytes storing
    // kEbmlID and the size written into the buffer-- using it would result in
    // an invalid WebM file.
    const uint64 num_bytes_to_ignore = 4 + sizeof(uint8);

    // EBML Version
    m_buf.WriteID2(WebmUtil::kEbmlVersionID);
    m_buf.Write1UInt(1);      // element size
    m_buf.Serialize1UInt(1);  // EBML Version = 1

    // EBML Read Version
    m_buf.WriteID2(WebmUtil::kEbmlReadVersionID);
    m_buf.Write1UInt(1);      // element size
    m_buf.Serialize1UInt(1);  // EBML Read Version = 1

    // EBML Max ID Length
    m_buf.WriteID2(WebmUtil::kEbmlMaxIDLengthID);
    m_buf.Write1UInt(1);      // element size
    m_buf.Serialize1UInt(4);  // EBML Max ID Length = 4

    // EBML Max Size Length
    m_buf.WriteID2(WebmUtil::kEbmlMaxSizeLengthID);
    m_buf.Write1UInt(1);      // element size
    m_buf.Serialize1UInt(8);  // EBML Max Size Length = 8

    // Doc Type
    m_buf.WriteID2(WebmUtil::kEbmlDocTypeID);
    m_buf.Write1String("webm");

    // Pad our doc type (so it can be changed to matroska easily)
    m_buf.WriteID1(WebmUtil::kEbmlVoidID);
    const int32 void_element_size = 9;
    m_buf.Write1UInt(void_element_size);
    for (int32 i = 0; i < void_element_size; ++i)
    {
        m_buf.Serialize1UInt(0);
    }

    // Doc Type Version
    m_buf.WriteID2(WebmUtil::kEbmlDocTypeVersionID);
    m_buf.Write1UInt(1);      // element size
    m_buf.Serialize1UInt(2);  // Doc Type Version

    // Doc Type Read Version
    m_buf.WriteID2(WebmUtil::kEbmlDocTypeReadVersionID);
    m_buf.Write1UInt(1);      // element size
    m_buf.Serialize1UInt(2);  // Doc Type Read Version

    // patch in the actual length of the EBML header
    const uint64 actual_header_length =
        m_buf.GetBufferLength() - num_bytes_to_ignore;
    assert(actual_header_length < 255);
    m_buf.RewriteUInt(offset_header_size, actual_header_length, 1);

    // dump |m_buf| into the out file
    m_file.Write(m_buf.GetBufferPtr(),
                 static_cast<ULONG>(m_buf.GetBufferLength()));
    m_buf.Reset();
}


void Context::InitSegment()
{
    m_file.WriteID4(WebmUtil::kEbmlSegmentID);  //Segment ID

    if (m_bLiveMux == false)
    {
        m_segment_pos = m_file.GetPosition() - 4;
        m_file.Serialize8UInt(0x01FFFFFFFFFFFFFFLL);
    }
    else
        m_file.Serialize1UInt(0xFF);

    if (m_pVideo && !m_bLiveMux)
        InitFirstSeekHead();  //Meta Seek

    InitInfo();      //Segment Info
    WriteTrack();
}


void Context::FinalSegment()
{
    m_cues_pos = m_file.GetPosition();  //end of clusters

    if (m_pVideo && !m_bLiveMux)
        WriteCues();

    m_clusters.clear();

    if (!m_bLiveMux)
    {
        const __int64 maxpos = m_file.GetPosition();
        m_file.SetSize(maxpos);

        const __int64 size = maxpos - m_segment_pos - 12;
        assert(size >= 0);

        m_file.SetPosition(m_segment_pos);

        const ULONG id = m_file.ReadID4();
        assert(id == WebmUtil::kEbmlSegmentID);
        id;

        m_file.Write8UInt(size);  //total size of the segment
    }

    if (m_pVideo && !m_bLiveMux)
        FinalFirstSeekHead();

    FinalInfo();
}


void Context::FinalInfo()
{
    if (!m_bLiveMux)
    {
        m_file.SetPosition(m_duration_pos);

        m_file.WriteID2(WebmUtil::kEbmlDurationID); // Duration ID
        m_file.Write1UInt(4);                       // payload size

        const float duration = static_cast<float>(m_max_timecode);
        m_file.Serialize4Float(duration);
    }
}


void Context::InitFirstSeekHead()
{
    m_seekhead_pos = m_file.GetPosition();

    // The SeekID is 2 + 1 + 4 = 7 bytes.
    // The SeekPos is 2 + 1 + 8 = 11 bytes.
    // Total payload for a seek entry is 7 + 11 = 18 bytes.
    // The Seek entry is 2 + 1 + 18 = 21 bytes.

    // (first seek head)
    // SegmentInfo (1/3)
    // Track (2/3)
    // (clusters)
    // Cues (3/3)

    const BYTE size = (4-1) + (3*21);  // (SeekHead ID - Void ID) + payload

    m_file.WriteID1(WebmUtil::kEbmlVoidID);
    m_file.Write1UInt(size);
    m_file.SetPosition(size, STREAM_SEEK_CUR);
}



void Context::FinalFirstSeekHead()
{
    if (!m_bLiveMux)
    {
        const LONGLONG start_pos = m_file.SetPosition(m_seekhead_pos);
        const BYTE payload_size = 3*21;

        m_file.WriteID4(WebmUtil::kEbmlSeekHeadID);
        m_file.Write1UInt(payload_size);

        WriteSeekEntry(WebmUtil::kEbmlSegmentInfoID, m_info_pos);
        WriteSeekEntry(WebmUtil::kEbmlTracksID, m_track_pos);
        WriteSeekEntry(WebmUtil::kEbmlCuesID, m_cues_pos);

        assert((m_file.GetPosition() - start_pos) == (4 + 1 + payload_size));
    }
}


void Context::WriteSeekEntry(ULONG id, __int64 pos_)
{
    assert(m_bLiveMux == false);

    //The SeekID is 2 + 1 + 4 = 7 bytes.
    //The SeekPos is 2 + 1 + 8 = 11 bytes.
    //Total payload for a seek entry is 7 + 11 = 18 bytes.

    //The Seek entry is 2 + 1 + 18 = 21 bytes.
    //Total payload for SeekHead is 20000 * 21 = 420000 bytes.

#ifdef _DEBUG
    const __int64 start_pos = m_file.GetPosition();
#endif

    m_file.WriteID2(WebmUtil::kEbmlSeekEntryID);
    m_file.Write1UInt(18);  // payload size

    m_file.WriteID2(WebmUtil::kEbmlSeekIDID);
    m_file.Write1UInt(4);   // payload size is 4 bytes
    m_file.WriteID4(id);

    const __int64 pos = pos_ - m_segment_pos - 12;
    assert(pos >= 0);

    m_file.WriteID2(WebmUtil::kEbmlSeekPositionID);
    m_file.Write1UInt(8);        // payload size is 8 bytes
    m_file.Serialize8UInt(pos);  // payload

#ifdef _DEBUG
    const __int64 stop_pos = m_file.GetPosition();
    assert((stop_pos - start_pos) == 21);
#endif
}


void Context::InitInfo()
{
    if (!m_bLiveMux)
        m_info_pos = m_file.GetPosition();

    m_buf.WriteID4(WebmUtil::kEbmlSegmentInfoID);

    const uint64 size_pos = m_buf.GetBufferLength();
    // pad with 0; we're going to patch the size into the buffer
    m_buf.Serialize2UInt(0);

    // We must exclude |num_bytes_to_ignore| from the size we obtain from
    // |m_buf|.  The value from |m_buf| includes the bytes storing
    // kEbmlSegmentInfoID and the size written into the buffer-- using it
    // would result in invalid segment info.
    const uint64 num_bytes_to_ignore = 4 + sizeof(uint16);

    m_buf.WriteID3(WebmUtil::kEbmlTimeCodeScaleID);
    m_buf.Write1UInt(4);  // payload size
    m_buf.Serialize4UInt(m_timecode_scale);

    if (!m_bLiveMux)
    {
        // remember where duration is
        m_duration_pos = m_file.GetPosition() + m_buf.GetBufferLength();
        m_buf.WriteID1(WebmUtil::kEbmlVoidID);
        m_buf.Write1UInt(5);  // (Duration ID - Void ID) + payload

        // reserve space in the buffer
        for (int32 i = 0; i < 5; ++i)
        {
            m_buf.Serialize1UInt(0xF);
        }
    }

    // MuxingApp
    m_buf.WriteID2(WebmUtil::kEbmlMuxingAppID);

    wstring fname;
    ComReg::ComRegGetModuleFileName(s_hModule, fname);

    wostringstream os;
    os << L"webmmux-";
    VersionHandling::GetVersion(fname.c_str(), os);

    m_buf.Write1UTF8(os.str().c_str()); // writes both EBML size and payload

    // WritingApp
    if (!m_writing_app.empty())
    {
        m_buf.WriteID2(WebmUtil::kEbmlWritingAppID);
        m_buf.Write1UTF8(m_writing_app.c_str());
    }

    const uint64 actual_seginfo_len =
        m_buf.GetBufferLength() - num_bytes_to_ignore;
    m_buf.RewriteUInt(size_pos, actual_seginfo_len, sizeof(uint16));

    m_file.Write(m_buf.GetBufferPtr(),
                 static_cast<ULONG>(m_buf.GetBufferLength()));
    m_buf.Reset();
}


void Context::WriteTrack()
{
    if (!m_bLiveMux)
        m_track_pos = m_file.GetPosition();

    assert(m_bBufferData == false);

    m_buf.WriteID4(WebmUtil::kEbmlTracksID);

    // store tracks element offset for patching later
    const uint64 track_len_offset = m_buf.GetBufferLength();

    // We must exclude |num_bytes_to_ignore| from the size we obtain from
    // |m_buf|.  The value from |m_buf| includes the bytes storing
    // kEbmlTracksID and the size written into the buffer-- using it
    // would result in invalid segment info.
    const uint64 num_bytes_to_ignore = 4 + sizeof(uint16);

    // reserve two bytes for the size of the tracks element
    m_buf.Serialize2UInt(0);

    int track_num = 0;

    if (m_pVideo)
        m_pVideo->WriteTrackEntry(++track_num);

    if (m_pAudio)  //TODO: allow for multiple audio streams
        m_pAudio->WriteTrackEntry(++track_num);

    if (m_bBufferData)
    {
        // Buffering was enabled by one of the tracks while writing the entry.
        // Currently this should only happen with Vorbis audio from the
        // Xiph.org directshow filter.
        m_buf_element_info.byte_count = sizeof(uint16);
        m_buf_element_info.num_bytes_to_ignore = num_bytes_to_ignore;
        m_buf_element_info.offset = track_len_offset;
    }
    else
    {
        uint64 actual_tracks_len =
            m_buf.GetBufferLength() - num_bytes_to_ignore;
        m_buf.RewriteUInt(track_len_offset, actual_tracks_len, sizeof(uint16));
        m_file.Write(m_buf.GetBufferPtr(),
                     static_cast<ULONG>(m_buf.GetBufferLength()));
        m_buf.Reset();
    }

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
    else if (m_bLiveMux)
    {
        #if 0 //def _DEBUG
        odbgstream os;
        os << "["__FUNCTION__"] " << "rframes.size()=" << rframes.size()
           << " vframes.size()=" << vframes.size()<< endl;
        #endif
        return;
    }

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
        if (dt >= kAudioClusterSizeInTimeMs)
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


void Context::CreateNewCluster(const StreamVideo::VideoFrame* pvf_stop)
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

    assert(m_bBufferData == false);
    assert(m_pVideo);

    const StreamVideo::frames_t& vframes = m_pVideo->GetFrames();
    assert(!vframes.empty());

    clusters_t& cc = m_clusters;

    //const Cluster* const pPrevCluster = cc.empty() ? 0 : &cc.back();

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

    // Write cluster header
    m_file.WriteID4(WebmUtil::kEbmlClusterID);
    if (!m_bLiveMux)
    {
        // Use a 8-byte cluster header
        m_file.Serialize4UInt(0x1FFFFFFF);  // temp cluster size; rewritten
                                            // below if |m_bLiveMux| is false
    }
    else
    {
        // Use a 5-byte cluster header
        m_file.Serialize1UInt(0xFF);
    }

    m_file.WriteID1(WebmUtil::kEbmlTimeCodeID);
    m_file.Write1UInt(4);
    m_file.Serialize4UInt(c.m_timecode);

    const __int64 off = c.m_pos - m_segment_pos - 12;
    assert(off >= 0);

#if 0
    //TODO: disable until we're sure this is allowed per the Webm std
    m_file.WriteID1(0xA7);        //Position ID
    m_file.Write1UInt(8);         //payload size is 8 bytes
    m_file.Serialize8UInt(off);   //payload

    if (pPrevCluster)
    {
        const __int64 size = c.m_pos - pPrevCluster->m_pos;
        assert(size > 0);

        m_file.WriteID1(0xAB);        //PrevSize ID
        m_file.Write1UInt(8);         //payload size is 8 bytes
        m_file.Serialize8UInt(size);  //payload
    }
#endif

    ULONG cFrames = 0;
    LONG vtc_prev  = -1;

    StreamVideo::frames_t& rframes = m_pVideo->GetKeyFrames();

    while (!vframes.empty())
    {
        typedef StreamVideo::frames_t::const_iterator video_iter_t;

        video_iter_t video_iter = vframes.begin();
        const video_iter_t video_iter_end = vframes.end();

        const StreamVideo::VideoFrame* const pvf = *video_iter++;
        assert(pvf);

        if (pvf == pvf_stop)
            break;

        const StreamVideo::VideoFrame* const pvf_next =
            (video_iter == video_iter_end) ? 0 : *video_iter;

        //const bool bLastVideo = (pvf_next == pvf_stop);

        const ULONG vt = pvf->GetTimecode();
        assert(vt >= c.m_timecode);
        assert((pvf_stop == 0) || (vt < pvf_stop->GetTimecode()));

        if ((m_pAudio == 0) || m_pAudio->GetFrames().empty())
        {
            if (!rframes.empty() && (pvf == rframes.front()))
                rframes.pop_front();

            const ULONG vtc = pvf->GetTimecode();

            WriteVideoFrame(c, cFrames, pvf_stop, pvf_next, vtc_prev);

            vtc_prev = vtc;

            continue;
        }

        const StreamAudio::frames_t& aframes = m_pAudio->GetFrames();
        typedef StreamAudio::frames_t::const_iterator audio_iter_t;

        audio_iter_t i = aframes.begin();
        const audio_iter_t j = aframes.end();

        const StreamAudio::AudioFrame* const paf = *i++;  //1st audio frame
        assert(paf);

        const ULONG at = paf->GetTimecode();
        assert(at >= c.m_timecode);

        if (vt < at)
        {
            if (!rframes.empty() && (pvf == rframes.front()))
                rframes.pop_front();

            const ULONG vtc = pvf->GetTimecode();

            WriteVideoFrame(c, cFrames, pvf_stop, pvf_next, vtc_prev);

            vtc_prev = vtc;

            continue;
        }

        //At this point, we have (at least) one audio frame,
        //and (at least) one video frame.  They could have an
        //equal timecode, or the audio might be smaller than
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

    if (m_bLiveMux == false)
    {
        // In default (not live) mode we must seek back to the
        // cluster size pos, and replace the -1 placeholder with
        // the correct value.
        const __int64 pos = m_file.GetPosition();

        const __int64 size_ = pos - c.m_pos - 8;
        assert(size_ <= ULONG_MAX);

        const ULONG size = static_cast<ULONG>(size_);

        m_file.SetPosition(c.m_pos + 4);
        m_file.Write4UInt(size);

        m_file.SetPosition(pos);
    }
}


void Context::CreateNewClusterAudioOnly()
{
    assert(m_bBufferData == false);
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

    // Write cluster header
    m_file.WriteID4(WebmUtil::kEbmlClusterID);
    if (!m_bLiveMux)
    {
        // Use a 7-byte cluster header
        m_file.SerializeUInt(0x3FFFFF, 3);  // temp cluster size; rewritten
                                            // below if |m_bLiveMux| is false
    }
    else
    {
        // Use a 5-byte cluster header
        m_file.Serialize1UInt(0xFF);
    }

    m_file.WriteID1(WebmUtil::kEbmlTimeCodeID);

    const BYTE timecode_size = m_file.GetSerializeUIntSize(c.m_timecode);
    assert(timecode_size <= 8);

    m_file.Write1UInt(timecode_size);
    m_file.SerializeUInt(c.m_timecode, timecode_size);

    const __int64 off = c.m_pos - m_segment_pos - 12;
    assert(off >= 0);

#if 0
    //disable this until we're sure it's allowed per the WebM std
    m_file.WriteID1(0xA7);        //Position ID
    m_file.Write1UInt(8);         //payload size is 8 bytes
    m_file.Serialize8UInt(off);   //payload

    if (pPrevCluster)
    {
        const __int64 size = c.m_pos - pPrevCluster->m_pos;
        assert(size > 0);

        m_file.WriteID1(0xAB);        //PrevSize ID
        m_file.Write1UInt(8);         //payload size is 8 bytes
        m_file.Serialize8UInt(size);  //payload
    }
#endif

    ULONG cFrames = 0;   //TODO: must write cues for audio

    while (!aframes.empty())
    {
        const StreamAudio::AudioFrame* const paf = aframes.front();
        assert(paf);

        const ULONG t = paf->GetTimecode();
        assert(t >= c.m_timecode);

        const LONG dt = LONG(t) - LONG(c.m_timecode);

        if (dt > kAudioClusterSizeInTimeMs)
            break;

        WriteAudioFrame(c, cFrames);
    }

    if (m_bLiveMux == false)
    {
        // In default (not live) mode we must seek back to the
        // cluster size pos, and replace the -1 placeholder with
        // the correct value.
        const LONGLONG pos = m_file.GetPosition();
        const LONGLONG size = pos - (c.m_pos + 7);  //7 = ID (4) + size (3)

        m_file.SetPosition(c.m_pos + 4);
        m_file.WriteUInt(size, 3);

        m_file.SetPosition(pos);
    }
}


void Context::WriteVideoFrame(
    Cluster& c,
    ULONG& cFrames,
    const StreamVideo::VideoFrame* stop,
    const StreamVideo::VideoFrame* next,
    LONG /* prev_timecode */ )
{
    assert(m_pVideo);
    StreamVideo& s = *m_pVideo;

    StreamVideo::frames_t& vframes = s.GetFrames();
    assert(!vframes.empty());

    StreamVideo::VideoFrame* const pf = vframes.front();
    assert(pf);
    assert(pf != stop);
    assert(pf != next);

    StreamVideo::frames_t& rframes = m_pVideo->GetKeyFrames();
    rframes;  //already popped
    assert(rframes.empty() || (pf != rframes.front()));

    assert(cFrames < ULONG_MAX);
    ++cFrames;

    const ULONG ft = pf->GetTimecode();

#if 1
    pf->WriteSimpleBlock(s, c.m_timecode);
#else
    if (next != stop)
        pf->WriteSimpleBlock(s, c.m_timecode);
    else
    {
        ULONG duration;

        if (next == 0)
            duration = pf->GetDuration();
        else
        {
            const ULONG tc_curr = pf->GetTimecode();
            const ULONG tc_next = next->GetTimecode();

            if (tc_next <= tc_curr)
                duration = 0;
            else
                duration = tc_next - tc_curr;
        }

        if (duration == 0)
            pf->WriteSimpleBlock(s, c.m_timecode);
        else if ((prev_timecode >= 0) && (ft > ULONG(prev_timecode)))
            pf->WriteBlockGroup(s, c.m_timecode, prev_timecode, duration);
        else if (pf->IsKey())
            pf->WriteBlockGroup(s, c.m_timecode, -1, duration);
        else
            pf->WriteSimpleBlock(s, c.m_timecode);
    }
#endif

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

   pf->WriteSimpleBlock(s, c.m_timecode);

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

bool Context::GetLiveMuxMode() const
{
    return m_bLiveMux;
}

void Context::SetLiveMuxMode(bool is_live)
{
    m_bLiveMux = is_live;
}

void Context::BufferData()
{
    assert(m_bBufferData == false);
    m_bBufferData = true;
}

void Context::FlushBufferedData()
{
    assert(m_bBufferData == true);
    const uint64 element_size =
        m_buf.GetBufferLength() - m_buf_element_info.num_bytes_to_ignore;
    m_buf.RewriteUInt(m_buf_element_info.offset, element_size,
                      m_buf_element_info.byte_count);
    m_file.Write(m_buf.GetBufferPtr(),
                 static_cast<ULONG>(m_buf.GetBufferLength()));
    m_buf.Reset();
    m_bBufferData = false;
}

void Context::ResetBuffer()
{
    m_bBufferData = false;
    m_buf.Reset();
}

} // namespace WebmMuxLib
