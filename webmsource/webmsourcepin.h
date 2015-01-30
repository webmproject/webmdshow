// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "cmediatypes.h"
#include <string>

namespace WebmSource
{

class Filter;

class Pin : public IPin
{
    Pin& operator=(const Pin&);

protected:

    Pin(Filter*, PIN_DIRECTION, const wchar_t*);
    virtual ~Pin();

public:

    Filter* const m_pFilter;
    const PIN_DIRECTION m_dir;
    const std::wstring m_id;
    CMediaTypes m_preferred_mtv;
    CMediaTypes m_connection_mtv;  //only one of these
    IPin* m_connection;

    //IUnknown interface:

    //IPin interface:

    //HRESULT STDMETHODCALLTYPE Connect(
    //    IPin *pReceivePin,
    //    const AM_MEDIA_TYPE *pmt);

    //HRESULT STDMETHODCALLTYPE ReceiveConnection(
    //    IPin*,
    //    const AM_MEDIA_TYPE*);

    HRESULT STDMETHODCALLTYPE Disconnect();

    HRESULT STDMETHODCALLTYPE ConnectedTo(
        IPin **pPin);

    HRESULT STDMETHODCALLTYPE ConnectionMediaType(
        AM_MEDIA_TYPE*);

    HRESULT STDMETHODCALLTYPE QueryPinInfo(
        PIN_INFO*);

    HRESULT STDMETHODCALLTYPE QueryDirection(
        PIN_DIRECTION*);

    HRESULT STDMETHODCALLTYPE QueryId(
        LPWSTR*);

    //HRESULT STDMETHODCALLTYPE QueryAccept(
    //    const AM_MEDIA_TYPE*);

    HRESULT STDMETHODCALLTYPE EnumMediaTypes(
        IEnumMediaTypes**);

    //HRESULT STDMETHODCALLTYPE QueryInternalConnections(
    //    IPin**,
    //    ULONG*);

    //HRESULT STDMETHODCALLTYPE EndOfStream();
    //
    //HRESULT STDMETHODCALLTYPE BeginFlush();
    //
    //HRESULT STDMETHODCALLTYPE EndFlush();
    //
    //HRESULT STDMETHODCALLTYPE NewSegment(
    //    REFERENCE_TIME tStart,
    //    REFERENCE_TIME tStop,
    //    double dRate);

protected:
    virtual HRESULT GetName(PIN_INFO&) const = 0;
    virtual HRESULT OnDisconnect();

};

}  //end namespace WebmSource

