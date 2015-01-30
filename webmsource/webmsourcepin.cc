// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
//#include <string>
#include "webmsourcepin.h"
#include "webmsourcefilter.h"
#include <vfwmsgs.h>
#include <cassert>


namespace WebmSource
{

Pin::Pin(
    Filter* pFilter,
    PIN_DIRECTION dir,
    const wchar_t* id)
    : m_pFilter(pFilter),
      m_dir(dir),
      m_id(id),
      m_connection(0)
{
}


Pin::~Pin()
{
    assert(m_connection == 0);
}


HRESULT Pin::EnumMediaTypes(IEnumMediaTypes** pp)
{
    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    return m_preferred_mtv.CreateEnum(this, pp);
}


HRESULT Pin::Disconnect()
{
    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_pFilter->m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    if (m_connection == 0)
        return S_FALSE;

    hr = OnDisconnect();
    assert(SUCCEEDED(hr));

    m_connection->Release();
    m_connection = 0;

    m_connection_mtv.Clear();

    return S_OK;
}


HRESULT Pin::OnDisconnect()
{
    return S_OK;
}


HRESULT Pin::ConnectedTo(IPin** pp)
{
    if (pp == 0)
        return E_POINTER;

    IPin*& p = *pp;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    p = m_connection;

    if (p == 0)
        return VFW_E_NOT_CONNECTED;

    p->AddRef();
    return S_OK;
}


HRESULT Pin::ConnectionMediaType(AM_MEDIA_TYPE* p)
{
    if (p == 0)
        return E_POINTER;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_connection == 0)
        return VFW_E_NOT_CONNECTED;

    assert(m_connection_mtv.Size() == 1);

    return m_connection_mtv.Copy(0, *p);
}


HRESULT Pin::QueryPinInfo(PIN_INFO* p)
{
    if (p == 0)
        return E_POINTER;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    PIN_INFO& i = *p;

    i.pFilter = static_cast<IBaseFilter*>(m_pFilter);
    i.pFilter->AddRef();

    i.dir = m_dir;

#if 1
    hr = GetName(i);
    assert(SUCCEEDED(hr));
#else
    const size_t buflen = sizeof(i.achName)/sizeof(WCHAR);

    //TODO: this name should come from track->name()
    const errno_t e = wcscpy_s(i.achName, buflen, m_id.c_str());
    e;
    assert(e == 0);
#endif

    return S_OK;
}


HRESULT Pin::QueryDirection(PIN_DIRECTION* p)
{
    if (p == 0)
        return E_POINTER;

    *p = m_dir;
    return S_OK;
}


HRESULT Pin::QueryId(LPWSTR* p)
{
    if (p == 0)
        return E_POINTER;

    wchar_t*& id = *p;

    const size_t len = m_id.length();            //wchar strlen
    const size_t buflen = len + 1;               //wchar strlen + wchar null
    const size_t cb = buflen * sizeof(wchar_t);  //total bytes

    id = (wchar_t*)CoTaskMemAlloc(cb);

    if (id == 0)
        return E_OUTOFMEMORY;

    const errno_t e = wcscpy_s(id, buflen, m_id.c_str());
    e;
    assert(e == 0);

    return S_OK;
}

} //end namespace WebmSource

