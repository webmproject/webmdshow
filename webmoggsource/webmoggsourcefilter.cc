// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include <uuids.h>
#include "webmoggsourcefilter.hpp"
#include "cenumpins.hpp"
#include "webmoggsourceoutpin.hpp"
#include "webmtypes.hpp"
#include "oggtrackaudio.hpp"
#include <new>
#include <cassert>
#include <vfwmsgs.h>
#include <process.h>
#include <limits>
#ifdef _DEBUG
#include "iidstr.hpp"
#include "odbgstream.hpp"
using std::endl;
#endif

using std::wstring;
using namespace oggparser;

namespace WebmOggSource
{

//const LONGLONG Filter::kNoSeek(std::numeric_limits<LONGLONG>::min());



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
      //m_file
      m_stream(&m_file)
      //m_pSeekBase(0),
      //m_seekBase_ns(-1),
      //m_currTime(kNoSeek)
{
    m_pClassFactory->LockServer(TRUE);

    const HRESULT hr = CLockable::Init();
    hr;
    assert(SUCCEEDED(hr));

    m_info.pGraph = 0;
    m_info.achName[0] = L'\0';

#ifdef _DEBUG
    odbgstream os;
    os << "webmoggsrc::ctor" << endl;
#endif
}
#pragma warning(default:4355)


Filter::~Filter()
{
#ifdef _DEBUG
    odbgstream os;
    os << "webmoggsrc::dtor" << endl;
#endif

    while (!m_pins.empty())
    {
        Outpin* p = m_pins.back();
        assert(p);

        m_pins.pop_back();
        delete p;
    }

    //delete m_pStream;

    m_pClassFactory->LockServer(FALSE);
}


Filter::nondelegating_t::nondelegating_t(Filter* p)
    : m_pFilter(p),
      m_cRef(0)  //see CreateInstance
{
}


Filter::nondelegating_t::~nondelegating_t()
{
}


HRESULT Filter::nondelegating_t::QueryInterface(
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
    else if (iid == __uuidof(IFileSourceFilter))
    {
        pUnk = static_cast<IFileSourceFilter*>(m_pFilter);
    }
    else if (iid == __uuidof(IAMFilterMiscFlags))
    {
        pUnk = static_cast<IAMFilterMiscFlags*>(m_pFilter);
    }
    else
    {
#if 0
        wodbgstream os;
        os << "webmsource::filter::QI: iid=" << IIDStr(iid) << std::endl;
#endif
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG Filter::nondelegating_t::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}


ULONG Filter::nondelegating_t::Release()
{
    if (LONG n = InterlockedDecrement(&m_cRef))
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

    *p = WebmTypes::CLSID_WebmOggSource;
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

            lock.Release();

            OnStop();

            hr = lock.Seize(this);
            assert(SUCCEEDED(hr));  //TODO

            break;

        case State_Stopped:
        default:
            break;
    }

    m_state = State_Stopped;
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
    DWORD /* timeout */ ,
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

    const HRESULT hr = lock.Seize(this);

    //The lock is only used for synchronization.  If Seize fails,
    //it means there's a serious problem with the filter.

    if (FAILED(hr))
        return E_FAIL;

    *p = m_state;
    return S_OK;
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

    if (m_pins.empty())
        return CEnumPins::CreateInstance(0, 0, pp);

    Outpin* const* const i = &m_pins[0];
    const ULONG n = static_cast<ULONG>(m_pins.size());

    return CEnumPins::CreateInstance<Outpin>(i, n, pp);
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

    typedef pins_t::const_iterator iter_t;

    iter_t i = m_pins.begin();
    const iter_t j = m_pins.end();

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
        //const errno_t e = wcscpy_s(m_info.achName, size, name);
        //e;
        //assert(e == 0);

        //if (wcslen(name) >= size)
        //    return E_INVALIDARG;

        const wchar_t* src = name;

        wchar_t* const dst_begin = m_info.achName;
        wchar_t* const dst_end = dst_begin + size - 1;

        wchar_t* dst = dst_begin;

        while ((dst < dst_end) && (*dst++ = *src++))
            ;

        *dst++ = L'\0';

        const size_t size_ = dst - dst_begin;
        size_;
        assert(size_ <= size);
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


HRESULT Filter::Load(LPCOLESTR filename, const AM_MEDIA_TYPE* pmt)
{
    pmt;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_file.IsOpen())
        return E_UNEXPECTED;

    //assert(m_pStream == 0);

    hr = m_file.Open(filename);

    if (FAILED(hr))
        return hr;

    //OggStream* pStream;

    //const long status = OggStream::Create(&m_file, pStream);

    //if (status < 0)  //error
    //    return E_FAIL;

    hr = OggInit();

    if (FAILED(hr))
    {
        m_file.Close();
        return hr;
    }

    m_filename = filename;
    //m_pStream = pStream;

    return S_OK;
}


HRESULT Filter::OggInit()
{
    assert(m_file.IsOpen());
    assert(m_pins.empty());

#if 0
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
        else if (type == 2)  //audio
        {
            typedef mkvparser::AudioTrack AT;
            const AT* const t = static_cast<const AT*>(pTrack);

            if (AudioStream* s = AudioStream::CreateInstance(t))
                CreateOutpin(s);
        }
    }

