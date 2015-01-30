// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_MFMEDIASTREAM_HPP__
#define __WEBMDSHOW_COMMON_MFMEDIASTREAM_HPP__

namespace WebmMfUtil
{

class MfMediaStream : public IMFAsyncCallback
{
public:
    _COM_SMARTPTR_TYPEDEF(IMFMediaStream, IID_IMFMediaStream);
    static HRESULT Create(IMFMediaStreamPtr& ptr_stream,
                          MfMediaStream** ptr_instance);
    HRESULT GetMediaType(IMFMediaType** ptr_type);
    HRESULT GetSample(IMFSample** ptr_sample);
    HRESULT WaitForStreamEvent(MediaEventType event_type);
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    // IMFAsyncCallback methods
    STDMETHODIMP GetParameters(DWORD*, DWORD*);
    STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult);

private:
    _COM_SMARTPTR_TYPEDEF(IMFMediaEvent, IID_IMFMediaEvent);
    _COM_SMARTPTR_TYPEDEF(IMFMediaEventGenerator, IID_IMFMediaEventGenerator);
    _COM_SMARTPTR_TYPEDEF(IMFMediaType, IID_IMFMediaType);
    _COM_SMARTPTR_TYPEDEF(IMFMediaTypeHandler, IID_IMFMediaTypeHandler);
    _COM_SMARTPTR_TYPEDEF(IMFSample, IID_IMFSample);
    _COM_SMARTPTR_TYPEDEF(IMFStreamDescriptor, IID_IMFStreamDescriptor);

    MfMediaStream();
    ~MfMediaStream();
    HRESULT Create_(IMFMediaStreamPtr& ptr_stream);
    HRESULT OnMediaSample_(IMFMediaEventPtr& ptr_event);
    HRESULT OnStreamPaused_(IMFMediaEventPtr& ptr_event);
    HRESULT OnStreamSeeked_(IMFMediaEventPtr& ptr_event);
    HRESULT OnStreamStarted_(IMFMediaEventPtr& ptr_event);
    HRESULT OnStreamStopped_(IMFMediaEventPtr& ptr_event);

    EventWaiter stream_event_;
    HRESULT stream_event_error_;
    IMFMediaEventGeneratorPtr ptr_event_queue_;
    IMFMediaStreamPtr ptr_stream_;
    IMFMediaTypePtr ptr_media_type_;
    MediaEventType expected_event_;
    IMFSamplePtr ptr_sample_;
    MediaEventType stream_event_recvd_;
    ULONG ref_count_;

    DISALLOW_COPY_AND_ASSIGN(MfMediaStream);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_MFMEDIASTREAM_HPP__
