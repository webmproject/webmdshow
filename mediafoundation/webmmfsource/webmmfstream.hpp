#pragma once
#include <list>

namespace WebmMfSourceLib
{

//class WebmMfSource;

class WebmMfStream : public IMFMediaStream
{
    WebmMfStream(const WebmMfStream&);
    WebmMfStream& operator=(const WebmMfStream&);

public:

protected:

    WebmMfStream(
        WebmMfSource*,
        IMFStreamDescriptor*,
        const mkvparser::Track*);
        //ULONG context_key,
        //ULONG stream_key);

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

    HRESULT GetCurrMediaTime(LONGLONG&) const;  //not presentation time!

    HRESULT Select();
    HRESULT Deselect();
    bool IsSelected() const;

    HRESULT Update();
    HRESULT Restart();

    HRESULT Stop();
    HRESULT Pause();
    HRESULT Shutdown();

    struct SeekInfo
    {
        const mkvparser::BlockEntry* pBE;
        const mkvparser::CuePoint* pCP;
        const mkvparser::CuePoint::TrackPosition* pTP;

        void Init();
    };

    //virtual HRESULT Seek(
    //    const PROPVARIANT& time,
    //    const SeekInfo&,
    //    bool bStart) = 0;
    virtual HRESULT Start(const PROPVARIANT&) = 0;

    virtual void SetRate(BOOL, float) = 0;

    bool IsEOS() const;
    HRESULT SetEOS();

    virtual HRESULT GetSample(IUnknown*) = 0;

    void SetCurrBlock(const mkvparser::BlockEntry*);  //TODO: pass cue point
    const mkvparser::BlockEntry* GetCurrBlock() const;
    bool IsCurrBlockLocked() const;

    int LockCurrBlock();
    HRESULT NotifyNextCluster(const mkvparser::Cluster*);

    ULONG m_cRef;
    WebmMfSource* const m_pSource;
    IMFStreamDescriptor* const m_pDesc;
    const mkvparser::Track* const m_pTrack;
    //const ULONG m_context_key;
    //const ULONG m_stream_key;

protected:

    HRESULT OnStart(const PROPVARIANT& time);
    HRESULT OnSeek(const PROPVARIANT& time);

    HRESULT GetNextBlock();
    HRESULT ProcessSample(IMFSample*);

    SeekInfo m_curr;
    const mkvparser::BlockEntry* m_pLocked;
    const mkvparser::BlockEntry* m_pNextBlock;
    bool m_bDiscontinuity;

private:

    IMFMediaEventQueue* m_pEvents;

    typedef std::list<IMFSample*> samples_t;
    samples_t m_samples;

    bool m_bSelected;
    bool m_bEOS;  //indicates whether we have posted EOS event

    void PurgeSamples();
    void DeliverSamples();

};

}  //end namespace WebmMfSourceLib
