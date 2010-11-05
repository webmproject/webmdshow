// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "cfactory.hpp"
#include "comreg.hpp"
#include "webmtypes.hpp"
#include <cassert>
#include <comdef.h>
//#include <uuids.h>

HMODULE g_hModule;
static ULONG s_cLock;

namespace WebmMfSourceLib
{
    HRESULT CreateHandler(
            IClassFactory*,
            IUnknown*,
            const IID&,
            void**);

    //HRESULT CreateSource(
    //        IClassFactory*,
    //        IUnknown*,
    //        const IID&,
    //        void**);

}  //end namespace WebmMfSourceLib


//static CFactory s_source_factory(&s_cLock, &WebmMfSourceLib::CreateSource);
static CFactory s_handler_factory(&s_cLock, &WebmMfSourceLib::CreateHandler);


BOOL APIENTRY DllMain(
    HINSTANCE hModule,
    DWORD  dwReason,
    LPVOID)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            g_hModule = hModule;
            break;
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
        default:
            break;
    }

    return TRUE;
}



STDAPI DllCanUnloadNow()
{
    return s_cLock ? S_FALSE : S_OK;
}


STDAPI DllGetClassObject(
    const CLSID& clsid,
    const IID& iid,
    void** ppv)
{
    if (clsid == WebmTypes::CLSID_WebmMfByteStreamHandler)
        return s_handler_factory.QueryInterface(iid, ppv);

    return CLASS_E_CLASSNOTAVAILABLE;
}


STDAPI DllUnregisterServer()
{
    HRESULT hr = ComReg::UnRegisterCustomFileType(
                    L".webm",
                    WebmTypes::CLSID_WebmMfByteStreamHandler);
    assert(SUCCEEDED(hr));  //TODO

    hr = ComReg::UnRegisterCustomFileType(
                    L"video/webm",
                    WebmTypes::CLSID_WebmMfByteStreamHandler);
    assert(SUCCEEDED(hr));  //TODO

    hr = ComReg::UnRegisterCustomFileType(
                    L"audio/webm",
                    WebmTypes::CLSID_WebmMfByteStreamHandler);
    assert(SUCCEEDED(hr));  //TODO

    hr = ComReg::UnRegisterCoclass(WebmTypes::CLSID_WebmMfByteStreamHandler);
    assert(SUCCEEDED(hr));  //TODO

    return S_OK;
}


#if 0
static void RegisterSource(const wchar_t* filename)
{
#if _DEBUG
    const wchar_t friendlyname[] = L"WebM MF Source (Debug)";
#else
    const wchar_t friendlyname[] = L"WebM MF Source";
#endif

    const HRESULT hr = ComReg::RegisterCoclass(
                        WebmTypes::CLSID_WebmMfSource,
                        friendlyname,
                        filename,
                        L"Webm.MfSource",
                        L"Webm.MfSource.1",
                        false,  //not insertable
                        false,  //not a control
                        ComReg::kBoth,  //DShow filters must support "both"
                        GUID_NULL,     //typelib
                        0,    //no version specified
                        0);   //no toolbox bitmap

    assert(SUCCEEDED(hr));  //TODO
}
#endif


static void RegisterHandler(const wchar_t* filename)
{
#if _DEBUG
    const wchar_t friendly_name[] = L"WebM MF Byte-Stream Handler (Debug)";
#else
    const wchar_t friendly_name[] = L"WebM MF Byte-Stream Handler";
#endif

    HRESULT hr = ComReg::RegisterCoclass(
                    WebmTypes::CLSID_WebmMfByteStreamHandler,
                    friendly_name,
                    filename,
                    0,  //?
                    0,  //?
                    false,  //not insertable
                    false,  //not a control
                    ComReg::kBoth,  //DShow filters must support "both"
                    GUID_NULL,     //typelib
                    0,    //no version specified
                    0);   //no toolbox bitmap

    assert(SUCCEEDED(hr));  //TODO

    hr = ComReg::RegisterByteStreamHandler(
            L".webm",
            WebmTypes::CLSID_WebmMfByteStreamHandler,
            friendly_name);

    assert(SUCCEEDED(hr));  //TODO

    hr = ComReg::RegisterByteStreamHandler(
            L"video/webm",
            WebmTypes::CLSID_WebmMfByteStreamHandler,
            friendly_name);

    assert(SUCCEEDED(hr));  //TODO

    hr = ComReg::RegisterByteStreamHandler(
            L"audio/webm",
            WebmTypes::CLSID_WebmMfByteStreamHandler,
            friendly_name);

    assert(SUCCEEDED(hr));  //TODO
}


STDAPI DllRegisterServer()
{
    HRESULT hr = DllUnregisterServer();
    assert(SUCCEEDED(hr));

    std::wstring filename_;

    hr = ComReg::ComRegGetModuleFileName(g_hModule, filename_);
    assert(SUCCEEDED(hr));
    assert(!filename_.empty());

    const wchar_t* const filename = filename_.c_str();

    //RegisterSource(filename);
    RegisterHandler(filename);

    return S_OK;
}
