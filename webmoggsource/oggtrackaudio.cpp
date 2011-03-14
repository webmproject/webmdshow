#include <strmif.h>
#include "oggtrackaudio.hpp"

namespace WebmOggSource
{

OggTrackAudio::OggTrackAudio(
    oggparser::OggStream* pStream,
    ULONG id) :
    OggTrack(pStream, id)
{
}


OggTrackAudio::~OggTrackAudio()
{
}


void OggTrackAudio::GetMediaTypes(CMediaTypes&) const
{
    //TODO
}


HRESULT OggTrackAudio::QueryAccept(const AM_MEDIA_TYPE*) const
{
    return E_FAIL;  //TODO
}


HRESULT OggTrackAudio::UpdateAllocatorProperties(ALLOCATOR_PROPERTIES&) const
{
    return E_FAIL;  //TODO
}


long OggTrackAudio::GetBufferSize() const
{
    return -1;  //TODO
}


std::wostream& OggTrackAudio::GetKind(std::wostream& os) const
{
    return os << L"Audio";
}


std::wstring OggTrackAudio::GetCodecName() const
{
    return L"Vorbis";
}

}  //end namespace WebmOggSource
