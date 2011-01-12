// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_WEBDSOUND_HPP__
#define __WEBMDSHOW_COMMON_WEBDSOUND_HPP__

#include <dsound.h>

namespace WebmDirectX
{

// Note: AudioBufferTemplate is not in use!!
template <class SampleType>
class AudioBufferTemplate
{
public:
    AudioBufferTemplate();
    ~AudioBufferTemplate();
    HRESULT Read(UINT32 max_bytes, UINT32* ptr_bytes_written,
                 SampleType* ptr_out_data);
    HRESULT Write(SampleType* ptr_data, UINT32 length_in_bytes);
    UINT32 BytesToSamples(UINT32 num_bytes)
    {
        return num_bytes + (sample_size_ - 1) / sample_size;
    };
    UINT32 SamplesToBytes(UINT32 num_samples)
    {
        return num_samples * sample_size_;
    };
private:
    const UINT32 sample_size_;
    typedef std::vector<SampleType> SampleBuffer;
    SampleBuffer audio_buf_;
    DISALLOW_COPY_AND_ASSIGN(AudioBufferTemplate);
};

class AudioBuffer
{
public:
    AudioBuffer();
    virtual ~AudioBuffer();
    UINT32 GetSampleSize()
    {
        return sample_size_;
    };
    virtual HRESULT Read(UINT32 max_bytes, UINT32* ptr_bytes_written,
                         void* ptr_out_data) = 0;
    virtual HRESULT Write(const void* const ptr_data,
                          UINT32 length_in_bytes,
                          UINT32* ptr_samples_written) = 0;
protected:
    UINT64 BytesToSamples(UINT64 num_bytes)
    {
        return num_bytes + (sample_size_ - 1) / sample_size_;
    };
    UINT64 SamplesToBytes(UINT64 num_samples)
    {
        return num_samples * sample_size_;
    };
    UINT32 sample_size_;
private:
    DISALLOW_COPY_AND_ASSIGN(AudioBuffer);
};

class F32AudioBuffer : public AudioBuffer
{
public:
    F32AudioBuffer();
    virtual ~F32AudioBuffer();
    virtual HRESULT Read(UINT32 max_bytes, UINT32* ptr_bytes_written,
                         void* ptr_out_data);
    virtual HRESULT Write(const void* const ptr_data,
                          UINT32 length_in_bytes,
                          UINT32* ptr_samples_written);
private:
    typedef std::vector<float> SampleBuffer;
    SampleBuffer audio_buf_;
    DISALLOW_COPY_AND_ASSIGN(F32AudioBuffer);
};

class S16AudioBuffer : public AudioBuffer
{
public:
    S16AudioBuffer();
    virtual ~S16AudioBuffer();
    virtual HRESULT Read(UINT32 max_bytes, UINT32* ptr_bytes_written,
                         void* ptr_out_data);
    virtual HRESULT Write(const void* const ptr_data,
                          UINT32 length_in_bytes,
                          UINT32* ptr_samples_written);
private:
    typedef std::vector<INT16> SampleBuffer;
    SampleBuffer audio_buf_;
    DISALLOW_COPY_AND_ASSIGN(S16AudioBuffer);
};

class AudioPlaybackDevice : public CLockable
{
public:
    AudioPlaybackDevice();
    ~AudioPlaybackDevice();
    HRESULT Open(HWND hwnd, const WAVEFORMATEXTENSIBLE* const ptr_wfx);
    HRESULT WriteAudioBuffer(const void* const ptr_samples,
                             UINT32 length_in_bytes);
private:
    HRESULT CreateAudioBuffer_(WORD fmt_tag, WORD bits);
    HRESULT CreateDirectSoundBuffer_(const WAVEFORMATEXTENSIBLE* const ptr_wfx);
    std::auto_ptr<AudioBuffer> ptr_audio_buffer_;
    HWND hwnd_;
    IDirectSound8* ptr_dsound_;
    IDirectSoundBuffer8* ptr_dsound_buf_;
    UINT32 dsound_buffer_size_;
    UINT64 samples_buffered_;
    UINT64 samples_played_;
    DISALLOW_COPY_AND_ASSIGN(AudioPlaybackDevice);
};

} // WebmDirectX

#endif // __WEBMDSHOW_COMMON_WEBDSOUND_HPP__