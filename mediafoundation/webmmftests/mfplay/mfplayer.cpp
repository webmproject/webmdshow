// mfplay.cpp : Defines the entry point for the console application.
//

#define WINVER _WIN32_WINNT_WIN7

#include <cassert>
#include <new>
#include <string>

#include <windows.h>
#include <windowsx.h>
#include <mfplay.h>
#include <mferror.h>
#include <shlwapi.h> // for QITAB

#include "debugutil.hpp"
#include "mfplayer.hpp"
#include "mfplayer_main.hpp"

namespace WebmMfUtil {

MfPlayerCallback:: MfPlayerCallback(MfPlayer* ptr_mfplayer) :
  ref_cnt_(1),
  ptr_mfplayer_(NULL)
{
    assert(ptr_mfplayer);
    ptr_mfplayer_ = ptr_mfplayer;
}

STDMETHODIMP MfPlayerCallback::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(MfPlayerCallback, IMFPMediaPlayerCallback),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

STDMETHODIMP_(ULONG) MfPlayerCallback::AddRef()
{
        return InterlockedIncrement(&ref_cnt_);
}

STDMETHODIMP_(ULONG) MfPlayerCallback::Release()
{
    ULONG count = InterlockedDecrement(&ref_cnt_);
    if (count == 0)
    {
        ptr_mfplayer_ = 0;
        delete this;
        return 0;
    }
    return count;
}

void MfPlayerCallback::OnMediaPlayerEvent(MFP_EVENT_HEADER* ptr_event_header)
{
    assert(ptr_event_header);
    if (NULL == ptr_event_header)
    {
        return;
    }

    if (FAILED(ptr_event_header->hrEvent))
    {
        ShowErrorMessage(L"Playback error", ptr_event_header->hrEvent);
        return;
    }

    assert(ptr_mfplayer_);
    if (ptr_mfplayer_)
    {
        switch (ptr_event_header->eEventType)
        {
        case MFP_EVENT_TYPE_MEDIAITEM_CREATED:
            ptr_mfplayer_->OnMediaItemCreated_(
                MFP_GET_MEDIAITEM_CREATED_EVENT(ptr_event_header));
            break;

        case MFP_EVENT_TYPE_MEDIAITEM_SET:
            ptr_mfplayer_->OnMediaItemSet_(
                MFP_GET_MEDIAITEM_SET_EVENT(ptr_event_header));
            break;
        }
    }
}

MfPlayer::MfPlayer() :
  ptr_mfplayercallback_(NULL),
  player_event_(INVALID_HANDLE_VALUE)
{
}

MfPlayer::~MfPlayer()
{
    Close();
}

HRESULT MfPlayer::Open(std::wstring url_str)
{
    assert(url_str.length() > 0);
    if (url_str.length() < 1)
        return E_INVALIDARG;

    ptr_mfplayercallback_ = new (std::nothrow) MfPlayerCallback(this);
    assert(ptr_mfplayercallback_);
    if (NULL == ptr_mfplayercallback_)
    {
        return E_OUTOFMEMORY;
    }

    player_event_ = CreateEvent(NULL, FALSE, FALSE, L"mfplayer_event");
    if (INVALID_HANDLE_VALUE == player_event_)
    {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = MFPCreateMediaPlayer(
                    NULL,
                    FALSE,                 // Start playback automatically?
                    0,                     // Flags
                    NULL/*ptr_mfplayercallback_*/,  // Callback pointer
                    NULL,    // TODO(tomfinegan): need window
                    &ptr_mediaplayer_);

    assert(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        ShowErrorMessage(L"cannot create player", hr);
        return hr;
    }

    hr = ptr_mediaplayer_->CreateMediaItemFromURL(url_str.c_str(), TRUE, 0,
                                                  &ptr_mediaitem_);
    assert(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        ShowErrorMessage(L"cannot create media item", hr);
        return hr;
    }

    hr = SetAvailableStreams_();
    assert(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        ShowErrorMessage(L"cannot set available streams", hr);
        return hr;
    }

#if 0
    // async setup

    DWORD wr = MsgWaitForMultipleObjects(1, &player_event_, TRUE, INFINITE,
                                         QS_ALLEVENTS);

    if (wr != WAIT_OBJECT_0)
    {
        ShowErrorMessage(L"creation event wait timed out", hr);
        hr = E_FAIL;
    }
#else
#endif

    return hr;
}

void MfPlayer::Close()
{
    if (ptr_mediaplayer_)
    {
        ptr_mediaplayer_->Shutdown();
        ptr_mediaplayer_->Release();
        ptr_mediaplayer_ = NULL;
    }

    if (ptr_mfplayercallback_)
    {
        ptr_mfplayercallback_->Release();
        ptr_mfplayercallback_ = NULL;
    }
}

HRESULT MfPlayer::Play()
{
    assert(true == player_init_done_);
    if (false == player_init_done_)
    {
        ShowErrorMessage(L"MfPlayer not ready", E_UNEXPECTED);
        return E_UNEXPECTED;
    }

    HRESULT hr = ptr_mediaplayer_->Play();
    assert(SUCCEEDED(hr));

    if (FAILED(hr))
    {
        ShowErrorMessage(L"IMFPMediaPlayer::Play failed.", hr);
    }

    return hr;
}

HRESULT MfPlayer::Pause()
{
    assert(true == player_init_done_);
    if (false == player_init_done_)
    {
        ShowErrorMessage(L"MfPlayer not ready", E_UNEXPECTED);
        return E_UNEXPECTED;
    }

    HRESULT hr = ptr_mediaplayer_->Pause();
    assert(SUCCEEDED(hr));

    if (FAILED(hr))
    {
        ShowErrorMessage(L"IMFPMediaPlayer::Pause failed.", hr);
    }

    return hr;
}


// OnMediaItemCreated
//
// Called when the IMFPMediaPlayer::CreateMediaItemFromURL method
// completes.
void MfPlayer::OnMediaItemCreated_(MFP_MEDIAITEM_CREATED_EVENT* ptr_event)
{
    HRESULT hr = E_FAIL;

    // The media item was created successfully.

    if (ptr_event)
    {
        assert(ptr_event->pMediaItem);
        ptr_mediaitem_ = ptr_event->pMediaItem;

        hr = SetAvailableStreams_();

        if (SUCCEEDED(hr))
        {
            SetEvent(player_event_);
        }
    }

    if (FAILED(hr))
    {
        ShowErrorMessage(L"Error playing this file.", hr);
    }
}

HRESULT MfPlayer::SetAvailableStreams_()
{
    assert(ptr_mediaitem_);
    if (NULL == ptr_mediaitem_)
    {
        return E_UNEXPECTED;
    }
    BOOL has_media_type = FALSE, is_selected = FALSE;

    // Check if the media item contains video.
    HRESULT hr = ptr_mediaitem_->HasVideo(&has_media_type, &is_selected);
    assert(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        return hr;
    }

    has_video_ = has_media_type && is_selected;

    // Check for audio
    hr = ptr_mediaitem_->HasAudio(&has_media_type, &is_selected);
    assert(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        return hr;
    }

    has_audio_ = has_media_type && is_selected;

    // Set the media item on the player. This method completes asynchronously.
    //hr = ptr_mediaplayer_->SetMediaItem(ptr_mediaitem_);
    //assert(SUCCEEDED(hr));

    return hr;
}

// OnMediaItemSet
//
// Called when the IMFPMediaPlayer::SetMediaItem method completes.
void MfPlayer::OnMediaItemSet_(MFP_MEDIAITEM_SET_EVENT* ptr_event)
{
    assert(ptr_event);
    (void)ptr_event;
    player_init_done_ = true;

    // need a SetEvent here...
}

HRESULT MfPlayer::UpdateVideo()
{
    HRESULT hr = E_INVALIDARG;

    if (has_video_ && ptr_mediaplayer_)
    {
        hr = ptr_mediaplayer_->UpdateVideo();
        assert(SUCCEEDED(hr));
    }

    return hr;
}

MFP_MEDIAPLAYER_STATE MfPlayer::GetState()
{
    MFP_MEDIAPLAYER_STATE mediaplayer_state = MFP_MEDIAPLAYER_STATE_EMPTY;

    if (ptr_mediaplayer_)
    {
        HRESULT hr = ptr_mediaplayer_->GetState(&mediaplayer_state);
        assert(SUCCEEDED(hr));
    }

    return mediaplayer_state;
}

} // WebMMfTests namespace
