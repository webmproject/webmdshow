#ifndef __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_MFPLAYER_CPP__
#define __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_MFPLAYER_CPP__

namespace WebmMfUtil {

class MfPlayer;

class MfPlayerCallback : public IMFPMediaPlayerCallback
{
public:
    explicit MfPlayerCallback(MfPlayer* ptr_mfplayer);

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IMFPMediaPlayerCallback methods
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER *pEventHeader);

private:
    long ref_count_;
    MfPlayer* ptr_mfplayer_;

    DISALLOW_COPY_AND_ASSIGN(MfPlayerCallback);
};

class MfPlayer
{
public:
    MfPlayer();
    ~MfPlayer();
    void Close();
    MFP_MEDIAPLAYER_STATE GetState() const;
    HRESULT Open(HWND video_hwnd, std::wstring url_str);
    HRESULT Play();
    HRESULT Pause();
    HRESULT UpdateVideo();
    std::wstring GetUrl() const;

private:
    void OnMediaItemCreated_(MFP_MEDIAITEM_CREATED_EVENT* ptr_event);
    void OnMediaItemSet_(MFP_MEDIAITEM_SET_EVENT* ptr_event);
    HRESULT SetAvailableStreams_();

    bool has_audio_;
    bool has_video_;
    bool player_init_done_;
    EventWaiter mediaplayer_event_;
    HWND video_hwnd_;
    IMFPMediaItem* ptr_mediaitem_;
    IMFPMediaPlayer* ptr_mediaplayer_;
    MfPlayerCallback* ptr_mfplayercallback_;
    std::wstring url_str_;

    DISALLOW_COPY_AND_ASSIGN(MfPlayer);
    friend MfPlayerCallback;
};

} // WebMMfTests namespace

#endif // __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_MFPLAYER_CPP__
