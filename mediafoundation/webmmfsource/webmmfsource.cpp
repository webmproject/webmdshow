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
#include <utility>  //std::make_pair
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif

using std::wstring;

_COM_SMARTPTR_TYPEDEF(IMFMediaEventQueue, __uuidof(IMFMediaEventQueue));
_COM_SMARTPTR_TYPEDEF(IMFStreamDescriptor, __uuidof(IMFStreamDescriptor));
_COM_SMARTPTR_TYPEDEF(IMFMediaEvent, __uuidof(IMFMediaEvent));


namespace WebmMfSourceLib
{

HRESULT CreateSource(
    IClassFactory* pClassFactory,
    IMFByteStream* pByteStream,
    IMFMediaSource** ppResult)
{
    assert(pClassFactory);
    assert(pByteStream);
    assert(ppResult);

    IMFMediaSource*& pResult = *ppResult;
    pResult = 0;

    WebmMfSource* const p =
        new (std::nothrow) WebmMfSource(pClassFactory, pByteStream);

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


WebmMfSource::WebmMfSource(
    IClassFactory* pClassFactory,
    IMFByteStream* pByteStream) :
    m_pClassFactory(pClassFactory),
    m_cRef(1),
    m_file(pByteStream),
    m_pSegment(0),
    m_state(kStateStopped),
    m_cEOS(0)
{
    HRESULT hr = m_pClassFactory->LockServer(TRUE);
    assert(SUCCEEDED(hr));

    hr = CLockable::Init();
    assert(SUCCEEDED(hr));  //TODO

    hr = MFCreateEventQueue(&m_pEvents);
    assert(SUCCEEDED(hr));
    assert(m_pEvents);

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource: ctor" << endl;
#endif
}


WebmMfSource::~WebmMfSource()
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource: dtor" << endl;
#endif

    if (m_pEvents)
    {
        const ULONG n = m_pEvents->Release();
        n;
        assert(n == 0);
    }

    while (!m_stream_descriptors.empty())
    {
        IMFStreamDescriptor* const pSD = m_stream_descriptors.back();
        assert(pSD);

        m_stream_descriptors.pop_back();

        const ULONG n = pSD->Release();
        n;
    }

    typedef streams_t::iterator iter_t;

    iter_t iter = m_streams.begin();
    const iter_t iter_end = m_streams.end();

    while (iter != iter_end)
    {
        WebmMfStream* const pStream = iter->second;
        assert(pStream);

        m_streams.erase(iter++);

        delete pStream;
    }

    const HRESULT hr = m_pClassFactory->LockServer(FALSE);
    assert(SUCCEEDED(hr));
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
    const LONG n = InterlockedIncrement(&m_cRef);

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::AddRef: n=" << n << endl;
#endif

    return n;
}


ULONG WebmMfSource::Release()
{
    const LONG n = InterlockedDecrement(&m_cRef);

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Release: n=" << n << endl;
#endif

    if (n)
        return n;

    delete this;
    return 0;
}


HRESULT WebmMfSource::Load()
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Load" << endl;
#endif

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

    //TODO: this does big-bang loading, which is not what we
    //want.  Load clusters incrementally.
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
        return VFW_E_INVALID_FILE_FORMAT;

    assert(m_stream_descriptors.empty());
    assert(m_streams.empty());

    const ULONG nTracks = pTracks->GetTracksCount();

    for (ULONG idx = 0; idx < nTracks; ++idx)
    {
        mkvparser::Track* const pTrack = pTracks->GetTrackByIndex(idx);

        if (pTrack == 0)
            continue;

        const LONGLONG type = pTrack->GetType();

        IMFStreamDescriptor* pDesc;

        if (type == 1)  //video
        {
            hr = WebmMfStreamVideo::CreateStreamDescriptor(pTrack, pDesc);

            if (hr != S_OK)
                continue;

            assert(pDesc);
            m_stream_descriptors.push_back(pDesc);
        }
        else if (type == 2)  //audio
        {
            hr = WebmMfStreamAudio::CreateStreamDescriptor(pTrack, pDesc);

            if (hr != S_OK)
                continue;

            assert(pDesc);
            m_stream_descriptors.push_back(pDesc);
        }
    }

