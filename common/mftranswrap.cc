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
#include "comdllwrapper.h"
#include "eventutil.h"
#include "hrtext.h"
#include "memutil.h"
#include "mftranswrap.h"
#include "mfutil.h"

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
  ptr_transform_dll_(NULL),
  ref_count_(0),
  buf_size_(0)
{
}

MfTransformWrapper::~MfTransformWrapper()
{
}

HRESULT MfTransformWrapper::Create_(std::wstring dll_path, GUID mfobj_clsid)
{
    HRESULT hr = ComDllWrapper::Create(dll_path, mfobj_clsid,
                                       &ptr_transform_dll_);
    if (FAILED(hr) || !ptr_transform_dll_)
    {
        DBGLOG("ComDllWrapper::Create failed path=" << dll_path.c_str()
            << HRLOG(hr));
        return hr;
    }
    hr = ptr_transform_dll_->CreateInstance(IID_IMFTransform,
        reinterpret_cast<void**>(&ptr_transform_));
    if (FAILED(hr) || !ptr_transform_)
    {
        DBGLOG("GetInterfacePtr failed" << HRLOG(hr));
        return hr;
    }
    return hr;
}

HRESULT MfTransformWrapper::GetInputType(IMFMediaType** ptr_type) const
{
    return copy_media_type(ptr_input_type_, ptr_type);
}

HRESULT MfTransformWrapper::GetOutputType(IMFMediaType** ptr_type) const
{
    return copy_media_type(ptr_output_type_, ptr_type);
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
    // TODO(tomfinegan): relocate |ptr_transform_buffer_| setup
    MFT_OUTPUT_STREAM_INFO stream_info = {0};
    CHK(hr, ptr_transform_->GetOutputStreamInfo(0, &stream_info));
    if (FAILED(hr))
    {
        return hr;
    }
    buf_size_ = stream_info.cbSize;
    CHK(hr, MFCreateMemoryBuffer((DWORD)buf_size_, &ptr_buf_));
    return hr;
}

HRESULT MfTransformWrapper::Transform(IMFSample* ptr_in_sample,
                                      IMFSample** ptr_out_sample)
{
    if (!ptr_in_sample)
    {
        DBGLOG("ERROR, NULL sample, E_INVALIDARG");
        return E_INVALIDARG;
    }
    HRESULT hr;
    CHK(hr, ptr_transform_->ProcessInput(0, ptr_in_sample, 0));
    if (FAILED(hr))
    {
        return hr;
    }
    IMFSamplePtr output_sample;
    CHK(hr, MFCreateSample(&output_sample));
    if (FAILED(hr))
    {
        return hr;
    }
    CHK(hr, output_sample->AddBuffer(ptr_buf_));
    if (FAILED(hr))
    {
        return hr;
    }
    MFT_OUTPUT_DATA_BUFFER mf_sample_buffer = {0};
    mf_sample_buffer.pSample = output_sample;
    // can't use CHK: MF_E_TRANSFORM_NEEDS_MORE_INPUT is an expected error, and
    // we don't want lies about that being an error in debugging output
    hr = ptr_transform_->ProcessOutput(0, 1, &mf_sample_buffer, NULL);
    if (MF_E_TRANSFORM_NEED_MORE_INPUT == hr)
    {
        // caller must discard the input sample: the MFT now owns it
        return S_FALSE;
    }
    if (SUCCEEDED(hr))
    {
        *ptr_out_sample = output_sample.Detach();
    }
    return hr;
}

} // WebmMfUtil namespace
