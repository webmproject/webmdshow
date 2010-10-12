#pragma once
//#include "webmmfstream.hpp"

namespace WebmMfSourceLib
{

class WebmMfStreamVideo : public WebmMfStream
{
    WebmMfStreamVideo(
        WebmMfSource*,
        IMFStreamDescriptor*,
        mkvparser::VideoTrack*);

    virtual ~WebmMfStreamVideo();

    WebmMfStreamVideo(const WebmMfStreamVideo&);
    WebmMfStreamVideo& operator=(const WebmMfStreamVideo&);

public:

    static HRESULT CreateStreamDescriptor(
                    mkvparser::Track*,
                    IMFStreamDescriptor*&);

    static HRESULT CreateStream(
                    IMFStreamDescriptor*,
                    WebmMfSource*,
                    mkvparser::Track*,
                    WebmMfStream*&);

    struct SeekInfo
    {
        mkvparser::Cluster* pCluster;
        const mkvparser::BlockEntry* pBlockEntry;
        const mkvparser::CuePoint* pCP;
        const mkvparser::CuePoint::TrackPosition* pTP;
    };

    HRESULT GetCurrMediaTime(LONGLONG&) const;
    void GetCluster(LONGLONG, SeekInfo&) const;

    HRESULT Start(
        const PROPVARIANT& time,
        const SeekInfo&);

    HRESULT Seek(
        const PROPVARIANT& time,
        const SeekInfo&);

protected:

    const mkvparser::BlockEntry* GetCurrBlock() const;
    HRESULT PopulateSample(IMFSample*);

private:

    bool m_bDiscontinuity;
    SeekInfo m_curr;


};

}  //end namespace WebmMfSourceLib