    if (m_stream_descriptors.empty())
        return VFW_E_INVALID_FILE_FORMAT;

    m_pSegment = pSegment.release();
    //TODO: m_pSeekBase = 0;
    //TODO: m_seekTime = kNoSeek;

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

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    const IMFMediaEventQueuePtr pEvents(m_pEvents);

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

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

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

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

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

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

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

    if (m_pEvents == 0)
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

    //TODO: these characteristics might be influenced by whether
    //this is a local file or a network download.  Also, depending
    //on how smart we are about parsing, on whether we have parsed
    //the entire file yet.

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

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    //http://msdn.microsoft.com/en-us/library/ms698990(v=VS.85).aspx
    //MFCreateStreamDescriptor

    //http://msdn.microsoft.com/en-us/library/ms695404(VS.85).aspx
    //MFCreatePresentationDescriptor

    stream_descriptors_t& sdv = m_stream_descriptors;
    assert(!sdv.empty());

    const DWORD cSD = static_cast<DWORD>(sdv.size());
    IMFStreamDescriptor** const apSD = &sdv[0];

    hr = MFCreatePresentationDescriptor(cSD, apSD, &pDesc);
    assert(SUCCEEDED(hr));
    assert(pDesc);

#ifdef _DEBUG
    DWORD dwCount;

    hr = pDesc->GetStreamDescriptorCount(&dwCount);
    assert(SUCCEEDED(hr));
    assert(dwCount == cSD);
#endif

    mkvparser::Tracks* const pTracks = m_pSegment->GetTracks();
    assert(pTracks);

    bool have_video = false;
    bool have_audio = false;

    for (DWORD idx = 0; idx < cSD; ++idx)
    {
        BOOL fSelected;
        IMFStreamDescriptorPtr pSD;

        hr = pDesc->GetStreamDescriptorByIndex(idx, &fSelected, &pSD);
        assert(SUCCEEDED(hr));
        assert(pSD);

        DWORD id;

        hr = pSD->GetStreamIdentifier(&id);
        assert(SUCCEEDED(hr));

        const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(id);
        assert(pTrack);

        const LONGLONG type = pTrack->GetType();

        if (type == 1)  //video
        {
            if (have_video)
            {
                hr = pDesc->DeselectStream(idx);
                assert(SUCCEEDED(hr));
            }
            else
            {
                hr = pDesc->SelectStream(idx);
                assert(SUCCEEDED(hr));

                have_video = true;
            }
        }
        else
        {
            assert(type == 2);  //audio

            if (have_audio)
            {
                hr = pDesc->DeselectStream(idx);
                assert(SUCCEEDED(hr));
            }
            else
            {
                hr = pDesc->SelectStream(idx);
                assert(SUCCEEDED(hr));

                have_audio = true;
            }
        }
    }

    const LONGLONG duration_ns = m_pSegment->GetDuration();
    assert(duration_ns >= 0);  //TODO

    const UINT64 duration = duration_ns / 100;

    hr = pDesc->SetUINT64(MF_PD_DURATION, duration);
    assert(SUCCEEDED(hr));  //TODO

    return S_OK;
}


