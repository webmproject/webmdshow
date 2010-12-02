#ifndef __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_MFPLAYER_CPP__
#define __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_MFPLAYER_CPP__

namespace WebmMfUtil {

class MfPlayer;

class MfPlayerCallback : public IMFPMediaPlayerCallback
{
    long ref_cnt_; // Reference count
    MfPlayer* ptr_mfplayer_;

public:

    explicit MfPlayerCallback(MfPlayer* ptr_mfplayer);

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IMFPMediaPlayerCallback methods
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER *pEventHeader);
};

class MfPlayer
{
public:
    MfPlayer();
    ~MfPlayer();
    HRESULT Open(std::wstring url_str);
    void Close();

    HRESULT Play();
    HRESULT Pause();
    HRESULT UpdateVideo();
    MFP_MEDIAPLAYER_STATE GetState();

private:
    MfPlayerCallback* ptr_mfplayercallback_;
    std::wstring url_str_;

    void OnMediaItemCreated_(MFP_MEDIAITEM_CREATED_EVENT* ptr_event);
    void OnMediaItemSet_(MFP_MEDIAITEM_SET_EVENT* ptr_event);

    HRESULT SetAvailableStreams_();

    bool has_audio_;
    bool has_video_;
    bool player_init_done_;

    HANDLE player_event_;

    IMFPMediaItem* ptr_mediaitem_;
    IMFPMediaPlayer* ptr_mediaplayer_;

    DISALLOW_COPY_AND_ASSIGN(MfPlayer);
    friend MfPlayerCallback;
};

} // WebMMfTests namespace

#endif // __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_MFPLAYER_CPP__
