// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_COMDLLWRAPPER_HPP__
#define __WEBMDSHOW_COMMON_COMDLLWRAPPER_HPP__

namespace WebmMfUtil
{

typedef HRESULT (*DllGetClassObjFunc)(REFCLSID rclsid, REFIID riid,
                                      LPVOID *ppv);

class ComDllWrapper
{
public:
    ComDllWrapper();
    ~ComDllWrapper();
    HRESULT LoadDll(std::wstring dll_path, CLSID mf_clsid);
    IClassFactoryPtr GetIClassFactoryPtr() const;
    HRESULT GetInterfacePtr(GUID interface_id, void** ptr_instance);
    const wchar_t* GetDllPath() const;
    const CLSID GetClsid() const;

private:
    CLSID clsid_;
    HMODULE dll_module_;
    std::wstring dll_path_;
    DllGetClassObjFunc ptrfn_get_class_object_;

    IClassFactoryPtr ptr_class_factory_;
    IUnknownPtr ptr_unk_;

    DISALLOW_COPY_AND_ASSIGN(ComDllWrapper);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_COMDLLWRAPPER_HPP__
