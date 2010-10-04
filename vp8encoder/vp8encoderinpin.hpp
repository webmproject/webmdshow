// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "vp8encoderpin.hpp"
#include "graphutil.hpp"
#include "vpx/vpx_encoder.h"
#include "ivp8sample.hpp"
#include <list>

namespace VP8EncoderLib
{

class Inpin : public Pin, public IMemInputPin
{
    Inpin(const Inpin&);
    Inpin& operator=(const Inpin&);

public:
    explicit Inpin(Filter*);
    virtual ~Inpin();

    //IUnknown interface:

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    //IPin interface:

    HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE*);

    HRESULT STDMETHODCALLTYPE Connect(IPin*, const AM_MEDIA_TYPE*);

    //HRESULT STDMETHODCALLTYPE Disconnect();

    HRESULT STDMETHODCALLTYPE ReceiveConnection(
        IPin*,
        const AM_MEDIA_TYPE*);

    HRESULT STDMETHODCALLTYPE QueryInternalConnections(
        IPin**,
        ULONG*);

    HRESULT STDMETHODCALLTYPE EndOfStream();

    HRESULT STDMETHODCALLTYPE BeginFlush();

    HRESULT STDMETHODCALLTYPE EndFlush();

    HRESULT STDMETHODCALLTYPE NewSegment(
        REFERENCE_TIME,
        REFERENCE_TIME,
        double);

    //IMemInputPin

    HRESULT STDMETHODCALLTYPE GetAllocator(
        IMemAllocator**);

    HRESULT STDMETHODCALLTYPE NotifyAllocator(
        IMemAllocator*,
        BOOL);

    HRESULT STDMETHODCALLTYPE GetAllocatorRequirements(ALLOCATOR_PROPERTIES*);

    HRESULT STDMETHODCALLTYPE Receive(IMediaSample*);

    HRESULT STDMETHODCALLTYPE ReceiveMultiple(
        IMediaSample**,
        long,
        long*);

    HRESULT STDMETHODCALLTYPE ReceiveCanBlock();

    //local functions

    HRESULT Start();  //from stopped to running/paused
    void Stop();      //from running/paused to stopped

    HRESULT OnApplySettings(std::wstring&);

protected:
    //HRESULT GetName(PIN_INFO&) const;
    std::wstring GetName() const;
    HRESULT OnDisconnect();
    void PurgePending();

public:
    GraphUtil::IMemAllocatorPtr m_pAllocator;
    vpx_codec_enc_cfg_t m_cfg;
    __int64 m_start_reftime;  //to implement IMediaSeeking::GetCurrentPos

private:
    bool m_bDiscontinuity;
    bool m_bEndOfStream;
    bool m_bFlush;
    vpx_codec_ctx_t m_ctx;

    typedef std::list<IVP8Sample::Frame> frames_t;
    frames_t m_pending;  //waiting to be pushed downstream

    void AppendFrame(const vpx_codec_cx_pkt_t*);
    void PopulateSample(IMediaSample*);

    void SetConfig();
    vpx_codec_err_t SetTokenPartitions();

    BYTE* m_buf;
    size_t m_buflen;

    BYTE* ConvertYUY2ToYV12(const BYTE*, ULONG, ULONG);

};


}  //end namespace VP8EncoderLib
