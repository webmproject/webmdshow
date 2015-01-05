// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma warning(disable : 4505)  // unreferenced local function removed

#include "webmmfvp8dec.h"

#include <comdef.h>

#include <cassert>
#include <new>

#include "libyuv_util.h"

#ifdef _DEBUG
#include "odbgstream.h"
#include "iidstr.h"
using std::endl;
using std::boolalpha;
#endif

_COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, __uuidof(IMFMediaBuffer));
_COM_SMARTPTR_TYPEDEF(IMF2DBuffer, __uuidof(IMF2DBuffer));

namespace WebmMfVp8DecLib {

HRESULT CreateDecoder(IClassFactory* pClassFactory, IUnknown* pOuter,
                      const IID& iid, void** ppv) {
  if (ppv == 0)
    return E_POINTER;

  *ppv = 0;

  if (pOuter)
    return CLASS_E_NOAGGREGATION;

  WebmMfVp8Dec* const p = new (std::nothrow) WebmMfVp8Dec(pClassFactory);

  if (p == 0)
    return E_OUTOFMEMORY;

  IMFTransform* const pUnk = p;

  const HRESULT hr = pUnk->QueryInterface(iid, ppv);

  const ULONG cRef = pUnk->Release();
  cRef;

  return hr;
}

WebmMfVp8Dec::WebmMfVp8Dec(IClassFactory* pClassFactory)
    : m_pClassFactory(pClassFactory),
      m_cRef(1),
      m_pInputMediaType(0),
      m_pOutputMediaType(0),
      m_scaled_image(0),
      m_rate(1),
      m_bThin(FALSE),
      m_drop_mode(MF_DROP_MODE_NONE),
      m_drop_budget(0),
      m_lag_sum(0),
      m_lag_count(0),
      m_lag_frames(0),
      m_frame_rate(-1) {
  HRESULT hr = m_pClassFactory->LockServer(TRUE);
  assert(SUCCEEDED(hr));

  hr = CLockable::Init();
  assert(SUCCEEDED(hr));

  // m_frame_rate.Init();
}

WebmMfVp8Dec::~WebmMfVp8Dec() {
  if (m_pInputMediaType) {
    const ULONG n = m_pInputMediaType->Release();
    n;
    assert(n == 0);

    m_pInputMediaType = 0;

    const vpx_codec_err_t e = vpx_codec_destroy(&m_ctx);
    e;
    assert(e == VPX_CODEC_OK);
  }

  vpx_img_free(m_scaled_image);
  m_scaled_image = NULL;

  if (m_pOutputMediaType) {
    const ULONG n = m_pOutputMediaType->Release();
    n;
    assert(n == 0);

    m_pOutputMediaType = 0;
  }

  Flush();

  HRESULT hr = m_pClassFactory->LockServer(FALSE);
  assert(SUCCEEDED(hr));
}

HRESULT WebmMfVp8Dec::QueryInterface(const IID& iid, void** ppv) {
  if (ppv == 0)
    return E_POINTER;

  IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

  if (iid == __uuidof(IUnknown)) {
    pUnk = static_cast<IMFTransform*>(this);  // must be nondelegating
  } else if (iid == __uuidof(IMFTransform)) {
    pUnk = static_cast<IMFTransform*>(this);
  } else if (iid == __uuidof(IMFQualityAdvise2)) {
    pUnk = static_cast<IMFQualityAdvise2*>(this);
  } else if (iid == __uuidof(IMFQualityAdvise)) {
    pUnk = static_cast<IMFQualityAdvise*>(this);
  } else if (iid == __uuidof(IMFRateControl)) {
    pUnk = static_cast<IMFRateControl*>(this);
  } else if (iid == __uuidof(IMFRateSupport)) {
    pUnk = static_cast<IMFRateSupport*>(this);
  } else if (iid == __uuidof(IMFGetService)) {
    pUnk = static_cast<IMFGetService*>(this);
  } else {
    //#ifdef _DEBUG
    //        wodbgstream os;
    //        os << "WebmMfVp8Dec::QI: iid=" << IIDStr(iid) << std::endl;
    //#endif

    pUnk = 0;
    return E_NOINTERFACE;
  }

  pUnk->AddRef();
  return S_OK;
}

ULONG WebmMfVp8Dec::AddRef() { return InterlockedIncrement(&m_cRef); }

ULONG WebmMfVp8Dec::Release() {
  if (LONG n = InterlockedDecrement(&m_cRef))
    return n;

  delete this;
  return 0;
}

HRESULT WebmMfVp8Dec::GetStreamLimits(DWORD* pdwInputMin, DWORD* pdwInputMax,
                                      DWORD* pdwOutputMin,
                                      DWORD* pdwOutputMax) {
  if (pdwInputMin == 0)
    return E_POINTER;

  if (pdwInputMax == 0)
    return E_POINTER;

  if (pdwOutputMin == 0)
    return E_POINTER;

  if (pdwOutputMax == 0)
    return E_POINTER;

  DWORD& dwInputMin = *pdwInputMin;
  dwInputMin = 1;

  DWORD& dwInputMax = *pdwInputMax;
  dwInputMax = 1;

  DWORD& dwOutputMin = *pdwOutputMin;
  dwOutputMin = 1;

  DWORD& dwOutputMax = *pdwOutputMax;
  dwOutputMax = 1;

  return S_OK;
}

HRESULT WebmMfVp8Dec::GetStreamCount(DWORD* pcInputStreams,
                                     DWORD* pcOutputStreams) {
  if (pcInputStreams == 0)
    return E_POINTER;

  if (pcOutputStreams == 0)
    return E_POINTER;

  DWORD& cInputStreams = *pcInputStreams;
  cInputStreams = 1;

  DWORD& cOutputStreams = *pcOutputStreams;
  cOutputStreams = 1;

  return S_OK;
}

HRESULT WebmMfVp8Dec::GetStreamIDs(DWORD, DWORD* pdwInputIDs, DWORD,
                                   DWORD* pdwOutputIDs) {
  if (pdwInputIDs)
    *pdwInputIDs = 0;

  if (pdwOutputIDs)
    *pdwOutputIDs = 0;

  return E_NOTIMPL;
}

HRESULT WebmMfVp8Dec::GetInputStreamInfo(DWORD dwInputStreamID,
                                         MFT_INPUT_STREAM_INFO* pStreamInfo) {
  if (pStreamInfo == 0)
    return E_POINTER;

  if (dwInputStreamID != 0)
    return MF_E_INVALIDSTREAMNUMBER;

  MFT_INPUT_STREAM_INFO& info = *pStreamInfo;

  // LONGLONG hnsMaxLatency;
  // DWORD dwFlags;
  // DWORD cbSize;
  // DWORD cbMaxLookahead;
  // DWORD cbAlignment;

  info.cbMaxLookahead = 0;
  info.hnsMaxLatency = 0;  // TODO: Is 0 correct?
  // TODO: does lag-in-frames matter here?
  // See "_MFT_INPUT_STREAM_INFO_FLAGS Enumeration" for more info:
  // http://msdn.microsoft.com/en-us/library/ms703975%28v=VS.85%29.aspx

  // enum _MFT_INPUT_STREAM_INFO_FLAGS
  // { MFT_INPUT_STREAM_WHOLE_SAMPLES = 0x1,
  // MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER = 0x2,
  // MFT_INPUT_STREAM_FIXED_SAMPLE_SIZE = 0x4,
  // MFT_INPUT_STREAM_HOLDS_BUFFERS    = 0x8,
  // MFT_INPUT_STREAM_DOES_NOT_ADDREF = 0x100,
  // MFT_INPUT_STREAM_REMOVABLE= 0x200,
  // MFT_INPUT_STREAM_OPTIONAL = 0x400,
  // MFT_INPUT_STREAM_PROCESSES_IN_PLACE = 0x800
  // };

  info.dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES;
  // MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER;
  // MFT_INPUT_STREAM_DOES_NOT_ADDREF;

  info.cbSize = 0;  // input size is variable
  info.cbAlignment = 0;  // no specific alignment requirements

  return S_OK;
}

HRESULT WebmMfVp8Dec::GetOutputStreamInfo(DWORD dwOutputStreamID,
                                          MFT_OUTPUT_STREAM_INFO* pStreamInfo) {
  if (pStreamInfo == 0)
    return E_POINTER;

  if (dwOutputStreamID != 0)
    return MF_E_INVALIDSTREAMNUMBER;

  Lock lock;

  const HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  MFT_OUTPUT_STREAM_INFO& info = *pStreamInfo;

  // enum _MFT_OUTPUT_STREAM_INFO_FLAGS
  //  {   MFT_OUTPUT_STREAM_WHOLE_SAMPLES = 0x1,
  // MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER    = 0x2,
  // MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE   = 0x4,
  // MFT_OUTPUT_STREAM_DISCARDABLE = 0x8,
  // MFT_OUTPUT_STREAM_OPTIONAL    = 0x10,
  // MFT_OUTPUT_STREAM_PROVIDES_SAMPLES    = 0x100,
  // MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES = 0x200,
  // MFT_OUTPUT_STREAM_LAZY_READ   = 0x400,
  // MFT_OUTPUT_STREAM_REMOVABLE   = 0x800
  //  };

  // see Decoder sample in the SDK
  // decoder.cc

  // The API says that the only flag that is meaningful prior to SetOutputType
  // is the OPTIONAL flag.  We need the frame dimensions, and the stride,
  // in order to calculte the cbSize value.

  info.dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES |
                 MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
                 MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;

  FrameSize size;

  info.cbSize = GetOutputBufferSize(size);
  info.cbAlignment = 0;

  return S_OK;
}

DWORD WebmMfVp8Dec::GetOutputBufferSize(FrameSize& s) const {
  // MFT was already locked by caller

  // TODO: for now, assume width and height are specified
  // via the input media type.

  if (m_pInputMediaType == 0)
    return 0;

  HRESULT hr = MFGetAttributeSize(m_pInputMediaType, MF_MT_FRAME_SIZE, &s.width,
                                  &s.height);

  assert(SUCCEEDED(hr));

  const DWORD w = s.width;
  assert(w);

  const DWORD h = s.height;
  assert(h);

  const DWORD cb = w * h + 2 * (((w + 1) / 2) * ((h + 1) / 2));

  // TODO: this result does not account for stride

  return cb;
}

HRESULT WebmMfVp8Dec::GetAttributes(IMFAttributes** pp) {
  if (pp)
    *pp = 0;

  return E_NOTIMPL;
}

HRESULT WebmMfVp8Dec::GetInputStreamAttributes(DWORD, IMFAttributes** pp) {
  if (pp)
    *pp = 0;

  return E_NOTIMPL;
}

HRESULT WebmMfVp8Dec::GetOutputStreamAttributes(DWORD, IMFAttributes** pp) {
  if (pp)
    *pp = 0;

  return E_NOTIMPL;
}

HRESULT WebmMfVp8Dec::DeleteInputStream(DWORD) { return E_NOTIMPL; }

HRESULT WebmMfVp8Dec::AddInputStreams(DWORD, DWORD*) { return E_NOTIMPL; }

HRESULT WebmMfVp8Dec::GetInputAvailableType(DWORD dwInputStreamID,
                                            DWORD dwTypeIndex,
                                            IMFMediaType** pp) {
  if (pp)
    *pp = 0;

  if (dwInputStreamID != 0)
    return MF_E_INVALIDSTREAMNUMBER;

  if (dwTypeIndex > 0)
    return MF_E_NO_MORE_TYPES;

  if (pp == 0)
    return E_POINTER;

  IMFMediaType*& pmt = *pp;

  HRESULT hr = MFCreateMediaType(&pmt);
  assert(SUCCEEDED(hr));
  assert(pmt);

  hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  assert(SUCCEEDED(hr));

  hr = pmt->SetGUID(MF_MT_SUBTYPE, WebmTypes::MEDIASUBTYPE_VP80);
  assert(SUCCEEDED(hr));

  hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, FALSE);
  assert(SUCCEEDED(hr));

