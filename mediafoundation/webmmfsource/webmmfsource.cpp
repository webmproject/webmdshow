#include "webmmfsource.hpp"
#include "webmmfstream.hpp"
#include "webmmfstreamvideo.hpp"
#include "webmmfstreamaudio.hpp"
#include "webmmfbytestreamhandler.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <new>
#include <cassert>
#include <comdef.h>
#include <vfwmsgs.h>  //TODO: replace with <mferror.h>
#include <memory>
#include <malloc.h>
#include <cmath>
#include <climits>
#include <utility>  //std::make_pair
#include <process.h>
#include <algorithm>
#ifdef _DEBUG
#include "odbgstream.hpp"
#include "iidstr.hpp"
using std::hex;
using std::endl;
using std::boolalpha;
#endif

//"D:\src\mediafoundation\topoedit\app\Debug\topoedit.exe"
//"C:\Program Files (x86)\Windows Media Player\wmplayer.exe"

using std::wstring;

_COM_SMARTPTR_TYPEDEF(IMFMediaEventQueue, __uuidof(IMFMediaEventQueue));
_COM_SMARTPTR_TYPEDEF(IMFStreamDescriptor, __uuidof(IMFStreamDescriptor));
_COM_SMARTPTR_TYPEDEF(IMFMediaEvent, __uuidof(IMFMediaEvent));


namespace WebmMfSourceLib
{

HRESULT WebmMfSource::CreateSource(
    IClassFactory* pCF,
    IMFByteStream* pBS,
    WebmMfSource*& pSource)
{
    pSource = new (std::nothrow) WebmMfSource(pCF, pBS);
    return pSource ? S_OK : E_OUTOFMEMORY;
}


#pragma warning(disable:4355)  //'this' ptr in member init list
WebmMfSource::WebmMfSource(
    IClassFactory* pCF,
    IMFByteStream* pBS) :
    m_pClassFactory(pCF),
    m_cRef(1),
    m_file(pBS),
    m_pSegment(0),
    m_preroll_ns(-1),
    m_bThin(FALSE),
    m_rate(1),
    m_async_read(this),
    m_hQuit(0),
    m_hRequestSample(0),
    m_hAsyncRead(0),
    m_hThread(0),
    m_track_init(0)
{
    HRESULT hr = m_pClassFactory->LockServer(TRUE);
    assert(SUCCEEDED(hr));

    hr = CLockable::Init();
    assert(SUCCEEDED(hr));  //TODO

    hr = MFCreateEventQueue(&m_pEvents);
    assert(SUCCEEDED(hr));
    assert(m_pEvents);

    m_thread_state = &WebmMfSource::StateAsyncRead;

    hr = InitThread();
    assert(SUCCEEDED(hr));

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource: ctor; this=0x" << (const void*)this << endl;
#endif
}
#pragma warning(default:4355)


WebmMfSource::~WebmMfSource()
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource: dtor; this=0x" << (const void*)this << endl;
#endif

    FinalThread();

    while (!m_requests.empty())
    {
        Request& r = m_requests.front();

        if (r.pToken)
            r.pToken->Release();

        m_requests.pop_front();
    }

    if (m_pEvents)
    {
        const ULONG n = m_pEvents->Release();
        n;
        assert(n == 0);

        m_pEvents = 0;
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
        assert(pStream->m_cRef == 0);

        m_streams.erase(iter++);

        delete pStream;
    }

    delete m_pSegment;
    m_pSegment = 0;

    const HRESULT hr = m_pClassFactory->LockServer(FALSE);
    assert(SUCCEEDED(hr));
}


HRESULT WebmMfSource::InitThread()
{
    m_hQuit = CreateEvent(0, 0, 0, 0);

    if (m_hQuit == 0)
        return HRESULT_FROM_WIN32(GetLastError());

    m_hRequestSample = CreateEvent(0, 0, 0, 0);

    if (m_hRequestSample == 0)
        return HRESULT_FROM_WIN32(GetLastError());

    m_hAsyncRead = CreateEvent(0, 0, 0, 0);

    if (m_hAsyncRead == 0)
        return HRESULT_FROM_WIN32(GetLastError());

    const uintptr_t h = _beginthreadex(
                            0,  //security,
                            0,  //stack size
                            &WebmMfSource::ThreadProc,
                            this,
                            0,  //run immediately
                            0);  //thread id

    if (h == 0)
        return E_FAIL;

    m_hThread = reinterpret_cast<HANDLE>(h);

    return S_OK;
}


void WebmMfSource::FinalThread()
{
    if (m_hThread)
    {
        assert(m_hQuit);

        BOOL b = SetEvent(m_hQuit);
        assert(b);

        const DWORD dw = WaitForSingleObject(m_hThread, 10 * 1000);
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

    if (m_hAsyncRead)
    {
        const BOOL b = CloseHandle(m_hAsyncRead);
        assert(b);

        m_hAsyncRead = 0;
    }

    if (m_hQuit)
    {
        const BOOL b = CloseHandle(m_hQuit);
        assert(b);

        m_hQuit = 0;
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
        pUnk = static_cast<IMFMediaSource*>(this);  //must be nondelegating
    }
    else if (iid == __uuidof(IMFMediaEventGenerator))
    {
        pUnk = static_cast<IMFMediaEventGenerator*>(this);
    }
    else if (iid == __uuidof(IMFMediaSource))
    {
        pUnk = static_cast<IMFMediaSource*>(this);
    }
    else if (iid == __uuidof(IMFRateControl))
    {
        pUnk = static_cast<IMFRateControl*>(this);
    }
    else if (iid == __uuidof(IMFRateSupport))
    {
        pUnk = static_cast<IMFRateSupport*>(this);
    }
    else if (iid == __uuidof(IMFGetService))
    {
        pUnk = static_cast<IMFGetService*>(this);
    }
    else
    {
#ifdef _DEBUG
        wodbgstream os;
        os << "WebmMfSource::QI: iid=" << IIDStr(iid) << std::endl;
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

#if 0 //def _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::AddRef: n=" << n
       << " this=0x" << (const void*)this
       << endl;
#endif

    return n;
}


ULONG WebmMfSource::Release()
{
    const LONG n = InterlockedDecrement(&m_cRef);

#if 0 //def _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Release: n=" << n
       << " this=0x" << (const void*)this
       << endl;
#endif

    if (n)
        return n;

    delete this;
    return 0;
}


HRESULT WebmMfSource::BeginLoad(IMFAsyncCallback* pCB)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pLoadResult)
        return MF_E_UNEXPECTED;

    IMFAsyncResultPtr pLoadResult;

    hr = MFCreateAsyncResult(0, pCB, 0, &pLoadResult);

    if (FAILED(hr))
        return hr;

    assert(m_thread_state == &WebmMfSource::StateAsyncRead);

    m_file.ResetAvailable();

    hr = m_file.AsyncReadInit(0, 1024, &m_async_read);

    if (FAILED(hr))
        return hr;

    m_async_state = &WebmMfSource::StateAsyncParseEbmlHeader;

    if (hr == S_OK)  //all bytes already in cache
    {
        m_async_read.m_hrStatus = S_OK;

        const BOOL b = SetEvent(m_hAsyncRead);
        assert(b);
    }

    m_pLoadResult = pLoadResult;
    return S_OK;
}


#if 0
HRESULT WebmMfSource::EndLoad(IMFAsyncResult* pLoadResult)
{
    if (pLoadResult == 0)
        return E_INVALIDARG;

    const HRESULT hrLoad = pLoadResult->GetStatus();

    if (FAILED(hrLoad))
        Shutdown();

    return hrLoad;
}
#endif


WebmMfSource::thread_state_t
WebmMfSource::LoadComplete(HRESULT hrLoad)
{
    if (m_pLoadResult)  //should always be true
    {
        HRESULT hr = m_pLoadResult->SetStatus(hrLoad);
        assert(SUCCEEDED(hr));

        hr = MFInvokeCallback(m_pLoadResult);
        assert(SUCCEEDED(hr));

        m_pLoadResult = 0;
    }

    if (FAILED(hrLoad))
        return &WebmMfSource::StateQuit;

    return 0;  //remain in async reading state, but wait for start
}


HRESULT WebmMfSource::ParseEbmlHeader(LONGLONG& pos)
{
    mkvparser::EBMLHeader h;

    const long long status = h.Parse(&m_file, pos);

    if (status != 0)  //TODO: liberalize
        return VFW_E_INVALID_FILE_FORMAT;

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

    return S_OK;
}


WebmMfSource::thread_state_t WebmMfSource::StateAsyncParseEbmlHeader()
{
    LONGLONG pos;

    const HRESULT hr = ParseEbmlHeader(pos);

    if (FAILED(hr))
        return LoadComplete(E_FAIL);

    using mkvparser::Segment;

    assert(m_pSegment == 0);  //TODO

    const LONGLONG result = Segment::CreateInstance(&m_file, pos, m_pSegment);

    if (result != 0)  //TODO: liberalize
        return LoadComplete(E_FAIL);

    m_async_state = &WebmMfSource::StateAsyncParseSegmentHeaders;
    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hAsyncRead);
    assert(b);

    return 0;
}


