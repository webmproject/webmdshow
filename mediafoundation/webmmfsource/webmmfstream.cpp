#include "webmmfsource.hpp"
#include "webmmfstream.hpp"
//#include "mkvparser.hpp"
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


namespace WebmMfSourceLib
{

WebmMfStream::WebmMfStream(
    WebmMfSource* pSource,
    IMFStreamDescriptor* pDesc,
    const mkvparser::Track* pTrack) :
    m_cRef(0),  //yes
    m_pSource(pSource),
    m_pDesc(pDesc),
    m_pTrack(pTrack),
    m_bSelected(-1),
    m_bEOS(true),
    m_pFirstBlock(0),
    m_pNextBlock(0),
    m_pLocked(0),
    m_time_ns(-1),
    m_cluster_pos(-1),
    m_rate(1),
    m_thin_ns(-3)  //means "not thinning"
{
    m_pDesc->AddRef();

    const HRESULT hr = MFCreateEventQueue(&m_pEvents);
    assert(SUCCEEDED(hr));
    assert(m_pEvents);

    m_curr.Init();
}


WebmMfStream::~WebmMfStream()
{
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
    const ULONG cSource = m_pSource->AddRef();
    cSource;

    const ULONG cStream = InterlockedIncrement(&m_cRef);

    return cStream;
}


ULONG WebmMfStream::Release()
{
    assert(m_cRef > 0);
    const ULONG cStream = InterlockedDecrement(&m_cRef);

    //WebmMfSource* const pSource = m_pSource;  //cache

    //if (cStream == 0)  //client is done with stream
    //    pSource->OnDestroy(this);

    const ULONG cSource = m_pSource->Release();
    cSource;

    return cStream;
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

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

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

    if (m_pSource->IsStopped())
        return MF_E_MEDIA_SOURCE_WRONGSTATE;

    if (m_bSelected <= 0)  //not selected
        return MF_E_INVALIDREQUEST;

    return m_pSource->RequestSample(this, pToken);
}


HRESULT WebmMfStream::ProcessSample(IMFSample* pSample)
{
    if (m_pEvents == 0)  //shutdown has been requested
        return S_FALSE;

    if (m_pSource->IsStopped())
        return S_FALSE;  //throw this sample away

    if (m_pSource->IsPaused())
    {
        pSample->AddRef();
        m_samples.push_back(pSample);

        return S_OK;
    }

    assert(m_samples.empty());

    const HRESULT hr = m_pEvents->QueueEventParamUnk(
                        MEMediaSample,
                        GUID_NULL,
                        S_OK,
                        pSample);

    assert(SUCCEEDED(hr));  //TODO

    return S_OK;
}


#if 0
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
#endif


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
    if (m_bSelected <= 0)
        return S_FALSE;

    m_bSelected = 0;
    m_bEOS = true;
    //m_thin_ns = -3;  //means "not thinning"
    //m_rate = 1;

    PurgeSamples();

    const HRESULT hr = QueueEvent(MEStreamStopped, GUID_NULL, S_OK, 0);
    assert(SUCCEEDED(hr));

    m_pSource->m_file.UnlockPage(m_pLocked);
    m_pLocked = 0;

    m_curr.Init();

    return S_OK;
}


HRESULT WebmMfStream::Pause()
{
    if (m_bSelected <= 0)
        return S_FALSE;

    const HRESULT hr = QueueEvent(MEStreamPaused, GUID_NULL, S_OK, 0);
    assert(SUCCEEDED(hr));

    return S_OK;
}


#if 0
//This is the wrong event queue: it must
//be sent on the source's queue, not the stream's.
HRESULT WebmMfStream::Update()
{
    m_bSelected = true;
    m_bEOS = false;

    HRESULT hr = m_pEvents->QueueEventParamUnk(
                    MEUpdatedStream,
                    GUID_NULL,
                    S_OK,
                    this);

    assert(SUCCEEDED(hr));

    return S_OK;
}
#endif


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


MediaEventType WebmMfStream::Select()
{
    const int b = m_bSelected;

    m_bSelected = 1;

    return (b < 0) ? MENewStream : MEUpdatedStream;
}


HRESULT WebmMfStream::Deselect()
{
    //This is like a stop, except that we don't
    //send any notifications to the pipeline.

    if (m_bSelected <= 0)
        return S_FALSE;

    PurgeSamples();

    m_bSelected = 0;
    m_bEOS = true;
    //m_thin_ns = -3;  //means "not thinning"
    //m_rate = 1;

    m_pSource->m_file.UnlockPage(m_pLocked);
    m_pLocked = 0;

    m_curr.Init();
    m_pNextBlock = 0;

    return S_OK;
}


bool WebmMfStream::IsSelected() const
{
    return (m_bSelected > 0);
}


HRESULT WebmMfStream::OnStart(const PROPVARIANT& var)
{
    //assert(pCurr);
    assert(m_bSelected > 0);
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

    return hr;
}


