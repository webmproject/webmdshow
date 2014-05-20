#include "omahautil.hpp"
#include "webmmfsource.hpp"
#include "webmmfstream.hpp"
#include "webmmfstreamvideo.hpp"
#include "webmmfstreamaudio.hpp"
#include "webmmfbytestreamhandler.hpp"
#include "webmtypes.hpp"
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
#include <propvarutil.h>
#ifdef _DEBUG
#include <iomanip>
#include "odbgstream.hpp"
#include "iidstr.hpp"
using std::hex;
using std::endl;
using std::boolalpha;
using std::fixed;
using std::setprecision;
#endif

//"D:\src\mediafoundation\topoedit\app\Debug\topoedit.exe"
//"C:\Program Files (x86)\Windows Media Player\wmplayer.exe"

//For debugging, send MEError event:
//http://msdn.microsoft.com/en-us/library/aa380072(VS.85).aspx
//http://msdn.microsoft.com/en-us/library/bb762305(VS.85).aspx
//
//VT_LPWSTR must be allocated with CoTaskMemAlloc
//(Presumably this also applies to VT_LPSTR)
//
//VT_LPWSTR is described as being a string pointer with no
//information on how it is allocated. You might then assume
//that the PROPVARIANT doesn't own the string and just has a
//pointer to it, but you'd be wrong.
//
//In fact, the string stored in a VT_LPWSTR PROPVARIANT must
//be allocated using CoTaskMemAlloc and be freed using CoTaskMemFree.
//
//Evidence for this:
//
//Look at what the inline InitPropVariantFromString function does:
//It sets a VT_LPWSTR using SHStrDupW, which in turn allocates the
//string using CoTaskMemAlloc. Knowing that, it's obvious that
//PropVariantClear is expected to free the string using CoTaskMemFree.
//
//I can't find this explicitly documented anywhere, which is a shame,
//but step through this code in a debugger and you can confirm that
//the string is freed by PropVariantClear:
//
//#include <Propvarutil.h>
//
//int wmain(int argc, TCHAR *lpszArgv[])
//{
//    PROPVARIANT pv;
//    InitPropVariantFromString(L"Moo", &pv);
//    ::PropVariantClear(&pv);
//}
//
//If  you put some other kind of string pointer into
//a VT_LPWSTR PROPVARIANT your program is probably going to crash.

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
    m_hCommand(0),
    m_hThread(0),
    m_pCurr(0),
    m_pNext(0),
    //m_bLive(true),
    m_bCanSeek(false),
    m_load_index(-1)
{
    HRESULT hr = m_pClassFactory->LockServer(TRUE);
    assert(SUCCEEDED(hr));

    hr = CLockable::Init();
    assert(SUCCEEDED(hr));  //TODO

    hr = MFCreateEventQueue(&m_pEvents);
    assert(SUCCEEDED(hr));
    assert(m_pEvents);

    QWORD length;

    hr = pBS->GetLength(&length);
    //MF_E_BYTESTREAM_UNKNOWN_LENGTH

    m_bLive = FAILED(hr);

    m_commands.push_back(Command(Command::kStop, this));

    m_thread_state = &WebmMfSource::StateAsyncRead;

    hr = InitThread();
    assert(SUCCEEDED(hr));

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource: ctor; this=0x"
       << (const void*)this
       << "; bLive="
       << boolalpha
       << m_bLive
       << endl;
#endif
}
#pragma warning(default:4355)


WebmMfSource::~WebmMfSource()
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource: dtor(begin); this=0x" << (const void*)this << endl;
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

    m_file.Clear();

#ifdef _DEBUG
    os << L"WebmMfSource: dtor(end); this=0x" << (const void*)this << endl;
#endif

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

    m_hCommand = CreateEvent(0, 0, 0, 0);

    if (m_hCommand == 0)
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

    if (m_hCommand)
    {
        const BOOL b = CloseHandle(m_hCommand);
        assert(b);

        m_hCommand = 0;
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

    IMFAsyncResultPtr pLoadResult;  //TODO: do we really need this?

    hr = MFCreateAsyncResult(0, pCB, 0, &pLoadResult);

    if (FAILED(hr))
        return hr;

    assert(m_thread_state == &WebmMfSource::StateAsyncRead);

    m_file.ResetAvailable(0);
    m_file.Seek(0);

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

    m_pLoadResult = pLoadResult;  //TODO: do we really local pLoadResult?
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

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return &WebmMfSource::StateRequestSample;
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

    if (h.m_docTypeVersion > 4)
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

    //assume here that we have 1024 bytes available, even if we don't
    //know the exact total yet.

    const LONGLONG result = Segment::CreateInstance(&m_file, pos, m_pSegment);

    if (result != 0)  //TODO: liberalize
        return LoadComplete(E_FAIL);

    //if (m_pSegment->m_size < 0)
    //    m_bLive = true;

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
        {
            Error(L"ParseSegmentHeaders AsyncReadInit failed.", hr);
            return LoadComplete(hr);
        }

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

        wodbgstream os;
        os << L"SegmentInfo.muxingApp=\"" << muxingApp
           << L"\"\nSegmentInfo.writingApp=\"" << writingApp
           << L'\"'
           << endl;

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
            hr = WebmMfStreamVideo::CreateStreamDescriptor(pTrack, pDesc);

#if 1
        else if (type == 2)  //audio
            hr = WebmMfStreamAudio::CreateStreamDescriptor(pTrack, pDesc);
#endif

        else
            continue;  //weird

        if (hr != S_OK)
            continue;

        m_stream_descriptors.push_back(pDesc);

        hr = CreateStream(pDesc, pTrack);
        assert(SUCCEEDED(hr));
    }

    if (m_stream_descriptors.empty())
        return LoadComplete(E_FAIL);

    assert(m_pSegment->GetCount() == 0);

    LONGLONG total, avail;

    const long status = m_file.Length(&total, &avail);

    if (status < 0)  //weird
        return LoadComplete(E_FAIL);

    m_bCanSeek = false;

    if (m_bLive)
    {
        __noop;

#ifdef _DEBUG
        odbgstream os;
        os << "webmmfsource: cannot seek because live" << endl;
#endif
    }
    else if (!HaveVideo())
        __noop;

    //else if (m_file.HasSlowSeek())
    //    __noop;

    //else if (m_file.IsPartiallyDownloaded())
    //    __noop;

    else if (m_pSegment->GetCues())
        m_bCanSeek = true;

    else if (total < 0)  //don't know total file size
        __noop;

    else if (const mkvparser::SeekHead* pSH = m_pSegment->GetSeekHead())
    {
        const int count = pSH->GetCount();

        for (int idx = 0; idx < count; ++idx)
        {
            const mkvparser::SeekHead::Entry* const p = pSH->GetEntry(idx);

            if (p->id == 0x0C53BB6B)  //Cues ID
            {
                const LONGLONG cues_off = p->pos;  //relative to segment
                assert(cues_off >= 0);

                const LONGLONG pos = m_pSegment->m_start + cues_off;

                if (pos >= total)
                    break;  //don't bother trying to parse the cues

                m_file.Seek(pos);

                m_async_state = &WebmMfSource::StateAsyncParseCues;
                m_async_read.m_hrStatus = S_OK;

                const BOOL b = SetEvent(m_hAsyncRead);
                assert(b);

                return 0;
            }
        }
    }

    m_file.EnableBuffering(GetDuration());

    m_async_state = &WebmMfSource::StateAsyncLoadCluster;
    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hAsyncRead);
    assert(b);

    return 0;
}


WebmMfSource::thread_state_t WebmMfSource::StateAsyncParseCues()
{
    //assert(!m_file.HasSlowSeek());
    //assert(!m_file.IsPartiallyDownloaded());  //?
    assert(m_pSegment->GetCues() == 0);

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

        if (status >= 0)
        {
            if (status == 0)  //success
                m_bCanSeek = true;

            m_file.Seek(0);
            m_file.ResetAvailable(0);

            assert(m_pSegment->GetCount() == 0);

            m_file.EnableBuffering(GetDuration());

            m_async_state = &WebmMfSource::StateAsyncLoadCluster;
            m_async_read.m_hrStatus = S_OK;

            const BOOL b = SetEvent(m_hAsyncRead);
            assert(b);

            return 0;  //stay in async read state
        }

        if (status != mkvparser::E_BUFFER_NOT_FULL)
            return LoadComplete(E_FAIL);

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
            return LoadComplete(hr);

        if (hr == S_FALSE)  //async read in progress
            return 0;       //no transition here
    }  //parsing cues element
}


WebmMfSource::thread_state_t WebmMfSource::StateAsyncLoadCluster()
{
#ifdef _DEBUG
    odbgstream os;
    os << "StateAsyncLoadCluster(begin)" << endl;
#endif

    //Ensure that cluster is at least partially loaded,
    //so we know the total size of this element.  (This
    //is necessary because all we know about a preloaded
    //cluster is its position.)

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
            return LoadComplete(hr);

        if (hr == S_FALSE)  //async read in progress
            return 0;
    }

    //We have a new (last) cluster, but it hasn't been loaded into
    //the cache yet, nor have its entries been parsed.

    //We want to get each entry, then test whether it is acceptable
    //as a first block

    const mkvparser::Cluster* const pCluster = m_pSegment->GetLast();
    assert(pCluster);
    assert(!pCluster->EOS());
    assert(pCluster->GetEntryCount() < 0);

    m_load_index = 0;

    m_async_state = &WebmMfSource::StateAsyncInitStreams;
    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hAsyncRead);
    assert(b);

#ifdef _DEBUG
    os << "StateAsyncLoadCluster(end): cluster.pos="
       << pCluster->GetPosition()
       << endl;
#endif

    return 0;
}


WebmMfSource::thread_state_t WebmMfSource::StateAsyncParseCluster()
{
    const mkvparser::Cluster* const pCluster = m_pSegment->GetLast();
    assert(pCluster);
    assert(!pCluster->EOS());

#ifdef _DEBUG
    odbgstream os;
    os << "StateAsyncParseCluster(begin): load_index="
       << m_load_index
       << " cluster.pos=" << pCluster->GetPosition()
       << endl;
#endif

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = pCluster->Parse(pos, len);

        if (status >= 0)  //successfully parsed entry, or no more entries
            break;

        if (status != mkvparser::E_BUFFER_NOT_FULL)
            return LoadComplete(E_FAIL);

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
            return LoadComplete(hr);

        if (hr == S_FALSE)  //async read in progress
            return 0;
    }

    m_async_state = &WebmMfSource::StateAsyncInitStreams;
    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hAsyncRead);
    assert(b);

#ifdef _DEBUG
    os << "StateAsyncParseCluster(end)" << endl;
#endif

    return 0;
}


