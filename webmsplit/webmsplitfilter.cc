// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include <uuids.h>
#include "webmsplitfilter.h"
#include "cenumpins.h"
#include "mkvparser.hpp"
#include "mkvparserstreamvideo.h"
#include "mkvparserstreamaudio.h"
#include "webmsplitoutpin.h"
#include "webmtypes.h"
#include <new>
#include <cassert>
#include <vfwmsgs.h>
#include <process.h>
#include <evcode.h>
#include <limits>
#ifdef _DEBUG
#include "iidstr.h"
#include "odbgstream.h"
using std::endl;
using std::hex;
using std::dec;
#endif

using std::wstring;
//using std::wistringstream;

namespace WebmSplit
{

const LONGLONG Filter::kNoSeek(std::numeric_limits<LONGLONG>::min());

HRESULT CreateInstance(
    IClassFactory* pClassFactory,
    IUnknown* pOuter,
    const IID& iid,
    void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    *ppv = 0;

    if ((pOuter != 0) && (iid != __uuidof(IUnknown)))
        return E_INVALIDARG;

    Filter* p = new (std::nothrow) Filter(pClassFactory, pOuter);

    if (p == 0)
        return E_OUTOFMEMORY;

    assert(p->m_nondelegating.m_cRef == 0);

    const HRESULT hr = p->m_nondelegating.QueryInterface(iid, ppv);

    if (SUCCEEDED(hr))
    {
        assert(*ppv);
        assert(p->m_nondelegating.m_cRef == 1);

        return S_OK;
    }

    assert(*ppv == 0);
    assert(p->m_nondelegating.m_cRef == 0);

    delete p;
    p = 0;

    return hr;
}


#pragma warning(disable:4355)  //'this' ptr in member init list
Filter::Filter(IClassFactory* pClassFactory, IUnknown* pOuter)
    : m_pClassFactory(pClassFactory),
      m_nondelegating(this),
      m_pOuter(pOuter ? pOuter : &m_nondelegating),
      m_state(State_Stopped),
      m_clock(0),
      m_hThread(0),
      m_pSegment(0),
      m_pSeekBase(0),
      m_seekBase_ns(-1),
      m_currTime(kNoSeek),
      m_inpin(this),
      m_cStarvation(-1)  //means "not starving"
{
    m_pClassFactory->LockServer(TRUE);

    const HRESULT hr = CLockable::Init();
    hr;
    assert(SUCCEEDED(hr));

    m_hNewCluster = CreateEvent(0, 0, 0, 0);
    assert(m_hNewCluster);  //TODO

    m_info.pGraph = 0;
    m_info.achName[0] = L'\0';

#ifdef _DEBUG
    odbgstream os;
    os << "WebmSplit::ctor" << endl;
#endif
}
#pragma warning(default:4355)


Filter::~Filter()
{
#ifdef _DEBUG
    odbgstream os;
    os << "WebmSplit::dtor" << endl;
#endif

    assert(m_hThread == 0);
    assert(m_outpins.empty());

    assert(m_pSegment == 0);

    m_pClassFactory->LockServer(FALSE);
}


void Filter::Init()
{
    assert(m_hThread == 0);

    if (!m_inpin.m_reader.IsOpen())
        return;

    if (m_pSegment->DoneParsing())
        return;  //nothing for thread to do

    const uintptr_t h = _beginthreadex(
                            0,  //security
                            0,  //stack size
                            &Filter::ThreadProc,
                            this,
                            0,   //run immediately
                            0);  //thread id

    m_hThread = reinterpret_cast<HANDLE>(h);
    assert(m_hThread);
}


void Filter::Final()
{
    if (m_hThread == 0)
        return;

    assert(m_inpin.m_reader.IsOpen());

    //odbgstream os;
    //os << "WebmSplit::Filter::Final(begin)" << endl;
    //os << "WebmSplit::Filter::Final: calling BeginFlush" << endl;

    //TODO: calling BeginFlush has the exact opposite semantics that
    //I thought it did: if flush is in effect, then the SyncRead blocks
    //indefinitely, until EndFlush is called.  In the local file playback
    //case this isn't a problem, since SyncRead will never officially block.
    //The problem case occurs when this is a slow network download, and
    //SyncRead blocks.  I thought that BeginFlush could be used to cancel
    //the SyncRead in progress, but that doesn't appear to be the case.
    //(Apparently it cancels asyncronous reads, not synchronous reads.)
    //This only really matters during the transition to stopped.  In that
    //case we could do something ugly like timeout the wait for signal
    //of thread termination, then if timeout occurs then forcibly
    //terminate the thread (but I don't know if that will work either).
    //The only other alternative is to use proper timed reads, but
    //that requires that reads be aligned.

    //HRESULT hr = pReader->BeginFlush();
    //assert(SUCCEEDED(hr));
    //
    //os << "WebmSplit::Filter::Final: called BeginFlush; "
    //   << "waiting for thread termination"
       //<< endl;

    //HRESULT hr = m_inpin.m_reader.Cancel();
    //assert(SUCCEEDED(hr));

    const DWORD dw = WaitForSingleObject(m_hThread, INFINITE);
    dw;
    assert(dw == WAIT_OBJECT_0);

    //os << "WebmSplit::Filter::Final: thread terminated" << endl;

    const BOOL b = CloseHandle(m_hThread);
    b;
    assert(b);

    m_hThread = 0;

    //os << "WebmSplit::Filter::Final: calling EndFlush" << endl;

    //hr = pReader->EndFlush();
    //assert(SUCCEEDED(hr));

    //os << "WebmSplit::Filter::Final: called EndFlush" << endl;
    //os << "WebmSplit::Filter::Final(end)" << endl;
}


Filter::CNondelegating::CNondelegating(Filter* p)
    : m_pFilter(p),
      m_cRef(0)  //see CreateInstance
{
}


Filter::CNondelegating::~CNondelegating()
{
}


HRESULT Filter::CNondelegating::QueryInterface(
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
    else if ((iid == __uuidof(IBaseFilter)) ||
             (iid == __uuidof(IMediaFilter)) ||
             (iid == __uuidof(IPersist)))
    {
        pUnk = static_cast<IBaseFilter*>(m_pFilter);
    }
    else
    {
#if 0
        wodbgstream os;
        os << "mkvsource::filter::QI: iid=" << IIDStr(iid) << std::endl;
#endif
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG Filter::CNondelegating::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}


ULONG Filter::CNondelegating::Release()
{
    const LONG n = InterlockedDecrement(&m_cRef);

    //odbgstream os;
    //os << "Filter::Release: n=" << n << endl;

    if (n > 0)
        return n;

    delete m_pFilter;
    return 0;
}


HRESULT Filter::QueryInterface(const IID& iid, void** ppv)
{
    return m_pOuter->QueryInterface(iid, ppv);
}


ULONG Filter::AddRef()
{
    return m_pOuter->AddRef();
}


ULONG Filter::Release()
{
    return m_pOuter->Release();
}


HRESULT Filter::GetClassID(CLSID* p)
{
    if (p == 0)
        return E_POINTER;

    *p = WebmTypes::CLSID_WebmSplit;
    return S_OK;
}



HRESULT Filter::Stop()
{
    //Stop is a synchronous operation: when it completes,
    //the filter is stopped.

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    switch (m_state)
    {
        case State_Paused:
        case State_Running:

            //Stop is synchronous.  When stop completes, all threads
            //should be stopped.  What does "stopped" mean"  In our
            //case it probably means "terminated".
            //It's a bit tricky here because we hold the filter
            //lock.  If threads need to acquire filter lock
            //then we'll have to release it.  Only the FGM can call
            //Stop, etc, so there's no problem to release lock
            //while Stop is executing, to allow threads to acquire
            //filter lock temporarily.
            //The streaming thread will receiving an indication
            //automatically (assuming it's connected), either via
            //GetBuffer or Receive, so there's nothing this filter
            //needs to do to tell the streaming thread to stop.
            //One implementation strategy is to have build a
            //vector of thread handles, and then wait for a signal
            //on one of them.  When the handle is signalled
            //(meaning that the thread has terminated), then
            //we remove that handle from the vector, close the
            //handle, and the wait again.  Repeat until the
            //all threads have been terminated.
            //We also need to clean up any unused samples,
            //and decommit the allocator.  (In fact, we could
            //decommit the allocator immediately, and then wait
            //for the threads to terminated.)

            m_state = State_Stopped;

            hr = m_inpin.m_reader.BeginFlush();
            assert(SUCCEEDED(hr));

            lock.Release();

            OnStop();

            hr = lock.Seize(this);
            assert(SUCCEEDED(hr));  //TODO

            hr = m_inpin.m_reader.EndFlush();
            assert(SUCCEEDED(hr));

            break;

        case State_Stopped:
        default:
            break;
    }

    return S_OK;
}


HRESULT Filter::Pause()
{
    //Unlike Stop(), Pause() can be asynchronous (that's why you have
    //GetState()).  We could use that here to build the samples index.

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //odbgstream os;
    //os << "WebmSplit::Filter::Pause" << endl;

    switch (m_state)
    {
        case State_Stopped:
            OnStart();
            break;

        case State_Running:
        case State_Paused:
        default:
            break;
    }

    m_state = State_Paused;
    return S_OK;
}


HRESULT Filter::Run(REFERENCE_TIME start)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //odbgstream os;
    //os << "WebmSplit::Filter::Run" << endl;

    switch (m_state)
    {
        case State_Stopped:
            OnStart();
            break;

        case State_Paused:
        case State_Running:
        default:
            break;
    }

    m_start = start;
    m_state = State_Running;

    return S_OK;
}


HRESULT Filter::GetState(
    DWORD timeout,
    FILTER_STATE* p)
{
    if (p == 0)
        return E_POINTER;

    //What the GetState.timeout parameter refers to is not to locking
    //the filter, but rather to waiting to determine the current state.
    //A request to Stop is always synchronous (hence no timeout parameter),
    //but a request to Pause can be asynchronous, so the caller can say
    //how long he's willing to wait for the transition (to paused) to
    //complete.

    //TODO: implement a waiting scheme here.  We'll probably have to
    //use SignalObjectAndWait atomically release the mutex and then
    //wait for the condition variable to change.
    //if (hr == VFW_E_TIMEOUT)
    //    return VFW_S_STATE_INTERMEDIATE;

    Lock lock;

    HRESULT hrLock = lock.Seize(this);

    //The lock is only used for synchronization.  If Seize fails,
    //it means there's a serious problem with the filter.

    if (FAILED(hrLock))
        return E_FAIL;

    FILTER_STATE& state = *p;

    if (m_cStarvation < 0)  //not starving
    {
        state = m_state;
        return S_OK;
    }

    assert(m_pSegment);

    long count = m_pSegment->GetCount();

    if (count > m_cStarvation)
    {
        m_cStarvation = -1;

        state = m_state;  //TODO: should be State_Paused?
        return S_OK;
    }

    for (;;)
    {
        lock.Release();

        DWORD index;

        //TODO: this timeout isn't quite correct.  The parameter refers
        //to the total wait time.  As used here in the call to WaitForHandles,
        //it refers to the wait time for this pass through the loop.

        const HRESULT hrWait = CoWaitForMultipleHandles(
                                0,  //wait flags
                                timeout,
                                1,
                                &m_hNewCluster,
                                &index);

        if (SUCCEEDED(hrWait))
            assert(index == 0);
        else if (hrWait != RPC_S_CALLPENDING) //error, despite "S" in name
            return hrWait;

        hrLock = lock.Seize(this);

        if (FAILED(hrLock))
            return E_FAIL;

        count = m_pSegment->GetCount();

        if (count > m_cStarvation)
        {
            m_cStarvation = -1;

            state = m_state;  //TODO: should be State_Paused?
            return S_OK;
        }

        if (FAILED(hrWait))  //there was a timeout before receiving signal
           return VFW_S_STATE_INTERMEDIATE;
    }
}



HRESULT Filter::SetSyncSource(
    IReferenceClock* clock)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_clock)
        m_clock->Release();

    m_clock = clock;

    if (m_clock)
        m_clock->AddRef();

    return S_OK;
}


