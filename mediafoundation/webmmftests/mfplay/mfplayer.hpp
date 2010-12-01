#ifndef __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_MFPLAYER_CPP__
#define __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_MFPLAYER_CPP__

namespace WebMMfTests {

class MfPlayer;

class MfPlayerCallback : public IMFPMediaPlayerCallback
{
    long ref_cnt_; // Reference count
    MfPlayer* ptr_player_;

public:

    explicit MfPlayerCallback(MfPlayer* ptr_player);

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

private:
    MfPlayerCallback* ptr_player_callback_;
    std::wstring url_str_;

    void OnMediaItemCreated_(MFP_MEDIAITEM_CREATED_EVENT* ptr_event);
    void OnMediaItemSet_(MFP_MEDIAITEM_SET_EVENT* ptr_event);

    friend MfPlayerCallback;

    bool has_audio_;
    bool has_video_;
    bool player_init_done_;

    HANDLE player_event_;

    IMFPMediaPlayer* ptr_player_;

    DISALLOW_COPY_AND_ASSIGN(MfPlayer);
};

} // WebMMfTests namespace

#endif // __MEDIAFOUNDATION_WEBMMFTESTS_MFPLAY_MFPLAYER_CPP__
