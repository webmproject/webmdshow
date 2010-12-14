// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <windowsx.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>

#include <comdef.h>
#include <string>

#include "debugutil.hpp"
#include "comreg.hpp"
#include "comdllwrapper.hpp"
#include "mfobjwrapper.hpp"
#include "webmtypes.hpp"

namespace WebmMfUtil
{

MfObjWrapperBase::MfObjWrapperBase()
{
}

MfObjWrapperBase::~MfObjWrapperBase()
{
}

MfByteStreamHandlerWrapper::MfByteStreamHandlerWrapper()
{
}

MfByteStreamHandlerWrapper::~MfByteStreamHandlerWrapper()
{
}

HRESULT MfByteStreamHandlerWrapper::Create(std::wstring dll_path,
                                           GUID mfobj_clsid)
{
    HRESULT hr = com_dll_.LoadDll(dll_path, mfobj_clsid);
    if (FAILED(hr))
    {
        DBGLOG("LoadDll failed path=" << dll_path.c_str() << ", hr=" << hr);
        return hr;
    }
    hr = com_dll_.GetInterfacePtr(IID_IMFByteStreamHandler,
                                  reinterpret_cast<void**>(&ptr_bsh_));
    if (FAILED(hr) || !ptr_bsh_)
    {
        DBGLOG("GetInterfacePtr failed, hr=" << hr);
        return hr;
    }
    return hr;
}

MfTransformWrapper::MfTransformWrapper()
{
}

MfTransformWrapper::~MfTransformWrapper()
{
}

HRESULT MfTransformWrapper::Create(std::wstring dll_path, GUID mfobj_clsid)
{
    HRESULT hr = com_dll_.LoadDll(dll_path, mfobj_clsid);
    if (FAILED(hr))
    {
        DBGLOG("LoadDll failed path=" << dll_path.c_str() << ", hr=" << hr);
        return hr;
    }
    hr = com_dll_.GetInterfacePtr(IID_IMFTransform,
                                  reinterpret_cast<void**>(&ptr_transform_));
    if (FAILED(hr) || !ptr_transform_)
    {
        DBGLOG("GetInterfacePtr failed, hr=" << hr);
        return hr;
    }
    return hr;
}

} // WebmMfUtil namespace