HRESULT Filter::GetSyncSource(
    IReferenceClock** pclock)
{
    if (pclock == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    IReferenceClock*& clock = *pclock;

    clock = m_clock;

    if (clock)
        clock->AddRef();

    return S_OK;
}


HRESULT Filter::EnumPins(IEnumPins** pp)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    const ULONG outpins_count = static_cast<ULONG>(m_outpins.size());
    const ULONG n = 1 + outpins_count;

    //odbgstream os;
    //os << "WebmSplit::filter::enumpins: n=" << n << endl;

    const size_t cb = n * sizeof(IPin*);
    IPin** const pins = (IPin**)_alloca(cb);

    IPin** pin = pins;

    *pin++ = &m_inpin;

    typedef outpins_t::iterator iter_t;

    iter_t i = m_outpins.begin();
    const iter_t j = m_outpins.end();

    while (i != j)
        *pin++ = *i++;

    return CEnumPins::CreateInstance(pins, n, pp);
}



HRESULT Filter::FindPin(
    LPCWSTR id1,
    IPin** pp)
{
    if (pp == 0)
        return E_POINTER;

    IPin*& p = *pp;
    p = 0;

    if (id1 == 0)
        return E_INVALIDARG;

    {
        Pin* const pPin = &m_inpin;

        const wstring& id2_ = pPin->m_id;
        const wchar_t* const id2 = id2_.c_str();

        if (wcscmp(id1, id2) == 0)  //case-sensitive
        {
            p = pPin;
            p->AddRef();

            return S_OK;
        }
    }

    typedef outpins_t::const_iterator iter_t;

    iter_t i = m_outpins.begin();
    const iter_t j = m_outpins.end();

    while (i != j)
    {
        Pin* const pPin = *i++;

        const wstring& id2_ = pPin->m_id;
        const wchar_t* const id2 = id2_.c_str();

        if (wcscmp(id1, id2) == 0)  //case-sensitive
        {
            p = pPin;
            p->AddRef();

            return S_OK;
        }
    }

    return VFW_E_NOT_FOUND;
}



