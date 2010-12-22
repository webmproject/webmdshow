// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <windowsx.h>

#include <cassert>
#include <cmath>
#include <queue>
#include <vector>

#include "debugutil.hpp"
#include "SDLVideoPlayer.h"

#define TRY_DECODE_THREAD 1
#define TRY_AUDIO_TIMING 1

#define FF_REFRESH_EVENT (SDL_USEREVENT)

struct VORBISFORMAT2  //matroska.org
{
  DWORD channels;
  DWORD samplesPerSec;
  DWORD bitsPerSample;
  DWORD headerSize[3];
};

SDLVideoPlayer::SDLVideoPlayer():
  signalquit(1),
  m_width(0),
  m_height(0),
  pscreen(NULL),
  overlay(NULL),
  mythread(NULL),
  //affmutex(NULL),
  m_vbuffer_mutex(NULL),
  m_setup_audio(false),
  m_channels(NULL),
  m_sample_rate(NULL),
  m_bits_per_sample(NULL),
  m_audio_mutex(NULL),
  m_audio_cond(NULL),
  m_scratch_buffer(NULL),
  m_scratch_size(0),
  m_vbuffer_read(0),
  m_vbuffer_write(0),
  m_vbuffer_size(0),
  m_base_milli(0),
  m_total_samples(0),
  m_inited(false),
  m_last_video_milli(-1),
  m_last_video_jitter(0)
{
  // Since this doesn't work:
  //#pragma warning(push)
  // disable unreferenced local function removed warning: we don't care
  // that msvc removed vpx_codec_control_VP8_SET_REFERENCE and
  // vpx_codec_control_VP8_COPY_REFERENCE
  //#pragma warning(disable:4505)
  //#pragma warning(pop)
  // Which is by design, according to: http://support.microsoft.com/kb/947783
  // Set a couple bogus function pointers to the following:
  typedef vpx_codec_err_t (*bogus_fn_ptr)(vpx_codec_ctx_t*,
                                          int, vpx_ref_frame_t*);
  bogus_fn_ptr ptr_bogus_1 = vpx_codec_control_VP8_SET_REFERENCE;
  bogus_fn_ptr ptr_bogus_2 = vpx_codec_control_VP8_COPY_REFERENCE;
  // and reference them to avoid the unreferenced local warning
  ptr_bogus_1;
  ptr_bogus_2;
}

SDLVideoPlayer::~SDLVideoPlayer()
{
  SDL_Quit();
}

int SDLVideoPlayer::Init()
{
    // TODO(tomfinegan): fgalligan@ had some comments about this method
    //                   hanging if called at certain times in a command
    //                   line app... keep an eye on this spot.
    if (m_inited)
    {
        DBGLOG("Already initialized");
        return S_FALSE;
    }
    if(SDL_Init(SDL_INIT_EVERYTHING) == -1)
    {
        DBGLOG("SDL_Init failed, " << SDL_GetError());
        return E_FAIL;
    }
    m_vbuffer_mutex = SDL_CreateMutex();
    m_audio_mutex = SDL_CreateMutex();
    m_audio_cond = SDL_CreateCond();
    for (int i = 0; i < OVERLAY_BUFFER_SIZE; ++i)
    {
        m_overlay_buffer[i] = NULL;
    }
    m_inited = true;
    return 0;
}

