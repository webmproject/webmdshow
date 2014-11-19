#pragma once
//#include "webmmfstream.h"

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
                    WebmMfStream*&);

    virtual ~WebmMfStreamVideo();

    bool GetSampleExtent(LONGLONG& pos, LONG& len);
    void GetSampleExtentCompletion();

    //void SetCurrBlockObject(const mkvparser::Cluster*);
    void SetCurrBlockIndex(const mkvparser::Cluster*);
    bool SetCurrBlockObject();

    long GetNextBlock();
    long NotifyNextCluster(const mkvparser::Cluster*);

    HRESULT GetSample(IUnknown* pToken);

protected:

    void OnDeselect();
    void OnSetCurrBlock();

private:

    const mkvparser::BlockEntry* m_pNextBlock;
    long m_next_index;

    static HRESULT GetFrameRate(
        const mkvparser::VideoTrack*,
        UINT32&,
        UINT32&);

};

}  //end namespace WebmMfSourceLib
