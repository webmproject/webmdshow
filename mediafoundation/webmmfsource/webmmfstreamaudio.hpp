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
                    mkvparser::Track*,
                    WebmMfStreamAudio*&);

    mkvparser::AudioTrack* const m_pTrack;

    //HRESULT STDMETHODCALLTYPE RequestSample(IUnknown*);

protected:

    mkvparser::AudioTrack* GetTrack();
    HRESULT OnPopulateSample(const mkvparser::BlockEntry*, IMFSample*);

};

}  //end namespace WebmMfSourceLib
