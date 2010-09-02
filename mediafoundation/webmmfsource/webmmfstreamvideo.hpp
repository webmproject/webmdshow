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

    static HRESULT CreateStream(
                    WebmMfSource*,
                    mkvparser::Track*,
                    WebmMfStreamVideo*&);

    mkvparser::VideoTrack* const m_pTrack;

    //HRESULT STDMETHODCALLTYPE RequestSample(IUnknown*);

protected:

    mkvparser::VideoTrack* GetTrack();
    HRESULT OnPopulateSample(const mkvparser::BlockEntry*, IMFSample*);

};

}  //end namespace WebmMfSourceLib