HRESULT WebmMfSource::Start(
    IMFPresentationDescriptor* pDesc,
    const GUID* pTimeFormat,
    const PROPVARIANT* pPos)
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Start" << endl;
#endif

    //Writing a Custom Media Source:
    //http://msdn.microsoft.com/en-us/library/ms700134(v=VS.85).aspx

    //IMFMediaSource::Start Method
    //http://msdn.microsoft.com/en-us/library/ms694101%28v=VS.85%29.aspx

    if (pDesc == 0)
        return E_INVALIDARG;

    if ((pTimeFormat != 0) && (*pTimeFormat != GUID_NULL))
        return MF_E_UNSUPPORTED_TIME_FORMAT;

    if (pPos == 0)  //TODO: interpret this same as VT_EMPTY?
        return E_INVALIDARG;

    switch (pPos->vt)
    {
        case VT_I8:
        case VT_EMPTY:
            break;

        default:
            return MF_E_UNSUPPORTED_TIME_FORMAT;
    }

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    //TODO: vet caller's presentation descriptor:
    //if invalid then return MF_E_INVALID_PRESENTATION
    pDesc;
    //TODO: number of stream descriptors must match
    //TODO: at least one stream must be selected

    //THIS IS WRONG:
    //for each stream desc in pres desc
    //  if stream desc is selected
    //     if stream object does NOT exist already
    //        create stream object
    //        source sends MENewStream event
    //        stream object is born with started state
    //        TODO: WHAT TIME DO WE USE IF VT_EMPTY?
    //     else if stream object DOES exist
    //        source sends MEUpdatedStream event
    //        if prev state was stopped then
    //          source sends MESourceStarted event
    //        else if prev state was started or paused then
    //          if start pos is "use curr time" (VT_EMPTY) then
    //            source sends MESourceStarted event
    //            stream sends MEStreamStarted event
    //          else if start pos is "use new time" then
    //            source sends MESourceSeeked event
    //            stream sends MEStreamSeeked
    //          endif
    //        endif
    //        stream object is transitioned to started state
    //     endif
    //  else if stream desc is NOT selected
    //     if stream object does NOT exist already
    //        nothing special we need to do here
    //     else if stream object DOES exist
    //        TODO: NEED HELP FROM MS FOR THIS CASE
    //        FOR NOW STOP STREAM
    //     endif
    //  endif
    //endfor
    //
    //FOR NOW ASSUME THAT THE DESC NAMES ALL STREAMS
    //IN THE MEDIA.  TODO: NEED HELP FROM MS FOR THIS CASE.

    //I THINK THIS IS CORRECT:

    //if prev state was stopped then
    //    source sends MESourceStarted event
    //else if prev state was started or paused then
    //    if start pos is "use curr time" (VT_EMPTY) then
    //        source sends MESourceStarted event
    //    else if start pos is "use new time" then
    //        source sends MESourceSeeked event
    //    endif
    //endif

    //for each stream desc in pres desc
    //  if stream desc is selected
    //     if stream object does NOT exist already
    //        create stream object
    //        source sends MENewStream event
    //        stream object is born with selected state
    //        TODO: WHAT TIME DO WE USE IF VT_EMPTY?
    //        WavSource.cpp says:
    //          if new time then
    //            t = new time
    //          else if VT_EMPTY then
    //            if state = stopped then
    //              t = 0
    //            else
    //              t = curr
    //            endif
    //          endif
    //     else if stream object DOES exist
    //        set it to selected
    //        source sends MEUpdatedStream event
    //        if prev state was stopped then
    //           time is reset back to 0
    //        else if prev state was started or paused then
    //          if start pos is "use curr time" (VT_EMPTY) then
    //            source sends MESourceStarted event
    //            stream sends MEStreamStarted event
    //          else if start pos is "use new time" then
    //            source sends MESourceSeeked event
    //            stream sends MEStreamSeeked
    //          endif
    //        endif
    //     endif
    //  else if stream desc is NOT selected
    //     if stream object does NOT exist already
    //        nothing special we need to do here
    //     else if stream object DOES exist
    //        set it to unselected
    //        later, if RequestSample is called, return error
    //     endif
    //  endif
    //endfor


    //TODO:
    //  if start fails asynchronously (after method returns S_OK) then
    //     source sends MESourceStarted event with failure code
    //  else if start fails synchronously then
    //     return error code, and do not raise any events
    //  endif

    PROPVARIANT var;

    GetTime(pDesc, *pPos, var);
    assert(var.vt == VT_I8);

    const LONGLONG time = var.hVal.QuadPart;
    assert(time >= 0);

    DWORD count;

    hr = pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));

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

    //if VT_EMPTY then this is a re-start
    //we need to figure out what the re-start time is
    //first determine whether there were streams selected previously
    //  if they're still selected, then synthesize the re-start time
    //  from one of them

    for (DWORD index = 0; index < count; ++index)
    {
        BOOL bSelected;
        IMFStreamDescriptorPtr pSD;

        hr = pDesc->GetStreamDescriptorByIndex(index, &bSelected, &pSD);
        assert(SUCCEEDED(hr));
        assert(pSD);

        DWORD id;

        hr = pSD->GetStreamIdentifier(&id);
        assert(SUCCEEDED(hr));

        const mkvparser::Tracks* const pTracks = m_pSegment->GetTracks();
        assert(pTracks);

        mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(id);
        assert(pTrack);
        assert(pTrack->GetNumber() == id);

        typedef streams_t::iterator iter_t;

        const iter_t iter = m_streams.find(id);
        const iter_t iter_end = m_streams.end();

        if (!bSelected)
        {
            if (iter != iter_end)  //already exists
            {
                WebmMfStream* const pStream = iter->second;
                assert(pStream);
                assert(pStream->m_pTrack == pTrack);

                pStream->Deselect();
            }

            continue;
        }

        //selected

        if (iter == iter_end)  //does NOT exist already
        {
            hr = NewStream(pSD, pTrack, time);
            assert(SUCCEEDED(hr));
        }
        else  //stream DOES exist
        {
            WebmMfStream* const pStream = iter->second;
            assert(pStream);
            assert(pStream->m_pTrack == pTrack);

            hr = UpdateStream(pSD, pStream, time);
            assert(SUCCEEDED(hr));
        }
    }

    if (m_state == kStateStopped)
    {
        //TOOD: verify whether this is really a Started event.
        //Can it ever be a Seeked event?  I'm following the WavSource example.

        hr = m_pEvents->QueueEventParamVar(
                MESourceStarted,
                GUID_NULL,
                S_OK,
                &var);

        assert(SUCCEEDED(hr));

        hr = StartStreams(var);
        assert(SUCCEEDED(hr));
    }
    else if (pPos->vt == VT_I8)  //seek
    {
        hr = m_pEvents->QueueEventParamVar(
                MESourceSeeked,
                GUID_NULL,
                S_OK,
                &var);

        assert(SUCCEEDED(hr));

        hr = SeekStreams(var);
        assert(SUCCEEDED(hr));
    }
    else //re-start
    {
        assert(pPos->vt == VT_EMPTY);

        IMFMediaEventPtr pEvent;

        hr = MFCreateMediaEvent(
                MESourceStarted,
                GUID_NULL,
                S_OK,
                pPos,
                &pEvent);

        assert(SUCCEEDED(hr));
        assert(pEvent);

        hr = pEvent->SetUINT64(MF_EVENT_SOURCE_ACTUAL_START, time);
        assert(SUCCEEDED(hr));

        hr = m_pEvents->QueueEvent(pEvent);
        assert(SUCCEEDED(hr));

        hr = RestartStreams();
        assert(SUCCEEDED(hr));
    }

    m_state = kStateStarted;

    //TODO
    //if there was an error
    //  if we did NOT queue a MESourceStarted/MESourceSeeked event
    //     then it's OK to simply return an error from Start
    //  else we DID queue a Started/Seeked event
    //     then queue an MEError event

    return S_OK;
}


