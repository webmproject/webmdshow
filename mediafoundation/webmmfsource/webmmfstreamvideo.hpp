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
                    mkvparser::VideoTrack*,
                    WebmMfStreamVideo*&);

protected:

    HRESULT OnPopulateSample(const mkvparser::BlockEntry*, IMFSample*);

};

}  //end namespace WebmMfSourceLib
