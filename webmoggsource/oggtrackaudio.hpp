#pragma once
#include "oggtrack.hpp"

namespace WebmOggSource
{

class OggTrackAudio : public OggTrack
{
    OggTrackAudio(const OggTrackAudio&);
    OggTrackAudio& operator=(const OggTrackAudio&);

public:
    OggTrackAudio(oggparser::OggStream*, ULONG);
    virtual ~OggTrackAudio();

    void GetMediaTypes(CMediaTypes&) const;
    HRESULT QueryAccept(const AM_MEDIA_TYPE*) const;

    HRESULT UpdateAllocatorProperties(ALLOCATOR_PROPERTIES&) const;

    long GetBufferSize() const;

protected:
    std::wostream& GetKind(std::wostream&) const;
    std::wstring GetCodecName() const;


};

}  //end namespace WebmOggSource
