#pragma once

namespace WebmMfSourceLib
{

class WebmMfStreamAudio : public WebmMfStream
{
    WebmMfStreamAudio(
        WebmMfSource*,
        IMFStreamDescriptor*,
        const mkvparser::AudioTrack*);

    virtual ~WebmMfStreamAudio();

    WebmMfStreamAudio(const WebmMfStreamAudio&);
    WebmMfStreamAudio& operator=(const WebmMfStreamAudio&);

public:

    static HRESULT CreateStreamDescriptor(
                    const mkvparser::Track*,
                    IMFStreamDescriptor*&);

    static HRESULT CreateStream(
                    IMFStreamDescriptor*,
                    WebmMfSource*,
                    const mkvparser::Track*,
                    WebmMfStream*&);

    HRESULT GetCurrMediaTime(LONGLONG&) const;

    HRESULT Seek(
        const PROPVARIANT& time,
        const SeekInfo&,
        bool bStart);

    void SetRate(BOOL, float);

    HRESULT GetSample(IUnknown* pToken);

};

}  //end namespace WebmMfSourceLib
