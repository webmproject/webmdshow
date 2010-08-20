#pragma once

namespace WebmMfSourceLib
{

class WebmMfByteStreamHandler : public IMFByteStreamHandler
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

    IClassFactory* m_pClassFactory;
    LONG m_cRef;

};


}  //end namespace WebmMfSourceLib
