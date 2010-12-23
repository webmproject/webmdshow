#include "webmmfsource.hpp"
#include "webmmfstream.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <cassert>
#include <comdef.h>
#include <vfwmsgs.h>
#include <process.h>
#include <windows.h>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
using std::boolalpha;
#endif

_COM_SMARTPTR_TYPEDEF(IMFMediaEventQueue, __uuidof(IMFMediaEventQueue));
_COM_SMARTPTR_TYPEDEF(IMFSample, __uuidof(IMFSample));


namespace WebmMfSourceLib
{

WebmMfStream::WebmMfStream(
    WebmMfSource* pSource,
    IMFStreamDescriptor* pDesc,
    const mkvparser::Track* pTrack) :
    m_pSource(pSource),
    m_pDesc(pDesc),
    m_pTrack(pTrack),
    m_bSelected(true),
    m_bEOS(false),
    m_hThread(0),
    m_hQuit(0),
    m_hRequestSample(0)
{
    m_pDesc->AddRef();

    const HRESULT hr = MFCreateEventQueue(&m_pEvents);
    assert(SUCCEEDED(hr));
    assert(m_pEvents);
}


WebmMfStream::~WebmMfStream()
{
    StopThread();
    PurgeTokens();
    PurgeSamples();

    if (m_pEvents)
    {
        const ULONG n = m_pEvents->Release();
        n;
        assert(n == 0);

        m_pEvents = 0;
    }

    const ULONG n = m_pDesc->Release();
    n;
}

HRESULT WebmMfStream::QueryInterface(const IID& iid, void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == __uuidof(IUnknown))
    {
        pUnk = this;  //must be nondelegating
    }
    else if (iid == __uuidof(IMFMediaEventGenerator))
    {
        pUnk = static_cast<IMFMediaEventGenerator*>(this);
    }
    else if (iid == __uuidof(IMFMediaStream))
    {
        pUnk = static_cast<IMFMediaStream*>(this);
    }
    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG WebmMfStream::AddRef()
{
    return m_pSource->AddRef();
}


ULONG WebmMfStream::Release()
{
    return m_pSource->Release();
}


HRESULT WebmMfStream::GetEvent(
    DWORD dwFlags,
    IMFMediaEvent** ppEvent)
{
    WebmMfSource::Lock lock;

    HRESULT hr = lock.Seize(m_pSource);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    const IMFMediaEventQueuePtr pEvents(m_pEvents);

    hr = lock.Release();
    assert(SUCCEEDED(hr));

    return pEvents->GetEvent(dwFlags, ppEvent);
}


HRESULT WebmMfStream::BeginGetEvent(
    IMFAsyncCallback* pCallback,
    IUnknown* pState)
{
    WebmMfSource::Lock lock;

    HRESULT hr = lock.Seize(m_pSource);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    return m_pEvents->BeginGetEvent(pCallback, pState);
}


HRESULT WebmMfStream::EndGetEvent(
    IMFAsyncResult* pResult,
    IMFMediaEvent** ppEvent)
{
    WebmMfSource::Lock lock;

    HRESULT hr = lock.Seize(m_pSource);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    return m_pEvents->EndGetEvent(pResult, ppEvent);
}


HRESULT WebmMfStream::QueueEvent(
    MediaEventType t,
    REFGUID g,
    HRESULT hrStatus,
    const PROPVARIANT* pValue)
{
    WebmMfSource::Lock lock;

    HRESULT hr = lock.Seize(m_pSource);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    return m_pEvents->QueueEventParamVar(t, g, hrStatus, pValue);
}


HRESULT WebmMfStream::GetMediaSource(IMFMediaSource** pp)
{
    WebmMfSource::Lock lock;

    HRESULT hr = lock.Seize(m_pSource);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    return m_pSource->IUnknown::QueryInterface(pp);
}


HRESULT WebmMfStream::GetStreamDescriptor(IMFStreamDescriptor** pp)
{
    if (pp == 0)
        return E_POINTER;

    WebmMfSource::Lock lock;

    HRESULT hr = lock.Seize(m_pSource);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    IMFStreamDescriptor*& p = *pp;

    p = m_pDesc;
    p->AddRef();

    return S_OK;
}


HRESULT WebmMfStream::RequestSample(IUnknown* pToken)
{
    WebmMfSource::Lock lock;

    HRESULT hr = lock.Seize(m_pSource);
    assert(SUCCEEDED(hr));  //TODO

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    if (!m_bSelected)
        return MF_E_INVALIDREQUEST;

    //This return value is incorrrect:
    //if (m_pSource->m_state == WebmMfSource::kStateStopped)
    //    return MF_E_INVALIDREQUEST;
    //See the description of IMFMediaStream::RequestSample here:
    //  http://msdn.microsoft.com/en-us/library/ms696240%28v=VS.85%29.aspx
    //The text says:
    //  "Because the Media Foundation pipeline is multi-threaded, the
    //source's RequestSample method might get called after the source has
    //stopped. If the media source is stopped, the method should return
    //MF_E_MEDIA_SOURCE_WRONGSTATE. The pipeline does not treat this return
    //code as an error condition. If the source returns any other error code,
    //the pipeline treats it as fatal error and halts the session."
    //
    //This is the correct behavior:

    if (m_pSource->m_state == WebmMfSource::kStateStopped)
        return MF_E_MEDIA_SOURCE_WRONGSTATE;

    hr = StartThread();

    if (FAILED(hr))
        return hr;

    if (pToken)
        pToken->AddRef();

    m_tokens.push_back(pToken);

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return S_OK;
}


void WebmMfStream::OnRequestSample()
{
    WebmMfSource::Lock lock;

    HRESULT hr = lock.Seize(m_pSource);

    if (FAILED(hr))
        return;

    if (m_pEvents == 0)  //TODO
        return;

    if (!m_bSelected)  //TODO
        return;

    const WebmMfSource::State state = m_pSource->m_state;

    if (state == WebmMfSource::kStateStopped)
        return;  //TODO: anything else to do here?

    if (state == WebmMfSource::kStateStarted)
        DeliverSamples();

    if (m_tokens.empty())
        return;  //TODO: test for EOS here?

    IMFSamplePtr pSample;

    hr = MFCreateSample(&pSample);
    assert(SUCCEEDED(hr));
    assert(pSample);

    for (;;)
    {
        //TODO: this is too liberal.  If we race ahead of the current pos
        //in the byte stream, then this purges the network cache.  We
        //only want to load a new cluster when PopulateSample tells us
        //that there's an underflow.
        const long status = m_pTrack->m_pSegment->LoadCluster();

        if (status < 0)
        {
            hr = E_FAIL;  //TODO: ask MS how to handle network underflow
            break;
        }

        hr = PopulateSample(pSample);

        if (SUCCEEDED(hr))
            break;

        assert(hr == VFW_E_BUFFER_UNDERFLOW);
    }

    if (hr == S_OK)  //have a sample
    {
        IUnknown* pToken = m_tokens.front();
        m_tokens.pop_front();

        if (pToken)
        {
            hr = pSample->SetUnknown(MFSampleExtension_Token, pToken);
            assert(SUCCEEDED(hr));

            pToken->Release();
            pToken = 0;
        }

        if (state == WebmMfSource::kStatePaused)  //TODO: verify this
        {
            m_samples.push_back(pSample.Detach());
            return;
        }

        hr = m_pEvents->QueueEventParamUnk(
                MEMediaSample,
                GUID_NULL,
                S_OK,
                pSample);

        assert(SUCCEEDED(hr));
        return;
    }

    //TODO: we still have pending tokens.  What should we do with them?
    //For now:
    PurgeTokens();

#ifdef _DEBUG
    odbgstream os;

    os << "WebmMfStream::RequestSample: EOS; track="
       << m_pTrack->GetNumber()
       << " type="
       << m_pTrack->GetType()
       << " bEOS="
       << boolalpha << m_bEOS
       << " hr.EOS=" << boolalpha << bool(hr == S_FALSE)
       << endl;
#endif

    //TODO: move this into RequestSample?
    if (m_bEOS)  //sent event already
        return; // MF_E_END_OF_STREAM;

    hr = m_pEvents->QueueEventParamVar(MEEndOfStream, GUID_NULL, S_OK, 0);
    assert(SUCCEEDED(hr));

    m_bEOS = true;
    m_pSource->NotifyEOS();
}


void WebmMfStream::PurgeSamples()
{
    while (!m_samples.empty())
    {
        IMFSample* const pSample = m_samples.front();
        assert(pSample);

        m_samples.pop_front();

        pSample->Release();
    }
}


void WebmMfStream::PurgeTokens()
{
    while (!m_tokens.empty())
    {
        IUnknown* const pToken = m_tokens.front();
        m_tokens.pop_front();

        if (pToken)
            pToken->Release();
    }
}


void WebmMfStream::DeliverSamples()
{
    while (!m_samples.empty())
    {
        IMFSample* const pSample = m_samples.front();
        assert(pSample);

        m_samples.pop_front();

        const HRESULT hr = m_pEvents->QueueEventParamUnk(
                            MEMediaSample,
                            GUID_NULL,
                            S_OK,
                            pSample);

        assert(SUCCEEDED(hr));

        pSample->Release();
    }
}


HRESULT WebmMfStream::Stop()
{
    if (!m_bSelected)
        return S_FALSE;

    PurgeSamples();

    const HRESULT hr = QueueEvent(MEStreamStopped, GUID_NULL, S_OK, 0);
    assert(SUCCEEDED(hr));

    OnStop();
    return S_OK;
}


HRESULT WebmMfStream::Pause()
{
    if (!m_bSelected)
        return S_FALSE;

    const HRESULT hr = QueueEvent(MEStreamPaused, GUID_NULL, S_OK, 0);
    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfStream::Shutdown()
{
    if (m_pEvents == 0)
        return S_FALSE;

    PurgeSamples();

    const HRESULT hr = m_pEvents->Shutdown();
    assert(SUCCEEDED(hr));

    const ULONG n = m_pEvents->Release();
    n;
    assert(n == 0);

    m_pEvents = 0;

    return S_OK;
}


HRESULT WebmMfStream::Select( /* LONGLONG time */ )
{
    assert(m_samples.empty());

    m_bSelected = true;
    m_bEOS = false;

    //SetCurrPos(time);

    return S_OK;
}


HRESULT WebmMfStream::Deselect()
{
    PurgeSamples();

    m_bSelected = false;

    return S_OK;
}


bool WebmMfStream::IsSelected() const
{
    return m_bSelected;
}


HRESULT WebmMfStream::OnStart(const PROPVARIANT& var)
{
    //assert(pCurr);
    assert(m_bSelected);
    assert(m_samples.empty());
    assert(m_pEvents);

    //m_pCurr = pCurr;
    //m_bDiscontinuity = true;

    const HRESULT hr = m_pEvents->QueueEventParamVar(
                        MEStreamStarted,
                        GUID_NULL,
                        S_OK,
                        &var);

    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfStream::OnSeek(const PROPVARIANT& var)
{
    //assert(pCurr);
    assert(m_bSelected);
    assert(m_pEvents);

    PurgeSamples();

    //m_pCurr = pCurr;
    //m_bDiscontinuity = true;

    const HRESULT hr = m_pEvents->QueueEventParamVar(
                        MEStreamSeeked,
                        GUID_NULL,
                        S_OK,
                        &var);

    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfStream::Restart()
{
    if (!m_bSelected)
        return S_FALSE;

#ifdef _DEBUG
    {
        wodbgstream os;
        os << L"WebmMfStream::Restart" << endl;
    }
#endif

    PROPVARIANT var;
    PropVariantInit(&var);

    var.vt = VT_EMPTY;  //restarts always report VT_EMPTY

    assert(m_pEvents);

    HRESULT hr = m_pEvents->QueueEventParamVar(
                    MEStreamStarted,
                    GUID_NULL,
                    S_OK,
                    &var);

    assert(SUCCEEDED(hr));

    //DeliverSamples();

    hr = StartThread();

    if (FAILED(hr))
        return hr;

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return S_OK;
}


HRESULT WebmMfStream::GetCurrMediaTime(LONGLONG& reftime) const
{
    //source object already locked by caller

    const mkvparser::BlockEntry* const pCurr = GetCurrBlock();

    if (pCurr == 0)  //?
    {
        reftime = 0;  //TODO: try to load the first cluster
        return S_OK;
    }

    const mkvparser::Cluster* const pCurrCluster = pCurr->GetCluster();
    assert(pCurrCluster);

    const mkvparser::Block* const pCurrBlock = pCurr->GetBlock();
    assert(pCurrBlock);

    const LONGLONG curr_ns = pCurrBlock->GetTime(pCurrCluster);
    assert(curr_ns >= 0);

    reftime = curr_ns / 100;
    return S_OK;
}


HRESULT WebmMfStream::StartThread()
{
    if (m_hThread)  //weird
        return S_FALSE;

    if (m_hQuit == 0)
    {
        m_hQuit = CreateEvent(0, 0, 0, 0);

        if (m_hQuit == 0)
        {
            const DWORD e = GetLastError();
            return HRESULT_FROM_WIN32(e);
        }
    }

    if (m_hRequestSample == 0)
    {
        m_hRequestSample = CreateEvent(0, 0, 0, 0);

        if (m_hRequestSample == 0)
        {
            const DWORD e = GetLastError();
            return HRESULT_FROM_WIN32(e);
        }
    }

    const uintptr_t h = _beginthreadex(
                            0,  //security
                            0,  //stack size
                            &WebmMfStream::ThreadProc,
                            this,
                            0,   //run immediately
                            0);  //thread id

    m_hThread = reinterpret_cast<HANDLE>(h);

    if (m_hThread == 0)  //error
        return E_FAIL;   //TODO: convert errno to HRESULT (how?)

    return S_OK;
}


void WebmMfStream::StopThread()
{
    if (m_hThread)
    {
        BOOL b = SetEvent(m_hQuit);
        assert(b);

        const DWORD dw = WaitForSingleObject(m_hThread, 5000);
        assert(dw == WAIT_OBJECT_0);

        b = CloseHandle(m_hThread);
        assert(b);

        m_hThread = 0;
    }

    if (m_hRequestSample)
    {
        const BOOL b = CloseHandle(m_hRequestSample);
        assert(b);

        m_hRequestSample = 0;
    }

    if (m_hQuit == 0)
    {
        const BOOL b = CloseHandle(m_hQuit);
        assert(b);

        m_hQuit = 0;
    }
}


unsigned WebmMfStream::ThreadProc(void* pv)
{
    WebmMfStream* const pStream = static_cast<WebmMfStream*>(pv);
    assert(pStream);

    return pStream->Main();
}


unsigned WebmMfStream::Main()
{
    enum { nh = 2 };
    const HANDLE ah[nh] = { m_hQuit, m_hRequestSample };

    for (;;)
    {
        const DWORD dw = WaitForMultipleObjects(nh, ah, FALSE, INFINITE);

        if (dw == WAIT_FAILED)
            return 1;  //error

        assert(dw >= WAIT_OBJECT_0);
        assert(dw < (WAIT_OBJECT_0 + nh));

        if (dw == WAIT_OBJECT_0) //hQuit
            return 0;

        assert(dw == (WAIT_OBJECT_0 + 1));  //hRequestSample

        OnRequestSample();
    }
}


}  //end namespace WebmMfSource
