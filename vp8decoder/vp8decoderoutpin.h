// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBDSHOW_VP8DECODER_VP8DECODEROUTPIN_HPP_
#define WEBDSHOW_VP8DECODER_VP8DECODEROUTPIN_HPP_

#include <comdef.h>

#include "graphutil.h"
#include "vp8decoderpin.h"

namespace VP8DecoderLib {
class Filter;

class Outpin : public Pin, public IMediaSeeking {
 public:
  explicit Outpin(Filter*);
  virtual ~Outpin();

  // IUnknown interface:
  HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  // IPin interface:
  HRESULT STDMETHODCALLTYPE Connect(IPin* pin, const AM_MEDIA_TYPE* media_type);
  HRESULT STDMETHODCALLTYPE
      ReceiveConnection(IPin* pin, const AM_MEDIA_TYPE* media_type);

  HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE*);
  HRESULT STDMETHODCALLTYPE QueryInternalConnections(IPin**, ULONG*);
  HRESULT STDMETHODCALLTYPE EndOfStream();
  HRESULT STDMETHODCALLTYPE BeginFlush();
  HRESULT STDMETHODCALLTYPE EndFlush();
  HRESULT STDMETHODCALLTYPE NewSegment(REFERENCE_TIME, REFERENCE_TIME, double);

  // IMediaSeeking
  HRESULT STDMETHODCALLTYPE GetCapabilities(DWORD*);
  HRESULT STDMETHODCALLTYPE CheckCapabilities(DWORD*);
  HRESULT STDMETHODCALLTYPE IsFormatSupported(const GUID*);
  HRESULT STDMETHODCALLTYPE QueryPreferredFormat(GUID*);
  HRESULT STDMETHODCALLTYPE GetTimeFormat(GUID*);
  HRESULT STDMETHODCALLTYPE IsUsingTimeFormat(const GUID*);
  HRESULT STDMETHODCALLTYPE SetTimeFormat(const GUID*);
  HRESULT STDMETHODCALLTYPE GetDuration(LONGLONG*);
  HRESULT STDMETHODCALLTYPE GetStopPosition(LONGLONG*);
  HRESULT STDMETHODCALLTYPE GetCurrentPosition(LONGLONG*);

  HRESULT STDMETHODCALLTYPE
      ConvertTimeFormat(LONGLONG*, const GUID*, LONGLONG, const GUID*);

  HRESULT STDMETHODCALLTYPE SetPositions(LONGLONG*, DWORD, LONGLONG*, DWORD);

  HRESULT STDMETHODCALLTYPE GetPositions(LONGLONG*, LONGLONG*);

  HRESULT STDMETHODCALLTYPE GetAvailable(LONGLONG*, LONGLONG*);

  HRESULT STDMETHODCALLTYPE SetRate(double);
  HRESULT STDMETHODCALLTYPE GetRate(double*);
  HRESULT STDMETHODCALLTYPE GetPreroll(LONGLONG*);

  // local functions
  GraphUtil::IMemInputPinPtr m_pInputPin;
  GraphUtil::IMemAllocatorPtr m_pAllocator;

  void OnInpinConnect(const AM_MEDIA_TYPE&);
  HRESULT OnInpinDisconnect();

  HRESULT Start();  // from stopped to running/paused
  void Stop();  // from running/paused to stopped

 protected:
  HRESULT OnDisconnect();
  HRESULT GetName(PIN_INFO&) const;

 private:
  Outpin(const Outpin&);
  Outpin& operator=(const Outpin&);

  void SetDefaultMediaTypes();

  static HRESULT QueryAcceptVideoInfo(const AM_MEDIA_TYPE& mt_in,
                                      const AM_MEDIA_TYPE& mt_out);

  static HRESULT QueryAcceptVideoInfo2(const AM_MEDIA_TYPE& mt_in,
                                       const AM_MEDIA_TYPE& mt_out);

  static bool VetBMIH(const VIDEOINFOHEADER& vih_in, const GUID& subtype_out,
                      const RECT& rc_out, const BITMAPINFOHEADER& bmih_out);

  void GetConnectionDimensions(LONG& w, LONG& h) const;

  void AddPreferred(const GUID& subtype, REFERENCE_TIME AvgTimePerFrame,
                    LONG width, LONG height, DWORD dwBitCount,
                    DWORD dwSizeImage);

  static void AddVIH(CMediaTypes&, const GUID& subtype,
                     REFERENCE_TIME AvgTimePerFrame, LONG width, LONG height,
                     DWORD dwBitCount, DWORD dwSizeImage);

  static void AddVIH2(CMediaTypes&, const GUID& subtype,
                      REFERENCE_TIME AvgTimePerFrame, LONG width, LONG height,
                      DWORD dwBitCount, DWORD dwSizeImage);
};

}  // end namespace VP8DecoderLib

#endif  // WEBDSHOW_VP8DECODER_VP8DECODEROUTPIN_HPP_
