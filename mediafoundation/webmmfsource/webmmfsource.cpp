#include "webmmfsource.hpp"
#include "webmmfstream.hpp"
#include "webmmfstreamvideo.hpp"
#include "webmmfstreamaudio.hpp"
//#include "webmtypes.hpp"
//#include "vorbistypes.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <new>
#include <cassert>
#include <comdef.h>
#include <vfwmsgs.h>  //TODO
#include <memory>
#include <malloc.h>
#include <cmath>

using std::wstring;

_COM_SMARTPTR_TYPEDEF(IMFMediaEventQueue, __uuidof(IMFMediaEventQueue));
_COM_SMARTPTR_TYPEDEF(IMFStreamDescriptor, __uuidof(IMFStreamDescriptor));
//_COM_SMARTPTR_TYPEDEF(IMFMediaType, __uuidof(IMFMediaType));


namespace WebmMfSourceLib
{

#if 0
HRESULT CreateSource(
    IClassFactory* pClassFactory,
    IUnknown* pOuter,
    const IID& iid,
    void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    *ppv = 0;

#if 0
    if ((pOuter != 0) && (iid != __uuidof(IUnknown)))
        return E_INVALIDARG;
#else
    if (pOuter)
        return CLASS_E_NOAGGREGATION;
#endif

    WebmMfSource* const p = new (std::nothrow) WebmMfSource(pClassFactory);

    if (p == 0)
        return E_OUTOFMEMORY;

    IUnknown* const pUnk = p;

    const HRESULT hr = pUnk->QueryInterface(iid, ppv);

    const ULONG cRef = pUnk->Release();
    cRef;

    return hr;
}
#else
HRESULT CreateSource(IMFByteStream* pByteStream, IMFMediaSource** ppResult)
{
    assert(pByteStream);
    assert(ppResult);

    IMFMediaSource*& pResult = *ppResult;
    pResult = 0;

    WebmMfSource* const p = new (std::nothrow) WebmMfSource(pByteStream);

    if (p == 0)
        return E_OUTOFMEMORY;

    pResult = p;

    HRESULT hr = p->Load();

    if (FAILED(hr))
    {
        pResult->Release();
        pResult = 0;
    }

    return hr;
}
#endif


WebmMfSource::WebmMfSource(IMFByteStream* pByteStream) :
    m_file(pByteStream),
    m_cRef(1),
    m_bShutdown(false),
    m_pSegment(0),
    m_pDesc(0),
    m_state(kStateStopped)
{
    //TODO: this seems odd: we lock the server when creating the handler,
    //but don't when creating an actual source object.  Do we need to
    //also lock the server when creating a source object?

    HRESULT hr = CLockable::Init();
    assert(SUCCEEDED(hr));  //TODO

    hr = MFCreateEventQueue(&m_pEvents);
    assert(SUCCEEDED(hr));
    assert(m_pEvents);
}


WebmMfSource::~WebmMfSource()
{
    assert(m_bShutdown);
    assert(m_pEvents == 0);

    if (m_pDesc)
    {
        const ULONG n = m_pDesc->Release();
        n;
        assert(n == 0);
    }
}


HRESULT WebmMfSource::QueryInterface(
    const IID& iid,
    void** ppv)
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
    else if (iid == __uuidof(IMFMediaSource))
    {
        pUnk = static_cast<IMFMediaSource*>(this);
    }
    else
    {
#if 0
        wodbgstream os;
        os << "mp3source::filter::QI: iid=" << IIDStr(iid) << std::endl;
#endif
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG WebmMfSource::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}


ULONG WebmMfSource::Release()
{
    if (LONG n = InterlockedDecrement(&m_cRef))
        return n;

    delete this;
    return 0;
}


HRESULT WebmMfSource::Load()
{
    long long result, pos;

    mkvparser::EBMLHeader h;

    result = h.Parse(&m_file, pos);

    if (result < 0)  //error
        //return static_cast<HRESULT>(result);  //TODO: verify this
        return VFW_E_INVALID_FILE_FORMAT;

    //TODO: verify this:
    assert(result == 0);  //all data available in local file

    //TODO: here and elsewhere: are there MF_E_xxx alternatives?

    if (h.m_version > 1)
        return VFW_E_INVALID_FILE_FORMAT;

    if (h.m_maxIdLength > 8)
        return VFW_E_INVALID_FILE_FORMAT;

    if (h.m_maxSizeLength > 8)
        return VFW_E_INVALID_FILE_FORMAT;

    const char* const docType = h.m_docType;

    if (_stricmp(docType, "webm") == 0)
        __noop;
    else if (_stricmp(docType, "matroska") == 0)
        __noop;
    else
        return VFW_E_INVALID_FILE_FORMAT;

    if (h.m_docTypeVersion > 2)
        return VFW_E_INVALID_FILE_FORMAT;

    if (h.m_docTypeReadVersion > 2)
        return VFW_E_INVALID_FILE_FORMAT;

    //Just the EBML header has been consumed.  pos points
    //to start of (first) segment.

    mkvparser::Segment* p;

    result = mkvparser::Segment::CreateInstance(&m_file, pos, p);

    if (result < 0)
        return static_cast<HRESULT>(result);  //TODO: verify this

    assert(result == 0);  //all data available in local file
    assert(p);

    std::auto_ptr<mkvparser::Segment> pSegment(p);

    HRESULT hr = pSegment->Load();

    if (FAILED(hr))
        return hr;

#ifdef _DEBUG
    if (const mkvparser::SegmentInfo* pInfo = pSegment->GetInfo())
    {
        wstring muxingApp, writingApp;

        if (const char* str = pInfo->GetMuxingAppAsUTF8())
            muxingApp = ConvertFromUTF8(str);

        if (const char* str = pInfo->GetWritingAppAsUTF8())
            writingApp = ConvertFromUTF8(str);

        pInfo = 0;
    }
#endif

    const mkvparser::Tracks* const pTracks = pSegment->GetTracks();

    if (pTracks == 0)
        return S_FALSE;

    assert(m_streams.empty());

    const unsigned long nTracks = pTracks->GetTracksCount();

    for (unsigned long idx = 0; idx < nTracks; ++idx)
    {
        mkvparser::Track* const pTrack = pTracks->GetTrackByIndex(idx);

        if (pTrack == 0)
            continue;

        const long long type = pTrack->GetType();

        if (type == 1)  //video
            CreateVideoStream(pTrack);

        else if (type == 2)  //audio
            CreateAudioStream(pTrack);
    }

    if (m_streams.empty())
        return VFW_E_INVALID_FILE_FORMAT;

    m_pSegment = pSegment.release();
    //TODO: m_pSeekBase = 0;
    //TODO: m_seekTime = kNoSeek;

    assert(m_pDesc == 0);

    //http://msdn.microsoft.com/en-us/library/ms698990(v=VS.85).aspx
    //MFCreateStreamDescriptor

    //http://msdn.microsoft.com/en-us/library/ms695404(VS.85).aspx
    //MFCreatePresentationDescriptor

    typedef std::vector<IMFStreamDescriptor*> dv_t;
    dv_t dv;

    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        WebmMfStream* const pStream = *i++;
        assert(pStream);

        dv.push_back(pStream->m_pDesc);
        assert(dv.back() != 0);
    }

