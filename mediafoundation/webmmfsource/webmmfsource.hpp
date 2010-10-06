#pragma once
#include <mfidl.h>
#include "clockable.hpp"
#include "mkvreader.hpp"
#include <vector>
#include <map>
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
    LONG m_cRef;
    MkvReader m_file;
    IMFMediaEventQueue* m_pEvents;

    typedef std::vector<IMFStreamDescriptor*> stream_descriptors_t;
    stream_descriptors_t m_stream_descriptors;

    typedef std::map<ULONG, WebmMfStream*> streams_t;
    streams_t m_streams;

    HRESULT NewStream(IMFStreamDescriptor*, mkvparser::Track*);
    HRESULT UpdateStream(IMFStreamDescriptor*, WebmMfStream*);

    void GetTime(
        IMFPresentationDescriptor*,
        const PROPVARIANT& request,
        PROPVARIANT& actual) const;

    void WebmMfSource::Seek(
        const PROPVARIANT& var,
        HRESULT (WebmMfStream::*pfn)(
            const PROPVARIANT&,
            mkvparser::Cluster*,
            const mkvparser::BlockEntry*));

    HRESULT StartStreams(const PROPVARIANT&);
    HRESULT SeekStreams(const PROPVARIANT&);
    HRESULT RestartStreams();

    //void GetBaseCluster(LONGLONG, mkvparser::Cluster*&);
    //void SetCurrPos(LONGLONG);

    ULONG m_cEOS;

public:

    mkvparser::Segment* m_pSegment;

    enum State { kStateStopped, kStatePaused, kStateStarted };
    State m_state;
    LONGLONG m_preroll_ns;

    void NotifyEOS();

};


}  //end namespace WebmMfSourceLib


