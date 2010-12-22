// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_MEDIAFOUNDATION_WEBMMFTESTS_SDLPLAY_SDLVIDEOPLAYER_H__
#define __WEBMDSHOW_MEDIAFOUNDATION_WEBMMFTESTS_SDLPLAY_SDLVIDEOPLAYER_H__

#include "vorbisdecoder.hpp"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

#pragma warning(push)
// disable member alignment sensitive to packing warning: we know SDL is 4
// byte aligned, and we're fine with that
#pragma warning(disable:4121)
#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_audio.h"
#include "SDL_timer.h"
#pragma warning(pop)

const int OVERLAY_BUFFER_SIZE = 5;

struct AudioFrame
{
  unsigned char* data;
  int size;
};

class SDLVideoPlayer
{
public:
    SDLVideoPlayer();
    ~SDLVideoPlayer();

    int Init();
    int Exit();
    static int event_thread(void *data);
    static int video_decode_loop(void *data);

    int setup_surface(int display_width, int display_height);
    int show_frame(vpx_image_t *img, int display_width,
                   int display_height);
    int convert_frame(SDL_Overlay *dst_overlay, vpx_image_t *src_img,
                      int display_width, int display_height);
    int put_frame(vpx_image_t *img, long long time, int display_width,
                  int display_height);
    int setup_vpx_decoder();
    int close_vpx_decoder();
    int decode_vpx_frame(const unsigned char* data, const int size,
                         const long long milli, const long long jitter);
    int setup_audio_hardware(const unsigned int channels,
                             const unsigned int sample_rate,
                             const unsigned int bits_per_sample);
    int close_audio_hardware();
    int setup_vorbis_decoder(const unsigned char* data, const int size);
    int vorbis_queue_put(const unsigned char* data, const int size);
    int packet_queue_get(AudioFrame **pkt, int block);
    static void audio_callback(void *userdata, Uint8 *stream, int len);

    unsigned int get_playback_milli();

    int get_last_time_rendered(long long& video_milli,
                               long long& video_milli_jitter,
                               long long& audio_milli);

private:

    // Video
    int m_width;
    int m_height;

    SDL_Surface *pscreen;
    SDL_Overlay *overlay;
    SDL_Rect drect;
    SDL_Thread *mythread;
    //SDL_mutex *affmutex;
    int signalquit;

    vpx_dec_ctx_t decoder;

    SDL_Thread *m_video_thread;
    SDL_mutex *m_vbuffer_mutex;
    SDL_Overlay *m_overlay_buffer[OVERLAY_BUFFER_SIZE];
    long long m_overlay_milli[OVERLAY_BUFFER_SIZE];
    int m_vbuffer_read;
    int m_vbuffer_write;
    int m_vbuffer_size;

    long long m_last_video_milli;
    long long m_last_video_jitter;

    // Audio
    bool m_setup_audio;
    int m_channels;
    int m_sample_rate;
    int m_bits_per_sample;
    SDL_mutex *m_audio_mutex;
    SDL_cond *m_audio_cond;
    SDL_AudioSpec m_wanted_spec;
    SDL_AudioSpec m_spec;
    unsigned char* m_scratch_buffer;
    int m_scratch_size;
    long long m_total_samples;

    std::queue<AudioFrame*> m_audio_queue;

    WebmMfVorbisDecLib::VorbisDecoder m_vorbis_decoder;

    // Timing
    unsigned int m_base_milli;

    bool m_inited;

    //DISALLOW_COPY_AND_ASSIGN(SDLVideoPlayer);
};


#endif // __WEBMDSHOW_MEDIAFOUNDATION_WEBMMFTESTS_SDLPLAY_SDLVIDEOPLAYER_H__
