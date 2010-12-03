// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webmmuxstreamaudio.hpp"
#include "webmmuxcontext.hpp"

#include <vector>

class CMediaTypes;

namespace VorbisTypes
{
struct VORBISFORMAT;
}

namespace WebmMuxLib
{

class StreamAudioVorbisOgg : public StreamAudio
{
    StreamAudioVorbisOgg(const StreamAudioVorbisOgg&);
    StreamAudioVorbisOgg& operator=(const StreamAudioVorbisOgg&);

protected:
    StreamAudioVorbisOgg(Context&, const BYTE*, ULONG);
    //virtual void Final();  //grant last wishes

    ULONG GetSamplesPerSec() const;
    BYTE GetChannels() const;

    void WriteTrackCodecID();
    void WriteTrackCodecName();
    void WriteTrackCodecPrivate();

public:
    static void GetMediaTypes(CMediaTypes&);
    static bool QueryAccept(const AM_MEDIA_TYPE&);

    //static HRESULT GetAllocatorRequirements(
    //                const AM_MEDIA_TYPE&,
    //                ALLOCATOR_PROPERTIES&);

    static StreamAudio* CreateStream(Context&, const AM_MEDIA_TYPE&);

    HRESULT Receive(IMediaSample*);
    int EndOfStream();

private:
    typedef std::vector<BYTE> header_t;
    header_t m_ident;
    header_t m_comment;
    header_t m_setup;

    const VorbisTypes::VORBISFORMAT& GetFormat() const;

    class VorbisFrame : public AudioFrame
    {
        VorbisFrame& operator=(const VorbisFrame&);
        VorbisFrame(const VorbisFrame&);

    private:
        ULONG m_timecode;
        ULONG m_duration;
        BYTE* m_data;
        ULONG m_size;

    public:
        VorbisFrame(IMediaSample*, StreamAudioVorbisOgg*);
        ~VorbisFrame();

        ULONG GetTimecode() const;
        ULONG GetDuration() const;

        ULONG GetSize() const;
        const BYTE* GetData() const;
    };

    __int64 m_codec_private_data_pos;

    HRESULT FinalizeTrackCodecPrivate();
};

}  //end namespace WebmMuxLib