int SDLVideoPlayer::setup_surface(int display_width, int display_height)
{
  Init();

  m_width = display_width;
  m_height = display_height;

  //Initialize all SDL subsystems
  //if( SDL_Init( SDL_INIT_EVERYTHING ) == -1 )
  //{
  //  return false;
  //}

  const Uint32 SDL_VIDEO_Flags = SDL_ANYFORMAT | SDL_DOUBLEBUF;
  //const Uint32 SDL_VIDEO_Flags =
  //    SDL_ANYFORMAT | SDL_DOUBLEBUF | SDL_RESIZABLE;
  //const Uint32 SDL_VIDEO_Flags = SDL_SWSURFACE | SDL_RESIZABLE;

  // TODO(tomfinegan): check pointer
  pscreen = SDL_SetVideoMode(display_width, display_height, 0,
                             SDL_VIDEO_Flags);
  if (!pscreen)
  {
      return E_OUTOFMEMORY;
  }
  // TODO(tomfinegan): check pointer
  overlay = SDL_CreateYUVOverlay(display_width, display_height,
                                 SDL_YV12_OVERLAY, pscreen);
  if (!overlay)
  {
      DBGLOG("SDL_CreateYUVOverlay failed, " << SDL_GetError());
      return E_OUTOFMEMORY;
  }
  for (int i=0; i<OVERLAY_BUFFER_SIZE; i++)
  {
    // TODO(tomfinegan): check pointers
    m_overlay_buffer[i] = SDL_CreateYUVOverlay(display_width, display_height,
                                               SDL_YV12_OVERLAY, pscreen);
    if (!m_overlay_buffer[i])
    {
        DBGLOG("SDL_CreateYUVOverlay failed, i=" << i << ", "
            << SDL_GetError());
        return E_OUTOFMEMORY;
    }
  }

  drect.x = 0;
  drect.y = 0;
  drect.w = static_cast<Uint16>(pscreen->w);
  drect.h = static_cast<Uint16>(pscreen->h);

  SDL_WM_SetCaption("PlaybackAdaptive", NULL);
  SDL_LockYUVOverlay(overlay);
  SDL_UnlockYUVOverlay(overlay);

  /* initialize thread data */
  //affmutex = SDL_CreateMutex();
  mythread = SDL_CreateThread(event_thread, (void *)this);
#ifdef TRY_DECODE_THREAD
  m_video_thread = SDL_CreateThread(video_decode_loop, (void *)this);
#endif

  return 0;
}


int SDLVideoPlayer::Exit()
{
  signalquit = 0;

  SDL_CloseAudio();

  SDL_Event sdlevent;
  sdlevent.type = SDL_QUIT;
  sdlevent.user.data1 = this;
  SDL_PushEvent(&sdlevent);

#ifdef TRY_DECODE_THREAD
  SDL_WaitThread(m_video_thread, NULL);
#endif
  SDL_WaitThread(mythread, NULL);

  for(int i = 0; i < OVERLAY_BUFFER_SIZE; ++i)
  {
      if (m_overlay_buffer[i])
      {
          SDL_FreeYUVOverlay(m_overlay_buffer[i]);
          m_overlay_buffer[i] = NULL;
      }
  }
  SDL_FreeYUVOverlay(overlay);
  overlay = NULL;

  m_vbuffer_mutex = SDL_CreateMutex();
  m_audio_mutex = SDL_CreateMutex();
  m_audio_cond = SDL_CreateCond();

  SDL_DestroyCond(m_audio_cond);
  SDL_DestroyMutex(m_audio_mutex);
  SDL_DestroyMutex(m_vbuffer_mutex);

  SDL_Quit();

  return 0;
}

#ifdef TRY_DECODE_THREAD

/*
int SDLVideoPlayer::event_thread(void *data)
{
  SDLVideoPlayer* pSDLPlayer = (SDLVideoPlayer*)data;

  //Initialize all SDL subsystems
  if( SDL_Init( SDL_INIT_EVERYTHING ) == -1 )
  {
    return false;
  }

  //Set up the screen
  pSDLPlayer->pscreen =
    SDL_SetVideoMode( 640, 480, 0, SDL_SWSURFACE | SDL_RESIZABLE );

  //Set the window caption
  SDL_WM_SetCaption( "Window Event Test", NULL );


  while (pSDLPlayer->signalquit)
  {
    SDL_Event sdlevent;
    SDL_WaitEvent(&sdlevent);
    switch (sdlevent.type)
    {
    case SDL_VIDEORESIZE:
      //Resize the screen
      pSDLPlayer->pscreen = SDL_SetVideoMode(sdlevent.resize.w,
                                             sdlevent.resize.h, 0,
                                             SDL_SWSURFACE | SDL_RESIZABLE);
      break;
    case SDL_QUIT:
      printf("\nStop asked\n");
      pSDLPlayer->signalquit = 0;
      break;
    }
  }               //end main loop

  return 0;
}
*/

int SDLVideoPlayer::event_thread(void *data)
{
  SDLVideoPlayer* pSDLPlayer = (SDLVideoPlayer*)data;

  while (pSDLPlayer->signalquit)
  {
    SDL_Event sdlevent;
    SDL_WaitEvent(&sdlevent);
    switch (sdlevent.type)
    {
    case SDL_QUIT:
      printf("\nStop asked\n");
      pSDLPlayer->signalquit = 0;
      break;
    }
  }               //end main loop

  return 0;
}

