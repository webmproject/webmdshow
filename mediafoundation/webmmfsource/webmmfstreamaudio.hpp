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
        const mkvparser::BlockEntry*,
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

    const mkvparser::BlockEntry* m_pCurr;
    //TODO: it sure would be nice to harmonize this ptr
    //between audio and video.  I would prefer that we
    //not do it two different ways.

};

}  //end namespace WebmMfSourceLib
