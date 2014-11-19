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
#include <cassert>
#include <comdef.h>
#include <uuids.h>
#include "graphutil.h"
#include "webmtypes.h"
#include "vorbistypes.h"

HMODULE g_hModule;
static ULONG s_cLock;

namespace WebmVorbisDecoderLib
{
    HRESULT CreateInstance(
            IClassFactory*,
            IUnknown*,
            const IID&,
            void**);

}  //end namespace WebmVorbisDecoderLib


static CFactory s_factory(&s_cLock, &WebmVorbisDecoderLib::CreateInstance);


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
    if (clsid == WebmTypes::CLSID_WebmVorbisDecoder)
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
                    WebmTypes::CLSID_WebmVorbisDecoder);

    assert(SUCCEEDED(hr));  //TODO

    hr = ComReg::UnRegisterCoclass(WebmTypes::CLSID_WebmVorbisDecoder);
    assert(SUCCEEDED(hr));  //TODO

    return S_OK;  //TODO
}


STDAPI DllRegisterServer()
{
    std::wstring filename_;

    HRESULT hr = ComReg::ComRegGetModuleFileName(g_hModule, filename_);
    assert(SUCCEEDED(hr));
    assert(!filename_.empty());

    const wchar_t* const filename = filename_.c_str();

#if _DEBUG
    const wchar_t friendlyname[] = L"WebM Vorbis Decoder Filter (Debug)";
#else
    const wchar_t friendlyname[] = L"WebM Vorbis Decoder Filter";
#endif

    hr = DllUnregisterServer();
    assert(SUCCEEDED(hr));

    hr = ComReg::RegisterCoclass(
            WebmTypes::CLSID_WebmVorbisDecoder,
            friendlyname,
            filename,
            L"Webm.VorbisDecoder",
            L"Webm.VorbisDecoder.1",
            false,  //not insertable
            false,  //not a control
            ComReg::kBoth,  //DShow filters must support "both"
            GUID_NULL,     //typelib
            0,    //no version specified
            0);   //no toolbox bitmap

    assert(SUCCEEDED(hr));

    const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
    assert(bool(pMapper));

    enum { cPins = 2 };
    REGFILTERPINS pins[cPins];

    REGFILTERPINS& inpin = pins[0];

    enum { nInpinMediaTypes = 1 };
    const REGPINTYPES inpinMediaTypes[nInpinMediaTypes] =
    {
        { &MEDIATYPE_Audio, &VorbisTypes::MEDIASUBTYPE_Vorbis2 }
    };

    inpin.strName = 0;              //obsolete
    inpin.bRendered = FALSE;        //TODO: ?
    inpin.bOutput = FALSE;
    inpin.bZero = FALSE;
    inpin.bMany = FALSE;
    inpin.clsConnectsToFilter = 0;  //obsolete
    inpin.strConnectsToPin = 0;     //obsolete
    inpin.nMediaTypes = nInpinMediaTypes;
    inpin.lpMediaType = inpinMediaTypes;

    REGFILTERPINS& outpin = pins[1];

    enum { nOutpinMediaTypes = 1 };
    const REGPINTYPES outpinMediaTypes[nOutpinMediaTypes] =
    {
        { &MEDIATYPE_Audio, &MEDIASUBTYPE_IEEE_FLOAT },
    };

    outpin.strName = 0;              //obsolete
    outpin.bRendered = FALSE;        //always FALSE for outpins
    outpin.bOutput = TRUE;
    outpin.bZero = FALSE;
    outpin.bMany = FALSE;
    outpin.clsConnectsToFilter = 0;  //obsolete
    outpin.strConnectsToPin = 0;     //obsolete
    outpin.nMediaTypes = nOutpinMediaTypes;
    outpin.lpMediaType = outpinMediaTypes;

    //pin setup complete

    REGFILTER2 filter;

    filter.dwVersion = 1;
    filter.dwMerit = MERIT_NORMAL;
    filter.cPins = cPins;
    filter.rgPins = pins;

    hr = pMapper->RegisterFilter(
            WebmTypes::CLSID_WebmVorbisDecoder,
            friendlyname,
            0,
            &CLSID_LegacyAmFilterCategory,
            0,
            &filter);

    return hr;
}
