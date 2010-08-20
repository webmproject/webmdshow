//#include <mfapi.h>
//#include <mfobjects.h>
#include <mfidl.h>
//#include <mferror.h>
#include "webmmfsource.hpp"
#include <new>
#include <cassert>

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
HRESULT CreateSource(IMFByteStream* pByteStream, IMFMediaSource** ppSource)
{
    assert(pByteStream);
    assert(ppSource);

    IMFMediaSource*& pSource = *ppSource;

    pSource = new (std::nothrow) WebmMfSource(pByteStream);

    return (pSource == 0) ? E_OUTOFMEMORY : S_OK;
}
#endif


WebmMfSource::WebmMfSource(IMFByteStream* pByteStream) :
    m_pByteStream(pByteStream),
    m_cRef(1)
{
    const ULONG n = m_pByteStream->AddRef();
    n;
}


WebmMfSource::~WebmMfSource()
{
    const ULONG n = m_pByteStream->Release();
    n;
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


#if 0
    HRESULT STDMETHODCALLTYPE GetEvent(
        DWORD dwFlags,
        IMFMediaEvent**);

    HRESULT STDMETHODCALLTYPE BeginGetEvent(
        IMFAsyncCallback*,
        IUnknown*);

    HRESULT STDMETHODCALLTYPE EndGetEvent(
        IMFAsyncResult*,
        IMFMediaEvent**);

    HRESULT STDMETHODCALLTYPE QueueEvent(
        MediaEventType met,
        REFGUID guidExtendedType,
        HRESULT hrStatus,
        const PROPVARIANT*);

    //IMFMediaSource

    HRESULT STDMETHODCALLTYPE GetCharacteristics(DWORD*);

    HRESULT STDMETHODCALLTYPE CreatePresentationDescriptor(
        IMFPresentationDescriptor**);

    HRESULT STDMETHODCALLTYPE Start(
        IMFPresentationDescriptor*,
        const GUID*,
        const PROPVARIANT*);

    HRESULT STDMETHODCALLTYPE Stop();

    HRESULT STDMETHODCALLTYPE Pause();

    HRESULT STDMETHODCALLTYPE Shutdown();
#endif



}  //end namespace WebmMfSource