#else
int SDLVideoPlayer::event_thread(void *data)
{
  SDLVideoPlayer* pSDLPlayer = (SDLVideoPlayer*)data;

  while (pSDLPlayer->signalquit)
  {
    SDL_Event sdlevent;
    SDL_WaitEvent(&sdlevent);
    switch (sdlevent.type)
    {
    case SDL_QUIT:
      printf("\nStop asked\n");
      pSDLPlayer->signalquit = 0;
      break;
    }
  }               //end main loop

  return 0;
}
/*
int SDLVideoPlayer::event_thread(void *data)
{
    SDLVideoPlayer* pSDLPlayer = (SDLVideoPlayer*)data;
    SDL_Surface *pscreen = pSDLPlayer->pscreen;
    SDL_Rect *drect = &pSDLPlayer->drect;
    //SDL_mutex *affmutex = pSDLPlayer->affmutex;

    while (pSDLPlayer->signalquit)
    {
        //SDL_LockMutex(affmutex);

        SDL_Event sdlevent;

        while (SDL_PollEvent(&sdlevent))     //scan the event queue
        {
            switch (sdlevent.type)
            {
            case SDL_VIDEORESIZE:
                pscreen = SDL_SetVideoMode(
                            sdlevent.resize.w & 0xfffe,
                            sdlevent.resize.h & 0xfffe, 0,
                            SDL_ANYFORMAT | SDL_DOUBLEBUF | SDL_RESIZABLE);
                drect->w = sdlevent.resize.w & 0xfffe;
                drect->h = sdlevent.resize.h & 0xfffe;
                break;

            case SDL_KEYUP:
                break;
            case SDL_KEYDOWN:

                switch (sdlevent.key.keysym.sym)
                {
                case SDLK_a:
                    break;
                case SDLK_s:
                    break;
                case SDLK_z:
                    break;
                case SDLK_x:
                    break;
                default :
                    break;
                }

                break;
            case SDL_QUIT:
                printf("\nStop asked\n");
                pSDLPlayer->signalquit = 0;
                break;
            }
        }           //end if poll

        //SDL_UnlockMutex(affmutex);
        SDL_Delay(50);
    }               //end main loop

    return 0;

}
*/

#endif

int SDLVideoPlayer::video_decode_loop(void *data)
{
  SDLVideoPlayer* pSDLPlayer = (SDLVideoPlayer*)data;

  while (pSDLPlayer->signalquit)
  {
    //int rv = 0;
    int delay = 0;

    SDL_LockMutex(pSDLPlayer->m_vbuffer_mutex);

    if (pSDLPlayer->m_vbuffer_size == 0)
    {
      delay = 1;
    }
    else
    {
      unsigned int time = pSDLPlayer->get_playback_milli();

      if (pSDLPlayer->m_overlay_milli[pSDLPlayer->m_vbuffer_read] > time)
      {
        // Frame is not ready to be shown
        __int64 overlay_milli = pSDLPlayer->m_overlay_milli[pSDLPlayer->m_vbuffer_read];
        delay = static_cast<int>(overlay_milli - time);
      }
      else
      {
        pSDLPlayer->m_last_video_milli =
            pSDLPlayer->m_overlay_milli[pSDLPlayer->m_vbuffer_read];

        //SDL_LockMutex(affmutex);
        SDL_DisplayYUVOverlay(
            pSDLPlayer->m_overlay_buffer[pSDLPlayer->m_vbuffer_read],
            &pSDLPlayer->drect);
        //SDL_UnlockMutex(affmutex);

        pSDLPlayer->m_last_video_jitter =
            time - pSDLPlayer->m_last_video_milli;

        pSDLPlayer->m_vbuffer_size--;
        pSDLPlayer->m_vbuffer_read++;
        if (pSDLPlayer->m_vbuffer_read == OVERLAY_BUFFER_SIZE)
          pSDLPlayer->m_vbuffer_read = 0;

        if (pSDLPlayer->m_last_video_jitter > 19)
        {
          delay = 1;
        }
        else
        {
          delay = 20 - static_cast<int>(pSDLPlayer->m_last_video_jitter);
        }
      }
    }

    SDL_UnlockMutex(pSDLPlayer->m_vbuffer_mutex);

    SDL_Delay(delay);
  }

  return 0;
}

