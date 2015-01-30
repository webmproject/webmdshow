// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_MFTRANSWRAP_HPP__
#define __WEBMDSHOW_COMMON_MFTRANSWRAP_HPP__

#include "debugutil.h"
#include "memutilfwd.h"

namespace WebmMfUtil
{

class ComDllWrapper;
_COM_SMARTPTR_TYPEDEF(IMFMediaType, IID_IMFMediaType);
_COM_SMARTPTR_TYPEDEF(IMFSample, IID_IMFSample);

class MfTransformWrapper : public IUnknown
{
public:
    static HRESULT CreateInstance(std::wstring dll_path, GUID mfobj_clsid,
                                  MfTransformWrapper** ptr_instance);
    HRESULT Transform(IMFSample* ptr_in_sample,
                      IMFSample** ptr_out_sample);
    HRESULT GetInputType(IMFMediaType** ptr_type) const;
    HRESULT GetOutputType(IMFMediaType** ptr_type) const;
    HRESULT SetInputType(IMFMediaTypePtr& ptr_type);
    HRESULT SetOutputType(IMFMediaTypePtr& ptr_type);
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
private:
    _COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, IID_IMFMediaBuffer);
    _COM_SMARTPTR_TYPEDEF(IMFTransform, IID_IMFTransform);

    MfTransformWrapper();
    ~MfTransformWrapper();
    HRESULT Create_(std::wstring dll_path, GUID mfobj_clsid);
    
    ComDllWrapper* ptr_transform_dll_;
    IMFMediaBufferPtr ptr_buf_;
    IMFMediaTypePtr ptr_input_type_;
    IMFMediaTypePtr ptr_output_type_;
    IMFTransformPtr ptr_transform_;
    size_t buf_size_;
    ULONG ref_count_;

    DISALLOW_COPY_AND_ASSIGN(MfTransformWrapper);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_MFTRANSWRAP_HPP__
