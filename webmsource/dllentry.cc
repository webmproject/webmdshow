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
#include "graphutil.h"
#include <cassert>
#include <comdef.h>
#include <uuids.h>

HMODULE g_hModule;
static ULONG s_cLock;

namespace WebmSource
{
    HRESULT CreateInstance(
            IClassFactory*,
            IUnknown*,
            const IID&,
            void**);

}  //end namespace WebmSource


static CFactory s_factory(&s_cLock, &WebmSource::CreateInstance);


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
    if (clsid == WebmTypes::CLSID_WebmSource)
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
                    WebmTypes::CLSID_WebmSource);

    //assert(SUCCEEDED(hr));

    hr = ComReg::UnRegisterCustomFileType(
            L".webm",
            WebmTypes::CLSID_WebmSource);
    assert(SUCCEEDED(hr));

    hr = ComReg::UnRegisterCoclass(WebmTypes::CLSID_WebmSource);

    return SUCCEEDED(hr) ? S_OK : S_FALSE;
}


STDAPI DllRegisterServer()
{
    std::wstring filename_;

    HRESULT hr = ComReg::ComRegGetModuleFileName(g_hModule, filename_);
    assert(SUCCEEDED(hr));
    assert(!filename_.empty());

    const wchar_t* const filename = filename_.c_str();

#if _DEBUG
    const wchar_t friendlyname[] = L"WebM Source Filter (Debug)";
#else
    const wchar_t friendlyname[] = L"WebM Source Filter";
#endif

    hr = DllUnregisterServer();
    assert(SUCCEEDED(hr));

    hr = ComReg::RegisterCoclass(
            WebmTypes::CLSID_WebmSource,
            friendlyname,
            filename,
            L"Webm.Source",
            L"Webm.Source.1",
            false,  //not insertable
            false,  //not a control
            ComReg::kBoth,  //DShow filters must support "both"
            GUID_NULL,     //typelib
            0,    //no version specified
            0);   //no toolbox bitmap

    assert(SUCCEEDED(hr));

    hr = ComReg::RegisterCustomFileType(
            L".webm",
            WebmTypes::CLSID_WebmSource,
            GUID_NULL,
            GUID_NULL);

    assert(SUCCEEDED(hr));

    const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
    assert(bool(pMapper));

#if 1
    enum { cPins = 1 };
    REGFILTERPINS pins[cPins];

    REGFILTERPINS& pin = pins[0];

    pin.strName = 0;              //obsolete
    pin.bRendered = FALSE;        //always FALSE for outpins
    pin.bOutput = TRUE;
    pin.bZero = FALSE;            //well, not really
    pin.bMany = TRUE;
    pin.clsConnectsToFilter = 0;  //obsolete
    pin.strConnectsToPin = 0;     //obsolete
    pin.nMediaTypes = 0;
    pin.lpMediaType = 0;
#else
    enum { cPins = 2 };
    REGFILTERPINS pins[cPins];

    REGFILTERPINS& v = pins[0];

    const REGPINTYPES vt[] =
    {
        { &MEDIATYPE_Video, &MEDIASUBTYPE_NULL }
    };

    v.strName = 0;              //obsolete
    v.bRendered = FALSE;        //always FALSE for outpins
    v.bOutput = TRUE;
    v.bZero = FALSE;            //well, not really
    v.bMany = TRUE;
    v.clsConnectsToFilter = 0;  //obsolete
    v.strConnectsToPin = 0;     //obsolete
    v.nMediaTypes = 1;
    v.lpMediaType = vt;

    REGFILTERPINS& a = pins[1];

    const REGPINTYPES at[] =
    {
        { &MEDIATYPE_Audio, &MEDIASUBTYPE_NULL }
    };

    a.strName = 0;              //obsolete
    a.bRendered = FALSE;        //always FALSE for outpins
    a.bOutput = TRUE;
    a.bZero = FALSE;            //well, not really
    a.bMany = TRUE;
    a.clsConnectsToFilter = 0;  //obsolete
    a.strConnectsToPin = 0;     //obsolete
    a.nMediaTypes = 1;
    a.lpMediaType = at;
#endif

    //pin setup complete

    REGFILTER2 filter;

    filter.dwVersion = 1;
    filter.dwMerit = MERIT_NORMAL;
    filter.cPins = cPins;
    filter.rgPins = pins;

    hr = pMapper->RegisterFilter(
            WebmTypes::CLSID_WebmSource,
            friendlyname,  //?
            0,
            &CLSID_LegacyAmFilterCategory,
            0,
            &filter);

    assert(SUCCEEDED(hr));

    return S_OK;
}