int SDLVideoPlayer::show_frame(vpx_image_t *img, int display_width,
                               int display_height)
{
    //SDL_LockMutex(affmutex);

    convert_frame(overlay, img, display_width, display_height);

    SDL_DisplayYUVOverlay(overlay, &drect);
    //SDL_UnlockMutex(affmutex);
    return 0;
}

int SDLVideoPlayer::convert_frame(SDL_Overlay *dst_overlay,
                                  vpx_image_t *src_img,
                                  int display_width,
                                  int display_height)
{
    SDL_LockYUVOverlay(dst_overlay);
    int i;

    unsigned char *in = src_img->planes[PLANE_Y];
    unsigned char *p = (unsigned char *) dst_overlay->pixels[0];

    for (i = 0; i < display_height;
        i++, in += src_img->stride[PLANE_Y], p += display_width)
    {
        memcpy(p, in, display_width);
    }

    in = src_img->planes[PLANE_V];

    for (i = 0; i < display_height / 2;
        i++, in += src_img->stride[PLANE_V], p += display_width / 2)
    {
        memcpy(p, in, display_width / 2);
    }

    in = src_img->planes[PLANE_U];

    for (i = 0; i < display_height / 2;
        i++, in += src_img->stride[PLANE_U], p += display_width / 2)
    {
        memcpy(p, in, display_width / 2);
    }

    SDL_UnlockYUVOverlay(dst_overlay);
    return 0;
}

int SDLVideoPlayer::put_frame(vpx_image_t *img, long long time,
                              int display_width, int display_height)
{
  int rv = 0;

  SDL_LockMutex(m_vbuffer_mutex);

  if (m_vbuffer_size == OVERLAY_BUFFER_SIZE)
  {
    // 1 signals we are full
    rv = 1;
  }
  else
  {
    // TODO(tomfinegan): check result or make void
    convert_frame(m_overlay_buffer[m_vbuffer_write], img, display_width,
                  display_height);
    m_overlay_milli[m_vbuffer_write] = time;
    m_vbuffer_size++;
    m_vbuffer_write++;
    if (m_vbuffer_write == OVERLAY_BUFFER_SIZE)
      m_vbuffer_write = 0;
  }

  SDL_UnlockMutex(m_vbuffer_mutex);
  return rv;
}

int SDLVideoPlayer::setup_vpx_decoder()
{
  vp8_postproc_cfg_t    ppcfg;
  vpx_codec_dec_cfg_t     cfg = {0};
  vpx_codec_dec_init(&decoder, &vpx_codec_vp8_dx_algo, &cfg, 0);

  /* Config post processing settings for decoder */
  ppcfg.post_proc_flag = VP8_DEMACROBLOCK | VP8_DEBLOCK;
  ppcfg.deblocking_level = 5;
  vpx_codec_control(&decoder, VP8_SET_POSTPROC, &ppcfg);

  return 0;
}

int SDLVideoPlayer::close_vpx_decoder()
{
  if (vpx_codec_destroy(&decoder))
  {
    //vpxlog_dbg(DISCARD, "Failed to destroy decoder: %s\n",
    //           vpx_codec_error(&decoder));
    return -1;
  }

  return 0;
}

int SDLVideoPlayer::decode_vpx_frame(const unsigned char* data,
                                     const int size, const long long milli,
                                     const long long jitter)
{
#ifdef TRY_DECODE_THREAD
  jitter;
  // Check to see if the decode buffer is full
  SDL_LockMutex(m_vbuffer_mutex);
  if (m_vbuffer_size == OVERLAY_BUFFER_SIZE)
  {
    // 1 signals we are full
    SDL_UnlockMutex(m_vbuffer_mutex);
    return 1;
  }
  SDL_UnlockMutex(m_vbuffer_mutex);

  vpx_dec_iter_t  iter = NULL;
  vpx_image_t    *img;

  int rv = 0;

  if (vpx_codec_decode(&decoder, data, size, 0, 0))
  {
    //vpxlog_dbg(FRAME, "Failed to decode frame: %s\n",
    //             vpx_codec_error(&decoder));
    return -1;
  }

  img = vpx_codec_get_frame(&decoder, &iter);

  put_frame(img, milli, m_width, m_height);
#else
  vpx_dec_iter_t  iter = NULL;
  vpx_image_t    *img;
  int rv = 0;

  if (vpx_codec_decode(&decoder, data, size, 0, 0))
  {
    //vpxlog_dbg(FRAME, "Failed to decode frame: %s\n",
    //             vpx_codec_error(&decoder));
    return -1;
  }

  img = vpx_codec_get_frame(&decoder, &iter);

  //TODO: Setup code to buffer up decoded frames
  show_frame(img, m_width, m_height);
#endif

  return rv;
}

