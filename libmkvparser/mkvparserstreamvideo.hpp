// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "mkvparserstream.hpp"

namespace MkvParser
{

class VideoTrack;

class VideoStream : public Stream
{
    explicit VideoStream(VideoTrack*);    
    VideoStream(const VideoStream&);
    VideoStream& operator=(const VideoStream&);
    
public:
    static VideoStream* CreateInstance(VideoTrack*);
    
    void GetMediaTypes(CMediaTypes&) const;
    HRESULT QueryAccept(const AM_MEDIA_TYPE*) const;
    HRESULT UpdateAllocatorProperties(ALLOCATOR_PROPERTIES&) const;
    long GetBufferSize() const;

protected:
    std::wostream& GetKind(std::wostream&) const;
    HRESULT OnPopulateSample(const BlockEntry* pNext, IMediaSample* pSample);

    void GetVp8MediaTypes(CMediaTypes&) const;
    void GetVfwMediaTypes(CMediaTypes&) const;

};


}  //end namespace MkvParser
