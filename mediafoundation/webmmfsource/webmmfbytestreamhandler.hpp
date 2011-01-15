#pragma once
#include "clockable.hpp"
#ifndef _INC_COMDEF
#include <comdef.h>
#endif

namespace WebmMfSourceLib
{

class WebmMfSource;

class WebmMfByteStreamHandler : public IMFByteStreamHandler,
                                public CLockable
{
    WebmMfByteStreamHandler(const WebmMfByteStreamHandler&);
    WebmMfByteStreamHandler& operator=(const WebmMfByteStreamHandler&);

public:

    friend HRESULT CreateHandler(  //byte-stream handler
            IClassFactory*,
            IUnknown*,
            const IID&,
            void**);

    //IUnknown

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    //IMFByteStreamHandler

    HRESULT STDMETHODCALLTYPE BeginCreateObject(
        IMFByteStream*,
        LPCWSTR,
        DWORD,
        IPropertyStore*,
        IUnknown**,
        IMFAsyncCallback*,
        IUnknown*);

    HRESULT STDMETHODCALLTYPE EndCreateObject(
        IMFAsyncResult*,
        MF_OBJECT_TYPE*,
        IUnknown**);

    HRESULT STDMETHODCALLTYPE CancelObjectCreation(IUnknown*);

    HRESULT STDMETHODCALLTYPE GetMaxNumberOfBytesRequiredForResolution(QWORD*);

private:

    explicit WebmMfByteStreamHandler(IClassFactory*);
    virtual ~WebmMfByteStreamHandler();
    
    _COM_SMARTPTR_TYPEDEF(IMFByteStream, __uuidof(IMFByteStream));
    _COM_SMARTPTR_TYPEDEF(IMFAsyncResult, __uuidof(IMFAsyncResult));
    _COM_SMARTPTR_TYPEDEF(IMFMediaSource, __uuidof(IMFMediaSource));
    
    IClassFactory* const m_pClassFactory;
    LONG m_cRef;
    IMFMediaSourcePtr m_pSource;
    IMFAsyncResultPtr m_pResult;
    BOOL m_bCancel;
    IMFByteStreamPtr m_pByteStream;

    class CAsyncLoad : public IMFAsyncCallback
    {
        CAsyncLoad(const CAsyncLoad&);
        CAsyncLoad& operator=(const CAsyncLoad&);

    public:
        explicit CAsyncLoad(WebmMfByteStreamHandler*);
        virtual ~CAsyncLoad();

        WebmMfByteStreamHandler* const m_pHandler;

        HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
        ULONG STDMETHODCALLTYPE AddRef();
        ULONG STDMETHODCALLTYPE Release();

        HRESULT STDMETHODCALLTYPE GetParameters(DWORD*, DWORD*);
        HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult*);
    };
    
    CAsyncLoad m_async_load;
    
    HRESULT EndLoad(IMFAsyncResult*);

};


}  //end namespace WebmMfSourceLib
