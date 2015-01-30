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
#include <comip.h>
#include <shlwapi.h>

#include <cassert>
#include <string>

#include "debugutil.h"
#include "hrtext.h"
#include "ComDllWrapper.h"

namespace WebmMfUtil
{

ComDllWrapper::ComDllWrapper():
  dll_module_(NULL),
  clsid_(GUID_NULL),
  ptrfn_get_class_object_(NULL),
  ref_count_(0)
{
}

ComDllWrapper::~ComDllWrapper()
{
    if (ptr_class_factory_)
    {
        //IClassFactory* ptr_class_factory = ptr_class_factory_.Detach();
        //ptr_class_factory_->LockServer(FALSE);
        ptr_class_factory_ = 0;
    }
    if (NULL != dll_module_)
    {
        FreeLibrary(dll_module_);
        dll_module_ = NULL;
    }
}

UINT ComDllWrapper::AddRef()
{
    return InterlockedIncrement(&ref_count_);
}

UINT ComDllWrapper::Release()
{
    UINT ref_count = InterlockedDecrement(&ref_count_);
    if (0 == ref_count)
    {
        delete this;
    }
    return ref_count;
}

HRESULT ComDllWrapper::Create(std::wstring dll_path, CLSID clsid,
                              ComDllWrapper** ptr_instance)
{
    if (!PathFileExists(dll_path.c_str()))
        return E_INVALIDARG;

    ComDllWrapper* ptr_wrapper = new (std::nothrow) ComDllWrapper();

    if (NULL == ptr_wrapper)
    {
        DBGLOG("ctor failed");
        return E_OUTOFMEMORY;
    }

    ptr_wrapper->dll_path_ = dll_path;
    ptr_wrapper->clsid_ = clsid;

    HRESULT hr = ptr_wrapper->LoadDll_();

    if (FAILED(hr))
    {
        DBGLOG("LoadDll_ failed");
        return hr;
    }
    ptr_wrapper->AddRef();
    *ptr_instance = ptr_wrapper;
    return hr;
}

HRESULT ComDllWrapper::LoadDll_()
{
    dll_module_ = LoadLibrary(dll_path_.c_str());
    if (dll_module_)
    {
        ptrfn_get_class_object_ = reinterpret_cast<DllGetClassObjFunc>(
            GetProcAddress(dll_module_, "DllGetClassObject"));
    }
    HRESULT hr = E_FAIL;
    if (dll_module_ && ptrfn_get_class_object_)
    {
        hr = ptrfn_get_class_object_(clsid_, IID_IClassFactory,
            reinterpret_cast<void**>(&ptr_class_factory_));
        if (FAILED(hr))
        {
            DBGLOG("DllGetClassObject failed" << HRLOG(hr));
        }
        //if (SUCCEEDED(hr) && ptr_class_factory_)
        //{
        //    hr = ptr_class_factory_->LockServer(TRUE);
        //    if (FAILED(hr))
        //    {
        //        DBGLOG("LockServer failed" << HRLOG(hr));
        //        return hr;
        //    }
        //}
    }
    return hr;
}

IClassFactoryPtr ComDllWrapper::GetIClassFactoryPtr() const
{
    return ptr_class_factory_;
}

HRESULT ComDllWrapper::CreateInstance(GUID interface_id, void** ptr_instance)
{
    if (!ptr_class_factory_)
    {
        return E_INVALIDARG;
    }
    return ptr_class_factory_->CreateInstance(NULL, interface_id,
                                              ptr_instance);
}

const wchar_t* ComDllWrapper::GetDllPath() const
{
    return dll_path_.c_str();
}

const CLSID ComDllWrapper::GetClsid() const
{
    return clsid_;
}

} // WebmMfUtil namespace