WebmMfSource::thread_state_t WebmMfSource::StateAsyncInitStreams()
{
    assert(m_pSegment->GetCount() > 0);
    assert(m_load_index >= 0);

    const mkvparser::Cluster* const pCluster = m_pSegment->GetLast();
    assert(pCluster);
    assert(!pCluster->EOS());

#ifdef _DEBUG
    odbgstream os;
    os << "StateAsyncInitStreams(begin): load_index="
       << m_load_index
       << " cluster.pos=" << pCluster->GetPosition()
       << endl;
#endif

    const mkvparser::Tracks* const pTracks = m_pSegment->GetTracks();
    assert(pTracks);

    const ULONG nTracks = pTracks->GetTracksCount();
    assert(nTracks > 0);

    typedef streams_t::const_iterator iter_t;
    const iter_t streams_end = m_streams.end();

    for (;;)
    {
        const mkvparser::BlockEntry* pEntry;

        long status = pCluster->GetEntry(m_load_index, pEntry);

        if (status < 0)  //need to parse this cluster some more
        {
            assert(status == mkvparser::E_BUFFER_NOT_FULL);

            m_async_state = &WebmMfSource::StateAsyncParseCluster;
            m_async_read.m_hrStatus = S_OK;

            const BOOL b = SetEvent(m_hAsyncRead);
            assert(b);

            return 0;
        }

        if (status == 0)  //nothing left on this cluster
        {
            if (m_pSegment->GetCount() >= 10)
                break;

            m_load_index = -1;

            m_async_state = &WebmMfSource::StateAsyncLoadCluster;
            m_async_read.m_hrStatus = S_OK;

            const BOOL b = SetEvent(m_hAsyncRead);
            assert(b);

            return 0;
        }

        assert(status > 0);  //successfully found an entry
        assert(pEntry);
        assert(!pEntry->EOS());

        const mkvparser::Block* const pBlock = pEntry->GetBlock();
        assert(pBlock);

        const LONGLONG tn_ = pBlock->GetTrackNumber();
        assert(tn_ > 0);

        const ULONG tn = static_cast<ULONG>(tn_);

        const iter_t iter = m_streams.find(tn);

        if (iter != streams_end)  //would be weird otherwise
        {
            WebmMfStream* const pStream = iter->second;
            assert(pStream);

            const HRESULT hr = pStream->SetFirstBlock(pEntry);
            assert(SUCCEEDED(hr) || (hr == E_FAIL));

            if (SUCCEEDED(hr) && InitStreamsDone())
                break;
        }

        ++m_load_index;
    }

    //If there were any streams that weren't successfully initialized,
    //then tidy things up by setting the first block to the EOS value.
    //A stream that was already successfully initialized will simply
    //return, so this loop will only have an effect on streams that
    //were not already initialized.
    //
    //Note that we are being liberal here, in the sense that we tried
    //to search multiple clusters to find an entry that was acceptable
    //as a first block.  We could have chosen to handle this case by
    //failing the load, but we decide instead to provide degraded
    //functionality, in the sense that only some of the streams
    //will be rendered.  (Streams whose first block is EOS will send
    //the EOS for that stream immediately, but the presentation itself
    //won't stop until all streams signal EOS.)

    iter_t iter = m_streams.begin();

    while (iter != streams_end)
    {
        const streams_t::value_type& v = *iter++;

        WebmMfStream* const pStream = v.second;
        assert(pStream);

        //Set stream to EOS, if not already initialized.

        const HRESULT hr = pStream->SetFirstBlock(0);
        assert(SUCCEEDED(hr));
        hr;
    }

#ifdef _DEBUG
    os << "StateAsyncInitStreams(end): Load complete" << endl;
#endif

    return LoadComplete(S_OK);
}


bool WebmMfSource::InitStreamsDone() const
{
    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        WebmMfStream* const pStream = v.second;
        assert(pStream);

        if (pStream->GetFirstBlock() == 0)
            return false;
    }

    return true;
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

    if (m_bLive)
    {
        dw = MFMEDIASOURCE_IS_LIVE;
        return S_OK;
    }

    dw = MFMEDIASOURCE_CAN_PAUSE;

    if (m_bCanSeek)
    {
        dw |= MFMEDIASOURCE_CAN_SEEK;

        if (m_file.HasSlowSeek())
            dw |= MFMEDIASOURCE_HAS_SLOW_SEEK;
    }

    return S_OK;
}


bool WebmMfSource::HaveVideo() const
{
    typedef streams_t::const_iterator iter_t;

    iter_t iter = m_streams.begin();
    const iter_t iter_end = m_streams.end();

    while (iter != iter_end)
    {
        const streams_t::value_type& v = *iter++;

        const WebmMfStream* const pStream = v.second;
        assert(pStream);

        const mkvparser::Track* const pTrack = pStream->m_pTrack;
        assert(pTrack);

        if (pTrack->GetType() == 1) //video
            return true;
    }

    return false;
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

    const PROPVARIANT& pos = *pPos;

    switch (pos.vt)
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
    os << L"WebmMfSource::Start: ";

    if (pos.vt == VT_EMPTY)
        os << "time=VT_EMPTY";
    else
    {
        const LONGLONG reftime = pos.hVal.QuadPart;

        os << "time[reftime]="
           << reftime
           << " time[sec]="
           << fixed
           << setprecision(3)
           << (double(reftime) / 10000000.0);
    }

    os << endl;
#endif

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    commands_t& cc = m_commands;

    if (cc.empty())
        return E_FAIL;

    if (cc.back().m_kind == Command::kStop)  //start
        cc.push_back(Command(Command::kStart, this));

    else if (pPos->vt == VT_I8)  //seek
        cc.push_back(Command(Command::kSeek, this));

    else
        cc.push_back(Command(Command::kRestart, this));

    Command& c = cc.back();

    c.SetDesc(pDesc);
    c.SetTime(pos);

    const BOOL b = SetEvent(m_hCommand);
    assert(b);

    return S_OK;
}


HRESULT WebmMfSource::Stop()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfSource::Stop (begin)" << endl;
#endif

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    commands_t& cc = m_commands;
    cc.push_back(Command(Command::kStop, this));

    const BOOL b = SetEvent(m_hCommand);
    assert(b);

    return S_OK;
}


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

    cc.push_back(Command(Command::kPause, this));

    const BOOL b = SetEvent(m_hCommand);
    assert(b);

    return S_OK;
}


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

    hr = m_file.Close();

#if 0
    //TODO: do this in dtor only?
    //The problem is that we haven't addressed the
    //issue of requests that haven't been serviced yet.
    //We do delete them in the dtor, but it might be
    //better to let the worker thread run normally,
    //until the request queue becomes exhausted.

    const BOOL b = SetEvent(m_hQuit);
    assert(b);
#else
    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);
#endif

    return S_OK;
}


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

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pEvents == 0)
        return MF_E_SHUTDOWN;

    if (rate < 0)
        return MF_E_REVERSE_UNSUPPORTED;  //TODO

    if (bThin && !ThinningSupported())
        return MF_E_THINNING_UNSUPPORTED;

    typedef streams_t::iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        WebmMfStream* const pStream = v.second;
        assert(pStream);

        pStream->SetRate(bThin, rate);
    }

    m_bThin = bThin;
    m_rate = rate;

    PROPVARIANT var;

    var.vt = VT_R4;
    var.fltVal = m_rate;

    hr = m_pEvents->QueueEventParamVar(
            MESourceRateChanged,
            GUID_NULL,
            S_OK,
            &var);

    assert(SUCCEEDED(hr));

    return S_OK;
}


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

    if (bThin && !ThinningSupported())
        return MF_E_THINNING_UNSUPPORTED;

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

    if (bThin && !ThinningSupported())
        return MF_E_THINNING_UNSUPPORTED;

    if (pRate == 0)
        return E_POINTER;

    float& r = *pRate;
    r = 128;  //arbitrary

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

    if (bThin && !ThinningSupported())
        return MF_E_THINNING_UNSUPPORTED;

    //float int_part;
    //const float frac_part = modf(rate, &int_part);

    if (rate > 128)
    {
        if (pNearestRate)
            *pNearestRate = 128;

        return MF_E_UNSUPPORTED_RATE;
    }

    if (pNearestRate)
        *pNearestRate = rate;

    return S_OK;
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


HRESULT WebmMfSource::CreateStream(
    IMFStreamDescriptor* pSD,
    const mkvparser::Track* pTrack)
{
    assert(pSD);
    assert(pTrack);
    assert(pTrack->GetNumber() > 0);

    const LONGLONG type = pTrack->GetType();
    WebmMfStream* pStream;

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
    assert(id == DWORD(pTrack->GetNumber()));

    const status_t status = m_streams.insert(std::make_pair(id, pStream));
    assert(status.second);  //new insertion
    assert(status.first->first == id);
    assert(status.first->second == pStream);

    return S_OK;
}

HRESULT WebmMfSource::QueueStreamEvent(
    MediaEventType met,
    WebmMfStream* pStream) const
{
    return m_pEvents->QueueEventParamUnk(met, GUID_NULL, S_OK, pStream);
}


LONGLONG WebmMfSource::GetCurrTime(IMFPresentationDescriptor* pDesc) const
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

    if (FAILED(hr) || (count == 0))
        return -1;

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
        assert(pTrack->GetNumber() > 0);
        assert(DWORD(pTrack->GetNumber()) == id);

        const iter_t iter = already_selected.find(id);

        if (iter != already_selected.end())  //already selected
        {
            WebmMfStream* const pStream = iter->second;
            assert(pStream);
            assert(pStream->m_pTrack == pTrack);

            newly_selected.insert(std::make_pair(id, pStream));
        }
    }

    if (newly_selected.empty())
        already_selected.swap(newly_selected);

    iter_t iter = newly_selected.begin();
    const iter_t iter_end = newly_selected.end();

    LONGLONG min_time_ns = -1;

    while (iter != iter_end)
    {
        const streams_t::value_type& value = *iter++;

        WebmMfStream* const pStream = value.second;
        assert(pStream);

        LONGLONG time_ns;

        hr = pStream->GetCurrTime(time_ns);

        if (FAILED(hr))
            continue;

        if ((min_time_ns < 0) || (time_ns < min_time_ns))
            min_time_ns = time_ns;
    }

    return min_time_ns;
}


void WebmMfSource::NotifyEOS()
{
    if (m_pEvents == 0)  //weird
        return;

    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        const WebmMfStream* const pStream = v.second;
        assert(pStream);

        if (!pStream->IsSelected())
            continue;

        if (!pStream->GetEOS())
        {
#ifdef _DEBUG
            odbgstream os;
            os << "WebmMfSource::NotifyEOS: !EOS on stream type="
               << pStream->m_pTrack->GetType()
               << endl;
#endif

            return;
        }
    }

#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::NotifyEOS: reporting EOF" << endl;
#endif

    const HRESULT hr = m_pEvents->QueueEventParamVar(
                        MEEndOfPresentation,
                        GUID_NULL,
                        S_OK,
                        0);

    assert(SUCCEEDED(hr));
}


WebmMfSource::CAsyncRead::CAsyncRead(WebmMfSource* p) :
    m_pSource(p),
    m_bCanInterrupt(false)
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

    WebmUtil::set_omaha_usage_flags(WebmTypes::APPID_WebmMf);

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
    enum { nh = 4 };
    const HANDLE ah[nh] =
    {
        m_hQuit,
        m_hCommand,
        m_hRequestSample,
        m_hAsyncRead,
    };

    for (;;)
    {
        const DWORD dw = WaitForMultipleObjects(nh, ah, FALSE, INFINITE);

        if (dw == WAIT_FAILED)
            return true;  //TODO: signal error to pipeline

        assert(dw >= WAIT_OBJECT_0);
        assert(dw < (WAIT_OBJECT_0 + nh));

        if (dw == WAIT_OBJECT_0) //hQuit
            return true;

        if (dw == (WAIT_OBJECT_0 + 1))  //hCommand
        {
            Lock lock;

            const HRESULT hr = lock.Seize(this);

            if (FAILED(hr))
                return true;

            const bool bCancel = OnCommand();

            if (!bCancel)
                continue;

            m_thread_state = &WebmMfSource::StateAsyncCancel;
            return false;
        }

        if (dw == (WAIT_OBJECT_0 + 2)) //hRequestSample
        {
            const requests_t& rr = m_requests;

            if (rr.empty())
                continue;  //spurious event

            if (!m_async_read.m_bCanInterrupt)
                continue;  //must allow async read to complete
#if 0
            bool have_audio = false;

            typedef requests_t::const_iterator iter_t;

            iter_t i = rr.begin();
            const iter_t j = rr.end();

            while (i != j)
            {
                const requests_t::value_type& v = *i++;

                if (v.pStream == 0)  //weird
                    continue;

                if (v.pStream->m_pTrack->GetType() == 2)  //audio
                {
                    have_audio = true;
                    break;
                }
            }

            if (!have_audio)
                continue;  //allow async read to complete
#endif
            m_thread_state = &WebmMfSource::StateAsyncCancel;
            return false;
        }

        assert(dw == (WAIT_OBJECT_0 + 3));  //hAsyncRead

        if (thread_state_t s = OnAsyncRead())
        {
            m_thread_state = s;
            return false;
        }
    }
}


