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

#include "debugutil.hpp"
#include "comdllwrapper.hpp"
#include "eventutil.hpp"
#include "hrtext.hpp"
#include "mftranswrap.hpp"

namespace WebmMfUtil
{

MfTransformWrapper::MfTransformWrapper():
  ptr_com_dll_(NULL)
{
}

MfTransformWrapper::~MfTransformWrapper()
{
}

HRESULT MfTransformWrapper::Create(std::wstring dll_path, GUID mfobj_clsid)
{
    HRESULT hr = ComDllWrapper::Create(dll_path, mfobj_clsid, &ptr_com_dll_);
    if (FAILED(hr) || !ptr_com_dll_)
    {
        DBGLOG("ComDllWrapper::Create failed path=" << dll_path.c_str()
            << HRLOG(hr));
        return hr;
    }
    hr = ptr_com_dll_->CreateInstance(IID_IMFTransform,
                                reinterpret_cast<void**>(&ptr_transform_));
    if (FAILED(hr) || !ptr_transform_)
    {
        DBGLOG("GetInterfacePtr failed" << HRLOG(hr));
        return hr;
    }
    return hr;
}

} // WebmMfUtil namespace