  hr = pmt->SetUINT32(MF_MT_COMPRESSED, TRUE);
  assert(SUCCEEDED(hr));

  return S_OK;
}

HRESULT WebmMfVp8Dec::GetOutputAvailableType(DWORD dwOutputStreamID,
                                             DWORD dwTypeIndex,
                                             IMFMediaType** pp) {
  if (pp)
    *pp = 0;

  if (dwOutputStreamID != 0)
    return MF_E_INVALIDSTREAMNUMBER;

  enum { subtype_count = 3 };

  if (dwTypeIndex >= subtype_count)
    return MF_E_NO_MORE_TYPES;

  if (pp == 0)
    return E_POINTER;

  IMFMediaType*& pmt = *pp;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  hr = MFCreateMediaType(&pmt);
  assert(SUCCEEDED(hr));
  assert(pmt);

  hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  assert(SUCCEEDED(hr));

  const GUID subtypes[subtype_count] = {MFVideoFormat_NV12, MFVideoFormat_YV12,
                                        MFVideoFormat_IYUV};

  const GUID& subtype = subtypes[dwTypeIndex];

  hr = pmt->SetGUID(MF_MT_SUBTYPE, subtype);
  assert(SUCCEEDED(hr));

  hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
  assert(SUCCEEDED(hr));

  hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
  assert(SUCCEEDED(hr));

  if (m_pInputMediaType == 0)  // nothing else we can say
    return S_OK;

  FrameRate r;

  hr = r.AssignFrom(m_pInputMediaType);

  if (hr == S_OK)  // means attribute was set, and its value is valid
  {
    // odbgstream os;
    // os << "\nvp8dec: num=" << r.m_numerator
    //   << " den=" << r.m_denominator;
    //
    // const float rate = float(r.m_numerator) / float(r.m_denominator);
    // os << rate << '\n' << endl;

    // TODO: a simpler way to handle this is to copy
    // an item directly from media type to media type (assuming
    // the MF API provides such a function), instead of going
    // through this intermediate FrameRate value.

    hr = r.CopyTo(pmt);
    assert(SUCCEEDED(hr));
  }

  FrameSize s;

  hr = MFGetAttributeSize(m_pInputMediaType, MF_MT_FRAME_SIZE, &s.width,
                          &s.height);

  assert(SUCCEEDED(hr));
  assert(s.width);
  assert(s.height);

  hr = MFSetAttributeSize(pmt, MF_MT_FRAME_SIZE, s.width, s.height);

  assert(SUCCEEDED(hr));

  return S_OK;
}

