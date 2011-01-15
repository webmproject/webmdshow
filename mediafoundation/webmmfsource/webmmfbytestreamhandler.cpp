#include <mfapi.h>
//#include <mfobjects.h>
#include <mfidl.h>
#include <mferror.h>
#include "webmmfbytestreamhandler.hpp"
#include "webmmfsource.hpp"
#include <new>
#include <cassert>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif

_COM_SMARTPTR_TYPEDEF(IMFMediaSource, __uuidof(IMFMediaSource));

namespace WebmMfSourceLib
{

//See webmmfsource.cpp:
HRESULT CreateSource(WebmMfByteStreamHandler*, WebmMfSource**);


HRESULT CreateHandler(
    IClassFactory* pClassFactory,
    IUnknown* pOuter,
    const IID& iid,
    void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    *ppv = 0;

    if (pOuter)
        return CLASS_E_NOAGGREGATION;

    typedef WebmMfByteStreamHandler H;

    H* const p = new (std::nothrow) H(pClassFactory);

    if (p == 0)
        return E_OUTOFMEMORY;

    IUnknown* const pUnk = p;

    const HRESULT hr = pUnk->QueryInterface(iid, ppv);

    const ULONG cRef = pUnk->Release();
    cRef;

    return hr;
}


#pragma warning(disable:4355)  //'this' ptr in member init list
WebmMfByteStreamHandler::WebmMfByteStreamHandler(
    IClassFactory* pClassFactory) :
    m_pClassFactory(pClassFactory),
    //m_pByteStream(0),
    m_cRef(1),
    m_bCancel(FALSE),
    //m_pResult(0),
    m_pSource(0),
    m_async_load(this)
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfByteStreamHandler: ctor: this=0x"
       << (const void*)this
       << endl;
#endif

    HRESULT hr = m_pClassFactory->LockServer(TRUE);
    assert(SUCCEEDED(hr));

    hr = CLockable::Init();
    assert(SUCCEEDED(hr));
}
#pragma warning(default:4355)

WebmMfByteStreamHandler::~WebmMfByteStreamHandler()
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfByteStreamHandler: dtor; this=0x"
       << (const void*)this
       << endl;
#endif

    //if (m_pResult)  //weird
    //{
    //    m_pResult->Release();
    //    m_pResult = 0;
    //}

    //if (m_pByteStream)  //weird
    //{
    //    m_pByteStream->Release();
    //    m_pByteStream = 0;
    //}
    
    if (m_pSource)  //weird
    {
        m_pSource->Release();
        m_pSource = 0;
    }

    const HRESULT hr = m_pClassFactory->LockServer(FALSE);
    assert(SUCCEEDED(hr));
}


