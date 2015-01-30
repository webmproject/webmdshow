// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <windowsx.h>
#include <comdef.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>

#include <string>

#include "debugutil.h"
#include "eventutil.h"
#include "memutil.h"
#include "mfsrcwrap.h"
#include "mftranswrap.h"
#include "mfutil.h"
// TODO(tomfinegan): relocate mf object dll paths include
#include "tests/mfdllpaths.h"
#include "webmtypes.h"

namespace WebmMfUtil
{

_COM_SMARTPTR_TYPEDEF(IMFMediaTypeHandler, IID_IMFMediaTypeHandler);

HRESULT copy_media_type(IMFMediaType* ptr_src, IMFMediaType** ptr_dest)
{
    if (!ptr_src)
    {
        return E_INVALIDARG;
    }
    IMFMediaType* ptr_type;
    HRESULT hr = MFCreateMediaType(&ptr_type);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, MFCreateMediaType failed" << HRLOG(hr));
        return hr;
    }
    hr = ptr_src->CopyAllItems(ptr_type);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, CopyAllItems failed" << HRLOG(hr));
        return hr;
    }
    *ptr_dest = ptr_type;
    return hr;
}

HRESULT get_event_iunk_ptr(IMFMediaEvent* ptr_event, IUnknown** ptr_iunk)
{
    if (!ptr_event)
    {
        return E_INVALIDARG;
    }
    PROPVARIANT event_val;
    PropVariantInit(&event_val);
    HRESULT hr = ptr_event->GetValue(&event_val);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, could not get event value" << HRLOG(hr));
        return hr;
    }
    *ptr_iunk = event_val.punkVal;
    return hr;
}

HRESULT get_media_type(IMFStreamDescriptor* ptr_desc, IMFMediaType** ptr_type)
{
    if (!ptr_desc)
    {
        return E_INVALIDARG;
    }
    IMFMediaTypeHandlerPtr ptr_media_type_handler;
    HRESULT hr = ptr_desc->GetMediaTypeHandler(&ptr_media_type_handler);
    if (FAILED(hr) || !ptr_media_type_handler)
    {
        DBGLOG("ERROR, GetMediaTypeHandler failed" << HRLOG(hr));
        return hr;
    }
    _COM_SMARTPTR_TYPEDEF(IMFMediaType, IID_IMFMediaType);
    IMFMediaTypePtr ptr_temp_mt;
    hr = ptr_media_type_handler->GetCurrentMediaType(&ptr_temp_mt);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, GetCurrentMediaType failed" << HRLOG(hr));
        return hr;
    }
    return copy_media_type(ptr_temp_mt, ptr_type);
}

HRESULT get_major_type(IMFStreamDescriptor* ptr_desc, GUID* ptr_type)
{
    if (!ptr_desc || !ptr_type)
    {
        return E_INVALIDARG;
    }
    IMFMediaTypeHandlerPtr ptr_media_type_handler;
    HRESULT hr = ptr_desc->GetMediaTypeHandler(&ptr_media_type_handler);
    if (FAILED(hr) || !ptr_media_type_handler)
    {
        DBGLOG("ERROR, GetMediaTypeHandler failed" << HRLOG(hr));
        return hr;
    }
    hr = ptr_media_type_handler->GetMajorType(ptr_type);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, GetMajorType failed" << HRLOG(hr));
        return hr;
    }
    return hr;
}

HRESULT get_sub_type(IMFStreamDescriptor* ptr_desc, GUID* ptr_sub_type)
{
    if (!ptr_desc || !ptr_sub_type)
    {
        return E_INVALIDARG;
    }
    _COM_SMARTPTR_TYPEDEF(IMFMediaType, IID_IMFMediaType);
    IMFMediaTypePtr ptr_type;
    HRESULT hr = get_media_type(ptr_desc, &ptr_type);
    if (FAILED(hr) || !ptr_type)
    {
        DBGLOG("ERROR, get_media_type failed" << HRLOG(hr));
        return hr;
    }
    hr = ptr_type->GetGUID(MF_MT_SUBTYPE, ptr_sub_type);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, GetGUID MF_MT_SUBTYPE failed" << HRLOG(hr));
        return hr;
    }
    return hr;
}

