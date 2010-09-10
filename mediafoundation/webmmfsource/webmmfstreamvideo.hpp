#pragma once
//#include "webmmfstream.hpp"

namespace WebmMfSourceLib
{

class WebmMfStreamVideo : public WebmMfStream
{
    WebmMfStreamVideo(
        IClassFactory*,
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