HRESULT WebmMfVp8Dec::SetInputType(DWORD dwInputStreamID, IMFMediaType* pmt,
                                   DWORD dwFlags) {
  if (dwInputStreamID != 0)
    return MF_E_INVALIDSTREAMNUMBER;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  if (pmt == 0) {
    // TODO: disallow this case while we're playing?

    if (m_pInputMediaType) {
      const ULONG n = m_pInputMediaType->Release();
      n;
      assert(n == 0);

      m_pInputMediaType = 0;

      const vpx_codec_err_t e = vpx_codec_destroy(&m_ctx);
      e;
      assert(e == VPX_CODEC_OK);
    }

    if (m_pOutputMediaType) {
      const ULONG n = m_pOutputMediaType->Release();
      n;
      assert(n == 0);

      m_pOutputMediaType = 0;
    }

    return S_OK;
  }

  // TODO: handle the case when already have an input media type
  // or output media type, or are already playing.  I don't
  // think we can change media types while we're playing.

  GUID g;

  hr = pmt->GetMajorType(&g);

  if (FAILED(hr))
    return MF_E_INVALIDMEDIATYPE;

  if (g != MFMediaType_Video)
    return MF_E_INVALIDMEDIATYPE;

  hr = pmt->GetGUID(MF_MT_SUBTYPE, &g);

  if (FAILED(hr))
    return MF_E_INVALIDMEDIATYPE;

  if (g != WebmTypes::MEDIASUBTYPE_VP80)
    return MF_E_INVALIDMEDIATYPE;

  // hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
  // assert(SUCCEEDED(hr));

  FrameRate r;

  hr = r.AssignFrom(pmt);

  if (FAILED(hr))  // input mt has framerate, but it's bad
    return hr;

  r.CopyTo(m_frame_rate);

  // odbgstream os;
  // os << "\nvp8dec: framerate=" << m_frame_rate << '\n' << endl;

  FrameSize s;

  hr = MFGetAttributeSize(pmt, MF_MT_FRAME_SIZE, &s.width, &s.height);

  if (FAILED(hr))
    return MF_E_INVALIDMEDIATYPE;

  if (s.width == 0)
    return MF_E_INVALIDMEDIATYPE;

  if (s.width % 2)  // TODO
    return MF_E_INVALIDMEDIATYPE;

  if (s.height == 0)
    return MF_E_INVALIDMEDIATYPE;

  // TODO: do we need to check for odd height too?

  if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
    return S_OK;

  if (m_pInputMediaType) {
    hr = m_pInputMediaType->DeleteAllItems();
    assert(SUCCEEDED(hr));

    const vpx_codec_err_t e = vpx_codec_destroy(&m_ctx);
    e;
    assert(e == VPX_CODEC_OK);
  } else {
    hr = MFCreateMediaType(&m_pInputMediaType);

    if (FAILED(hr))
      return hr;
  }

  hr = pmt->CopyAllItems(m_pInputMediaType);

  if (FAILED(hr))
    return hr;

  // m_frame_rate = r;

  // TODO: should this really be done here?

  vpx_codec_iface_t& vp8 = vpx_codec_vp8_dx_algo;

  const int flags = 0;  // TODO: VPX_CODEC_USE_POSTPROC;

  const vpx_codec_err_t err = vpx_codec_dec_init(&m_ctx, &vp8, 0, flags);

  if (err == VPX_CODEC_MEM_ERROR)
    return E_OUTOFMEMORY;

  if (err != VPX_CODEC_OK)
    return E_FAIL;

  // const HRESULT hr = OnApplyPostProcessing();

  if (m_pOutputMediaType) {
    // TODO: Is this the correct behavior?

    const ULONG n = m_pOutputMediaType->Release();
    n;
    assert(n == 0);

    m_pOutputMediaType = 0;
  }

#if 0
    //TODO:
    //We could update the preferred ("available") output media types,
    //now that we know the frame rate and frame size, etc.

    hr = MFCreateMediaType(&m_pOutputMediaType);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YV12);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
    assert(SUCCEEDED(hr));  //TODO

    if (r.numerator)  //means "has been set"
    {
        hr = MFSetAttributeRatio(
                m_pOutputMediaType,
                MF_MT_FRAME_RATE,
                r.numerator,
                r.denominator);

        assert(SUCCEEDED(hr));  //TODO
    }

    hr = MFSetAttributeSize(
            m_pOutputMediaType,
            MF_MT_FRAME_SIZE,
            s.width,
            s.height);

    assert(SUCCEEDED(hr));  //TODO
#endif

  return S_OK;
}

HRESULT WebmMfVp8Dec::SetOutputType(DWORD dwOutputStreamID, IMFMediaType* pmt,
                                    DWORD dwFlags) {
  if (dwOutputStreamID != 0)
    return MF_E_INVALIDSTREAMNUMBER;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  if (pmt == 0) {
    // TODO: disallow this case while we're playing?

    if (m_pOutputMediaType) {
      const ULONG n = m_pOutputMediaType->Release();
      n;
      assert(n == 0);

      m_pOutputMediaType = 0;
    }

    return S_OK;
  }

  if (m_pInputMediaType == 0)
    return MF_E_TRANSFORM_TYPE_NOT_SET;

  GUID g;

  hr = pmt->GetMajorType(&g);

  if (FAILED(hr))
    return MF_E_INVALIDMEDIATYPE;

  if (g != MFMediaType_Video)
    return MF_E_INVALIDMEDIATYPE;

  hr = pmt->GetGUID(MF_MT_SUBTYPE, &g);

  if (FAILED(hr))
    return MF_E_INVALIDMEDIATYPE;

  if (g == MFVideoFormat_NV12)
    __noop;
  else if (g == MFVideoFormat_YV12)
    __noop;
  else if (g == MFVideoFormat_IYUV)
    __noop;
  else  // TODO: add I420 support
    return MF_E_INVALIDMEDIATYPE;

  // hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
  // assert(SUCCEEDED(hr));

  // hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
  // assert(SUCCEEDED(hr));

  FrameRate r_out;

  hr = r_out.AssignFrom(pmt);

  if (FAILED(hr))  // output type has value, but it's not valid
    return hr;

  if (hr == S_OK)  // output type has value, and it's valid
  {
    FrameRate r_in;

    hr = r_in.AssignFrom(m_pInputMediaType);

    if ((hr == S_OK) && !r_in.Match(r_out))
      return MF_E_INVALIDMEDIATYPE;
  }

  FrameSize s_out;

  hr = MFGetAttributeSize(pmt, MF_MT_FRAME_SIZE, &s_out.width, &s_out.height);

  if (SUCCEEDED(hr)) {
    if (s_out.width == 0)
      return MF_E_INVALIDMEDIATYPE;

    // TODO: check whether width is odd

    if (s_out.height == 0)
      return MF_E_INVALIDMEDIATYPE;

    FrameSize s_in;

    hr = MFGetAttributeSize(m_pInputMediaType, MF_MT_FRAME_SIZE, &s_in.width,
                            &s_in.height);

    assert(SUCCEEDED(hr));

    if (s_out.width != s_in.width)
      return MF_E_INVALIDMEDIATYPE;

    if (s_out.height != s_in.height)
      return MF_E_INVALIDMEDIATYPE;
  }

  if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
    return S_OK;

  if (m_pOutputMediaType) {
    hr = m_pOutputMediaType->DeleteAllItems();
    assert(SUCCEEDED(hr));
  } else {
    hr = MFCreateMediaType(&m_pOutputMediaType);

    if (FAILED(hr))
      return hr;
  }

  hr = pmt->CopyAllItems(m_pOutputMediaType);

  if (FAILED(hr))
    return hr;

  // TODO: do something

  return S_OK;
}

HRESULT WebmMfVp8Dec::GetInputCurrentType(DWORD dwInputStreamID,
                                          IMFMediaType** pp) {
  if (dwInputStreamID != 0)
    return MF_E_INVALIDSTREAMNUMBER;

  if (pp)
    *pp = 0;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  if (m_pInputMediaType == 0)
    return MF_E_TRANSFORM_TYPE_NOT_SET;

  IMFMediaType*& p = *pp;

  hr = MFCreateMediaType(&p);

  if (FAILED(hr))
    return hr;

  return m_pInputMediaType->CopyAllItems(p);
}