HRESULT WebmMfStream::Seek(const PROPVARIANT& var)
{
    if (m_bSelected <= 0)
        return S_FALSE;

    //assert(m_pLocked == 0);
    //m_bDiscontinuity = true;
    ////TODO: m_curr = curr;
    //assert(m_curr.pBE);
    //m_pNextBlock = 0;

    return OnSeek(var);
}


HRESULT WebmMfStream::OnSeek(const PROPVARIANT& var)
{
    //assert(pCurr);
    assert(m_bSelected > 0);
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
    if (m_bSelected <= 0)
        return S_FALSE;

#if 0 //def _DEBUG
    wodbgstream os;
    os << L"WebmMfStream::Restart; track.type="
       << m_pTrack->GetType()
       << endl;
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

    DeliverSamples();

    return S_OK;
}


bool WebmMfStream::GetEOS() const
{
    return m_bEOS;
}


bool WebmMfStream::IsCurrBlockEOS() const
{
    const mkvparser::BlockEntry* const pCurr = m_curr.pBE;
    return ((pCurr != 0) && pCurr->EOS());
}


HRESULT WebmMfStream::SetFirstBlock(const mkvparser::Cluster* pCluster)
{
    if (m_pFirstBlock)
        return S_FALSE;  //already have a value, so nothing else to do

    assert(pCluster);

    if (pCluster->EOS())
    {
        m_pFirstBlock = m_pTrack->GetEOS();
        return S_OK;
    }

    const mkvparser::BlockEntry* const pBlock = pCluster->GetEntry(m_pTrack);

    if (pBlock == 0)     //weird: no entries on this cluster
        return E_FAIL;   //so try the next cluster

    if (pBlock->EOS())   //no (acceptable) entry for this track
        return E_FAIL;   //so try the next cluster

    m_pFirstBlock = pBlock;  //have an acceptable entry
    return S_OK;             //first block for this stream has been found
}


const mkvparser::BlockEntry* WebmMfStream::GetFirstBlock() const
{
    return m_pFirstBlock;  //should be initialized to EOS
}


void WebmMfStream::SetCurrBlock(const mkvparser::BlockEntry* pBE)
{
    m_pSource->m_file.UnlockPage(m_pLocked);
    m_pLocked = 0;

    m_curr.Init(pBE);
    m_pNextBlock = 0;

    m_bDiscontinuity = true;
    m_bEOS = false;

    m_time_ns = -1;
    m_cluster_pos = -1;

    if (m_thin_ns >= 0)
        m_thin_ns = -1;  //TODO: for now, send new notification
}


void WebmMfStream::SetCurrBlockInit(
    LONGLONG time_ns,
    LONGLONG cluster_pos)
{
    assert(time_ns >= 0);
    assert(cluster_pos >= 0);

    m_pSource->m_file.UnlockPage(m_pLocked);
    m_pLocked = 0;

    m_curr.Init();
    m_pNextBlock = 0;

    m_bDiscontinuity = true;
    m_bEOS = false;

    m_time_ns = time_ns;
    m_cluster_pos = cluster_pos;

    if (m_thin_ns >= 0)
        m_thin_ns = -1;  //TODO: for now, send new notification
}


#if 0
void WebmMfStream::SetCurrBlockCompletion(const mkvparser::Cluster* pCluster)
{
    assert(m_curr.pBE == 0);
    assert(m_time_ns >= 0);

    if ((pCluster == 0) ||
        pCluster->EOS() ||
        (pCluster->GetEntryCount() <= 0))
    {
        m_curr.Init(m_pTrack->GetEOS());
        return;
    }

    m_curr.Init(pCluster->GetEntry(m_pTrack, m_time_ns));
    assert(m_curr.pBE);

    m_cluster_pos = -1;
    m_time_ns = -1;
}
#endif


const mkvparser::BlockEntry* WebmMfStream::GetCurrBlock() const
{
    return m_curr.pBE;
}


HRESULT WebmMfStream::GetCurrMediaTime(LONGLONG& reftime) const
{
    //source object already locked by caller

    const mkvparser::BlockEntry* const pCurr = GetCurrBlock();

    if (pCurr == 0)  //waiting to load curr block, following a start request
    {
        if (m_time_ns < 0)
            return E_FAIL;

        reftime = m_time_ns / 100;
        return S_OK;
    }

    if (pCurr->EOS())  //already reached end of stream
    {
        const LONGLONG time_ns = m_pSource->m_pSegment->GetDuration();

        if (time_ns < 0)  //no duration
            return E_FAIL;

        reftime = time_ns / 100;
        return S_OK;
    }

    const mkvparser::Cluster* const pCurrCluster = pCurr->GetCluster();
    assert(pCurrCluster);
    assert(!pCurrCluster->EOS());

    const mkvparser::Block* const pCurrBlock = pCurr->GetBlock();
    assert(pCurrBlock);

    const LONGLONG curr_ns = pCurrBlock->GetTime(pCurrCluster);
    assert(curr_ns >= 0);

    reftime = curr_ns / 100;
    return S_OK;
}


