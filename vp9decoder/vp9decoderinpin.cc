// Copyright (c) 2013 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

// TODO(tomfinegan): Get rid of this warning.
#pragma warning(once : 4505)  // unreferenced local function has been removed
#include "vp9decoderinpin.h"

#include <dvdmedia.h>
#include <evcode.h>
#include <uuids.h>
#include <vfwmsgs.h>

#include <cassert>

#include "vpx/vp8dx.h"

#include "graphutil.h"
#include "mediatypeutil.h"
#include "vp9decoderfilter.h"
#include "vp9decoderoutpin.h"
#include "webmtypes.h"

#ifdef _DEBUG
#include <iomanip>
#include "odbgstream.h"
using std::endl;
using std::hex;
using std::dec;
using std::fixed;
using std::setprecision;
#endif

namespace VP9DecoderLib {

Inpin::Inpin(Filter* p)
    : Pin(p, PINDIR_INPUT, L"input"), m_bEndOfStream(false), m_bFlush(false) {
  AM_MEDIA_TYPE mt;

  mt.majortype = MEDIATYPE_Video;
  mt.subtype = WebmTypes::MEDIASUBTYPE_VP90;
  mt.bFixedSizeSamples = FALSE;
  mt.bTemporalCompression = TRUE;
  mt.lSampleSize = 0;
  mt.formattype = GUID_NULL;
  mt.pUnk = 0;
  mt.cbFormat = 0;
  mt.pbFormat = 0;

  m_preferred_mtv.Add(mt);
}

HRESULT Inpin::QueryInterface(const IID& iid, void** ppv) {
  if (ppv == 0)
    return E_POINTER;

  IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

  if (iid == __uuidof(IUnknown)) {
    pUnk = static_cast<IPin*>(this);
  } else if (iid == __uuidof(IPin)) {
    pUnk = static_cast<IPin*>(this);
  } else if (iid == __uuidof(IMemInputPin)) {
    pUnk = static_cast<IMemInputPin*>(this);
  } else {
    pUnk = 0;
    return E_NOINTERFACE;
  }

  pUnk->AddRef();
  return S_OK;
}

ULONG Inpin::AddRef() {
  return m_pFilter->AddRef();
}

ULONG Inpin::Release() {
  return m_pFilter->Release();
}

HRESULT Inpin::Connect(IPin*, const AM_MEDIA_TYPE*) {
  return E_UNEXPECTED;  // for output pins only
}

HRESULT Inpin::QueryInternalConnections(IPin** pa, ULONG* pn) {
  if (pn == 0)
    return E_POINTER;

  Filter::Lock lock;

  HRESULT hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return hr;

  const ULONG m = 1;  // number of output pins

  ULONG& n = *pn;

  if (n == 0) {
    if (pa == 0) {  // query for required number
      n = m;
      return S_OK;
    }

    return S_FALSE;  // means "insufficient number of array elements"
  }

  if (n < m) {
    n = 0;
    return S_FALSE;  // means "insufficient number of array elements"
  }

  if (pa == 0) {
    n = 0;
    return E_POINTER;
  }

  IPin*& pin = pa[0];

  pin = &m_pFilter->m_outpin;
  pin->AddRef();

  n = m;
  return S_OK;
}

HRESULT Inpin::ReceiveConnection(IPin* pin, const AM_MEDIA_TYPE* pmt) {
  if (pin == 0)
    return E_INVALIDARG;

  if (pmt == 0)
    return E_INVALIDARG;

  Filter::Lock lock;

  HRESULT hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return hr;

  if (m_pFilter->GetStateLocked() != State_Stopped)
    return VFW_E_NOT_STOPPED;

  if (bool(m_pPinConnection))
    return VFW_E_ALREADY_CONNECTED;

  m_connection_mtv.Clear();

  hr = QueryAccept(pmt);

  if (hr != S_OK)
    return VFW_E_TYPE_NOT_ACCEPTED;

  const AM_MEDIA_TYPE& mt = *pmt;

  hr = m_connection_mtv.Add(mt);

  if (FAILED(hr))
    return hr;

  m_pPinConnection = pin;

  // TODO: init decompressor here?

  m_pFilter->m_outpin.OnInpinConnect(mt);

  return S_OK;
}

HRESULT Inpin::EndOfStream() {
  Filter::Lock lock;

  HRESULT hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return hr;

  if (!bool(m_pPinConnection))
    return VFW_E_NOT_CONNECTED;

  m_bEndOfStream = true;

  if (IPin* pPin = m_pFilter->m_outpin.m_pPinConnection) {
    lock.Release();

#ifdef _DEBUG
    odbgstream os;
    os << "vp9decoder::inpin::EOS: calling pin->EOS" << endl;
#endif

    const HRESULT hr = pPin->EndOfStream();

#ifdef _DEBUG
    os << "vp9decoder::inpin::EOS: called pin->EOS; hr=0x" << hex << hr << dec
       << endl;
#endif

    return hr;
  }

  return S_OK;
}

HRESULT Inpin::BeginFlush() {
  Filter::Lock lock;

  HRESULT hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return hr;

  if (!bool(m_pPinConnection))
    return VFW_E_NOT_CONNECTED;

#if 0  // def _DEBUG
    odbgstream os;
    os << "vp9decoder::inpin::beginflush" << endl;
#endif

  m_bFlush = true;

  if (IPin* pPin = m_pFilter->m_outpin.m_pPinConnection) {
    lock.Release();

    const HRESULT hr = pPin->BeginFlush();
    return hr;
  }

  return S_OK;
}

HRESULT Inpin::EndFlush() {
  Filter::Lock lock;

  HRESULT hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return hr;

  if (!bool(m_pPinConnection))
    return VFW_E_NOT_CONNECTED;

#if 0  // def _DEBUG
    odbgstream os;
    os << "vp9decoder::inpin::endflush" << endl;
#endif

  m_bFlush = false;
  m_bEndOfStream = false;

  if (IPin* pPin = m_pFilter->m_outpin.m_pPinConnection) {
    lock.Release();

    const HRESULT hr = pPin->EndFlush();
    return hr;
  }

  return S_OK;
}

HRESULT Inpin::NewSegment(REFERENCE_TIME st, REFERENCE_TIME sp, double r) {
  Filter::Lock lock;

  HRESULT hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return hr;

  if (!bool(m_pPinConnection))
    return VFW_E_NOT_CONNECTED;

  if (IPin* pPin = m_pFilter->m_outpin.m_pPinConnection) {
    lock.Release();

    const HRESULT hr = pPin->NewSegment(st, sp, r);
    return hr;
  }

  return S_OK;
}

HRESULT Inpin::QueryAccept(const AM_MEDIA_TYPE* pmt) {
  if (pmt == 0)
    return E_INVALIDARG;

  const AM_MEDIA_TYPE& mt = *pmt;

  if (mt.majortype != MEDIATYPE_Video)
    return S_FALSE;

  if (mt.subtype != WebmTypes::MEDIASUBTYPE_VP90)
    return S_FALSE;

  if (mt.formattype != FORMAT_VideoInfo)  // TODO: liberalize
    return S_FALSE;

  if (mt.pbFormat == 0)
    return S_FALSE;

  if (mt.cbFormat < sizeof(VIDEOINFOHEADER))
    return S_FALSE;

  const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
  const BITMAPINFOHEADER& bmih = vih.bmiHeader;

  if (bmih.biSize != sizeof(BITMAPINFOHEADER))  // TODO: liberalize
    return S_FALSE;

  if (bmih.biWidth <= 0)
    return S_FALSE;

  if (bmih.biHeight <= 0)
    return S_FALSE;

  if (bmih.biCompression != WebmTypes::MEDIASUBTYPE_VP90.Data1)  //"VP90"
    return S_FALSE;

  return S_OK;
}

HRESULT Inpin::GetAllocator(IMemAllocator** p) {
  if (p)
    *p = 0;

  return VFW_E_NO_ALLOCATOR;
}

HRESULT Inpin::NotifyAllocator(IMemAllocator* pAllocator, BOOL) {
  if (pAllocator == 0)
    return E_INVALIDARG;

  ALLOCATOR_PROPERTIES props;

  const HRESULT hr = pAllocator->GetProperties(&props);
  hr;
  assert(SUCCEEDED(hr));

#ifdef _DEBUG
  wodbgstream os;
  os << "vp9dec::inpin::NotifyAllocator: props.cBuffers=" << props.cBuffers
     << " cbBuffer=" << props.cbBuffer << " cbAlign=" << props.cbAlign
     << " cbPrefix=" << props.cbPrefix << endl;
#endif

  return S_OK;
}

HRESULT Inpin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pp) {
  if (pp == 0)
    return E_POINTER;

  return S_OK;
}

