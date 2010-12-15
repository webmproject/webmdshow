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
#include <shlwapi.h>

#include <comdef.h>
#include <string>
#include <vector>

#include "debugutil.hpp"
#include "eventutil.hpp"
#include "hrtext.hpp"
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

MfByteStreamHandlerWrapper::MfByteStreamHandlerWrapper():
  audio_stream_count_(0),
  video_stream_count_(0),
  ref_count_(0),
  stream_count_(0)
{
}

MfByteStreamHandlerWrapper::~MfByteStreamHandlerWrapper()
{
    while (!audio_streams_.empty())
    {
        IMFStreamDescriptor* ptr_desc = audio_streams_.back();
        ptr_desc->Release();
        audio_streams_.pop_back();
    }
    while (!video_streams_.empty())
    {
        IMFStreamDescriptor* ptr_desc = video_streams_.back();
        ptr_desc->Release();
        video_streams_.pop_back();
    }
    if (ptr_media_src_)
    {
        ptr_media_src_->Shutdown();
        ptr_media_src_ = 0;
    }
    if (ptr_stream_)
    {
        ptr_stream_->Close();
        ptr_stream_ = 0;
    }
    if (ptr_handler_)
    {
        ptr_handler_ = 0;
    }
}

HRESULT MfByteStreamHandlerWrapper::Create(std::wstring dll_path,
                                           GUID mfobj_clsid)
{
    HRESULT hr = ComDllWrapper::Create(dll_path, mfobj_clsid, &ptr_com_dll_);
    if (FAILED(hr) || !ptr_com_dll_)
    {
        DBGLOG("ComDllWrapper::Create failed path=" << dll_path.c_str()
            << HRLOG(hr));
        return hr;
    }
    hr = ptr_com_dll_->CreateInstance(IID_IMFByteStreamHandler,
        reinterpret_cast<void**>(&ptr_handler_));
    if (FAILED(hr) || !ptr_handler_)
    {
        DBGLOG("GetInterfacePtr failed" << HRLOG(hr));
        return hr;
    }
    hr = open_event_.Create();
    if (FAILED(hr))
    {
        DBGLOG("open event creation failed" << HRLOG(hr));
        return hr;
    }

    return hr;
}

HRESULT MfByteStreamHandlerWrapper::Create(
    std::wstring dll_path,
    GUID mfobj_clsid,
    MfByteStreamHandlerWrapper** ptr_bsh_instance)
{
    *ptr_bsh_instance = new (std::nothrow) MfByteStreamHandlerWrapper();

    if (!*ptr_bsh_instance)
    {
        DBGLOG("ctor failed");
        return E_OUTOFMEMORY;
    }

    MfByteStreamHandlerWrapper* ptr_bsh_wrapper = *ptr_bsh_instance;

    HRESULT hr = ptr_bsh_wrapper->Create(dll_path, mfobj_clsid);

    if (SUCCEEDED(hr))
    {
        ptr_bsh_wrapper->AddRef();
    }
    else
    {
        DBGLOG("ERROR, wrapper create failed" << HRLOG(hr));
    }

    return hr;
}


