// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "webmmuxpin.hpp"
#include "graphutil.hpp"

namespace WebmMuxLib
{

class Outpin : public Pin
{
    Outpin(const Outpin&);
    Outpin& operator=(const Outpin&);

public:

    explicit Outpin(Filter*);
    ~Outpin();

    void Init();   //transition from Stopped
    void Final();  //transitio to Stopped

    //IUnknown interface:

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    //IPin interface:

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

    //local functions

    HRESULT GetCurrentPosition(LONGLONG&) const;

protected:

    virtual HRESULT OnDisconnect();

private:

    GraphUtil::IMemAllocatorPtr m_pAllocator;
    ALLOCATOR_PROPERTIES m_props;

public:

    IStreamPtr m_pStream;

};

}  //end namespace WebmMuxLib