HRESULT WebmMfSource::Stop()
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Stop" << endl;
#endif

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    m_state = kStateStopped;

    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        WebmMfStream* const pStream = v.second;
        assert(pStream);

        hr = pStream->Stop();
        assert(SUCCEEDED(hr));
    }

    hr = QueueEvent(MESourceStopped, GUID_NULL, S_OK, 0);
    assert(SUCCEEDED(hr));

    return S_OK;
}



HRESULT WebmMfSource::Pause()
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Pause" << endl;
#endif

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    if (m_state != kStateStarted)
        return MF_E_INVALID_STATE_TRANSITION;

    m_state = kStatePaused;

    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        WebmMfStream* const pStream = v.second;
        assert(pStream);

        hr = pStream->Pause();
        assert(SUCCEEDED(hr));
    }

    hr = QueueEvent(MESourcePaused, GUID_NULL, S_OK, 0);
    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfSource::Shutdown()
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Shutdown" << endl;
#endif

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    typedef streams_t::iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        WebmMfStream* const pStream = i->second;
        assert(pStream);

        m_streams.erase(i++);

        hr = pStream->Shutdown();
        assert(SUCCEEDED(hr));
    }

    hr = m_pEvents->Shutdown();
    assert(SUCCEEDED(hr));

    const ULONG n = m_pEvents->Release();
    n;
    assert(n == 0);

    m_pEvents = 0;

    return S_OK;
}


