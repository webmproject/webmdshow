#pragma once

namespace WebmMfSourceLib
{

class WebmMfStreamAudio : public WebmMfStream
{
    WebmMfStreamAudio(
        WebmMfSource*,
        IMFStreamDescriptor*,
        mkvparser::AudioTrack*);

    virtual ~WebmMfStreamAudio();

    WebmMfStreamAudio(const WebmMfStreamAudio&);
    WebmMfStreamAudio& operator=(const WebmMfStreamAudio&);

public:

    static HRESULT CreateStream(
                    WebmMfSource*,
                    mkvparser::AudioTrack*,
                    WebmMfStreamAudio*&);

protected:

    HRESULT OnPopulateSample(const mkvparser::BlockEntry*, IMFSample*);

};

}  //end namespace WebmMfSourceLib
