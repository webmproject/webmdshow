// Copyright (c) 2013 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "vp9decoderoutpin.h"

#include <amvideo.h>
#include <dvdmedia.h>
#include <strmif.h>
#include <uuids.h>
#include <vfwmsgs.h>

#include <cassert>

#include "cmediasample.h"
#include "mediatypeutil.h"
#include "vp9decoderfilter.h"
#include "webmtypes.h"

#ifdef _DEBUG
#include "odbgstream.h"
#include "iidstr.h"
using std::endl;
using std::dec;
using std::hex;
#endif

using std::wstring;

namespace VP9DecoderLib {

Outpin::Outpin(Filter* pFilter) : Pin(pFilter, PINDIR_OUTPUT, L"output") {
  SetDefaultMediaTypes();
}

Outpin::~Outpin() {
  assert(!bool(m_pAllocator));
  assert(!bool(m_pInputPin));
}

// transition from stopped
HRESULT Outpin::Start() {
  if (m_pPinConnection == 0)
    return S_FALSE;  // nothing we need to do

  assert(bool(m_pAllocator));
  assert(bool(m_pInputPin));

  const HRESULT hr = m_pAllocator->Commit();
  hr;
  assert(SUCCEEDED(hr));  // TODO

  return S_OK;
}

// transition to stopped
void Outpin::Stop() {
  if (m_pPinConnection == 0)
    return;  // nothing was done

  assert(bool(m_pAllocator));
  assert(bool(m_pInputPin));

  const HRESULT hr = m_pAllocator->Decommit();
  hr;
  assert(SUCCEEDED(hr));
}

HRESULT Outpin::QueryInterface(const IID& iid, void** ppv) {
  if (ppv == 0)
    return E_POINTER;

  IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

  if (iid == __uuidof(IUnknown)) {
    pUnk = static_cast<IPin*>(this);
  } else if (iid == __uuidof(IPin)) {
    pUnk = static_cast<IPin*>(this);
  } else if (iid == __uuidof(IMediaSeeking))
    pUnk = static_cast<IMediaSeeking*>(this);
  else {
#if 0
    wodbgstream os;
    os << "vp9dec::outpin::QI: iid=" << IIDStr(iid) << std::endl;
#endif
    pUnk = 0;
    return E_NOINTERFACE;
  }

  pUnk->AddRef();
  return S_OK;
}

ULONG Outpin::AddRef() {
  return m_pFilter->AddRef();
}

ULONG Outpin::Release() {
  return m_pFilter->Release();
}

HRESULT Outpin::Connect(IPin* pin, const AM_MEDIA_TYPE* pmt) {
  if (pin == 0)
    return E_POINTER;

  GraphUtil::IMemInputPinPtr pInputPin;

  HRESULT hr = pin->QueryInterface(&pInputPin);

  if (hr != S_OK)
    return hr;

  Filter::Lock lock;

  hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return hr;

  if (m_pFilter->GetStateLocked() != State_Stopped)
    return VFW_E_NOT_STOPPED;

  if (bool(m_pPinConnection))
    return VFW_E_ALREADY_CONNECTED;

  if (!bool(m_pFilter->m_inpin.m_pPinConnection))
    return VFW_E_NO_TYPES;  // VFW_E_NOT_CONNECTED?

  m_connection_mtv.Clear();

  if (pmt) {
    hr = QueryAccept(pmt);

    if (hr != S_OK)
      return VFW_E_TYPE_NOT_ACCEPTED;

    if (pmt->formattype == FORMAT_VideoInfo ||
        pmt->formattype == FORMAT_VideoInfo2) {
      hr = pin->ReceiveConnection(this, pmt);

      if (FAILED(hr))
        return hr;

      m_connection_mtv.Add(*pmt);
    } else {
      // partial media type
      const ULONG n = m_preferred_mtv.Size();
      LONG idx = -1;

      for (ULONG i = 0; i < n; ++i) {
        const AM_MEDIA_TYPE& mt = m_preferred_mtv[i];

        if (pmt->subtype == mt.subtype) {
          idx = i;
          break;
        }
      }

      if (idx < 0)
        return VFW_E_TYPE_NOT_ACCEPTED;

      const AM_MEDIA_TYPE& mt = m_preferred_mtv[idx];

      hr = pin->ReceiveConnection(this, &mt);

      if (FAILED(hr))
        return hr;

      m_connection_mtv.Add(mt);
    }
  } else {
    ULONG i = 0;
    const ULONG j = m_preferred_mtv.Size();

    while (i < j) {
      const AM_MEDIA_TYPE& mt = m_preferred_mtv[i];

      hr = pin->ReceiveConnection(this, &mt);

      if (SUCCEEDED(hr))
        break;

      ++i;
    }

    if (i >= j)
      return VFW_E_NO_ACCEPTABLE_TYPES;

    const AM_MEDIA_TYPE& mt = m_preferred_mtv[i];

    m_connection_mtv.Add(mt);
  }

  GraphUtil::IMemAllocatorPtr pAllocator;

  hr = pInputPin->GetAllocator(&pAllocator);

  if (FAILED(hr)) {
    hr = CMediaSample::CreateAllocator(&pAllocator);

    if (FAILED(hr))
      return VFW_E_NO_ALLOCATOR;
  }

  assert(bool(pAllocator));

  ALLOCATOR_PROPERTIES props, actual;

  props.cBuffers = -1;  // number of buffers
  props.cbBuffer = -1;  // size of each buffer, excluding prefix
  props.cbAlign = -1;  // applies to prefix, too
  props.cbPrefix = -1;  // imediasample::getbuffer does NOT include prefix

  hr = pInputPin->GetAllocatorRequirements(&props);

  if (props.cBuffers <= 0)
    props.cBuffers = 1;

  LONG w, h;
  GetConnectionDimensions(w, h);

  const long cbBuffer = 2 * w * h;

  if (props.cbBuffer < cbBuffer)
    props.cbBuffer = cbBuffer;

  if (props.cbAlign <= 0)
    props.cbAlign = 1;

  if (props.cbPrefix < 0)
    props.cbPrefix = 0;

  hr = pAllocator->SetProperties(&props, &actual);

  if (FAILED(hr))
    return hr;

  hr = pInputPin->NotifyAllocator(pAllocator, 0);  // allow writes

  if (FAILED(hr) && (hr != E_NOTIMPL))
    return hr;

  m_pPinConnection = pin;
  m_pAllocator = pAllocator;
  m_pInputPin = pInputPin;

  return S_OK;
}

HRESULT Outpin::OnDisconnect() {
  m_pInputPin = 0;
  m_pAllocator = 0;

  return S_OK;
}

HRESULT Outpin::ReceiveConnection(IPin*, const AM_MEDIA_TYPE*) {
  return E_UNEXPECTED;  // for input pins only
}

HRESULT Outpin::QueryAccept(const AM_MEDIA_TYPE* pmt_query) {
  if (pmt_query == 0)
    return E_INVALIDARG;

  const AM_MEDIA_TYPE& mt_query = *pmt_query;

  if (mt_query.majortype != MEDIATYPE_Video)
    return S_FALSE;

  if (mt_query.subtype == MEDIASUBTYPE_NV12)
    __noop;
  else if (mt_query.subtype == MEDIASUBTYPE_YV12)
    __noop;
  else if (mt_query.subtype == WebmTypes::MEDIASUBTYPE_I420)
    __noop;
  else if (mt_query.subtype == MEDIASUBTYPE_UYVY)
    __noop;
  else if (mt_query.subtype == MEDIASUBTYPE_YVYU)
    __noop;
  else if (mt_query.subtype == MEDIASUBTYPE_YUY2)
    __noop;
  else if (mt_query.subtype == MEDIASUBTYPE_YUYV)
    __noop;
  else
    return S_FALSE;

  Filter::Lock lock;

  const HRESULT hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return S_FALSE;

  const Inpin& inpin = m_pFilter->m_inpin;

  if (!bool(inpin.m_pPinConnection))
    return S_FALSE;

  if (mt_query.formattype == FORMAT_None)
    return S_OK;

  if (mt_query.formattype == GUID_NULL)
    return S_OK;

  const AM_MEDIA_TYPE& mt_in = inpin.m_connection_mtv[0];

  if (mt_query.formattype == FORMAT_VideoInfo)
    return QueryAcceptVideoInfo(mt_in, mt_query);

  if (mt_query.formattype == FORMAT_VideoInfo2)
    return QueryAcceptVideoInfo2(mt_in, mt_query);

  return S_OK;
}

HRESULT Outpin::QueryAcceptVideoInfo(const AM_MEDIA_TYPE& mt_in,
                                     const AM_MEDIA_TYPE& mt_out) {
  assert(mt_in.formattype == FORMAT_VideoInfo);
  assert(mt_in.pbFormat);
  assert(mt_in.cbFormat >= sizeof(VIDEOINFOHEADER));
  assert(mt_out.formattype == FORMAT_VideoInfo);

  if (mt_out.pbFormat == 0)
    return S_FALSE;

  if (mt_out.cbFormat < sizeof(VIDEOINFOHEADER))
    return S_FALSE;

  const VIDEOINFOHEADER& vih_in =
      reinterpret_cast<VIDEOINFOHEADER&>(*mt_in.pbFormat);

  const VIDEOINFOHEADER& vih_out =
      reinterpret_cast<VIDEOINFOHEADER&>(*mt_out.pbFormat);

  const BITMAPINFOHEADER& bmih_out = vih_out.bmiHeader;

  if (!VetBMIH(vih_in, mt_out.subtype, vih_out.rcSource, bmih_out))
    return S_FALSE;

  return S_OK;
}

HRESULT Outpin::QueryAcceptVideoInfo2(const AM_MEDIA_TYPE& mt_in,
                                      const AM_MEDIA_TYPE& mt_out) {
  assert(mt_in.formattype == FORMAT_VideoInfo);
  assert(mt_in.pbFormat);
  assert(mt_in.cbFormat >= sizeof(VIDEOINFOHEADER));
  assert(mt_out.formattype == FORMAT_VideoInfo2);

  if (mt_out.pbFormat == 0)
    return S_FALSE;

  if (mt_out.cbFormat < sizeof(VIDEOINFOHEADER2))
    return S_FALSE;

  const VIDEOINFOHEADER& vih_in =
      reinterpret_cast<VIDEOINFOHEADER&>(*mt_in.pbFormat);

  const VIDEOINFOHEADER2& vih2_out =
      reinterpret_cast<VIDEOINFOHEADER2&>(*mt_out.pbFormat);

  const BITMAPINFOHEADER& bmih_out = vih2_out.bmiHeader;

  if (vih2_out.dwInterlaceFlags & AMINTERLACE_UNUSED)
    return S_FALSE;

  if (vih2_out.dwCopyProtectFlags & ~AMCOPYPROTECT_RestrictDuplication)
    return S_FALSE;

  if (vih2_out.dwReserved2)
    return S_FALSE;

  if (!VetBMIH(vih_in, mt_out.subtype, vih2_out.rcSource, bmih_out))
    return S_FALSE;

  return S_OK;
}

bool Outpin::VetBMIH(const VIDEOINFOHEADER& vih_in, const GUID& subtype_out,
                     const RECT& rc_out, const BITMAPINFOHEADER& bmih_out) {
  if (bmih_out.biSize != sizeof(BITMAPINFOHEADER))  // TODO: liberalize
    return false;

  if (bmih_out.biCompression != subtype_out.Data1)
    return false;

  const LONG stride_out = bmih_out.biWidth;

  if (stride_out <= 0)
    return false;

  if (stride_out % 2)
    return false;

  const LONG height_out = labs(bmih_out.biHeight);  // yes, negative OK

  const BITMAPINFOHEADER& bmih_in = vih_in.bmiHeader;

  const LONG width_in = bmih_in.biWidth;
  assert(width_in >= 0);

  const LONG height_in = bmih_in.biHeight;
  assert(height_in >= 0);

  if (stride_out < width_in)
    return false;

  if (height_out != height_in)
    return false;

  const LONG width_out = rc_out.right - rc_out.left;

  if (width_out == 0) {
    if (stride_out != width_in)
      return false;
  } else if (width_out != width_in) {
    return false;
  }

  return true;
}

HRESULT Outpin::QueryInternalConnections(IPin** pa, ULONG* pn) {
  if (pn == 0)
    return E_POINTER;

  Filter::Lock lock;

  HRESULT hr = lock.Seize(m_pFilter);

  if (FAILED(hr))
    return hr;

  ULONG& n = *pn;

  if (n == 0) {
    if (pa == 0) {
      // query for required number
      n = 1;
      return S_OK;
    }

    return S_FALSE;  // means "insufficient number of array elements"
  }

  if (n < 1) {
    n = 0;
    return S_FALSE;  // means "insufficient number of array elements"
  }

  if (pa == 0) {
    n = 0;
    return E_POINTER;
  }

  IPin*& pPin = pa[0];

  pPin = &m_pFilter->m_inpin;
  pPin->AddRef();

  n = 1;
  return S_OK;
}

HRESULT Outpin::EndOfStream() {
  return E_UNEXPECTED;  // for inpins only
}

HRESULT Outpin::NewSegment(REFERENCE_TIME, REFERENCE_TIME, double) {
  return E_UNEXPECTED;
}

HRESULT Outpin::BeginFlush() {
  return E_UNEXPECTED;
}

HRESULT Outpin::EndFlush() {
  return E_UNEXPECTED;
}

HRESULT Outpin::GetCapabilities(DWORD* pdw) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->GetCapabilities(pdw);

