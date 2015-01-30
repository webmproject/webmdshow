// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "cfactory.h"
#include "comreg.h"
#include "webmtypes.h"
#include <cassert>
#include <comdef.h>
#include <uuids.h>
#include "graphutil.h"

static HMODULE s_hModule;
static ULONG s_cLock;


namespace WebmSplit
{
    HRESULT CreateInstance(
            IClassFactory*,
            IUnknown*,
            const IID&,
            void**);

}  //end namespace WebmSplit


static CFactory s_factory(&s_cLock, &WebmSplit::CreateInstance);


BOOL APIENTRY DllMain(
    HINSTANCE hModule,
    DWORD  dwReason,
    LPVOID)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            s_hModule = hModule;
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
    if (clsid == WebmTypes::CLSID_WebmSplit)
        return s_factory.QueryInterface(iid, ppv);

    return CLASS_E_CLASSNOTAVAILABLE;
}


STDAPI DllUnregisterServer()
{
    const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
    assert(bool(pMapper));

    HRESULT hr = pMapper->UnregisterFilter(
                    &CLSID_LegacyAmFilterCategory,
                    0,
                    WebmTypes::CLSID_WebmSplit);

    if (FAILED(hr))
        return hr;

#if 0
    hr = ComReg::UnRegisterCustomFileType(
            L".webm",
            CLSID_AsyncReader);

    if (FAILED(hr))
        return hr;
#endif

#if 0
    hr = ComReg::UnRegisterProtocolSource(
            L"http",
            L".webm",
            CLSID_URLReader /* WebmTypes::CLSID_WebmSplit */ );

    if (FAILED(hr))
        return hr;
#endif

    hr = ComReg::UnRegisterCoclass(WebmTypes::CLSID_WebmSplit);

    if (FAILED(hr))
        return hr;

    return S_OK;
}


STDAPI DllRegisterServer()
{
    std::wstring filename_;

    HRESULT hr = ComReg::ComRegGetModuleFileName(s_hModule, filename_);
    assert(SUCCEEDED(hr));
    assert(!filename_.empty());

    const wchar_t* const filename = filename_.c_str();

#if _DEBUG
    const wchar_t friendlyname[] = L"WebM Splitter Filter (Debug)";
#else
    const wchar_t friendlyname[] = L"WebM Splitter Filter";
#endif

    hr = DllUnregisterServer();

    if (FAILED(hr))
        return hr;

    hr = ComReg::RegisterCoclass(
            WebmTypes::CLSID_WebmSplit,
            friendlyname,
            filename,
            L"Webm.Split",
            L"Webm.Split.1",
            false,  //not insertable
            false,  //not a control
            ComReg::kBoth,  //DShow filters must support "both"
            GUID_NULL,     //typelib
            0,    //no version specified
            0);   //no toolbox bitmap

    if (FAILED(hr))
        return hr;

#if 0
    hr = ComReg::RegisterCustomFileType(
            L".webm",
            CLSID_AsyncReader,             //source
            MEDIATYPE_Stream,              //major
            WebmTypes::MEDIASUBTYPE_WEBM); //minor

    if (FAILED(hr))
        return hr;
#endif

#if 0
    hr = ComReg::RegisterProtocolSource(
            L"http",
            L".webm",
            CLSID_URLReader /* WebmTypes::CLSID_WebmSplit */ );

    if (FAILED(hr))
        return hr;
#endif

    const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
    assert(bool(pMapper));

    enum { cPins = 2 };
    REGFILTERPINS pins[cPins];

    REGFILTERPINS& inpin = pins[0];

    enum { nInpinMediaTypes = 1 };
    const REGPINTYPES inpinMediaTypes[nInpinMediaTypes] =
    {
        { &MEDIATYPE_Stream, &WebmTypes::MEDIASUBTYPE_WEBM }
    };

    inpin.strName = 0;              //obsolete
    inpin.bRendered = FALSE;        //TODO: should this be TRUE?
    inpin.bOutput = FALSE;
    inpin.bZero = FALSE;
    inpin.bMany = FALSE;
    inpin.clsConnectsToFilter = 0;  //obsolete
    inpin.strConnectsToPin = 0;     //obsolete
    inpin.nMediaTypes = nInpinMediaTypes;
    inpin.lpMediaType = inpinMediaTypes;

    REGFILTERPINS& outpin = pins[1];

    outpin.strName = 0;              //obsolete
    outpin.bRendered = FALSE;        //always FALSE for outpins
    outpin.bOutput = TRUE;
    outpin.bZero = FALSE;            //?
    outpin.bMany = TRUE;
    outpin.clsConnectsToFilter = 0;  //obsolete
    outpin.strConnectsToPin = 0;     //obsolete
    outpin.nMediaTypes = 0;
    outpin.lpMediaType = 0;

    //pin setup complete

    REGFILTER2 filter;

    filter.dwVersion = 1;
    filter.dwMerit = MERIT_NORMAL;
    filter.cPins = cPins;
    filter.rgPins = pins;

    hr = pMapper->RegisterFilter(
            WebmTypes::CLSID_WebmSplit,
            friendlyname,
            0,
            &CLSID_LegacyAmFilterCategory,
            0,
            &filter);

    return hr;
}
