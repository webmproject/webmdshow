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
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <shlwapi.h>

#include <cassert>
#include <string>

#include "debugutil.h"
#include "eventutil.h"
#include "comreg.h"
#include "comdllwrapper.h"
#include "mfmediastream.h"
#include "mfsrcwrap.h"
#include "mfutil.h"
#include "webmtypes.h"

namespace WebmMfUtil
{

MfByteStreamHandlerWrapper::MfByteStreamHandlerWrapper():
  audio_stream_count_(0),
  event_type_recvd_(0),
  expected_event_type_(0),
  media_event_error_(0),
  ptr_audio_stream_(NULL),
  ptr_com_dll_(NULL),
  ptr_video_stream_(NULL),
  ref_count_(0),
  selected_stream_count_(0),
  state_(MFSTATE_STOPPED),
  stream_count_(0),
  video_stream_count_(0)
{
}

MfByteStreamHandlerWrapper::~MfByteStreamHandlerWrapper()
{
    if (ptr_audio_stream_)
    {
        ptr_audio_stream_->Release();
        ptr_audio_stream_ = NULL;
    }
    if (ptr_video_stream_)
    {
        ptr_video_stream_->Release();
        ptr_video_stream_ = NULL;
    }
    if (ptr_media_src_)
    {
        ptr_media_src_->Shutdown();
    }
    if (ptr_byte_stream_)
    {
        ptr_byte_stream_->Close();
    }
}

HRESULT MfByteStreamHandlerWrapper::Create_(std::wstring dll_path,
                                            GUID mfobj_clsid)
{
    HRESULT hr = ComDllWrapper::Create(dll_path, mfobj_clsid, &ptr_com_dll_);
    if (FAILED(hr) || !ptr_com_dll_)
    {
        DBGLOG("ComDllWrapper::Create failed path=" << dll_path.c_str()
            << HRLOG(hr));
        return hr;
    }
    hr = ptr_com_dll_->CreateInstance(IID_IMFByteStreamHandler,
        reinterpret_cast<void**>(&ptr_handler_));
    if (FAILED(hr) || !ptr_handler_)
    {
        DBGLOG("GetInterfacePtr failed" << HRLOG(hr));
        return hr;
    }
    hr = open_event_.Create();
    if (FAILED(hr))
    {
        DBGLOG("open event creation failed" << HRLOG(hr));
        return hr;
    }
    hr = media_source_event_.Create();
    if (FAILED(hr))
    {
        DBGLOG("media source event creation failed" << HRLOG(hr));
        return hr;
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::Create(
    std::wstring dll_path,
    GUID mfobj_clsid,
    MfByteStreamHandlerWrapper** ptr_bsh_instance)
{
    *ptr_bsh_instance = new (std::nothrow) MfByteStreamHandlerWrapper();
    if (!*ptr_bsh_instance)
    {
        DBGLOG("ctor failed");
        return E_OUTOFMEMORY;
    }
    MfByteStreamHandlerWrapper* ptr_bsh_wrapper = *ptr_bsh_instance;
    HRESULT hr = ptr_bsh_wrapper->Create_(dll_path, mfobj_clsid);
    if (SUCCEEDED(hr))
    {
        ptr_bsh_wrapper->AddRef();
    }
    else
    {
        DBGLOG("ERROR, Create_ failed" << HRLOG(hr));
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::GetAudioSample(IMFSample** ptr_sample)
{
    if (0 == audio_stream_count_)
    {
        DBGLOG("no audio streams");
        return E_INVALIDARG;
    }
    if (NULL == ptr_audio_stream_)
    {
        DBGLOG("ERROR, audio stream not created, E_UNEXPECTED");
        return E_UNEXPECTED;
    }
    return ptr_audio_stream_->GetSample(ptr_sample);
}

HRESULT MfByteStreamHandlerWrapper::GetAudioMediaType(
    IMFMediaType **ptr_type) const
{
    if (0 == audio_stream_count_)
    {
        DBGLOG("no audio streams");
        return E_INVALIDARG;
    }
    if (NULL == ptr_audio_stream_)
    {
        DBGLOG("ERROR, audio stream not created, E_UNEXPECTED");
        return E_UNEXPECTED;
    }
    return ptr_audio_stream_->GetMediaType(ptr_type);
}

UINT MfByteStreamHandlerWrapper::GetAudioStreamCount() const
{
    return audio_stream_count_;
}

HRESULT MfByteStreamHandlerWrapper::GetVideoSample(IMFSample** ptr_sample)
{
    if (0 == video_stream_count_)
    {
        DBGLOG("no video streams");
        return E_INVALIDARG;
    }
    if (NULL == ptr_video_stream_)
    {
        DBGLOG("ERROR, video stream not created, E_UNEXPECTED");
        return E_UNEXPECTED;
    }
    return ptr_video_stream_->GetSample(ptr_sample);
}

HRESULT MfByteStreamHandlerWrapper::GetVideoMediaType(
    IMFMediaType **ptr_type) const
{
    if (0 == video_stream_count_)
    {
        DBGLOG("no video streams");
        return E_INVALIDARG;
    }
    if (NULL == ptr_video_stream_)
    {
        DBGLOG("ERROR, video stream not created, E_UNEXPECTED");
        return E_UNEXPECTED;
    }
    return ptr_video_stream_->GetMediaType(ptr_type);
}

UINT MfByteStreamHandlerWrapper::GetVideoStreamCount() const
{
    return video_stream_count_;
}

HRESULT MfByteStreamHandlerWrapper::OpenURL(std::wstring url)
{
    if (1 > url.length())
    {
        DBGLOG("ERROR, empty url string");
        return E_INVALIDARG;
    }
    if (!ptr_handler_)
    {
        DBGLOG("ERROR, byte stream handler NULL");
        return E_OUTOFMEMORY;
    }
    HRESULT hr = MFCreateFile(MF_ACCESSMODE_READ,
                              MF_OPENMODE_FAIL_IF_NOT_EXIST,
                              MF_FILEFLAGS_NONE, url.c_str(),
                              &ptr_byte_stream_);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, MFCreateFile failed" << HRLOG(hr));
        return hr;
    }
    hr = ptr_handler_->BeginCreateObject(ptr_byte_stream_, url.c_str(), 0,
                                         NULL, NULL, this, NULL);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, byte stream handler BeginCreateObject failed, hr="
            << hr);
        return hr;
    }
    hr = open_event_.Wait();
    if (FAILED(hr))
    {
        DBGLOG("ERROR, open event wait failed" << HRLOG(hr));
        return hr;
    }
    if (!ptr_media_src_)
    {
        DBGLOG("ERROR, NULL media source");
        hr = E_OUTOFMEMORY;
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(MfByteStreamHandlerWrapper, IMFAsyncCallback),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

ULONG MfByteStreamHandlerWrapper::AddRef()
{
    return InterlockedIncrement(&ref_count_);
}

ULONG MfByteStreamHandlerWrapper::Release()
{
    UINT ref_count = InterlockedDecrement(&ref_count_);
    if (ref_count == 0)
    {
        delete this;
    }
    return ref_count;
}

// IMFAsyncCallback method
STDMETHODIMP MfByteStreamHandlerWrapper::GetParameters(DWORD*, DWORD*)
{
    // Implementation of this method is optional.
    return E_NOTIMPL;
}

// IMFAsyncCallback method
STDMETHODIMP MfByteStreamHandlerWrapper::Invoke(IMFAsyncResult* pAsyncResult)
{
    if (!ptr_media_src_ && !ptr_event_queue_)
    {
        assert(ptr_media_src_ == NULL);

        IUnknownPtr ptr_unk_media_src_;
        MF_OBJECT_TYPE obj_type;
        HRESULT hr = ptr_handler_->EndCreateObject(pAsyncResult, &obj_type,
                                                   &ptr_unk_media_src_);
        if (FAILED(hr) || !ptr_unk_media_src_)
        {
            DBGLOG("ERROR, EndCreateObject failed" << HRLOG(hr)
              << " return E_FAIL.");
            return E_FAIL;
        }
        else
        {
            ptr_media_src_ = ptr_unk_media_src_;
            hr = ptr_media_src_->QueryInterface(IID_IMFMediaEventGenerator,
                reinterpret_cast<void**>(&ptr_event_queue_));
            if (FAILED(hr) || !ptr_event_queue_)
            {
                DBGLOG("ERROR, failed to obtain media source event generator"
                  << HRLOG(hr) << " return E_FAIL.");
                return E_FAIL;
            }
        }
        return open_event_.Set();
    }
    else
    {
        IMFMediaEventPtr ptr_event;
        HRESULT hr = ptr_event_queue_->EndGetEvent(pAsyncResult, &ptr_event);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, EndGetEvent failed" << HRLOG(hr)
              << " return E_FAIL.");
            return E_FAIL;
        }
        return HandleMediaSourceEvent_(ptr_event);
    }
}

HRESULT MfByteStreamHandlerWrapper::HandleMediaSourceEvent_(
    IMFMediaEventPtr& ptr_event)
{
    MediaEventType event_type = MEError;
    HRESULT hr = ptr_event->GetType(&event_type);
    if (FAILED(hr))
    {
        media_event_error_ = hr;
        DBGLOG("ERROR, cannot get event type" << HRLOG(hr));
    }
    else
    {
        if (0 != expected_event_type_ && event_type != expected_event_type_)
        {
            DBGLOG("ERROR, unexpected event type, expected "
              << expected_event_type_ << " got " << event_type);
            media_event_error_ = E_UNEXPECTED;
        }
        else
        {
            switch (event_type)
            {
            case MENewStream:
                DBGLOG("MENewStream");
                media_event_error_ = OnNewStream_(ptr_event);
                if (FAILED(media_event_error_))
                {
                    DBGLOG("MENewStream handling failed");
                }
                break;
            case MESourcePaused:
                DBGLOG("MESourcePaused");
                media_event_error_ = OnSourcePaused_(ptr_event);
                if (FAILED(media_event_error_))
                {
                    DBGLOG("MESourcePaused handling failed");
                }
                break;
            case MESourceSeeked:
                DBGLOG("MESourceSeeked");
                media_event_error_ = OnSourceSeeked_(ptr_event);
                if (FAILED(media_event_error_))
                {
                    DBGLOG("MESourceSeeked handling failed");
                }
                break;
            case MESourceStarted:
                DBGLOG("MESourceStarted");
                media_event_error_ = OnSourceStarted_(ptr_event);
                if (FAILED(media_event_error_))
                {
                    DBGLOG("MESourceStarted handling failed");
                }
                break;
            case MESourceStopped:
                DBGLOG("MESourceStopped");
                media_event_error_ = OnSourceStopped_(ptr_event);
                if (FAILED(media_event_error_))
                {
                    DBGLOG("MESourceStopped handling failed");
                }
                break;
            case MEUpdatedStream:
                DBGLOG("MEUpdatedStream");
                media_event_error_ = OnUpdatedStream_(ptr_event);
                if (FAILED(media_event_error_))
                {
                    DBGLOG("MEUpdatedStream handling failed");
                }
                break;
            default:
                DBGLOG("unhandled event_type=" << event_type);
                media_event_error_ = E_UNEXPECTED;
                break;
            }
        }
    }
    expected_event_type_ = 0;
    event_type_recvd_ = event_type;
    return media_source_event_.Set();
}

HRESULT MfByteStreamHandlerWrapper::OnNewStream_(IMFMediaEventPtr &ptr_event)
{
    IUnknownPtr ptr_iunk;
    HRESULT hr = get_event_iunk_ptr(ptr_event, &ptr_iunk);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, could not obtain IUnknown from event" << HRLOG(hr));
        return hr;
    }
    IMFMediaStreamPtr ptr_media_stream = ptr_iunk;
    if (!ptr_media_stream)
    {
        DBGLOG("ERROR, stream pointer null");
        return E_POINTER;
    }
    _COM_SMARTPTR_TYPEDEF(IMFStreamDescriptor, IID_IMFStreamDescriptor);
    IMFStreamDescriptorPtr ptr_stream_desc;
    hr = ptr_media_stream->GetStreamDescriptor(&ptr_stream_desc);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, could not get stream descriptor" << HRLOG(hr));
        return hr;
    }
    if (!ptr_stream_desc)
    {
        DBGLOG("ERROR, stream descriptor null");
        return E_POINTER;
    }
    _COM_SMARTPTR_TYPEDEF(IMFMediaTypeHandler, IID_IMFMediaTypeHandler);
    IMFMediaTypeHandlerPtr ptr_handler;
    hr = ptr_stream_desc->GetMediaTypeHandler(&ptr_handler);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, could not get media type handler" << HRLOG(hr));
        return hr;
    }
    if (!ptr_handler)
    {
        DBGLOG("ERROR, media type handler null");
        return E_POINTER;
    }
    GUID major_type = GUID_NULL;
    hr = ptr_handler->GetMajorType(&major_type);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, could not get major type" << HRLOG(hr));
        return hr;
    }
    if (MFMediaType_Audio != major_type && MFMediaType_Video != major_type)
    {
        DBGLOG("ERROR, unexpected major type (not audio or video)");
        return E_FAIL;
    }
    // TODO(tomfinegan): should I hang onto the IMFMediaStream's, or should
    //                   I only interact with them in event handlers?
    if (MFMediaType_Audio == major_type)
    {
        hr = MfMediaStream::Create(ptr_media_stream, &ptr_audio_stream_);
        if (FAILED(hr) || !ptr_audio_stream_)
        {
            DBGLOG("audio MfMediaStream creation failed" << HRLOG(hr));
        }
    }
    else
    {
        hr = MfMediaStream::Create(ptr_media_stream, &ptr_video_stream_);
        if (FAILED(hr) || !ptr_video_stream_)
        {
            DBGLOG("video MfMediaStream creation failed" << HRLOG(hr));
        }
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::OnUpdatedStream_(IMFMediaEventPtr &)
{
    // no-op for now
    return S_OK;
}

HRESULT MfByteStreamHandlerWrapper::OnSourcePaused_(
    IMFMediaEventPtr&)
{
    // no-op for now
    return S_OK;
}

HRESULT MfByteStreamHandlerWrapper::OnSourceSeeked_(
    IMFMediaEventPtr&)
{
    // no-op for now
    return S_OK;
}

HRESULT MfByteStreamHandlerWrapper::OnSourceStarted_(
    IMFMediaEventPtr&)
{
    // no-op for now
    return S_OK;
}

HRESULT MfByteStreamHandlerWrapper::OnSourceStopped_(
    IMFMediaEventPtr&)
{
    // no-op for now
    return S_OK;
}

HRESULT MfByteStreamHandlerWrapper::WaitForEvent_(
    MediaEventType expected_event_type)
{
    expected_event_type_ = expected_event_type;
    HRESULT hr = ptr_media_src_->BeginGetEvent(this, NULL);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, BeginGetEvent failed" << HRLOG(hr));
        return hr;
    }
    hr = media_source_event_.Wait();
    if (FAILED(hr))
    {
        DBGLOG("ERROR, media source event wait failed" << HRLOG(hr));
        return hr;
    }
    if (FAILED(media_event_error_))
    {
        // when event handling fails the last error is stored in
        // |event_type_recvd_|, just return it to the caller
        DBGLOG("ERROR, media source event handling failed"
            << HRLOG(media_event_error_));
        return media_event_error_;
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::WaitForPausedEvents_()
{
    // http://msdn.microsoft.com/en-us/library/ms694275(v=VS.85).aspx
    // When paused the source sends an MESourcePaused event, and every active
    // stream sends an MEStreamPaused event.

    // wait for MESourcePaused
    HRESULT hr = WaitForEvent_(MESourcePaused);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, source pause wait failed" << HRLOG(hr));
        return hr;
    }
    // now wait for the MEStreamPaused events
    // TODO(tomfinegan): might have to flush RequestSample event responses from
    //                   the stream event queues on Pause because:
    // http://msdn.microsoft.com/en-us/library/ms694275(v=VS.85).aspx
    // Note that the source's event queue is not serialized with the stream
    // event queues, so the client might receive some samples after the
    // MESourcePaused event, due to multi-threading issues. But the client
    // will not receive any samples from a stream after the MEStreamPaused
    // event.
    if (ptr_audio_stream_)
    {
        hr = ptr_audio_stream_->WaitForStreamEvent(MEStreamPaused);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, audio stream pause wait failed" << HRLOG(hr));
            return hr;
        }
    }
    if (ptr_video_stream_)
    {
        hr = ptr_video_stream_->WaitForStreamEvent(MEStreamPaused);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, video stream pause wait failed" << HRLOG(hr));
            return hr;
        }
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::WaitForNewStreamEvents_()
{
    const UINT num_new_events = audio_stream_count_ + video_stream_count_;
    if (0 == num_new_events)
    {
        DBGLOG("ERROR, 0 events to wait on" << HRLOG(E_INVALIDARG));
        return E_INVALIDARG;
    }
    // Previous call to Start on |ptr_media_src_| was our first:
    // http://msdn.microsoft.com/en-us/library/ms694101(v=VS.85).aspx
    // For each new stream, the source sends an MENewStream event. This
    // event is sent for the first Start call in which the stream appears.
    // The event data is a pointer to the stream's IMFMediaStream
    // interface.
    HRESULT hr = E_FAIL;
    UINT num_events = 0;
    while (num_events < num_new_events)
    {
        hr = WaitForEvent_(MENewStream);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, WaitForEvent_ MENewStream failed"
                << HRLOG(hr));
            return hr;
        }
        ++num_events;
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::WaitForUpdatedStreamEvents_()
{
    const UINT num_update_events = audio_stream_count_ + video_stream_count_;
    if (0 == num_update_events)
    {
        DBGLOG("ERROR, 0 events to wait on" << HRLOG(E_INVALIDARG));
        return E_INVALIDARG;
    }
    // Most recent call to Start was a restart
    // http://msdn.microsoft.com/en-us/library/ms694101(v=VS.85).aspx
    // For each updated stream, the source sends an MEUpdatedStream
    // event. A stream is updated if the stream already existed when
    // Start was called (for example, if the application seeks during
    // playback). The event data is a pointer to the stream's
    // IMFMediaStream interface.
    HRESULT hr = E_FAIL;
    UINT num_events = 0;
    while (num_events < num_update_events)
    {
        hr = WaitForEvent_(MEUpdatedStream);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, stream update wait failed" << HRLOG(hr));
            return hr;
        }
        ++num_events;
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::WaitForStartedEvents_()
{
    // http://msdn.microsoft.com/en-us/library/ms694101(v=VS.85).aspx
    // If the source sends an MESourceStarted event, each media stream sends
    // an MEStreamStarted event.

    // wait for MESourceStarted
    HRESULT hr = WaitForEvent_(MESourceStarted);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, source start wait failed" << HRLOG(hr));
        return hr;
    }
    // now wait for the MEStreamStarted events
    if (ptr_audio_stream_)
    {
        hr = ptr_audio_stream_->WaitForStreamEvent(MEStreamStarted);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, audio stream start wait failed" << HRLOG(hr));
            return hr;
        }
    }
    if (ptr_video_stream_)
    {
        hr = ptr_video_stream_->WaitForStreamEvent(MEStreamStarted);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, video stream start wait failed" << HRLOG(hr));
            return hr;
        }
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::WaitForSeekedEvents_()
{
    // http://msdn.microsoft.com/en-us/library/ms694101(v=VS.85).aspx
    // If the source sends an MESourceSeeked event, each stream sends an
    // MEStreamSeeked event.

    // wait for MESourceSeeked
    HRESULT hr = WaitForEvent_(MESourceSeeked);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, source start wait failed" << HRLOG(hr));
        return hr;
    }
    // now wait for the MEStreamSeeked events
    if (ptr_audio_stream_)
    {
        hr = ptr_audio_stream_->WaitForStreamEvent(MEStreamSeeked);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, audio stream seek wait failed" << HRLOG(hr));
            return hr;
        }
    }
    if (ptr_video_stream_)
    {
        hr = ptr_video_stream_->WaitForStreamEvent(MEStreamSeeked);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, video stream seek wait failed" << HRLOG(hr));
            return hr;
        }
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::WaitForStoppedEvents_()
{
    // http://msdn.microsoft.com/en-us/library/ms694101(v=VS.85).aspx
    // If the source sends an MESourceStopped event, each stream sends an
    // MEStreamStopped event.

    // wait for MESourceStopped
    HRESULT hr = WaitForEvent_(MESourceStopped);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, source start wait failed" << HRLOG(hr));
        return hr;
    }
    // now wait for the MEStreamStopped events
    if (ptr_audio_stream_)
    {
        hr = ptr_audio_stream_->WaitForStreamEvent(MEStreamStopped);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, audio stream stop wait failed" << HRLOG(hr));
            return hr;
        }
    }
    if (ptr_video_stream_)
    {
        hr = ptr_video_stream_->WaitForStreamEvent(MEStreamStopped);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, video stream stop wait failed" << HRLOG(hr));
            return hr;
        }
    }
    return hr;
}

