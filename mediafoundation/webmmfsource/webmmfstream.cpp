#include "webmmfsource.hpp"
#include "webmmfstream.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <cassert>
#include <comdef.h>
#include <vfwmsgs.h>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif

_COM_SMARTPTR_TYPEDEF(IMFMediaEventQueue, __uuidof(IMFMediaEventQueue));
_COM_SMARTPTR_TYPEDEF(IMFSample, __uuidof(IMFSample));


namespace WebmMfSourceLib
{

WebmMfStream::WebmMfStream(
    WebmMfSource* pSource,
    IMFStreamDescriptor* pDesc,
    mkvparser::Track* pTrack) :
    m_pSource(pSource),
    m_pDesc(pDesc),
    m_pTrack(pTrack),
    //m_pBaseCluster(0),
    m_pCurr(0),
    m_bDiscontinuity(true),
    m_bSelected(true),
    m_bEOS(false)
{
    m_pDesc->AddRef();

    const HRESULT hr = MFCreateEventQueue(&m_pEvents);
    assert(SUCCEEDED(hr));
    assert(m_pEvents);
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

    if (m_pSource->m_state == WebmMfSource::kStateStopped)
        return MF_E_INVALIDREQUEST;

    IMFSamplePtr pSample;

    hr = MFCreateSample(&pSample);
    assert(SUCCEEDED(hr));
    assert(pSample);

    for (;;)
    {
        hr = PopulateSample(pSample);

        if (hr != VFW_E_BUFFER_UNDERFLOW)
            break;

        hr = Preload();  //TODO: file-based read assumed
        assert(SUCCEEDED(hr));
    }

    if (hr == S_OK)  //have a sample
    {
        //WavSource sample says:
        // NOTE: If we processed sample requests asynchronously, we would
        // need to call AddRef on the token and put the token onto a FIFO
        // queue. See documenation for IMFMediaStream::RequestSample.

        if (pToken)
        {
            hr = pSample->SetUnknown(MFSampleExtension_Token, pToken);
            assert(SUCCEEDED(hr));
        }

        if (m_pSource->m_state == WebmMfSource::kStatePaused)
        {
            m_samples.push_back(pSample.Detach());
            return S_OK;
        }

        hr = m_pEvents->QueueEventParamUnk(
                MEMediaSample,
                GUID_NULL,
                S_OK,
                pSample);

        assert(SUCCEEDED(hr));
        return S_OK;
    }

    assert(hr == S_FALSE);  //EOS

    if (m_bEOS)  //sent event already
        return MF_E_END_OF_STREAM;

    hr = m_pEvents->QueueEventParamVar(MEEndOfStream, GUID_NULL, S_OK, 0);
    assert(SUCCEEDED(hr));

    m_bEOS = true;
    m_pSource->NotifyEOS();

    return S_OK;  //TODO: MF_E_END_OF_STREAM instead?
}


HRESULT WebmMfStream::Preload()
{
    mkvparser::Segment* const pSegment = m_pTrack->m_pSegment;

    mkvparser::Cluster* pCluster;
    LONGLONG pos;

    //TODO: file-based load is assumed here
    //We need to determine how MF handles network streams.

    //TODO: is it possible to be smarter here, to keep
    //parsing until we have a cluster containing a block from
    //this track?

    const long result = pSegment->ParseCluster(pCluster, pos);
    result;
    assert(result >= 0);

    const bool bDone = pSegment->AddCluster(pCluster, pos);

    return bDone ? S_FALSE : S_OK;
}


HRESULT WebmMfStream::PopulateSample(IMFSample* pSample)
{
    if (m_pCurr == 0)
    {
        const long result = m_pTrack->GetFirst(m_pCurr);

        if (result == mkvparser::E_BUFFER_NOT_FULL)
            return VFW_E_BUFFER_UNDERFLOW;

        assert(result >= 0);
        assert(m_pCurr);

        //TODO:
        //This doesn't seem correct: the base cluster must be the
        //same for all streams (since that is how time is calculated),
        //but we can't really guarantee that here (unless we also
        //have a guarantee that all streams are initialized to
        //pCurr=NULL, in which case all streams would be all have
        //the same base).
        //
        //The other problem is that there's no guarantee that
        //m_pCurr is on the first cluster in the segment.  Technically
        //it doesn't matter, but if pCurr is far away then there will
        //be a large gap in time before this frame renders.
        //
        //I'd feel better if the initialization step were explicit,
        //and for all streams simultaneously; say, during the
        //transition to paused/running.  This would have the effect
        //of setting pCurr for all streams to some non-NULL value,
        //for which we could then test.
        //END TODO.

        //TODO: determine this value during Start
        //m_pBaseCluster = m_pTrack->m_pSegment->GetFirst();
        //assert(m_pBaseCluster);
    }

    if (m_pCurr->EOS())
        return S_FALSE;  //no more samples: send EOS downstream

    const mkvparser::BlockEntry* pNextBlock;

    const long result = m_pTrack->GetNext(m_pCurr, pNextBlock);

    if (result == mkvparser::E_BUFFER_NOT_FULL)
        return VFW_E_BUFFER_UNDERFLOW;

    assert(result >= 0);
    assert(pNextBlock);

    HRESULT hr = OnPopulateSample(pNextBlock, pSample);
    assert(SUCCEEDED(hr));  //TODO
    assert(hr == S_OK);     //TODO: for now, assume we never throw away

    m_pCurr = pNextBlock;

#if 0  //TODO: resolve this
    if (hr != S_OK)
        return 2;  //throw away this sample
#endif

    if (m_bDiscontinuity)
    {
        hr = pSample->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
        assert(SUCCEEDED(hr));

        m_bDiscontinuity = false;  //TODO: must set back to true during a seek
    }

    return S_OK;  //TODO
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


HRESULT WebmMfStream::Start(
    const PROPVARIANT& var,
    mkvparser::Cluster* pBaseCluster,
    const mkvparser::BlockEntry* pCurr)
{
    if (!m_bSelected)
        return S_FALSE;

    assert(pBaseCluster);
    assert(m_samples.empty());
    assert(m_pEvents);

    if (pCurr)
        m_pCurr = pCurr;
    else
        m_pCurr = pBaseCluster->GetEntry(m_pTrack);

#ifdef _DEBUG
    {
        const LONGLONG ns = pBaseCluster->GetTime();

        const mkvparser::Block* const pBlock = m_pCurr->GetBlock();
        const LONGLONG ns2 = pBlock->GetTime(pBaseCluster);

        wodbgstream os;
        os << L"WebmMfStream::Start: cluster.ns=" << ns
           << " cluster.secs=" << (double(ns) / 1000000000)
           << " block.ns=" << ns2
           << " block.secs=" << (double(ns2) / 1000000000)
           << endl;
    }
#endif

    m_bDiscontinuity = true;

    const HRESULT hr = m_pEvents->QueueEventParamVar(
                        MEStreamStarted,
                        GUID_NULL,
                        S_OK,
                        &var);

    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfStream::Seek(
    const PROPVARIANT& var,
    mkvparser::Cluster* pBaseCluster,
    const mkvparser::BlockEntry* pCurr)
{
    if (!m_bSelected)
        return S_FALSE;

    PurgeSamples();

    if (pCurr)
        m_pCurr = pCurr;
    else
        m_pCurr = pBaseCluster->GetEntry(m_pTrack);

#ifdef _DEBUG
    {
        const LONGLONG ns = pBaseCluster->GetTime();

        const mkvparser::Block* const pBlock = m_pCurr->GetBlock();
        const LONGLONG ns2 = pBlock->GetTime(pBaseCluster);

        wodbgstream os;
        os << L"WebmMfStream::Seek: cluster.ns=" << ns
           << " cluster.secs=" << (double(ns) / 1000000000)
           << " block.ns=" << ns2
           << " block.secs=" << (double(ns2) / 1000000000)
           << endl;
    }
#endif

    m_bDiscontinuity = true;

    assert(m_pEvents);

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

    const HRESULT hr = m_pEvents->QueueEventParamVar(
                        MEStreamStarted,
                        GUID_NULL,
                        S_OK,
                        &var);

    assert(SUCCEEDED(hr));

    DeliverSamples();

    return S_OK;
}


HRESULT WebmMfStream::GetCurrMediaTime(LONGLONG& reftime) const
{
    //source object already locked by caller

    if (m_pCurr == 0)
    {
        reftime = 0;  //TODO: try to load the first cluster
        return S_OK;
    }

    mkvparser::Cluster* const pCurrCluster = m_pCurr->GetCluster();
    assert(pCurrCluster);

    const mkvparser::Block* const pCurrBlock = m_pCurr->GetBlock();
    assert(pCurrBlock);

    const LONGLONG curr_ns = pCurrBlock->GetTime(pCurrCluster);
    assert(curr_ns >= 0);

    reftime = curr_ns / 100;
    return S_OK;
}


}  //end namespace WebmMfSource