bool WebmMfSource::StateAsyncCancel()
{
    //waiting for completion of obsolete async read

#if 0 //def _DEBUG
    odbgstream os;
    os << "\n\nWebmMfSource::StateAsyncCancel(begin)" << endl;
#endif

    enum { nh = 3 };
    const HANDLE ah[nh] = { m_hQuit, m_hAsyncRead, m_hCommand };

    for (;;)
    {
        const DWORD dw = WaitForMultipleObjects(nh, ah, FALSE, INFINITE);

        if (dw == WAIT_FAILED)
            return true;  //TODO: signal error to pipeline

        assert(dw >= WAIT_OBJECT_0);
        assert(dw < (WAIT_OBJECT_0 + nh));

        if (dw == WAIT_OBJECT_0) //hQuit
            return true;

        Lock lock;

        const HRESULT hr = lock.Seize(this);

        if (FAILED(hr))
            return true;

        if (dw == (WAIT_OBJECT_0 + 1))  //hAsyncRead
        {
#if 0 //def _DEBUG
            os << "\nWebmMfSource::StateAsyncCancel(end):"
               << " cancelling async read\n\n"
               << endl;
#endif
            m_file.AsyncReadCancel();

            const BOOL b = SetEvent(m_hRequestSample);
            assert(b);

            m_thread_state = &WebmMfSource::StateRequestSample;

            return false;
        }

        assert(dw == (WAIT_OBJECT_0 + 2));  //hCommand

#if 0 //def _DEBUG
        os << "\nWebmMfSource::StateAsyncCancel(cont'd):"
           << "handling command\n"
           << endl;
#endif

        OnCommand();
    }
}


bool WebmMfSource::StateRequestSample()
{
    enum { nh = 3 };
    const HANDLE ah[nh] = { m_hQuit, m_hCommand, m_hRequestSample };

    for (;;)
    {
        const DWORD dw = WaitForMultipleObjects(nh, ah, FALSE, INFINITE);

        if (dw == WAIT_FAILED)  //weird
            return true;  //TODO: signal error to pipeline

        assert(dw >= WAIT_OBJECT_0);
        assert(dw < (WAIT_OBJECT_0 + nh));

        if (dw == WAIT_OBJECT_0) //hQuit
            return true;

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

    commands_t& cc = m_commands;

    if (m_pEvents == 0)  //shutdown
    {
        cc.clear();
        PurgeRequests();

        return &WebmMfSource::StateQuit;
    }

    if (cc.size() > 1)
    {
        OnCommand();

        const BOOL b = SetEvent(m_hRequestSample);
        assert(b);

        return 0;  //come back here
    }

    requests_t& rr = m_requests;

    //{
    //    odbgstream os;
    //    os << "OnRequestSample: requests.size=" << rr.size() << endl;
    //}

    if (rr.empty())
    {
        if (m_bCanSeek)
        {
            const mkvparser::Cues* const pCues = m_pSegment->GetCues();
            assert(pCues);

            pCues->LoadCuePoint();

            if (!pCues->DoneParsing())
            {
                const BOOL b = SetEvent(m_hRequestSample);
                assert(b);

                return 0;
            }
        }

        if (IsStopped())
        {
            //m_file.Clear();
            return 0;
        }

        m_async_read.m_bCanInterrupt = true;

        bool bDone;

        if (thread_state_t s = Parse(bDone))
            return s;

        if (!bDone)
        {
            //odbgstream os;
            //os << "OnRequestSample: Parse returned not done" << endl;

            const BOOL b = SetEvent(m_hRequestSample);
            assert(b);
        }

        //odbgstream os;
        //os //<< "OnRequestSample: Parse returned DONE; file.GetCurrPos="
        //   << m_file.GetCurrentPosition()
        //   << endl;

        return 0;
    }

    m_async_read.m_bCanInterrupt = false;

    Request& r = rr.front();

    WebmMfStream* const pStream = r.pStream;
    assert(pStream);

    if (!pStream->IsSelected())  //weird
    {
        if (r.pToken)
            r.pToken->Release();

        rr.pop_front();

        const BOOL b = SetEvent(m_hRequestSample);
        assert(b);

        return 0;
    }

    const mkvparser::BlockEntry* const pCurr = pStream->GetCurrBlock();

    //Note that pCurr will be null only when we're thinning, or we've done
    //a seek.  Neither of these conditions apply when this a "slow seek"
    //case.

    if (pCurr == 0)  //must lazy-init curr block before doing anything else
    {
        //This was a seek (OnStartInitStreams).
        //This is also true when the video stream is in thinning mode,
        //since it resets its own pCurrBlock back to NULL, and re-inits
        //its cluster_pos, to force us to follow this path here.
#if 0
        m_pCurr = pStream->GetCurrBlockCluster();
        assert(m_pCurr);
        assert(!m_pCurr->EOS());
#else
        const LONGLONG cluster_pos = pStream->GetCurrBlockClusterPosition();
        assert(cluster_pos >= 0);

        m_pCurr = m_pSegment->FindOrPreloadCluster(cluster_pos);
        assert(m_pCurr);
        assert(!m_pCurr->EOS());

        pStream->SetCurrBlockIndex(m_pCurr);
#endif

        m_async_read.m_hrStatus = S_OK;
        m_async_state = &WebmMfSource::StateAsyncGetCurrBlockObjectInit;

        const BOOL b = SetEvent(m_hAsyncRead);
        assert(b);

        return &WebmMfSource::StateAsyncRead;
    }

    if (pCurr->EOS())
    {
        if (r.pToken)
            r.pToken->Release();

        const HRESULT hr = pStream->SetEOS();
        assert(SUCCEEDED(hr));  //TODO

#ifdef _DEBUG
        odbgstream os;
        os << "WebmMfSource::OnRequestSample: EOS detected for stream type="
           << pStream->m_pTrack->GetType()
           << endl;
#endif

        rr.pop_front();

        NotifyEOS();

        const BOOL b = SetEvent(m_hRequestSample);
        assert(b);

        return 0;
    }

#if 0
    const long status = pStream->GetNextBlock(m_pCurr);

    if (status < 0)  //underflow on curr cluster
    {
        odbgstream os;
        os << "OnRequestSample.GetNextBlock: type="
           << pStream->m_pTrack->GetType()
           << "; UNDERFLOW ON CURR CLUSTER"
           << endl;

        if (status != mkvparser::E_BUFFER_NOT_FULL)
        {
            Error(L"GetNextBlock failed", E_FAIL);
            return &WebmMfSource::StateQuit;
        }

        m_async_read.m_hrStatus = S_OK;
        m_async_state = &WebmMfSource::StateAsyncGetNextBlockCurrInit;

        const BOOL b = SetEvent(m_hAsyncRead);
        assert(b);

        return &WebmMfSource::StateAsyncRead;
    }

    if (status == 0)  //must move to next cluster
    {
        odbgstream os;
        os << "OnRequestSample.GetNextBlock: type="
           << pStream->m_pTrack->GetType()
           << "; END OF CURR CLUSTER - MOVING TO NEXT"
           << endl;

        m_async_state = &WebmMfSource::StateAsyncGetNextBlockNextInit;
        m_async_read.m_hrStatus = S_OK;

        const BOOL b = SetEvent(m_hAsyncRead);
        assert(b);

        return &WebmMfSource::StateAsyncRead;
    }
#else
    //If this is "handle slow-seek" case, then we must read from
    //the stream in a purely serial fashion.  Up until now we have
    //assumed that we could jump among frame objects, and file in
    //holes in the cache as we load in frames.  However, if the
    //stream only supports slow seek, then we cannot really skip
    //sections of the stream anymore -- we need to read serially
    //until we have the frame we need, filling the cache as we go.
    //
    //Suppose we maintain a "curr cluster and curr block index"
    //here.  We compare that to the cluster and index of this
    //stream's pCurr block.  If the source's curr cluster/index
    //is behind this stream's curr block, then we need to load
    //the cache until it's equal or greater.
    //
    //The search for the next block always starts at the index
    //1 greater than the curr block's index (on the curr block's
    //cluster).  If the source is behind this stream's curr block,
    //then it needs to fill the cache before allowing the search
    //for the next block to proceed.
    //
    //All of this assumes that the search for the next block
    //itself proceeds serially.  This search will search for
    //the next block OBJECT, but this has nothing to do with
    //what's in the cache.  We only go to the cache if we
    //cannot find the next object.
    //
    //We could always call GetEntry on behalf of the stream,
    //and then give it the correspond block object for it
    //to vet for suitability as a next block.  The advantage
    //is that we can control here how we walk the blocks
    //in the cluster, such that we ensure that the cache
    //it loaded serially.
    //
    //For Video:
    //The next block really does refer to the next block, the one
    //that follows the curr block.  This is true even in thinning mode.
    //In thinning mode it's used only to compute the duration of this
    //sample; it is not used to define the next sample (which we do
    //by using cue points).  In non-thinning mode, it is used both
    //to compute the duration of this sample and as the value of
    //the next sample.
    //
    //For Audio:
    //The next block determines the extent of this sample.  It is not
    //used to compute duration, which we do not pass downstream.
    //
    //if this is "has slow seek" then
    //   thinning mode doesn't apply
    //   so we're sending all frames (even when rate != 1)
    //   we must serially populate the cache, and then give blocks
    //     to stream from the already-loaded cache.
    //
    //else if this is a (fast seek possible) normal stream then
    //   if thinning then
    //      we do fetch the next block for video, to compute the duration
    //      of this sample; in the thinning mode case we might choose to
    //      not bother, in which case (for video) there's nothing else we'd
    //      need to do once we have the curr block.
    //
    //      for audio, get next block determines the extent of the audio
    //      stream (only); we don't send any payload.
    //
    //   else if not thinning then
    //      we could just do what we're doing now, although it wouldn't
    //      hurt to use our pLoad cluster

    m_pCurr = pCurr->GetCluster();
    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    for (;;)  //GetNextBlock
    {
        //We can give the stream our current cluster and block index;
        //it can compare its own position and decide whether our
        //(cluster, index) needs to be updated.  If the stream is behind
        //our curr pos, then it's safe to load it from the cache
        //(in fact we should be able to make this guarantee).  If the
        //stream is ahead of our position, then we need to advance our
        //position and try again.

        long status = pStream->GetNextBlock();

        if (status > 0)  //have next block
            break;

        if (status == 0)  //must move to next cluster
        {
            //odbgstream os;
            //os << "OnRequestSample.GetNextBlock: type="
            //   << pStream->m_pTrack->GetType()
            //   << "; END OF CURR CLUSTER - MOVING TO NEXT; curr_start="
            //   << m_pCurr->m_element_start
            //   << endl;

            m_async_state = &WebmMfSource::StateAsyncGetNextBlockNextInit;
            m_async_read.m_hrStatus = S_OK;

            const BOOL b = SetEvent(m_hAsyncRead);
            assert(b);

            return &WebmMfSource::StateAsyncRead;
        }

        assert(status < 0);  //underflow on curr cluster

        //odbgstream os;
        //os << "OnRequestSample.GetNextBlock: type="
        //   << pStream->m_pTrack->GetType()
        //   << "; UNDERFLOW ON CURR CLUSTER; curr_start="
        //   << m_pCurr->m_element_start
        //   << endl;

        for (;;)  //parse curr cluster
        {
            LONGLONG pos;
            LONG len;

            status = m_pCurr->Parse(pos, len);

            if (status >= 0)
                break;

            if (status != mkvparser::E_BUFFER_NOT_FULL)  //error
                return &WebmMfSource::StateQuit;

            const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

            if (FAILED(hr))
            {
                Error(L"LoadCurr AsyncReadInit failed (#2).", hr);
                return &WebmMfSource::StateQuit;
            }

            if (hr == S_FALSE)  //event will be set in Invoke
            {
                //const LONG page_size = m_file.GetPageSize();
                //const LONG count = (len + page_size - 1) / page_size;
                //odbgstream os;
                //os << "OnRequestSample.pCurr->Parse: type="
                //   << r.pStream->m_pTrack->GetType()
                //   << " pos="
                //   << pos
                //   << " count="
                //   << count
                //   << "; ASYNC READ; curr_start="
                //   << m_pCurr->m_element_start
                //   << endl;

              //m_async_state = &WebmMfSource::StateAsyncGetNextBlockCurrInit;
                m_async_state = &WebmMfSource::StateAsyncRequestSample;
                return &WebmMfSource::StateAsyncRead;
            }
        }  //end for parse curr cluster
    }  //end for GetNextBlock
#endif

    LONGLONG pos;
    LONG len;

    while (!pStream->GetSampleExtent(pos, len))
    {
        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"OnRequestSample AsyncReadInit failed.", hr);
            return &WebmMfSource::StateQuit;
        }

        if (hr == S_FALSE)  //async read in progress
        {
            //const LONG page_size = m_file.GetPageSize();
            //const LONG count = (len + page_size - 1) / page_size;
            //odbgstream os;
            //os << "OnRequestSample.GetSampleExtent: type="
            //   << pStream->m_pTrack->GetType()
            //   << " pos="
            //   << pos
            //   << " count="
            //   << count
            //   << "; ASYNC READ"
            //   << endl;

            m_async_state = &WebmMfSource::StateAsyncGetSampleExtent;
            return &WebmMfSource::StateAsyncRead;
        }

        pStream->GetSampleExtentCompletion();
    }