WebmMfSource::thread_state_t WebmMfSource::StateAsyncParseSegmentHeaders()
{
    assert(m_pSegment);

    for (;;)
    {
        const long long status = m_pSegment->ParseHeaders();

        if (status == 0)
            break;

        if (status < 0)
            return LoadComplete(E_FAIL);

        assert(status <= LONG_MAX);
        const LONG len = static_cast<LONG>(status);

        const HRESULT hr = m_file.AsyncReadInit(0, len, &m_async_read);

        if (FAILED(hr))
            return LoadComplete(E_FAIL);

        if (hr == S_FALSE)  //async read in progress
            return 0;

        //S_OK means requested bytes already in cache
    }

#ifdef _DEBUG
    if (const mkvparser::SegmentInfo* pInfo = m_pSegment->GetInfo())
    {
        wstring muxingApp, writingApp;

        if (const char* str = pInfo->GetMuxingAppAsUTF8())
            muxingApp = ConvertFromUTF8(str);

        if (const char* str = pInfo->GetWritingAppAsUTF8())
            writingApp = ConvertFromUTF8(str);

        pInfo = 0;
    }
#endif

    const mkvparser::Tracks* const pTracks = m_pSegment->GetTracks();

    if (pTracks == 0)
        return LoadComplete(E_FAIL);

    assert(m_stream_descriptors.empty());
    assert(m_streams.empty());

    const ULONG nTracks = pTracks->GetTracksCount();

    for (ULONG idx = 0; idx < nTracks; ++idx)
    {
        const mkvparser::Track* const pTrack = pTracks->GetTrackByIndex(idx);

        if (pTrack == 0)  //weird
            continue;

        const LONGLONG type = pTrack->GetType();

        IMFStreamDescriptor* pDesc;
        HRESULT hr;

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
        return LoadComplete(E_FAIL);

    return LoadComplete(S_OK);
}


WebmMfSource::thread_state_t WebmMfSource::StateAsyncParseCues()
{
    const mkvparser::Cues* pCues = m_pSegment->GetCues();

    if (pCues == 0)
    {
        const mkvparser::SeekHead* const pSH = m_pSegment->GetSeekHead();
        assert(pSH);

        const int count = pSH->GetCount();
        assert(count > 0);

        LONGLONG cues_off = -1;  //offset relative to start of segment

        for (int idx = 0; idx < count; ++idx)
        {
            const mkvparser::SeekHead::Entry* const p = pSH->GetEntry(idx);

            if (p->id == 0x0C53BB6B)  //Cues ID
            {
                cues_off = p->pos;
                assert(cues_off >= 0);

                break;
            }
        }

        assert(cues_off >= 0);

        for (;;)  //parsing cues element
        {
            LONGLONG pos;
            LONG len;

            const long status = m_pSegment->ParseCues(cues_off, pos, len);

            if (status == 0)  //we have cues; fall through
            {
                pCues = m_pSegment->GetCues();
                assert(pCues);

                break;
            }

            if (status > 0)  //weird: parse was successful, but no cues
            {
                assert(m_pSegment->GetCues() == 0);

                m_file.ResetAvailable();
                m_track_init = 0;

                //here we transition to a new async (sub)state:
                m_async_state = &WebmMfSource::StateAsyncInitStreams;

                m_async_read.m_hrStatus = S_OK;

                const BOOL b = SetEvent(m_hAsyncRead);
                assert(b);

                return 0;  //stay in async read state
            }

            if (status != mkvparser::E_BUFFER_NOT_FULL)
            {
                //assume we cannot recover
                //TODO: announce error to pipline

                //transition to new (super)state:
                return &WebmMfSource::StateQuit;
            }

            const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

            if (FAILED(hr))
                return &WebmMfSource::StateQuit;  //TODO: announce error

            if (hr == S_FALSE)  //async read in progress
                return 0;       //no transition here

            hr;  //requested bytes were already in cache
            continue;
        }  //parsing cues element
    }

    if (!pCues->LoadCuePoint())
    {
        m_file.ResetAvailable();
        m_track_init = 0;

        m_async_state = &WebmMfSource::StateAsyncInitStreams;
    }

    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hAsyncRead);
    assert(b);

    return 0;  //stay in async read state

}


WebmMfSource::thread_state_t WebmMfSource::StateAsyncInitStreams()
{
    if (m_pSegment->GetCount() == 0)
    {
        for (;;)
        {
            LONGLONG pos;
            LONG len;

            const long status = m_pSegment->LoadCluster(pos, len);

            if (status == 0)  //have new cluster
                break;

            if (status > 0)  //EOF
                return LoadComplete(E_FAIL);

            if (status != mkvparser::E_BUFFER_NOT_FULL)
                return LoadComplete(E_FAIL);

            const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

            if (FAILED(hr))
                return LoadComplete(E_FAIL);

            if (hr == S_FALSE)  //async read in progress
                return 0;
        }

        assert(m_pSegment->GetCount() == 1);
    }

    const mkvparser::Cluster* const pCluster = m_pSegment->GetLast();
    assert(pCluster);
    assert(!pCluster->EOS());

    const mkvparser::Tracks* const pTracks = m_pSegment->GetTracks();
    assert(pTracks);

    const ULONG nTracks = pTracks->GetTracksCount();
    assert(nTracks > 0);

    ULONG& idx = m_track_init;

    while (idx < nTracks)
    {
        const mkvparser::Track* const pTrack = pTracks->GetTrackByIndex(idx);

        if (pTrack == 0)  //weird
        {
            ++idx;
            continue;
        }

        const LONGLONG tn = pTrack->GetNumber();

        const mkvparser::BlockEntry* pCurr = pCluster->GetFirst();

        while (pCurr)
        {
            const mkvparser::Block* const pBlock = pCurr->GetBlock();
            assert(pBlock);

            if (pBlock->GetTrackNumber() == tn)
                break;

            pCurr = pCluster->GetNext(pCurr);
        }

        if (pCurr == 0)  //no matching track found in this cluster
            break;

        ++idx;  //this track has now been initialized
    }

    if (idx >= nTracks)  //stream init done
    {
        m_file.EnableBuffering(GetDuration());  //do this here, or sooner?
        return &WebmMfSource::StateRequestSample;
    }

    if (m_pSegment->GetCount() >= 10)
        return &WebmMfSource::StateQuit;  //TODO: handle as EOS

    //must load another cluster to init stream(s)

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = m_pSegment->LoadCluster(pos, len);

        if (status == 0)  //have new cluster
        {
            m_async_read.m_hrStatus = S_OK;

            const BOOL b = SetEvent(m_hAsyncRead);
            assert(b);

            return 0;
        }

        if (status > 0)  //EOF (weird)
            return LoadComplete(E_FAIL);

        if (status != mkvparser::E_BUFFER_NOT_FULL)  //parse error
            return LoadComplete(E_FAIL);

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
            return LoadComplete(E_FAIL);

        if (hr == S_FALSE)  //async read in progress
            return 0;
    }
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

#if 0  //TDOO: RESTORE THIS
    dw = MFMEDIASOURCE_CAN_PAUSE |
         MFMEDIASOURCE_CAN_SEEK;
#else
    dw = MFMEDIASOURCE_CAN_PAUSE;
#endif

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

    const mkvparser::Tracks* const pTracks = m_pSegment->GetTracks();
    assert(pTracks);

    LONG idx_video = -1;
    LONG idx_audio = -1;

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
            if (idx_video >= 0)
            {
                hr = pDesc->DeselectStream(idx);
                assert(SUCCEEDED(hr));
            }
            else
            {
                hr = pDesc->SelectStream(idx);
                assert(SUCCEEDED(hr));

                idx_video = idx;
            }
        }
        else
        {
            assert(type == 2);  //audio

            if (idx_audio >= 0)
            {
                hr = pDesc->DeselectStream(idx);
                assert(SUCCEEDED(hr));
            }
            else
            {
                hr = pDesc->SelectStream(idx);
                assert(SUCCEEDED(hr));

                idx_audio = idx;
            }
        }
    }

    const LONGLONG duration = GetDuration();  //reftime units

    if (duration > 0)
    {
        const HRESULT hr = pDesc->SetUINT64(MF_PD_DURATION, duration);
        assert(SUCCEEDED(hr));
    }

    LONGLONG total, avail;

    const int status = m_file.Length(&total, &avail);

    if ((status >= 0) && (total >= 0))
    {
        const HRESULT hr = pDesc->SetUINT64(MF_PD_TOTAL_FILE_SIZE, total);
        assert(SUCCEEDED(hr));
    }

    return S_OK;
}