HRESULT mf_startup()
{
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, MFStartup failed, hr=" << hr);
    }
    return hr;
}

HRESULT mf_shutdown()
{
    HRESULT hr = MFShutdown();
    if (FAILED(hr))
    {
        DBGLOG("ERROR, MFShutdown failed, hr=" << hr);
    }
    return hr;
}

HRESULT get_webm_vorbis_sample(MfByteStreamHandlerWrapper* ptr_source,
                               IMFSample** ptr_out_sample)
{
    if (!ptr_source)
    {
        DBGLOG("ERROR null ptr_source, E_INVALIDARG");
        return E_INVALIDARG;
    }
    HRESULT hr;
    IMFSamplePtr ptr_sample;
    CHK(hr, ptr_source->GetAudioSample(&ptr_sample));
    if (FAILED(hr))
    {
        return hr;
    }
    if (!ptr_sample)
    {
        DBGLOG("ERROR null ptr_source, E_FAIL");
        return E_FAIL;
    }
    DWORD buffer_count = 0;
    CHK(hr, ptr_sample->GetBufferCount(&buffer_count));
    if (FAILED(hr))
    {
        return hr;
    }
    if (1 != buffer_count)
    {
        DBGLOG("ERROR sample buffer_count != 1");
        return E_UNEXPECTED;
    }
    *ptr_out_sample = ptr_sample.Detach();
    return hr;
}

HRESULT get_webm_vp8_sample(MfByteStreamHandlerWrapper* ptr_source,
                            IMFSample** ptr_out_sample)
{
    if (!ptr_source)
    {
        DBGLOG("ERROR null ptr_source, E_INVALIDARG");
        return E_INVALIDARG;
    }
    HRESULT hr;
    IMFSamplePtr ptr_sample;
    CHK(hr, ptr_source->GetVideoSample(&ptr_sample));
    if (FAILED(hr))
    {
        return hr;
    }
    if (!ptr_sample)
    {
        DBGLOG("ERROR null ptr_source, E_FAIL");
        return E_FAIL;
    }
    DWORD buffer_count = 0;
    CHK(hr, ptr_sample->GetBufferCount(&buffer_count));
    if (FAILED(hr))
    {
        return hr;
    }
    if (1 != buffer_count)
    {
        DBGLOG("ERROR sample buffer_count != 1");
        return E_UNEXPECTED;
    }
    *ptr_out_sample = ptr_sample.Detach();
    return hr;
}

HRESULT open_webm_source(const std::wstring& dll_path,
                         const std::wstring& url,
                         MfByteStreamHandlerWrapper** ptr_wrapper_instance)
{
    MfByteStreamHandlerWrapper* ptr_mf_bsh = NULL;
    HRESULT hr;
    using WebmTypes::CLSID_WebmMfByteStreamHandler;
    CHK(hr, MfByteStreamHandlerWrapper::Create(dll_path,
                                               CLSID_WebmMfByteStreamHandler,
                                               &ptr_mf_bsh));
    if (FAILED(hr))
    {
        return hr;
    }
    CHK(hr, ptr_mf_bsh->OpenURL(url));
    if (FAILED(hr))
    {
        return hr;
    }
    CHK(hr, ptr_mf_bsh->LoadMediaStreams());
    if (SUCCEEDED(hr))
    {
        *ptr_wrapper_instance = ptr_mf_bsh;
    }
    return hr;
}

HRESULT open_webm_decoder(const std::wstring& dll_path, const GUID& clsid,
                          MfTransformWrapper** ptr_decoder_instance)
{
    MfTransformWrapper* ptr_transform = NULL;
    HRESULT hr;
    CHK(hr, MfTransformWrapper::CreateInstance(dll_path, clsid,
                                               &ptr_transform));
    if (SUCCEEDED(hr))
    {
        *ptr_decoder_instance = ptr_transform;
    }
    return hr;
}