  if (pdw == 0)
    return E_POINTER;

  DWORD& dw = *pdw;
  dw = 0;

  return S_OK;  //?
}

HRESULT Outpin::CheckCapabilities(DWORD* pdw) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->CheckCapabilities(pdw);

  if (pdw == 0)
    return E_POINTER;

  DWORD& dw = *pdw;

  const DWORD dwRequested = dw;

  if (dwRequested == 0)
    return E_INVALIDARG;

  return E_FAIL;
}

HRESULT Outpin::IsFormatSupported(const GUID* p) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->IsFormatSupported(p);

  if (p == 0)
    return E_POINTER;

  const GUID& g = *p;

  if (g == TIME_FORMAT_MEDIA_TIME)
    return S_OK;

  return S_FALSE;
}

HRESULT Outpin::QueryPreferredFormat(GUID* p) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->QueryPreferredFormat(p);

  if (p == 0)
    return E_POINTER;

  *p = TIME_FORMAT_MEDIA_TIME;
  return S_OK;
}

HRESULT Outpin::GetTimeFormat(GUID* p) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->GetTimeFormat(p);

  if (p == 0)
    return E_POINTER;

  *p = TIME_FORMAT_MEDIA_TIME;
  return S_OK;
}

HRESULT Outpin::IsUsingTimeFormat(const GUID* p) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->IsUsingTimeFormat(p);

  if (p == 0)
    return E_INVALIDARG;

  return (*p == TIME_FORMAT_MEDIA_TIME) ? S_OK : S_FALSE;
}