    DWORD n = static_cast<DWORD>(dv.size());
    assert(n);

    hr = MFCreatePresentationDescriptor(n, &dv[0], &m_pDesc);
    assert(SUCCEEDED(hr));
    assert(m_pDesc);

    hr = m_pDesc->GetStreamDescriptorCount(&n);
    assert(SUCCEEDED(hr));
    assert(n == static_cast<DWORD>(dv.size()));

    for (DWORD idx = 0; idx < n; ++idx)
    {
        hr = m_pDesc->SelectStream(idx);
        assert(SUCCEEDED(hr));
    }

    const LONGLONG duration_ns = m_pSegment->GetDuration();
    assert(duration_ns >= 0);

    const UINT64 duration = duration_ns / 100;

    hr = m_pDesc->SetUINT64(MF_PD_DURATION, duration);
    assert(SUCCEEDED(hr));

    return S_OK;
}


std::wstring WebmMfSource::ConvertFromUTF8(const char* str)
{
    const int cch = MultiByteToWideChar(
                        CP_UTF8,
                        0,  //TODO: MB_ERR_INVALID_CHARS
                        str,
                        -1,  //include NUL terminator in result
                        0,
                        0);  //request length

    assert(cch > 0);

    const size_t cb = cch * sizeof(wchar_t);
    wchar_t* const wstr = (wchar_t*)_malloca(cb);

    const int cch2 = MultiByteToWideChar(
                        CP_UTF8,
                        0,  //TODO: MB_ERR_INVALID_CHARS
                        str,
                        -1,
                        wstr,
                        cch);

    cch2;
    assert(cch2 > 0);
    assert(cch2 == cch);

    return wstr;
}


