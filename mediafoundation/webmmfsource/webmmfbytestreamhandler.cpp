#include <mfapi.h>
//#include <mfobjects.h>
#include <mfidl.h>
//#include <mferror.h>
#include "webmmfbytestreamhandler.hpp"
#include <new>
#ifndef _INC_COMDEF
#include <comdef.h>
#endif

_COM_SMARTPTR_TYPEDEF(IMFMediaSource, __uuidof(IMFMediaSource));

namespace WebmMfSourceLib
{


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
    m_pClassFactory->LockServer(TRUE);
}


WebmMfByteStreamHandler::~WebmMfByteStreamHandler()
{
    m_pClassFactory->LockServer(FALSE);
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
    DWORD,
    IPropertyStore*,
    IUnknown** ppUnkCancelCookie,
    IMFAsyncCallback* pCallback,
    IUnknown* pUnkState)
{
    pURL;  //?

    if (ppUnkCancelCookie)
        *ppUnkCancelCookie = 0;

    if (pByteStream == 0)
        return E_INVALIDARG;

    if (pCallback == 0)
        return E_INVALIDARG;

    extern HRESULT CreateSource(IMFByteStream*, IMFMediaSource**);

    IMFMediaSourcePtr pSource;

    //TODO: pass pByteStream as arg to Open, not CreateSource
    HRESULT hr = CreateSource(pByteStream, &pSource);

    if (FAILED(hr))
        return hr;
        
    //TODO: init parser lib

    IMFAsyncResult* pResult;

    hr = MFCreateAsyncResult(pSource, pCallback, pUnkState, &pResult);

    if (FAILED(hr))
        return hr;

    hr = MFInvokeCallback(pResult);

    return hr;
}


HRESULT WebmMfByteStreamHandler::EndCreateObject(
    IMFAsyncResult* pResult,
    MF_OBJECT_TYPE* pObjectType,
    IUnknown** ppObject)
{
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

    HRESULT hr = pResult->GetObject(&pObject);

    if (FAILED(hr))
        return hr;

    assert(pObject);

    IMFMediaSource* pSource;

    hr = pObject->QueryInterface(&pSource);

    if (FAILED(hr))
        return hr;

    const ULONG n = pSource->Release();
    n;
    assert(n);

    type = MF_OBJECT_MEDIASOURCE;
    return S_OK;
}


HRESULT WebmMfByteStreamHandler::CancelObjectCreation(IUnknown*)
{
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