HRESULT Outpin::SetTimeFormat(const GUID* p) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->SetTimeFormat(p);

  if (p == 0)
    return E_INVALIDARG;

  if (*p == TIME_FORMAT_MEDIA_TIME)
    return S_OK;

  return E_INVALIDARG;
}

HRESULT Outpin::GetDuration(LONGLONG* p) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->GetDuration(p);

  if (p == 0)
    return E_POINTER;

  return E_FAIL;
}

HRESULT Outpin::GetStopPosition(LONGLONG* p) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->GetStopPosition(p);

  if (p == 0)
    return E_POINTER;

  return E_FAIL;
}

HRESULT Outpin::GetCurrentPosition(LONGLONG* p) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->GetCurrentPosition(p);

  if (p == 0)
    return E_POINTER;

  return E_FAIL;
}

HRESULT Outpin::ConvertTimeFormat(LONGLONG* ptgt, const GUID* ptgtfmt,
                                  LONGLONG src, const GUID* psrcfmt) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->ConvertTimeFormat(ptgt, ptgtfmt, src, psrcfmt);

  if (ptgt == 0)
    return E_POINTER;

  LONGLONG& tgt = *ptgt;

  const GUID& tgtfmt = ptgtfmt ? *ptgtfmt : TIME_FORMAT_MEDIA_TIME;
  const GUID& srcfmt = psrcfmt ? *psrcfmt : TIME_FORMAT_MEDIA_TIME;

  if (tgtfmt != TIME_FORMAT_MEDIA_TIME)
    return E_INVALIDARG;

  if (srcfmt != TIME_FORMAT_MEDIA_TIME)
    return E_INVALIDARG;

  tgt = src;
  return S_OK;
}