HRESULT Inpin::Receive(IMediaSample* pInSample) {
  if (pInSample == 0)
    return E_INVALIDARG;

  //#define DEBUG_RECEIVE

#ifdef DEBUG_RECEIVE
  __int64 start_reftime, stop_reftime;
  const HRESULT hr_debug = pInSample->GetTime(&start_reftime, &stop_reftime);

  odbgstream os;
  os << "vp8dec::inpin::receive: ";

  os << std::fixed << std::setprecision(3);

  if (hr_debug == S_OK) {
    os << "start[ms]=" << double(start_reftime) / 10000
      << "; stop[ms]=" << double(stop_reftime) / 10000
      << "; dt[ms]=" << double(stop_reftime - start_reftime) / 10000;
  } else if (hr_debug == VFW_S_NO_STOP_TIME) {
    os << "start[ms]=" << double(start_reftime) / 10000;
  }

  os << endl;
#endif  // DEBUG_RECEIVE

  Filter::Lock lock;

  HRESULT hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return hr;

  //#ifdef DEBUG_RECEIVE
  //    wodbgstream os;
  //    os << L"vp9dec::inpin::Receive: THREAD=0x"
  //       << std::hex << GetCurrentThreadId() << std::dec
  //       << endl;
  //#endif

  if (!bool(m_pPinConnection))
    return VFW_E_NOT_CONNECTED;

  Outpin& outpin = m_pFilter->m_outpin;

  if (!bool(outpin.m_pPinConnection))
    return S_FALSE;

  if (!bool(outpin.m_pAllocator))  // should never happen
    return VFW_E_NO_ALLOCATOR;

  if (m_pFilter->GetStateLocked() == State_Stopped)
    return VFW_E_NOT_RUNNING;

  if (m_bEndOfStream)
    return VFW_E_SAMPLE_REJECTED_EOS;

  if (m_bFlush)
    return S_FALSE;

  BYTE* buf;

  hr = pInSample->GetPointer(&buf);

  if (FAILED(hr)) {
    assert(SUCCEEDED(hr));
    return hr;
  }

  if (buf == NULL) {
    assert(buf);
    return E_FAIL;
  }

  const long len = pInSample->GetActualDataLength();
  assert(len >= 0);

  const vpx_codec_err_t err = vpx_codec_decode(&m_ctx, buf, len, 0, 0);

  if (err != VPX_CODEC_OK)
    return m_pFilter->OnDecodeFailureLocked();

  hr = pInSample->IsSyncPoint();

  m_pFilter->OnDecodeSuccessLocked(hr == S_OK);

  if (pInSample->IsPreroll() == S_OK)
    return S_OK;

  lock.Release();

  GraphUtil::IMediaSamplePtr pOutSample;

  hr = outpin.m_pAllocator->GetBuffer(&pOutSample, 0, 0, 0);

  if (FAILED(hr))
    return S_FALSE;

  assert(pOutSample != NULL);

  hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return hr;

  if (m_pFilter->GetStateLocked() == State_Stopped)
    return VFW_E_NOT_RUNNING;

  if (outpin.m_pPinConnection == NULL)
    return S_FALSE;

  if (outpin.m_pInputPin == NULL)
    return S_FALSE;

  vpx_codec_iter_t iter = 0;

  vpx_image_t* const f = vpx_codec_get_frame(&m_ctx, &iter);

  if (f == 0)
    return S_OK;

  AM_MEDIA_TYPE* pmt;

  hr = pOutSample->GetMediaType(&pmt);

  if (SUCCEEDED(hr) && (pmt != 0)) {
    assert(outpin.QueryAccept(pmt) == S_OK);
    outpin.m_connection_mtv.Clear();
    outpin.m_connection_mtv.Add(*pmt);

    MediaTypeUtil::Free(pmt);
    pmt = 0;
  }

  const AM_MEDIA_TYPE& mt = outpin.m_connection_mtv[0];

  const BITMAPINFOHEADER* bmih_ptr;
  const RECT* rc_ptr;

  if (mt.formattype == FORMAT_VideoInfo) {
    assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER));
    assert(mt.pbFormat);

    const VIDEOINFOHEADER& vih_out = (VIDEOINFOHEADER&)(*mt.pbFormat);
    const RECT& rc_out = vih_out.rcSource;
    const BITMAPINFOHEADER& bmih_out = vih_out.bmiHeader;

    bmih_ptr = &bmih_out;
    rc_ptr = &rc_out;
  } else if (mt.formattype == FORMAT_VideoInfo2) {
    assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER2));
    assert(mt.pbFormat);

    const VIDEOINFOHEADER2& vih2_out = (VIDEOINFOHEADER2&)(*mt.pbFormat);
    const RECT& rc_out = vih2_out.rcSource;
    const BITMAPINFOHEADER& bmih_out = vih2_out.bmiHeader;

    bmih_ptr = &bmih_out;
    rc_ptr = &rc_out;
  } else {
    return E_FAIL;
  }

  if (mt.subtype == MEDIASUBTYPE_NV12)
    CopyToPlanar(f, pOutSample, mt.subtype, *bmih_ptr);

  else if (mt.subtype == MEDIASUBTYPE_YV12)
    CopyToPlanar(f, pOutSample, mt.subtype, *bmih_ptr);

  else if (mt.subtype == WebmTypes::MEDIASUBTYPE_I420)
    CopyToPlanar(f, pOutSample, mt.subtype, *bmih_ptr);

  else if (mt.subtype == MEDIASUBTYPE_UYVY)
    CopyToPacked(f, pOutSample, mt.subtype, *rc_ptr, *bmih_ptr);

  else if (mt.subtype == MEDIASUBTYPE_YUY2)
    CopyToPacked(f, pOutSample, mt.subtype, *rc_ptr, *bmih_ptr);

  else if (mt.subtype == MEDIASUBTYPE_YUYV)
    CopyToPacked(f, pOutSample, mt.subtype, *rc_ptr, *bmih_ptr);

  else if (mt.subtype == MEDIASUBTYPE_YVYU)
    CopyToPacked(f, pOutSample, mt.subtype, *rc_ptr, *bmih_ptr);

  else
    return E_FAIL;

  __int64 st, sp;

  hr = pInSample->GetTime(&st, &sp);

  if (FAILED(hr)) {
    hr = pOutSample->SetTime(0, 0);
    assert(SUCCEEDED(hr));
  } else if (hr == S_OK) {
    hr = pOutSample->SetTime(&st, &sp);
    assert(SUCCEEDED(hr));
  } else {
    hr = pOutSample->SetTime(&st, 0);
    assert(SUCCEEDED(hr));
  }

  hr = pOutSample->SetSyncPoint(TRUE);
  assert(SUCCEEDED(hr));

  hr = pOutSample->SetPreroll(FALSE);
  assert(SUCCEEDED(hr));

  hr = pInSample->IsDiscontinuity();
  hr = pOutSample->SetDiscontinuity(hr == S_OK);

  hr = pOutSample->SetMediaTime(0, 0);

