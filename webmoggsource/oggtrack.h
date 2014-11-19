#pragma once
#include <string>
#include <vector>
//#include "cmediatypes.h"

namespace oggparser
{
class OggStream;
}

class CMediaTypes;

namespace WebmOggSource
{

class OggTrack
{
    OggTrack(const OggTrack&);
    OggTrack& operator=(const OggTrack&);

protected:
    oggparser::OggStream* const m_pStream;
    const ULONG m_id;
    OggTrack(oggparser::OggStream*, ULONG);

public:
    virtual ~OggTrack();

    void Reset();
    //void Stop();

    std::wstring GetId() const;    //IPin::QueryId
    std::wstring GetName() const;  //IPin::QueryPinInfo
    virtual void GetMediaTypes(CMediaTypes&) const = 0;
    virtual HRESULT QueryAccept(const AM_MEDIA_TYPE*) const = 0;

    virtual HRESULT SetConnectionMediaType(const AM_MEDIA_TYPE&) = 0;
    virtual HRESULT UpdateAllocatorProperties(ALLOCATOR_PROPERTIES&) const = 0;

    virtual HRESULT GetPackets(long& count) = 0;

    typedef std::vector<IMediaSample*> samples_t;

    virtual HRESULT PopulateSamples(const samples_t&) = 0;
    static void Clear(samples_t&);

protected:
    bool m_bDiscontinuity;
    //const BlockEntry* m_pCurr;
    //const BlockEntry* m_pStop;
    //const Cluster* m_pBase;
    //LONGLONG m_base_time_ns;

    virtual std::wostream& GetKind(std::wostream&) const = 0;
    virtual std::wstring GetCodecName() const = 0;
    virtual void OnReset() = 0;

    //HRESULT InitCurr();

    //virtual void OnPopulateSample(
    //            const BlockEntry*,
    //            const samples_t&) const = 0;

//private:
//    const BlockEntry* m_pLocked;
//    void SetCurr(const mkvparser::BlockEntry*);

};

}  //end namespace WebmOggSource
