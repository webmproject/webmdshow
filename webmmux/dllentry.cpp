// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "cfactory.hpp"
#include "comreg.hpp"
#include "webmtypes.hpp"
#include "vorbistypes.hpp"
#include "graphutil.hpp"
#include <cassert>
#include <comdef.h>
#include <uuids.h>

static ULONG s_cLock;

namespace WebmMux
{
    extern HMODULE s_hModule = 0;
    
    HRESULT CreateInstance(
            IClassFactory*,
            IUnknown*, 
            const IID&, 
            void**);

}  //end namespace WebmMux


static CFactory s_factory(&s_cLock, &WebmMux::CreateInstance);


BOOL APIENTRY DllMain(
    HINSTANCE hModule, 
    DWORD  dwReason, 
    LPVOID)
{
	switch (dwReason)
	{
	    case DLL_PROCESS_ATTACH:
        {
            WebmMux::s_hModule = hModule;
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
    if (clsid == WebmTypes::CLSID_WebmMux)
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
                    WebmTypes::CLSID_WebmMux);
                    
    //assert(SUCCEEDED(hr));

    hr = ComReg::UnRegisterCoclass(WebmTypes::CLSID_WebmMux);

    return SUCCEEDED(hr) ? S_OK : S_FALSE;
}


STDAPI DllRegisterServer()
{
    std::wstring filename_;

    HRESULT hr = ComReg::ComRegGetModuleFileName(WebmMux::s_hModule, filename_);
    assert(SUCCEEDED(hr));
    assert(!filename_.empty());
    
    const wchar_t* const filename = filename_.c_str();

#if _DEBUG    
    const wchar_t friendlyname[] = L"WebM Muxer Filter (Debug)";
#else
    const wchar_t friendlyname[] = L"WebM Muxer Filter";
#endif

    hr = DllUnregisterServer();
    assert(SUCCEEDED(hr));
    
    hr = ComReg::RegisterCoclass(
            WebmTypes::CLSID_WebmMux,
            friendlyname,
            filename,
            L"Webm.Muxer",
            L"Webm.Muxer.1",
            false,          //not insertable
            false,          //not a control
            ComReg::kBoth,  //DShow filters must support "both"
            GUID_NULL,      //typelib
            0,              //no version specified
            0);             //no toolbox bitmap
            
    //hr = ComReg::RegisterTypeLibResource(filename, 0);
    //assert(SUCCEEDED(hr));

    const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
    assert(bool(pMapper));

    REGFILTERPINS pins[3];
    
    //video inpin
    
    REGFILTERPINS& inpin_v = pins[0];
    
    inpin_v.strName = 0;              //obsolete
    inpin_v.bRendered = TRUE;  //TODO: correct value?
    inpin_v.bOutput = FALSE;
    inpin_v.bZero = FALSE;
    inpin_v.bMany = FALSE;
    inpin_v.clsConnectsToFilter = 0;  //obsolete
    inpin_v.strConnectsToPin = 0;     //obsolete
    
    enum { kVideoTypes = 1 };

    const REGPINTYPES inpintypes_v[kVideoTypes] =
    {
        { &MEDIATYPE_Video, &WebmTypes::MEDIASUBTYPE_VP80 }
    };

    inpin_v.nMediaTypes = kVideoTypes;          
    inpin_v.lpMediaType = inpintypes_v;

    //audio inpin

    REGFILTERPINS& inpin_a = pins[1];

    inpin_a.strName = 0;
    inpin_a.bRendered = TRUE;  //TODO: correct value?
    inpin_a.bOutput = FALSE;
    inpin_a.bZero = FALSE;
    inpin_a.bMany = FALSE;
    inpin_a.clsConnectsToFilter = 0;
    inpin_a.strConnectsToPin = 0;
    
    enum { kAudioTypes = 2 };
    
    const REGPINTYPES inpintypes_a[kAudioTypes] =
    {
        //matroska.org:
        { &MEDIATYPE_Audio, &VorbisTypes::MEDIASUBTYPE_Vorbis2 },  
        
        //xiph.org:
        { &MEDIATYPE_Audio, &VorbisTypes::MEDIASUBTYPE_Vorbis }
    };
    
    inpin_a.nMediaTypes = kAudioTypes;
    inpin_a.lpMediaType = inpintypes_a;

    //stream outpin
    
    REGFILTERPINS& outpin = pins[2];
    
    outpin.strName = 0;              //obsolete
    outpin.bRendered = FALSE;
    outpin.bOutput = TRUE;
    outpin.bZero = FALSE;
    outpin.bMany = FALSE;
    outpin.clsConnectsToFilter = 0;  //obsolete
    outpin.strConnectsToPin = 0;     //obsolete
    
    enum { kOutpinTypes = 1 };

    const REGPINTYPES outpin_types[kOutpinTypes] =
    {
        { &MEDIATYPE_Stream, &WebmTypes::MEDIASUBTYPE_WEBM }
    };
    
    outpin.nMediaTypes = kOutpinTypes;          
    outpin.lpMediaType = outpin_types;
    
    //pin setup complete
    
    REGFILTER2 filter;
    
    filter.dwVersion = 1;
    filter.dwMerit = MERIT_DO_NOT_USE;
    filter.cPins = 3;
    filter.rgPins = pins;
    
    hr = pMapper->RegisterFilter(
            WebmTypes::CLSID_WebmMux,
            friendlyname,
            0,
            &CLSID_LegacyAmFilterCategory,
            0,
            &filter);                       

    return hr;
}
