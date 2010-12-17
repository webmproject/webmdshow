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

enum MfState
{
    MFSTATE_STOPPED = 0,
    MFSTATE_STARTED = 1,
    MFSTATE_PAUSED = 2,
    MFSTATE_ERROR = 3
};

// TODO(tomfinegan): get rid of MfObjWrapperBase?  Not so sure it's useful
//                   given how different |MfByteStreamHandlerWrapper| and
//                   |MfTransformWrapper| are likely to become...
class MfObjWrapperBase
{
public:
    MfObjWrapperBase();
    virtual ~MfObjWrapperBase();
    virtual HRESULT Create(std::wstring dll_path, GUID mfobj_clsid) = 0;
protected:
    ComDllWrapper* ptr_com_dll_;
    MfState state_;
    DISALLOW_COPY_AND_ASSIGN(MfObjWrapperBase);
};

class MfMediaStream;

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
    HRESULT Start(bool seeking, LONGLONG start_time);
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
    _COM_SMARTPTR_TYPEDEF(IMFMediaEventGenerator, IID_IMFMediaEventGenerator);
    _COM_SMARTPTR_TYPEDEF(IMFMediaSource, IID_IMFMediaSource);
    _COM_SMARTPTR_TYPEDEF(IMFMediaStream, IID_IMFMediaStream);
    _COM_SMARTPTR_TYPEDEF(IMFPresentationDescriptor,
                          IID_IMFPresentationDescriptor);
    _COM_SMARTPTR_TYPEDEF(IMFMediaTypeHandler, IID_IMFMediaTypeHandler);

    MfByteStreamHandlerWrapper();
    HRESULT HandleMediaSourceEvent_(IMFMediaEventPtr& ptr_event);
    HRESULT OnSourcePaused_(IMFMediaEventPtr& ptr_event);
    HRESULT OnSourceSeeked_(IMFMediaEventPtr& ptr_event);
    HRESULT OnSourceStarted_(IMFMediaEventPtr& ptr_event);
    HRESULT OnNewStream_(IMFMediaEventPtr& ptr_event);
    HRESULT OnUpdatedStream_(IMFMediaEventPtr& ptr_event);
    HRESULT WaitForEvent_(MediaEventType expected_event_type);
    HRESULT WaitForNewStreamEvents_();
    HRESULT WaitForPausedEvents_();
    HRESULT WaitForStartedEvents_();
    HRESULT WaitForSeekedEvents_();
    HRESULT WaitForStreamEvents_();
    HRESULT WaitForUpdatedStreamEvents_();
    virtual HRESULT Create(std::wstring dll_path, GUID mfobj_clsid);

    DWORD stream_count_;
    EventWaiter open_event_;
    EventWaiter media_source_event_;
    HRESULT media_event_error_;
    IMFByteStreamPtr ptr_byte_stream_;
    IMFByteStreamHandlerPtr ptr_handler_;
    IMFMediaEventGeneratorPtr ptr_event_queue_;
    IMFMediaSourcePtr ptr_media_src_;
    IMFPresentationDescriptorPtr ptr_pres_desc_;
    MediaEventType expected_event_type_;
    MediaEventType event_type_recvd_;
    MfMediaStream* ptr_audio_stream_;
    MfMediaStream* ptr_video_stream_;
    UINT audio_stream_count_;
    UINT selected_stream_count_;
    UINT video_stream_count_;
    ULONG ref_count_;

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
