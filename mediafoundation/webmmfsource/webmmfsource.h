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
    //HRESULT EndLoad(IMFAsyncResult*);

    bool IsStopped() const;
    bool IsPaused() const;
    HRESULT RequestSample(WebmMfStream*, IUnknown*);

private:

    static std::wstring ConvertFromUTF8(const char*);

    WebmMfSource(IClassFactory*, IMFByteStream*);
    virtual ~WebmMfSource();

    LONGLONG GetDuration() const;

    IClassFactory* const m_pClassFactory;
    LONG m_cRef;
    IMFMediaEventQueue* m_pEvents;

    _COM_SMARTPTR_TYPEDEF(IMFAsyncResult, __uuidof(IMFAsyncResult));
    IMFAsyncResultPtr m_pLoadResult;

    typedef std::vector<IMFStreamDescriptor*> stream_descriptors_t;
    stream_descriptors_t m_stream_descriptors;

    HRESULT CreateStream(IMFStreamDescriptor*, const mkvparser::Track*);

    //HRESULT UpdatedStream(WebmMfStream*);
    HRESULT QueueStreamEvent(MediaEventType, WebmMfStream*) const;

    void GetTime(
        IMFPresentationDescriptor*,
        const PROPVARIANT& request,
        PROPVARIANT& actual) const;

public:

    MkvReader m_file;
    mkvparser::Segment* m_pSegment;
    LONGLONG m_preroll_ns;

private:

    BOOL m_bThin;
    float m_rate;

    struct Request
    {
        WebmMfStream* pStream;
        IUnknown* pToken;
    };

    typedef std::list<Request> requests_t;
    requests_t m_requests;

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
        bool m_bCanInterrupt;

        HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
        ULONG STDMETHODCALLTYPE AddRef();
        ULONG STDMETHODCALLTYPE Release();

        HRESULT STDMETHODCALLTYPE GetParameters(DWORD*, DWORD*);
        HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult*);
    };

    CAsyncRead m_async_read;

    HANDLE m_hThread;
    HANDLE m_hQuit;
    HANDLE m_hAsyncRead;
    HANDLE m_hRequestSample;
    HANDLE m_hCommand;

    HRESULT InitThread();
    void FinalThread();

    static unsigned __stdcall ThreadProc(void*);
    unsigned Main();

    typedef bool (WebmMfSource::*thread_state_t)();
    thread_state_t m_thread_state;

    bool StateAsyncCancel();
    bool StateAsyncRead();
    bool StateRequestSample();
    bool StateQuit();

    thread_state_t OnAsyncRead();
    thread_state_t OnRequestSample();
    bool OnCommand();

    //For async parsing and loading of clusters.
    const mkvparser::Cluster* m_pCurr;
    const mkvparser::Cluster* m_pNext;

    //for async load:
    thread_state_t StateAsyncParseEbmlHeader();
    thread_state_t StateAsyncParseSegmentHeaders();
    thread_state_t StateAsyncLoadCluster();
    thread_state_t StateAsyncParseCluster();
    thread_state_t StateAsyncInitStreams();
    thread_state_t StateAsyncParseCues();
    long m_load_index;

    //for async locking and loading:
    thread_state_t StateAsyncLockCurr();
    thread_state_t StateAsyncGetSampleExtent();

    //for async cluster parsing of curr cluster:
    //thread_state_t StateAsyncLoadCurrInit();
    //thread_state_t StateAsyncLoadCurrFinal();
    thread_state_t StateAsyncGetCurrBlockObjectInit();
    thread_state_t StateAsyncGetCurrBlockObjectFinal();
    //thread_state_t StateAsyncGetNextBlockCurrInit();
    //thread_state_t StateAsyncGetNextBlockCurrFinal();
    thread_state_t StateAsyncGetNextBlockNextInit();
    thread_state_t StateAsyncGetNextBlockNextNotify();
    thread_state_t StateAsyncGetNextBlockNextParse();

    //for async cluster parsing of next cluster:
    //thread_state_t StateAsyncParseNextInit();
    //thread_state_t StateAsyncParseNextFinal();
    //thread_state_t StateAsyncLoadNext();
    //thread_state_t StateAsyncNotifyNext();

    //for async pre-fetching of next cluster
    //thread_state_t StateAsyncPreloadNext();
    //thread_state_t StateAsyncParseCurr();
    //thread_state_t StateAsyncParseNextInit();
    //thread_state_t StateAsyncParseNextFinal();
    //thread_state_t StateAsyncPreloadSample();
    thread_state_t StateAsyncRequestSample();

    typedef thread_state_t (WebmMfSource::*async_state_t)();
    async_state_t m_async_state;

    HRESULT ParseEbmlHeader(LONGLONG&);

    void PurgeCache();
    //thread_state_t PreloadCache(bool& bDone);
    //thread_state_t PreloadCache(const mkvparser::BlockEntry*);
    thread_state_t Parse(bool& bDone);
    thread_state_t Parse(const mkvparser::Cluster*, bool& bDone);
    //thread_state_t PreloadSample(WebmMfStream*);

    thread_state_t LoadComplete(HRESULT);

    class Command
    {
        Command& operator=(const Command&);

    public:
        enum Kind
        {
            kStart,
            kSeek,
            kRestart,  //un-pause
            kPause,
            kStop
        };

        const Kind m_kind;
        WebmMfSource* const m_pSource;

        Command(Kind, WebmMfSource*);
        Command(const Command&);

        ~Command();

        void SetDesc(IMFPresentationDescriptor*);
        void SetTime(const PROPVARIANT&);

        void OnStart();
        void OnSeek() const;
        void OnRestart() const;  //un-pause
        void OnPause() const;
        void OnStop() const;

    private:
        IMFPresentationDescriptor* m_pDesc;
        PROPVARIANT m_time;

        LONGLONG GetClusterPos(LONGLONG time_ns) const;

        void OnStartNoSeek() const;
        void OnStartInitStreams(LONGLONG time_ns, LONGLONG base_pos) const;
        void OnStartComplete() const;
        void OnSeekComplete() const;

    };

    typedef std::list<Command> commands_t;
    commands_t m_commands;

    //The map key is stream identifier of the stream descriptor,
    //which is the same as MKV track number.
    typedef std::map<ULONG, WebmMfStream*> streams_t;
    streams_t m_streams;

    LONGLONG GetCurrTime(IMFPresentationDescriptor*) const;

    HRESULT Error(const wchar_t*, HRESULT) const;

    void PurgeRequests();

    bool ThinningSupported() const;

    bool m_bLive;
    bool m_bCanSeek;
    bool HaveVideo() const;

    bool InitStreamsDone() const;

};


}  //end namespace WebmMfSourceLib