// TODO(tomfinegan): I should rename this to GetDefaultStreams_, then add a
//                   GetAvailableStreams method that exposes all of the
//                   available streams after calling GetDefaultStreams_.
HRESULT MfByteStreamHandlerWrapper::LoadMediaStreams()
{
    HRESULT hr = ptr_media_src_->CreatePresentationDescriptor(&ptr_pres_desc_);
    if (FAILED(hr) || !ptr_pres_desc_)
    {
        DBGLOG("ERROR, CreatePresentationDescriptor failed" << HRLOG(hr));
        return hr;
    }
    hr = ptr_pres_desc_->GetStreamDescriptorCount(&stream_count_);
    if (FAILED(hr) || !ptr_pres_desc_)
    {
        DBGLOG("ERROR, GetStreamDescriptorCount failed" << HRLOG(hr));
        return hr;
    }
    // get stream descriptors and store audio/video descs in our vectors
    for (DWORD i = 0; i < stream_count_; ++i)
    {
        BOOL selected;
        _COM_SMARTPTR_TYPEDEF(IMFStreamDescriptor, IID_IMFStreamDescriptor);
        IMFStreamDescriptorPtr ptr_desc;
        hr = ptr_pres_desc_->GetStreamDescriptorByIndex(i, &selected,
                                                        &ptr_desc);
        if (FAILED(hr) || !ptr_desc)
        {
            DBGLOG("ERROR, GetStreamDescriptorByIndex failed, count="
                << stream_count_ << " index=" << i << HRLOG(hr));
            return hr;
        }
        // TODO(tomfinegan): decide what to do w/unselected streams
        if (selected)
        {
            GUID major_type;
            hr = get_major_type(ptr_desc, &major_type);
            if (FAILED(hr))
            {
                DBGLOG("ERROR, get_major_type failed, count=" << stream_count_
                    << " index=" << i << HRLOG(hr));
                return hr;
            }
            if (MFMediaType_Audio == major_type)
            {
                ++audio_stream_count_;
            }
            else if (MFMediaType_Video == major_type)
            {
                ++video_stream_count_;
            }
            ++selected_stream_count_;
        }
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::Start(bool seeking, LONGLONG start_time)
{
    if (!ptr_media_src_)
    {
        DBGLOG("ERROR, no media source");
        return E_INVALIDARG;
    }
    if (!ptr_pres_desc_)
    {
        DBGLOG("ERROR, no presentation descriptor");
        return E_INVALIDARG;
    }
    if (!audio_stream_count_ && !video_stream_count_)
    {
        DBGLOG("ERROR, no streams");
        return E_INVALIDARG;
    }
    if (!ptr_event_queue_)
    {
        DBGLOG("ERROR, no event queue");
        return E_INVALIDARG;
    }
    PROPVARIANT time_var;
    PropVariantInit(&time_var);
    if (seeking)
    {
        // seeking enabled, store the time in |time_var|
        time_var.vt = VT_I8;
        time_var.hVal.QuadPart = start_time;
    }
    else
    {
        // not seeking, leave |time_var| empty
        time_var.vt = VT_EMPTY;
    }
    // GUID_NULL == TIME_FORMAT_NONE, 100 ns units (aka TIME_FORMAT_MEDIA_TIME
    // in DirectShow)
    const GUID time_format = GUID_NULL;
    HRESULT hr = ptr_media_src_->Start(ptr_pres_desc_, &time_format,
                                       &time_var);
    // Note: if IMFMediaSource::Start fails asynchronously, our handler will
    //       receive an MESourceStarted event w/data set to error code.
    //       However, a comment in webmmfsource lists this behavior as TODO.
    if (FAILED(hr))
    {
        DBGLOG("ERROR, IMFMediaSource::Start failed" << HRLOG(hr));
        return hr;
    }
    state_ = MFSTATE_STARTED;
    DBGLOG("state_=MFSTATE_STARTED");
    if (!ptr_audio_stream_ && !ptr_video_stream_)
    {
        // Our |IMFMediaStreamPtr|'s pointers are stored when MENewStream
        // is received in |WaitForNewStreamEvents_|.
        hr = WaitForNewStreamEvents_();
        if (FAILED(hr))
        {
            state_ = MFSTATE_ERROR;
            DBGLOG("state_=MFSTATE_ERROR, new stream event wait failed"
                << HRLOG(hr));
            return hr;
        }
        // always set seeking to false; we're getting MESourceStarted and
        // MEStreamStarted in response to the first Start when a time is
        // specified.
        seeking = false;
    }
    else
    {
        // If the streams already exist when calling IMFMediaSource::Start,
        // each stream will send a MEUpdatedStream event.
        hr = WaitForUpdatedStreamEvents_();
        if (FAILED(hr))
        {
            state_ = MFSTATE_ERROR;
            DBGLOG("state_=MFSTATE_ERROR, updated stream event wait failed"
                << HRLOG(hr));
            return hr;
        }
    }
    if (!seeking)
    {
        // if we did not seek, webmmfsource will send:
        // MESourceStarted
        // MEStreamStarted * num streams
        hr = WaitForStartedEvents_();
        if (FAILED(hr))
        {
            state_ = MFSTATE_ERROR;
            DBGLOG("state_=MFSTATE_ERROR, start event wait failed"
                << HRLOG(hr));
            return hr;
        }
    }
    else
    {
        // when we seek, webmmfsource will send:
        // MESourceSeeked
        // MEStreamSeeked * num streams
        hr = WaitForSeekedEvents_();
        if (FAILED(hr))
        {
            state_ = MFSTATE_ERROR;
            DBGLOG("state_=MFSTATE_ERROR, seek event wait failed"
                << HRLOG(hr));
            return hr;
        }
    }
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::Pause()
{
    const bool no_streams = (!ptr_audio_stream_ && !ptr_video_stream_);
    const bool wrong_state = MFSTATE_STARTED != state_;
    if (no_streams || wrong_state)
    {
        DBGLOG("ERROR, MF_E_INVALID_STATE_TRANSITION");
        return MF_E_INVALID_STATE_TRANSITION;
    }
    HRESULT hr = E_FAIL;
    hr = ptr_media_src_->Pause();
    if (FAILED(hr))
    {
        DBGLOG("ERROR, Paused failed" << HRLOG(hr));
        return hr;
    }
    hr = WaitForPausedEvents_();
    if (FAILED(hr))
    {
        DBGLOG("ERROR, Paused failed" << HRLOG(hr));
        return hr;
    }
    state_ = MFSTATE_PAUSED;
    return hr;
}

HRESULT MfByteStreamHandlerWrapper::Stop()
{
    const bool no_streams = (!ptr_audio_stream_ && !ptr_video_stream_);
    const bool wrong_state =
        (MFSTATE_STARTED != state_ && MFSTATE_PAUSED != state_);
    if (no_streams || wrong_state)
    {
        DBGLOG("ERROR, MF_E_INVALID_STATE_TRANSITION");
        return MF_E_INVALID_STATE_TRANSITION;
    }
    HRESULT hr = E_FAIL;
    hr = ptr_media_src_->Stop();
    if (FAILED(hr))
    {
        DBGLOG("ERROR, Paused failed" << HRLOG(hr));
        return hr;
    }
    hr = WaitForStoppedEvents_();
    if (FAILED(hr))
    {
        DBGLOG("ERROR, Stop failed" << HRLOG(hr));
        return hr;
    }
    state_ = MFSTATE_STOPPED;
    return hr;
}

} // WebmMfUtil namespace
