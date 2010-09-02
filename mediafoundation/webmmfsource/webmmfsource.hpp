#pragma once
#include <mfidl.h>
#include "clockable.hpp"
#include "mkvreader.hpp"
#include <vector>
#include <string>

namespace WebmMfSourceLib
{

class WebmMfStream;

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

    static std::wstring ConvertFromUTF8(const char*);

    explicit WebmMfSource(IMFByteStream*);
    virtual ~WebmMfSource();

    HRESULT Load();

    //IMFByteStream* const m_pByteStream;
    MkvReader m_file;
    LONG m_cRef;
    IMFMediaEventQueue* m_pEvents;

    typedef std::vector<WebmMfStream*> streams_t;
    streams_t m_streams;

    void CreateVideoStream(mkvparser::Track*);
    void CreateAudioStream(mkvparser::Track*);

public:

    bool m_bShutdown;
    mkvparser::Segment* m_pSegment;

};


}  //end namespace WebmMfSourceLib


