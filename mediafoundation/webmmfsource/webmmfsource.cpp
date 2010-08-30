//#include <mfobjects.h>
#include <mfidl.h>
#include "clockable.hpp"
#include "webmmfsource.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <new>
#include <cassert>
#include <comdef.h>

_COM_SMARTPTR_TYPEDEF(IMFMediaEventQueue, __uuidof(IMFMediaEventQueue));
  

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

    return pSource ? S_OK : E_OUTOFMEMORY;
}
#endif


WebmMfSource::WebmMfSource(IMFByteStream* pByteStream) :
    m_pByteStream(pByteStream),
    m_cRef(1),
    m_bShutdown(false)
{
    const ULONG n = m_pByteStream->AddRef();
    n;

    HRESULT hr = CLockable::Init();
    assert(SUCCEEDED(hr));  //TODO
    
    hr = MFCreateEventQueue(&m_pEvents);
    assert(SUCCEEDED(hr));
    assert(m_pEvents);
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
//    {	MFMEDIASOURCE_IS_LIVE	= 0x1,
//	MFMEDIASOURCE_CAN_SEEK	= 0x2,
//	MFMEDIASOURCE_CAN_PAUSE	= 0x4,
//	MFMEDIASOURCE_HAS_SLOW_SEEK	= 0x8,
//	MFMEDIASOURCE_HAS_MULTIPLE_PRESENTATIONS	= 0x10,
//	MFMEDIASOURCE_CAN_SKIPFORWARD	= 0x20,
//	MFMEDIASOURCE_CAN_SKIPBACKWARD	= 0x40

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



}  //end namespace WebmMfSource
