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
#include "hrtext.hpp"
#include "mfutil.hpp"

namespace WebmMfUtil
{

HRESULT get_major_type(IMFStreamDescriptor* ptr_desc, GUID* ptr_type)
{
    if (!ptr_desc || !ptr_type)
    {
        return E_INVALIDARG;
    }
    _COM_SMARTPTR_TYPEDEF(IMFMediaTypeHandler, IID_IMFMediaTypeHandler);
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

} // WebmMfUtil namespace
