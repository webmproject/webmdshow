// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "webmmuxstreamvideo.hpp"
#include <vector>

namespace WebmMux
{

class StreamVideoVPx : public StreamVideo
{
    StreamVideoVPx(const StreamVideoVPx&);
    StreamVideoVPx& operator=(const StreamVideoVPx&);
    
public:
    StreamVideoVPx(Context&, const AM_MEDIA_TYPE&);
    
    HRESULT Receive(IMediaSample*);
    int EndOfStream();

protected:

    //void WriteTrackName();
    void WriteTrackCodecID();
    void WriteTrackCodecName();
    void WriteTrackSettings();

private:
    
    class VPxFrame : public VideoFrame
    {
        VPxFrame(const VPxFrame&);
        VPxFrame& operator=(const VPxFrame&);
      
    private:
        IMediaSample* const m_pSample;
        ULONG m_timecode;

    public:
        explicit VPxFrame(IMediaSample*, StreamVideoVPx*);
        ~VPxFrame(); 

        bool IsKey() const;
        ULONG GetTimecode() const;
        ULONG GetSize() const;
        const BYTE* GetData() const;

    };
    
public:
   
    LONG GetLastTimecode() const;

};

}  //end namespace WebmMux
