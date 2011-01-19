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
                    //ULONG context_key,
                    //ULONG stream_key,
                    WebmMfStream*&);

    virtual ~WebmMfStreamVideo();

    HRESULT GetCurrMediaTime(LONGLONG&) const;
    //void GetSeekInfo(LONGLONG, SeekInfo&) const;

    //HRESULT Seek(
    //    const PROPVARIANT& time,
    //    const SeekInfo&,
    //    bool bStart);

    HRESULT Start(const PROPVARIANT&);

    void SetRate(BOOL, float);

    HRESULT GetSample(IUnknown* pToken);

private:

    float m_rate;
    LONGLONG m_thin_ns;

    static HRESULT GetFrameRate(
        const mkvparser::VideoTrack*,
        UINT32&,
        UINT32&);

};

}  //end namespace WebmMfSourceLib