HRESULT Filter::QueryFilterInfo(FILTER_INFO* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    enum { size = sizeof(p->achName)/sizeof(WCHAR) };
    const errno_t e = wcscpy_s(p->achName, size, m_info.achName);
    e;
    assert(e == 0);

    p->pGraph = m_info.pGraph;

    if (p->pGraph)
        p->pGraph->AddRef();

    return S_OK;
}


HRESULT Filter::JoinFilterGraph(
    IFilterGraph *pGraph,
    LPCWSTR name)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //NOTE:
    //No, do not adjust reference counts here!
    //Read the docs for the reasons why.
    //ENDNOTE.

    m_info.pGraph = pGraph;

    if (name == 0)
        m_info.achName[0] = L'\0';
    else
    {
        enum { size = sizeof(m_info.achName)/sizeof(WCHAR) };
        const errno_t e = wcscpy_s(m_info.achName, size, name);
        e;
        assert(e == 0);  //TODO
    }

    return S_OK;
}


HRESULT Filter::QueryVendorInfo(LPWSTR* pstr)
{
    if (pstr == 0)
        return E_POINTER;

    wchar_t*& str = *pstr;

    str = 0;
    return E_NOTIMPL;
}


HRESULT Filter::Open()
{
    if (m_pSegment)
        return VFW_E_WRONG_STATE;

    assert(m_outpins.empty());

    MkvReader& reader = m_inpin.m_reader;

    __int64 result, pos;

    mkvparser::EBMLHeader h;

    result = h.Parse(&reader, pos);

    if (result < 0)  //error
    {
        if (result == mkvparser::E_FILE_FORMAT_INVALID)
            return VFW_E_INVALID_FILE_FORMAT;

        if (result == mkvparser::E_BUFFER_NOT_FULL)
            return VFW_E_BUFFER_UNDERFLOW;

        return VFW_E_RUNTIME_ERROR;
    }

    if (result > 0)
        return VFW_E_BUFFER_UNDERFLOW;

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

    if (h.m_docTypeVersion < 1)
        return VFW_E_INVALID_FILE_FORMAT;

    if (h.m_docTypeReadVersion > 2)
        return VFW_E_INVALID_FILE_FORMAT;

    //Just the EBML header has been consumed.  pos points
    //to start of (first) segment.

    mkvparser::Segment* p;

    result = mkvparser::Segment::CreateInstance(&reader, pos, p);

    if (result < 0)  //error
    {
        if (result == mkvparser::E_FILE_FORMAT_INVALID)
            return VFW_E_INVALID_FILE_FORMAT;

        if (result == mkvparser::E_BUFFER_NOT_FULL)
            return VFW_E_BUFFER_UNDERFLOW;

        return VFW_E_RUNTIME_ERROR;
    }

    if (result > 0)
        return VFW_E_BUFFER_UNDERFLOW;  //TODO: handle this as below

    assert(p);
    std::auto_ptr<mkvparser::Segment> pSegment(p);

#if 0
    result = pSegment->FindFirstCluster(pos);

    if (result < 0)
    {
        if (result == mkvparser::E_FILE_FORMAT_INVALID)
            return VFW_E_INVALID_FILE_FORMAT;

        if (result != mkvparser::E_BUFFER_NOT_FULL)
            return VFW_E_RUNTIME_ERROR;
    }

    //if you're going to do this, why not just a sync read...
    const HRESULT hr = reader.Wait(*this, pos, 1, 5000);

    if (FAILED(hr))
        return hr;
#endif

    result = pSegment->ParseHeaders();

    if (result < 0)  //error
    {
        if (result == mkvparser::E_FILE_FORMAT_INVALID)
            return VFW_E_INVALID_FILE_FORMAT;

        if (result == mkvparser::E_BUFFER_NOT_FULL)
            return VFW_E_BUFFER_UNDERFLOW;

        return VFW_E_RUNTIME_ERROR;
    }

    if (result > 0)
        return VFW_E_BUFFER_UNDERFLOW;

    using namespace mkvparser;

    const SegmentInfo* const pInfo = pSegment->GetInfo();

    if (pInfo == 0)
        return VFW_E_INVALID_FILE_FORMAT;  //TODO: liberalize

#ifdef _DEBUG
    {
        wstring muxingApp, writingApp;

        if (const char* str = pInfo->GetMuxingAppAsUTF8())
            muxingApp = Stream::ConvertFromUTF8(str);

        if (const char* str = pInfo->GetWritingAppAsUTF8())
            writingApp = Stream::ConvertFromUTF8(str);
    }
#endif

    const Tracks* const pTracks = pSegment->GetTracks();

    if (pTracks == 0)
        return VFW_E_INVALID_FILE_FORMAT;

    const ULONG n = pTracks->GetTracksCount();

    for (ULONG i = 0; i < n; ++i)
    {
        const Track* const pTrack = pTracks->GetTrackByIndex(i);

        if (pTrack == 0)
            continue;

        const long long type = pTrack->GetType();

        if (type == 1)  //video
        {
            typedef mkvparser::VideoTrack VT;
            const VT* const t = static_cast<const VT*>(pTrack);

            if (VideoStream* s = VideoStream::CreateInstance(t))
                CreateOutpin(s);
        }
#if 1
        else if (type == 2)  //audio
        {
            typedef mkvparser::AudioTrack AT;
            const AT* const t = static_cast<const AT*>(pTrack);

            if (AudioStream* s = AudioStream::CreateInstance(t))
                CreateOutpin(s);
        }
#endif
    }

    if (m_outpins.empty())
        return VFW_E_INVALID_FILE_FORMAT;  //TODO: better return value here?

    m_pSegment = pSegment.release();
    m_pSeekBase = 0;
    m_seekBase_ns = -1;
    m_currTime = kNoSeek;

    return S_OK;
}