HRESULT Outpin::SetPositions(LONGLONG* pCurr, DWORD dwCurr, LONGLONG* pStop,
                             DWORD dwStop) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->SetPositions(pCurr, dwCurr, pStop, dwStop);

  return E_FAIL;
}

HRESULT Outpin::GetPositions(LONGLONG* pCurrPos, LONGLONG* pStopPos) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->GetPositions(pCurrPos, pStopPos);

  return E_FAIL;
}

HRESULT Outpin::GetAvailable(LONGLONG* pEarliest, LONGLONG* pLatest) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->GetAvailable(pEarliest, pLatest);

  return E_FAIL;
}

HRESULT Outpin::SetRate(double r) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->SetRate(r);

  return E_FAIL;
}

HRESULT Outpin::GetRate(double* p) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->GetRate(p);

  return E_FAIL;
}

HRESULT Outpin::GetPreroll(LONGLONG* p) {
  const Inpin& inpin = m_pFilter->m_inpin;
  const GraphUtil::IMediaSeekingPtr pSeek(inpin.m_pPinConnection);

  if (bool(pSeek))
    return pSeek->GetPreroll(p);

  return E_FAIL;
}

HRESULT Outpin::GetName(PIN_INFO& info) const {
  wstring name;

  if (!bool(m_pPinConnection)) {
    name = L"YUV";
  } else {
    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    const char* p = (const char*)&mt.subtype.Data1;
    const char* const q = p + 4;

    while (p != q) {
      const char c = *p++;
      name += wchar_t(c);  //?
    }
  }

  const wchar_t* const name_ = name.c_str();

#if _MSC_VER >= 1400
  enum { namelen = sizeof(info.achName) / sizeof(WCHAR) };
  const errno_t e = wcscpy_s(info.achName, namelen, name_);
  e;
  assert(e == 0);
#else
  wcscpy(info.achName, name_);
#endif

  return S_OK;
}

