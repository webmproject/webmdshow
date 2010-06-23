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

namespace WebmMuxLib
{

struct Cluster;

class StreamAudio : public Stream
{
    StreamAudio(const StreamAudio&);
    StreamAudio& operator=(const StreamAudio&);

protected:
    StreamAudio(Context&, const BYTE*, ULONG);

    void WriteTrackType();
    void WriteTrackSettings();

    const void* GetFormat(ULONG&) const;

    virtual ULONG GetSamplesPerSec() const = 0;
    virtual BYTE GetChannels() const = 0;

public:
    ~StreamAudio();

    void Flush();
    bool Wait() const;

    class AudioFrame : public Frame
    {
        AudioFrame(const AudioFrame&);
        AudioFrame& operator=(const AudioFrame&);

    protected:
        AudioFrame();

    public:
        bool IsKey() const;

    };

    typedef std::list<AudioFrame*> frames_t;
    frames_t& GetFrames();

private:
    void* const m_pFormat;
    const ULONG m_cFormat;
    frames_t m_frames;

};

}  //end namespace WebmMuxLib
