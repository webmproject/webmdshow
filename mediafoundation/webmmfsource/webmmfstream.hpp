#pragma once
#include <list>

namespace WebmMfSourceLib
{

//class WebmMfSource;

class WebmMfStream : public IMFMediaStream
{
    WebmMfStream(const WebmMfStream&);
    WebmMfStream& operator=(const WebmMfStream&);

protected:

    WebmMfStream(
        WebmMfSource*,
        IMFStreamDescriptor*,
        const mkvparser::Track*);

public:

    virtual ~WebmMfStream();

    //IUnknown

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    //IMFMediaEventGenerator

    HRESULT STDMETHODCALLTYPE GetEvent(
        DWORD dwFlags,
        IMFMediaEvent**);

    HRESULT STDMETHODCALLTYPE BeginGetEvent(
        IMFAsyncCallback*,
        IUnknown*);

    HRESULT STDMETHODCALLTYPE EndGetEvent(
        IMFAsyncResult*,
        IMFMediaEvent**);

    HRESULT STDMETHODCALLTYPE QueueEvent(
        MediaEventType met,
        REFGUID guidExtendedType,
        HRESULT hrStatus,
        const PROPVARIANT*);

    //IMFMediaStream

    HRESULT STDMETHODCALLTYPE GetMediaSource(IMFMediaSource**);
    HRESULT STDMETHODCALLTYPE GetStreamDescriptor(IMFStreamDescriptor**);
    HRESULT STDMETHODCALLTYPE RequestSample(IUnknown*);

    //Local methods and properties

    HRESULT GetCurrTime(LONGLONG& time_ns) const;

    MediaEventType Select();
    HRESULT Deselect();
    bool IsSelected() const;
    bool IsShutdown() const;

    //HRESULT Update();
    HRESULT Restart();

    HRESULT Stop();
    HRESULT Pause();
    HRESULT Shutdown();

    struct SeekInfo
    {
        long index;
        const mkvparser::Cluster* pCluster;
        const mkvparser::BlockEntry* pBE;
        const mkvparser::CuePoint* pCP;
        const mkvparser::CuePoint::TrackPosition* pTP;

        void Init(const mkvparser::BlockEntry* pBE = 0);
    };

    HRESULT Start(const PROPVARIANT&);

    //TODO: implement thinning
    HRESULT Seek(const PROPVARIANT&);

    //virtual void SetRate(BOOL, float) = 0;
    void SetRate(BOOL, float);

    //bool IsCurrBlockEOS() const;
    bool GetEOS() const;
    HRESULT SetEOS();

    virtual HRESULT GetSample(IUnknown*) = 0;

#if 0
    HRESULT SetFirstBlock(const mkvparser::Cluster*);
#else
    HRESULT SetFirstBlock(const mkvparser::BlockEntry*);
#endif

    const mkvparser::BlockEntry* GetFirstBlock() const;

    void SetCurrBlock(const mkvparser::BlockEntry*);

    void SetCurrBlockInit(LONGLONG time_ns, LONGLONG cluster_pos);
    //bool HaveCurrBlockObject(LONGLONG& cluster_pos) const;
    //bool HaveCurrBlockObject() const;
    //const mkvparser::Cluster* GetCurrBlockCluster();
    LONGLONG GetCurrBlockClusterPosition() const;

    //virtual void SetCurrBlockObject(const mkvparser::Cluster*) = 0;
    virtual void SetCurrBlockIndex(const mkvparser::Cluster*) = 0;
    virtual bool SetCurrBlockObject() = 0;

    const mkvparser::BlockEntry* GetCurrBlock() const;

    //bool IsCurrBlockLocked(LONGLONG& pos, LONG& len) const;
    bool IsCurrBlockLocked() const;

    virtual bool GetSampleExtent(LONGLONG& pos, LONG& len) = 0;
    virtual void GetSampleExtentCompletion() = 0;

    virtual long GetNextBlock() = 0;
    virtual long NotifyNextCluster(const mkvparser::Cluster*) = 0;

    int LockCurrBlock();

    ULONG m_cRef;
    WebmMfSource* const m_pSource;
    IMFStreamDescriptor* const m_pDesc;
    const mkvparser::Track* const m_pTrack;

protected:

    HRESULT OnSeek(const PROPVARIANT& time);

    //bool HaveCurrBlockIndex(LONGLONG& cluster_pos) const;

    HRESULT ProcessSample(IMFSample*);

    virtual void OnDeselect() = 0;
    virtual void OnSetCurrBlock() = 0;

    SeekInfo m_curr;
    const mkvparser::BlockEntry* m_pFirstBlock;
    //const mkvparser::BlockEntry* m_pNextBlock;
    const mkvparser::BlockEntry* m_pLocked;
    bool m_bDiscontinuity;

    //to lazy-init the curr block entry following a start/seek:
    LONGLONG m_time_ns;
    LONGLONG m_cluster_pos;

    float m_rate;
    LONGLONG m_thin_ns;

private:

    IMFMediaEventQueue* m_pEvents;

    typedef std::list<IMFSample*> samples_t;
    samples_t m_samples;

    int m_bSelected;
    bool m_bEOS;  //indicates whether we have posted EOS event

    void PurgeSamples();
    void DeliverSamples();

};

}  //end namespace WebmMfSourceLib
