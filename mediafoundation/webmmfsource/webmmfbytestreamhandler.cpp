#include <mfapi.h>
//#include <mfobjects.h>
#include <mfidl.h>
#include <mferror.h>
#include "webmmfbytestreamhandler.hpp"
#include "webmmfsource.hpp"
#include <new>
#include <cassert>
#ifndef _INC_COMDEF
#include <comdef.h>
#endif
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


WebmMfByteStreamHandler::WebmMfByteStreamHandler(
    IClassFactory* pClassFactory) :
    m_pClassFactory(pClassFactory),
    m_pByteStream(0),
    m_bCancel(FALSE),
    m_cRef(1),
    m_pResult(0)
{
    HRESULT hr = m_pClassFactory->LockServer(TRUE);
    assert(SUCCEEDED(hr));

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfByteStreamHandler: ctor: this=0x"
       << (const void*)this
       << endl;
#endif

    hr = CLockable::Init();
    assert(SUCCEEDED(hr));
}


WebmMfByteStreamHandler::~WebmMfByteStreamHandler()
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfByteStreamHandler: dtor; this=0x"
       << (const void*)this
       << endl;
#endif

    if (m_pResult)  //weird
    {
        m_pResult->Release();
        m_pResult = 0;
    }

    if (m_pByteStream)
    {
        m_pByteStream->Release();
        m_pByteStream = 0;
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

    if (m_pResult)  //assume one-at-a-time creation
        return MF_E_UNEXPECTED;

    if (pByteStream == 0)
        return E_INVALIDARG;

    if (pCallback == 0)
        return E_INVALIDARG;

    if ((dwFlags & MF_RESOLUTION_MEDIASOURCE) == 0)
        return E_INVALIDARG;

    if (m_pByteStream)  //weird
        m_pByteStream->Release();

    m_pByteStream = pByteStream;
    m_pByteStream->AddRef();

    WebmMfSource* pSource;

    hr = CreateSource(this, &pSource);

    if (FAILED(hr))
        return hr;

    IMFMediaSource* const pObject = pSource;

    hr = MFCreateAsyncResult(pObject, pCallback, pState, &m_pResult);

    if (SUCCEEDED(hr))
        hr = pSource->AsyncLoad();

    pObject->Release();
    return hr;
}


HRESULT WebmMfByteStreamHandler::EndCreateObject(
    IMFAsyncResult* pResult,
    MF_OBJECT_TYPE* pObjectType,
    IUnknown** ppObject)
{
    //lock lock;
    //
    //HRESULT hr = lock.seize(this);
    //
    //if (failed(hr))
    //    return hr;

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

    HRESULT hr = pResult->GetStatus();

    if (FAILED(hr))  //for example, cancelled
        return hr;

    IUnknownPtr pUnk;

    hr = pResult->GetObject(&pUnk);

    if (FAILED(hr))
        return hr;

    assert(pUnk);

    hr = pUnk->QueryInterface(__uuidof(IMFMediaSource), (void**)&pObject);

    if (FAILED(hr))
        return hr;

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

    if (m_pByteStream == 0)
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


HRESULT WebmMfByteStreamHandler::AsyncLoadComplete(HRESULT hrLoad)
{
    //TODO: does implementing this using IMFAsyncCallback
    //(AsyncLoadComplete then becomes Invoke) break the
    //cyclic dependency between the byte stream handler
    //and the media source?  If so, would this allow us
    //to not derive from CLockable?

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfByteStreamHandler::AsyncLoadComplete" << endl;
#endif

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //TODO: do we still call invoke if we're cancelled?

    const BOOL bCancel = m_bCancel;

    IMFAsyncResult* const pResult = m_pResult;

    if (pResult == 0)  //weird
        return MF_E_UNEXPECTED;

    m_pResult = 0;  //has not been Release'd yet

    if (m_pByteStream)
    {
        m_pByteStream->Release();
        m_pByteStream = 0;
    }

    lock.Release();

    if (bCancel)
        hrLoad = MF_E_OPERATION_CANCELLED;  //?

    hr = pResult->SetStatus(hrLoad);
    assert(SUCCEEDED(hr));

    //IUnknown* pObject;
    //
    //hr = pResult->GetObject(&pObject);
    //assert(SUCCEEDED(hr));
    //assert(pObject);

    hr = MFInvokeCallback(pResult);
    assert(SUCCEEDED(hr));

    pResult->Release();  //TODO: resolve refcount issues

    return hr;
}


}  //end namespace WebmMfSourceLib
