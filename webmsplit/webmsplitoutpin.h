// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "webmsplitpin.h"
#include <comdef.h>
#include "graphutil.h"

namespace mkvparser
{
class Stream;
}

namespace WebmSplit
{

class Outpin : public Pin,
               public IMediaSeeking
{
    Outpin(const Outpin&);
    Outpin& operator=(const Outpin&);

protected:
    Outpin(Filter*, mkvparser::Stream*);
    virtual ~Outpin();

    void Final();
    HRESULT OnDisconnect();
    HRESULT GetName(PIN_INFO&) const;

    HRESULT PopulateSamples(mkvparser::Stream::samples_t&);

    mkvparser::Stream* m_pStream;
    GraphUtil::IMemAllocatorPtr m_pAllocator;
    GraphUtil::IMemInputPinPtr m_pInputPin;
    HANDLE m_hThread;
    HANDLE m_hStop;
    HANDLE m_hNewCluster;
    ULONG m_cRef;

public:
    static Outpin* Create(Filter*, mkvparser::Stream*);
    ULONG Destroy();  //when inpin becomes disconnected

    HRESULT Start();  //from stopped to running/paused
    void Stop();      //from running/paused to stopped

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

    mkvparser::Stream* GetStream() const;
    void OnNewCluster();

private:
    static unsigned __stdcall ThreadProc(void*);
    unsigned Main();

    void StartThread();
    void StopThread();

};

}  //end namespace WebmSplit
