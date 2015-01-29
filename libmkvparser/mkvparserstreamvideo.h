// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "mkvparserstream.h"

namespace mkvparser
{

class VideoTrack;

class VideoStream : public Stream
{
    explicit VideoStream(const VideoTrack*);
    VideoStream(const VideoStream&);
    VideoStream& operator=(const VideoStream&);

public:
    static VideoStream* CreateInstance(const VideoTrack*);

    void GetMediaTypes(CMediaTypes&) const;
    HRESULT QueryAccept(const AM_MEDIA_TYPE*) const;
    //HRESULT UpdateAllocatorProperties(ALLOCATOR_PROPERTIES&) const;

protected:
    std::wostream& GetKind(std::wostream&) const;

    long GetBufferSize() const;
    long GetBufferCount() const;

    void OnPopulateSample(const BlockEntry*, const samples_t&) const;

    void GetVpxMediaTypes(const GUID& subtype, CMediaTypes&) const;
    void GetVfwMediaTypes(CMediaTypes&) const;

};


}  //end namespace mkvparser