#if 0
    //LONGLONG st, sp;
    hr = pOutSample->GetTime(&st, &sp);
    assert(SUCCEEDED(hr));

    odbgstream os;
    os << "V: " << fixed << setprecision(3) << (double(st)/10000000.0) << endl;
#endif

  lock.Release();

  return outpin.m_pInputPin->Receive(pOutSample);
}

HRESULT Inpin::ReceiveMultiple(IMediaSample** pSamples,
                               long n,  // in
                               long* pm) { // out
  if (pm == 0)
    return E_POINTER;

  long& m = *pm;  // out
  m = 0;

  if (n <= 0)
    return S_OK;  // weird

  if (pSamples == 0)
    return E_INVALIDARG;

  for (long i = 0; i < n; ++i) {
    IMediaSample* const pSample = pSamples[i];
    assert(pSample);

    const HRESULT hr = Receive(pSample);

    if (hr != S_OK)
      return hr;

    ++m;
  }

  return S_OK;
}

void Inpin::CopyToPlanar(const vpx_image_t* f, IMediaSample* pOutSample,
                         const GUID& subtype_out,
                         const BITMAPINFOHEADER& bmih_out) {
  // Y

  const BYTE* pInY = f->planes[VPX_PLANE_Y];
  assert(pInY);

  unsigned int width_in = f->d_w;
  unsigned int height_in = f->d_h;

  BYTE* pOutBuf;

  HRESULT hr = pOutSample->GetPointer(&pOutBuf);
  assert(SUCCEEDED(hr));
  assert(pOutBuf);

  BYTE* pOut = pOutBuf;

  const int strideInY = f->stride[VPX_PLANE_Y];

  LONG strideOut = bmih_out.biWidth;
  assert(strideOut);
  assert((strideOut % 2) == 0);  //?

  for (unsigned int y = 0; y < height_in; ++y) {
    memcpy(pOut, pInY, width_in);
    pInY += strideInY;
    pOut += strideOut;
  }

  width_in = (width_in + 1) / 2;
  height_in = (height_in + 1) / 2;

  const BYTE* pInV = f->planes[VPX_PLANE_V];
  assert(pInV);

  const int strideInV = f->stride[VPX_PLANE_V];

  const BYTE* pInU = f->planes[VPX_PLANE_U];
  assert(pInU);

  const int strideInU = f->stride[VPX_PLANE_U];

  if (subtype_out == MEDIASUBTYPE_NV12) {
    // UV

    for (unsigned int y = 0; y < height_in; ++y) {
      const BYTE* u = pInU;
      const BYTE* v = pInV;
      BYTE* uv = pOut;

      for (unsigned int idx = 0; idx < width_in; ++idx) {
        *uv++ = *u++;
        *uv++ = *v++;
      }

      pInU += strideInU;
      pInV += strideInV;
      pOut += strideOut;
    }
  } else if (subtype_out == MEDIASUBTYPE_YV12) {
    strideOut /= 2;

    // V

    for (unsigned int y = 0; y < height_in; ++y) {
      memcpy(pOut, pInV, width_in);
      pInV += strideInV;
      pOut += strideOut;
    }

    // U

    for (unsigned int y = 0; y < height_in; ++y) {
      memcpy(pOut, pInU, width_in);
      pInU += strideInU;
      pOut += strideOut;
    }
  } else {
    assert(subtype_out == WebmTypes::MEDIASUBTYPE_I420);
    strideOut /= 2;

    // U

    for (unsigned int y = 0; y < height_in; ++y) {
      memcpy(pOut, pInU, width_in);
      pInU += strideInU;
      pOut += strideOut;
    }

    // V

    for (unsigned int y = 0; y < height_in; ++y) {
      memcpy(pOut, pInV, width_in);
      pInV += strideInV;
      pOut += strideOut;
    }
  }

  const ptrdiff_t lenOut_ = pOut - pOutBuf;
  const long lenOut = static_cast<long>(lenOut_);

  hr = pOutSample->SetActualDataLength(lenOut);
  assert(SUCCEEDED(hr));
}