#if 1
    if (!pStream->IsCurrBlockLocked())
    {
        //TODO:
        //For audio, GetSampleExtent loads all the pages in the cache, and that
        //ensures that AsyncReadInit will always complete with S_OK.  However,
        //video doesn't work that way (GetSampleExtent does nothing, in fact),
        //and so we need this mechanism for ensuring that the video frame is
        //loaded in the cache.  As an alternative, we could make the video
        //stream behave as audio does, which would allow use to safely call
        //LockCurrBlock here (without also having to call AsyncReadInit).

        const mkvparser::Block* const pBlock = pCurr->GetBlock();
        assert(pBlock);

        pos = pBlock->m_start;
        assert(pos >= 0);

        const LONGLONG size = pBlock->m_size;
        assert(size >= 0);
        assert(size <= LONG_MAX);

        const LONG len = static_cast<LONG>(size);

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"OnRequestSample AsyncReadInit failed.", hr);
            return &WebmMfSource::StateQuit;
        }

        if (hr == S_FALSE)  //async read in progress
        {
            //const LONG page_size = m_file.GetPageSize();
            //const LONG count = (len + page_size - 1) / page_size;

            //if (count >= 16)
            //{
            //    odbgstream os;
            //    os << "OnRequestSample.IsCurrBlockLocked: type="
            //       << pStream->m_pTrack->GetType()
            //       << " pos=" << pos
            //       << " len=" << len
            //       << " count=" << count
            //       << "; ASYNC READ; cluster start="
            //       << pCurr->GetCluster()->m_element_start
            //       << endl;
            //}

            m_async_state = &WebmMfSource::StateAsyncLockCurr;
            return &WebmMfSource::StateAsyncRead;
        }

        const int status = pStream->LockCurrBlock();
        status;
        assert(status == 0);
    }
#endif

    hr = pStream->GetSample(r.pToken);

    if (FAILED(hr))
    {
        Error(L"OnRequestSample: GetSample failed.", hr);
        return &WebmMfSource::StateQuit;
    }

    if (r.pToken)
        r.pToken->Release();

    rr.pop_front();

    PurgeCache();

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return 0;
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncRequestSample()
{
    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return &WebmMfSource::StateRequestSample;
}


void WebmMfSource::PurgeCache()
{
    if (m_bCanSeek && !m_pSegment->GetCues()->DoneParsing())
        return;

    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    using namespace mkvparser;

    LONGLONG pos = -1;  //pos of cluster relative to segment

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        const WebmMfStream* const pStream = v.second;
        assert(pStream);

        if (!pStream->IsSelected())
            continue;

        const BlockEntry* const pCurr = pStream->GetCurrBlock();

        if ((pCurr != 0) && pCurr->EOS())
            continue;

        LONGLONG cluster_pos;

        if (pCurr == 0)  //block object hasn't been parsed yet
        {
            cluster_pos = pStream->GetCurrBlockClusterPosition();
            assert(cluster_pos >= 0);
        }
        else
        {
            const Cluster* const pCurrCluster = pCurr->GetCluster();
            assert(pCurrCluster);
            assert(!pCurrCluster->EOS());

            cluster_pos = pCurrCluster->GetPosition();
            assert(cluster_pos >= 0);
        }

        if ((pos < 0) || (cluster_pos < pos))
            pos = cluster_pos;
    }

    if (pos >= 0)
        m_file.Purge(m_pSegment->m_start + pos);
}


WebmMfSource::thread_state_t
WebmMfSource::Parse(bool& bDone)
{
    bDone = true;

    if (m_bThin || (m_rate != 1))
        return 0;

    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    const mkvparser::Cluster* pCurr = 0;

    using namespace mkvparser;

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        const WebmMfStream* const pStream = v.second;
        assert(pStream);

        if (!pStream->IsSelected())
            continue;

        const BlockEntry* const pCurrEntry = pStream->GetCurrBlock();

        if (pCurrEntry == 0)
            continue;

        if (pCurrEntry->EOS())
            continue;

        const Cluster* const pCurrCluster = pCurrEntry->GetCluster();
        assert(pCurrCluster);
        assert(!pCurrCluster->EOS());

        const LONGLONG start = pCurrCluster->m_element_start;

        if ((pCurr == 0) || (start > pCurr->m_element_start))
            pCurr = pCurrCluster;
    }

    return Parse(pCurr, bDone);
}


WebmMfSource::thread_state_t
WebmMfSource::Parse(const mkvparser::Cluster* pCurr, bool& bDone)
{
    bDone = true;

    if (pCurr == 0)
        return 0;

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = pCurr->Parse(pos, len);

        if (status > 0)  //nothing left to parse on curr cluster
            break;

        if (status == 0)  //parsed something on curr cluster
        {
            bDone = false;
            return 0;
        }

        if (status != mkvparser::E_BUFFER_NOT_FULL)  //bad file format
        {
            Error(L"Parse failed.", E_FAIL);
            return &WebmMfSource::StateQuit;
        }

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"StateAsyncGetCurrBlockObjectInit AsyncReadInit.", hr);
            return &WebmMfSource::StateQuit;
        }

        if (hr == S_FALSE)  //event will be set in Invoke
        {
            //const LONG page_size = m_file.GetPageSize();
            //const LONG count = (len + page_size - 1) / page_size;

            //const LONGLONG filepos = m_file.GetCurrentPosition();
            //
            //odbgstream os;
            //os << "Parse: pCurr->Parse: ASYNC READ; pos=" << pos
            //   //<< " len=" << len
            //   //<< " page_count=" << count
            //   //<< " curr cluster start=" << m_pCurr->m_element_start
            //   << " filepos=" << filepos
            //   << " pos-filepos=" << (pos - filepos)
            //   << endl;

            //m_async_state = &WebmMfSource::StateAsyncParseCurr;
            m_async_state = &WebmMfSource::StateAsyncRequestSample;
            return &WebmMfSource::StateAsyncRead;
        }
    }

    assert(bDone);

    //Create next cluster object (if it doesn't already exist).

    const mkvparser::Cluster* pNext;

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = m_pSegment->ParseNext(pCurr, pNext, pos, len);

        if (status > 0)  //EOF
            return 0;

        if (status == 0)  //have next cluster
            break;

        if (status != mkvparser::E_BUFFER_NOT_FULL)
        {
            Error(L"StateAsyncGetNextBlockNextInit ParseNext.", E_FAIL);
            return &WebmMfSource::StateQuit;  //TODO
        }

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"StateAsyncGetNextBlockNextInit AsyncReadInit.", hr);
            return &WebmMfSource::StateQuit;  //TODO
        }

        if (hr == S_FALSE)  //async read in progress
        {
            //const LONG page_size = m_file.GetPageSize();
            //const LONG count = (len + page_size - 1) / page_size;
            //odbgstream os;
            //os << "Parse: pSegment->ParseNext ASYNC READ; pos=" << pos
            //   << " len=" << len
            //   << " page_count=" << count
            //   << endl;

            //m_async_state = &WebmMfSource::StateAsyncParseNextInit;
            m_async_state = &WebmMfSource::StateAsyncRequestSample;
            return &WebmMfSource::StateAsyncRead;
        }
    }

    assert(bDone);
    assert(pNext);
    assert(!pNext->EOS());

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = pNext->Parse(pos, len);

        if (status > 0) //nothing remains to be parsed
            return 0;

        if (status == 0)  //parsed something
        {
            bDone = false;
            return 0;
        }

        if (status != mkvparser::E_BUFFER_NOT_FULL)  //bad file format
        {
            Error(L"Parse failed.", E_FAIL);
            return &WebmMfSource::StateQuit;
        }

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"Parse AsyncReadInit.", hr);
            return &WebmMfSource::StateQuit;
        }

        if (hr == S_FALSE)  //event will be set in Invoke
        {
            //const LONG page_size = m_file.GetPageSize();
            //const LONG count = (len + page_size - 1) / page_size;
            //odbgstream os;
            //os << "Parse: pNext->Parse ASYNC READ; pos=" << pos
            //   << " len=" << len
            //   << " page_count=" << count
            //   << endl;

            //m_async_state = &WebmMfSource::StateAsyncParseNextFinal;
            m_async_state = &WebmMfSource::StateAsyncRequestSample;
            return &WebmMfSource::StateAsyncRead;
        }
    }
}


#if 0
WebmMfSource::thread_state_t
WebmMfSource::StateAsyncParseCurr()
{
    //odbgstream os;
    //os << "StateAsyncParseCurr(begin)" << endl;

    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = m_pCurr->Parse(pos, len);

        if (status >= 0)  //success, or end-of-cluster reached
        {
            const BOOL b = SetEvent(m_hRequestSample);
            assert(b);

            return &WebmMfSource::StateRequestSample;
        }

        if (status != mkvparser::E_BUFFER_NOT_FULL)  //bad file format
        {
            Error(L"Parse failed.", E_FAIL);
            return &WebmMfSource::StateQuit;
        }

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"StateAsyncParseCurr AsyncReadInit.", hr);
            return &WebmMfSource::StateQuit;
        }

        if (hr == S_FALSE)  //event will be set in Invoke
        {
            //const LONG page_size = m_file.GetPageSize();
            //const LONG count = (len + page_size - 1) / page_size;
            //odbgstream os;
            //os << "StateAsyncParseCurr: pCurr->Parse ASYNC READ; pos=" << pos
            //   << " len=" << len
            //   << " page_count=" << count
            //   << endl;

            return 0;
        }
    }
}
#endif


#if 0
WebmMfSource::thread_state_t
WebmMfSource::StateAsyncParseNextInit()
{
    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = m_pSegment->ParseNext(m_pCurr, m_pNext, pos, len);

        if (status >= 0)  //have next cluster, or EOF
        {
            const BOOL b = SetEvent(m_hRequestSample);
            assert(b);

            return &WebmMfSource::StateRequestSample;
        }

        if (status != mkvparser::E_BUFFER_NOT_FULL)
        {
            Error(L"StateAsyncParseNextInit.", E_FAIL);
            return &WebmMfSource::StateQuit;  //TODO
        }

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"StateAsyncParseNextInit AsyncReadInit.", hr);
            return &WebmMfSource::StateQuit;  //TODO
        }

        if (hr == S_FALSE)  //async read in progress
            return 0;
    }
}
#endif


#if 0
WebmMfSource::thread_state_t
WebmMfSource::StateAsyncParseNextFinal()
{
    assert(m_pNext);
    assert(!m_pNext->EOS());

    //odbgstream os;
    //os << "StateAsyncParseNextFinal(begin)" << endl;

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = m_pNext->Parse(pos, len);

        if (status >= 0)  //success, or end-of-cluster reached
        {
            const BOOL b = SetEvent(m_hRequestSample);
            assert(b);

            return &WebmMfSource::StateRequestSample;
        }

        if (status != mkvparser::E_BUFFER_NOT_FULL)  //bad file format
        {
            Error(L"Parse failed.", E_FAIL);
            return &WebmMfSource::StateQuit;
        }

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"StateAsyncParseNextFinal AsyncReadInit.", hr);
            return &WebmMfSource::StateQuit;
        }

        if (hr == S_FALSE)  //event will be set in Invoke
        {
            //const LONG page_size = m_file.GetPageSize();
            //const LONG count = (len + page_size - 1) / page_size;
            //odbgstream os;
            //os << "StateAsyncParseNextFinal: pNext->Parse ASYNC READ; pos="
            //   << pos
            //   << " len=" << len
            //   << " page_count=" << count
            //   << endl;

            return 0;
        }
    }
}
#endif

