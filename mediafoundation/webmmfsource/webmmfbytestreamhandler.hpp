#pragma once
#include "clockable.hpp"

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

    //local members

    IClassFactory* const m_pClassFactory;
    IMFByteStream* m_pByteStream;

    HRESULT AsyncLoadComplete(HRESULT);

private:

    explicit WebmMfByteStreamHandler(IClassFactory*);
    virtual ~WebmMfByteStreamHandler();

    LONG m_cRef;
    IMFAsyncResult* m_pResult;
    BOOL m_bCancel;

};


}  //end namespace WebmMfSourceLib