void Outpin::OnInpinConnect(const AM_MEDIA_TYPE& mtIn) {
  assert(mtIn.cbFormat >= sizeof(VIDEOINFOHEADER));
  assert(mtIn.pbFormat);

  const VIDEOINFOHEADER& vihIn = (VIDEOINFOHEADER&)(*mtIn.pbFormat);
  const BITMAPINFOHEADER& bmihIn = vihIn.bmiHeader;

  m_preferred_mtv.Clear();

  const LONG w = bmihIn.biWidth;
  assert(w > 0);

  const LONG h = bmihIn.biHeight;
  assert(h > 0);

  m_preferred_mtv.Clear();

  // planar

  DWORD dwBitCount = 12;
  DWORD dwSizeImage = w * h + 2 * ((w + 1) / 2 * (h + 1) / 2);

  AddPreferred(MEDIASUBTYPE_NV12, vihIn.AvgTimePerFrame, w, h, dwBitCount,
               dwSizeImage);

  AddPreferred(MEDIASUBTYPE_YV12, vihIn.AvgTimePerFrame, w, h, dwBitCount,
               dwSizeImage);

  AddPreferred(WebmTypes::MEDIASUBTYPE_I420, vihIn.AvgTimePerFrame, w, h,
               dwBitCount, dwSizeImage);

  // packed

  dwBitCount = 16;
  dwSizeImage = 2 * w * h;

  AddPreferred(MEDIASUBTYPE_UYVY, vihIn.AvgTimePerFrame, w, h, dwBitCount,
               dwSizeImage);

  AddPreferred(MEDIASUBTYPE_YUY2, vihIn.AvgTimePerFrame, w, h, dwBitCount,
               dwSizeImage);

  AddPreferred(MEDIASUBTYPE_YUYV, vihIn.AvgTimePerFrame, w, h, dwBitCount,
               dwSizeImage);

  AddPreferred(MEDIASUBTYPE_YVYU, vihIn.AvgTimePerFrame, w, h, dwBitCount,
               dwSizeImage);
}

HRESULT Outpin::OnInpinDisconnect() {
  if (bool(m_pPinConnection)) {
    IFilterGraph* const pGraph = m_pFilter->m_info.pGraph;
    assert(pGraph);

    HRESULT hr = pGraph->Disconnect(m_pPinConnection);
    assert(SUCCEEDED(hr));

    hr = pGraph->Disconnect(this);
    assert(SUCCEEDED(hr));

    assert(!bool(m_pPinConnection));
  }

  SetDefaultMediaTypes();

  return S_OK;
}

void Outpin::SetDefaultMediaTypes() {
  m_preferred_mtv.Clear();
}

void Outpin::GetConnectionDimensions(LONG& w, LONG& h) const {
  assert(!m_connection_mtv.Empty());
  const AM_MEDIA_TYPE& mt = m_connection_mtv[0];

  if (mt.formattype == FORMAT_VideoInfo) {
    assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER));
    assert(mt.pbFormat);

    const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
    const BITMAPINFOHEADER& bmih = vih.bmiHeader;

    w = bmih.biWidth;
    assert(w > 0);

    h = labs(bmih.biHeight);
    assert(h > 0);
  } else {
    assert(mt.formattype == FORMAT_VideoInfo2);
    assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER2));
    assert(mt.pbFormat);

    const VIDEOINFOHEADER2& vih2 = (VIDEOINFOHEADER2&)(*mt.pbFormat);
    const BITMAPINFOHEADER& bmih = vih2.bmiHeader;

    w = bmih.biWidth;
    assert(w > 0);

    h = labs(bmih.biHeight);
    assert(h > 0);
  }
}