bool WebmMfSource::OnCommand()
{
    commands_t& cc = m_commands;

    if (m_pEvents == 0)  //shutdown
    {
        cc.clear();
        return true;  //yes, cancel any pending I/O
    }

    if (cc.size() <= 1)  //spurious event
        return false;    //no state change

#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::OnCommand (begin): cmds.size="
       << cc.size()
       << " cmd.back.kind="
       << cc.back().m_kind
       << endl;
#endif

    typedef commands_t::iterator iter_t;

    const iter_t prev = cc.begin();
    const iter_t curr = ++iter_t(prev);

    Command& c = *curr;

    bool bCancel;

    switch (c.m_kind)
    {
        case Command::kStart:
            c.OnStart();
            bCancel = true;
            break;

        case Command::kSeek:
            //PurgeRequests();  //?
            c.OnSeek();
            bCancel = true;
            break;

        case Command::kRestart:  //un-pause
            c.OnRestart();
            bCancel = false;
            break;

        case Command::kPause:
            c.OnPause();
            bCancel = false;
            break;

        case Command::kStop:
            PurgeRequests();
            c.OnStop();
            bCancel = true;
            break;

        default:
            assert(false);
            bCancel = false;
            break;
    }

    cc.pop_front();

#ifdef _DEBUG
    os << "WebmMfSource::OnCommand (end): cmds.size="
       << cc.size()
       << " cancel="
       << boolalpha
       << bCancel
       << endl;
#endif

    return bCancel;
}


WebmMfSource::Command::Command(Kind k, WebmMfSource* pSource) :
    m_kind(k),
    m_pSource(pSource),
    m_pDesc(0)
{
    PropVariantInit(&m_time);
}


WebmMfSource::Command::Command(const Command& rhs) :
    m_kind(rhs.m_kind),
    m_pSource(rhs.m_pSource),
    m_pDesc(rhs.m_pDesc)
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

    if (m_time.vt == VT_I8)
    {
        LONGLONG& reftime = m_time.hVal.QuadPart;

        if (reftime < 0)
            reftime = 0;
    }
}


#if 0
bool WebmMfSource::Command::OnStart()
{
#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::Command::OnStart" << endl;
#endif

    assert(m_kind == kStart);
    assert(m_pDesc);

    if (m_state)
        return (this->*m_state)();

    //initialization step

    LONGLONG& reftime = m_time.hVal.QuadPart;

    if (m_time.vt == VT_EMPTY)
    {
        m_time.vt = VT_I8;
        reftime = 0;
    }

    const LONGLONG time_ns = reftime * 100;
    assert(time_ns >= 0);

    m_pSource->m_preroll_ns = time_ns;  //TODO: better way to handle this?

    mkvparser::Segment* const pSegment = m_pSource->m_pSegment;

    const mkvparser::Tracks* const pTracks = pSegment->GetTracks();
    assert(pTracks);

    DWORD count;

    HRESULT hr = m_pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));
    assert(count > 0);

    LONGLONG base_pos = -2;  // -2 = audio only

    if (const mkvparser::Cues* pCues = pSegment->GetCues())
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

            using mkvparser::Track;

            const Track* const pTrack = pTracks->GetTrackByNumber(id);
            assert(pTrack);
            assert(pTrack->GetNumber() == id);

            if (pTrack->GetType() != 1)  //not video
                continue;

            if (base_pos == -2)
                base_pos = -1;  // base_pos >= -1 means we have video

            using mkvparser::CuePoint;

            const CuePoint* pCP;
            const CuePoint::TrackPosition* pTP;

            if (!pCues->Find(time_ns, pTrack, pCP, pTP))
                continue;

            assert(pCP);
            assert(pTP);
            assert(pTP->m_pos >= 0);

            if ((base_pos < 0) || (pTP->m_pos < base_pos))
                base_pos = pTP->m_pos;
        }

    if (base_pos < 0)  //no video, or no cue points
        return OnStartNoSeek(&Command::OnStartComplete);

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

    const mkvparser::Cluster*& pCurr = m_pSource->m_pCurr;

    pCurr = pSegment->FindOrPreloadCluster(base_pos);
    assert(pCurr);
    assert(!pCurr->EOS());

    //Since we found this cluster via a cue point, we assume
    //that it contains a keyframe, so there's nothing else
    //we need to do for video (except load the cluster in cache).
    //
    //If the webm file is correctly formatted, then, if we have an
    //audio stream, the audio should appear on the same cluster as
    //the keyframe.  If it doesn't, then we can either search
    //for audio on later clusters, or treat this as a badly-formatted
    //file, or just set the audio stream to EOS.
    //
    //The streams are already initialized to pCurr=NULL (because
    //we're stopped), so if we
    //don't find an audio block on that cluster, then pCurr for
    //the audio stream will remain set to NULL.  We treat this
    //value as an EOS indication, so there's nothing special we
    //need to do in this case.

    //We must reset the parse pointer, because we might have
    //preloaded a new cluster, so it will need to be parsed.

    m_pSource->m_file.ResetAvailable();

    m_state = &Command::StateStartInitStreams;
    //m_index = 0;

    m_pSource->m_async_state = &WebmMfSource::StateAsyncLoadCurr;
    m_pSource->m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_pSource->m_hAsyncRead);
    assert(b);

    return false;
}
#else

LONGLONG WebmMfSource::Command::GetClusterPos(LONGLONG time_ns) const
{
    if (time_ns <= 0)
        return -1;  //force no-seek start

    if (!m_pSource->m_bCanSeek)
        return -2;

    mkvparser::Segment* const pSegment = m_pSource->m_pSegment;

    const mkvparser::Cues* const pCues = pSegment->GetCues();
    assert(pCues);

    LONGLONG base_pos = -1;

    const mkvparser::Tracks* const pTracks = pSegment->GetTracks();
    assert(pTracks);

    typedef streams_t::const_iterator iter_t;
    const iter_t iter_end = m_pSource->m_streams.end();

    DWORD count;

    HRESULT hr = m_pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));
    assert(count > 0);

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
        assert(pTrack->GetNumber() > 0);
        assert(DWORD(pTrack->GetNumber()) == id);

        if (pTrack->GetType() != 1)  //not video
            continue;

        const iter_t iter = m_pSource->m_streams.find(id);

        if (iter == iter_end)  //weird
            continue;

        WebmMfStream* const pStream = iter->second;
        assert(pStream);

        using mkvparser::BlockEntry;
        using mkvparser::Block;

        const BlockEntry* const pFirst = pStream->GetFirstBlock();

        if (pFirst == 0)  //requires care
            continue;

        if (pFirst->EOS())
            continue;

        const Block* const pBlock = pFirst->GetBlock();
        assert(pBlock);

        const LONGLONG first_time_ns = pBlock->GetTime(pFirst->GetCluster());
        assert(first_time_ns >= 0);

        if (time_ns <= first_time_ns)
            continue;

        using mkvparser::CuePoint;

        const CuePoint* pCP;
        const CuePoint::TrackPosition* pTP;

        for (;;)
        {
            pCues->LoadCuePoint();

            pCP = pCues->GetLast();
            assert(pCP);

            if (pCP->GetTime(pSegment) >= time_ns)
                break;

            if (pCues->DoneParsing())
                break;
        }

        if (!pCues->Find(time_ns, pTrack, pCP, pTP))
            continue;

        assert(pCP);
        assert(pTP);
        assert(pTP->m_pos >= 0);

        if ((base_pos < 0) || (pTP->m_pos < base_pos))
            base_pos = pTP->m_pos;
    }

    return base_pos;
}


void WebmMfSource::Command::OnStart()
{
    assert(m_kind == kStart);
    assert(m_pDesc);

    LONGLONG& reftime = m_time.hVal.QuadPart;

    if (m_time.vt == VT_EMPTY)
    {
        m_time.vt = VT_I8;
        reftime = 0;
    }

    const LONGLONG time_ns = reftime * 100;
    assert(time_ns >= 0);

#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::Command::OnStart: time_ns="
       << time_ns
       << " time[sec]="
       << (double(reftime) / 10000000)
       << endl;
#endif

    //If the byte stream "has slow seek" then we cannot
    //seek, so there's no need to call GetClusterPos.
    //Alternatively we could choose OnStartNoSeek immediately
    //if the requested time is EMPTY or reftime=0.

    const LONGLONG base_pos = GetClusterPos(time_ns);

    if (base_pos < 0)
        OnStartNoSeek();
    else
        OnStartInitStreams(time_ns, base_pos);

    OnStartComplete();
}
#endif


void WebmMfSource::Command::OnStartNoSeek() const
{
#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::Command::OnStartNoSeek" << endl;
#endif

    //We want to set curr pos = pos of first cluster
    //if this is a slow seek, then in principle actually setting
    //the curr pos shouldn't be necessary.

    //we don't have anything but a time
    //without cue points we cannot jump to anywhere in the file,
    //  so where are forced to search among already-loaded clusters
    //  that's exactly what segment::findcluster does for us
    //
    //we don't necessarily have a guarantee that we even have a cluster
    //  unless we make that guarantee.  I suppose we have that
    //  with AsyncReadInitStreams.

    //Calling FindCluster is only meaningful if have parsed
    //one or more clusters.
    //Should be even bother trying to seek in this case?
    //MF treats the stopped state as having time=0, which
    //we can interpret (for the purposes of the "start" command)
    //to mean "start from first cluster".
    //
    //We have been assuming that if we don't have a Cues element,
    //that we can seek by searching among clusters.  But if
    //we haven't parsed any clusters yet, or there are fewer parsed
    //clusters then exist in the file, the seeking isn't really
    //a meaningful operation.
    //
    //This is especially true if time.vt = VT_EMPTY, which is an
    //explicit request to start from beginning.  If the client
    //attempts to issue a "start" command, and time.vt == VT_I8,
    //and time.t > 0, and we don't have any cues (which means
    //we said we cannot seek), then we could just say too bad,
    //you can't search and so we're starting from the beginning
    //of thie file irrespective of this non-zero time request.
    //
    //If we say that we cannot seek, then we assume either
    //that the requested time will be VT_EMPTY, or time will be 0.
    //If t > 0 then should be return an error?
    //
    //This still leaves the issue of handing the "start" command
    //if we haven't parsed any clusters.  We do parse a few clusters
    //when we first initialize (in AsyncStateInitStreams), so
    //we have a guarntee here that if clusters exist, then
    //at least one of them will be parsed, and so FindCluster
    //will return a non-EOS cluster.
    //
    //If we decide that we cannot seek (because we do not have Cues),
    //this only means that we don't search for a cluster -- we just
    //immediately start on the first cluster.  We have a guarantee
    //that at least one cluster has been loaded.  But this doesn't
    //relieve us of the burden of searching for the first block
    //for each stream.


    //TODO SUN AM:
    //If we can't seek (don't have cues), then
    //we just set each stream to its first block.
    //We cached the first value when we call StateAsyncInitStreams.
    //That means we don't need to bother calling FindCluster here,
    //since we already have the first block for each stream.
    //
    //The only issue is how to handle loading the current cluster.
    //In principle we could have different clusters for each
    //stream.  We could iterate over the streams to find the
    //cluster with the lowest pos.  (Can we just start with
    //first cluster in segment?  This is a start following a
    //stop, which by definition of stop means t=0.)
    //
    //As long as we call file.resetavail, the OnRequestSample
    //will do the right thing when it checks whether pCurr
    //is locked.

    //SUN AM UPDATE:
    //I think we still need this logic, because it's still possible
    //that even with a Cues element we won't find any matching
    //cue points.

#if 0
    pCurr = pSegment->FindCluster(time_ns);
    assert(pCurr);
    assert(!pCurr->EOS());     //because of AsyncReadInitStreams
    assert(pCurr->m_pos > 0);  //because we always partially load
    assert(pCurr->m_size > 0);

    //We cannot assert this for the reasons explained in
    //StateStartInitStreams.
    //assert(pCurr->GetTime() <= time_ns);
#endif

    //The problem here is that we're simply finding a cluster.
    //We don't have a guarantee that we have a keyframe on that
    //cluster.  It would be weird if we seeked to a cluster
    //that happened to not have any keyframes, and then we
    //immediately reached EOS, especially because there might
    //be surrounding clusters that do have keyframes, and we'd
    //prefer to navigate to one of those.
    //
    //Do we have a guarantee here that cluster pCurr has been
    //fully loaded?  It would be nice to search this cluster
    //right here to determine whether it has a keyframe.
    //If it does have a keyframe then we're done.  If no keyframe
    //on this cluster then we have to decide where to search:
    //either before or after.  We prefer before, but if we
    //reach the first cluster there's nothing else we can do
    //except search forward.
    //
    //The alternative is for us to build our own keyframe
    //index.  That would be optimal.

    //if (pCurr->GetTime() <= time_ns)
    //  we have at least one cluster less than the requested time
    //
    //
    //else
    //  there were no clusters less than the requested time
    //  can we assume this is the first cluster?
    //  if we search, we only need to search forward
    //
    //
    //endif

    //we have the cluster object, that we can guarantee is at least
    //partially loaded
    //we need to load it into the cache

    m_pSource->m_preroll_ns = -1;  //don't throw anything away

    mkvparser::Segment* const pSegment = m_pSource->m_pSegment;

    const mkvparser::Tracks* const pTracks = pSegment->GetTracks();
    assert(pTracks);

    typedef streams_t::const_iterator iter_t;
    const iter_t iter_end = m_pSource->m_streams.end();

    DWORD count;

    HRESULT hr = m_pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));
    assert(count > 0);

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

        using mkvparser::Track;

        const Track* const pTrack = pTracks->GetTrackByNumber(id);
        assert(pTrack);
        assert(pTrack->GetNumber() > 0);
        assert(DWORD(pTrack->GetNumber()) == id);

        const iter_t iter = m_pSource->m_streams.find(id);

        if (iter == iter_end)  //weird
            continue;

        WebmMfStream* const pStream = iter->second;
        assert(pStream);

        if (!bSelected)
        {
            pStream->Deselect();
            continue;
        }

        const MediaEventType met = pStream->Select();

        hr = m_pSource->QueueStreamEvent(met, pStream);
        assert(SUCCEEDED(hr));

        //TODO: if this is an empty stream (no clusters contain
        //blocks from this track), then we have to ensure that
        //GetFirstBlock returns an EOS block, so that pCurr
        //gets set to EOS.  If it gets initialized to pFirst=NULL,
        //then pCurr=NULL, and this is intepreted to mean
        //"use the requested seek time to find the block".

        pStream->SetCurrBlock(pStream->GetFirstBlock());
    }

    const mkvparser::Cluster* const pCurr = pSegment->GetFirst();