HRESULT setup_webm_decode(MfByteStreamHandlerWrapper* ptr_source,
                          MfTransformWrapper* ptr_decoder,
                          const GUID& major_type)
{
    if (!ptr_source || !ptr_decoder)
    {
        DBGLOG("ERROR NULL ptr_source or ptr_decoder.");
        return E_INVALIDARG;
    }
    HRESULT hr = E_INVALIDARG;
    IMFMediaTypePtr ptr_type;
    // get output type from |ptr_source|
    if (MFMediaType_Audio == major_type)
    {
        CHK(hr, ptr_source->GetAudioMediaType(&ptr_type));
    }
    else if (MFMediaType_Video == major_type)
    {
        CHK(hr, ptr_source->GetVideoMediaType(&ptr_type));
    }
    if (FAILED(hr))
    {
        return hr;
    }
    // set |ptr_decoder| input type to the output type from |ptr_source|
    CHK(hr, ptr_decoder->SetInputType(ptr_type));
    if (FAILED(hr))
    {
        return hr;
    }
    // clear |ptr_type|: using an empty type lets the decoder use its default
    ptr_type = 0;
    CHK(hr, ptr_decoder->SetOutputType(ptr_type));
    return hr;
}

HRESULT setup_webm_vorbis_decoder(const std::wstring& url,
                                  MfByteStreamHandlerWrapper** ptr_bsh,
                                  MfTransformWrapper** ptr_transform)
{
    HRESULT hr;
    using WebmUtil::auto_ref_counted_obj_ptr;
    auto_ref_counted_obj_ptr<MfByteStreamHandlerWrapper> ptr_source(NULL);
    CHK(hr, open_webm_source(WEBM_SOURCE_PATH, url, &ptr_source));
    if (FAILED(hr))
    {
        return hr;
    }
    CHK(hr, ptr_source->Start(false, 0LL));
    if (FAILED(hr))
    {
        return hr;
    }
    if (ptr_source->GetAudioStreamCount() < 1)
    {
        DBGLOG("ERROR no audio streams.");
        return E_INVALIDARG;
    }
    using WebmTypes::CLSID_WebmMfVorbisDec;
    auto_ref_counted_obj_ptr<MfTransformWrapper> ptr_decoder(NULL);
    CHK(hr, open_webm_decoder(VORBISDEC_PATH, CLSID_WebmMfVorbisDec,
                              &ptr_decoder));
    if (FAILED(hr))
    {
        return hr;
    }
    CHK(hr, setup_webm_decode(ptr_source, ptr_decoder, MFMediaType_Audio));
    if (SUCCEEDED(hr))
    {
        *ptr_bsh = ptr_source.detach();
        *ptr_transform = ptr_decoder.detach();
    }
    return hr;
}

HRESULT setup_webm_vp8_decoder(const std::wstring& url,
                               MfByteStreamHandlerWrapper** ptr_bsh,
                               MfTransformWrapper** ptr_transform)
{
    HRESULT hr;
    using WebmUtil::auto_ref_counted_obj_ptr;
    auto_ref_counted_obj_ptr<MfByteStreamHandlerWrapper> ptr_source(NULL);
    CHK(hr, open_webm_source(WEBM_SOURCE_PATH, url, &ptr_source));
    if (FAILED(hr))
    {
        return hr;
    }
    CHK(hr, ptr_source->Start(false, 0LL));
    if (FAILED(hr))
    {
        return hr;
    }
    if (ptr_source->GetVideoStreamCount() < 1)
    {
        DBGLOG("ERROR no video streams.");
        return E_INVALIDARG;
    }
    using WebmTypes::CLSID_WebmMfVp8Dec;
    auto_ref_counted_obj_ptr<MfTransformWrapper> ptr_decoder(NULL);
    CHK(hr, open_webm_decoder(VP8DEC_PATH, CLSID_WebmMfVp8Dec, &ptr_decoder));
    if (FAILED(hr))
    {
        return hr;
    }
    CHK(hr, setup_webm_decode(ptr_source, ptr_decoder, MFMediaType_Video));
    if (SUCCEEDED(hr))
    {
        *ptr_bsh = ptr_source.detach();
        *ptr_transform = ptr_decoder.detach();
    }
    return hr;
}

} // WebmMfUtil namespace