void Outpin::AddPreferred(const GUID& subtype, REFERENCE_TIME AvgTimePerFrame,
                          LONG width, LONG height, DWORD dwBitCount,
                          DWORD dwSizeImage) {
  AddVIH2(m_preferred_mtv, subtype, AvgTimePerFrame, width, height, dwBitCount,
          dwSizeImage);

  AddVIH(m_preferred_mtv, subtype, AvgTimePerFrame, width, height, dwBitCount,
         dwSizeImage);
}

void Outpin::AddVIH(CMediaTypes& mtv, const GUID& subtype,
                    REFERENCE_TIME AvgTimePerFrame, LONG width, LONG height,
                    DWORD dwBitCount, DWORD dwSizeImage) {
  AM_MEDIA_TYPE mt;

  VIDEOINFOHEADER vih;
  BITMAPINFOHEADER& bmih = vih.bmiHeader;

  mt.majortype = MEDIATYPE_Video;
  mt.subtype = subtype;
  mt.bFixedSizeSamples = TRUE;
  mt.bTemporalCompression = FALSE;
  mt.lSampleSize = 0;
  mt.formattype = FORMAT_VideoInfo;
  mt.pUnk = 0;
  mt.cbFormat = sizeof vih;
  mt.pbFormat = (BYTE*)&vih;

  SetRectEmpty(&vih.rcSource);
  SetRectEmpty(&vih.rcTarget);
  vih.dwBitRate = 0;
  vih.dwBitErrorRate = 0;
  vih.AvgTimePerFrame = AvgTimePerFrame;

  bmih.biSize = sizeof bmih;
  bmih.biWidth = width;
  bmih.biHeight = height;
  bmih.biPlanes = 1;
  bmih.biBitCount = static_cast<WORD>(dwBitCount);
  bmih.biCompression = subtype.Data1;
  bmih.biSizeImage = dwSizeImage;
  bmih.biXPelsPerMeter = 0;
  bmih.biYPelsPerMeter = 0;
  bmih.biClrUsed = 0;
  bmih.biClrImportant = 0;

  mtv.Add(mt);
}

void Outpin::AddVIH2(CMediaTypes& mtv, const GUID& subtype,
                     REFERENCE_TIME AvgTimePerFrame, LONG width, LONG height,
                     DWORD dwBitCount, DWORD dwSizeImage) {
  AM_MEDIA_TYPE mt;

  VIDEOINFOHEADER2 vih2;
  BITMAPINFOHEADER& bmih = vih2.bmiHeader;

  mt.majortype = MEDIATYPE_Video;
  mt.subtype = subtype;
  mt.bFixedSizeSamples = TRUE;
  mt.bTemporalCompression = FALSE;
  mt.lSampleSize = 0;
  mt.formattype = FORMAT_VideoInfo2;
  mt.pUnk = 0;
  mt.cbFormat = sizeof vih2;
  mt.pbFormat = (BYTE*)&vih2;

  RECT& rc = vih2.rcSource;
  rc.left = 0;
  rc.top = 0;
  rc.right = width;
  rc.bottom = height;

  vih2.rcTarget = rc;

  vih2.dwBitRate = 0;
  vih2.dwBitErrorRate = 0;
  vih2.AvgTimePerFrame = AvgTimePerFrame;
  vih2.dwInterlaceFlags = 0;
  vih2.dwCopyProtectFlags = 0;
  vih2.dwPictAspectRatioX = width;
  vih2.dwPictAspectRatioY = height;
  vih2.dwControlFlags = 0;
  vih2.dwReserved2 = 0;

  bmih.biSize = sizeof bmih;
  bmih.biWidth = width;
  bmih.biHeight = height;
  bmih.biPlanes = 1;
  bmih.biBitCount = static_cast<WORD>(dwBitCount);
  bmih.biCompression = subtype.Data1;
  bmih.biSizeImage = dwSizeImage;
  bmih.biXPelsPerMeter = 0;
  bmih.biYPelsPerMeter = 0;
  bmih.biClrUsed = 0;
  bmih.biClrImportant = 0;

  mtv.Add(mt);
}

}  // namespace VP9DecoderLib
