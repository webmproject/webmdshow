// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "webmvorbisdecoderpin.hpp"
#include "graphutil.hpp"
#include "vorbis/codec.h"

namespace WebmVorbisDecoderLib
{

class Inpin : public Pin, public IMemInputPin
{
    Inpin(const Inpin&);
    Inpin& operator=(const Inpin&);

public:
    explicit Inpin(Filter*);

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

protected:
    HRESULT GetName(PIN_INFO&) const;
    HRESULT OnDisconnect();

public:
    GraphUtil::IMemAllocatorPtr m_pAllocator;
    HRESULT OnApplyPostProcessing();

private:
    bool m_bEndOfStream;
    bool m_bFlush;

    ogg_packet m_packet;

    vorbis_info m_info;
    vorbis_comment m_comment;
    vorbis_dsp_state m_dsp_state;
    vorbis_block m_block;

    LONGLONG m_first_reftime;
    LONGLONG m_start_reftime;
    double m_samples;

};


}  //end namespace WebmVorbisDecoderLib