HRESULT WebmMfByteStreamHandler::QueryInterface(
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
    else if (iid == __uuidof(IMFByteStreamHandler))
    {
        pUnk = static_cast<IMFByteStreamHandler*>(this);
    }
    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG WebmMfByteStreamHandler::AddRef()
{
    const ULONG n = InterlockedIncrement(&m_cRef);

#if 0 //def _DEBUG
    odbgstream os;
    os << "WebmMfByteStreamHandler::AddRef: n=" << n << endl;
#endif

    return n;
}


ULONG WebmMfByteStreamHandler::Release()
{
    const LONG n = InterlockedDecrement(&m_cRef);

#if 0 //def _DEBUG
    odbgstream os;
    os << "WebmMfByteStreamHandler::Release: n=" << n << endl;
#endif

    if (n == 0)
        delete this;

    return n;
}


HRESULT WebmMfByteStreamHandler::BeginCreateObject(
    IMFByteStream* pByteStream,
    LPCWSTR pURL,
    DWORD dwFlags,
    IPropertyStore*,
    IUnknown** ppCancelCookie,
    IMFAsyncCallback* pCallback,
    IUnknown* pState)
{
    pURL;  //?

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfByteStreamHandler::BeginCreateObject: url="
       << (pURL ? pURL : L"<NONE>")
       << endl;
#endif

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (ppCancelCookie)
        *ppCancelCookie = 0;

    if (pByteStream == 0)
        return E_INVALIDARG;

    if (pCallback == 0)
        return E_INVALIDARG;

    if ((dwFlags & MF_RESOLUTION_MEDIASOURCE) == 0)
        return E_INVALIDARG;
        
    if (m_pSource)  //assume one-at-a-time creation
        return MF_E_UNEXPECTED;
        
    DWORD dw;
    hr = pByteStream->GetCapabilities(&dw);

    if (SUCCEEDED(hr))
    {
#ifdef _DEBUG
        odbgstream os;
        os << "webmmfsource::BeginCreateObject: caps:";

        if (dw & MFBYTESTREAM_IS_READABLE)
            os << " READABLE";

        if (dw & MFBYTESTREAM_IS_WRITABLE)
            os << " WRITABLE";

        if (dw & MFBYTESTREAM_IS_SEEKABLE)
            os << " SEEKABLE";

        if (dw & MFBYTESTREAM_IS_REMOTE)
            os << " REMOTE";

        if (dw & MFBYTESTREAM_IS_DIRECTORY)
            os << " DIRECTORY";

        if (dw & MFBYTESTREAM_HAS_SLOW_SEEK)
            os << " SLOW_SEEK";

        if (dw & MFBYTESTREAM_IS_PARTIALLY_DOWNLOADED)
            os << " PARTIALLY_DOWNLOADED";

        if (dw & MFBYTESTREAM_SHARE_WRITE)
            os << " SHARE_WRITE";

        os << endl;
#endif

        __noop;  //TODO: vet capabilities
    }

    WebmMfSource* pSource;
    
    hr = WebmMfSource::CreateSource(m_pClassFactory, pByteStream, pSource);

    if (FAILED(hr))
        return hr;
        
    const IMFMediaSourcePtr pSource_(pSource, false);
    
    IMFAsyncResultPtr pResult;
    
    hr = MFCreateAsyncResult(0, pCallback, pState, &pResult);

    if (FAILED(hr))
        return hr;
        
    hr = pSource->BeginLoad(&m_async_load);
    
    if (FAILED(hr))
        return hr;
        
    m_pSource = pSource_;
    m_pResult = pResult;
    
    m_bCancel = FALSE;    
    m_pByteStream = pByteStream;
    
    return S_OK;
}


HRESULT WebmMfByteStreamHandler::EndCreateObject(
    IMFAsyncResult* pResult,
    MF_OBJECT_TYPE* pObjectType,
    IUnknown** ppObject)
{
    Lock lock;
    
    HRESULT hr = lock.Seize(this);
    
    if (FAILED(hr))
        return hr;

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfByteStreamHandler::EndCreateObject (begin)" << endl;
#endif

    if (ppObject == 0)
        return E_POINTER;

    IUnknown*& pObject = *ppObject;
    pObject = 0;

    if (pObjectType == 0)
        return E_POINTER;

    MF_OBJECT_TYPE& type = *pObjectType;
    type = MF_OBJECT_INVALID;

    if (pResult == 0)
        return E_INVALIDARG;

    hr = pResult->GetStatus();

    if (FAILED(hr))  //for example, cancelled
        return hr;

#if 0
    IUnknownPtr pUnk;

    hr = pResult->GetObject(&pUnk);

    if (FAILED(hr))
        return hr;

    assert(pUnk);

    hr = pUnk->QueryInterface(__uuidof(IMFMediaSource), (void**)&pObject);

    if (FAILED(hr))
        return hr;
#else
    if (!m_pSource)  //should never happen
        return MF_E_UNEXPECTED;
        
    pObject = m_pSource.Detach();
#endif
        
    type = MF_OBJECT_MEDIASOURCE;

#ifdef _DEBUG
    os << L"WebmMfByteStreamHandler::EndCreateObject (end)" << endl;
#endif

    return S_OK;
}


HRESULT WebmMfByteStreamHandler::CancelObjectCreation(IUnknown*)
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfByteStreamHandler::CancelObjectCreation" << endl;
#endif

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bCancel)
        return S_OK;

    if (!m_pByteStream)
        return S_OK;

    m_bCancel = TRUE;

    return m_pByteStream->Close();
}


HRESULT WebmMfByteStreamHandler::GetMaxNumberOfBytesRequiredForResolution(
    QWORD* pcb)
{
    if (pcb == 0)
        return E_POINTER;

    QWORD& cb = *pcb;

    cb = 1024;  //TODO: ask the webm parser for this value

    return S_OK;
}


WebmMfByteStreamHandler::CAsyncLoad::CAsyncLoad(
    WebmMfByteStreamHandler* p) : 
    m_pHandler(p)
{
}


WebmMfByteStreamHandler::CAsyncLoad::~CAsyncLoad()
{
}


HRESULT WebmMfByteStreamHandler::CAsyncLoad::QueryInterface(
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


ULONG WebmMfByteStreamHandler::CAsyncLoad::AddRef()
{
    return m_pHandler->AddRef();
}


ULONG WebmMfByteStreamHandler::CAsyncLoad::Release()
{
    return m_pHandler->Release();
}


HRESULT WebmMfByteStreamHandler::CAsyncLoad::GetParameters(
    DWORD*,
    DWORD*)
{
    return E_NOTIMPL;  //means "assume default behavior"
}


HRESULT WebmMfByteStreamHandler::CAsyncLoad::Invoke(IMFAsyncResult* pResult)
{
    if (pResult == 0)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(m_pHandler);

    if (FAILED(hr))
        return hr;
        
    return m_pHandler->EndLoad(pResult);
}


HRESULT WebmMfByteStreamHandler::EndLoad(IMFAsyncResult* pResult)
{
    if (!m_pResult)  //should never happen
        return MF_E_UNEXPECTED;

    HRESULT hr;
    
    if (m_bCancel)
        hr = MF_E_OPERATION_CANCELLED;
    
    else if (m_pSource == 0)  //should never happen
        hr = MF_E_UNEXPECTED;

    else
    {
        IMFMediaSource* const pSource_ = m_pSource;
        WebmMfSource* const pSource = static_cast<WebmMfSource*>(pSource_);
        
        hr = pSource->EndLoad(pResult);
    }
        
    const HRESULT hrSetStatus = m_pResult->SetStatus(hr);
    hrSetStatus;
    assert(SUCCEEDED(hrSetStatus));
    
    hr = MFInvokeCallback(m_pResult);
    
    //m_pSource = 0;
    m_pByteStream = 0;
    m_pResult = 0;

    return hr;
}


}  //end namespace WebmMfSourceLib
