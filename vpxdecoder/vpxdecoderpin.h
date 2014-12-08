// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMDSHOW_VPXDECODER_VPXDECODERPIN_H_
#define WEBMDSHOW_VPXDECODER_VPXDECODERPIN_H_

#include <amvideo.h>
#include <strmif.h>

#include <string>

#include "cmediatypes.h"
#include "graphutil.h"

namespace VPXDecoderLib {
class Filter;

class Pin : public IPin {
 public:
  Filter* const m_pFilter;
  const PIN_DIRECTION m_dir;
  const std::wstring m_id;
  CMediaTypes m_preferred_mtv;
  CMediaTypes m_connection_mtv;  // only one of these
  GraphUtil::IPinPtr m_pPinConnection;

  // IPin interface:
  HRESULT STDMETHODCALLTYPE Disconnect();
  HRESULT STDMETHODCALLTYPE ConnectedTo(IPin** pPin);
  HRESULT STDMETHODCALLTYPE ConnectionMediaType(AM_MEDIA_TYPE*);
  HRESULT STDMETHODCALLTYPE QueryPinInfo(PIN_INFO*);
  HRESULT STDMETHODCALLTYPE QueryDirection(PIN_DIRECTION*);
  HRESULT STDMETHODCALLTYPE QueryId(LPWSTR*);
  HRESULT STDMETHODCALLTYPE EnumMediaTypes(IEnumMediaTypes**);

 protected:
  Pin(Filter*, PIN_DIRECTION, const wchar_t*);
  virtual ~Pin();

  virtual HRESULT GetName(PIN_INFO&) const = 0;
  virtual HRESULT OnDisconnect();

 private:
   Pin& operator=(const Pin&);
};

}  // namespace VPXDecoderLib

#endif  // WEBMDSHOW_VPXDECODER_VPXDECODERPIN_H_