LONGLONG WebmMfSource::GetDuration() const
{
    const LONGLONG duration_ns = m_pSegment->GetDuration();

    if (duration_ns > 0)
    {
        const UINT64 reftime = duration_ns / 100;  //reftime units
        return reftime;
    }

#if 0  //TODO: RESTORE THIS
    if (const mkvparser::Cues* pCues = m_pSegment->GetCues())
    {
        using namespace mkvparser;

        const CuePoint* const pCP = pCues->GetLast();
        assert(pCP);  //TODO

        const Tracks* const pTracks = m_pSegment->GetTracks();
        const ULONG count = pTracks->GetTracksCount();

        for (ULONG idx = 0; idx < count; ++idx)
        {
            const Track* const pTrack = pTracks->GetTrackByIndex(idx);

            if (pTrack == 0)
                continue;

            const CuePoint::TrackPosition* const pTP = pCP->Find(pTrack);

            if (pTP == 0)
                continue;

            const BlockEntry* const pBE = pCues->GetBlock(pCP, pTP);

            if ((pBE == 0) || pBE->EOS())
                continue;

            const Cluster* pCluster = pBE->GetCluster();
            assert(pCluster);
            assert(!pCluster->EOS());

            if (pCluster->m_index >= 0)  //loaded
            {
                const Cluster* const p = m_pSegment->GetLast();
                assert(p);
                assert(p->m_index >= 0);

                pCluster = p;
            }
            else //pre-loaded
            {
                for (int i = 0; i < 10; ++i)
                {
                    const Cluster* const p = m_pSegment->GetNext(pCluster);

                    if ((p == 0) || p->EOS())
                        break;

                    pCluster = p;
                }
            }

            const LONGLONG ns = pCluster->GetLastTime();
            assert(ns >= 0);

            const UINT64 reftime = ns / 100;  //reftime
            return reftime;
        }
    }
#endif

    //TODO: anything else we can do here?

    return -1;
}


#if 0  //TODO: restore this
HRESULT WebmMfSource::Start(
    IMFPresentationDescriptor* pDesc,
    const GUID* pTimeFormat,
    const PROPVARIANT* pPos)
{
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

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Start (begin); requests.size="
       << m_requests.size()
       << endl;
#endif

    assert(m_requests.empty());  //TODO

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

#ifdef _DEBUG
    os << L"start reftime=" << time
       << L" secs=" << (double(time) / 10000000)
       << endl;
#endif

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

        const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(id);
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
            hr = NewStream(pSD, pTrack);
            assert(SUCCEEDED(hr));
        }
        else  //stream DOES exist
        {
            WebmMfStream* const pStream = iter->second;
            assert(pStream);
            assert(pStream->m_pTrack == pTrack);

            hr = UpdateStream(pStream);
            assert(SUCCEEDED(hr));
        }
    }

    const State old_state = m_state;

    m_state = kStateStarted;  //new state

    if (old_state == kStateStopped)
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

    //TODO
    //if there was an error
    //  if we did NOT queue a MESourceStarted/MESourceSeeked event
    //     then it's OK to simply return an error from Start
    //  else we DID queue a Started/Seeked event
    //     then queue an MEError event

#ifdef _DEBUG
    os << L"WebmMfSource::Start (end)" << endl;
#endif

    return S_OK;
}
#else  //TODO


bool WebmMfSource::IsStopped() const
{
    const commands_t& cc = m_commands;
    return (cc.empty() || (cc.back().m_kind == Command::kStop));
}


bool WebmMfSource::IsPaused() const
{
    const commands_t& cc = m_commands;
    return (!cc.empty() && (cc.back().m_kind == Command::kPause));
}


HRESULT WebmMfSource::Start(
    IMFPresentationDescriptor* pDesc,
    const GUID* pTimeFormat,
    const PROPVARIANT* pPos)
{
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

    commands_t& cc = m_commands;

    if (cc.empty())  //first time
    {
        assert(m_thread_state == &WebmMfSource::StateAsyncRead);

        cc.push_back(Command(Command::kStop));  //establish invariant

        bool bParseCues = false;

        if (m_pSegment->GetCues())
            bParseCues = true;

        else if (const mkvparser::SeekHead* pSH = m_pSegment->GetSeekHead())
        {
            const int count = pSH->GetCount();

            for (int idx = 0; idx < count; ++idx)
            {
                const mkvparser::SeekHead::Entry* const p = pSH->GetEntry(idx);

                if (p->id == 0x0C53BB6B)  //Cues ID
                {
                    bParseCues = true;

                    //TODO: is this needed here?
                    //m_file.ResetAvailable();

                    break;
                }
            }
        }

        if (bParseCues)
            m_async_state = &WebmMfSource::StateAsyncParseCues;
        else
        {
            m_track_init = 0;
            m_async_state = &WebmMfSource::StateAsyncInitStreams;
        }

        m_async_read.m_hrStatus = S_OK;

        const BOOL b = SetEvent(m_hAsyncRead);
        assert(b);
    }

    if (cc.back().m_kind == Command::kStop)  //start
        cc.push_back(Command(Command::kStart));

    else if (pPos->vt == VT_I8)  //seek
        //TODO: cc.push_back(Command(Command::kSeek));
        return E_NOTIMPL;

    else
        cc.push_back(Command(Command::kRestart));

    Command& c = cc.back();

    c.SetDesc(pDesc);
    c.SetTime(*pPos);

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return S_OK;
}
#endif  //TODO


#if 0  //TODO: restore this
HRESULT WebmMfSource::Stop()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Stop (begin): requests.size="
       << m_requests.size()
       << endl;
#endif

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

#ifdef _DEBUG
    os << L"WebmMfSource::Stop (end)" << endl;
#endif

    return S_OK;
}
#else
HRESULT WebmMfSource::Stop()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    commands_t& cc = m_commands;
    cc.push_back(Command(Command::kStop));

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return S_OK;
}
#endif


#if 0
HRESULT WebmMfSource::Pause()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Pause: requests.size="
       << m_requests.size()
       << endl;
#endif

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
#else
HRESULT WebmMfSource::Pause()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Pause" << endl;
#endif

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    commands_t& cc = m_commands;

    if (cc.empty())
        return MF_E_INVALID_STATE_TRANSITION;

    switch (cc.back().m_kind)
    {
        case Command::kStart:
        case Command::kSeek:
        case Command::kRestart:
            break;

        case Command::kPause:
        case Command::kStop:
            return MF_E_INVALID_STATE_TRANSITION;

        default:
            assert(false);
            return E_FAIL;
    }

    cc.push_back(Command(Command::kPause));

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return S_OK;
}
#endif


#if 0 //TODO: restore this
HRESULT WebmMfSource::Shutdown()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Shutdown: requests.size="
       << m_requests.size()
       << endl;
#endif

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    typedef streams_t::iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        WebmMfStream* const pStream = v.second;
        assert(pStream);

        hr = pStream->Shutdown();
        assert(SUCCEEDED(hr));
    }

    hr = m_pEvents->Shutdown();
    assert(SUCCEEDED(hr));

    const ULONG n = m_pEvents->Release();
    n;
    assert(n == 0);

    m_pEvents = 0;

    const BOOL b = SetEvent(m_hQuit);
    assert(b);

    return S_OK;
}
#else
HRESULT WebmMfSource::Shutdown()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Shutdown: requests.size="
       << m_requests.size()
       << endl;
#endif

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    typedef streams_t::iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        WebmMfStream* const pStream = v.second;
        assert(pStream);

        hr = pStream->Shutdown();
        assert(SUCCEEDED(hr));
    }

    hr = m_pEvents->Shutdown();
    assert(SUCCEEDED(hr));

    const ULONG n = m_pEvents->Release();
    n;
    assert(n == 0);

    m_pEvents = 0;

    //TODO: do this in dtor only?
    //The problem is that we haven't addressed the
    //issue of requests that haven't been serviced yet.
    //We do delete them in the dtor, but it might be
    //better to let the worker thread run normally,
    //until the request queue becomes exhausted.

    const BOOL b = SetEvent(m_hQuit);
    assert(b);

    return S_OK;
}
#endif


