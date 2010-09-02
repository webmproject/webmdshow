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
//_COM_SMARTPTR_TYPEDEF(IMFStreamDescriptor, __uuidof(IMFStreamDescriptor));
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
    //m_pByteStream(pByteStream),
    m_file(pByteStream),
    m_cRef(1),
    m_bShutdown(false),
    m_pSegment(0)
{
    //const ULONG n = m_pByteStream->AddRef();
    //n;

    HRESULT hr = CLockable::Init();
    assert(SUCCEEDED(hr));  //TODO

    hr = MFCreateEventQueue(&m_pEvents);
    assert(SUCCEEDED(hr));
    assert(m_pEvents);
}


WebmMfSource::~WebmMfSource()
{
    //const ULONG n = m_pByteStream->Release();
    //n;

    assert(m_bShutdown);
    assert(m_pEvents == 0);
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

    const HRESULT hr = pSegment->Load();

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

#if 0
    assert(m_pins.empty());

    using namespace MkvParser;

    typedef Stream::TCreateOutpins<VideoTrack, VideoStream, Filter> EV;
    pTracks->EnumerateVideoTracks(EV(this, &VideoStream::CreateInstance));

    typedef Stream::TCreateOutpins<AudioTrack, AudioStream, Filter> EA;
    pTracks->EnumerateAudioTracks(EA(this, &AudioStream::CreateInstance));

    if (m_pins.empty())
        return VFW_E_INVALID_FILE_FORMAT;  //TODO: better return value here?
#else
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
#endif

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

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bShutdown)
        return MF_E_SHUTDOWN;

    //http://msdn.microsoft.com/en-us/library/ms698990(v=VS.85).aspx
    //MFCreateStreamDescriptor

    //http://msdn.microsoft.com/en-us/library/ms695404(VS.85).aspx
    //MFCreatePresentationDescriptor

    return S_OK;
}


#if 0
    HRESULT STDMETHODCALLTYPE Start(
        IMFPresentationDescriptor*,
        const GUID*,
        const PROPVARIANT*);

    HRESULT STDMETHODCALLTYPE Stop();

    HRESULT STDMETHODCALLTYPE Pause();

    HRESULT STDMETHODCALLTYPE Shutdown();
#endif


void WebmMfSource::CreateVideoStream(mkvparser::Track* pTrack)
{
    WebmMfStreamVideo* pStream;

    const HRESULT hr = WebmMfStreamVideo::CreateStream(this, pTrack, pStream);
    assert(hr == S_OK);
    assert(pStream);

    m_streams.push_back(pStream);
}


void WebmMfSource::CreateAudioStream(mkvparser::Track* pTrack)
{
    WebmMfStreamAudio* pStream;

    const HRESULT hr = WebmMfStreamAudio::CreateStream(this, pTrack, pStream);
    assert(hr == S_OK);
    assert(pStream);

    m_streams.push_back(pStream);
}


}  //end namespace WebmMfSource
