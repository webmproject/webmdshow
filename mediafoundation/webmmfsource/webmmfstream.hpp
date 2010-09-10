#pragma once

namespace WebmMfSourceLib
{

//class WebmMfSource;

class WebmMfStream : public IMFMediaStream
{
    WebmMfStream(const WebmMfStream&);
    WebmMfStream& operator=(const WebmMfStream&);

protected:

    WebmMfStream(
        IClassFactory*,
        WebmMfSource*,
        IMFStreamDescriptor*,
        mkvparser::Track*);

    virtual ~WebmMfStream();

public:

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

    HRESULT Stop();
    HRESULT Pause();
    ULONG Shutdown();

    IClassFactory* const m_pClassFactory;
    LONG m_cRef;
    WebmMfSource* const m_pSource;
    WebmMfSource::State m_state;
    IMFStreamDescriptor* const m_pDesc;
    mkvparser::Track* const m_pTrack;

protected:

    virtual HRESULT OnPopulateSample(
                        const mkvparser::BlockEntry*,
                        IMFSample*) = 0;

    mkvparser::Cluster* m_pBaseCluster;
    const mkvparser::BlockEntry* m_pCurr;
    const mkvparser::BlockEntry* m_pStop;
    bool m_bDiscontinuity;

private:

    HRESULT Preload();
    HRESULT PopulateSample(IMFSample*);

    IMFMediaEventQueue* m_pEvents;

};

}  //end namespace WebmMfSourceLib