#if 0
    LONGLONG avail;

    if ((pCurr == 0) || pCurr->EOS())  //weird
        avail = 0;
    else if (pCurr->GetEntryCount() < 0)  //weird
        avail = 0;
    else
    {
        const LONGLONG pos = pCurr->m_element_start;
        assert(pos >= 0);

        const LONGLONG size = pCurr->GetElementSize();
        assert(size > 0);

        avail = pos + size;
    }

    m_pSource->m_file.ResetAvailable(avail);
#else
    MkvReader& f = m_pSource->m_file;

    f.ResetAvailable(0);
    f.Seek(pCurr->m_element_start);
#endif
}


void WebmMfSource::Command::OnStartComplete() const
{
    HRESULT hr = m_pSource->QueueEvent(
                    MESourceStarted,
                    GUID_NULL,
                    S_OK,
                    &m_time);
    assert(SUCCEEDED(hr));

    typedef streams_t::iterator iter_t;

    iter_t iter = m_pSource->m_streams.begin();
    const iter_t iter_end = m_pSource->m_streams.end();

    while (iter != iter_end)
    {
        const streams_t::value_type& v = *iter++;

        WebmMfStream* const s = v.second;
        assert(s);

        hr = s->Start(m_time);
        assert(SUCCEEDED(hr));
    }
}


void WebmMfSource::Command::OnSeekComplete() const
{
    HRESULT hr = m_pSource->QueueEvent(
                    MESourceSeeked,
                    GUID_NULL,
                    S_OK,
                    &m_time);
    assert(SUCCEEDED(hr));

    typedef streams_t::iterator iter_t;

    iter_t iter = m_pSource->m_streams.begin();
    const iter_t iter_end = m_pSource->m_streams.end();

    while (iter != iter_end)
    {
        const streams_t::value_type& v = *iter++;

        WebmMfStream* const s = v.second;
        assert(s);

        hr = s->Seek(m_time);
        assert(SUCCEEDED(hr));
    }
}


#if 0
bool WebmMfSource::Command::StateStartInitStreams()
{
    //we have loaded m_pCurr (the base cluster for this seek) in the cache

    const mkvparser::Cluster* const pCurr = m_pSource->m_pCurr;
    assert(pCurr);
    assert(!pCurr->EOS());
    assert(pCurr->m_pos >= 0);
    assert(pCurr->m_size >= 0);

    //TODO:
    //For now, we cannot assert this:
    //assert(pCurr->GetTime() <= time_ns);
    //It is possible that the first cluster (or first few clusters)
    //do not have any keyframes, and if the requested seek time is 0,
    //then the cluster containing the first keyframe will not have
    //a timecode less or equal to the requested time.
    //END TODO.

    assert(m_time.vt == VT_I8);

    const LONGLONG reftime = m_time.hVal.QuadPart;
    assert(reftime >= 0);

    //To handle the pathological case when the first cluster(s)
    //doesn't have a keyframe, and so the cluster containing
    //the first keyframe has a timecode greater than the requested
    //seek time.  See comments above (where we check the cluster).

    const LONGLONG time_ns_ = reftime * 100;
    const LONGLONG time_ns = (pCurr->GetTime() <= time_ns_) ? time_ns_ : -1;

    mkvparser::Segment* const pSegment = m_pSource->m_pSegment;

    const mkvparser::Tracks* const pTracks = pSegment->GetTracks();
    assert(pTracks);

    DWORD count;

    HRESULT hr = m_pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));
    assert(count > 0);
    //assert(m_index < count);

    typedef streams_t::const_iterator iter_t;

    const streams_t& streams = m_pSource->m_streams;
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

        //When we're born, we're in a stopped state, but not stream instances
        //have been created yet, so a stream isn't selected then there's
        //nothing we need to do here.
        //
        //After we have been started and then stopped, stream instances have
        //been created, but are stopped when we handled the stop command.
        //During stop we automatically deselect the streams, so again there's
        //nothing we need to here to handle an stream that is not selected
        //for this start request.

        const iter_t iter = streams.find(id);

        if (iter == iter_end)  //weird
            continue;

        WebmMfStream* const pStream = iter->second;
        assert(pStream);

        if (!bSelected)
        {
            pStream->Deselect();
            continue;
        }

        const MediaEventType met = pStream->Select();

        //TODO:
        //MEUpdatedStream Event
        //http://msdn.microsoft.com/en-us/library/ms696195(v=vs.85).aspx
        //
        //This page is saying that there are some circumstances under which
        //you would send MENewStream instead of MEUpdatedStream (namely,
        //if this stream wasn't selected in the previous start request).
        //
        //TODO: this means that before we set the stream to selected,
        //we must query its current value.
        //
        //"On the first call to Start in which a stream becomes active,
        //the media source sends an MENewStream event for the stream.
        //On subsequent calls to Start, the media source sends an
        //MEUpdatedStream event, until the stream is deselected."

        hr = m_pSource->QueueStreamEvent(met, pStream);
        assert(SUCCEEDED(hr));

        using mkvparser::BlockEntry;
        using mkvparser::Cluster;

        const BlockEntry* const pFirstEntry = pStream->GetFirstBlock();

        if ((pFirstEntry == 0) || pFirstEntry->EOS())  //weird: empty stream
        {
            pStream->SetCurrBlock(pFirstEntry);  //implies immediate EOS
            continue;
        }

        const mkvparser::Block* const pFirstBlock = pFirstEntry->GetBlock();
        assert(pFirstBlock);

        const Cluster* const pFirstCluster = pFirstEntry->GetCluster();
        assert(pFirstCluster);
        assert(!pFirstCluster->EOS());

        const LONGLONG first_time_ns = pFirstBlock->GetTime(pFirstCluster);

        if (time_ns <= first_time_ns)
        {
            pStream->SetCurrBlock(pFirstEntry);
            continue;
        }

        const BlockEntry* const pBlock = pCurr->GetEntry(pTrack, time_ns);
        pStream->SetCurrBlock(pBlock);
    }

    //TODO:
    //MESourceStarted Event
    //http://msdn.microsoft.com/en-us/library/ms701823(VS.85).aspx
    //
    //"A media source raises this event when it starts from a stopped state,
    //or starts from a paused state at the same position in the source. The
    //event is raised if the IMFMediaSource::Start method returns S_OK.
    //
    //If the media source starts from the current position and the source's
    //previous state was running or paused, the event data can empty
    //(VT_EMPTY).
    //If the event data is VT_EMPTY, the media source might set the
    //MF_EVENT_SOURCE_ACTUAL_START
    //attribute with the actual starting time.
    //
    //If the media source starts from a new position, or the source's
    //previous state
    //was stopped, the event data must be the starting time (VT_I8).
    //
    //If the Start method causes a seek, the media source sends the
    //MESourceSeeked
    //event instead of MESourceStarted.

    return true;  //done
}
#else
void WebmMfSource::Command::OnStartInitStreams(
    LONGLONG time_ns,
    LONGLONG base_pos) const
{
    m_pSource->m_preroll_ns = m_pSource->m_bThin ? -1 : time_ns;

    mkvparser::Segment* const pSegment = m_pSource->m_pSegment;

    const mkvparser::Tracks* const pTracks = pSegment->GetTracks();
    assert(pTracks);

    DWORD count;

    HRESULT hr = m_pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));
    assert(count > 0);

    typedef streams_t::const_iterator iter_t;

    const streams_t& streams = m_pSource->m_streams;
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
        assert(pTrack->GetNumber() > 0);
        assert(DWORD(pTrack->GetNumber()) == id);

        const iter_t iter = streams.find(id);

        if (iter == iter_end)  //weird
            continue;

        WebmMfStream* const pStream = iter->second;
        assert(pStream);

        if (!bSelected)
        {
            pStream->Deselect();
            continue;
        }

        const MediaEventType met = pStream->Select();

        hr = m_pSource->QueueStreamEvent(met, pStream);
        assert(SUCCEEDED(hr));

        pStream->SetCurrBlockInit(time_ns, base_pos);
    }

    MkvReader& f = m_pSource->m_file;

    f.ResetAvailable(base_pos);
    f.Seek(base_pos);
}
#endif


void WebmMfSource::Command::OnSeek() const
{
    assert(m_kind == kSeek);
    assert(m_pDesc);
    assert(m_time.vt == VT_I8);

    const LONGLONG reftime = m_time.hVal.QuadPart;
    assert(reftime >= 0);

    const LONGLONG time_ns = reftime * 100;
    assert(time_ns >= 0);

#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::Command::OnSeek: time_ns="
       << time_ns
       << " time[sec]="
       << (double(reftime) / 10000000)
       << endl;
#endif

    const LONGLONG base_pos = GetClusterPos(time_ns);

    if (base_pos < 0)  //no video, or no cue points
        OnStartNoSeek();
    else
        OnStartInitStreams(time_ns, base_pos);

    OnSeekComplete();
}


void WebmMfSource::Command::OnStop() const
{
#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::Command::OnStop" << endl;
#endif

    streams_t& ss = m_pSource->m_streams;

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

    const HRESULT hr = m_pSource->QueueEvent(
                        MESourceStopped,
                        GUID_NULL,
                        S_OK,
                        0);

    assert(SUCCEEDED(hr));

    MkvReader& f = m_pSource->m_file;

    f.ResetAvailable(0);
    f.Seek(0);
}


void WebmMfSource::Command::OnPause() const
{
#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::Command::OnPause" << endl;
#endif

    streams_t& ss = m_pSource->m_streams;

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

    const HRESULT hr = m_pSource->QueueEvent(
                        MESourcePaused,
                        GUID_NULL,
                        S_OK,
                        0);
    assert(SUCCEEDED(hr));
}


