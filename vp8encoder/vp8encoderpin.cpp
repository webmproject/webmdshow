// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "vp8encoderpin.hpp"
#include "vp8encoderfilter.hpp"
#include <vfwmsgs.h>
#include <cassert>
#include <amvideo.h>
#include <dvdmedia.h>
#include <uuids.h>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif

namespace VP8EncoderLib
{

Pin::Pin(
    Filter* pFilter,
    PIN_DIRECTION dir,
    const wchar_t* id) :
    m_pFilter(pFilter),
    m_dir(dir),
    m_id(id)
{
}


Pin::~Pin()
{
    assert(!bool(m_pPinConnection));
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

    if (!bool(m_pPinConnection))
        return S_FALSE;

    hr = OnDisconnect();
    assert(SUCCEEDED(hr));

    m_pPinConnection = 0;
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

    p = m_pPinConnection;

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

    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;

    const CMediaTypes& mtv = m_connection_mtv;
    assert(mtv.Size() == 1);

    return mtv.Copy(0, *p);
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

    //hr = GetName(i);
    //assert(SUCCEEDED(hr));

    const std::wstring name_ = GetName();
    const wchar_t* const name = name_.c_str();

#if _MSC_VER >= 1400
    enum { namelen = sizeof(i.achName) / sizeof(WCHAR) };
    const errno_t e = wcscpy_s(i.achName, namelen, name);
    e;
    assert(e == 0);
#else
    wcscpy(i.achName, name);
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


const BITMAPINFOHEADER& Pin::GetBMIH() const
{
    assert(m_connection_mtv.Size() == 1);

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    assert(mt.pbFormat);

    if (mt.formattype == FORMAT_VideoInfo)
    {
        assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER));

        const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
        const BITMAPINFOHEADER& bmih = vih.bmiHeader;

        return bmih;
    }

    assert(mt.formattype == FORMAT_VideoInfo2);
    assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER2));

    const VIDEOINFOHEADER2& vih = (VIDEOINFOHEADER2&)(*mt.pbFormat);
    const BITMAPINFOHEADER& bmih = vih.bmiHeader;

    return bmih;
}


__int64 Pin::GetAvgTimePerFrame() const
{
    assert(m_connection_mtv.Size() == 1);

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    assert(mt.pbFormat);

    if (mt.formattype == FORMAT_VideoInfo)
    {
        assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER));

        const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
        return vih.AvgTimePerFrame;
    }

    assert(mt.formattype == FORMAT_VideoInfo2);
    assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER2));

    const VIDEOINFOHEADER2& vih = (VIDEOINFOHEADER2&)(*mt.pbFormat);
    return vih.AvgTimePerFrame;
}


}  //end namespace VP8EncoderLib