#if 0  //TODO
HRESULT WebmMfSource::SetRate(BOOL bThin, float rate)
{
#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::SetRate: bThin="
       << boolalpha << (bThin ? true : false)
       << " rate="
       << rate
       << " state=" << m_state
       << endl;
#endif

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    if (rate < 0)
        return MF_E_REVERSE_UNSUPPORTED;  //TODO

#if 0  //TODO: restore this
    if ((m_pSegment->GetCues() == 0) && bThin)
        return MF_E_THINNING_UNSUPPORTED;
#else
    if (bThin)
        return MF_E_THINNING_UNSUPPORTED;
#endif

    //IMFRateControl::SetRate Method
    //http://msdn.microsoft.com/en-us/library/ms696979%28v=VS.85%29.aspx

    //TODO:
    //If the transition is not supported, the method returns
    //MF_E_UNSUPPORTED_RATE_TRANSITION.

    //When a media source completes a call to SetRate, it sends
    //the MESourceRateChanged event. Other pipeline components do
    //not send this event.

    //TODO:
    //If a media source switches between thinned and non-thinned playback,
    //the streams send an MEStreamThinMode event to indicate the transition.
    //Events from the media source are not synchronized with events from
    //the media streams. After you receive the MESourceRateChanged event,
    //you can still receive samples that were queued before the stream
    //switched to thinned or non-thinned mode. The MEStreamThinMode event
    //marks the exact point in the stream where the transition occurs.

    //TODO: suppose bThin is false, but (m_rate != 1) ?

    m_bThin = bThin;
    m_rate = rate;

    typedef streams_t::iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        streams_t::value_type& value = *i++;

        WebmMfStream* const pStream = value.second;
        assert(pStream);

        if (pStream->IsSelected())
            pStream->SetRate(m_bThin, m_rate);
    }

    PROPVARIANT var;

    var.vt = VT_R4;
    var.fltVal = rate;

    hr = m_pEvents->QueueEventParamVar(
            MESourceRateChanged,
            GUID_NULL,
            S_OK,
            &var);

    assert(SUCCEEDED(hr));

    return S_OK;
}
#else
HRESULT WebmMfSource::SetRate(BOOL bThin, float rate)
{
#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::SetRate: bThin="
       << boolalpha << (bThin ? true : false)
       << " rate="
       << rate
       //<< " state=" << m_state
       << endl;
#endif

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    if (rate < 0)
        return MF_E_REVERSE_UNSUPPORTED;  //TODO

#if 0  //TODO: restore this
    if ((m_pSegment->GetCues() == 0) && bThin)
        return MF_E_THINNING_UNSUPPORTED;
#else
    if (bThin)
        return MF_E_THINNING_UNSUPPORTED;
#endif

    //IMFRateControl::SetRate Method
    //http://msdn.microsoft.com/en-us/library/ms696979%28v=VS.85%29.aspx

    //TODO:
    //If the transition is not supported, the method returns
    //MF_E_UNSUPPORTED_RATE_TRANSITION.

    //When a media source completes a call to SetRate, it sends
    //the MESourceRateChanged event. Other pipeline components do
    //not send this event.

    //TODO:
    //If a media source switches between thinned and non-thinned playback,
    //the streams send an MEStreamThinMode event to indicate the transition.
    //Events from the media source are not synchronized with events from
    //the media streams. After you receive the MESourceRateChanged event,
    //you can still receive samples that were queued before the stream
    //switched to thinned or non-thinned mode. The MEStreamThinMode event
    //marks the exact point in the stream where the transition occurs.

    //TODO: suppose bThin is false, but (m_rate != 1) ?

    m_bThin = bThin;
    m_rate = rate;

#if 0  //TODO: restore this
    typedef streams_t::iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        streams_t::value_type& value = *i++;

        WebmMfStream* const pStream = value.second;
        assert(pStream);

        if (pStream->IsSelected())
            pStream->SetRate(m_bThin, m_rate);
    }

    PROPVARIANT var;

    var.vt = VT_R4;
    var.fltVal = rate;

    hr = m_pEvents->QueueEventParamVar(
            MESourceRateChanged,
            GUID_NULL,
            S_OK,
            &var);

    assert(SUCCEEDED(hr));
#endif

    return S_OK;
}
#endif


HRESULT WebmMfSource::GetRate(BOOL* pbThin, float* pRate)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    if (pbThin)
        *pbThin = m_bThin;

    if (pRate)
        *pRate = m_rate;

#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::GetRate: thin_ns="
       << boolalpha
       << (m_bThin ? true : false)
       << " rate="
       << m_rate
       << endl;
#endif

    return S_OK;
}


HRESULT WebmMfSource::GetSlowestRate(
    MFRATE_DIRECTION d,
    BOOL bThin,
    float* pRate)
{
//#ifdef _DEBUG
//    odbgstream os;
//    os << "WebmMfSource::GetSlowestRate" << endl;
//#endif

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    if (d == MFRATE_REVERSE)
        return MF_E_REVERSE_UNSUPPORTED;  //TODO

#if 0  //TODO: restore this
    if ((m_pSegment->GetCues() == 0) && bThin)
        return MF_E_THINNING_UNSUPPORTED;
#else
    if (bThin)
        return MF_E_THINNING_UNSUPPORTED;
#endif

    if (pRate == 0)
        return E_POINTER;

    float& r = *pRate;
    r = 0;

    return S_OK;
}


HRESULT WebmMfSource::GetFastestRate(
    MFRATE_DIRECTION d,
    BOOL bThin,
    float* pRate)
{
//#ifdef _DEBUG
//    odbgstream os;
//    os << "WebmMfSource::GetFastestRate" << endl;
//#endif

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    if (d == MFRATE_REVERSE)
        return MF_E_REVERSE_UNSUPPORTED;  //TODO

#if 0  //TODO: restore this
    if ((m_pSegment->GetCues() == 0) && bThin)
        return MF_E_THINNING_UNSUPPORTED;
#else
    if (bThin)
        return MF_E_THINNING_UNSUPPORTED;
#endif

    if (pRate == 0)
        return E_POINTER;

    float& r = *pRate;
    r = 128;  //more or less arbitrary

    return S_OK;
}


HRESULT WebmMfSource::IsRateSupported(
    BOOL bThin,
    float rate,
    float* pNearestRate)
{
//#ifdef _DEBUG
//    odbgstream os;
//    os << "WebmMfSource::IsRateSupported: rate=" << rate << endl;
//#endif

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    if (rate < 0)
        return MF_E_REVERSE_UNSUPPORTED;  //TODO

#if 0  //TODO: restore this
    if ((m_pSegment->GetCues() == 0) && bThin)
        return MF_E_THINNING_UNSUPPORTED;
#else
    if (bThin)
        return MF_E_THINNING_UNSUPPORTED;
#endif

    //float int_part;
    //const float frac_part = modf(rate, &int_part);

    if (rate > 128)
    {
        if (pNearestRate)
            *pNearestRate = 128;

        return MF_E_UNSUPPORTED_RATE;
    }

    if (pNearestRate)
        *pNearestRate = rate;  //TODO

    return S_OK;  //TODO
}


HRESULT WebmMfSource::GetService(
    REFGUID sid,
    REFIID iid,
    LPVOID* ppv)
{
    if (sid == MF_RATE_CONTROL_SERVICE)
        return WebmMfSource::QueryInterface(iid, ppv);

    if (ppv)
        *ppv = 0;

    return MF_E_UNSUPPORTED_SERVICE;
}



#if 0
HRESULT WebmMfSource::NewStream(
    IMFStreamDescriptor* pSD,
    const mkvparser::Track* pTrack)
{
    assert(pSD);
    assert(pTrack);

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
    }

    const LONGLONG id_ = pTrack->GetNumber();
    const ULONG id = static_cast<ULONG>(id_);

    typedef streams_t::iterator iter_t;
    typedef std::pair<iter_t, bool> status_t;

    const status_t status = m_streams.insert(std::make_pair(id, pStream));
    assert(status.second);  //new insertion
    assert(status.first->first == id);
    assert(status.first->second == pStream);

    pStream->Select();

    HRESULT hr = m_pEvents->QueueEventParamUnk(
                    MENewStream,
                    GUID_NULL,
                    S_OK,
                    pStream);

    assert(SUCCEEDED(hr));

    return S_OK;
}
#else
HRESULT WebmMfSource::NewStream(
    IMFStreamDescriptor* pSD,
    const mkvparser::Track* pTrack,
    WebmMfStream*& pStream)
{
    assert(pSD);
    assert(pTrack);

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
    }

    typedef streams_t::iterator iter_t;
    typedef std::pair<iter_t, bool> status_t;

    DWORD id;

    HRESULT hr = pSD->GetStreamIdentifier(&id);
    assert(SUCCEEDED(hr));
    assert(id == pTrack->GetNumber());

    const status_t status = m_streams.insert(std::make_pair(id, pStream));
    assert(status.second);  //new insertion
    assert(status.first->first == id);
    assert(status.first->second == pStream);

    hr = m_pEvents->QueueEventParamUnk(
            MENewStream,
            GUID_NULL,
            S_OK,
            pStream);

    assert(SUCCEEDED(hr));

    return S_OK;
}
#endif


