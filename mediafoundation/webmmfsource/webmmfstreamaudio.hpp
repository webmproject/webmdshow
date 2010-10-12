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

    static HRESULT CreateStreamDescriptor(
                    mkvparser::Track*,
                    IMFStreamDescriptor*&);

    static HRESULT CreateStream(
                    IMFStreamDescriptor*,
                    WebmMfSource*,
                    mkvparser::Track*,
                    WebmMfStream*&);

    HRESULT GetCurrMediaTime(LONGLONG&) const;

    HRESULT Start(
        const PROPVARIANT& time,
        const mkvparser::BlockEntry*);

    HRESULT Seek(
        const PROPVARIANT& time,
        const mkvparser::BlockEntry*);

protected:

    const mkvparser::BlockEntry* GetCurrBlock() const;
    HRESULT PopulateSample(IMFSample*);

private:

    bool m_bDiscontinuity;
    const mkvparser::BlockEntry* m_pCurr;

};

}  //end namespace WebmMfSourceLib
