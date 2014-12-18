// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBMDSHOW_MEDIAFOUNDATION_WEBMMFVP8DEC_WEBMMFVP8DEC_H_
#define WEBMDSHOW_MEDIAFOUNDATION_WEBMMFVP8DEC_WEBMMFVP8DEC_H_

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>

#include <list>

#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

#include "clockable.h"
#include "webmtypes.h"

namespace WebmMfVp8DecLib {

class WebmMfVp8Dec : public IMFTransform,
                     // public IVP8PostProcessing,  //TODO
                     // public IMFQualityAdvise,
                     public IMFQualityAdvise2,
                     public IMFRateControl,
                     public IMFRateSupport,
                     public IMFGetService,
                     public CLockable {
  friend HRESULT CreateDecoder(IClassFactory*, IUnknown*, const IID&, void**);

  WebmMfVp8Dec(const WebmMfVp8Dec&);
  WebmMfVp8Dec& operator=(const WebmMfVp8Dec&);

 public:
  // IUnknown

  HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  // IMFTransform

  HRESULT STDMETHODCALLTYPE
      GetStreamLimits(DWORD* pdwInputMinimum, DWORD* pdwInputMaximum,
                      DWORD* pdwOutputMinimum, DWORD* pdwOutputMaximum);

  HRESULT STDMETHODCALLTYPE
      GetStreamCount(DWORD* pcInputStreams, DWORD* pcOutputStreams);

  HRESULT STDMETHODCALLTYPE
      GetStreamIDs(DWORD dwInputIDArraySize, DWORD* pdwInputIDs,
                   DWORD dwOutputIDArraySize, DWORD* pdwOutputIDs);

  HRESULT STDMETHODCALLTYPE
      GetInputStreamInfo(DWORD dwInputStreamID,
                         MFT_INPUT_STREAM_INFO* pStreamInfo);

  HRESULT STDMETHODCALLTYPE
      GetOutputStreamInfo(DWORD dwOutputStreamID,
                          MFT_OUTPUT_STREAM_INFO* pStreamInfo);

  HRESULT STDMETHODCALLTYPE GetAttributes(IMFAttributes**);

  HRESULT STDMETHODCALLTYPE
      GetInputStreamAttributes(DWORD dwInputStreamID, IMFAttributes**);

  HRESULT STDMETHODCALLTYPE
      GetOutputStreamAttributes(DWORD dwOutputStreamID, IMFAttributes**);

  HRESULT STDMETHODCALLTYPE DeleteInputStream(DWORD dwStreamID);

  HRESULT STDMETHODCALLTYPE
      AddInputStreams(DWORD cStreams, DWORD* adwStreamIDs);

  HRESULT STDMETHODCALLTYPE GetInputAvailableType(DWORD dwInputStreamID,
                                                  DWORD dwTypeIndex,
                                                  IMFMediaType**);

  HRESULT STDMETHODCALLTYPE GetOutputAvailableType(DWORD dwOutputStreamID,
                                                   DWORD dwTypeIndex,
                                                   IMFMediaType**);

  HRESULT STDMETHODCALLTYPE
      SetInputType(DWORD dwInputStreamID, IMFMediaType*, DWORD dwFlags);

  HRESULT STDMETHODCALLTYPE
      SetOutputType(DWORD dwOutputStreamID, IMFMediaType*, DWORD dwFlags);

  HRESULT STDMETHODCALLTYPE
      GetInputCurrentType(DWORD dwInputStreamID, IMFMediaType**);

  HRESULT STDMETHODCALLTYPE
      GetOutputCurrentType(DWORD dwOutputStreamID, IMFMediaType**);

  HRESULT STDMETHODCALLTYPE
      GetInputStatus(DWORD dwInputStreamID, DWORD* pdwFlags);

  HRESULT STDMETHODCALLTYPE GetOutputStatus(DWORD* pdwFlags);

  HRESULT STDMETHODCALLTYPE
      SetOutputBounds(LONGLONG hnsLowerBound, LONGLONG hnsUpperBound);

  HRESULT STDMETHODCALLTYPE ProcessEvent(DWORD dwInputStreamID, IMFMediaEvent*);

  HRESULT STDMETHODCALLTYPE ProcessMessage(MFT_MESSAGE_TYPE, ULONG_PTR);

  HRESULT STDMETHODCALLTYPE
      ProcessInput(DWORD dwInputStreamID, IMFSample*, DWORD dwFlags);

  HRESULT STDMETHODCALLTYPE
      ProcessOutput(DWORD dwFlags, DWORD cOutputBufferCount,
                    MFT_OUTPUT_DATA_BUFFER* pOutputSamples, DWORD* pdwStatus);

  // IMFQualityAdvise

  HRESULT STDMETHODCALLTYPE SetDropMode(MF_QUALITY_DROP_MODE);

  HRESULT STDMETHODCALLTYPE SetQualityLevel(MF_QUALITY_LEVEL);

  HRESULT STDMETHODCALLTYPE GetDropMode(MF_QUALITY_DROP_MODE*);

  HRESULT STDMETHODCALLTYPE GetQualityLevel(MF_QUALITY_LEVEL*);

  HRESULT STDMETHODCALLTYPE DropTime(LONGLONG);

  // IMFQualityAdvise2

  HRESULT STDMETHODCALLTYPE NotifyQualityEvent(IMFMediaEvent*, DWORD*);

  // IMFRateControl

  HRESULT STDMETHODCALLTYPE SetRate(BOOL, float);

  HRESULT STDMETHODCALLTYPE GetRate(BOOL*, float*);

  // IMFRateSupport

  HRESULT STDMETHODCALLTYPE GetSlowestRate(MFRATE_DIRECTION, BOOL, float*);

  HRESULT STDMETHODCALLTYPE GetFastestRate(MFRATE_DIRECTION, BOOL, float*);

  HRESULT STDMETHODCALLTYPE IsRateSupported(BOOL, float, float*);

  // IMFGetService

  HRESULT STDMETHODCALLTYPE GetService(REFGUID, REFIID, LPVOID*);

 private:
  explicit WebmMfVp8Dec(IClassFactory*);
  virtual ~WebmMfVp8Dec();

  IClassFactory* const m_pClassFactory;
  LONG m_cRef;

  struct FrameSize {
    UINT32 width;
    UINT32 height;
  };

  struct FrameRate {
    UINT32 m_numerator;
    UINT32 m_denominator;

    void Init();
    bool Match(const FrameRate& rhs) const;

    HRESULT AssignFrom(IMFMediaType*);
    HRESULT CopyTo(IMFMediaType*) const;
    HRESULT CopyTo(float&) const;
  };

  // FrameRate m_frame_rate;
  float m_frame_rate;
  IMFMediaType* m_pInputMediaType;
  IMFMediaType* m_pOutputMediaType;

  struct SampleInfo {
    IMFSample* pSample;
    DWORD dwBuffer;

    HRESULT DecodeAll(vpx_codec_ctx_t&);
    HRESULT DecodeOne(vpx_codec_ctx_t&, LONGLONG&, LONGLONG&);
  };

  typedef std::list<SampleInfo> samples_t;
  samples_t m_samples;

  vpx_codec_ctx_t m_ctx;
  vpx_image_t* m_scaled_image;

  float m_rate;  // trick-play mode
  BOOL m_bThin;

  MF_QUALITY_DROP_MODE m_drop_mode;
  int m_drop_budget;
  int m_drop_late;
  void ReplenishDropBudget();

  // struct LagInfo
  //{
  //    LONGLONG lag;
  //};

  LONGLONG m_lag_sum;  // sum of lag values in list
  int m_lag_count;

  // typedef std::list<LagInfo> lag_info_list_t;
  // lag_info_list_t m_lag_info_list;

  // float m_lag_reftime;
  int m_lag_frames;

  DWORD GetOutputBufferSize(FrameSize&) const;
  HRESULT Decode(IMFSample*);
  HRESULT GetFrame(BYTE*, ULONG, const GUID&);

  void Flush();
};

}  // end namespace WebmMfVp8DecLib

#endif  // WEBMDSHOW_MEDIAFOUNDATION_WEBMMFVP8DEC_WEBMMFVP8DEC_H_