#if 0 //TODO: restore this
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

    {
        iter_t iter = m_streams.begin();
        const iter_t iter_end = m_streams.end();

        while (iter != iter_end)
        {
            const streams_t::value_type& value = *iter++;

            WebmMfStream* const pStream = value.second;
            assert(pStream);

            if (!pStream->IsSelected())
                continue;

            already_selected.insert(value);
        }
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

        const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(id);
        assert(pTrack);
        assert(pTrack->GetNumber() == id);

        const iter_t iter = already_selected.find(id);

        if (iter != already_selected.end())  //already selected
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

        iter_t iter = newly_selected.begin();
        const iter_t iter_end = newly_selected.end();

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

    iter_t iter = already_selected.begin();
    const iter_t iter_end = already_selected.end();

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
#else
UINT64 WebmMfSource::GetActualStartTime(IMFPresentationDescriptor* pDesc) const
{
    streams_t already_selected;

    typedef streams_t::const_iterator iter_t;

    {
        iter_t iter = m_streams.begin();
        const iter_t iter_end = m_streams.end();

        while (iter != iter_end)
        {
            const streams_t::value_type& value = *iter++;

            WebmMfStream* const pStream = value.second;
            assert(pStream);

            if (!pStream->IsSelected())
                continue;

            already_selected.insert(value);
        }
    }

    const mkvparser::Tracks* const pTracks = m_pSegment->GetTracks();
    assert(pTracks);

    streams_t newly_selected;

    DWORD count;

    HRESULT hr = pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));
    assert(count > 0);

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

        const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(id);
        assert(pTrack);
        assert(pTrack->GetNumber() == id);

        const iter_t iter = already_selected.find(id);

        if (iter != already_selected.end())  //already selected
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

        iter_t iter = newly_selected.begin();
        const iter_t iter_end = newly_selected.end();

        LONGLONG min_time = -1;

        while (iter != iter_end)
        {
            const streams_t::value_type& value = *iter++;

            WebmMfStream* const pStream = value.second;
            assert(pStream);

            LONGLONG curr_time;

            hr = pStream->GetCurrMediaTime(curr_time);
            assert(SUCCEEDED(hr));
            assert(curr_time >= 0);

            if ((min_time < 0) || (curr_time < min_time))
                min_time = curr_time;
        }

        assert(min_time >= 0);
        return min_time;
    }

    //Weird: none of the streams that were selected in this start request
    //had been selected in the previous start request.  It's hard to know
    //what "re-start from current position" even means in this case, but
    //whatever.  Let's just use the smallest time from what was selected
    //previously.

    iter_t iter = already_selected.begin();
    const iter_t iter_end = already_selected.end();

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

        if ((min_time < 0) || (curr_time < min_time))
            min_time = curr_time;
    }

    assert(min_time >= 0);
    return min_time;
}
#endif  //TODO


#if 0
void WebmMfSource::Seek(
    const PROPVARIANT& var,
    bool bStart) //true=start false=seek
{
    assert(var.vt == VT_I8);

    const ULONG ctx = m_context_key;

    const LONGLONG reftime = var.hVal.QuadPart;
    assert(reftime >= 0);

#ifdef _DEBUG
    wodbgstream os;
    os << "WebmMfSource::Seek: reftime=" << reftime
       << " secs=" << (double(reftime) / 10000000)
       << " ctx=" << ctx
       << endl;
#endif

    const LONGLONG time_ns = reftime * 100;
    m_preroll_ns = time_ns;

    struct VideoStream
    {
        WebmMfStreamVideo* pStream;
        WebmMfStream::SeekInfo info;
    };

    typedef std::vector<VideoStream> vs_t;

    vs_t vs;
    vs.reserve(m_streams.size());

    LONG base = -1;

    typedef std::vector<WebmMfStreamAudio*> as_t;
    as_t as;

    typedef streams_t::iterator iter_t;

    iter_t iter = m_streams.begin();
    const iter_t iter_end = m_streams.end();

    while (iter != iter_end)
    {
        const streams_t::value_type& value = *iter++;

        WebmMfStream* const pStream = value.second;
        assert(pStream);

        if (pStream->m_context_key != ctx)
            continue;

        if (!pStream->IsSelected())
            continue;

        const mkvparser::Track* const pTrack = pStream->m_pTrack;
        const LONGLONG type = pTrack->GetType();

        if (type == 2)  //audio
        {
            typedef WebmMfStreamAudio AS;
            AS* const s = static_cast<AS*>(pStream);

            as.push_back(s);
            continue;
        }

        assert(type == 1);  //video

        const vs_t::size_type idx = vs.size();

        vs.push_back(VideoStream());

        VideoStream& s = vs.back();
        WebmMfStream::SeekInfo& i = s.info;

        s.pStream = static_cast<WebmMfStreamVideo*>(pStream);
        s.pStream->GetSeekInfo(time_ns, i);

        if ((i.pBE == 0) || i.pBE->EOS())
            continue;

        if (base < 0)
            base = static_cast<LONG>(idx);
        else
        {
            const mkvparser::Cluster* const pCluster = i.pBE->GetCluster();
            assert(pCluster);
            assert(!pCluster->EOS());

            const WebmMfStreamVideo::SeekInfo& info = vs[base].info;
            const mkvparser::Cluster* const pBase = info.pBE->GetCluster();

            if (pCluster->GetTime() < pBase->GetTime())
                base = static_cast<LONG>(idx);
        }
    }

    const vs_t::size_type nvs = vs.size();

    for (vs_t::size_type idx = 0; idx < nvs; ++idx)
    {
        const VideoStream& s = vs[idx];
        assert(s.pStream->IsSelected());

        s.pStream->Seek(var, s.info, bStart);
    }

    const mkvparser::Cluster* pBaseCluster;

    if (base >= 0)  //have video
        pBaseCluster = vs[base].info.pBE->GetCluster();
    else  //no video stream(s)
    {
        //TODO: we can do better here, by trying to see if
        //the audio streams have cue points of their own.

        assert(m_pSegment->GetCount() >= 1);

        pBaseCluster = m_pSegment->FindCluster(time_ns);
        assert(pBaseCluster);
    }

    const as_t::size_type nas = as.size();

    for (as_t::size_type idx = 0; idx < nas; ++idx)
    {
        WebmMfStreamAudio* const s = as[idx];
        assert(s->IsSelected());

        WebmMfStream::SeekInfo i;

        i.pBE = pBaseCluster->GetEntry(s->m_pTrack);
        i.pCP = 0;
        i.pTP = 0;

        s->Seek(var, i, bStart);
    }
}
#endif


#if 0
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

        if (pStream->IsSelected())
            ++m_cEOS;  //to send event when all streams send EOS
    }

    //m_file.Clear();
    Seek(var, true);  //start

    return S_OK;
}


HRESULT WebmMfSource::SeekStreams(const PROPVARIANT& var)
{
    //m_file.Clear();
    Seek(var, false);  //seek

    return S_OK;
}
#endif


#if 0
HRESULT WebmMfSource::RestartStreams()
{
    //const ULONG ctx = m_context_key;

    typedef streams_t::iterator iter_t;

    iter_t iter = m_streams.begin();
    const iter_t iter_end = m_streams.end();

    while (iter != iter_end)
    {
        const streams_t::value_type& value = *iter++;

        WebmMfStream* const pStream = value.second;
        assert(pStream);

        //if (pStream->m_cRef == 0)
        //    continue;
        //
        //if (pStream->m_context_key != ctx)
        //    continue;

        const HRESULT hr = pStream->Restart();
        assert(SUCCEEDED(hr));
    }

    return S_OK;
}
#endif


#if 0  //TODO
void WebmMfSource::NotifyEOS()
{
    assert(m_cEOS > 0);
    --m_cEOS;

    if (m_cEOS > 0)
        return;

    if (m_pEvents == 0)  //weird
        return;

    const HRESULT hr = m_pEvents->QueueEventParamVar(
                        MEEndOfPresentation,
                        GUID_NULL,
                        S_OK,
                        0);

    assert(SUCCEEDED(hr));
}
#elif 0
void WebmMfSource::NotifyEOS(WebmMfStream* pStream)
{
    assert(pStream);
    assert(pStream->m_cRef > 0);

    if (pStream->m_context_key < m_context_key)  //stale
        return;

    //TODO

    //Start
    //   all streams are new
    //   all (selected) streams go in eos map
    //
    //Seek
    //   (don't know yet)
    //
    //Restart
    //   only streams that are selected are active again
    //   we can even have newly-created streams
    //   streams that previously existed, but that aren't selected,
    //     don't count towards the eos count so they need to be removed
    //     from the eos map
    //   stream that are newly-created are (asummed to be?) selected,
    //     so they must get added to the eos map

    const eos_map_t::iterator eos_map_iter = m_eos_map.find(m_context_key);

    if (eos_map_iter == m_eos_map.end())  //not found (weird)
        return;

    eos_map_t::value_type& v = *eos_map_iter;
    stream_keys_t& keys = v.second;

    const ULONG key = pStream->m_stream_key;

    typedef stream_keys_t::iterator iter_t;

    const iter_t i = keys.begin();
    const iter_t j = keys.end();

    const iter_t k = std::find(i, j, key);

    if (k == j)  //key not found (weird)
        return;

    keys.erase(k);

    if (!keys.empty())
        return;  //wait for other streams to reach EOS

    m_eos_map.erase(eos_map_iter);

    if (m_pEvents == 0)  //weird
        return;

    const HRESULT hr = m_pEvents->QueueEventParamVar(
                        MEEndOfPresentation,
                        GUID_NULL,
                        S_OK,
                        0);
    hr;
    assert(SUCCEEDED(hr));
}
#else
void WebmMfSource::NotifyEOS()
{
    if (m_pEvents == 0)  //weird
        return;

    //TODO: send end-of-presentation when all selected streams have reached eos

#if 0  //TODO: restore this:

    const HRESULT hr = m_pEvents->QueueEventParamVar(
                        MEEndOfPresentation,
                        GUID_NULL,
                        S_OK,
                        0);

    assert(SUCCEEDED(hr));

#endif
}
#endif  //TODO: restore this


WebmMfSource::CAsyncRead::CAsyncRead(WebmMfSource* p) : m_pSource(p)
{
}


WebmMfSource::CAsyncRead::~CAsyncRead()
{
}