void Filter::CreateOutpin(mkvparser::Stream* s)
{
    //Outpin* const p = new (std::nothrow) Outpin(this, s);
    Outpin* const p = Outpin::Create(this, s);
    m_outpins.push_back(p);
}



void Filter::OnStart()
{
    //m_inpin.Start();

    typedef outpins_t::iterator iter_t;

    iter_t i = m_outpins.begin();
    const iter_t j = m_outpins.end();

    int n = 0;

    while (i != j)
    {
        Outpin* const pPin = *i++;
        assert(pPin);

        const HRESULT hr = pPin->Start();
        assert(SUCCEEDED(hr));

        if (hr == S_OK)
            ++n;
    }

    if (n)
    {
        assert(m_pSegment);
        m_cStarvation = 0;  //temporarily enter starvation mode to force check
    }

    Init();  //create reader thread
}


void Filter::OnStop()
{
    Final();  //terminate reader thread

    typedef outpins_t::iterator iter_t;

    iter_t i = m_outpins.begin();
    const iter_t j = m_outpins.end();

    while (i != j)
    {
        Outpin* const pPin = *i++;
        assert(pPin);

        pPin->Stop();
    }

    //m_inpin.Stop();
}


int Filter::GetConnectionCount() const
{
    //filter already locked by caller

    int n = 0;

    typedef outpins_t::const_iterator iter_t;

    iter_t i = m_outpins.begin();
    const iter_t j = m_outpins.end();

    while (i != j)
    {
        const Outpin* const pin = *i++;
        assert(pin);

        if (pin->m_pPinConnection)
            ++n;
    }

    return n;
}