HRESULT WebmMfVp8Dec::GetOutputCurrentType(DWORD dwOutputStreamID,
                                           IMFMediaType** pp) {
  if (dwOutputStreamID != 0)
    return MF_E_INVALIDSTREAMNUMBER;

  if (pp)
    *pp = 0;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  // TODO: synthesize from input media type?

  if (m_pOutputMediaType == 0)  // TODO: liberalize?
    return MF_E_TRANSFORM_TYPE_NOT_SET;

  IMFMediaType*& p = *pp;

  hr = MFCreateMediaType(&p);

  if (FAILED(hr))
    return hr;

  return m_pOutputMediaType->CopyAllItems(p);
}

HRESULT WebmMfVp8Dec::GetInputStatus(DWORD dwInputStreamID, DWORD* pdwFlags) {
  if (dwInputStreamID != 0)
    return MF_E_INVALIDSTREAMNUMBER;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  if (m_pInputMediaType == 0)
    return MF_E_TRANSFORM_TYPE_NOT_SET;

  // TODO: check output media type too?

  if (pdwFlags == 0)
    return E_POINTER;

  DWORD& dwFlags = *pdwFlags;

  dwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;  // because we always queue

  return S_OK;
}

HRESULT WebmMfVp8Dec::GetOutputStatus(DWORD*) {
#if 1
  return E_NOTIMPL;
#else
  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  if (m_pInputMediaType == 0)
    return MF_E_TRANSFORM_TYPE_NOT_SET;

  // TODO: check output media type too?

  if (pdwFlags == 0)
    return E_POINTER;

  DWORD& dwFlags = *pdwFlags;

  const vpx_image_t* const f = vpx_codec_get_frame(&m_ctx, &m_iter);

  dwFlags = f ? MFT_OUTPUT_STATUS_SAMPLE_READY : 0;

  // TODO: alternatively, we could return E_NOTIMPL, which
  // forces client to call ProcessOutput to determine whether
  // a sample is ready.

  return S_OK;
#endif
}

HRESULT WebmMfVp8Dec::SetOutputBounds(LONGLONG /* hnsLowerBound */,
                                      LONGLONG /* hnsUpperBound */) {
  return E_NOTIMPL;  // TODO
}

HRESULT WebmMfVp8Dec::ProcessEvent(DWORD dwInputStreamID,
                                   IMFMediaEvent* /* pEvent */) {
  if (dwInputStreamID != 0)
    return MF_E_INVALIDSTREAMNUMBER;

#if 0  // def _DEBUG
    if (pEvent)
    {
        MediaEventType t;

        HRESULT hr = pEvent->GetType(&t);
        assert(SUCCEEDED(hr));

        odbgstream os;
        os << "WebmMfVp8Dec::ProcessEvent: type=" << t << endl;
    }
#endif

  return E_NOTIMPL;  // TODO
}

HRESULT WebmMfVp8Dec::ProcessMessage(MFT_MESSAGE_TYPE m, ULONG_PTR) {
#if 0  // def _DEBUG
    odbgstream os;
    os << "WebmMfVp8Dec::ProcessMessage(samples.size="
       << m_samples.size() << "): ";
#endif

  switch (m) {
    case MFT_MESSAGE_COMMAND_FLUSH:
#if 0  // def _DEBUG
            os << "COMMAND_FLUSH" << endl;
#endif

      // http://msdn.microsoft.com/en-us/library/dd940419%28v=VS.85%29.aspx

      Flush();
      return S_OK;

    case MFT_MESSAGE_COMMAND_DRAIN:
#if 0  // def _DEBUG
            os << "COMMAND_DRAIN" << endl;
#endif

      // http://msdn.microsoft.com/en-us/library/dd940418%28v=VS.85%29.aspx

      // TODO: input stream does not accept input in the MFT processes all
      // data from previous calls to ProcessInput.

      return S_OK;

    case MFT_MESSAGE_SET_D3D_MANAGER:
#if 0  // def _DEBUG
            os << "SET_D3D" << endl;
#endif

      return S_OK;

    case MFT_MESSAGE_DROP_SAMPLES:
#if 0  // def _DEBUG
            os << "DROP_SAMPLES" << endl;
#endif

      return S_OK;

    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
#if 0  // def _DEBUG
            os << "NOTIFY_BEGIN_STREAMING" << endl;
#endif

      // http://msdn.microsoft.com/en-us/library/dd940421%28v=VS.85%29.aspx

      // TODO: init decoder library here, instead of during SetInputType

      return S_OK;

    case MFT_MESSAGE_NOTIFY_END_STREAMING:
#if 0  // def _DEBUG
            os << "NOTIFY_END_STREAMING" << endl;
#endif

      // http://msdn.microsoft.com/en-us/library/dd940423%28v=VS.85%29.aspx

      // NOTE: flush is not performed here

      return S_OK;

    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
#if 0  // def _DEBUG
            os << "NOTIFY_EOS" << endl;
#endif

      // http://msdn.microsoft.com/en-us/library/dd940422%28v=VS.85%29.aspx

      // TODO: set discontinuity flag on first sample after this

      return S_OK;

    case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
#if 0  // def _DEBUG
            os << "NOTIFY_START_OF_STREAM" << endl;
#endif

      return S_OK;

    case MFT_MESSAGE_COMMAND_MARKER:
#if 0  // def _DEBUG
            os << "COMMAND_MARKER" << endl;
#endif

      return S_OK;

    default:
      return S_OK;
  }
}

HRESULT WebmMfVp8Dec::ProcessInput(DWORD dwInputStreamID, IMFSample* pSample,
                                   DWORD) {
  if (dwInputStreamID != 0)
    return MF_E_INVALIDSTREAMNUMBER;

  if (pSample == 0)
    return E_INVALIDARG;

  DWORD count;

  HRESULT hr = pSample->GetBufferCount(&count);
  assert(SUCCEEDED(hr));

  if (count == 0)  // weird
    return S_OK;

  // if (count > 1)
  //    return E_INVALIDARG;

  // TODO: check duration
  // TODO: check timestamp

  Lock lock;

  hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  if (m_pInputMediaType == 0)
    return MF_E_TRANSFORM_TYPE_NOT_SET;

  if (m_pOutputMediaType == 0)  // TODO: need this check here?
    return MF_E_TRANSFORM_TYPE_NOT_SET;

  pSample->AddRef();

  m_samples.push_back(SampleInfo());

  SampleInfo& i = m_samples.back();

  i.pSample = pSample;
  i.dwBuffer = 0;

  return S_OK;
}

HRESULT WebmMfVp8Dec::ProcessOutput(DWORD dwFlags, DWORD cOutputBufferCount,
                                    MFT_OUTPUT_DATA_BUFFER* pOutputSamples,
                                    DWORD* pdwStatus) {
  if (pdwStatus)
    *pdwStatus = 0;

  if (dwFlags)
    return E_INVALIDARG;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  if (m_pInputMediaType == 0)
    return MF_E_TRANSFORM_TYPE_NOT_SET;

  if (m_pOutputMediaType == 0)
    return MF_E_TRANSFORM_TYPE_NOT_SET;

  if (cOutputBufferCount == 0)
    return E_INVALIDARG;

  if (pOutputSamples == 0)
    return E_INVALIDARG;

  const MFT_OUTPUT_DATA_BUFFER& data = pOutputSamples[0];
  // data.dwStreamID should equal 0, but we ignore it

  for (;;) {
    hr = Decode(data.pSample);

    if (FAILED(hr) || (hr == S_OK))
      return hr;
  }
}