HRESULT WebmMfSource::CAsyncRead::QueryInterface(
    const IID& iid,
    void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == __uuidof(IUnknown))
    {
        pUnk = static_cast<IMFAsyncCallback*>(this);  //must be nondelegating
    }
    else if (iid == __uuidof(IMFAsyncCallback))
    {
        pUnk = static_cast<IMFAsyncCallback*>(this);
    }
    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG WebmMfSource::CAsyncRead::AddRef()
{
    return m_pSource->AddRef();
}


ULONG WebmMfSource::CAsyncRead::Release()
{
    return m_pSource->Release();
}


HRESULT WebmMfSource::CAsyncRead::GetParameters(
    DWORD*,
    DWORD*)
{
    return E_NOTIMPL;  //means "assume default behavior"
}


HRESULT WebmMfSource::CAsyncRead::Invoke(IMFAsyncResult* pResult)
{
    if (pResult == 0)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(m_pSource);

    if (FAILED(hr))
        return hr;

    MkvReader& f = m_pSource->m_file;

    m_hrStatus = f.AsyncReadCompletion(pResult);

    const BOOL b = SetEvent(m_pSource->m_hAsyncRead);
    assert(b);

    return S_OK;
}


unsigned WebmMfSource::ThreadProc(void* pv)
{
    WebmMfSource* const pSource = static_cast<WebmMfSource*>(pv);
    assert(pSource);

    return pSource->Main();
}


unsigned WebmMfSource::Main()
{
#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::Main(begin): threadid=0x"
       << hex
       << GetCurrentThreadId()
       << endl;
#endif

    for (;;)
    {
        const bool done = (this->*m_thread_state)();

        if (done)
            break;
    }

#ifdef _DEBUG
    os << "WebmMfSource::Main(end): threadid=0x"
       << hex
       << GetCurrentThreadId()
       << endl;
#endif

    return 0;
}


bool WebmMfSource::StateAsyncRead()
{
    enum { nh = 2 };
    const HANDLE ah[nh] = { m_hQuit, m_hAsyncRead };

    for (;;)
    {
        const DWORD dw = WaitForMultipleObjects(nh, ah, FALSE, INFINITE);

        if (dw == WAIT_FAILED)
            return true;  //TODO: signal error to pipeline

        assert(dw >= WAIT_OBJECT_0);
        assert(dw < (WAIT_OBJECT_0 + nh));

        if (dw == WAIT_OBJECT_0) //hQuit
            return true;

        assert(dw == (WAIT_OBJECT_0 + 1));  //hAsyncRead

        if (thread_state_t s = OnAsyncRead())
        {
            m_thread_state = s;
            return false;
        }
    }
}


bool WebmMfSource::StateRequestSample()
{
    enum { nh = 2 };
    const HANDLE ah[nh] = { m_hQuit, m_hRequestSample };

    for (;;)
    {
        const DWORD dw = WaitForMultipleObjects(nh, ah, FALSE, INFINITE);

        if (dw == WAIT_FAILED)  //weird
            return true;  //TODO: signal error to pipeline

        assert(dw >= WAIT_OBJECT_0);
        assert(dw < (WAIT_OBJECT_0 + nh));

        if (dw == WAIT_OBJECT_0) //hQuit
            return true;

        assert(dw == (WAIT_OBJECT_0 + 1));  //hRequestSample

        if (thread_state_t s = OnRequestSample())
        {
            m_thread_state = s;
            return false;
        }
    }
}


bool WebmMfSource::StateQuit()
{
    return true;  //done
}


HRESULT WebmMfSource::RequestSample(
    WebmMfStream* pStream,
    IUnknown* pToken)
{
    const Request r = { pStream, pToken };

    m_requests.push_back(r);

    if (pToken)
        pToken->AddRef();

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return S_OK;
}


WebmMfSource::thread_state_t WebmMfSource::OnRequestSample()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return &WebmMfSource::StateQuit;

    if (m_pEvents == 0)  //shutdown
        return &WebmMfSource::StateQuit;

    while (!m_requests.empty())
    {
        Request& r = m_requests.front();

        //TODO: check whether stream still selected?

        if (r.pStream->IsEOS())
        {
            if (r.pToken)
            {
                r.pToken->Release();
                r.pToken = 0;
            }

            WebmMfStream* const pStream = r.pStream;
            assert(pStream);

            const HRESULT hr = pStream->SetEOS();
            assert(SUCCEEDED(hr));  //TODO

            m_requests.pop_front();

            NotifyEOS();
            continue;
        }

        if (!r.pStream->IsCurrBlockLocked())
        {
            using mkvparser::BlockEntry;

            const BlockEntry* const pBlock = r.pStream->GetCurrBlock();
            assert(pBlock);
            assert(!pBlock->EOS());

            m_pCurr = pBlock->GetCluster();
            assert(m_pCurr);
            assert(!m_pCurr->EOS());

            const LONGLONG pos = m_pCurr->m_pos;
            assert(pos >= 0);

            const LONGLONG size_ = m_pCurr->m_size;
            assert(size_ > 0);
            assert(size_ <= LONG_MAX);

            const LONG len = static_cast<LONG>(size_);

            const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);
            assert(SUCCEEDED(hr));  //TODO

            if (hr == S_FALSE)  //async read in progress
            {
                m_async_state = &WebmMfSource::StateAsyncLockCurr;
                return &WebmMfSource::StateAsyncRead;
            }

            const int status = r.pStream->LockCurrBlock();
            status;
            assert(status == 0);
            assert(r.pStream->IsCurrBlockLocked());
        }

        hr = r.pStream->GetSample(r.pToken);

        if (SUCCEEDED(hr))
        {
            if (r.pToken)
            {
                r.pToken->Release();
                r.pToken = 0;
            }

            m_requests.pop_front();
            continue;
        }

        //TODO:
        //if (hr != VFW_E_BUFFER_UNDERFLOW)
        //  signal error
        //for now:
        assert(hr == VFW_E_BUFFER_UNDERFLOW);

        m_file.ResetAvailable();

        const mkvparser::BlockEntry* const pBlock = r.pStream->GetCurrBlock();
        assert(pBlock);
        assert(!pBlock->EOS());

        m_pCurr = pBlock->GetCluster();
        assert(m_pCurr);
        assert(!m_pCurr->EOS());

        m_async_state = &WebmMfSource::StateAsyncParseNext;
        m_async_read.m_hrStatus = S_OK;

        const BOOL b = SetEvent(m_hAsyncRead);
        assert(b);

        return &WebmMfSource::StateAsyncRead;
    }

    PurgeCache();

    if (m_commands.size() > 1)
        return OnCommand();

    return 0;  //wait for another request
}


void WebmMfSource::PurgeCache()
{
    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    using namespace mkvparser;

    LONGLONG pos = -1;

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        const WebmMfStream* const pStream = v.second;
        assert(pStream);

        if (!pStream->IsSelected())
        {
            assert(!pStream->IsCurrBlockLocked());
            continue;
        }

        const BlockEntry* const pCurrBlock = pStream->GetCurrBlock();

        if (pCurrBlock == 0)
            continue;

        if (pCurrBlock->EOS())
            continue;

        const Cluster* const pCurr = pCurrBlock->GetCluster();
        assert(pCurr);
        assert(!pCurr->EOS());

        LONGLONG curr_pos = pCurr->m_pos;

        if (curr_pos < 0)
            curr_pos *= -1;

        if ((pos < 0) || (curr_pos < pos))
            pos = curr_pos;
    }

    if (pos >= 0)
        m_file.Purge(pos);
}


WebmMfSource::thread_state_t WebmMfSource::OnCommand()
{
    //There should be no (sample) requests pending.
    //No async reads should be pending.
    //There should be at least one command in the queue.

    commands_t& cc = m_commands;

    typedef commands_t::iterator iter_t;

    while (cc.size() > 1)
    {
        const iter_t prev = cc.begin();
        const iter_t curr = ++iter_t(prev);

        Command& c = *curr;

        switch (c.m_kind)
        {
            case Command::kStart:
            {
                assert(prev->m_kind == Command::kStop);

                const bool bDone = c.OnStart(this);

                if (!bDone)  //don't pop command queue yet
                    return &WebmMfSource::StateAsyncRead;

                break;  //pop front and continue
            }

#if 0  //TODO
            case Command::kSeek:
                c.OnSeek();
                break;
#endif

            case Command::kRestart:  //un-pause
                c.OnRestart(this);
                break;

            case Command::kPause:
                c.OnPause(this);
                break;

            case Command::kStop:
                c.OnStop(this);
                break;

            default:
                assert(false);
                return &WebmMfSource::StateQuit;
        }

        cc.pop_front();
    }

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return &WebmMfSource::StateRequestSample;
}


WebmMfSource::Command::Command(Kind k) :
    m_kind(k),
    m_pDesc(0),
    m_state(0)
{
    PropVariantInit(&m_time);
}

WebmMfSource::Command::Command(const Command& rhs) :
    m_kind(rhs.m_kind),
    m_pDesc(rhs.m_pDesc),
    m_state(rhs.m_state)
{
    if (m_pDesc)
        m_pDesc->AddRef();

    m_time = rhs.m_time;
}


