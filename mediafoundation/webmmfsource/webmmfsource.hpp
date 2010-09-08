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
    friend HRESULT CreateSource(
        IClassFactory*,
        IMFByteStream*,
        IMFMediaSource**);

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

    WebmMfSource(IClassFactory*, IMFByteStream*);
    virtual ~WebmMfSource();

    HRESULT Load();

    IClassFactory* const m_pClassFactory;
    MkvReader m_file;
    LONG m_cRef;
    IMFMediaEventQueue* m_pEvents;
    IMFPresentationDescriptor* m_pDesc;

    typedef std::vector<WebmMfStream*> streams_t;
    streams_t m_streams;

    void CreateVideoStream(mkvparser::Track*);
    void CreateAudioStream(mkvparser::Track*);

public:

    bool m_bShutdown;
    mkvparser::Segment* m_pSegment;

    enum State { kStateStopped, kStatePaused, kStateStarted };
    State m_state;

};


}  //end namespace WebmMfSourceLib


