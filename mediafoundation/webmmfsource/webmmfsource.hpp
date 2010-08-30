#pragma once

namespace WebmMfSourceLib
{

class WebmMfSource : public IMFMediaSource,
                     public CLockable
{
#if 0
    friend HRESULT CreateSource(
            IClassFactory*,
            IUnknown*,
            const IID&,
            void**);
#else
    friend HRESULT CreateSource(IMFByteStream*, IMFMediaSource**);
#endif

    WebmMfSource(const WebmMfSource&);
    WebmMfSource& operator=(const WebmMfSource&);

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

    //IMFMediaSource

    HRESULT STDMETHODCALLTYPE GetCharacteristics(DWORD*);

    HRESULT STDMETHODCALLTYPE CreatePresentationDescriptor(
        IMFPresentationDescriptor**);

    HRESULT STDMETHODCALLTYPE Start(
        IMFPresentationDescriptor*,
        const GUID*,
        const PROPVARIANT*);

    HRESULT STDMETHODCALLTYPE Stop();

    HRESULT STDMETHODCALLTYPE Pause();

    HRESULT STDMETHODCALLTYPE Shutdown();

private:

    explicit WebmMfSource(IMFByteStream*);
    virtual ~WebmMfSource();

    IMFByteStream* const m_pByteStream;
    LONG m_cRef;
    bool m_bShutdown;    
    IMFMediaEventQueue* m_pEvents;

};


}  //end namespace WebmMfSourceLib