HRESULT WebmMfVp8Dec::Decode(IMFSample* pSample_out) {
  if (pSample_out == 0)
    return E_INVALIDARG;

  if (m_samples.empty())
    return MF_E_TRANSFORM_NEED_MORE_INPUT;

  HRESULT hr;
  LONGLONG time, duration;

  {
    SampleInfo& i = m_samples.front();
    assert(i.pSample);

    UINT32 iKey;

    hr = i.pSample->GetUINT32(MFSampleExtension_CleanPoint, &iKey);

    const bool bKey = SUCCEEDED(hr) && (iKey != FALSE);

    // TODO: this is an optimization oppurtunity, since it would
    // optimize-away the expense of decoding too (not the expense
    // of mere rendering, which is what other modes optimize away).
    // It's tricky to implement since if we
    // transition OUT of DROP_5 mode, then it's possible that we
    // we're in the middle of a GOP and so we cannot decode
    // again until we see the next keyframe.  In other modes, we
    // decode every frame (we are suppressing the rendering, not
    // decode), so a state transition between modes is not an issue.
    // For now, just decompress every frame.
    //
    // if (!bKey && (m_drop_mode == MF_DROP_MODE_5))
    //{
    //    i.pSample->Release();
    //    i.pSample = 0;
    //
    //    m_samples.pop_front();
    //
    //    return S_FALSE;
    //}
    // END TODO.

    UINT32 bPreroll;

    hr = i.pSample->GetUINT32(WebmTypes::WebMSample_Preroll, &bPreroll);

    if (SUCCEEDED(hr) && (bPreroll != FALSE)) {
      // preroll frame
      hr = i.DecodeAll(m_ctx);

      i.pSample->Release();
      i.pSample = 0;

      m_samples.pop_front();

      if (FAILED(hr))  // bad decode
        return hr;

      ReplenishDropBudget();
      return S_FALSE;  // throw away this sample
    }

    hr = i.DecodeOne(m_ctx, time, duration);

    if (FAILED(hr) || (hr != S_OK)) {
      i.pSample->Release();
      i.pSample = 0;

      m_samples.pop_front();

      if (FAILED(hr))  // bad decode
        return hr;
    }

    if (m_drop_mode == MF_DROP_MODE_NONE)
      __noop;  // we're not throwing away frames

    else if (m_drop_mode >= MF_DROP_MODE_5) {
      if (!bKey)
        return S_FALSE;  // throw away this sample
    } else if (bKey || (m_drop_budget > 1))
      --m_drop_budget;  // spend out budget to render this frame

    else {
      ReplenishDropBudget();
      return S_FALSE;  // throw away this sample
    }
  }

  DWORD count_out;

  hr = pSample_out->GetBufferCount(&count_out);

  if (SUCCEEDED(hr) && (count_out != 1))  // TODO: handle this?
    return E_INVALIDARG;

  IMFMediaBufferPtr buf_out;

  hr = pSample_out->GetBufferByIndex(0, &buf_out);

  if (FAILED(hr) || !bool(buf_out))
    return E_INVALIDARG;

  GUID subtype;

  hr = m_pOutputMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
  assert(SUCCEEDED(hr));

  // TODO:
  // MFVideoFormat_I420

  FrameSize frame_size;

  const DWORD cbFrameLen = GetOutputBufferSize(frame_size);
  assert(cbFrameLen > 0);

  // The sequence of querying for the output buffer is described on
  // the page "Uncompressed Video Buffers".
  // http://msdn.microsoft.com/en-us/library/aa473821%28v=VS.85%29.aspx

  IMF2DBuffer* buf2d_out;

  hr = buf_out->QueryInterface(&buf2d_out);

  if (SUCCEEDED(hr)) {
    assert(buf2d_out);

    BYTE* ptr_out;
    LONG stride_out;

    hr = buf2d_out->Lock2D(&ptr_out, &stride_out);
    assert(SUCCEEDED(hr));
    assert(ptr_out);
    assert(stride_out > 0);  // top-down DIBs are positive, right?

    const HRESULT hrGetFrame = GetFrame(ptr_out, stride_out, subtype);

    // TODO: set output buffer length?

    hr = buf2d_out->Unlock2D();
    assert(SUCCEEDED(hr));

    buf2d_out->Release();
    buf2d_out = 0;

    if (FAILED(hrGetFrame) || (hrGetFrame != S_OK))
      return hrGetFrame;
  } else {
    BYTE* ptr_out;
    DWORD cbMaxLen;

    hr = buf_out->Lock(&ptr_out, &cbMaxLen, 0);
    assert(SUCCEEDED(hr));
    assert(ptr_out);
    assert(cbMaxLen >= cbFrameLen);

    // TODO: verify stride of output buffer
    // The page "Uncompressed Video Buffers" here:
    // http://msdn.microsoft.com/en-us/library/aa473821%28v=VS.85%29.aspx
    // explains how to calculate the "minimum stride":
    //  MF_MT_DEFAULT_STRIDE
    //  or, MFGetStrideForBitmapInfoHeader
    //  or, calculate it yourself

    INT32 stride_out;

    hr = pSample_out->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&stride_out);

    if (SUCCEEDED(hr) && (stride_out != 0))
      assert(stride_out > 0);
    else {
      const DWORD fcc = subtype.Data1;

      const DWORD w = frame_size.width;
      LONG stride_out_;

      hr = MFGetStrideForBitmapInfoHeader(fcc, w, &stride_out_);

      if (SUCCEEDED(hr) && (stride_out_ != 0)) {
        assert(stride_out_ > 0);
        stride_out = stride_out_;
      } else {
        assert((w % 2) == 0);  // TODO
        stride_out = w;  // TODO: is this correct???
      }
    }

    const HRESULT hrGetFrame = GetFrame(ptr_out, stride_out, subtype);

    hr = buf_out->Unlock();
    assert(SUCCEEDED(hr));

    if (FAILED(hrGetFrame) || (hrGetFrame != S_OK))
      return hrGetFrame;

    hr = buf_out->SetCurrentLength(cbFrameLen);
    assert(SUCCEEDED(hr));
  }

  if (time >= 0) {
    hr = pSample_out->SetSampleTime(time);
    assert(SUCCEEDED(hr));
  }

  if (duration >= 0) {
    hr = pSample_out->SetSampleDuration(duration);
    assert(SUCCEEDED(hr));
  }

  return S_OK;  // done
}

