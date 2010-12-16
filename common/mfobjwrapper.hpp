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
    ComDllWrapper* ptr_com_dll_;
    DISALLOW_COPY_AND_ASSIGN(MfObjWrapperBase);
};



class MfByteStreamHandlerWrapper : public IMFAsyncCallback,
                                   public MfObjWrapperBase

{
public:
    static HRESULT Create(std::wstring dll_path, GUID mfobj_clsid,
                          MfByteStreamHandlerWrapper** ptr_bsh_wrapper);
    virtual ~MfByteStreamHandlerWrapper();
    HRESULT LoadMediaStreams();
    HRESULT OpenURL(std::wstring url);
    HRESULT Pause();
    HRESULT Start(LONGLONG start_time);
    HRESULT Stop();
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    // IMFAsyncCallback methods
    STDMETHODIMP GetParameters(DWORD*, DWORD*);
    STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult);
private:
    _COM_SMARTPTR_TYPEDEF(IMFByteStreamHandler, IID_IMFByteStreamHandler);
    _COM_SMARTPTR_TYPEDEF(IMFByteStream, IID_IMFByteStream);
    _COM_SMARTPTR_TYPEDEF(IMFMediaEvent, IID_IMFMediaEvent);
    _COM_SMARTPTR_TYPEDEF(IMFMediaSource, IID_IMFMediaSource);
    _COM_SMARTPTR_TYPEDEF(IMFMediaStream, IID_IMFMediaStream);
    _COM_SMARTPTR_TYPEDEF(IMFPresentationDescriptor,
                          IID_IMFPresentationDescriptor);
    _COM_SMARTPTR_TYPEDEF(IMFMediaTypeHandler, IID_IMFMediaTypeHandler);
    _COM_SMARTPTR_TYPEDEF(IMFMediaEventGenerator, IID_IMFMediaEventGenerator);

    MfByteStreamHandlerWrapper();
    HRESULT HandleEvent_(IMFMediaEventPtr& ptr_event);
    HRESULT OnSourceStarted_(IMFMediaEventPtr& ptr_event);
    HRESULT OnNewStream_(IMFMediaEventPtr& ptr_event);
    HRESULT WaitForMediaEvent_(DWORD expected_event);
    virtual HRESULT Create(std::wstring dll_path, GUID mfobj_clsid);

    DWORD expected_media_event_type_;
    DWORD stream_count_;
    EventWaiter open_event_;
    EventWaiter media_source_event_;
    HRESULT media_event_error_;
    IMFByteStreamPtr ptr_byte_stream_;
    IMFByteStreamHandlerPtr ptr_handler_;
    IMFMediaEventGeneratorPtr ptr_event_queue_;
    IMFMediaSourcePtr ptr_media_src_;
    IMFMediaStreamPtr ptr_audio_stream_;
    IMFMediaStreamPtr ptr_video_stream_;
    IMFPresentationDescriptorPtr ptr_pres_desc_;
    UINT audio_stream_count_;
    UINT ref_count_;
    UINT selected_stream_count_;
    UINT video_stream_count_;

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