HRESULT WebmMfSource::NewStream(
    IMFStreamDescriptor* pSD,
    mkvparser::Track* pTrack,
    LONGLONG time)
{
    assert(pSD);
    assert(pTrack);
    assert(time >= 0);

    WebmMfStream* pStream;
    const LONGLONG type = pTrack->GetType();

    if (type == 1)  //video
    {
        const HRESULT hr = WebmMfStreamVideo::CreateStream(
                            pSD,
                            this,
                            pTrack,
                            pStream);

        assert(SUCCEEDED(hr));  //TODO
        assert(pStream);
        //assert(pStream->IsSelected());
    }
    else
    {
        assert(type == 2);  //audio

        const HRESULT hr = WebmMfStreamAudio::CreateStream(
                            pSD,
                            this,
                            pTrack,
                            pStream);

        assert(SUCCEEDED(hr));  //TODO
        assert(pStream);
        //assert(pStream->IsSelected());
    }

    const LONGLONG id_ = pTrack->GetNumber();
    const ULONG id = static_cast<ULONG>(id_);

    typedef streams_t::iterator iter_t;
    typedef std::pair<iter_t, bool> status_t;

    const status_t status = m_streams.insert(std::make_pair(id, pStream));
    assert(status.second);  //new insertion
    assert(status.first->first == id);
    assert(status.first->second == pStream);

    pStream->Select(time);

    HRESULT hr = m_pEvents->QueueEventParamUnk(
                    MENewStream,
                    GUID_NULL,
                    S_OK,
                    pStream);

    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfSource::UpdateStream(
    IMFStreamDescriptor* pDesc,
    WebmMfStream* pStream,
    LONGLONG time)
{
    assert(pDesc);
    assert(pStream);
    assert(time >= 0);

    pStream->Select(time);

    HRESULT hr = m_pEvents->QueueEventParamUnk(
                    MEUpdatedStream,
                    GUID_NULL,
                    S_OK,
                    pStream);

    assert(SUCCEEDED(hr));

    return S_OK;
}


void WebmMfSource::GetTime(
    IMFPresentationDescriptor* pDesc,
    const PROPVARIANT& r,   //requested
    PROPVARIANT& a) const   //actual
{
    PropVariantInit(&a);

    a.vt = VT_I8;
    LONGLONG& t = a.hVal.QuadPart;

    if (r.vt == VT_I8)
    {
        a.hVal = r.hVal;

        if (t < 0)
            t = 0;

        return;
    }

    assert(r.vt == VT_EMPTY);

    if (m_state == kStateStopped)
    {
        t = 0;
        return;
    }

    streams_t already_selected;

    typedef streams_t::const_iterator iter_t;

    iter_t iter = m_streams.begin();
    iter_t iter_end = m_streams.end();

    while (iter != iter_end)
    {
        const streams_t::value_type& value = *iter++;

        WebmMfStream* const pStream = iter->second;
        assert(pStream);

        if (!pStream->IsSelected())
            continue;

        already_selected.insert(value);
    }

    streams_t newly_selected;

    DWORD count;

    HRESULT hr = pDesc->GetStreamDescriptorCount(&count);
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

        const mkvparser::Tracks* const pTracks = m_pSegment->GetTracks();
        assert(pTracks);

        mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(id);
        assert(pTrack);
        assert(pTrack->GetNumber() == id);

        iter = already_selected.find(id);

        if (iter != iter_end)  //already selected
        {
            WebmMfStream* const pStream = iter->second;
            assert(pStream);
            assert(pStream->m_pTrack == pTrack);

            newly_selected.insert(std::make_pair(id, pStream));
        }
    }

    if (!newly_selected.empty())
    {
        //We're supposed to use the minimum timestamp

        iter = newly_selected.begin();
        iter_end = newly_selected.end();

        LONGLONG min_time = -1;

        while (iter != iter_end)
        {
            const streams_t::value_type& value = *iter++;

            WebmMfStream* const pStream = value.second;
            assert(pStream);

            LONGLONG curr_time;
            //TODO: media time or presentation time?
            //If this a seek, then the presentation time is
            //the difference between the seek base and the
            //current block's media time.

            hr = pStream->GetCurrMediaTime(curr_time);
            assert(SUCCEEDED(hr));
            assert(curr_time >= 0);

            if (curr_time > min_time)
                min_time = curr_time;
        }

        t = min_time;
        return;
    }

    //Weird: none of the streams that were selected in this start request
    //had been selected in the previous start request.  It's hard to know
    //what "re-start from current position" even means in this case, but
    //whatever.  Let's just use the smallest time from what was selected
    //previously.

    iter = already_selected.begin();
    iter_end = already_selected.end();

    LONGLONG min_time = -1;

    while (iter != iter_end)
    {
        const streams_t::value_type& value = *iter++;

        WebmMfStream* const pStream = value.second;
        assert(pStream);

        LONGLONG curr_time;  //TODO: see my comments above

        hr = pStream->GetCurrMediaTime(curr_time);
        assert(SUCCEEDED(hr));
        assert(curr_time >= 0);

        if (curr_time > min_time)
            min_time = curr_time;
    }

    t = min_time;
}