void Inpin::CopyToPacked(const vpx_image_t* f, IMediaSample* pOutSample,
                         const GUID& subtype_out, const RECT& rc_out,
                         const BITMAPINFOHEADER& bmih_out) {
  const LONG rect_width_out = rc_out.right - rc_out.left;
  assert(rect_width_out >= 0);

  const LONG width_out =
      (rect_width_out > 0) ? rect_width_out : bmih_out.biWidth;
  assert(width_out > 0);

  const LONG rect_height_out = rc_out.bottom - rc_out.top;
  assert(rect_height_out >= 0);

  const LONG height_out =
      (rect_height_out > 0) ? rect_height_out : labs(bmih_out.biHeight);

  const BYTE* pInY_base = f->planes[VPX_PLANE_Y];
  assert(pInY_base);

  const int strideInY = f->stride[VPX_PLANE_Y];

  const BYTE* pInV_base = f->planes[VPX_PLANE_V];
  assert(pInV_base);

  const int strideInV = f->stride[VPX_PLANE_V];

  const BYTE* pInU_base = f->planes[VPX_PLANE_U];
  assert(pInU_base);

  const int strideInU = f->stride[VPX_PLANE_U];

  const unsigned int width_in = f->d_w;
  assert(LONG(width_in) == width_out);

  const unsigned int height_in = f->d_h;
  assert(LONG(height_in) == height_out);

  BYTE* pOutBuf;

  HRESULT hr = pOutSample->GetPointer(&pOutBuf);
  assert(SUCCEEDED(hr));
  assert(pOutBuf);

  const LONG strideOut_ = 2 * width_in;
  LONG strideOut;

  if (bmih_out.biWidth < strideOut_)
    strideOut = strideOut_;
  else
    strideOut = bmih_out.biWidth;

  const LONG uv_width = width_in / 2;
  const LONG uv_height = height_in / 2;

  int u_off, v_off, y_off;

  if (subtype_out == MEDIASUBTYPE_UYVY) {
    u_off = 0;
    v_off = 2;
    y_off = 1;
  } else if ((subtype_out == MEDIASUBTYPE_YUY2) ||
             (subtype_out == MEDIASUBTYPE_YUYV)) {
    u_off = 1;
    v_off = 3;
    y_off = 0;
  } else {
    assert(subtype_out == MEDIASUBTYPE_YVYU);

    u_off = 3;
    v_off = 1;
    y_off = 0;
  }

  BYTE* pOut = pOutBuf;

  for (LONG hdx = 0; hdx < uv_height; ++hdx) {
    BYTE* const pOut0 = pOut;
    pOut += strideOut;

    BYTE* const pOut1 = pOut;
    pOut += strideOut;

    BYTE* pOutU0 = pOut0 + u_off;
    BYTE* pOutU1 = pOut1 + u_off;

    BYTE* pOutV0 = pOut0 + v_off;
    BYTE* pOutV1 = pOut1 + v_off;

    BYTE* pOutY0 = pOut0 + y_off;
    BYTE* pOutY1 = pOut1 + y_off;

    const BYTE* pInU = pInU_base;
    pInU_base += strideInU;

    const BYTE* pInV = pInV_base;
    pInV_base += strideInV;

    const BYTE* pInY0 = pInY_base;
    pInY_base += strideInY;

    const BYTE* pInY1 = pInY_base;
    pInY_base += strideInY;

    for (LONG wdx = 0; wdx < uv_width; ++wdx) {
      *pOutU0 = *pInU;
      *pOutU1 = *pInU;

      pOutU0 += 4;
      pOutU1 += 4;

      ++pInU;

      *pOutV0 = *pInV;
      *pOutV1 = *pInV;

      pOutV0 += 4;
      pOutV1 += 4;

      ++pInV;

      *pOutY0 = *pInY0;
      *pOutY1 = *pInY1;

      pOutY0 += 2;
      pOutY1 += 2;

      ++pInY0;
      ++pInY1;

      *pOutY0 = *pInY0;
      *pOutY1 = *pInY1;

      pOutY0 += 2;
      pOutY1 += 2;

      ++pInY0;
      ++pInY1;
    }
  }

  const ptrdiff_t lenOut_ = pOut - pOutBuf;
  const long lenOut = static_cast<long>(lenOut_);

  hr = pOutSample->SetActualDataLength(lenOut);
  assert(SUCCEEDED(hr));
}