WebmMfSource::Command::~Command()
{
    if (m_pDesc)
        m_pDesc->Release();

    PropVariantClear(&m_time);
}


void WebmMfSource::Command::SetDesc(IMFPresentationDescriptor* pDesc)
{
    if (pDesc)
    {
        pDesc->AddRef();

        if (m_pDesc)
            m_pDesc->Release();

        m_pDesc = pDesc;
    }
    else if (m_pDesc)
    {
        m_pDesc->Release();
        m_pDesc = 0;
    }
}


void WebmMfSource::Command::SetTime(const PROPVARIANT& time)
{
    PropVariantClear(&m_time);
    m_time = time;
}


bool WebmMfSource::Command::OnStart(WebmMfSource* pSource)
{
    assert(pSource);
    assert(m_kind == kStart);
    assert(m_pDesc);

    if (m_state)
        return (this->*m_state)(pSource);

    //initialization step

    LONGLONG& reftime = m_time.hVal.QuadPart;

    if (m_time.vt == VT_EMPTY)
    {
        m_time.vt = VT_I8;
        reftime = 0;
    }
    else
    {
        assert(m_time.vt == VT_I8);

        if (reftime < 0)
            reftime = 0;
    }

    const LONGLONG time_ns = reftime * 100;

    pSource->m_preroll_ns = time_ns;  //TODO: better way to handle this?

    mkvparser::Segment* const pSegment = pSource->m_pSegment;

    const mkvparser::Tracks* const pTracks = pSegment->GetTracks();
    assert(pTracks);

    DWORD count;

    HRESULT hr = m_pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));
    assert(count > 0);

    const mkvparser::Cues* const pCues = pSegment->GetCues();

    LONGLONG base_pos = -2;

    for (DWORD index = 0; index < count; ++index)
    {
        BOOL bSelected;
        IMFStreamDescriptorPtr pSD;

        hr = m_pDesc->GetStreamDescriptorByIndex(index, &bSelected, &pSD);
        assert(SUCCEEDED(hr));
        assert(pSD);

        if (!bSelected)
            continue;

        DWORD id;  //MKV track number

        hr = pSD->GetStreamIdentifier(&id);
        assert(SUCCEEDED(hr));

        const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(id);
        assert(pTrack);
        assert(pTrack->GetNumber() == id);

        if (pTrack->GetType() != 1)  //not video
            continue;

        if (base_pos == -2)
            base_pos = -1;  // base_pos >= -1 means we have video

        if (pCues)
        {
            const mkvparser::CuePoint* pCP;
            const mkvparser::CuePoint::TrackPosition* pTP;

            const bool bFound = pCues->Find(time_ns, pTrack, pCP, pTP);

            if (bFound)
            {
                assert(pCP);
                assert(pTP);
                assert(pTP->m_pos >= 0);

                if ((base_pos < 0) || (pTP->m_pos < base_pos))
                    base_pos = pTP->m_pos;

                continue;
            }
        }
    }

    const mkvparser::Cluster*& pCurr = pSource->m_pCurr;

    if (base_pos < 0)  //no video, or no cue points
    {
        //we don't have anything but a time
        //without cue points we cannot jump to anywhere in the file,
        //  so where are forced to search among already-loaded clusters
        //  that's exactly what segment::findcluster does for us
        //
        //we don't necessarily have a guarantee that we even have a cluster
        //  unless we make that guarantee.  I suppose we have that
        //  with AsyncReadInitStreams.

        pCurr = pSegment->FindCluster(time_ns);
        assert(pCurr);
        assert(!pCurr->EOS());     //because of AsyncReadInitStreams
        assert(pCurr->m_pos > 0);  //because we always partially load
        assert(pCurr->m_size > 0);
        assert(pCurr->GetTime() <= time_ns);

        //we have the cluster object, that we can guarantee is at least
        //partially loaded
        //we need to load it into the cache
    }
    else
    {
        //have have a cue point, so we have a cluster pos
        //we must load that cluster in the cache
        //but that's only the base cluster -- we must still search
        //the subsequent clusters
        //  as req'd for each stream, so that each stream has a pCurr

        //Segment::GetBlock will search for this pos among existing cluster
        //objects
        //(either loaded or pre-loaded), and if not found then it will
        //preload the cluster.
        //normally this wouldn't be a problem but we require that the cluster
        //be at least
        //partially loaded (so have a non-negative pos, len, and timecode).

        pCurr = pSegment->FindOrPreloadCluster(base_pos);
        assert(pCurr);
        assert(!pCurr->EOS());
    }

    pSource->m_file.ResetAvailable();

    m_state = &Command::StateStartInitStreams;
    m_index = 0;

    pSource->m_async_state = &WebmMfSource::StateAsyncLoadCurr;
    pSource->m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(pSource->m_hAsyncRead);
    assert(b);

    return false;
}


bool WebmMfSource::Command::StateStartInitStreams(WebmMfSource* pSource)
{
    //we have loaded m_pCurr (the base cluster for this seek) in the cache

    assert(m_time.vt == VT_I8);

    const LONGLONG reftime = m_time.hVal.QuadPart;
    assert(reftime >= 0);

    const LONGLONG time_ns = reftime * 100;

    const mkvparser::Cluster*& pCurr = pSource->m_pCurr;
    assert(pCurr);
    assert(!pCurr->EOS());
    assert(pCurr->m_pos >= 0);
    assert(pCurr->m_size >= 0);
    assert(pCurr->GetTime() <= time_ns);

    mkvparser::Segment* const pSegment = pSource->m_pSegment;

    const mkvparser::Tracks* const pTracks = pSegment->GetTracks();
    assert(pTracks);

    DWORD count;

    HRESULT hr = m_pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));
    assert(count > 0);
    //assert(m_index < count);

    typedef streams_t::const_iterator iter_t;

    const streams_t& streams = pSource->m_streams;
    const iter_t iter_end = streams.end();

    //TODO: InitStreams will need to called multiple times,
    //in order to find the cluster containing the pCurr block for this track
    for (m_index = 0; m_index < count; ++m_index)
    {
        BOOL bSelected;
        IMFStreamDescriptorPtr pSD;

        hr = m_pDesc->GetStreamDescriptorByIndex(m_index, &bSelected, &pSD);
        assert(SUCCEEDED(hr));
        assert(pSD);

        //When we're born, we're in a stopped state, but not stream instances
        //have been created yet, so a stream isn't selected then there's
        //nothing we need to do here.
        //
        //After we have been started and then stopped, stream instances have
        //been created, but are stopped when we handled the stop command.
        //During stop we automatically deselect the streams, so again there's
        //nothing we need to here to handle an stream that is not selected
        //for this start request.

        if (!bSelected)
            continue;

        DWORD id;  //MKV track number

        hr = pSD->GetStreamIdentifier(&id);
        assert(SUCCEEDED(hr));

        const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(id);
        assert(pTrack);
        assert(pTrack->GetNumber() == id);

        //We need to set the pCurr of the stream
        //the stream object has not been created yet
        //we have a potential cluster: we need to search it for a block entry
        //for this track.  Once we have the cluster, we can create the stream
        //object and then move onto the next stream.

        //TODO:
        //if this is video, and we have cues, get the cue point
        //(this is a bit inefficient, but it's good enough for now)
        //for now just do a linear search

        //TODO:
        //this is wrong, because we have no guarantee that we have a
        //matching track in this cluster, or a keyframe.

        using mkvparser::BlockEntry;

        const BlockEntry* const pBlock = pCurr->GetEntry(pTrack, time_ns);
        assert(pBlock);           //TODO:
        assert(!pBlock->EOS());   //TODO
        assert((pTrack->GetType() == 2) || pBlock->GetBlock()->IsKey());

        const iter_t iter = streams.find(id);

        if (iter == iter_end)  //stream does not already exist
        {
            WebmMfStream* pStream;

            hr = pSource->NewStream(pSD, pTrack, pStream);
            assert(SUCCEEDED(hr));
            assert(pStream);

            pStream->SetCurrBlock(pBlock);
        }
        else
        {
            WebmMfStream* const pStream = iter->second;
            assert(pStream);

            hr = pStream->Update();
            assert(SUCCEEDED(hr));

            pStream->SetCurrBlock(pBlock);
        }
    }

    hr = pSource->QueueEvent(MESourceStarted, GUID_NULL, S_OK, &m_time);
    assert(SUCCEEDED(hr));

    iter_t iter = streams.begin();

    while (iter != iter_end)
    {
        const streams_t::value_type& v = *iter++;

        WebmMfStream* const s = v.second;
        assert(s);

        hr = s->Start(m_time);
        assert(SUCCEEDED(hr));
    }

    return true;   //done with start request
}


void WebmMfSource::Command::OnStop(WebmMfSource* pSource)
{
    streams_t& ss = pSource->m_streams;

    typedef streams_t::const_iterator iter_t;

    iter_t i = ss.begin();
    const iter_t j = ss.end();

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        WebmMfStream* const s = v.second;
        assert(s);

        const HRESULT hr = s->Stop();
        assert(SUCCEEDED(hr));
    }

    const HRESULT hr = pSource->QueueEvent(
                        MESourceStopped,
                        GUID_NULL,
                        S_OK,
                        0);

    assert(SUCCEEDED(hr));
}