HRESULT WebmMfStream::SetEOS()
{
    if (m_bEOS)
        return S_FALSE;

    m_bEOS = true;

    const HRESULT hr = m_pEvents->QueueEventParamVar(
                        MEEndOfStream,
                        GUID_NULL,
                        S_OK,
                        0);

    assert(SUCCEEDED(hr));

    return S_OK;
}


#if 0
HRESULT WebmMfStream::NotifyCurrCluster(const mkvparser::Cluster* pCluster)
{
    assert(pCluster);
    assert(!pCluster->EOS());

    const LONGLONG tn = m_pTrack->GetNumber();

    const mkvparser::BlockEntry* pCurr = pCluster->GetFirst();

    while (pCurr)
    {
        const mkvparser::Block* const pBlock = pCurr->GetBlock();
        assert(pBlock);

        if (pBlock->GetTrackNumber() == tn)
        {
            SetCurrBlock(pCurr);
            return S_OK;
        }

        pCurr = pCluster->GetNext(pCurr);
    }

    return VFW_E_BUFFER_UNDERFLOW;
}
#endif


HRESULT WebmMfStream::GetNextBlock()
{
    const mkvparser::BlockEntry* const pCurr = m_curr.pBE;
    assert(pCurr);
    assert(!pCurr->EOS());

    const LONGLONG tn = m_pTrack->GetNumber();

    const mkvparser::Cluster* const pCluster = pCurr->GetCluster();
    assert(pCluster);
    assert(!pCluster->EOS());

    m_pNextBlock = pCluster->GetNext(pCurr);

    while (m_pNextBlock)
    {
        const mkvparser::Block* const pBlock = m_pNextBlock->GetBlock();
        assert(pBlock);

        if (pBlock->GetTrackNumber() == tn)
            return S_OK;  //success

        m_pNextBlock = pCluster->GetNext(m_pNextBlock);
    }

    return VFW_E_BUFFER_UNDERFLOW;  //did not find next entry for this track
}


HRESULT WebmMfStream::NotifyNextCluster(const mkvparser::Cluster* pNextCluster)
{
    if ((pNextCluster == 0) || pNextCluster->EOS())
    {
        m_pNextBlock = m_pTrack->GetEOS();
        return S_OK;
    }

    const LONGLONG tn = m_pTrack->GetNumber();

    m_pNextBlock = pNextCluster->GetFirst();

    while (m_pNextBlock)
    {
        const mkvparser::Block* const pBlock = m_pNextBlock->GetBlock();
        assert(pBlock);

        if (pBlock->GetTrackNumber() == tn)
            return S_OK;  //success

        m_pNextBlock = pNextCluster->GetNext(m_pNextBlock);
    }

    return VFW_E_BUFFER_UNDERFLOW;
}


bool WebmMfStream::IsCurrBlockLocked() const
{
    return (m_pLocked != 0);
}


int WebmMfStream::LockCurrBlock()
{
    const mkvparser::BlockEntry* const pCurr = m_curr.pBE;
    assert(pCurr);
    assert(!pCurr->EOS());
    assert(m_pLocked == 0);

    const int status = m_pSource->m_file.LockPage(pCurr);
    assert(status == 0);

    if (status)  //should never happen
        return status;

    m_pLocked = pCurr;
    return 0;  //succeeded
}


void WebmMfStream::SeekInfo::Init(const mkvparser::BlockEntry* pBE_)
{
    pBE = pBE_;
    pCP = 0;
    pTP = 0;
}


bool WebmMfStream::IsCurrBlockLoaded(LONGLONG& pos) const
{
    if (m_curr.pBE)
        return true;

    pos = m_cluster_pos;
    assert(pos >= 0);

    return false;
}


void WebmMfStream::SetRate(BOOL bThin, float rate)
{
    m_rate = rate;
    assert(m_rate >= 0);  //TODO: implement reverse playback

    //m_thin_ns <= -3
    //  in not thinning mode
    //
    //m_thin_ns == -2
    //  we were in thinning mode
    //  not thinning already requested, but
    //  MEStreamThinMode has not been sent yet
    //
    //m_thin_ns == -1
    //  we were in not thinning mode
    //  thinning mode requested, but event
    //  hasn't been sent yet
    //
    //m_thin_ns >= 0
    //  in thinning mode
    //  thinning mode had been requested, and event
    //  has been sent

    if (bThin)
    {
        if (m_thin_ns <= -3)  //not thinning
        {
            m_thin_ns = -1;   //send notice of transition
            return;
        }

        if (m_thin_ns == -2)  //not thinning already requested
        {
            m_thin_ns = -1;   //go ahead and send notice of transition
            return;
        }

        //no change req'd here
    }
    else if (m_thin_ns <= -3)  //already not thinning
        return;

    else if (m_thin_ns == -2)  //not thinning already requested
        return;

    else if (m_thin_ns == -1)  //thinning requested
        m_thin_ns = -2;        //not thinning requested

    else
        m_thin_ns = -2;  //not thinning requested
}


}  //end namespace WebmMfSource
