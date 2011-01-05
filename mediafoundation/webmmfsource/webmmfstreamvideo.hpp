#pragma once
//#include "webmmfstream.hpp"

namespace WebmMfSourceLib
{

class WebmMfStreamVideo : public WebmMfStream
{
    WebmMfStreamVideo(
        WebmMfSource*,
        IMFStreamDescriptor*,
        const mkvparser::VideoTrack*);

    virtual ~WebmMfStreamVideo();

    WebmMfStreamVideo(const WebmMfStreamVideo&);
    WebmMfStreamVideo& operator=(const WebmMfStreamVideo&);

public:

    static HRESULT CreateStreamDescriptor(
                    const mkvparser::Track*,
                    IMFStreamDescriptor*&);

    static HRESULT CreateStream(
                    IMFStreamDescriptor*,
                    WebmMfSource*,
                    const mkvparser::Track*,
                    WebmMfStream*&);

    struct SeekInfo
    {
        const mkvparser::BlockEntry* pBE;
        const mkvparser::CuePoint* pCP;
        const mkvparser::CuePoint::TrackPosition* pTP;
    };

    HRESULT GetCurrMediaTime(LONGLONG&) const;
    void GetSeekInfo(LONGLONG, SeekInfo&) const;

    HRESULT Seek(
        const PROPVARIANT& time,
        const SeekInfo&,
        bool bStart);

    void SetRate(BOOL, float);

    const mkvparser::BlockEntry* GetCurrBlock() const;

    bool IsEOS() const;
    HRESULT GetSample(IUnknown* pToken);
    //const mkvparser::Cluster* GetCurrCluster() const;

    int LockCurrBlock();

protected:

    //void SetCurrBlock(const mkvparser::BlockEntry*);
    void OnStop();

private:

    bool m_bDiscontinuity;
    SeekInfo m_curr;
    float m_rate;
    LONGLONG m_thin_ns;

    static HRESULT GetFrameRate(
        const mkvparser::VideoTrack*,
        UINT32&,
        UINT32&);

};

}  //end namespace WebmMfSourceLib