    if (m_pins.empty())
        return VFW_E_INVALID_FILE_FORMAT;  //TODO: better return value here?

    //m_pSeekBase = 0;
    //m_seekBase_ns = -1;
    //m_currTime = kNoSeek;

#else

    //TODO: for now, just hard-code a single Vorbis audio track, since that's
    //all our OggStream parser supports anyway.  If there's interest in
    //generalizing this component to work with an ogg container having
    //multiple tracks ("logical bitreams", in Ogg parlance), then we can just
    //add support for that later.

    //TODO: use logical bitstream id instead of hard-coding 1
    //OggTrackAudio* const pTrack = new OggTrackAudio(&m_stream, 1);

    OggTrackAudio* pTrack;

    HRESULT hr = OggTrackAudio::Create(&m_stream, pTrack);

    if (FAILED(hr))
        return hr;

    Outpin* const pOutpin = new Outpin(this, pTrack);

    m_pins.push_back(pOutpin);

#endif

    return S_OK;
}


#if 0  //TODO

void Filter::CreateOutpin(mkvparser::Stream* s)
{
    Outpin* const p = new (std::nothrow) Outpin(this, s);
    m_pins.push_back(p);
}

#endif  //TODO


HRESULT Filter::GetCurFile(LPOLESTR* pname, AM_MEDIA_TYPE* pmt)
{
    if (pmt)
        memset(pmt, 0, sizeof(AM_MEDIA_TYPE));  //TODO

    if (pname == 0)
        return E_POINTER;

    wchar_t*& name = *pname;
    name = 0;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (!m_file.IsOpen())
        return S_FALSE;

    const size_t len = m_filename.length();
    const size_t size = len + 1;
    const size_t cb = size * sizeof(wchar_t);

    name = (wchar_t*)CoTaskMemAlloc(cb);

    if (name == 0)
        return E_OUTOFMEMORY;

    const errno_t e = wcscpy_s(name, size, m_filename.c_str());
    e;
    assert(e == 0);

    return S_OK;
}


ULONG Filter::GetMiscFlags()
{
    return AM_FILTER_MISC_FLAGS_IS_SOURCE;
}


void Filter::OnStart()
{
    typedef pins_t::iterator iter_t;

    iter_t i = m_pins.begin();
    const iter_t j = m_pins.end();

    while (i != j)
    {
        Outpin* const pPin = *i++;
        assert(pPin);

        pPin->Init();
    }

    //Init();
}


void Filter::OnStop()
{
    //Final();

    typedef pins_t::iterator iter_t;

    iter_t i = m_pins.begin();
    const iter_t j = m_pins.end();

    while (i != j)
    {
        Outpin* const pPin = *i++;
        assert(pPin);

        pPin->Final();
    }
}


#if 0

int Filter::GetConnectionCount() const
{
    //filter already locked by caller

    int n = 0;

    typedef pins_t::const_iterator iter_t;

    iter_t i = m_pins.begin();
    const iter_t j = m_pins.end();

    while (i != j)
    {
        const Outpin* const pin = *i++;
        assert(pin);

        if (pin->m_connection)
            ++n;
    }

    return n;
}