void WebmMfSource::Command::OnRestart() const //unpause
{
    //If an unpause interupts an async read in progress,
    //then it might make sense to not cancel
    //the async read, since an unpause is benign, unlike
    //a start or seek.  In those cases, we really do want
    //to cancel the i/o in progress.

    assert(m_pDesc);

    mkvparser::Segment* const pSegment = m_pSource->m_pSegment;

    const mkvparser::Tracks* const pTracks = pSegment->GetTracks();
    assert(pTracks);

    //    if this is a restart, set the MF_EVENT_SOURCE_ACTUAL_START,
    //      with the pos as the propvar value.

    //if VT_EMPTY then this is a re-start
    //we need to figure out what the re-start time is
    //first determine whether there were streams selected previously
    //  if they're still selected, then synthesize the re-start time
    //  from one of them

    const LONGLONG curr_time_ns = m_pSource->GetCurrTime(m_pDesc);

#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfSource::Command::OnRestart: curr_time_ns="
       << curr_time_ns
       << " sec="
       << (double(curr_time_ns) / 1000000000)
       << endl;
#endif

    DWORD count;

    HRESULT hr = m_pDesc->GetStreamDescriptorCount(&count);
    assert(SUCCEEDED(hr));

    typedef streams_t::iterator iter_t;

    streams_t& streams = m_pSource->m_streams;
    const iter_t iter_end = streams.end();

#if 0  //TODO: see comments below
    LONGLONG base_pos = -1;

    if (curr_time_ns >= 0)
        base_pos = GetClusterPos(curr_time_ns);
#endif

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
        assert(pTrack->GetNumber() > 0);
        assert(DWORD(pTrack->GetNumber()) == id);

        const iter_t iter = streams.find(id);

        if (iter == iter_end)  //weird
            continue;

        WebmMfStream* const pStream = iter->second;
        assert(pStream);
        assert(pStream->m_pTrack == pTrack);

        if (!bSelected)
        {
            pStream->Deselect();
            continue;
        }

        //stream selected in this restart

        //TODO: it's not clear whether we need to send
        //this if the stream was already selected.
        //
        //For the answer see the page here:
        //MEUpdatedStream Event
        //http://msdn.microsoft.com/en-us/library/ms696195(v=vs.85).aspx

        const MediaEventType met = pStream->Select();

        hr = m_pSource->QueueStreamEvent(met, pStream);
        assert(SUCCEEDED(hr));

        if (met == MENewStream)
        {
#if 0
            if (base_pos < 0)
                pStream->SetCurrBlock(pStream->GetFirstBlock());
            else
                pStream->SetCurrBlockInit(curr_time_ns, base_pos);
#else
            //TODO:
            //The proper way to handle this is to seek this track
            //to the curr time.  But to do that means you either need
            //to look up the cue point for that time if this is video,
            //or look up the cluster if this is audio.  But this is
            //a lot of work to do just in case one of the tracks
            //might be newly-selected.
            //
            //For now, just start at beginning.  This is not correct,
            //if we've been playing for a while and we're not near t=0.
            //To make this correct there really needs to be a seek.

            pStream->SetCurrBlock(pStream->GetFirstBlock());
#endif
        }
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

    if (curr_time_ns >= 0)
    {
        const LONGLONG reftime = curr_time_ns / 100;
        hr = pEvent->SetUINT64(MF_EVENT_SOURCE_ACTUAL_START, reftime);
        assert(SUCCEEDED(hr));
    }

    hr = m_pSource->m_pEvents->QueueEvent(pEvent);
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

    if (m_pEvents == 0)  //shutdown
    {
        const BOOL b = SetEvent(m_hRequestSample);
        assert(b);

        return &WebmMfSource::StateRequestSample;  //clean up and exit
    }

    //this is the value of calling AsyncReadCompletion
    hr = m_async_read.m_hrStatus;  //assigned a value in Invoke

    if (FAILED(hr))
    {
        Error(L"OnAsyncRead AsyncReadCompletion failed.", hr);
        return &WebmMfSource::StateQuit;
    }

    if (hr == S_FALSE)  //not all bytes requested are in cache yet
    {
        hr = m_file.AsyncReadContinue(&m_async_read);

        if (FAILED(hr))
        {
            Error(L"OnAsyncRead AsyncReadContinue failed.", hr);
            return &WebmMfSource::StateQuit;
        }

        if (hr == S_FALSE)  //async read in progress
            return 0;       //wait for async read to complete
    }

    //All bytes requested are now in the cache.

    return (this->*m_async_state)();
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncLockCurr()
{
    //We get here after all requested bytes have been loaded into the cache.

    const requests_t& rr = m_requests;

    if (!rr.empty())
    {
        const Request& r = rr.front();
        assert(r.pStream);

        const int status = r.pStream->LockCurrBlock();
        status;
        assert(status == 0);
    }

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return &WebmMfSource::StateRequestSample;
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncGetSampleExtent()
{
    const requests_t& rr = m_requests;

    if (!rr.empty())
    {
        const Request& r = rr.front();
        assert(r.pStream);

        //odbgstream os;
        //os << "StateAsyncGetSampleExtent: type="
        //   << r.pStream->m_pTrack->GetType()
        //   << endl;

        r.pStream->GetSampleExtentCompletion();
    }

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return &WebmMfSource::StateRequestSample;
}


#if 0
WebmMfSource::thread_state_t
WebmMfSource::StateAsyncParseNextInit()
{
    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = m_pSegment->ParseNext(m_pCurr, m_pNext, pos, len);

        if (status >= 0)  //have next cluster, or EOF
            break;

        assert(status == mkvparser::E_BUFFER_NOT_FULL);  //TODO

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"ParseNext AsyncReadInit failed.", hr);
            return &WebmMfSource::StateQuit;  //TODO
        }

        if (hr == S_FALSE)  //async read in progress
            return 0;
    }

    m_async_state = &WebmMfSource::StateAsyncParseNextFinal;
    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hAsyncRead);
    assert(b);

    return 0;
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncParseNextFinal()
{
    //We have the next cluster, or we've reached EOF.
    //Next we must populate the cache with the cluster's payload.

    if (m_pNext)
    {
        assert(m_pNext);
        assert(!m_pNext->EOS());

        for (;;)
        {
            LONGLONG pos;
            LONG len;

            const long status = m_pNext->Parse(pos, len);

            if (status >= 0)
                break;

            if (status != mkvparser::E_BUFFER_NOT_FULL)  //error
            {
                Error(L"StateAsyncParseNextFinal: Cluster::Parse", E_FAIL);
                return &WebmMfSource::StateQuit;
            }

            const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

            if (FAILED(hr))
            {
                Error(L"StateAsyncParseNextFinal: AsyncReadInit failed.", hr);
                return &WebmMfSource::StateQuit;  //TODO
            }

            if (hr == S_FALSE)  //event will be set in Invoke
                return 0;
        }
    }

    const requests_t& rr = m_requests;

    if (rr.empty())  //weird
    {
        const BOOL b = SetEvent(m_hRequestSample);
        assert(b);

        return &WebmMfSource::StateRequestSample;
    }

    const Request& r = rr.front();
    assert(r.pStream);

    const long status = r.pStream->NotifyNextCluster(m_pNext);

    if (status > 0)  //success
    {
        const BOOL b = SetEvent(m_hRequestSample);
        assert(b);

        return &WebmMfSource::StateRequestSample;
    }

    if (status < 0) //error, or underflow
    {
        if (status != mkvparser::E_BUFFER_NOT_FULL)
        {
            Error(L"StateAsyncParseNextFinal: NotifyNextCluster.", E_FAIL);
            return &WebmMfSource::StateQuit;
        }

        assert(m_pNext);

        m_async_read.m_hrStatus = S_OK;

        const BOOL b = SetEvent(m_hAsyncRead);
        assert(b);

        return 0;
    }

    //We need a new cluster.

    m_pCurr = m_pNext;
    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    m_async_state = &WebmMfSource::StateAsyncParseNextInit;
    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hAsyncRead);
    assert(b);

    return 0;
}
#endif


#if 0
WebmMfSource::thread_state_t
WebmMfSource::StateAsyncLoadCurrInit()
{
    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        //We need to at least partially loaded the cluster, to know
        //its size.  We care about its size because we want to populate
        //the cache with the cluster.
        //
        //If the cluster is already parsed, then LoadBlockEntries will
        //simply return, because the cluster object has already been parsed.
        //But this doesn't mean that this cluster is in the cache -- it
        //only means that we have a cluster object and its has block entry
        //objects.
        //
        //If this cluster is only preloaded (possible during a cue-based
        //seek), or only partially loaded, then this operation will
        //parse the cluster which will require that the cluster have
        //been populated in the cache, and so the code below should
        //complete without having to do anything, because LoadBlockEntries
        //populated the cache for us (in order to parse it).
        //
        //But if the cluster object has already been fully parsed, then
        //LoadBlockEntries won't do anything, and the async read below
        //really will populate the cache.

        const long status = m_pCurr->Load(pos, len);

        if (status >= 0)
            break;

        if (status != mkvparser::E_BUFFER_NOT_FULL)  //error
            return &WebmMfSource::StateQuit;  //TODO

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"LoadCurr AsyncReadInit failed (#1).", hr);
            return &WebmMfSource::StateQuit;
        }

        //If AsyncReadInit returns S_FALSE, it means we needed to
        //populate the cache, and we were unable to complete the
        //parse (LoadBlockEntries).  In this case we must come back
        //here in order to actually do the parse again, now that
        //we have the data in the cache.  So it makes sense for
        //StateAsyncLoadCurr to be called twice.  The second time
        //it's called, LoadBlockEntries will succeed, and we jump
        //out of the loop.  But see TODO below.

        if (hr == S_FALSE)  //async read in progress
            return 0;
    }

#if 0
    LONGLONG pos = m_pCurr->m_pos;
    assert(pos > 0);

    pos += m_pSegment->m_start;  //absolute pos

    const LONGLONG size = m_pCurr->m_size;
    assert(size > 0);

    const LONGLONG len_ = 8 + 8 + size;
    assert(len_ > 0);
    assert(len_ <= LONG_MAX);

    const LONG len = static_cast<LONG>(len_);
#else
    const LONGLONG pos = m_pCurr->m_element_start;
    assert(pos >= 0);

    const LONGLONG size = m_pCurr->GetElementSize();
    assert(size > 0);
    assert(size <= LONG_MAX);

    const LONG len = static_cast<LONG>(size);