HRESULT WebmMfVp8Dec::GetFrame(BYTE* pOutBuf, ULONG strideOut,
                               const GUID& subtype) {
  assert(pOutBuf);
  assert(strideOut);
  assert((strideOut % 2) == 0);  // TODO: resolve this issue

  vpx_codec_iter_t iter = 0;

  const vpx_image_t* f = vpx_codec_get_frame(&m_ctx, &iter);

  if (f == 0)  // alt-ref
    return S_FALSE;  // tell caller to pop this buffer and call me back

  // Scale (if necessary).
  FrameSize size;
  GetOutputBufferSize(size);
  if (f->d_h != size.height || f->d_w != size.width) {
    if (!webmdshow::LibyuvScaleI420(size.width, size.height,
                                    f, &m_scaled_image)) {
      assert(false && "webmdshow::LibyuvScaleI420 failed");
      return E_FAIL;
    }
    f = m_scaled_image;
  }

  // Y

  const BYTE* pInY = f->planes[VPX_PLANE_Y];
  assert(pInY);

  unsigned int wIn = f->d_w;
  unsigned int hIn = f->d_h;

  BYTE* pOut = pOutBuf;

  const int strideInY = f->stride[VPX_PLANE_Y];

  for (unsigned int y = 0; y < hIn; ++y) {
    memcpy(pOut, pInY, wIn);
    pInY += strideInY;
    pOut += strideOut;
  }

  const BYTE* pInV = f->planes[VPX_PLANE_V];
  assert(pInV);

  const int strideInV = f->stride[VPX_PLANE_V];

  const BYTE* pInU = f->planes[VPX_PLANE_U];
  assert(pInU);

  const int strideInU = f->stride[VPX_PLANE_U];

  wIn = (wIn + 1) / 2;
  hIn = (hIn + 1) / 2;

  if (subtype == MFVideoFormat_NV12) {
    for (unsigned int y = 0; y < hIn; ++y) {
      const BYTE* u = pInU;  // src
      const BYTE* v = pInV;  // src
      BYTE* uv = pOut;  // dst

      for (unsigned int idx = 0; idx < wIn; ++idx)  // uv
      {
        *uv++ = *u++;  // Cb
        *uv++ = *v++;  // Cr
      }

      pInU += strideInU;
      pInV += strideInV;
      pOut += strideOut;
    }
  } else if (subtype == MFVideoFormat_YV12) {
    strideOut /= 2;

    // V

    for (unsigned int y = 0; y < hIn; ++y) {
      memcpy(pOut, pInV, wIn);
      pInV += strideInV;
      pOut += strideOut;
    }

    // U

    for (unsigned int y = 0; y < hIn; ++y) {
      memcpy(pOut, pInU, wIn);
      pInU += strideInU;
      pOut += strideOut;
    }
  } else {
    assert(subtype == MFVideoFormat_IYUV);

    strideOut /= 2;

    // U

    for (unsigned int y = 0; y < hIn; ++y) {
      memcpy(pOut, pInU, wIn);
      pInU += strideInU;
      pOut += strideOut;
    }

    // V

    for (unsigned int y = 0; y < hIn; ++y) {
      memcpy(pOut, pInV, wIn);
      pInV += strideInV;
      pOut += strideOut;
    }
  }

  const vpx_image_t* const f2 = vpx_codec_get_frame(&m_ctx, &iter);
  f2;
  assert(f2 == 0);

  return S_OK;
}

HRESULT WebmMfVp8Dec::SetRate(BOOL bThin, float rate) {
  // odbgstream os;
  // os << "WebmMfVp8Dec::SetRate: bThin="
  //   << boolalpha << (bThin ? true : false)
  //   << " rate="
  //   << rate
  //   << endl;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  // if (m_pEvents == 0)
  //    return MF_E_SHUTDOWN;

  // if (bThin)
  //    return MF_E_THINNING_UNSUPPORTED;  //TODO

  if (rate < 0)
    return MF_E_REVERSE_UNSUPPORTED;  // TODO

  m_rate = rate;
  m_bThin = bThin;

  // reset drop mode when rate change

  m_lag_sum = 0;
  m_lag_count = 0;
  m_lag_frames = 0;

  hr = WebmMfVp8Dec::SetDropMode(MF_DROP_MODE_NONE);
  assert(SUCCEEDED(hr));

  return S_OK;
}

HRESULT WebmMfVp8Dec::GetRate(BOOL* pbThin, float* pRate) {
  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  // if (m_pEvents == 0)
  //    return MF_E_SHUTDOWN;

  if (pbThin)
    *pbThin = m_bThin;

  if (pRate)  // return error when pRate ptr is NULL?
    *pRate = m_rate;

  // odbgstream os;
  // os << "WebmMfVp8Dec::GetRate: bThin="
  //   << boolalpha << m_bThin
  //   << " rate="
  //   << m_rate
  //   << endl;

  return S_OK;
}

HRESULT WebmMfVp8Dec::GetSlowestRate(MFRATE_DIRECTION d, BOOL /* bThin */,
                                     float* pRate) {
  // odbgstream os;
  // os << "WebmMfVp8Dec::GetSlowestRate" << endl;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  // if (m_pEvents == 0)
  //    return MF_E_SHUTDOWN;

  if (d == MFRATE_REVERSE)
    return MF_E_REVERSE_UNSUPPORTED;  // TODO

  // if (bThin)
  //    return MF_E_THINNING_UNSUPPORTED;  //TODO

  if (pRate == 0)
    return E_POINTER;

  float& r = *pRate;
  r = 0;  //?

  return S_OK;
}

HRESULT WebmMfVp8Dec::GetFastestRate(MFRATE_DIRECTION d, BOOL /* bThin */,
                                     float* pRate) {
  // odbgstream os;
  // os << "WebmMfSource::GetFastestRate" << endl;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  // if (m_pEvents == 0)
  //    return MF_E_SHUTDOWN;

  if (d == MFRATE_REVERSE)
    return MF_E_REVERSE_UNSUPPORTED;  // TODO

  // if (bThin)
  //    return MF_E_THINNING_UNSUPPORTED;  //TODO

  if (pRate == 0)
    return E_POINTER;

  float& r = *pRate;
  r = 64;  //?

  return S_OK;
}

HRESULT WebmMfVp8Dec::IsRateSupported(BOOL /* bThin */, float rate,
                                      float* pNearestRate) {
  // odbgstream os;
  // os << "WebmMfVp8Dec::IsRateSupported: rate=" << rate << endl;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  // if (m_pEvents == 0)
  //    return MF_E_SHUTDOWN;

  // if (bThin)
  //    return MF_E_THINNING_UNSUPPORTED;  //TODO

  if (rate < 0)
    return MF_E_REVERSE_UNSUPPORTED;  // TODO

  // float int_part;
  // const float frac_part = modf(rate, &int_part);

  if (pNearestRate)
    *pNearestRate = rate;

  return S_OK;  // TODO
}

HRESULT WebmMfVp8Dec::GetService(REFGUID sid, REFIID iid, LPVOID* ppv) {
  if (sid == MF_RATE_CONTROL_SERVICE)
    return WebmMfVp8Dec::QueryInterface(iid, ppv);

  if (ppv)
    *ppv = 0;

  return MF_E_UNSUPPORTED_SERVICE;
}

void WebmMfVp8Dec::Flush() {
  while (!m_samples.empty()) {
    SampleInfo& i = m_samples.front();
    assert(i.pSample);

    i.pSample->Release();
    i.pSample = 0;

    m_samples.pop_front();
  }
}

HRESULT WebmMfVp8Dec::SampleInfo::DecodeAll(vpx_codec_ctx_t& ctx) {
  assert(pSample);

  DWORD count;

  HRESULT hr = pSample->GetBufferCount(&count);
  assert(SUCCEEDED(hr));
  assert(count > 0);

  while (dwBuffer < count) {
    IMFMediaBufferPtr buf;

    hr = pSample->GetBufferByIndex(dwBuffer, &buf);
    assert(SUCCEEDED(hr));
    assert(buf);

    BYTE* ptr;
    DWORD len;

    hr = buf->Lock(&ptr, 0, &len);
    assert(SUCCEEDED(hr));
    assert(ptr);
    assert(len);

    const vpx_codec_err_t e = vpx_codec_decode(&ctx, ptr, len, 0, 0);

    hr = buf->Unlock();
    assert(SUCCEEDED(hr));

    if (e != VPX_CODEC_OK)
      return MF_E_INVALID_STREAM_DATA;

    ++dwBuffer;  // consume this buffer
  }

  return S_OK;
}

