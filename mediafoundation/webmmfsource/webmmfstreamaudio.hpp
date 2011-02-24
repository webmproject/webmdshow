#pragma once
#include <list>

namespace WebmMfSourceLib
{

class WebmMfStreamAudio : public WebmMfStream
{
    WebmMfStreamAudio(
        WebmMfSource*,
        IMFStreamDescriptor*,
        const mkvparser::AudioTrack*);

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

    virtual ~WebmMfStreamAudio();

    //HRESULT GetCurrMediaTime(LONGLONG&) const;

    //HRESULT Seek(
    //    const PROPVARIANT& time,
    //    const SeekInfo&,
    //    bool bStart);

    HRESULT Start(const PROPVARIANT&);

    //void SetCurrBlockObject(const mkvparser::Cluster*);
    void SetCurrBlockIndex(const mkvparser::Cluster*);
    bool SetCurrBlockObject();

    bool GetSampleExtent(LONGLONG& pos, LONG& len);
    void GetSampleExtentCompletion();

    long GetNextBlock();
    long NotifyNextCluster(const mkvparser::Cluster*);

    HRESULT GetSample(IUnknown* pToken);
    HRESULT ReadBlock(IMFSample*, const mkvparser::BlockEntry*) const;

protected:

    void OnDeselect();
    void OnSetCurrBlock();

private:

    const mkvparser::BlockEntry* m_pQuota;
    long m_next_index;

    typedef std::list<const mkvparser::BlockEntry*> blocks_t;
    blocks_t m_blocks;
    blocks_t m_sample_extent;
    //LONG m_sample_extent;

};

}  //end namespace WebmMfSourceLib
