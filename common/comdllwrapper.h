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
    ~ComDllWrapper();
    const wchar_t* GetDllPath() const;
    const CLSID GetClsid() const;
    HRESULT CreateInstance(GUID interface_id, void** ptr_instance);
    IClassFactoryPtr GetIClassFactoryPtr() const;
    static HRESULT Create(std::wstring dll_path, CLSID clsid,
                          ComDllWrapper** ptr_instance);
    UINT AddRef();
    UINT Release();

private:
    ComDllWrapper();
    HRESULT LoadDll_();

    CLSID clsid_;
    DllGetClassObjFunc ptrfn_get_class_object_;
    HMODULE dll_module_;
    IClassFactoryPtr ptr_class_factory_;
    std::wstring dll_path_;
    UINT ref_count_;

    DISALLOW_COPY_AND_ASSIGN(ComDllWrapper);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_COMDLLWRAPPER_HPP__
