// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "webmmuxebmlio.hpp"
#include "webmmuxstreamvideo.hpp"
#include "webmmuxstreamaudio.hpp"
#include <list>

namespace WebmMuxLib
{

class Context
{
   Context(const Context&);
   Context& operator=(const Context&);

public:

   EbmlIO::File m_file;
   std::wstring m_writing_app;

   Context();
   ~Context();

   void SetVideoStream(StreamVideo*);

   //TODO: this needs to liberalized to handle multiple audio streams.
   void SetAudioStream(StreamAudio*);

   void Open(IStream*);
   void Close();

    void NotifyVideoFrame(StreamVideo*, StreamVideo::VideoFrame*);
    int NotifyVideoEOS(StreamVideo*);
    void FlushVideo(StreamVideo*);
    bool WaitVideo() const;

    void NotifyAudioFrame(StreamAudio*, StreamAudio::AudioFrame*);
    int NotifyAudioEOS(StreamAudio*);
    void FlushAudio(StreamAudio*);
    bool WaitAudio() const;

    ULONG GetTimecodeScale() const;
    ULONG GetTimecode() const;  //of frame most recently written to file

private:

   StreamVideo* m_pVideo;
   StreamAudio* m_pAudio;  //TODO: accept multiple audio streams

   void Final();

   void WriteEbmlHeader();

   void InitSegment();
   void FinalSegment();

   void InitFirstSeekHead();
   void FinalFirstSeekHead();
   void WriteSeekEntry(ULONG, __int64);

   void InitInfo();
   void FinalInfo();

   void WriteTrack();

   __int64 m_segment_pos;
   __int64 m_first_seekhead_pos;
   //__int64 m_second_seekhead_pos;
   __int64 m_info_pos;
   __int64 m_track_pos;
   __int64 m_cues_pos;
   __int64 m_duration_pos;
   const ULONG m_timecode_scale;  //TODO: video vs. audio
   ULONG m_max_timecode;  //unscaled

    struct Keyframe
    {
        ULONG m_timecode;    //unscaled
        ULONG m_block;       //1-based number of block within cluster

        Keyframe();   //needed for std::list
        Keyframe(ULONG timecode, ULONG block);
    };

    struct Cluster
    {
        //absolute pos within file (NOT offset relative to segment)
        __int64 m_pos;

        ULONG m_timecode;

        typedef std::list<Keyframe> keyframes_t;
        keyframes_t m_keyframes;
    };

   typedef std::list<Cluster> clusters_t;
   clusters_t m_clusters;

   //void WriteSecondSeekHead();
   void WriteCues();
   //void FinalClusters(__int64 pos);

    //bool ReadyToCreateNewClusterVideo(const StreamVideo::VideoFrame&) const;
    void CreateNewCluster(const StreamVideo::VideoFrame*);
    void CreateNewClusterAudioOnly();

    void WriteVideoFrame(
        Cluster&,
        ULONG&,
        const StreamVideo::VideoFrame* stop,
        const StreamVideo::VideoFrame* next,
        LONG prev_timecode);

    void WriteAudioFrame(Cluster&, ULONG&);

    void WriteCuePoints(const Cluster&);
    void WriteCuePoint(const Cluster&, const Keyframe&);

    //EOS can happen either because we receive a notification from the stream,
    //or because the graph was stopped (before reaching end-of-stream proper).
    //The following flags are use to keep track of whether we've seen
    //EOS already.

    bool m_bEOSVideo;
    bool m_bEOSAudio;
    int m_cEOS;
    int EOS(Stream*);

};


}  //end namespace WebmMuxLib