void WebmMfSource::Command::OnPause(WebmMfSource* pSource)
{
    streams_t& ss = pSource->m_streams;

    typedef streams_t::const_iterator iter_t;

    iter_t i = ss.begin();
    const iter_t j = ss.end();

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        WebmMfStream* const s = v.second;
        assert(s);

        const HRESULT hr = s->Pause();
        assert(SUCCEEDED(hr));
    }

    const HRESULT hr = pSource->QueueEvent(MESourcePaused, GUID_NULL, S_OK, 0);
    assert(SUCCEEDED(hr));
}


void WebmMfSource::Command::OnRestart(WebmMfSource* pSource)  //unpause
{
    assert(pSource);
    assert(m_pDesc);

    mkvparser::Segment* const pSegment = pSource->m_pSegment;

    const mkvparser::Tracks* const pTracks = pSegment->GetTracks();
    assert(pTracks);

    //    if this is a restart, set the MF_EVENT_SOURCE_ACTUAL_START,
    //      with the pos as the propvar value.

    //if VT_EMPTY then this is a re-start
    //we need to figure out what the re-start time is
    //first determine whether there were streams selected previously
    //  if they're still selected, then synthesize the re-start time
    //  from one of them

    const UINT64 reftime = pSource->GetActualStartTime(m_pDesc);

    DWORD count;

    HRESULT hr = m_pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));

    typedef streams_t::iterator iter_t;

    streams_t& streams = pSource->m_streams;
    const iter_t iter_end = streams.end();

    for (DWORD index = 0; index < count; ++index)
    {
        BOOL bSelected;
        IMFStreamDescriptorPtr pSD;

        hr = m_pDesc->GetStreamDescriptorByIndex(index, &bSelected, &pSD);
        assert(SUCCEEDED(hr));
        assert(pSD);

        DWORD id;  //MKV track number

        hr = pSD->GetStreamIdentifier(&id);
        assert(SUCCEEDED(hr));

        const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(id);
        assert(pTrack);
        assert(pTrack->GetNumber() == id);

        const iter_t iter = streams.find(id);

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

        //stream selected in this restart

        if (iter != iter_end)  //stream exists already
        {
            WebmMfStream* const pStream = iter->second;
            assert(pStream);
            assert(pStream->m_pTrack == pTrack);

            //TODO: it's not clear whether we need to send
            //this if the stream was already selected.

            hr = pStream->Update();
            assert(SUCCEEDED(hr));

            continue;
        }

        //stream selected, but doesn't exist yet

        const LONGLONG time_ns = reftime * 100LL;  //reftime to ns

        using mkvparser::Cluster;
        using mkvparser::BlockEntry;

        const Cluster* const pCurr = pSegment->FindCluster(time_ns);
        assert(pCurr);
        assert(!pCurr->EOS());
        assert(pCurr->m_pos > 0);
        assert(pCurr->m_size > 0);
        assert(pCurr->GetTime() <= time_ns);

        const BlockEntry* const pBlock = pCurr->GetEntry(pTrack, time_ns);
        assert(pBlock);           //TODO:
        assert(!pBlock->EOS());   //TODO
        assert((pTrack->GetType() == 2) || pBlock->GetBlock()->IsKey());

        WebmMfStream* pStream;

        hr = pSource->NewStream(pSD, pTrack, pStream);
        assert(SUCCEEDED(hr));
        assert(pStream);

        pStream->SetCurrBlock(pBlock);
    }

    IMFMediaEventPtr pEvent;

    PROPVARIANT var;
    PropVariantInit(&var);

    var.vt = VT_EMPTY;  //restarts always report VT_EMPTY

    hr = MFCreateMediaEvent(
            MESourceStarted,
            GUID_NULL,
            S_OK,
            &var,
            &pEvent);

    assert(SUCCEEDED(hr));
    assert(pEvent);

    hr = pEvent->SetUINT64(MF_EVENT_SOURCE_ACTUAL_START, reftime);
    assert(SUCCEEDED(hr));

    hr = pSource->m_pEvents->QueueEvent(pEvent);
    assert(SUCCEEDED(hr));

#if 0
    hr = pSource->RestartStreams();
    assert(SUCCEEDED(hr));
#else
    iter_t iter = streams.begin();

    while (iter != iter_end)
    {
        const streams_t::value_type& value = *iter++;

        WebmMfStream* const pStream = value.second;
        assert(pStream);

        hr = pStream->Restart();
        assert(SUCCEEDED(hr));
    }
#endif
}


WebmMfSource::thread_state_t WebmMfSource::OnAsyncRead()
{
    //Invoke was called, which called AsyncReadCompletion to end
    //the async read.  Invoke then signalled our event.

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return &WebmMfSource::StateQuit;

    //TODO:
    //if (m_pEvents == 0)  //shutdown
    //    return &WebmMfSource::StateQuit;

    hr = m_async_read.m_hrStatus;  //assigned a value in Invoke

    if (FAILED(hr))
        return &WebmMfSource::StateQuit;  //TODO: announce error

    if (hr == S_FALSE)  //not all bytes requested are in cache yet
    {
        hr = m_file.AsyncReadContinue(&m_async_read);

        if (FAILED(hr))
            return &WebmMfSource::StateQuit;  //TODO

        return 0;  //wait for async read to complete
    }

    //All bytes requested are now in the cache.

    return (this->*m_async_state)();
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncLockCurr()
{
    //We get here after all requested bytes have been loaded into the cache.

    const requests_t& rr = m_requests;

    if (rr.empty())  //should never happen
        return &WebmMfSource::StateQuit;

    const Request& r = rr.front();
    assert(r.pStream);

    const int status = r.pStream->LockCurrBlock();
    status;
    assert(status == 0);

    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return &WebmMfSource::StateRequestSample;
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncParseNext()
{
    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = m_pSegment->ParseNext(m_pCurr, m_pNext, pos, len);

        if (status >= 0)  //have next cluster, or EOF
            return StateAsyncLoadNext();

        assert(status == mkvparser::E_BUFFER_NOT_FULL);  //TODO

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
            return &WebmMfSource::StateQuit;  //TODO

        if (hr == S_FALSE)
        {
            m_async_state = &WebmMfSource::StateAsyncParseNext;
            return 0;
        }
    }
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncLoadNext()
{
    //We have the next cluster, or we've reached EOF.
    //Next we must populate the cache with the cluster's payload.

    if (m_pNext)
    {
        //we have next cluster, now load it into cache

        LONGLONG pos = m_pNext->m_pos;  //relative to segment
        assert(pos >= 0);

        const LONGLONG len_ = m_pNext->m_size;
        assert(len_ > 0);
        assert(len_ <= LONG_MAX);

        const LONG len = static_cast<LONG>(len_);

        pos += m_pSegment->m_start;  //absolute pos

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
            return &WebmMfSource::StateQuit;  //TODO

        if (hr == S_FALSE)  //event will be set in Invoke
        {
            m_async_state = &WebmMfSource::StateAsyncLoadNext;
            return 0;
        }

        //hr = S_OK means cluster was already in the cache
    }

    const requests_t& rr = m_requests;

    if (rr.empty())  //should never happen
        return &WebmMfSource::StateQuit;

    const Request& r = rr.front();
    assert(r.pStream);

    HRESULT hr = r.pStream->NotifyNextCluster(m_pNext);

    if (SUCCEEDED(hr))
    {
        const BOOL b = SetEvent(m_hRequestSample);
        assert(b);

        return &WebmMfSource::StateRequestSample;
    }

    assert(hr == VFW_E_BUFFER_UNDERFLOW);  //TODO

    m_pCurr = m_pNext;
    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    m_async_state = &WebmMfSource::StateAsyncParseNext;
    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hAsyncRead);
    assert(b);

    return 0;
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncLoadCurr()
{
    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = m_pCurr->Load(pos, len);

        if (status >= 0)
            break;

        if (status != mkvparser::E_BUFFER_NOT_FULL)  //error
            return &WebmMfSource::StateQuit;  //TODO

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
            return &WebmMfSource::StateQuit;  //TODO

        if (hr == S_FALSE)  //async read in progress
        {
            m_async_state = &WebmMfSource::StateAsyncLoadCurr;
            return 0;
        }
    }

    LONGLONG pos = m_pCurr->m_pos;
    assert(pos > 0);

    pos += m_pSegment->m_start;  //absolute pos

    const LONGLONG len_ = m_pCurr->m_size;
    assert(len_ > 0);
    assert(len_ <= LONG_MAX);

    const LONG len = static_cast<LONG>(len_);

    const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

    if (FAILED(hr))
        return &WebmMfSource::StateQuit;  //TODO

    if (hr == S_FALSE)  //event will be set in Invoke
    {
        m_async_state = &WebmMfSource::StateAsyncLoadCurr;
        return 0;
    }

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return &WebmMfSource::StateRequestSample;
}


}  //end namespace WebmMfSource
