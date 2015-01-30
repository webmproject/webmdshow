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

class AudioTrack;
class AudioStream : public Stream
{
    explicit AudioStream(const AudioTrack*);
    AudioStream(const AudioStream&);
    AudioStream& operator=(const AudioStream&);

public:
    static AudioStream* CreateInstance(const AudioTrack*);

    void GetMediaTypes(CMediaTypes&) const;
    HRESULT QueryAccept(const AM_MEDIA_TYPE*) const;

#if 0  //if we decide to support Xiph Ogg Vorbis decoder filter:
    HRESULT SetConnectionMediaType(const AM_MEDIA_TYPE&);
#endif

    //HRESULT UpdateAllocatorProperties(ALLOCATOR_PROPERTIES&) const;

protected:
    std::wostream& GetKind(std::wostream&) const;

    long GetBufferSize() const;
    long GetBufferCount() const;

    void OnPopulateSample(const BlockEntry*, const samples_t&) const;

    void GetVorbisMediaTypes(CMediaTypes&) const;

#if 0  //if we decide to support Xiph Ogg Vorbis decoder filter:
    bool SendPreroll(IMediaSample*);
    bool (AudioStream::*m_preroll)(IMediaSample*);
    bool DoNothing(IMediaSample*);
    bool SendOggIdentPacket(IMediaSample*);
    bool SendOggCommentPacket(IMediaSample*);
    bool SendOggSetupPacket(IMediaSample*);
#endif

    BYTE GetChannels() const;
    ULONG GetSamplesPerSec() const;
    BYTE GetBitsPerSample() const;

};


}  //end namespace mkvparser