unsigned Filter::ThreadProc(void* pv)
{
    Filter* const pFilter = static_cast<Filter*>(pv);
    assert(pFilter);

    return pFilter->Main();
}


unsigned Filter::Main()
{
    assert(m_pSegment);

    for (;;)
    {
        Sleep(0);

#if 0
        LONGLONG cluster_pos, new_pos;

        const long status = m_pSegment->ParseCluster(cluster_pos, new_pos);

        if (status < 0)  //TODO: how to handle outpin streaming threads?
            return 1;

        Lock lock;

        const HRESULT hr = lock.Seize(this);
        assert(SUCCEEDED(hr));  //TODO

        if (FAILED(hr))
            return 1;

        const bool bDone = m_pSegment->AddCluster(cluster_pos, new_pos);

        //odbgstream os;
        //os << "webmsplit::filter::main: ParseCluster; cluster_pos="
        //   << cluster_pos
        //   << " new_pos="
        //   << new_pos
        //   << " count="
        //   << m_pSegment->GetCount()
        //   << " unparsed="
        //   << m_pSegment->Unparsed()
        //   << endl;
#else
        Lock lock;

        HRESULT hr = lock.Seize(this);

        if (FAILED(hr))
            return 1;

        for (;;)
        {
            LONGLONG pos;
            LONG size;

            const long status = m_pSegment->LoadCluster(pos, size);

            if (status >= 0)
                break;

            if (status != mkvparser::E_BUFFER_NOT_FULL)
                return 1;

            hr = m_inpin.m_reader.Wait(*this, pos, size, INFINITE);

            if (FAILED(hr))  //wait was cancelled
                return 1;
        }

        const bool bDone = m_pSegment->DoneParsing();
#endif

        OnNewCluster();

        if (bDone)
            return 0;

        if (m_state == State_Stopped)
            return 0;
    }
}


