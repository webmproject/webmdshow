#pragma once

namespace WebmMfSourceLib
{

class WebmMfStreamAudio : public WebmMfStream
{
    WebmMfStreamAudio(
        IClassFactory*,
        WebmMfSource*,
        IMFStreamDescriptor*,
        mkvparser::AudioTrack*);

    virtual ~WebmMfStreamAudio();

    WebmMfStreamAudio(const WebmMfStreamAudio&);
    WebmMfStreamAudio& operator=(const WebmMfStreamAudio&);

public:

    static HRESULT CreateStreamDescriptor(
                    mkvparser::Track*,
                    IMFStreamDescriptor*&);

    static HRESULT CreateStream(
                    IClassFactory*,
                    IMFStreamDescriptor*,
                    WebmMfSource*,
                    mkvparser::Track*,
                    LONGLONG time,
                    WebmMfStream*&);

protected:

    HRESULT OnPopulateSample(const mkvparser::BlockEntry*, IMFSample*);

};

}  //end namespace WebmMfSourceLib
