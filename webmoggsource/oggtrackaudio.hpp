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
    HRESULT UpdateAllocatorProperties(ALLOCATOR_PROPERTIES&) const;

    HRESULT GetSampleCount(long&);
    HRESULT PopulateSamples(const samples_t&);

protected:
    std::wostream& GetKind(std::wostream&) const;
    std::wstring GetCodecName() const;
    void OnReset();

    oggparser::OggStream::Packet m_ident;
    oggparser::OggStream::Packet m_comment;
    oggparser::OggStream::Packet m_setup;

    oggparser::VorbisIdent m_fmt;
    LONGLONG m_granule_pos;
    LONGLONG m_reftime;

    typedef oggparser::OggStream::packets_t packets_t;
    packets_t m_packets;

    //long GetBufferSize() const;
    //REFERENCE_TIME GetCurrTime() const;

    HRESULT PopulateSample(
        const oggparser::OggStream::Packet&,
        IMediaSample*) const;

};

}  //end namespace WebmOggSource
