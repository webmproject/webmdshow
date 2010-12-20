// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_MFTRANSWRAP_HPP__
#define __WEBMDSHOW_COMMON_MFTRANSWRAP_HPP__

namespace WebmMfUtil
{

class ComDllWrapper;

class MfTransformWrapper
{
public:
    MfTransformWrapper();
    virtual ~MfTransformWrapper();
    virtual HRESULT Create(std::wstring dll_path, GUID mfobj_clsid);

private:
    _COM_SMARTPTR_TYPEDEF(IMFTransform, IID_IMFTransform);

    ComDllWrapper* ptr_com_dll_;
    IMFTransformPtr ptr_transform_;

    DISALLOW_COPY_AND_ASSIGN(MfTransformWrapper);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_MFTRANSWRAP_HPP__
