// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "webmsourcepin.h"
#include <comdef.h>
#include "graphutil.h"

namespace MkvParser
{
class Stream;
}

namespace WebmSource
{

class Outpin : public Pin,
               public IMediaSeeking
{
    Outpin(const Outpin&);
    Outpin& operator=(const Outpin&);

protected:
    HRESULT OnDisconnect();
    HRESULT GetName(PIN_INFO&) const;

    GraphUtil::IMemAllocatorPtr m_pAllocator;
    GraphUtil::IMemInputPinPtr m_pInputPin;
    HANDLE m_hThread;

    HRESULT PopulateSamples(mkvparser::Stream::samples_t&);

public:
    Outpin(Filter*, mkvparser::Stream*);
    virtual ~Outpin();

    void Init();
    void Final();

    //IUnknown interface:

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    //IPin interface:

    //HRESULT STDMETHODCALLTYPE EnumMediaTypes(IEnumMediaTypes**);

    HRESULT STDMETHODCALLTYPE Connect(IPin*, const AM_MEDIA_TYPE*);

    //HRESULT STDMETHODCALLTYPE Disconnect();

    HRESULT STDMETHODCALLTYPE ReceiveConnection(
        IPin*,
        const AM_MEDIA_TYPE*);

    HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE*);

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

    //IMediaSeeking

    HRESULT STDMETHODCALLTYPE GetCapabilities(DWORD*);
    HRESULT STDMETHODCALLTYPE CheckCapabilities(DWORD*);
    HRESULT STDMETHODCALLTYPE IsFormatSupported(const GUID*);
    HRESULT STDMETHODCALLTYPE QueryPreferredFormat(GUID*);
    HRESULT STDMETHODCALLTYPE GetTimeFormat(GUID*);
    HRESULT STDMETHODCALLTYPE IsUsingTimeFormat(const GUID*);
    HRESULT STDMETHODCALLTYPE SetTimeFormat(const GUID*);
    HRESULT STDMETHODCALLTYPE GetDuration(LONGLONG*);
    HRESULT STDMETHODCALLTYPE GetStopPosition(LONGLONG*);
    HRESULT STDMETHODCALLTYPE GetCurrentPosition(LONGLONG*);

    HRESULT STDMETHODCALLTYPE ConvertTimeFormat(
        LONGLONG*,
        const GUID*,
        LONGLONG,
        const GUID*);

    HRESULT STDMETHODCALLTYPE SetPositions(
        LONGLONG*,
        DWORD,
        LONGLONG*,
        DWORD);

    HRESULT STDMETHODCALLTYPE GetPositions(
        LONGLONG*,
        LONGLONG*);

    HRESULT STDMETHODCALLTYPE GetAvailable(
        LONGLONG*,
        LONGLONG*);

    HRESULT STDMETHODCALLTYPE SetRate(double);
    HRESULT STDMETHODCALLTYPE GetRate(double*);
    HRESULT STDMETHODCALLTYPE GetPreroll(LONGLONG*);

    mkvparser::Stream* const m_pStream;

private:
    static unsigned __stdcall ThreadProc(void*);
    unsigned Main();

    void StartThread();
    void StopThread();

};

}  //end namespace WebmSource

