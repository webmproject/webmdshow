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
                     public IMFRateControl,
                     public IMFRateSupport,
                     public IMFGetService,
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

    //IMFRateControl

    HRESULT STDMETHODCALLTYPE SetRate(BOOL, float);

    HRESULT STDMETHODCALLTYPE GetRate(BOOL*, float*);

    //IMFRateSupport

    HRESULT STDMETHODCALLTYPE GetSlowestRate(MFRATE_DIRECTION, BOOL, float*);

    HRESULT STDMETHODCALLTYPE GetFastestRate(MFRATE_DIRECTION, BOOL, float*);

    HRESULT STDMETHODCALLTYPE IsRateSupported(BOOL, float, float*);

    //IMFGetService

    HRESULT STDMETHODCALLTYPE GetService(REFGUID, REFIID, LPVOID*);

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
        bool bStart);  //true = start, false = seek

    HRESULT StartStreams(const PROPVARIANT&);
    HRESULT SeekStreams(const PROPVARIANT&);
    HRESULT RestartStreams();

    ULONG m_cEOS;
    float m_rate;

public:

    mkvparser::Segment* m_pSegment;

    enum State { kStateStopped, kStatePaused, kStateStarted };
    State m_state;
    LONGLONG m_preroll_ns;
    //LONGLONG m_thin_ns;
    BOOL m_bThin;

    void NotifyEOS();

};


}  //end namespace WebmMfSourceLib


