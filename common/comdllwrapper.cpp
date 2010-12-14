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

#include "debugutil.hpp"
#include "ComDllWrapper.hpp"

namespace WebmMfUtil
{

ComDllWrapper::ComDllWrapper():
  dll_module_(NULL),
  clsid_(GUID_NULL),
  ptrfn_get_class_object_(NULL)
{
}

ComDllWrapper::~ComDllWrapper()
{
    if (ptr_class_factory_)
    {
        ptr_class_factory_ = 0;
    }
    if (ptr_unk_)
    {
        ptr_unk_ = 0;
    }
    if (NULL != dll_module_)
    {
        FreeLibrary(dll_module_);
        dll_module_ = NULL;
    }
}

HRESULT ComDllWrapper::LoadDll(std::wstring dll_path, CLSID clsid)
{
    if (!PathFileExists(dll_path.c_str()))
        return E_INVALIDARG;

    dll_path_ = dll_path;
    clsid_ = clsid;
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

        if (SUCCEEDED(hr) && ptr_class_factory_)
        {
            hr = ptr_class_factory_->CreateInstance(NULL, IID_IUnknown,
                reinterpret_cast<void**>(&ptr_unk_));
        }
    }

    return hr;
}

IClassFactoryPtr ComDllWrapper::GetIClassFactoryPtr() const
{
    return ptr_class_factory_;
}

HRESULT ComDllWrapper::GetInterfacePtr(GUID interface_id, void** ptr_instance)
{
    if (!ptr_unk_)
    {
        return E_INVALIDARG;
    }
    return ptr_unk_->QueryInterface(interface_id, ptr_instance);
}

} // WebmMfUtil namespace