void Filter::OnNewCluster()
{
    const BOOL b = SetEvent(m_hNewCluster);  //see Filter::GetState
    b;
    assert(b);

    typedef outpins_t::iterator iter_t;

    iter_t i = m_outpins.begin();
    const iter_t j = m_outpins.end();

    while (i != j)
    {
        Outpin* const outpin = *i++;
        assert(outpin);

        outpin->OnNewCluster();
    }
}


HRESULT Filter::OnDisconnectInpin()
{
    assert(m_hThread == 0);

    while (!m_outpins.empty())
    {
        Outpin* const pPin = m_outpins.back();
        assert(pPin);

        if (IPin* pPinConnection = pPin->m_pPinConnection)
        {
            assert(m_info.pGraph);

            HRESULT hr = m_info.pGraph->Disconnect(pPinConnection);
            assert(SUCCEEDED(hr));

            hr = m_info.pGraph->Disconnect(pPin);
            assert(SUCCEEDED(hr));
        }

        m_outpins.pop_back();

        const ULONG n = pPin->Destroy();
        n;
    }

    m_currTime = kNoSeek;
    m_pSeekBase = 0;
    m_seekBase_ns = -1;

    delete m_pSegment;
    m_pSegment = 0;

    m_cStarvation = -1;
    return S_OK;
}


void Filter::OnStarvation(ULONG count)
{
#ifdef _DEBUG
    odbgstream os;
    os << "WebmSplit::Filter::OnStarvation: count=" << count
       << " m_cStarvation=" << m_cStarvation
       << endl;
#endif

    if (m_cStarvation < 0)
    {
        const GraphUtil::IMediaEventSinkPtr pSink(m_info.pGraph);
        assert(bool(pSink));

        const HRESULT hr = pSink->Notify(EC_STARVATION, 0, 0);
        hr;
        assert(SUCCEEDED(hr));

        m_cStarvation = count;
    }
}