HRESULT WebmMfVp8Dec::SampleInfo::DecodeOne(vpx_codec_ctx_t& ctx,
                                            LONGLONG& time,
                                            LONGLONG& duration) {
  assert(pSample);

  DWORD count;

  HRESULT hr = pSample->GetBufferCount(&count);
  assert(SUCCEEDED(hr));
  assert(count > 0);
  assert(count > dwBuffer);

  IMFMediaBufferPtr buf;

  hr = pSample->GetBufferByIndex(dwBuffer, &buf);
  assert(SUCCEEDED(hr));
  assert(buf);

  BYTE* ptr;
  DWORD len;

  hr = buf->Lock(&ptr, 0, &len);
  assert(SUCCEEDED(hr));
  assert(ptr);
  assert(len);

  const vpx_codec_err_t e = vpx_codec_decode(&ctx, ptr, len, 0, 0);

  hr = buf->Unlock();
  assert(SUCCEEDED(hr));

  if (e != VPX_CODEC_OK)
    return MF_E_INVALID_STREAM_DATA;

  LONGLONG t;

  hr = pSample->GetSampleTime(&t);

  if (FAILED(hr) || (t < 0)) {
    time = -1;
    duration = -1;
  } else {
    LONGLONG d;

    hr = pSample->GetSampleDuration(&d);

    if (FAILED(hr) || (d <= 0)) {
      if (dwBuffer == 0)  // first buffer
        time = t;
      else
        time = -1;

      duration = -1;
    } else {
      duration = d / count;
      time = t + dwBuffer * duration;
    }
  }

  ++dwBuffer;  // consume this buffer
  return (dwBuffer >= count) ? S_FALSE : S_OK;
}

HRESULT WebmMfVp8Dec::SetDropMode(MF_QUALITY_DROP_MODE d) {
  if (d > MF_DROP_MODE_5)
    return MF_E_NO_MORE_DROP_MODES;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  m_drop_mode = d;
  m_drop_budget = 0;

  // odbgstream os;
  // os << "SetDropMode=" << m_drop_mode << endl;

  return S_OK;

  // we always keep keyframes ("cleanpoint")
  // 1: drop 1 out of every 2 frames
  // 2: drop 2 out of every 3 frames
  // 3: drop 3 out of every 4 frames
  // 4: drop 4 out of every 5 frames
  // 5: drop all delta frames
  //
  // alternatively:
  // 1: drop 1 out of every 16 = 2^4 = 2^(5-1)
  // 2: drop 1 out of every 8  = 2^3 = 2^(5-2) = 2 out of every 16
  // 3: drop 1 out of every 4  = 2^2 = 2^(5-3) = 4 out of every 16
  // 4: drop 1 out of every 2  = 2^1 = 2^(5-4) = 8 out of every 16
  // 5: drop all delta frames

  // when we detect a keyframe, this resets the counter
  //
  // level 1:
  // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
  //                              x
  //
  // level 2:
  // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
  //              x               x
  //
  // level 3:
  // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
  //      x       x       x       x       x
  //
  // level 4:
  // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
  //  x   x   x   x   x   x   x   x   x   x
  //
  // we could also tie this to framerate, e.g.
  // 1: drop 1 frame per second
  // 2: drop 1 frame per 1/2 sec
  // 3: drop 1 frame per 1/4 sec
  // 4: drop 1 frame per 1/8 sec
  // 5: drop all delta frames
  //
  // Give outselves a budget: the smaller the drop mode,
  // the larger the budget.  Drop the first eligrable
  // frame when our budget drops to 0.
}

void WebmMfVp8Dec::ReplenishDropBudget() {
  if (m_drop_mode == MF_DROP_MODE_NONE) {
    m_drop_budget = 0;  // doesn't really matter
    return;
  }

  if (m_drop_mode >= MF_DROP_MODE_5) {
    m_drop_budget = 0;  // doesn't really matter
    return;
  }

  const int d = m_drop_mode;
  assert(d >= 1);
  assert(d <= 4);

  m_drop_budget = (1 << (5 - d));
}

HRESULT WebmMfVp8Dec::SetQualityLevel(MF_QUALITY_LEVEL q) {
  if (q == MF_QUALITY_NORMAL)
    return S_OK;

  return MF_E_NO_MORE_QUALITY_LEVELS;
}

HRESULT WebmMfVp8Dec::GetDropMode(MF_QUALITY_DROP_MODE* p) {
  if (p == 0)
    return E_POINTER;

  Lock lock;

  HRESULT hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  MF_QUALITY_DROP_MODE& d = *p;
  d = m_drop_mode;

  // odbgstream os;
  // os << "GetDropMode=" << m_drop_mode << endl;

  return S_OK;
}

HRESULT WebmMfVp8Dec::GetQualityLevel(MF_QUALITY_LEVEL* p) {
  if (p == 0)
    return E_POINTER;

  MF_QUALITY_LEVEL& q = *p;
  q = MF_QUALITY_NORMAL;  // no post-processing for us to shut off

  return S_OK;
}

HRESULT WebmMfVp8Dec::DropTime(LONGLONG t) {
  t;
  return MF_E_DROPTIME_NOT_SUPPORTED;
}