int SDLVideoPlayer::setup_audio_hardware(const unsigned int channels,
                                         const unsigned int sample_rate,
                                         const unsigned int bits_per_sample)
{
  Init();

  m_channels = channels;
  m_sample_rate = sample_rate;
  m_bits_per_sample = bits_per_sample;

  m_wanted_spec.freq = m_sample_rate;
  m_wanted_spec.format = AUDIO_S16SYS;
  m_wanted_spec.channels = static_cast<Uint8>(m_channels);
  m_wanted_spec.silence = 0;
  m_wanted_spec.samples = 1024; // SDL_AUDIO_BUFFER_SIZE;
  m_wanted_spec.callback = audio_callback;
  m_wanted_spec.userdata = this;

  if(SDL_OpenAudio(&m_wanted_spec, &m_spec) < 0) {
    fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
    return -1;
  }

  m_setup_audio = true;

  // Start the audio thread.
  // TODO: Does this need to be called from somewhere else?
  SDL_PauseAudio(0);

  return 0;
}

int SDLVideoPlayer::setup_vorbis_decoder(const unsigned char* data,
                                         const int size)
{
  // From WebM Media Foundation Source
  const BYTE* const begin = data;
  const BYTE* const end = begin + size;

  const BYTE* p = begin;
  //assert(p < end);

  const BYTE n = *p++;
  n;
  //assert(n == 2);
  //assert(p < end);

  const ULONG id_len = *p++;  //TODO: don't assume < 255
  //assert(id_len < 255);
  //assert(id_len > 0);
  //assert(p < end);

  const ULONG comment_len = *p++;  //TODO: don't assume < 255
  //assert(comment_len < 255);
  //assert(comment_len > 0);
  //assert(p < end);

  //p points to first header

  const BYTE* const id_hdr = p;
  id_hdr;

  const BYTE* const comment_hdr = id_hdr + id_len;
  comment_hdr;

  const BYTE* const setup_hdr = comment_hdr + comment_len;
  setup_hdr;
  //assert(setup_hdr < end);

  const ptrdiff_t setup_len_ = end - setup_hdr;
  //assert(setup_len_ > 0);

  const DWORD setup_len = static_cast<DWORD>(setup_len_);

  const size_t hdr_len = id_len + comment_len + setup_len;

  //using VorbisTypes::VORBISFORMAT2;

  const size_t cb = sizeof(VORBISFORMAT2) + hdr_len;
  unsigned char* const pb = new unsigned char[cb];

  VORBISFORMAT2& fmt = (VORBISFORMAT2&)(*pb);

  fmt.channels = m_channels;
  fmt.samplesPerSec = m_sample_rate;
  fmt.bitsPerSample = m_bits_per_sample;
  fmt.headerSize[0] = id_len;
  fmt.headerSize[1] = comment_len;
  fmt.headerSize[2] = setup_len;

  //assert(p < end);
  //assert(size_t(end - p) == hdr_len);

  BYTE* const dst = pb + sizeof(VORBISFORMAT2);
  memcpy(dst, p, hdr_len);


  // From Vorbis Decoder Media Foundation
  const VORBISFORMAT2& vorbis_format =
      *reinterpret_cast<const VORBISFORMAT2*>(pb);

  const BYTE* p_headers[3];
  p_headers[0] = pb + sizeof VORBISFORMAT2;
  p_headers[1] = p_headers[0] + vorbis_format.headerSize[0];
  p_headers[2] = p_headers[1] + vorbis_format.headerSize[1];

  int status = m_vorbis_decoder.CreateDecoder(p_headers,
    &vorbis_format.headerSize[0],
    WebmMfVorbisDecLib::VORBIS_SETUP_HEADER_COUNT);

  status;
  return 0;
}

