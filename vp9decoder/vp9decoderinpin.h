// Copyright (c) 2013 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMDSHOW_VP9DECODER_VP9DECODERINPIN_HPP_
#define WEBMDSHOW_VP9DECODER_VP9DECODERINPIN_HPP_

#include <amvideo.h>

#include "vpx/vpx_decoder.h"

#include "graphutil.h"
#include "vp9decoderpin.h"

namespace VP9DecoderLib {

class Inpin : public Pin, public IMemInputPin {
 public:
  explicit Inpin(Filter*);

  // IUnknown interface:
  HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  // IPin interface:
  HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE*);
  HRESULT STDMETHODCALLTYPE Connect(IPin*, const AM_MEDIA_TYPE*);
  HRESULT STDMETHODCALLTYPE ReceiveConnection(IPin*, const AM_MEDIA_TYPE*);
  HRESULT STDMETHODCALLTYPE QueryInternalConnections(IPin**, ULONG*);
  HRESULT STDMETHODCALLTYPE EndOfStream();
  HRESULT STDMETHODCALLTYPE BeginFlush();
  HRESULT STDMETHODCALLTYPE EndFlush();
  HRESULT STDMETHODCALLTYPE NewSegment(REFERENCE_TIME, REFERENCE_TIME, double);

  // IMemInputPin
  HRESULT STDMETHODCALLTYPE GetAllocator(IMemAllocator**);
  HRESULT STDMETHODCALLTYPE NotifyAllocator(IMemAllocator*, BOOL);
  HRESULT STDMETHODCALLTYPE GetAllocatorRequirements(ALLOCATOR_PROPERTIES*);
  HRESULT STDMETHODCALLTYPE Receive(IMediaSample*);
  HRESULT STDMETHODCALLTYPE ReceiveMultiple(IMediaSample**, long, long*);
  HRESULT STDMETHODCALLTYPE ReceiveCanBlock();

  // local functions
  HRESULT Start();  // from stopped to running/paused
  void Stop();  // from running/paused to stopped

 protected:
  HRESULT GetName(PIN_INFO&) const;
  HRESULT OnDisconnect();

 private:
  static void CopyToPlanar(const vpx_image_t* image, IMediaSample* sample,
                           const GUID& subtype_out,
                           const BITMAPINFOHEADER& bmih_out);

  static void CopyToPacked(const vpx_image_t* image, IMediaSample* sample,
                           const GUID& subtype_out, const RECT& rc_out,
                           const BITMAPINFOHEADER& bmih_out);

  Inpin(const Inpin&);
  Inpin& operator=(const Inpin&);

  bool m_bEndOfStream;
  bool m_bFlush;
  vpx_codec_ctx_t m_ctx;
};

}  // namespace VP9DecoderLib

#endif  // WEBMDSHOW_VP9DECODER_VP9DECODERINPIN_HPP_