HRESULT WebmMfSource::GetEvent(
    DWORD dwFlags,
    IMFMediaEvent** ppEvent)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bShutdown)
        return MF_E_SHUTDOWN;

    const IMFMediaEventQueuePtr pEvents(m_pEvents);
    assert(pEvents);

    hr = lock.Release();
    assert(SUCCEEDED(hr));

    return pEvents->GetEvent(dwFlags, ppEvent);
}


HRESULT WebmMfSource::BeginGetEvent(
    IMFAsyncCallback* pCallback,
    IUnknown* pState)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bShutdown)
        return MF_E_SHUTDOWN;

    assert(m_pEvents);

    return m_pEvents->BeginGetEvent(pCallback, pState);
}



HRESULT WebmMfSource::EndGetEvent(
    IMFAsyncResult* pResult,
    IMFMediaEvent** ppEvent)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bShutdown)
        return MF_E_SHUTDOWN;

    assert(m_pEvents);

    return m_pEvents->EndGetEvent(pResult, ppEvent);
}


HRESULT WebmMfSource::QueueEvent(
    MediaEventType t,
    REFGUID g,
    HRESULT hrStatus,
    const PROPVARIANT* pValue)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bShutdown)
        return MF_E_SHUTDOWN;

    assert(m_pEvents);

    return m_pEvents->QueueEventParamVar(t, g, hrStatus, pValue);
}


HRESULT WebmMfSource::GetCharacteristics(DWORD* pdw)
{
    if (pdw == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bShutdown)
        return MF_E_SHUTDOWN;

    DWORD& dw = *pdw;

  //TODO
//enum _MFMEDIASOURCE_CHARACTERISTICS
// { MFMEDIASOURCE_IS_LIVE = 0x1,
//   MFMEDIASOURCE_CAN_SEEK = 0x2,
//   MFMEDIASOURCE_CAN_PAUSE = 0x4,
//   MFMEDIASOURCE_HAS_SLOW_SEEK = 0x8,
//   MFMEDIASOURCE_HAS_MULTIPLE_PRESENTATIONS = 0x10,
//   MFMEDIASOURCE_CAN_SKIPFORWARD = 0x20,
//   MFMEDIASOURCE_CAN_SKIPBACKWARD = 0x40

    dw = MFMEDIASOURCE_CAN_PAUSE |
         MFMEDIASOURCE_CAN_SEEK;

    return S_OK;
}


HRESULT WebmMfSource::CreatePresentationDescriptor(
    IMFPresentationDescriptor** ppDesc)
{
    if (ppDesc == 0)
        return E_POINTER;

    IMFPresentationDescriptor*& pDesc = *ppDesc;
    pDesc = 0;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bShutdown)
        return MF_E_SHUTDOWN;

    assert(m_pDesc);
    return m_pDesc->Clone(&pDesc);
}


HRESULT WebmMfSource::Start(
    IMFPresentationDescriptor* pDesc,
    const GUID* pTimeFormat,
    const PROPVARIANT* pStartPos)
{
    if (pDesc == 0) //TODO: liberalize this case?
        return E_INVALIDARG;

    if (pStartPos == 0)
        return E_INVALIDARG;

    if ((pTimeFormat != 0) && (*pTimeFormat != GUID_NULL))
        return MF_E_UNSUPPORTED_TIME_FORMAT;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bShutdown)
        return MF_E_SHUTDOWN;

    //TODO: vet caller's presentation descriptor:
    //if invalid then return MF_E_INVALID_PRESENTATION
    pDesc;

    const PROPVARIANT& start_pos = *pStartPos;

    //This can be called in any state: yikes!

    LONGLONG time;

    switch (start_pos.vt)
    {
        case VT_I8:
            time = start_pos.hVal.QuadPart;
            //TODO: vet time value
            break;

        case VT_EMPTY:
            if (m_state == kStateStopped)
                time = 0;
            else
                time = -1;  //interpret as "curr pos"

            break;

        default:
            return MF_E_UNSUPPORTED_TIME_FORMAT;
    }

    //Writing a Custom Media Source:
    //http://msdn.microsoft.com/en-us/library/ms700134(v=VS.85).aspx
    //


    DWORD count;

    hr = pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));

    for (DWORD index = 0; index < count; ++index)
    {
        BOOL bSelected;
        IMFStreamDescriptorPtr pSD;

        hr = pDesc->GetStreamDescriptorByIndex(index, &bSelected, &pSD);
        assert(SUCCEEDED(hr));
        assert(pSD);

        if (!bSelected)
            continue;

        DWORD id;

        hr = pSD->GetStreamIdentifier(&id);
        assert(SUCCEEDED(hr));

        typedef streams_t::const_iterator iter_t;

        iter_t i = m_streams.begin();
        const iter_t j = m_streams.end();

        while (i != j)
        {
            WebmMfStream* const pStream = *i++;
            assert(pStream);

            const DWORD num = pStream->m_pTrack->GetNumber();

            if (num != id)
                continue;

            //TODO: set stream pos before queueing stream event, or after?

            hr = m_pEvents->QueueEventParamUnk(
                    MEUpdatedStream,   //TODO: handle MENewStream
                    GUID_NULL,
                    S_OK,
                    pStream);

            assert(SUCCEEDED(hr));

            //TODO: it's not clear whether streams "exist" when the
            //source object is stopped.  When we transition to stopped,
            //does that mean streams should be destroyed?

            //TODO: set pos of stream
            //Do this after queueing stream event, or before?

            break;
        }
    }

    //TODO:
    //if this is a seek
    //  queue an the MESourceSeeked event,
    //    with the pos as the propvar value.
    //else if this is not a seek,
    //  create a MESourceStarted event.
    //    if this is a restart, set the MF_EVENT_SOURCE_ACTUAL_START,
    //      with the pos as the propvar value.
    //  queue the event
    //endif

    //for each stream
    //  if this is a seek
    //     queue a stream seeked event
    //     flush any queued samples
    //  else
    //     queue a stream started event
    //     deliver any queue samples
    //  endif
    //endfor

    //if there was an error
    //  if we did NOT queue a MESourceStarted/MESourceSeeked event
    //     then it's OK to simply return an error from Start
    //  else we DID queue a Started/Seeked event
    //     then queue an MEError event

    return S_OK;
}


