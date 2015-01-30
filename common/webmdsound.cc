// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <mmreg.h>

#include <cassert>
#include <vector>

#include "clockable.h"
#include "debugutil.h"
#include "eventutil.h"
#include "memutil.h"
#include "threadutil.h"
#include "webmdsound.h"

namespace WebmDirectX
{

const UINT32 kF32BytesPerSample = sizeof(float);
const UINT32 kF32BitsPerSample = kF32BytesPerSample * 8;
const UINT32 kS16BytesPerSample = sizeof(INT16);
const UINT32 kS16BitsPerSample = kS16BytesPerSample * 8;

// Note: AudioBufferTemplate is not in use!!
template <class SampleType>
AudioBufferTemplate<SampleType>::AudioBufferTemplate():
  sample_size_(sizeof SampleType)
{
    DBGLOG("ctor, sample_size_=" << sample_size_);
}

template <class SampleType>
AudioBufferTemplate<SampleType>::~AudioBufferTemplate()
{
    DBGLOG("dtor");
}

template <class SampleType>
HRESULT AudioBufferTemplate<SampleType>::Read(UINT32 out_buf_size,
                                              UINT32* ptr_bytes_written,
                                              SampleType* ptr_samples)
{
    if (!out_buf_size || !ptr_bytes_written || !ptr_samples)
    {
        return E_INVALIDARG;
    }
    if (audio_buf_.empty())
    {
        DBGLOG("buffer empty");
        return S_FALSE;
    }
    UINT32 aud_bytes_available = SamplesToBytes(audio_buf_.size());
    UINT32 bytes_to_copy = out_buf_size >= aud_bytes_available ?
        aud_bytes_available : out_buf_size;
    void* ptr_out_data = reinterpret_cast<void*>(ptr_samples);
    HRESULT hr = ::memcpy_s(ptr_out_data, max_bytes, &audio_buf_[0],
                            bytes_to_copy);
    if (SUCCEEDED(hr))
    {
        UINT32 samples_to_erase = BytesToSamples(bytes_to_copy);
        audio_buf_.erase(audio_buf_[0], audio_buf_[samples_to_erase]);
    }
    return hr;
}

template <class SampleType>
HRESULT AudioBufferTemplate<SampleType>::Write(SampleType* ptr_samples,
                                               UINT32 length_in_bytes)
{
    if (!ptr_samples || !length_in_bytes)
    {
        return E_INVALIDARG;
    }
    UINT32 num_samples = BytesToSamples(length_in_bytes);
    audio_buf_.insert(audio_buf_.end(), num_samples, ptr_samples);
    return hr;
}

AudioBuffer::AudioBuffer():
  sample_size_(0)
{
    DBGLOG("ctor");
}

AudioBuffer::~AudioBuffer()
{
    DBGLOG("dtor");
}

F32AudioBuffer::F32AudioBuffer()
{
    AudioBuffer::sample_size_ = kF32BytesPerSample;
    assert(kF32BytesPerSample == 4);
    DBGLOG("ctor");
}

F32AudioBuffer::~F32AudioBuffer()
{
    DBGLOG("dtor");
}

HRESULT F32AudioBuffer::Read(UINT32 out_buf_size, UINT32* ptr_bytes_written,
                             void* ptr_samples)
{
    if (!out_buf_size || !ptr_bytes_written || !ptr_samples)
    {
        return E_INVALIDARG;
    }
    if (audio_buf_.empty())
    {
        DBGLOG("buffer empty");
        return S_FALSE;
    }
    UINT64 aud_bytes_available = SamplesToBytes(audio_buf_.size());
    UINT64 bytes_to_copy = out_buf_size >= aud_bytes_available ?
        aud_bytes_available : out_buf_size;
    void* ptr_out_data = reinterpret_cast<void*>(ptr_samples);
    HRESULT hr = ::memcpy_s(ptr_out_data, out_buf_size, &audio_buf_[0],
                            bytes_to_copy);
    if (SUCCEEDED(hr))
    {
        UINT64 samples_to_erase = BytesToSamples(bytes_to_copy);
        audio_buf_.erase(audio_buf_.begin(), audio_buf_.begin()+samples_to_erase);
    }
    return hr;
}

HRESULT F32AudioBuffer::Write(const void* const ptr_samples,
                              UINT32 length_in_bytes,
                              UINT32* ptr_samples_written)
{
    if (!ptr_samples || !length_in_bytes || !ptr_samples_written)
    {
        return E_INVALIDARG;
    }
    UINT64 num_samples = BytesToSamples(length_in_bytes);
    typedef const float* const f32_read_ptr;
    f32_read_ptr ptr_fp_samples = reinterpret_cast<f32_read_ptr>(ptr_samples);
    audio_buf_.insert(audio_buf_.end(), num_samples, *ptr_fp_samples);
    *ptr_samples_written = static_cast<UINT32>(num_samples);
    return S_OK;
}

S16AudioBuffer::S16AudioBuffer()
{
    AudioBuffer::sample_size_ = kS16BytesPerSample;
    assert(kS16BytesPerSample == 2);
    DBGLOG("ctor");
}

S16AudioBuffer::~S16AudioBuffer()
{
    DBGLOG("dtor");
}

HRESULT S16AudioBuffer::Read(UINT32 out_buf_size, UINT32* ptr_bytes_written,
                             void* ptr_samples)
{
    if (!out_buf_size || !ptr_bytes_written || !ptr_samples)
    {
        return E_INVALIDARG;
    }
    if (audio_buf_.empty())
    {
        DBGLOG("buffer empty");
        return S_FALSE;
    }
    UINT64 aud_bytes_available = SamplesToBytes(audio_buf_.size());
    UINT64 bytes_to_copy = out_buf_size >= aud_bytes_available ?
        aud_bytes_available : out_buf_size;
    void* ptr_out_data = reinterpret_cast<void*>(ptr_samples);
    HRESULT hr = ::memcpy_s(ptr_out_data, out_buf_size, &audio_buf_[0],
                            bytes_to_copy);
    if (SUCCEEDED(hr))
    {
        UINT64 samples_to_erase = BytesToSamples(bytes_to_copy);
        audio_buf_.erase(audio_buf_.begin(),
                         audio_buf_.begin()+samples_to_erase);
    }
    return hr;
}

HRESULT S16AudioBuffer::Write(const void* const ptr_samples,
                              UINT32 length_in_bytes,
                              UINT32* ptr_samples_written)
{
    if (!ptr_samples || !length_in_bytes || !ptr_samples_written)
    {
        return E_INVALIDARG;
    }
    UINT64 num_samples = BytesToSamples(length_in_bytes);
    typedef const INT16* const s16_read_ptr;
    s16_read_ptr ptr_s16_samples = reinterpret_cast<s16_read_ptr>(ptr_samples);
    audio_buf_.insert(audio_buf_.end(), num_samples, *ptr_s16_samples);
    *ptr_samples_written = (UINT32)num_samples;
    return S_OK;
}

AudioPlaybackDevice::AudioPlaybackDevice():
  dsound_buffer_size_(0),
  hwnd_(NULL),
  play_cursor_(0),
  ptr_dsound_(NULL),
  ptr_dsound_buf_(NULL),
  ptr_dsound_thread_event_(NULL),
  ptr_dsound_thread_(NULL),
  samples_buffered_(0),
  samples_played_(0),
  state_(STATE_STOPPED)
{
}

AudioPlaybackDevice::~AudioPlaybackDevice()
{
    WebmUtil::safe_rel(ptr_dsound_);
    WebmUtil::safe_rel(ptr_dsound_buf_);
}

HRESULT AudioPlaybackDevice::Open(HWND hwnd,
                                  const WAVEFORMATEXTENSIBLE* const ptr_wfx)
{
    HRESULT hr;
    CHK(hr, DirectSoundCreate8(NULL /* same as DSDEVID_DefaultPlayback */,
                               &ptr_dsound_, NULL));
    if (FAILED(hr))
    {
        return hr;
    }
    if (!hwnd)
    {
        HWND desktop_hwnd = GetDesktopWindow();
        // TODO(tomfinegan): Using |desktop_hwnd| is wrong, we need our own
        //                   window here.  Using the desktop window means that
        //                   users are stuck hearing our audio when the desktop
        //                   window is active, and might not be able to hear
        //                   anything when our own window is active.
        hwnd_ = desktop_hwnd;
    }
    CHK(hr, ptr_dsound_->SetCooperativeLevel(hwnd_, DSSCL_PRIORITY));
    if (FAILED(hr))
    {
        return hr;
    }
    CHK(hr, CreateAudioBuffer_(ptr_wfx->Format.wFormatTag,
                               ptr_wfx->Format.wBitsPerSample));
    if (FAILED(hr))
    {
        return hr;
    }
    CHK(hr, CreateDirectSoundBuffer_(ptr_wfx));
    if (FAILED(hr))
    {
        return hr;
    }
    return hr;
}

HRESULT AudioPlaybackDevice::Start()
{
    if (STATE_STOPPED != state_)
    {
        DBGLOG("ERROR Already started.");
        return E_UNEXPECTED;
    }
    // Create the event we'll use to control |DSoundWriterThread_|
    using WebmMfUtil::EventWaiter;
    ptr_dsound_thread_event_.reset(new (std::nothrow) EventWaiter());
    if (!ptr_dsound_thread_event_.get())
    {
        DBGLOG("ERROR no memory for thread event.");
        return E_OUTOFMEMORY;
    }
    HRESULT hr;
    CHK(hr, ptr_dsound_thread_event_->Create());
    if (FAILED(hr))
    {
        return hr;
    }
    // Create the thread, |ptr_dsound_thread_|
    using WebmMfUtil::SimpleThread;
    ptr_dsound_thread_.reset(new (std::nothrow) WebmMfUtil::SimpleThread());
    if (!ptr_dsound_thread_.get())
    {
        DBGLOG("ERROR no memory for thread.");
        return E_OUTOFMEMORY;
    }
    // Start the thread
    CHK(hr, ptr_dsound_thread_->Run(DSoundWriterThread_,
                                    reinterpret_cast<void*>(this)));
    if (SUCCEEDED(hr))
    {
        state_ = STATE_STARTED;
    }
    return hr;
}

HRESULT AudioPlaybackDevice::Stop()
{
    if (state_ == STATE_STOPPED)
    {
        DBGLOG("ERROR Already stopped.");
        return E_UNEXPECTED;
    }
    HRESULT hr = S_OK;
    if (STATE_PLAY == state_)
    {
        CHK(hr, Pause());
        if (FAILED(hr))
        {
            return hr;
        }
    }
    if (ptr_dsound_thread_->Running())
    {
        // tell the |DSoundWriterThread_| to stop
        CHK(hr, ptr_dsound_thread_event_->Set());
        if (FAILED(hr))
        {
            return hr;
        }
        // wait for |DSoundWriterThread_| to signal
        CHK(hr, ptr_dsound_thread_event_->Wait());
        if (FAILED(hr))
        {
            return hr;
        }
        state_ = STATE_STOPPED;
    }
    return hr;
}

HRESULT AudioPlaybackDevice::Pause()
{
    if (STATE_PLAY != state_)
    {
        DBGLOG("ERROR wrong state, not playing.");
        return E_UNEXPECTED;
    }
    HRESULT hr;
    CHK(hr, ptr_dsound_buf_->Stop());
    if (SUCCEEDED(hr))
    {
        state_ = STATE_PAUSE;
    }
    return hr;
}

HRESULT AudioPlaybackDevice::Play()
{
    bool wrong_state = (STATE_PAUSE != state_ && STATE_STARTED != state_);
    if (wrong_state)
    {
        DBGLOG("ERROR wrong state.");
        return E_UNEXPECTED;
    }
    HRESULT hr;
    CHK(hr, ptr_dsound_buf_->Play(0, 0, DSBPLAY_LOOPING));
    if (SUCCEEDED(hr))
    {
        state_ = STATE_PLAY;
    }
    return hr;
}

HRESULT AudioPlaybackDevice::WriteAudioBuffer(const void* const ptr_samples,
                                              UINT32 length_in_bytes)
{
    if (!ptr_audio_buf_.get() || !ptr_audio_buf_->GetSampleSize())
    {
        DBGLOG("ERROR not configured");
        return E_UNEXPECTED;
    }
    if (!ptr_samples || !length_in_bytes)
    {
        DBGLOG("ERROR bad arg(s)");
        return E_INVALIDARG;
    }
    if (length_in_bytes < ptr_audio_buf_->GetSampleSize())
    {
        DBGLOG("ERROR less than 1 sample in user input buffer");
        return E_INVALIDARG;
    }
    Lock lock;
    HRESULT hr = S_OK;
    CHK(hr, lock.Seize(this));
    if (FAILED(hr))
    {
        return hr;
    }
    UINT32 samples_written = 0;
    CHK(hr, ptr_audio_buf_->Write(ptr_samples, length_in_bytes,
                                  &samples_written));
    if (SUCCEEDED(hr))
    {
        samples_buffered_ += samples_written;
    }
    return hr;
}

HRESULT AudioPlaybackDevice::WriteDSoundBuffer_()
{
    if (!ptr_audio_buf_.get() || !ptr_dsound_buf_)
    {
        DBGLOG("ERROR not configured");
        return E_UNEXPECTED;
    }
    Lock lock;
    HRESULT hr = S_OK;
    CHK(hr, lock.Seize(this));
    if (FAILED(hr))
    {
        return hr;
    }
    UINT32 bytes_available = 0;
    UINT32 samples_available = 0;
    CHK(hr, ptr_audio_buf_->Available(&samples_available, &bytes_available));
    if (FAILED(hr))
    {
        return hr;
    }
    if (!samples_available)
    {
        DBGLOG("no samples in buffer");
        return S_FALSE;
    }
    // adjust |bytes_available| to ensure we avoid trying to lock a portion
    // of |ptr_dsound_buf_| that's larger than the entire buffer
    bytes_available = bytes_available > dsound_buffer_size_ ?
        dsound_buffer_size_ : bytes_available;
    // We own our internal lock, try to lock the dsound buffer...
    DWORD write_offset = 0; // ignored by dsound because we set the
                            // DSBLOCK_FROMWRITECURSOR flag
    // DirectSound buffers are circular, so we might get two write pointers
    // back.  When we do, we must write to both if Lock gives us two non-null
    // pointers to satisfy our |length_in_bytes| requirement.
    void* ptr_write1 = NULL;
    void* ptr_write2 = NULL;
    DWORD write_space1 = 0;
    DWORD write_space2 = 0;
    // Always lock the dsound buffer at the current write cursor position
    DWORD lock_flags = DSBLOCK_FROMWRITECURSOR;
    CHK(hr, ptr_dsound_buf_->Lock(write_offset, bytes_available, &ptr_write1,
                                  &write_space1, &ptr_write2, &write_space2,
                                  lock_flags));
    if (FAILED(hr))
    {
        DBGLOG("ERROR Lock failed.");
        return hr;
    }
    bool buffer_full = (write_space1 + write_space2) == 0;
    if (buffer_full)
    {
        UpdateSamplesPlayed_();
        return S_FALSE;
    }
    UINT32 bytes_written1 = 0;
    if (ptr_write1)
    {
        const UINT32 bytes_to_write =
            write_space1 > bytes_available ? bytes_available : write_space1;
        CHK(hr, ptr_audio_buf_->Read(bytes_to_write, &bytes_written1,
                                     ptr_write1));
    }
    UINT32 bytes_written2 = 0;
    UINT32 bytes_left = bytes_available - write_space1;
    if (ptr_write2 && bytes_available > write_space1 && bytes_left)
    {
        CHK(hr, ptr_audio_buf_->Read(write_space2, &bytes_written2,
                                     ptr_write2));
    }
    CHK(hr, ptr_dsound_buf_->Unlock(ptr_write1, bytes_written1, ptr_write2,
                                    bytes_written2));
    UpdateSamplesPlayed_();
    //DBGLOG("bytes_written1=" << bytes_written1
    //    << "bytes_written2=" << bytes_written2
    //    << "total bytes written=" << bytes_written1 + bytes_written2);
    return hr;
}

void AudioPlaybackDevice::UpdateSamplesPlayed_()
{
    DWORD play_cursor = 0, write_cursor = 0;
    // get |ptr_dsound_buf_| cursor positions
    HRESULT hr;
    CHK(hr, ptr_dsound_buf_->GetCurrentPosition(&play_cursor, &write_cursor));
    UINT64 bytes_played = 0;
    if (play_cursor < play_cursor_)
    {
        // wrapped
        bytes_played = play_cursor + play_cursor_ - dsound_buffer_size_;
    }
    else
    {
        bytes_played = play_cursor - play_cursor_;
    }
    if (bytes_played && bytes_played >= ptr_audio_buf_->GetSampleSize())
    {
        // store total samples played in |samples_played_| for user playback
        // timing
        UINT64 samples_played = ptr_audio_buf_->BytesToSamples(bytes_played);
        samples_played_ += samples_played;
        //DBGLOG("samples_played=" << samples_played);
    }
    //DBGLOG("total samples_played_=" << samples_played_);
    play_cursor_ = play_cursor;
}

DWORD AudioPlaybackDevice::DSoundWriterThread_(void* ptr_this)
{
    if (!ptr_this)
    {
        DBGLOG("ERROR NULL thread data pointer");
        return EXIT_FAILURE;
    }
    AudioPlaybackDevice* ptr_apd =
        reinterpret_cast<AudioPlaybackDevice*>(ptr_this);
    WebmMfUtil::EventWaiter* apd_event =
        ptr_apd->ptr_dsound_thread_event_.get();
    HRESULT hr;
    for (;;)
    {
        if (S_OK == apd_event->ZeroWait())
        {
            // received a message, at present that means it's time to stop
            CHK(hr, apd_event->Set());
            break;
        }
        if (STATE_PLAY == ptr_apd->state_)
        {
            // we intentionally ignore the return values other than S_FALSE from
            // |WriteDsoundBuffer_|, though we log failures for sanity's sake in
            // debug mode
            CHK(hr, ptr_apd->WriteDSoundBuffer_());
            // and now we yield
            if (S_FALSE == hr)
            {
                // direct sound buffer full, let it play out for a bit
                Sleep(1);
            }
            else
            {
                Sleep(0);
            }
        }
        else
        {
            // paused or stopping, just sleep for a millisecond
            Sleep(1);
        }
    }
    return EXIT_SUCCESS;
}

HRESULT AudioPlaybackDevice::CreateAudioBuffer_(WORD fmt_tag, WORD bits)
{
    if (WAVE_FORMAT_PCM != fmt_tag && WAVE_FORMAT_IEEE_FLOAT != fmt_tag)
    {
        DBGLOG("ERROR unsupported format tag");
        return E_INVALIDARG;
    }
    // Create our internal audio buffer based on input sample type
    // (we support only S16 and float samples)
    if (WAVE_FORMAT_PCM == fmt_tag && kS16BitsPerSample == bits)
    {
        ptr_audio_buf_.reset(new (std::nothrow) S16AudioBuffer());
    }
    else if (WAVE_FORMAT_IEEE_FLOAT == fmt_tag && kF32BitsPerSample == bits)
    {
        ptr_audio_buf_.reset(new (std::nothrow) F32AudioBuffer());
    }
    else
    {
        DBGLOG("ERROR unsupported sample size");
        return E_INVALIDARG;
    }
    return ptr_audio_buf_.get() ? S_OK : E_OUTOFMEMORY;
}

HRESULT AudioPlaybackDevice::CreateDirectSoundBuffer_(
    const WAVEFORMATEXTENSIBLE* const ptr_wfx)
{
    if (!ptr_wfx)
    {
        DBGLOG("NULL WAVEFORMATEX!");
        return E_INVALIDARG;
    }
    if (!ptr_dsound_)
    {
        DBGLOG("called without valid IDirectSound pointer!");
        return E_UNEXPECTED;
    }
    const WORD fmt_tag = ptr_wfx->Format.wFormatTag;
    if (WAVE_FORMAT_PCM != fmt_tag && WAVE_FORMAT_IEEE_FLOAT != fmt_tag)
    {
        DBGLOG("unsupported format tag!");
        return E_INVALIDARG;
    }
    DSBUFFERDESC aud_buffer_desc = {0};
    aud_buffer_desc.dwSize = sizeof DSBUFFERDESC;
    aud_buffer_desc.guid3DAlgorithm = DS3DALG_DEFAULT;
    aud_buffer_desc.lpwfxFormat = (WAVEFORMATEX*)ptr_wfx;
    dsound_buffer_size_ = ptr_wfx->Format.nAvgBytesPerSec;
    aud_buffer_desc.dwBufferBytes = dsound_buffer_size_;
    aud_buffer_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
    // Obtain our IDirectSoundBuffer8 interface pointer, |ptr_dsound_buf_|, by:
    // 1. Create an IDirectSoundBuffer.
    // 2. Call QueryInterface on the IDirectSoundBuffer instance to obtain the
    //    IDirectSoundBuffer8 instance.
    HRESULT hr;
    IDirectSoundBuffer* ptr_dsbuf;
    CHK(hr, ptr_dsound_->CreateSoundBuffer(&aud_buffer_desc, &ptr_dsbuf, NULL));
    if (FAILED(hr) || !ptr_dsbuf)
    {
        return hr;
    }
    void* ptr_dsound_buf8 = reinterpret_cast<void*>(ptr_dsound_buf_);
    CHK(hr, ptr_dsbuf->QueryInterface(IID_IDirectSoundBuffer8,
                                      &ptr_dsound_buf8));
    if (FAILED(hr))
    {
        return hr;
    }
    return hr;
}

} // WebmDirectX namespace
