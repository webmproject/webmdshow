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
        mkvparser::Track*);

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

    //virtual HRESULT Seek(LONGLONG) = 0;
    HRESULT GetCurrMediaTime(LONGLONG&) const;  //not presentation time!

    HRESULT Select(LONGLONG);
    HRESULT Deselect();
    bool IsSelected() const;

    HRESULT Start(const PROPVARIANT& time);
    HRESULT Seek(const PROPVARIANT& time);
    HRESULT Restart();

    HRESULT Stop();
    HRESULT Pause();
    HRESULT Shutdown();

    WebmMfSource* const m_pSource;
    IMFStreamDescriptor* const m_pDesc;
    mkvparser::Track* const m_pTrack;

protected:

    virtual HRESULT OnPopulateSample(
                        const mkvparser::BlockEntry*,
                        IMFSample*) = 0;

    mkvparser::Cluster* m_pBaseCluster;
    const mkvparser::BlockEntry* m_pCurr;
    bool m_bDiscontinuity;

private:

    HRESULT Preload();
    HRESULT PopulateSample(IMFSample*);

    IMFMediaEventQueue* m_pEvents;

    typedef std::list<IMFSample*> samples_t;
    samples_t m_samples;

    bool m_bSelected;
    bool m_bEOS;  //indicates whether we have posted EOS event

    void PurgeSamples();
    void DeliverSamples();
    void SetCurrPos(LONGLONG);

};

}  //end namespace WebmMfSourceLib