HRESULT MfByteStreamHandlerWrapper::OpenURL(std::wstring url)
{
    if (1 > url.length())
    {
        DBGLOG("ERROR, empty url string");
        return E_INVALIDARG;
    }
    if (!ptr_handler_)
    {
        DBGLOG("ERROR, byte stream handler NULL");
        return E_OUTOFMEMORY;
    }
    HRESULT hr = MFCreateFile(MF_ACCESSMODE_READ,
                              MF_OPENMODE_FAIL_IF_NOT_EXIST,
                              MF_FILEFLAGS_NONE, url.c_str(), &ptr_stream_);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, MFCreateFile failed" << HRLOG(hr));
        return hr;
    }
    hr = ptr_handler_->BeginCreateObject(ptr_stream_, url.c_str(), 0, NULL,
                                         NULL, this, NULL);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, byte stream handler BeginCreateObject failed, hr="
            << hr);
        return hr;
    }
    hr = open_event_.Wait();
    if (FAILED(hr))
    {
        DBGLOG("ERROR, open event wait failed" << HRLOG(hr));
        return hr;
    }
    if (!ptr_media_src_)
    {
        DBGLOG("ERROR, NULL media source");
        hr = E_OUTOFMEMORY;
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(MfByteStreamHandlerWrapper, IMFAsyncCallback),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

ULONG MfByteStreamHandlerWrapper::AddRef()
{
    return InterlockedIncrement(&ref_count_);
}

ULONG MfByteStreamHandlerWrapper::Release()
{
    UINT ref_count = InterlockedDecrement(&ref_count_);
    if (ref_count == 0)
    {
        delete this;
    }
    return ref_count;
}

// IMFAsyncCallback method
STDMETHODIMP MfByteStreamHandlerWrapper::GetParameters(DWORD*, DWORD*)
{
    // Implementation of this method is optional.
    return E_NOTIMPL;
}

// IMFAsyncCallback method
STDMETHODIMP MfByteStreamHandlerWrapper::Invoke(IMFAsyncResult* pAsyncResult)
{
    IUnknownPtr ptr_unk_media_src_;
    MF_OBJECT_TYPE obj_type;
    HRESULT hr = ptr_handler_->EndCreateObject(pAsyncResult, &obj_type,
                                               &ptr_unk_media_src_);
    if (FAILED(hr) || !ptr_unk_media_src_)
    {
        DBGLOG("ERROR, byte stream handler EndCreateObject failed"
            << HRLOG(hr));
    }
    ptr_media_src_ = ptr_unk_media_src_;
    return open_event_.Set();
}

HRESULT MfByteStreamHandlerWrapper::LoadMediaStreams()
{
    HRESULT hr = ptr_media_src_->CreatePresentationDescriptor(&ptr_pres_desc_);
    if (FAILED(hr) || !ptr_pres_desc_)
    {
        DBGLOG("ERROR, CreatePresentationDescriptor failed" << HRLOG(hr));
        return hr;
    }
    hr = ptr_pres_desc_->GetStreamDescriptorCount(&stream_count_);
    if (FAILED(hr) || !ptr_pres_desc_)
    {
        DBGLOG("ERROR, GetStreamDescriptorCount failed" << HRLOG(hr));
        return hr;
    }
    // get stream descriptors and store audio/video descs in our vectors
    for (DWORD i = 0; i < stream_count_; ++i)
    {
        BOOL selected;
        IMFStreamDescriptor* ptr_desc;
        hr = ptr_pres_desc_->GetStreamDescriptorByIndex(i, &selected,
                                                        &ptr_desc);
        if (FAILED(hr) || !ptr_desc)
        {
            DBGLOG("ERROR, GetStreamDescriptorByIndex failed, count="
                << stream_count_ << " index=" << i << HRLOG(hr));
            return hr;
        }
        // TODO(tomfinegan): decide what to do w/unselected streams
        if (selected)
        {
            IMFMediaTypeHandlerPtr ptr_media_type_handler;
            hr = ptr_desc->GetMediaTypeHandler(&ptr_media_type_handler);
            if (FAILED(hr) || !ptr_media_type_handler)
            {
                DBGLOG("ERROR, GetMediaTypeHandler failed, count="
                    << stream_count_ << " index=" << i << HRLOG(hr));
                goto get_media_stream_type_handler_fail;
            }
            GUID major_type;
            hr = ptr_media_type_handler->GetMajorType(&major_type);
            if (FAILED(hr))
            {
                DBGLOG("ERROR, GetMajorType failed, count=" << stream_count_
                    << " index=" << i << HRLOG(hr));
                goto get_media_stream_type_handler_fail;
            }
            if (MFMediaType_Audio == major_type)
            {
                audio_streams_.push_back(ptr_desc);
            }
            else if (MFMediaType_Video == major_type)
            {
                video_streams_.push_back(ptr_desc);
            }
            ++selected_stream_count_;
            get_media_stream_type_handler_fail:
                if (FAILED(hr) && ptr_desc)
                {
                    ptr_desc->Release();
                }
        }
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
