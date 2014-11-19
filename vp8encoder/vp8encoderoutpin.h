// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "vp8encoderpin.h"
#include <comdef.h>
#include "graphutil.h"

namespace VP8EncoderLib
{
class Filter;

class Outpin : public Pin
{
    Outpin(const Outpin&);
    Outpin& operator=(const Outpin&);

protected:
    HRESULT OnDisconnect();

    Outpin(Filter*, const wchar_t* id);
    virtual ~Outpin();

public:
    HRESULT Start();  //from stopped to running/paused
    void Stop();      //from running/paused to stopped

    //IPin interface:

    //HRESULT STDMETHODCALLTYPE EnumMediaTypes(IEnumMediaTypes**);

    HRESULT STDMETHODCALLTYPE Connect(IPin*, const AM_MEDIA_TYPE*);

    //HRESULT STDMETHODCALLTYPE Disconnect();

    HRESULT STDMETHODCALLTYPE ReceiveConnection(
        IPin*,
        const AM_MEDIA_TYPE*);

    //HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE*);

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

    //local functions

    GraphUtil::IMemInputPinPtr m_pInputPin;
    GraphUtil::IMemAllocatorPtr m_pAllocator;

    virtual void OnInpinConnect();
    HRESULT OnInpinDisconnect();

    HRESULT SetAvgTimePerFrame(__int64 AvgTimePerFrame);
    virtual void SetDefaultMediaTypes() = 0;

protected:
    virtual HRESULT PostConnect(IPin*) = 0;
    HRESULT InitAllocator(IMemInputPin*, IMemAllocator*);
    virtual void GetSubtype(GUID&) const = 0;

};


}  //end namespace VP8EncoderLib
