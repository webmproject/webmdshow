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

#include "debugutil.h"
#include "eventutil.h"
#include "mfmediastream.h"
#include "mfutil.h"

namespace WebmMfUtil
{

MfMediaStream::MfMediaStream():
  expected_event_(0),
  ref_count_(0),
  stream_event_error_(S_OK),
  stream_event_recvd_(0)
{
}

MfMediaStream::~MfMediaStream()
{
}

HRESULT MfMediaStream::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(MfMediaStream, IMFAsyncCallback),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

ULONG MfMediaStream::AddRef()
{
    return InterlockedIncrement(&ref_count_);
}

ULONG MfMediaStream::Release()
{
    UINT ref_count = InterlockedDecrement(&ref_count_);
    if (ref_count == 0)
    {
        delete this;
    }
    return ref_count;
}

HRESULT MfMediaStream::Create_(IMFMediaStreamPtr& ptr_stream)
{
    ptr_stream_ = ptr_stream;
    HRESULT hr = ptr_stream_->QueryInterface(IID_IMFMediaEventGenerator,
        reinterpret_cast<void**>(&ptr_event_queue_));
    if (FAILED(hr))
    {
        DBGLOG("ERROR, failed to obtain stream event generator" << HRLOG(hr)
            << " returning E_FAIL.");
        return E_FAIL;
    }
    IMFStreamDescriptorPtr ptr_desc;
    hr = ptr_stream->GetStreamDescriptor(&ptr_desc);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, GetStreamDescriptor failed" << HRLOG(hr));
        return hr;
    }
    hr = get_media_type(ptr_desc, &ptr_media_type_);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, get_media_type failed" << HRLOG(hr));
        return hr;
    }
    hr = stream_event_.Create();
    if (FAILED(hr))
    {
        DBGLOG("ERROR, stream event creation failed" << HRLOG(hr));
        return hr;
    }
    return hr;
}

// IMFAsyncCallback method
STDMETHODIMP MfMediaStream::GetParameters(DWORD*, DWORD*)
{
    // Implementation of this method is optional.
    return E_NOTIMPL;
}

// IMFAsyncCallback method
STDMETHODIMP MfMediaStream::Invoke(IMFAsyncResult* pAsyncResult)
{
    stream_event_error_ = E_FAIL;
    MediaEventType event_type = MEError;
    IMFMediaEventPtr ptr_event;
    HRESULT hr = ptr_event_queue_->EndGetEvent(pAsyncResult, &ptr_event);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, EndGetEvent failed" << HRLOG(hr));
    }
    else
    {
        hr = ptr_event->GetType(&event_type);
        if (FAILED(hr))
        {
            DBGLOG("ERROR, cannot get event type" << HRLOG(hr));
        }
        if (0 != expected_event_ && event_type != expected_event_)
        {
            DBGLOG("ERROR, unexpected event type, expected "
              << expected_event_ << " got " << event_type);
            stream_event_error_ = E_UNEXPECTED;
        }
        else
        {
            switch (event_type)
            {
            case MEMediaSample:
                DBGLOG("MEMediaSample");
                stream_event_error_ = OnMediaSample_(ptr_event);
                if (FAILED(stream_event_error_))
                {
                    DBGLOG("MEMediaSample handling failed");
                }
                break;
            case MEStreamPaused:
                DBGLOG("MEStreamPaused");
                stream_event_error_ = OnStreamPaused_(ptr_event);
                if (FAILED(stream_event_error_))
                {
                    DBGLOG("MEStreamSeeked handling failed");
                }
                break;
            case MEStreamSeeked:
                DBGLOG("MEStreamSeeked");
                stream_event_error_ = OnStreamSeeked_(ptr_event);
                if (FAILED(stream_event_error_))
                {
                    DBGLOG("MEStreamSeeked handling failed");
                }
                break;
            case MEStreamStarted:
                DBGLOG("MEStreamStarted");
                stream_event_error_ = OnStreamStarted_(ptr_event);
                if (FAILED(stream_event_error_))
                {
                    DBGLOG("MEStreamStarted handling failed");
                }
                break;
            case MEStreamStopped:
                DBGLOG("MEStreamStopped");
                stream_event_error_ = OnStreamStopped_(ptr_event);
                if (FAILED(stream_event_error_))
                {
                    DBGLOG("MEStreamStopped handling failed");
                }
                break;
            default:
                stream_event_error_ = E_UNEXPECTED;
                DBGLOG("unhandled event_type=" << event_type);
                break;
            }
        }
    }
    expected_event_ = 0;
    stream_event_recvd_ = event_type;
    return stream_event_.Set();
}

