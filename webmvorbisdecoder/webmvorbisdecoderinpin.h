// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "webmvorbisdecoderpin.h"
#include "graphutil.h"
#include "vorbis/codec.h"
#include <vector>
#include <deque>
#include <list>

namespace WebmVorbisDecoderLib
{

class Inpin : public Pin, public IMemInputPin
{
    Inpin(const Inpin&);
    Inpin& operator=(const Inpin&);

public:
    explicit Inpin(Filter*);
    ~Inpin();

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

    HANDLE m_hSamples;
    int GetSample(IMediaSample**);
    void OnCompletion();

protected:
    HRESULT GetName(PIN_INFO&) const;
    HRESULT OnDisconnect();

private:
    GraphUtil::IMemAllocatorPtr m_pAllocator;

    bool m_bEndOfStream;
    bool m_bFlush;
    bool m_bDone;

    ogg_packet m_packet;

    vorbis_info m_info;
    vorbis_comment m_comment;
    vorbis_dsp_state m_dsp_state;
    vorbis_block m_block;

    LONGLONG m_first_reftime;
    LONGLONG m_start_reftime;
    double m_samples;
    bool m_bDiscontinuity;

    typedef std::deque<float> samples_t;
    typedef std::vector<samples_t> channels_t;
    channels_t m_channels;

    typedef std::list<IMediaSample*> buffers_t;
    buffers_t m_buffers;

    void Decode(IMediaSample*);
    void PopulateSample(IMediaSample*, long, const WAVEFORMATEX&);
    HRESULT PopulateSamples();

};


}  //end namespace WebmVorbisDecoderLib
