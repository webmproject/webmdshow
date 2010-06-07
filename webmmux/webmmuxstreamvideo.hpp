// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "webmmuxstream.hpp"
#include <list>

namespace WebmMux
{

class StreamVideo : public Stream
{
    StreamVideo(const StreamVideo&);
    StreamVideo& operator=(const StreamVideo&);
    
protected:
    StreamVideo(Context&, const AM_MEDIA_TYPE&);
    ~StreamVideo();

    void WriteTrackType();
    
public:
    void Flush();
    bool Wait() const;

    class VideoFrame : public Frame
    {
        VideoFrame(const VideoFrame&);
        VideoFrame& operator=(const VideoFrame&);

    protected:
        VideoFrame();

    };

    virtual LONG GetLastTimecode() const = 0;

    typedef std::list<VideoFrame*> frames_t;    
    frames_t& GetFrames();
    frames_t& GetKeyFrames();
    
protected:
    frames_t m_vframes;
    frames_t m_rframes;    
    AM_MEDIA_TYPE m_mt;     

    const BITMAPINFOHEADER& GetBitmapInfoHeader() const;
    float GetFramerate() const;

};

} //end namespace WebmMux