HRESULT MfMediaStream::OnMediaSample_(IMFMediaEventPtr& ptr_event)
{
    IUnknownPtr ptr_iunk;
    HRESULT hr = get_event_iunk_ptr(ptr_event, &ptr_iunk);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, could not obtain IUnknown from event" << HRLOG(hr));
        return hr;
    }
    ptr_sample_ = ptr_iunk;
    if (!ptr_sample_)
    {
        DBGLOG("ERROR, sample pointer null");
        return E_POINTER;
    }
    return S_OK;
}

HRESULT MfMediaStream::OnStreamPaused_(IMFMediaEventPtr&)
{
    // no-op for now
    return S_OK;
}

HRESULT MfMediaStream::OnStreamSeeked_(IMFMediaEventPtr&)
{
    // no-op for now
    return S_OK;
}

HRESULT MfMediaStream::OnStreamStarted_(IMFMediaEventPtr&)
{
    // no-op for now
    return S_OK;
}

HRESULT MfMediaStream::OnStreamStopped_(IMFMediaEventPtr&)
{
    // no-op for now
    return S_OK;
}

HRESULT MfMediaStream::Create(IMFMediaStreamPtr& ptr_mfstream,
                              MfMediaStream** ptr_instance)
{
    if (!ptr_mfstream)
    {
        DBGLOG("null IMFMediaStream, returning E_INVALIDARG");
        return E_INVALIDARG;
    }
    MfMediaStream* ptr_stream = new (std::nothrow) MfMediaStream();
    if (!ptr_stream)
    {
        DBGLOG("null MfMediaStream, returning E_OUTOFMEMORY");
        return E_OUTOFMEMORY;
    }
    HRESULT hr = ptr_stream->Create_(ptr_mfstream);
    if (FAILED(hr))
    {
        DBGLOG("null MfMediaStream::Create_ failed" << HRLOG(hr));
        return hr;
    }
    ptr_stream->AddRef();
    *ptr_instance = ptr_stream;
    return hr;
}

HRESULT MfMediaStream::GetMediaType(IMFMediaType **ptr_type)
{
    if (!ptr_media_type_)
    {
        DBGLOG("ERROR, media type not set, E_UNEXPECTED");
        return E_UNEXPECTED;
    }
    return copy_media_type(ptr_media_type_, ptr_type);
}

HRESULT MfMediaStream::WaitForStreamEvent(MediaEventType event_type)
{
    expected_event_ = event_type;
    HRESULT hr = ptr_stream_->BeginGetEvent(this, NULL);
    if (FAILED(hr))
    {
        DBGLOG("ERROR, BeginGetEvent failed" << HRLOG(hr));
        return hr;
    }
    hr = stream_event_.Wait();
    if (FAILED(hr))
    {
        DBGLOG("ERROR, stream event wait failed" << HRLOG(hr));
        return hr;
    }
    if (FAILED(stream_event_error_))
    {
        // when event handling fails the last error is stored in
        // |media_event_type_recvd_|, just return it to the caller
        DBGLOG("ERROR, stream event handling failed"
            << HRLOG(stream_event_error_));
        return stream_event_error_;
    }
    return hr;
}

HRESULT MfMediaStream::GetSample(IMFSample **ptr_sample)
{
    if (!ptr_stream_)
    {
        DBGLOG("ERROR, stream not set, E_UNEXPECTED");
        return E_UNEXPECTED;
    }
    // TODO(tomfinegan): need to handle pause; RequestSample will succeed while
    //                   paused, but the stream won't deliver the sample.
    //                   This means we'll block in |WaitForStreamEvent|...?
    //                   Curse you for not being specific MSDN.  I guess I have
    //                   to return a wrong state error when paused to avoid the
    //                   situation altogether.
    HRESULT hr = ptr_stream_->RequestSample(NULL);
    // MF_E_MEDIA_SOURCE_WRONGSTATE is not treated as an error by the pipeline
    if (FAILED(hr))
    {
        DBGLOG("ERROR, RequestSample failed" << HRLOG(hr));
        return hr;
    }
    hr = WaitForStreamEvent(MEMediaSample);
    if (FAILED(hr))
    {
        DBGLOG("WaitForStreamEvent MEMediaSample failed" << HRLOG(hr));
        return hr;
    }
    if (FAILED(stream_event_error_))
    {
        DBGLOG("MEMediaSample event handling failed"
            << HRLOG(stream_event_error_));
        return stream_event_error_;
    }
    if (!ptr_sample_)
    {
        // TODO(tomfinegan):
        DBGLOG("ERROR, null sample");
        return E_OUTOFMEMORY;
    }
    *ptr_sample = ptr_sample_.Detach();
    return hr;
}

} // WebmMfUtil namespace
