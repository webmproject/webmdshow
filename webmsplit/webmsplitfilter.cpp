// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include <uuids.h>
#include "webmsplitfilter.hpp"
#include "cenumpins.hpp"
#include "webmsplitoutpin.hpp"
#include "mkvparser.hpp"
#include "mkvparserstreamvideo.hpp"
#include "mkvparserstreamaudio.hpp"
#include "webmtypes.hpp"
#include <new>
#include <cassert>
#include <vfwmsgs.h>
#include <process.h>
#include <evcode.h>
#include <limits>
#ifdef _DEBUG
#include "iidstr.hpp"
#include "odbgstream.hpp"
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
      m_seekTime(kNoSeek),
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
        
    if (m_pSegment->Unparsed() <= 0)
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

    //odbgstream os;

    Lock lock;
    
    HRESULT hr = lock.Seize(this);
    
    if (FAILED(hr))
        return hr;
        
    //odbgstream os;
    //os << "WebmSplit::Filter::Stop" << endl;

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
            
            //os << "WebmSplit::filter::stop: calling Release" << endl;

            m_state = State_Stopped;

            lock.Release();
            
            //os << "WebmSplit::filter::stop: called Release; calling OnStop" << endl;
            
            OnStop();            
            
            //os << "WebmSplit::filter::stop: called OnStop; calling Seize" << endl;
                
            hr = lock.Seize(this);
            assert(SUCCEEDED(hr));  //TODO
            
            //os << "WebmSplit::filter::stop: Seize called" << endl;

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
        
    //odbgstream os;
        
    if (m_cStarvation < 0)  //not starving
    {    
        //os << "\nWebmSplit::Filter::GetState: NOT STARVING\n" << endl;

        state = m_state;
        return S_OK;
    }
    
    assert(m_pSegment);

    long count = m_pSegment->GetCount();

    if (count > m_cStarvation)
    {
        //os << "\nWebmSplit::Filter::GetState: cStarvation=" << m_cStarvation
        //   << " clusters.count=" << count
        //   << "; EXITING STARVATION MODE\n"
        //   << endl;

        m_cStarvation = -1;

        state = m_state;  //TODO: should be State_Paused?
        return S_OK;
    }

    for (;;)
    {
        //os << "\nWebmSplit::Filter::GetState: cStarvation=" << m_cStarvation
        //   << " clusters.count=" << count
        //   << " timeout=" << timeout
        //   << "; WAITING FOR SIGNAL\n"
        //   << endl;

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
        else if (hrWait != RPC_S_CALLPENDING) //despite the "S" in this name, this is an error                        
            return hrWait;
            
        hrLock = lock.Seize(this);

        if (FAILED(hrLock))
            return E_FAIL;

        count = m_pSegment->GetCount();
        
        if (count > m_cStarvation)
        {
            //os << "\nWebmSplit::Filter::GetState(cont'd): cStarvation=" << m_cStarvation
            //   << " clusters.count=" << count
            //   << "; EXITING STARVATION MODE\n"
            //   << endl;

            m_cStarvation = -1;

            state = m_state;  //TODO: should be State_Paused?
            return S_OK;
        }
        
        if (FAILED(hrWait))  //there was a timeout before receiving signal
        {
            //os << "\nWebmSplit::Filter::GetState(cont'd): cStarvation=" << m_cStarvation
            //   << " clusters.count=" << count
            //   << "; INTERMEDIATE FILTER STATE\n"
            //   << endl;

           return VFW_S_STATE_INTERMEDIATE;
        }
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


HRESULT Filter::Open(MkvParser::IMkvFile* pFile)
{
    if (m_pSegment)
        return VFW_E_WRONG_STATE;
        
    assert(pFile);
    assert(m_outpins.empty());
    
    __int64 result, pos;
    
    //TODO: must initialize header to defaults
    
    MkvParser::EBMLHeader h;
    
    result = h.Parse(pFile, pos);
    
    if (result < 0)  //error
        return static_cast<HRESULT>(result);
        
    if (result > 0)  //need more data
        return VFW_E_BUFFER_UNDERFLOW;  //require full header 
        
    if (h.m_version > 1)
        return VFW_E_INVALID_FILE_FORMAT;
        
    if (h.m_maxIdLength > 8)
        return VFW_E_INVALID_FILE_FORMAT;
    
    if (h.m_maxSizeLength > 8)
        return VFW_E_INVALID_FILE_FORMAT;
        
    if (_stricmp(h.m_docType.c_str(), "webm") == 0)
        __noop;
    else if (_stricmp(h.m_docType.c_str(), "matroska") == 0)
        __noop;
    else
        return VFW_E_INVALID_FILE_FORMAT;
        
    if (h.m_docTypeVersion > 2)
        return VFW_E_INVALID_FILE_FORMAT;
        
    if (h.m_docTypeReadVersion > 2)
        return VFW_E_INVALID_FILE_FORMAT;

    //Just the EBML header has been consumed.  pos points
    //to start of (first) segment.
    
    MkvParser::Segment* p;
    
    result = MkvParser::Segment::CreateInstance(pFile, pos, p);
    
    if (result < 0)
        return static_cast<HRESULT>(result);
        
    if (result > 0)
        return VFW_E_BUFFER_UNDERFLOW;
        
    assert(p);
    
    std::auto_ptr<MkvParser::Segment> pSegment(p);
    
    result = pSegment->ParseHeaders();
    
    if (result < 0)
        return static_cast<HRESULT>(result);
        
    if (result > 0)
        return VFW_E_BUFFER_UNDERFLOW;
    
    const MkvParser::Tracks* const pTracks = pSegment->GetTracks();
        
    if (pTracks == 0)
        return VFW_E_INVALID_FILE_FORMAT;
        
    const MkvParser::SegmentInfo* const pInfo = pSegment->GetInfo();
    
    if (pInfo == 0)
        return VFW_E_INVALID_FILE_FORMAT;  //TODO: liberalize
    
    using MkvParser::VideoTrack;
    using MkvParser::VideoStream;

    using MkvParser::AudioTrack;
    using MkvParser::AudioStream;
    
    using MkvParser::Stream;
    
    typedef Stream::TCreateOutpins<VideoTrack, VideoStream, Filter> EV;
    typedef Stream::TCreateOutpins<AudioTrack, AudioStream, Filter> EA;
    
    const EV ev(this, &VideoStream::CreateInstance);
    pTracks->EnumerateVideoTracks(ev);
    
    const EA ea(this, &AudioStream::CreateInstance);
    pTracks->EnumerateAudioTracks(ea);
    
    if (m_outpins.empty())
        return VFW_E_INVALID_FILE_FORMAT;  //TODO: better return value here?
        
    //ALLOCATOR_PROPERTIES props;
    //props.cbBuffer = GetMaxBufferSize();
    //props.cbAlign = 1;
    //props.cbPrefix = 0;
    //props.cBuffers = 1;
    
    //HRESULT hr = pReader->RequestAllocator(0, &props, &m_pAllocator);
    //assert(SUCCEEDED(hr));  //TODO
    //assert(bool(m_pAllocator));
    
    m_pSegment = pSegment.release();
    m_pSeekBase = 0;
    m_seekTime = kNoSeek;

    return S_OK;
}


void Filter::CreateOutpin(MkvParser::Stream* s)
{
    //Outpin* const p = new (std::nothrow) Outpin(this, s);
    Outpin* const p = Outpin::Create(this, s);    
    m_outpins.push_back(p);
}



void Filter::OnStart()
{
    //TODO: init inpin
    
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
    
    Init();
}


void Filter::OnStop()
{
    Final();
    
    typedef outpins_t::iterator iter_t;
    
    iter_t i = m_outpins.begin();
    const iter_t j = m_outpins.end();
    
    while (i != j)
    {
        Outpin* const pPin = *i++;
        assert(pPin);
        
        pPin->Stop();
    }
    
    //TODO: final inpin
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
    const __int64 stop = m_pSegment->m_start + m_pSegment->m_size;
    stop;
    
    //odbgstream os;
    //os << "WebmSplit::filter::main: thread running" << endl;
    
    for (;;)
    {
        Sleep(0);
        
        MkvParser::Cluster* pCluster;
        __int64 pos;
        
        HRESULT hr = m_pSegment->ParseCluster(pCluster, pos);
        assert(SUCCEEDED(hr));  //TODO
        assert((hr != S_OK) || (pCluster != 0));
        
        //os << "WebmSplit::filter::main: cluster=0x" << (void*)pCluster
        //   << " pos=" << pos
        //   << "/" << stop
        //   << " hr=0x" << hex << hr << dec
        //   << endl;
           
        if (FAILED(hr))  //TODO: how to handle outpin streaming threads?
            return 1;
            
        Lock lock;
        
        hr = lock.Seize(this);
        assert(SUCCEEDED(hr));  //TODO

        if (FAILED(hr))
            return 1;  //TODO: pCluster != 0 => memory leak
            
        const bool bDone = m_pSegment->AddCluster(pCluster, pos);
        
        //os << "WebmSplit::filter::main: AddCluster; newcount="
        //   << m_pSegment->GetCount()
        //   << endl;
           
        OnNewCluster();

        if (bDone)
        {
            //os << "WebmSplit::filter::main: AddCluster returned Done; EXITING" << endl;
            return 0;
        }
            
        if (m_state == State_Stopped)
        {
            //os << "WebmSplit::filter::main: state=Stopped; EXITING" << endl;
            return 0;
        }
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
    
    //wodbgstream os;
    //os << "WebmSplit::Filter::OnDisconnectInpin(begin)" << endl;
    
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
        
        //os << "WebmSplit::Filter::OnDisconnectInpin(cont'd): destroying outpin["
        //   << pPin->m_id
        //   << "]"
        //   << endl;

        const ULONG n = pPin->Destroy();
        n;

        //os << "WebmSplit::Filter::OnDisconnectInpin(cont'd): destroyed outpin"
        //   << "; n=" << n
        //   << endl;
    }
    
    m_seekTime = kNoSeek;
    m_pSeekBase = 0;

    delete m_pSegment;
    m_pSegment = 0;
    
    m_cStarvation = -1;

    //os << "WebmSplit::Filter::OnDisconnectInpin(end)" << endl;
    
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
    
    MkvParser::Stream* const pSeekStream = pOutpin->GetStream();
    assert(pSeekStream);

    //odbgstream os;
        
    if (m_seekTime == currTime)
    {
        pSeekStream->SetCurrPosition(m_pSeekBase);
        //os << "mkvsource::filter::setcurrpos: seektime=currtime #1" << endl;
        return;
    }

    if (pSeekStream->m_pTrack->GetType() == 1)  //video    
    {
        const AM_MEDIA_TYPE& mt = pOutpin->m_connection_mtv[0];
        const BOOL bVideo = (mt.majortype == MEDIATYPE_Video);
        bVideo;
        assert(bVideo);

        m_pSeekBase = pSeekStream->SetCurrPosition(currTime, dwCurr);
        m_seekTime = currTime;        
        //os << "mkvsource::filter::setcurrpos: outpin is video #2" << endl;
        return;
    }
    
    typedef outpins_t::const_iterator iter_t;
    
    iter_t i = m_outpins.begin();
    const iter_t j = m_outpins.end();
    
    while (i != j)
    {
        const Outpin* const pin = *i++;
        assert(pin);
        
        if (!bool(pin->m_pPinConnection))
            continue;
        
        const AM_MEDIA_TYPE& mt = pin->m_connection_mtv[0];
        const BOOL bVideo = (mt.majortype == MEDIATYPE_Video);
        
        if (!bVideo)
            continue;
            
        MkvParser::Stream* const pStream = pin->GetStream();
        assert(pStream);
        assert(pStream != pSeekStream);
        assert(pStream->m_pTrack->GetType() == 1);  //video
        
        const LONGLONG ns = pSeekStream->GetSeekTime(currTime, dwCurr);
        m_pSeekBase = pStream->GetSeekBase(ns);
        m_seekTime = currTime;
        
        //os << "mkvsource::filter::setcurrpos: searched for and found video pin;"
        //   << " timecode[ns]=" << ns
        //   << " seekbase.time[ns]="
        //   << ((m_pSeekBase == 0) ? -42 : m_pSeekBase->GetTime())
        //   << " #3a"
        //   << endl;

        pSeekStream->SetCurrPosition(m_pSeekBase);

        //os << "mkvsource::filter::setcurrpos: searched for and found video pin #3b" << endl;
        return;
    }
    
    //os << "mkvsource::filter::setcurrpos: searched for but did not find video pin #4" << endl;

    m_pSeekBase = pSeekStream->SetCurrPosition(currTime, dwCurr);
    m_seekTime = currTime;        
}



} //end namespace WebmSplit

