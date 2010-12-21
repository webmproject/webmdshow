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

HRESULT MfTransformWrapper::CreateInstance(std::wstring dll_path,
                                           GUID mfobj_clsid,
                                           MfTransformWrapper** ptr_instance)
{
    if (dll_path.empty() || GUID_NULL == mfobj_clsid)
    {
        return E_INVALIDARG;
    }
    MfTransformWrapper* ptr_wrapper = new (std::nothrow) MfTransformWrapper();
    if (!ptr_wrapper)
    {
        return E_OUTOFMEMORY;
    }
    HRESULT hr = ptr_wrapper->Create_(dll_path, mfobj_clsid);
    if (SUCCEEDED(hr))
    {
        *ptr_instance = ptr_wrapper;
        ptr_wrapper->AddRef();
    }
    else
    {
        DBGLOG("ERROR, Create_ failed" << HRLOG(hr));
    }
    return hr;
}

HRESULT MfTransformWrapper::QueryInterface(REFIID, void**)
{
    return E_NOTIMPL;
}

ULONG MfTransformWrapper::AddRef()
{
    return InterlockedIncrement(&ref_count_);
}

ULONG MfTransformWrapper::Release()
{
    UINT ref_count = InterlockedDecrement(&ref_count_);
    if (ref_count == 0)
    {
        delete this;
    }
    return ref_count;
}

MfTransformWrapper::MfTransformWrapper():
  ptr_com_dll_(NULL),
  ref_count_(0)
{
}

MfTransformWrapper::~MfTransformWrapper()
{
}

HRESULT MfTransformWrapper::Create_(std::wstring dll_path, GUID mfobj_clsid)
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

HRESULT MfTransformWrapper::SetInputType(IMFMediaTypePtr& ptr_type)
{
    if (!ptr_transform_)
    {
        DBGLOG("ERROR, transform obj not created, E_INVALIDARG");
        return E_INVALIDARG;
    }
    if (!ptr_type)
    {
        DBGLOG("ERROR, media type required, E_INVALIDARG");
        return E_INVALIDARG;
    }
    HRESULT hr;
    // this won't work (for vorbis at least-- SetInputType will fail without
    // the setup headers from the source object!
    //if (!ptr_type)
    //{
    //    hr = ptr_transform_->GetInputAvailableType(0, 0, &ptr_type);
    //    if (FAILED(hr))
    //    {
    //        DBGLOG("GetInputAvailableType failed" << HRLOG(hr));
    //        return hr;
    //    }
    //}
    hr = ptr_transform_->SetInputType(0, ptr_type, 0);
    if (FAILED(hr))
    {
        DBGLOG("IMFTransform::SetInputType failed" << HRLOG(hr));
        return hr;
    }
    ptr_input_type_ = ptr_type;
    return hr;
}

HRESULT MfTransformWrapper::SetOutputType(IMFMediaTypePtr& ptr_type)
{
    if (!ptr_transform_)
    {
        DBGLOG("ERROR, transform obj not created, E_INVALIDARG");
        return E_INVALIDARG;
    }
    HRESULT hr;
    if (!ptr_type)
    {
        hr = ptr_transform_->GetOutputAvailableType(0, 0, &ptr_type);
        if (FAILED(hr))
        {
            DBGLOG("GetOutputAvailableType failed" << HRLOG(hr));
            return hr;
        }
    }
    hr = ptr_transform_->SetOutputType(0, ptr_type, 0);
    if (FAILED(hr))
    {
        DBGLOG("IMFTransform::SetOutputType failed" << HRLOG(hr));
        return hr;
    }
    ptr_output_type_ = ptr_type;
    return hr;
}


} // WebmMfUtil namespace