void Filter::SetCurrPosition(
    LONGLONG currTime,
    DWORD dwCurr,
    Outpin* pOutpin)
{
    assert(pOutpin);
    assert(pOutpin->m_connection);

    using namespace mkvparser;

    Stream* const pOutpinStream = pOutpin->m_pStream;
    const Track* const pOutpinTrack = pOutpinStream->m_pTrack;

    if (m_currTime == currTime)
    {
       const BlockEntry* pCurr;

        if (m_pSeekBase == 0)  //lazy init
            pCurr = 0;
        else if (m_pSeekBase->EOS())
            pCurr = pOutpinTrack->GetEOS();
        else
            pCurr = m_pSeekBase->GetEntry(pOutpinTrack, m_seekTime_ns);

        pOutpinStream->SetCurrPosition(m_seekBase_ns, pCurr);
        return;
    }

    m_currTime = currTime;
    const LONGLONG ns = pOutpinStream->GetSeekTime(currTime, dwCurr);

    long status = m_pSegment->LoadCluster();
    assert(status >= 0);  //TODO

    if (pOutpinTrack->GetType() == 1)  //video
    {
        const AM_MEDIA_TYPE& mt = pOutpin->m_connection_mtv[0];
        const BOOL bVideo = (mt.majortype == MEDIATYPE_Video);
        bVideo;
        assert(bVideo);

        if (const Cues* pCues = m_pSegment->GetCues())
        {
            const CuePoint* pCP;
            const CuePoint::TrackPosition* pTP;

            if (pCues->Find(ns, pOutpinTrack, pCP, pTP))
            {
                const BlockEntry* const pCurr = pCues->GetBlock(pCP, pTP);

                if ((pCurr != 0) && !pCurr->EOS())
                {
                    m_pSeekBase = pCurr->GetCluster();
                    m_seekBase_ns = pCurr->GetBlock()->GetTime(m_pSeekBase);
                    m_seekTime_ns = m_seekBase_ns;

                    pOutpinStream->SetCurrPosition(m_seekBase_ns, pCurr);
                    return;
                }
            }
        }

        const BlockEntry* pCurr;

        for (;;)
        {
            status = pOutpinTrack->Seek(ns, pCurr);

            if (status >= 0)
                break;

            assert(status == mkvparser::E_BUFFER_NOT_FULL);

            status = m_pSegment->LoadCluster();
            assert(status >= 0);
        }

        assert(pCurr);

        if (pCurr->EOS())  //pathological
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

        pOutpinStream->SetCurrPosition(m_seekBase_ns, pCurr);
        return;
    }

    typedef pins_t::const_iterator iter_t;

    iter_t i = m_pins.begin();
    const iter_t j = m_pins.end();

    while (i != j)
    {
        const Outpin* const pin = *i++;
        assert(pin);

        if (pin->m_connection == 0)
            continue;

        const AM_MEDIA_TYPE& mt = pin->m_connection_mtv[0];
        const BOOL bVideo = (mt.majortype == MEDIATYPE_Video);

        if (!bVideo)
            continue;

        Stream* const pStream = pin->m_pStream;
        assert(pStream);
        assert(pStream != pOutpinStream);

        const Track* const pVideoTrack = pStream->m_pTrack;
        assert(pVideoTrack->GetType() == 1);  //video

        if (const Cues* pCues = m_pSegment->GetCues())
        {
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

                    pCurr = m_pSeekBase->GetEntry(pOutpinTrack, m_seekBase_ns);
                    assert(pCurr);

                    if (!pCurr->EOS())
                    {
                        const Block* const pBlock = pCurr->GetBlock();
                        assert(pBlock);

                        m_seekBase_ns = pBlock->GetTime(m_pSeekBase);
                    }

                    pOutpinStream->SetCurrPosition(m_seekBase_ns, pCurr);
                    return;
                }
            }
        }

        const BlockEntry* pCurr;

        for (;;)
        {
            status = pVideoTrack->Seek(ns, pCurr);

            if (status >= 0)
                break;

            assert(status == mkvparser::E_BUFFER_NOT_FULL);

            status = m_pSegment->LoadCluster();
            assert(status >= 0);
        }

        if (pCurr->EOS())  //pathological
        {
            m_pSeekBase = &m_pSegment->m_eos;
            m_seekBase_ns = -1;
            m_seekTime_ns = -1;

            pCurr = pOutpinTrack->GetEOS();
            pOutpinStream->SetCurrPosition(m_seekBase_ns, pCurr);
            return;
        }

        m_pSeekBase = pCurr->GetCluster();
        m_seekBase_ns = pCurr->GetBlock()->GetTime(m_pSeekBase);
        m_seekTime_ns = m_seekBase_ns;  //to find same block later

        pCurr = m_pSeekBase->GetEntry(pOutpinTrack, m_seekBase_ns);
        assert(pCurr);

        if (!pCurr->EOS())
            m_seekBase_ns = pCurr->GetBlock()->GetTime(m_pSeekBase);

        pOutpinStream->SetCurrPosition(m_seekBase_ns, pCurr);
        return;
    }

    const BlockEntry* pCurr;

    for (;;)
    {
        status = pOutpinTrack->Seek(ns, pCurr);

        if (status >= 0)
            break;

        assert(status == mkvparser::E_BUFFER_NOT_FULL);

        status = m_pSegment->LoadCluster();
        assert(status >= 0);
    }

    assert(pCurr);

    if (pCurr->EOS())  //pathological
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

    pOutpinStream->SetCurrPosition(m_seekBase_ns, pCurr);
}

#endif  //TODO


} //end namespace WebmOggSource