HRESULT WebmMfSource::Stop()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bShutdown)
        return MF_E_SHUTDOWN;

    m_state = kStateStopped;

    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        WebmMfStream* const pStream = *i++;
        assert(pStream);

        //TODO: flush enqueued samples in stream(s)

        hr = pStream->QueueEvent(MEStreamStopped, GUID_NULL, S_OK, 0);
        assert(SUCCEEDED(hr));
    }

    hr = QueueEvent(MESourceStopped, GUID_NULL, S_OK, 0);
    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfSource::Pause()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bShutdown)
        return MF_E_SHUTDOWN;

    if (m_state == kStateStopped)  //must be started
        return MF_E_INVALID_STATE_TRANSITION;

    if (m_state == kStatePaused)  //TODO: reject this case too?
        return S_FALSE;  //?

    m_state = kStatePaused;

    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        WebmMfStream* const pStream = *i++;
        assert(pStream);

        hr = pStream->QueueEvent(MEStreamPaused, GUID_NULL, S_OK, 0);
        assert(SUCCEEDED(hr));
    }

    hr = QueueEvent(MESourcePaused, GUID_NULL, S_OK, 0);
    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfSource::Shutdown()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bShutdown)
        return S_FALSE;  //TODO: return MF_E_SHUTDOWN?

    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        WebmMfStream* const pStream = *i++;
        assert(pStream);

        pStream;  //TODO: tell stream to shut down
    }

    hr = m_pEvents->Shutdown();
    assert(SUCCEEDED(hr));

    const ULONG n = m_pEvents->Release();
    n;
    assert(n == 0);

    m_pEvents = 0;

    m_bShutdown = true;
    return S_OK;
}


void WebmMfSource::CreateVideoStream(mkvparser::Track* pTrack)
{
    assert(pTrack);
    assert(pTrack->GetType() == 1);  //video

    using mkvparser::VideoTrack;
    VideoTrack* const pVT = static_cast<VideoTrack*>(pTrack);

    WebmMfStreamVideo* pStream;

    const HRESULT hr = WebmMfStreamVideo::CreateStream(this, pVT, pStream);
    assert(hr == S_OK);
    assert(pStream);

    m_streams.push_back(pStream);
}


void WebmMfSource::CreateAudioStream(mkvparser::Track* pTrack)
{
    assert(pTrack);
    assert(pTrack->GetType() == 2);  //audio

    using mkvparser::AudioTrack;
    AudioTrack* const pAT = static_cast<AudioTrack*>(pTrack);

    WebmMfStreamAudio* pStream;

    const HRESULT hr = WebmMfStreamAudio::CreateStream(this, pAT, pStream);
    assert(hr == S_OK);
    assert(pStream);

    m_streams.push_back(pStream);
}


}  //end namespace WebmMfSource
