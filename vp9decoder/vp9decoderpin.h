// Copyright (c) 2013 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WEBMDSHOW_VP9DECODER_VP9DECODERPIN_HPP_
#define WEBMDSHOW_VP9DECODER_VP9DECODERPIN_HPP_

#include <amvideo.h>
#include <strmif.h>

#include <string>

#include "cmediatypes.h"
#include "graphutil.h"

namespace VP9DecoderLib {

class Filter;

class Pin : public IPin {
 public:
  // IPin interface:
  HRESULT STDMETHODCALLTYPE Disconnect();
  HRESULT STDMETHODCALLTYPE ConnectedTo(IPin** pPin);
  HRESULT STDMETHODCALLTYPE ConnectionMediaType(AM_MEDIA_TYPE*);
  HRESULT STDMETHODCALLTYPE QueryPinInfo(PIN_INFO*);
  HRESULT STDMETHODCALLTYPE QueryDirection(PIN_DIRECTION*);
  HRESULT STDMETHODCALLTYPE QueryId(LPWSTR*);
  HRESULT STDMETHODCALLTYPE EnumMediaTypes(IEnumMediaTypes**);

  Filter* const m_pFilter;
  const PIN_DIRECTION m_dir;
  const std::wstring m_id;
  CMediaTypes m_preferred_mtv;
  CMediaTypes m_connection_mtv;  // only one of these
  GraphUtil::IPinPtr m_pPinConnection;

 protected:
  virtual HRESULT GetName(PIN_INFO&) const = 0;
  virtual HRESULT OnDisconnect();
  Pin(Filter* pFilter, PIN_DIRECTION dir, const wchar_t* id)
      : m_pFilter(pFilter), m_dir(dir), m_id(id) {}
  virtual ~Pin() {}

 private:
  Pin& operator=(const Pin&);
};

}  // namespace VP9DecoderLib

#endif  // WEBMDSHOW_VP9DECODER_VP9DECODERPIN_HPP_