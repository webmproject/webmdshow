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

class MfByteStreamHandlerWrapper : public IMFAsyncCallback,
                                   public MfObjWrapperBase

{
public:
    static HRESULT Create(std::wstring dll_path, GUID mfobj_clsid,
                          MfByteStreamHandlerWrapper** ptr_bsh_wrapper);
    virtual ~MfByteStreamHandlerWrapper();

    HRESULT OpenURL(std::wstring url);

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IMFAsyncCallback methods
    STDMETHODIMP GetParameters(DWORD*, DWORD*);
    STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult);


private:
    MfByteStreamHandlerWrapper();
    virtual HRESULT Create(std::wstring dll_path, GUID mfobj_clsid);
    _COM_SMARTPTR_TYPEDEF(IMFByteStreamHandler, IID_IMFByteStreamHandler);
    _COM_SMARTPTR_TYPEDEF(IMFByteStream, IID_IMFByteStream);
    _COM_SMARTPTR_TYPEDEF(IMFMediaSource, IID_IMFMediaSource);
    EventWaiter open_event_;
    IMFByteStreamPtr ptr_stream_;
    IMFByteStreamHandlerPtr ptr_handler_;
    IMFMediaSourcePtr ptr_media_src_;
    UINT ref_count_;

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
