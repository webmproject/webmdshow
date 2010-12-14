// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_MFOBJWRAPPER_HPP__
#define __WEBMDSHOW_COMMON_MFOBJWRAPPER_HPP__

namespace WebmMfUtil
{

class MfObjWrapperBase
{
public:
    MfObjWrapperBase();
    virtual ~MfObjWrapperBase();
    virtual HRESULT Create(std::wstring dll_path, GUID mfobj_clsid) = 0;
protected:
    ComDllWrapper com_dll_;

    DISALLOW_COPY_AND_ASSIGN(MfObjWrapperBase);
};

class MfByteStreamHandlerWrapper : public MfObjWrapperBase
{
public:
    MfByteStreamHandlerWrapper();
    virtual ~MfByteStreamHandlerWrapper();
    virtual HRESULT Create(std::wstring dll_path, GUID mfobj_clsid);

private:
    _COM_SMARTPTR_TYPEDEF(IMFByteStreamHandler, IID_IMFByteStreamHandler);
    IMFByteStreamHandlerPtr ptr_bsh_;

    DISALLOW_COPY_AND_ASSIGN(MfByteStreamHandlerWrapper);
};

class MfTransformWrapper : public MfObjWrapperBase
{
public:
    MfTransformWrapper();
    virtual ~MfTransformWrapper();
    virtual HRESULT Create(std::wstring dll_path, GUID mfobj_clsid);

private:
    _COM_SMARTPTR_TYPEDEF(IMFTransform, IID_IMFTransform);
    IMFTransformPtr ptr_transform_;

    DISALLOW_COPY_AND_ASSIGN(MfTransformWrapper);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_MFOBJWRAPPER_HPP__
