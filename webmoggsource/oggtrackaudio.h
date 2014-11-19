#pragma once
#include "oggtrack.hpp"
#include "oggparser.hpp"

namespace WebmOggSource
{

class OggTrackAudio : public OggTrack
{
    OggTrackAudio(const OggTrackAudio&);
    OggTrackAudio& operator=(const OggTrackAudio&);

private:
    OggTrackAudio(oggparser::OggStream*, ULONG);
    HRESULT Init();

public:
    static HRESULT Create(oggparser::OggStream*, OggTrackAudio*&);
    virtual ~OggTrackAudio();

    void GetMediaTypes(CMediaTypes&) const;
    HRESULT QueryAccept(const AM_MEDIA_TYPE*) const;
    HRESULT SetConnectionMediaType(const AM_MEDIA_TYPE&);
    HRESULT UpdateAllocatorProperties(ALLOCATOR_PROPERTIES&) const;

    HRESULT GetPackets(long&);
    HRESULT PopulateSamples(const samples_t&);

protected:
    std::wostream& GetKind(std::wostream&) const;
    std::wstring GetCodecName() const;
    void OnReset();
    long GetPackets();

    oggparser::OggStream::Packet m_ident;
    oggparser::OggStream::Packet m_comment;
    oggparser::OggStream::Packet m_setup;

    oggparser::VorbisIdent m_fmt;
    LONGLONG m_granule_pos;
    LONGLONG m_reftime;
    GUID m_subtype;

    long (OggTrackAudio::*m_pfnGetSampleCount)() const;
    long GetSampleCountVorbis2() const;
    long GetSampleCountVorbis2XiphLacing() const;

    HRESULT (OggTrackAudio::*m_pfnPopulateSamples)(const samples_t&);
    HRESULT PopulateSamplesVorbis2(const samples_t&);
    HRESULT PopulateSamplesVorbis2XiphLacing(const samples_t&);

    typedef oggparser::OggStream::packets_t packets_t;
    packets_t m_packets;

    HRESULT PopulateSample(
        const oggparser::OggStream::Packet&,
        IMediaSample*) const;

};

}  //end namespace WebmOggSource
