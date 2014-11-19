// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "cfactory.h"
#include "comreg.h"
#include "webmtypes.h"
#include "vorbistypes.h"
#include <mfapi.h>
#include <cassert>
#include <comdef.h>
#include <uuids.h>

HMODULE g_hModule;
static ULONG s_cLock;

namespace WebmMfVorbisDecLib
{

    HRESULT CreateDecoder(
            IClassFactory*,
            IUnknown*,
            const IID&,
            void**);

}  //end namespace WebmMfVorbisDecLib

static CFactory s_handler_factory(
                    &s_cLock,
                    &WebmMfVorbisDecLib::CreateDecoder);

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
    if (clsid == WebmTypes::CLSID_WebmMfVorbisDec)
        return s_handler_factory.QueryInterface(iid, ppv);

    return CLASS_E_CLASSNOTAVAILABLE;
}


STDAPI DllUnregisterServer()
{
    HRESULT hr = MFTUnregister(WebmTypes::CLSID_WebmMfVorbisDec);
    //assert(SUCCEEDED(hr));  //TODO: dump this it fails

    hr = ComReg::UnRegisterCoclass(WebmTypes::CLSID_WebmMfVorbisDec);

    return hr;
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

#if _DEBUG
    const wchar_t friendly_name[] =
        L"WebM MF Vorbis Decoder Transform (Debug)";
#else
    const wchar_t friendly_name[] =
        L"WebM MF Vorbis Decoder Transform";
#endif

    hr = ComReg::RegisterCoclass(
            WebmTypes::CLSID_WebmMfVorbisDec,
            friendly_name,
            filename,
            L"Webm.MfVorbisDec",
            L"Webm.MfVorbisDec.1",
            false,  //not insertable
            false,  //not a control
            ComReg::kBoth,  //DShow filters must support "both"
            GUID_NULL,     //typelib
            0,    //no version specified
            0);   //no toolbox bitmap

    if (FAILED(hr))
        return hr;

    enum { cInputTypes = 1 };
    MFT_REGISTER_TYPE_INFO pInputTypes[cInputTypes] =
    {
        { MFMediaType_Audio, VorbisTypes::MEDIASUBTYPE_Vorbis2 }
    };

    enum { cOutputTypes = 1 };
    MFT_REGISTER_TYPE_INFO pOutputTypes[cOutputTypes] =
    {
        { MFMediaType_Audio, MFAudioFormat_Float }
    };

    wchar_t* const friendly_name_ = const_cast<wchar_t*>(friendly_name);

    hr = MFTRegister(
            WebmTypes::CLSID_WebmMfVorbisDec,
            MFT_CATEGORY_AUDIO_DECODER,
            friendly_name_,
            MFT_ENUM_FLAG_SYNCMFT,  //TODO: for now, just support sync
            cInputTypes,
            pInputTypes,
            cOutputTypes,
            pOutputTypes,
            0);  //no attributes

    return hr;
}
