#include <mfapi.h>
//#include <mfobjects.h>
#include <mfidl.h>
//#include <mferror.h>
#include "webmmfbytestreamhandler.hpp"
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
HRESULT CreateSource(IClassFactory*, IMFByteStream*, IMFMediaSource**);


HRESULT CreateHandler(
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

    WebmMfByteStreamHandler* const p =
        new (std::nothrow) WebmMfByteStreamHandler(pClassFactory);

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
    m_cRef(1)
{
    const HRESULT hr = m_pClassFactory->LockServer(TRUE);
    assert(SUCCEEDED(hr));

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfByteStreamHandler: ctor" << endl;
#endif
}


WebmMfByteStreamHandler::~WebmMfByteStreamHandler()
{
#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfByteStreamHandler: dtor" << endl;
#endif

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


ULONG WebmMfByteStreamHandler::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}


ULONG WebmMfByteStreamHandler::Release()
{
    if (LONG n = InterlockedDecrement(&m_cRef))
        return n;

    delete this;
    return 0;
}


HRESULT WebmMfByteStreamHandler::BeginCreateObject(
    IMFByteStream* pByteStream,
    LPCWSTR pURL,
    DWORD dwFlags,
    IPropertyStore*,
    IUnknown** ppUnkCancelCookie,
    IMFAsyncCallback* pCallback,
    IUnknown* pUnkState)
{
    pURL;  //?

#ifdef _DEBUG
    wodbgstream os;
    os << L"WebmMfByteStreamHandler::BeginCreateObject: url="
       << (pURL ? pURL : L"<NONE>")
       << endl;
#endif

    if (ppUnkCancelCookie)
        *ppUnkCancelCookie = 0;

    if (pByteStream == 0)
        return E_INVALIDARG;

    if (pCallback == 0)
        return E_INVALIDARG;

    dwFlags;
    //TODO: if (!(dwFlags & MF_RESOLUTION_MEDIASOURCE) return E_INVALIDARG;

    IMFMediaSourcePtr pSource;

    //TODO: pass pByteStream as arg to Open, not CreateSource
    HRESULT hr = CreateSource(m_pClassFactory, pByteStream, &pSource);

    if (FAILED(hr))
        return hr;

    //TODO: init parser lib

    IMFAsyncResult* pResult;

    hr = MFCreateAsyncResult(pSource, pCallback, pUnkState, &pResult);

    if (FAILED(hr))
        return hr;

    assert(pResult);

    //TODO: pSource->BeginOpen(pByteStream, this, 0);
    //etc: see mpeg1source example

    hr = MFInvokeCallback(pResult);

    pResult->Release();

    return hr;
}


HRESULT WebmMfByteStreamHandler::EndCreateObject(
    IMFAsyncResult* pResult,
    MF_OBJECT_TYPE* pObjectType,
    IUnknown** ppObject)
{
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

    IUnknownPtr pUnk;

    HRESULT hr = pResult->GetObject(&pUnk);

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

    return E_NOTIMPL;
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


}  //end namespace WebmMfSourceLib