void Filter::SetCurrPosition(
    LONGLONG currTime,
    DWORD dwCurr,
    Outpin* pOutpin)
{
    assert(pOutpin);
    assert(bool(pOutpin->m_pPinConnection));

    using namespace mkvparser;

    Stream* const pSeekStream = pOutpin->GetStream();
    assert(pSeekStream);

    if (m_currTime == currTime)
    {
        SetCurrPositionUsingSameTime(pSeekStream);
        return;
    }

    m_currTime = currTime;

    if (InCache())
        m_pSegment->LoadCluster();

    if (m_pSegment->GetCount() <= 0)  //no clusters loaded yet
    {
        m_pSeekBase = 0;  //best we can do is to assume first cluster
        m_seekBase_ns = -1;
        m_seekTime_ns = -1;

        pSeekStream->SetCurrPosition(-1, 0);  //lazy init
        return;
    }

    const LONGLONG ns = pSeekStream->GetSeekTime(currTime, dwCurr);
    const Track* const pSeekTrack = pSeekStream->m_pTrack;

    if (pSeekTrack->GetType() == 1)  //video
        SetCurrPositionVideo(ns, pSeekStream);
    else
        SetCurrPositionAudio(ns, pSeekStream);
}


void Filter::SetCurrPositionUsingSameTime(mkvparser::Stream* pStream)
{
    const mkvparser::BlockEntry* pCurr;

    const mkvparser::Track* const pTrack = pStream->m_pTrack;

    if (m_pSeekBase == 0)  //lazy init
        pCurr = 0;

    else if (m_pSeekBase->EOS())
        pCurr = pTrack->GetEOS();

    else
    {
        pCurr = m_pSeekBase->GetEntry(pTrack, m_seekTime_ns);

#ifdef _DEBUG
        if (pCurr == 0)
            __noop;

        else if (pCurr->EOS())
            __noop;

        else if (pTrack->GetType() == 1)  //video
        {
            const mkvparser::Block* const pCurrBlock = pCurr->GetBlock();
            const LONGLONG ns = pCurrBlock->GetTime(pCurr->GetCluster());
            assert(ns >= m_seekBase_ns);
            assert(pCurrBlock->IsKey());
        }
#endif
    }

    pStream->SetCurrPosition(m_seekBase_ns, pCurr);
}


void Filter::SetCurrPositionVideo(
    LONGLONG ns,
    mkvparser::Stream* pStream)
{
    using namespace mkvparser;
    const Track* const pTrack = pStream->m_pTrack;

    const bool bInCache = InCache();

    if (!bInCache)
        __noop;
    else if (const Cues* pCues = m_pSegment->GetCues())
    {
        while (!pCues->DoneParsing())
        {
            pCues->LoadCuePoint();

            const CuePoint* const pCP = pCues->GetLast();
            assert(pCP);

            if (pCP->GetTime(m_pSegment) >= ns)
                break;
        }

        const CuePoint* pCP;
        const CuePoint::TrackPosition* pTP;

        if (pCues->Find(ns, pTrack, pCP, pTP))
        {
            const BlockEntry* const pCurr = pCues->GetBlock(pCP, pTP);

            if ((pCurr != 0) && !pCurr->EOS())
            {
                m_pSeekBase = pCurr->GetCluster();
                m_seekBase_ns = pCurr->GetBlock()->GetTime(m_pSeekBase);
                m_seekTime_ns = m_seekBase_ns;

                pStream->SetCurrPosition(m_seekBase_ns, pCurr);
                return;
            }
        }
    }

    const mkvparser::BlockEntry* pCurr = 0;

    for (;;)
    {
        long status = pTrack->Seek(ns, pCurr);

        if ((status >= 0) ||
            (status != mkvparser::E_BUFFER_NOT_FULL) ||
            !bInCache)
        {
            break;
        }

        status = m_pSegment->LoadCluster();

        if (status < 0)
            break;
    }

    if ((pCurr == 0) || pCurr->EOS())
    {
        m_pSeekBase = &m_pSegment->m_eos;
        m_seekBase_ns = -1;
        m_seekTime_ns = -1;
    }
    else
    {
        m_pSeekBase = pCurr->GetCluster();
        m_seekBase_ns = pCurr->GetBlock()->GetTime(m_pSeekBase);
        m_seekTime_ns = m_seekBase_ns;
    }

    pStream->SetCurrPosition(m_seekBase_ns, pCurr);
}