HRESULT Inpin::ReceiveCanBlock() {
  Filter::Lock lock;

  const HRESULT hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return S_OK;  //?

  if (IMemInputPin* pPin = m_pFilter->m_outpin.m_pInputPin) {
    lock.Release();
    return pPin->ReceiveCanBlock();
  }

  return S_FALSE;
}

HRESULT Inpin::OnDisconnect() {
  return m_pFilter->m_outpin.OnInpinDisconnect();
}

HRESULT Inpin::GetName(PIN_INFO& info) const {
  const wchar_t name[] = L"VP90";

#if _MSC_VER >= 1400
  enum { namelen = sizeof(info.achName) / sizeof(WCHAR) };
  const errno_t e = wcscpy_s(info.achName, namelen, name);
  e;
  assert(e == 0);
#else
  wcscpy(info.achName, name);
#endif

  return S_OK;
}

HRESULT Inpin::Start() {
  m_bEndOfStream = false;
  m_bFlush = false;

  vpx_codec_iface_t& vp9 = vpx_codec_vp9_dx_algo;

  const int flags = 0;

  const vpx_codec_err_t err = vpx_codec_dec_init(&m_ctx, &vp9, 0, flags);

  if (err == VPX_CODEC_MEM_ERROR)
    return E_OUTOFMEMORY;

  if (err != VPX_CODEC_OK)
    return E_FAIL;

  return S_OK;
}

void Inpin::Stop() {
  const vpx_codec_err_t err = vpx_codec_destroy(&m_ctx);
  err;
  assert(err == VPX_CODEC_OK);
}

}  // namespace VP9DecoderLib
