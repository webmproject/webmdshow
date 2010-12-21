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

#include "debugutil.hpp"
#include "eventutil.hpp"
#include "mfutil.hpp"

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

} // WebmMfUtil namespace