#endif

    const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

    if (FAILED(hr))
    {
        Error(L"LoadCurr AsyncReadInit failed (#2).", hr);
        return &WebmMfSource::StateQuit;
    }

    //TODO:
    //I think there's a optimization opportunity here.
    //If we're here, it means that the cluster has been parsed
    //(and we have the block entries in the cluster).  However,
    //if AsyncReadInit returns S_FALSE, it means we must asynchronously
    //read data into the cache, and we return back here.  However,
    //in that case we come back here (there is no state change),
    //which means we must call AsyncReadInit again, which means
    //we must walk the page cache again (and the second time,
    //it will succeed).  It would make more sense (and be slightly
    //more efficient) that if AsyncReadInit returns S_FALSE that
    //we transition to a new state, to "complete" this read
    //(there would be no actual work to do), and immediately
    //transition to the requesting samples state.
    //END TODO.

    if (hr == S_FALSE)  //event will be set in Invoke
    {
        m_async_state = &WebmMfSource::StateAsyncLoadCurrFinal;
        return 0;
    }

    return StateAsyncLoadCurrFinal();
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncLoadCurrFinal()
{
    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    const LONGLONG load_pos = m_pCurr->GetPosition();
    assert(load_pos >= 0);

    m_pCurr->LoadBlockEntries();
    assert(m_pCurr->GetEntryCount() >= 0);

    const requests_t& rr = m_requests;

    if (!rr.empty())  //should always be true
    {
        const Request& r = rr.front();

        WebmMfStream* const s = r.pStream;
        assert(s);

        LONGLONG preload_pos;  //offset relative to segment

        if (!s->IsSelected())  //weird
            __noop;
        else if (s->IsCurrBlockEOS()) //weird
            __noop;
        else if (s->HaveCurrBlockObject(preload_pos))  //weird
            __noop;
        else if (load_pos != preload_pos)  //weird
            __noop;
        else
            s->SetCurrBlockObject(m_pCurr);
    }

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return &WebmMfSource::StateRequestSample;
}
#else
WebmMfSource::thread_state_t
WebmMfSource::StateAsyncGetCurrBlockObjectInit()
{
    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    //odbgstream os;
    //os << "StateAsyncGetCurrBlockObjectInit(begin): pCurr.Count="
    //   << m_pCurr->GetEntryCount()
    //   << endl;

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = m_pCurr->Parse(pos, len);

        if (status >= 0)
            break;

        if (status != mkvparser::E_BUFFER_NOT_FULL)  //error
        {
            Error(L"StateAsyncGetCurrBlockObjectInit pCurr.Parse", E_FAIL);
            return &WebmMfSource::StateQuit;
        }

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"StateAsyncGetCurrBlockObjectInit AsyncReadInit.", hr);
            return &WebmMfSource::StateQuit;
        }

        if (hr == S_FALSE)  //event will be set in Invoke
        {
            m_async_state = &WebmMfSource::StateAsyncGetCurrBlockObjectFinal;
            return 0;
        }
    }

    //os << "StateAsyncGetCurrBlockObjectInit(end): pCurr.Count="
    //   << m_pCurr->GetEntryCount()
    //   << endl;

    return StateAsyncGetCurrBlockObjectFinal();
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncGetCurrBlockObjectFinal()
{
    const requests_t& rr = m_requests;

    if (!rr.empty())
    {
        const Request& r = rr.front();
        assert(r.pStream);

        const bool bDone = r.pStream->SetCurrBlockObject();

        if (!bDone)
        {
            m_async_state = &WebmMfSource::StateAsyncGetCurrBlockObjectInit;
            m_async_read.m_hrStatus = S_OK;

            const BOOL b = SetEvent(m_hAsyncRead);
            assert(b);

            return 0;
        }
    }

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return &WebmMfSource::StateRequestSample;
}


#if 0
WebmMfSource::thread_state_t
WebmMfSource::StateAsyncGetNextBlockCurrInit()
{
    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    const requests_t& rr = m_requests;

    if (rr.empty())  //weird
    {
        const BOOL b = SetEvent(m_hRequestSample);
        assert(b);

        return &WebmMfSource::StateRequestSample;
    }

    const Request& r = rr.front();
    assert(r.pStream);

    //for (;;)
    //{
        for (;;)
        {
            LONGLONG pos;
            LONG len;

            const long status = m_pCurr->Parse(pos, len);

            if (status >= 0)
            {
                //odbgstream os;
                //os << "StateAsyncGetNextBlockCurrInit.pCurr->Parse: type="
                //   << r.pStream->m_pTrack->GetType()
                //   << "; SUCCESS"
                //   << endl;

                break;
            }

            if (status != mkvparser::E_BUFFER_NOT_FULL)  //error
                return &WebmMfSource::StateQuit;

            const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

            if (FAILED(hr))
            {
                Error(L"LoadCurr AsyncReadInit failed (#2).", hr);
                return &WebmMfSource::StateQuit;
            }

            if (hr == S_FALSE)  //event will be set in Invoke
            {
                //const LONG page_size = m_file.GetPageSize();
                //const LONG count = (len + page_size - 1) / page_size;
                //odbgstream os;
                //os << "StateAsyncGetNextBlockCurrInit.pCurr->Parse: type="
                //   << r.pStream->m_pTrack->GetType()
                //   << " pos="
                //   << pos
                //   << " count="
                //   << count
                //   << "; ASYNC READ"
                //   << endl;

                return 0;
            }
        }

#if 0
        const mkvparser::Cluster* pCluster;

        const long status = r.pStream->GetNextBlock(pCluster);
        assert(pCluster == m_pCurr);

        if (status == 0)  //need block, but none left on current cluster
        {
            odbgstream os;
            os << "StateAsyncGetNextBlockCurrInit.pStream->GetNextBlock: type="
               << r.pStream->m_pTrack->GetType()
               << "; UNDERFLOW ON CURR CLUSTER - MOVING TO NEXT"
               << endl;

            m_async_read.m_hrStatus = S_OK;
            m_async_state = &WebmMfSource::StateAsyncGetNextBlockNextInit;

            const BOOL b = SetEvent(m_hAsyncRead);
            assert(b);

            return 0;
        }

        if (status > 0)  //success
        {
            const BOOL b = SetEvent(m_hRequestSample);
            assert(b);

            return &WebmMfSource::StateRequestSample;
        }

        assert(status < 0);  //underflow on curr cluster

        odbgstream os;
        os << "StateAsyncGetNextBlockCurrInit.pStream->GetNextBlock: type="
           << r.pStream->m_pTrack->GetType()
           << "; UNDERFLOW ON CURR CLUSTER"
           << endl;
#else
    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return &WebmMfSource::StateRequestSample;
#endif
    //}
}
#endif


#if 0
WebmMfSource::thread_state_t
WebmMfSource::StateAsyncGetNextBlockCurrFinal()
{
    const requests_t& rr = m_requests;

    if (!rr.empty())
    {
        const Request& r = rr.front();
        assert(r.pStream);

        const long status = r.pStream->GetNextBlock(m_pCurr);

        if (status < 0)  //underflow on curr cluster
        {
            if (status != mkvparser::E_BUFFER_NOT_FULL)
            {
                Error(L"GetNextBlock failed", E_FAIL);
                return &WebmMfSource::StateQuit;
            }

            m_async_read.m_hrStatus = S_OK;
            m_async_state = &WebmMfSource::StateAsyncGetNextBlockCurrInit;

            const BOOL b = SetEvent(m_hAsyncRead);
            assert(b);

            return 0;
        }

        if (status == 0)  //need block, but none left on current cluster
        {
            m_async_read.m_hrStatus = S_OK;
            m_async_state = &WebmMfSource::StateAsyncGetNextBlockNextInit;

            const BOOL b = SetEvent(m_hAsyncRead);
            assert(b);

            return 0;
        }
    }

    const BOOL b = SetEvent(m_hRequestSample);
    assert(b);

    return &WebmMfSource::StateRequestSample;
}
#endif


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncGetNextBlockNextInit()
{
    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = m_pSegment->ParseNext(m_pCurr, m_pNext, pos, len);

        if (status >= 0)  //have next cluster, or EOF
            break;

        if (status != mkvparser::E_BUFFER_NOT_FULL)
        {
            Error(L"StateAsyncGetNextBlockNextInit ParseNext.", E_FAIL);
            return &WebmMfSource::StateQuit;  //TODO
        }

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"StateAsyncGetNextBlockNextInit AsyncReadInit.", hr);
            return &WebmMfSource::StateQuit;  //TODO
        }

        if (hr == S_FALSE)  //async read in progress
            return 0;
    }

    m_async_state = &WebmMfSource::StateAsyncGetNextBlockNextNotify;
    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hAsyncRead);
    assert(b);

    return 0;
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncGetNextBlockNextNotify()
{
    //We have the next cluster, or we've reached EOF.
    //Next we must populate the cache with the cluster's payload.

    const requests_t& rr = m_requests;

    if (rr.empty())  //weird
    {
        const BOOL b = SetEvent(m_hRequestSample);
        assert(b);

        return &WebmMfSource::StateRequestSample;
    }

    const Request& r = rr.front();
    assert(r.pStream);

    const long status = r.pStream->NotifyNextCluster(m_pNext);

    if (status > 0)  //success
    {
        const BOOL b = SetEvent(m_hRequestSample);
        assert(b);

        return &WebmMfSource::StateRequestSample;
    }

    assert(m_pNext);
    assert(!m_pNext->EOS());

    if (status == 0)
    {
        //We need a new cluster, beyond pNext.

        m_pCurr = m_pNext;

        m_async_state = &WebmMfSource::StateAsyncGetNextBlockNextInit;
        m_async_read.m_hrStatus = S_OK;

        const BOOL b = SetEvent(m_hAsyncRead);
        assert(b);

        return 0;
    }

    assert(status < 0); //error, or underflow, on pNext cluster

    m_async_state = &WebmMfSource::StateAsyncGetNextBlockNextParse;
    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hAsyncRead);
    assert(b);

    return 0;
}


WebmMfSource::thread_state_t
WebmMfSource::StateAsyncGetNextBlockNextParse()
{
    assert(m_pNext);
    assert(!m_pNext->EOS());

    for (;;)
    {
        LONGLONG pos;
        LONG len;

        const long status = m_pNext->Parse(pos, len);

        if (status >= 0)
            break;

        if (status != mkvparser::E_BUFFER_NOT_FULL)  //error
        {
            Error(L"StateAsyncParseNextFinal: Cluster::Parse", E_FAIL);
            return &WebmMfSource::StateQuit;
        }

        const HRESULT hr = m_file.AsyncReadInit(pos, len, &m_async_read);

        if (FAILED(hr))
        {
            Error(L"StateAsyncParseNextFinal: AsyncReadInit", hr);
            return &WebmMfSource::StateQuit;  //TODO
        }

        if (hr == S_FALSE)  //event will be set in Invoke
            return 0;
    }

    m_async_state = &WebmMfSource::StateAsyncGetNextBlockNextNotify;
    m_async_read.m_hrStatus = S_OK;

    const BOOL b = SetEvent(m_hAsyncRead);
    assert(b);

    return 0;
}
#endif


HRESULT WebmMfSource::Error(const wchar_t* str, HRESULT hrStatus) const
{
    if (m_pEvents == 0)
        return S_FALSE;

    PROPVARIANT var;

    HRESULT hr = InitPropVariantFromString(str, &var);
    assert(SUCCEEDED(hr));

    const HRESULT hrResult = m_pEvents->QueueEventParamVar(
                                MEError,
                                GUID_NULL,
                                hrStatus,
                                &var);
    assert(SUCCEEDED(hrResult));

    hr = PropVariantClear(&var);
    assert(SUCCEEDED(hr));

    return hrResult;
}


void WebmMfSource::PurgeRequests()
{
    requests_t& rr = m_requests;

    while (!rr.empty())
    {
        Request& r = rr.front();

        if (r.pToken)
            r.pToken->Release();

        rr.pop_front();
    }
}


bool WebmMfSource::ThinningSupported() const
{
#if 0
    if (m_file.HasSlowSeek())
        return false;

    if (m_file.IsPartiallyDownloaded())
        return false;

    if (m_pSegment->GetCues() == 0)
        return false;

    typedef streams_t::const_iterator iter_t;

    iter_t i = m_streams.begin();
    const iter_t j = m_streams.end();

    bool bThin = false;

    while (i != j)
    {
        const streams_t::value_type& v = *i++;

        const WebmMfStream* const pStream = v.second;
        assert(pStream);

        const mkvparser::Track* const pTrack = pStream->m_pTrack;
        assert(pTrack);

        if (pTrack->GetType() != 1)  //not video
            continue;

        //more we should test here?
        //  stream selected
        //  whether cues has entries for this track
        //  etc

        bThin = true;  //TODO
        break;
    }

    return bThin;
#else
    //TODO: we probably need to manage the cache
    //differently when we're in thinning mode.
    //Right now, whenever we ask the cache for
    //data at a given position, we adjust the
    //position down (backwards, towards the front)
    //to the value of the stream's current pos.
    //This ensures that all data in the stream
    //gets put in the cache, so that there are
    //never gaps in the cache.  This is the behavior
    //that we want when we're streaming, because
    //we always have the data in the cache, ready
    //to be consumed.
    //
    //However, in the thinning mode case, this
    //is entirely wasteful and probably too
    //inefficient, because we only need a small
    //subset of the data from the stream to be
    //in the cache.  Firstly, we only need enough
    //stream data to determine block boundaries.
    //Secondly, we only need the block payload for
    //video keyframes (we don't send non-key video
    //frames, and we don't send any blocks at
    //all for audio, when thinning).  Thinning
    //mode is a case when we really do want
    //the cache to have gaps.  We need a way
    //to tell the cache manager to not adjust
    //the requested position.

    //return m_pCanSeek;
    return false;  //TODO: fix this
#endif
}


}  //end namespace WebmMfSource