HRESULT WebmMfVp8Dec::NotifyQualityEvent(IMFMediaEvent* pe, DWORD* pdw) {
  if (pe == 0)
    return E_INVALIDARG;

  if (pdw == 0)
    return E_POINTER;

  DWORD& dw = *pdw;
  dw = 0;  // TODO: possibly set this to MF_QUALITY_CANNOT_KEEP_UP

  MediaEventType type;

  HRESULT hr = pe->GetType(&type);

  if (FAILED(hr))
    return hr;

  if (type != MEQualityNotify)
    return E_INVALIDARG;

  GUID ext;

  hr = pe->GetExtendedType(&ext);

  if (FAILED(hr))
    return hr;

  if (ext != MF_QUALITY_NOTIFY_SAMPLE_LAG)
    return S_OK;

  PROPVARIANT v;

  hr = pe->GetValue(&v);

  if (FAILED(hr))
    return E_INVALIDARG;

  if (v.vt != VT_I8)
    return E_INVALIDARG;

  const LONGLONG reftime = v.hVal.QuadPart;

  // MEQualityNotify Event
  // http://msdn.microsoft.com/en-us/library/ms694893(VS.85).aspx

  // negative is good - frames are early
  // positive is bad - frames are late
  // value is a lateness - time in 100ns units
  // we're seeing extreme lateness: 2 or 3s

  // if (ext == MF_QUALITY_NOTIFY_PROCESSING_LATENCY)
  //    return S_OK;

  Lock lock;

  hr = lock.Seize(this);

  if (FAILED(hr))
    return hr;

  if (m_frame_rate <= 0)  // no framerate available
    return S_OK;  // TODO: for now, don't bother trying to drop

  if (m_bThin || (m_rate != 1))  // bThin = true implies drop level 5 already
    return S_OK;

  if (reftime <= 0)  // assume we're all caught up, even if value is spurious
  {
    m_lag_sum = 0;
    m_lag_count = 0;
    m_lag_frames = 0;

    hr = WebmMfVp8Dec::SetDropMode(MF_DROP_MODE_NONE);
    assert(SUCCEEDED(hr));

    return S_OK;
  }

  m_lag_sum += reftime;
  ++m_lag_count;

  if (m_lag_count < 16)
    return S_OK;

  const float lag_reftime = float(m_lag_sum) / float(m_lag_count);

  const float frames_ = lag_reftime * m_frame_rate / 10000000.0F;
  const int frames = static_cast<int>(frames_);

  if (frames <= 1)  // treat small values as noise
  {
    m_lag_sum = 0;
    m_lag_count = 0;
    m_lag_frames = 0;

    hr = WebmMfVp8Dec::SetDropMode(MF_DROP_MODE_NONE);
    assert(SUCCEEDED(hr));
  }

  MF_QUALITY_DROP_MODE m;

  hr = WebmMfVp8Dec::GetDropMode(&m);
  assert(SUCCEEDED(hr));

  if ((m_lag_frames > 0) &&  // have previous value
      (frames >= m_lag_frames) &&  // we're not getting better
      (m < MF_DROP_MODE_5))  // TODO: use MF_QUALITY_CANNOT_KEEP_UP?
  {
#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfVp8Dec::NotifyQualityEvent: old drop mode=" << m;
#endif

    m = static_cast<MF_QUALITY_DROP_MODE>(int(m) + 1);

#ifdef _DEBUG
    os << " new drop mode=" << m << endl;
#endif

    hr = WebmMfVp8Dec::SetDropMode(m);
    assert(SUCCEEDED(hr));
  }

  m_lag_sum = 0;
  m_lag_count = 0;
  m_lag_frames = frames;

  return S_OK;

  // Here's a case when there's been a long sequence of frames
  // having positive lag.  That means we're slightly behind, so
  // we should compare how much we're behind now to how much
  // we were behind in the recent past.  The reason we do that
  // is to discover whether we have a trend.
  //
  // How many data points to we use to compute the trend?  We used
  // the most recent 16 points to compute our latest lag value, so
  // we must compare the most recent lag value to previous lag values.
  //
  // If we have a previous lag value, then we can compute a trend, even
  // if we only have two points.
  //
  // If the avg lag computed here is same or (slightly?) larger, then it
  // means we really are behind.  We would choose a drop level.  Our
  // current algorithm even for dropping doesn't work that fast: at
  // drop level 1, we drop only 1 frame for every 16 frames, so once
  // we set a non-zero drop level we do have to wait at least 16 frames
  // before reacting (which is what we do here).
  //
  // We don't necessarily have to compare lag times: we convert this to
  // a frame, so we could compare frames instead.
  //
  // The issue remain: how often do we adjust the drop level?
  //
  // I suppose that when the reported lag time is less than 0, then this
  // will reset everything anyway, so we can set the drop level back to
  // 0 when that happens.
  //
  // We probalby only have to increase the drop level, until we're caught
  // up, and it gets reset back to 0.  The question is how often we adjust
  // the drop level up.
  //
  // 1st time: 1 out of 16
  // 2nd time: 2 out of 16
  // 3rd time: 4 out of 16
  // 4th time: 8 out of 16
  // 5th time: keyframes only
  //
  // if lag has increased since last time, go ahead and increase the drop
  // level.
  //
  // if lag has decreased since last time, the we can probably either decrease
  // the drop level, or leave it as is.

  // These dampened out by themselves, so there's nothing special we needed
  // to do.  We don't want to react right away.  The problem we need to
  // handle
  // is when the lag is positive and large, and stays constant or is
  // increasing.
  // It the number is trending down then the problem solves itself.  The hard
  // cases are when the lag is growing, or staying constant and large.
  //
  // How to define "growing"?  If the slope is positive, that's growing.  If
  // we remain at any positive slope, the lag will definitely grow.
  //
  // We could use a one-shot flag: if the flag is true, throw away the next
  // non-key frame, then clear the flag.
  //
  // Our goal is to pull the slope down to 0 (or negative).  This should drive
  // the lag down, which should drive the slope towards 0.
  //
  // A running average isn't necessarily what we want, although this
  // will give us an estimate of the avg lag.  If the avg is large and
  // constant then we must correct.

  // How to compute avg lag:
  //  (sum of lag times) / (number of lag times)
  // or could weigh curr avg and curr lag
  // e.g.  new avg lag = (15/16) * (old avg lag) + (1/16) * (new lag)
  // if lag <= 0 then reset avg lag

  // 10000000 = 1 sec
  // 5000000 = 1/2 sec
  // 2500000 = 1/4 sec
  // 1250000 = 1/8 sec
  //  625000 = 1/16 sec
  //  312500 = 1/32 sec
  //  156250 = 1/64 sec
  //   78125 = 1/128 sec

  // 1 sec late
  //  if we see 2 frames the set drop mode #5 (keyframe only)
  //
  // 1/2 sec late
  //  if we see 4 frames then set drop mode #4
  //
  // 1/4 sec late
  //  if we see

  // drop mode 1 = 1/16 frames are dropped
  //          2 = 1/8 frames
  //          3 = 1/4 frames
  //          4 = 1/2 frames
  //
  // If we do set the drop mode, then the decoder doesn't
  // react that fast.
  //
  // for drop mode 1, we gain 1/16th sec every 16 frames
  // if we were behind by 1/16th sec, then we'd catch up after 1 cycle
  // if we were behind by 1/8th sec, we'd catch up after 2 cycles, etc
  //
  // for drop mode 2, we gain 1/8th sec every 8 frames
  // if we were behind by 1/8th sec, we'd catch up after 1 cycle
  // if we were behind by 1/4 sec, then we'd catch up after 2 cycles, etc
  //
  // for drop mode 3, we gain 1/4 sec every 4 frames
  // if we were behind by 1/4 sec, we'd catch up after 1 cycle
  // if we were behind by 1/2 sec, then we'd catch up after 2 cycles, etc
  //
  // for drop mode 4, we gain 1/2 sec every 2 frames
  //
  // THAT'S ALL WRONG
  // the drop modes work in terms of frames, not times
  // IQualityAdvise2 works in terms of time, not frames
  //
  // To convert between the two advise interfaces we need the frame rate
}

void WebmMfVp8Dec::FrameRate::Init() {
  m_numerator = 0;
  m_denominator = 0;
}

HRESULT WebmMfVp8Dec::FrameRate::AssignFrom(IMFMediaType* pmt) {
  UINT32& num = m_numerator;
  UINT32& den = m_denominator;

  const HRESULT hr = MFGetAttributeRatio(pmt, MF_MT_FRAME_RATE, &num, &den);

  // TODO: check explicitly for the result that means "attribute not set".
  // MF_E_ATTRIBUTENOTFOUND
  // MF_E_INVALIDTYPE

  if (FAILED(hr))  // we assume here this means value wasn't found
  {
    Init();
    return S_FALSE;
  }

  if (den == 0)
    return MF_E_INVALIDMEDIATYPE;

  if (num == 0)
    return MF_E_INVALIDMEDIATYPE;

  return S_OK;
}

HRESULT WebmMfVp8Dec::FrameRate::CopyTo(IMFMediaType* pmt) const {
  const UINT32 num = m_numerator;
  const UINT32 den = m_denominator;

  return MFSetAttributeRatio(pmt, MF_MT_FRAME_RATE, num, den);
}

HRESULT WebmMfVp8Dec::FrameRate::CopyTo(float& value) const {
  const UINT32 num = m_numerator;
  const UINT32 den = m_denominator;

  if (den == 0)
    value = -1;  // serves as our "undefined framerate" nonce value
  else
    value = float(num) / float(den);

  return S_OK;
}

bool WebmMfVp8Dec::FrameRate::Match(const FrameRate& rhs) const {
  const UINT64 n_in = m_numerator;
  const UINT64 d_in = m_denominator;

  const UINT64 n_out = rhs.m_numerator;
  const UINT64 d_out = rhs.m_denominator;

  // n_in / d_in = n_out / d_out
  // n_in * d_out = n_out * d_in

  return ((n_in * d_out) == (n_out * d_in));
}

}  // end namespace WebmMfVp8DecLib
