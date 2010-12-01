// mfplay.cpp : Defines the entry point for the console application.
//

#define WINVER _WIN32_WINNT_WIN7

#include <cassert>
#include <new>
#include <windows.h>
#include <windowsx.h>
#include <mfplay.h>
#include <mferror.h>
#include <shlwapi.h> // for QITAB
#include <string>

#include "debugutil.hpp"
#include "mfplayer.hpp"
#include "mfplayer_main.hpp"

namespace WebMMfTests {

MfPlayerCallback:: MfPlayerCallback(MfPlayer* ptr_player) :
  ref_cnt_(1),
  ptr_player_(NULL)
{
    assert(ptr_player);
    ptr_player_ = ptr_player;
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
        ptr_player_ = 0;
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

    assert(ptr_player_);
    if (ptr_player_)
    {
        switch (ptr_event_header->eEventType)
        {
        case MFP_EVENT_TYPE_MEDIAITEM_CREATED:
            ptr_player_->OnMediaItemCreated_(
                MFP_GET_MEDIAITEM_CREATED_EVENT(ptr_event_header));
            break;

        case MFP_EVENT_TYPE_MEDIAITEM_SET:
            ptr_player_->OnMediaItemSet_(
                MFP_GET_MEDIAITEM_SET_EVENT(ptr_event_header));
            break;
        }
    }
}

MfPlayer::MfPlayer() :
  ptr_player_callback_(NULL),
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

    ptr_player_callback_ = new (std::nothrow) MfPlayerCallback(this);
    assert(ptr_player_callback_);
    if (NULL == ptr_player_callback_)
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
                    ptr_player_callback_,  // Callback pointer
                    NULL,    // TODO(tomfinegan): need window
                    &ptr_player_);

    assert(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        ShowErrorMessage(L"cannot create player", hr);
        return hr;
    }

    // TODO(tomfinegan): might be simpler to use MF synchronously...
    hr = ptr_player_->CreateMediaItemFromURL(url_str.c_str(), FALSE, 0, NULL);
    assert(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        ShowErrorMessage(L"cannot create media item", hr);
        return hr;
    }

    DWORD wr = MsgWaitForMultipleObjects(1, &player_event_, TRUE, INFINITE,
                                         QS_ALLEVENTS);

    if (wr != WAIT_OBJECT_0)
    {
        ShowErrorMessage(L"creation event wait timed out", hr);
        hr = E_FAIL;
    }

    return hr;
}

void MfPlayer::Close()
{
    assert(0);
}

HRESULT MfPlayer::Play()
{
    assert(true == player_init_done_);
    if (false == player_init_done_)
    {
        ShowErrorMessage(L"MfPlayer not ready", E_UNEXPECTED);
        return E_UNEXPECTED;
    }

    HRESULT hr = ptr_player_->Play();
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

    HRESULT hr = ptr_player_->Pause();
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
    HRESULT hr = S_OK;

    // The media item was created successfully.

    if (ptr_event)
    {
        BOOL has_media_type = FALSE, is_selected = FALSE;

        // Check if the media item contains video.
        hr = ptr_event->pMediaItem->HasVideo(&has_media_type, &is_selected);
        assert(SUCCEEDED(hr));
        if (FAILED(hr)) { goto done; }

        has_video_ = has_media_type && is_selected;

        hr = ptr_event->pMediaItem->HasAudio(&has_media_type, &is_selected);
        assert(SUCCEEDED(hr));
        if (FAILED(hr)) { goto done; }

        has_audio_ = has_media_type && is_selected;

        // Set the media item on the player. This method completes asynchronously.
        hr = ptr_player_->SetMediaItem(ptr_event->pMediaItem);
        assert(SUCCEEDED(hr));

        SetEvent(player_event_);
    }

done:
    if (FAILED(hr))
    {
        ShowErrorMessage(L"Error playing this file.", hr);
    }
}


// OnMediaItemSet
//
// Called when the IMFPMediaPlayer::SetMediaItem method completes.
void MfPlayer::OnMediaItemSet_(MFP_MEDIAITEM_SET_EVENT* ptr_event)
{
    assert(ptr_event);
    (void)ptr_event;
    player_init_done_ = true;
}

} // WebMMfTests namespace
