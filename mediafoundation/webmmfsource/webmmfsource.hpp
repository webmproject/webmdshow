#pragma once
#include <mfidl.h>
#include "clockable.hpp"
#include "mkvreader.hpp"
#include <vector>
#include <map>
#include <string>
#ifndef _INC_COMDEF
#include <comdef.h>
#endif

namespace WebmMfSourceLib
{

class WebmMfStream;
class WebmMfByteStreamHandler;

class WebmMfSource : public IMFMediaSource,
                     public IMFRateControl,
                     public IMFRateSupport,
                     public IMFGetService,
                     public CLockable
{
    WebmMfSource(const WebmMfSource&);
    WebmMfSource& operator=(const WebmMfSource&);

public:

    static HRESULT CreateSource(
            IClassFactory*, 
            IMFByteStream*,
            WebmMfSource*&);

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

    //local methods

    HRESULT BeginLoad(IMFAsyncCallback*);
    HRESULT EndLoad(IMFAsyncResult*);

    HRESULT RequestSample(WebmMfStream*, IUnknown*);

private:

    static std::wstring ConvertFromUTF8(const char*);

    WebmMfSource(IClassFactory*, IMFByteStream*);
    virtual ~WebmMfSource();

    //HRESULT SyncLoad();
    LONGLONG GetDuration() const;

    IClassFactory* const m_pClassFactory;
    LONG m_cRef;
    IMFMediaEventQueue* m_pEvents;

    _COM_SMARTPTR_TYPEDEF(IMFAsyncResult, __uuidof(IMFAsyncResult));
    IMFAsyncResultPtr m_pLoadResult;

    typedef std::vector<IMFStreamDescriptor*> stream_descriptors_t;
    stream_descriptors_t m_stream_descriptors;

    typedef std::map<ULONG, WebmMfStream*> streams_t;
    streams_t m_streams;

    HRESULT NewStream(IMFStreamDescriptor*, const mkvparser::Track*);
    HRESULT UpdateStream(WebmMfStream*);

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

public:

    MkvReader m_file;
    mkvparser::Segment* m_pSegment;

    enum State { kStateStopped, kStatePaused, kStateStarted };
    State m_state;
    LONGLONG m_preroll_ns;

private:

    BOOL m_bThin;
    float m_rate;

    struct Request
    {
        WebmMfStream* pStream;
        IUnknown* pToken;
    };

    void NotifyEOS();

    class CAsyncRead : public IMFAsyncCallback
    {
        CAsyncRead(const CAsyncRead&);
        CAsyncRead& operator=(const CAsyncRead&);

    public:
        explicit CAsyncRead(WebmMfSource*);
        virtual ~CAsyncRead();

        WebmMfSource* const m_pSource;
        HRESULT m_hrStatus;  //of calling MkvReader::AsyncReadContinue

        HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
        ULONG STDMETHODCALLTYPE AddRef();
        ULONG STDMETHODCALLTYPE Release();

        HRESULT STDMETHODCALLTYPE GetParameters(DWORD*, DWORD*);
        HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult*);
    };

    CAsyncRead m_async_read;

    typedef std::list<Request> requests_t;
    requests_t m_requests;

    HANDLE m_hThread;
    HANDLE m_hQuit;
    HANDLE m_hRequestSample;
    HANDLE m_hAsyncRead;

    HRESULT InitThread();
    void FinalThread();

    static unsigned __stdcall ThreadProc(void*);
    unsigned Main();

    typedef bool (WebmMfSource::*thread_state_t)();
    thread_state_t m_thread_state;

    bool StateAsyncRead();
    bool StateRequestSample();
    bool StateQuit();

    thread_state_t OnAsyncRead();
    thread_state_t OnRequestSample();

    const mkvparser::Cluster* m_pCurr;
    const mkvparser::Cluster* m_pNext;

    //for async load:
    thread_state_t StateAsyncParseEbmlHeader();
    thread_state_t StateAsyncParseSegmentHeaders();
    thread_state_t StateAsyncInitStreams();

    //for async locking:
    thread_state_t StateAsyncLockCurr();

    //for async cluster parsing:
    thread_state_t StateAsyncParseNext();
    thread_state_t StateAsyncLoadNext();

    typedef thread_state_t (WebmMfSource::*async_state_t)();
    async_state_t m_async_state;

    HRESULT ParseEbmlHeader(LONGLONG&);
    void PurgeCache();
    
    thread_state_t LoadComplete(HRESULT);

};


}  //end namespace WebmMfSourceLib