HRESULT WebmMfSource::StartStreams(const PROPVARIANT& var)
{
    typedef streams_t::iterator iter_t;

    iter_t iter = m_streams.begin();
    const iter_t iter_end = m_streams.end();

    m_cEOS = 0;

    while (iter != iter_end)
    {
        const streams_t::value_type& value = *iter++;

        WebmMfStream* const pStream = value.second;
        assert(pStream);

        const HRESULT hr = pStream->Start(var);
        assert(SUCCEEDED(hr));

        if (pStream->IsSelected())
            ++m_cEOS;
    }

    return S_OK;
}


HRESULT WebmMfSource::SeekStreams(const PROPVARIANT& var)
{
    typedef streams_t::iterator iter_t;

    iter_t iter = m_streams.begin();
    const iter_t iter_end = m_streams.end();

    while (iter != iter_end)
    {
        const streams_t::value_type& value = *iter++;

        WebmMfStream* const pStream = value.second;
        assert(pStream);

        const HRESULT hr = pStream->Seek(var);
        assert(SUCCEEDED(hr));
    }

    return S_OK;
}


HRESULT WebmMfSource::RestartStreams()
{
    typedef streams_t::iterator iter_t;

    iter_t iter = m_streams.begin();
    const iter_t iter_end = m_streams.end();

    while (iter != iter_end)
    {
        const streams_t::value_type& value = *iter++;

        WebmMfStream* const pStream = value.second;
        assert(pStream);

        const HRESULT hr = pStream->Restart();
        assert(SUCCEEDED(hr));
    }

    return S_OK;
}


void WebmMfSource::NotifyEOS()
{
    assert(m_cEOS > 0);

    --m_cEOS;

    if (m_cEOS == 0)
    {
        assert(m_pEvents);

        const HRESULT hr = m_pEvents->QueueEventParamVar(
                            MEEndOfPresentation,
                            GUID_NULL,
                            S_OK,
                            0);

        assert(SUCCEEDED(hr));
    }
}

}  //end namespace WebmMfSource