void Filter::SetCurrPositionAudio(
    LONGLONG ns,
    mkvparser::Stream* pSeekStream)
{
    using namespace mkvparser;
    const Track* const pSeekTrack = pSeekStream->m_pTrack;

    typedef outpins_t::const_iterator iter_t;

    iter_t i = m_outpins.begin();
    const iter_t j = m_outpins.end();

    mkvparser::Stream* pVideoStream = 0;

    while (i != j)
    {
        const Outpin* const pin = *i++;
        assert(pin);

        if (!bool(pin->m_pPinConnection))
            continue;

        const AM_MEDIA_TYPE& mt = pin->m_connection_mtv[0];
        const BOOL bVideo = (mt.majortype == MEDIATYPE_Video);

        if (bVideo)
        {
            pVideoStream = pin->GetStream();
            assert(pVideoStream);
            assert(pVideoStream != pSeekStream);

            break;
        }
    }

    if (pVideoStream == 0)  //no video tracks in this file
    {
        const mkvparser::BlockEntry* pCurr;
        const long status = pSeekTrack->Seek(ns, pCurr);

        if ((status < 0) || (pCurr == 0) || pCurr->EOS())
        {
            m_pSeekBase = &m_pSegment->m_eos;
            m_seekBase_ns = -1;
            m_seekTime_ns = -1;
        }
        else
        {
            m_pSeekBase = pCurr->GetCluster();
            m_seekBase_ns = m_pSeekBase->GetFirstTime();
            m_seekTime_ns = ns;
        }

        pSeekStream->SetCurrPosition(m_seekBase_ns, pCurr);
        return;
    }

    const mkvparser::Track* const pVideoTrack = pVideoStream->m_pTrack;
    assert(pVideoTrack->GetType() == 1);  //video

    const bool bInCache = InCache();

    if (!bInCache)
        __noop;
    else if (const Cues* pCues = m_pSegment->GetCues())
    {
        while (!pCues->DoneParsing())
        {
            pCues->LoadCuePoint();

            const CuePoint* const pCP = pCues->GetLast();
            assert(pCP);

            if (pCP->GetTime(m_pSegment) >= ns)
                break;
        }

        const CuePoint* pCP;
        const CuePoint::TrackPosition* pTP;

        if (pCues->Find(ns, pVideoTrack, pCP, pTP))
        {
            const BlockEntry* pCurr = pCues->GetBlock(pCP, pTP);

            if ((pCurr != 0) && !pCurr->EOS())
            {
                m_pSeekBase = pCurr->GetCluster();
                m_seekBase_ns = pCurr->GetBlock()->GetTime(m_pSeekBase);
                m_seekTime_ns = m_seekBase_ns;  //to find same block later

                pCurr = m_pSeekBase->GetEntry(pSeekTrack, m_seekBase_ns);
                assert(pCurr);

                if (!pCurr->EOS())
                    m_seekBase_ns = pCurr->GetBlock()->GetTime(m_pSeekBase);

                pSeekStream->SetCurrPosition(m_seekBase_ns, pCurr);
                return;
            }
        }
    }

    const BlockEntry* pCurr = 0;

    for (;;)
    {
        long status = pVideoTrack->Seek(ns, pCurr);

        if ((status >= 0) ||
            (status != mkvparser::E_BUFFER_NOT_FULL) ||
            !bInCache)
        {
            break;
        }

        status = m_pSegment->LoadCluster();

        if (status < 0)
            break;
    }

    if ((pCurr == 0) || pCurr->EOS())
    {
        m_pSeekBase = &m_pSegment->m_eos;
        m_seekBase_ns = -1;
        m_seekTime_ns = -1;

        pCurr = pSeekTrack->GetEOS();
        pSeekStream->SetCurrPosition(m_seekBase_ns, pCurr);
        return;
    }

    m_pSeekBase = pCurr->GetCluster();
    m_seekBase_ns = pCurr->GetBlock()->GetTime(m_pSeekBase);
    m_seekTime_ns = m_seekBase_ns;  //to find same block later

    pCurr = m_pSeekBase->GetEntry(pSeekTrack, m_seekBase_ns);
    assert(pCurr);

    if (!pCurr->EOS())
        m_seekBase_ns = pCurr->GetBlock()->GetTime(m_pSeekBase);

    pSeekStream->SetCurrPosition(m_seekBase_ns, pCurr);
}


bool Filter::InCache()
{
    LONGLONG total, avail;

    const int status = m_inpin.m_reader.Length(&total, &avail);

    if (status < 0)
        return false;

    if (total < 0)
        return false;

    return (avail >= total);
}


} //end namespace WebmSplit