int SDLVideoPlayer::vorbis_queue_put(const unsigned char* data,
                                     const int size)
{
  AudioFrame* pAudioFrame = new AudioFrame();
  pAudioFrame->data = new unsigned char[size];
  memcpy(pAudioFrame->data, data, size);
  pAudioFrame->size = size;

  SDL_LockMutex(m_audio_mutex);
  m_audio_queue.push(pAudioFrame);
  SDL_CondSignal(m_audio_cond);

  SDL_UnlockMutex(m_audio_mutex);
  return 0;
}

int SDLVideoPlayer::packet_queue_get(AudioFrame **pkt, int block)
{
  int ret;

  SDL_LockMutex(m_audio_mutex);

  for(;;) {

    /*
    if(quit) {
      ret = -1;
      break;
    }
    */

    if (m_audio_queue.empty())
    {
      if (!block) {
        ret = 0;
        break;
      } else {
        SDL_CondWait(m_audio_cond, m_audio_mutex);
      }
    }
    else
    {
      *pkt = m_audio_queue.front();
      m_audio_queue.pop();
      ret = 1;
      break;
    }
  }
  SDL_UnlockMutex(m_audio_mutex);
  return ret;
}

void SDLVideoPlayer::audio_callback(void *userdata, Uint8 *stream, int len)
{
  SDLVideoPlayer *pPlayer = (SDLVideoPlayer *)userdata;

  // TODO: use block align here
  const unsigned int samples_wanted = len / 4;

  while (len > 0)
  {
    unsigned int samples_available;
    int rv = pPlayer->m_vorbis_decoder.GetOutputSamplesAvailable(
        &samples_available);

    // TODO: use block align here
    unsigned int bytes_available = samples_available * 4;

    assert(len > 0);
    unsigned int ulen = len;
    if (bytes_available >= ulen)
    {
      // TODO Don't use magic numbers here
      const int float_buf_size = samples_wanted * 2 * 4;
      if (pPlayer->m_scratch_size < float_buf_size)
      {
        delete [] pPlayer->m_scratch_buffer;
        pPlayer->m_scratch_buffer = new unsigned char[float_buf_size];
        pPlayer->m_scratch_size = float_buf_size;
      }

      rv = pPlayer->m_vorbis_decoder.ConsumeOutputSamples(
          (float*)pPlayer->m_scratch_buffer, samples_wanted);

      const int samples_to_convert = samples_wanted * 2;

      short *ptr = (short*)stream;
      float *mono = (float *)pPlayer->m_scratch_buffer;

      for (int i=0; i<samples_to_convert; i++)
      {
        int val=static_cast<int>(floor(mono[i]*32767.f+.5f));

        if(val>32767){
          val=32767;
          //clipflag=1;
        }
        if(val<-32768){
          val=-32768;
          //clipflag=1;
        }
        *ptr=static_cast<short>(val);
        ptr++;
      }

#ifdef TRY_AUDIO_TIMING
      pPlayer->m_total_samples += samples_wanted;
#endif

      len -= len;
      stream += len;
    }
    else
    {
      // Need to decode audio data
      AudioFrame* pPkt;
      if(pPlayer->packet_queue_get(&pPkt, 1) < 0) {
        return;
      }

      rv = pPlayer->m_vorbis_decoder.Decode(pPkt->data, pPkt->size);

      // TODO: Try and resuse memory here
      delete [] pPkt->data;
      delete pPkt;
    }
  }
}

unsigned int SDLVideoPlayer::get_playback_milli()
{
#ifdef TRY_AUDIO_TIMING
  if (m_setup_audio)
  {
    double milliseconds =
        (((double)m_total_samples) / m_sample_rate) * 1000.0;
    return (unsigned int)milliseconds;
  }
#endif
  if (!m_base_milli)
    m_base_milli = timeGetTime();

  return timeGetTime() - m_base_milli;

}

int SDLVideoPlayer::get_last_time_rendered(long long& video_milli,
                                           long long& video_milli_jitter,
                                           long long& audio_milli)
{
  audio_milli = -1;

#ifdef TRY_AUDIO_TIMING
  if (m_setup_audio)
  {
    double milliseconds =
        ((static_cast<double>(m_total_samples)) / m_sample_rate) * 1000.0;
    audio_milli = static_cast<long long>(milliseconds);
  }
#endif

  video_milli = m_last_video_milli;
  video_milli_jitter = m_last_video_jitter;

  return 0;
}
