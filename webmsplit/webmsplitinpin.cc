// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webmsplitfilter.h"
#include "mkvparser.hpp"
#include "mkvparserstreamvideo.h"
#include "mkvparserstreamaudio.h"
#include "webmsplitoutpin.h"
#include <vfwmsgs.h>
#include <uuids.h>
#include <cassert>
//#include "odbgstream.h"
//using std::endl;

namespace WebmSplit
{


Inpin::Inpin(Filter* p) :
    Pin(p, PINDIR_INPUT, L"input")
{
}


HRESULT Inpin::QueryInterface(const IID& iid, void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == __uuidof(IUnknown))
        pUnk = static_cast<IPin*>(this);

    else if (iid == __uuidof(IPin))
        pUnk = static_cast<IPin*>(this);

    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG Inpin::AddRef()
{
    return m_pFilter->AddRef();
}


ULONG Inpin::Release()
{
    return m_pFilter->Release();
}


HRESULT Inpin::Connect(IPin*, const AM_MEDIA_TYPE*)
{
    return E_UNEXPECTED;  //for output pins only
}


HRESULT Inpin::QueryInternalConnections(
    IPin** pa,
    ULONG* pn)
{
    if (pn == 0)
        return E_POINTER;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    const Filter::outpins_t& outpins = m_pFilter->m_outpins;
    const ULONG m = static_cast<ULONG>(outpins.size());

    //odbgstream os;
    //os << "WebmSplit::inpin: QueryInternalConnections; m=" << m << endl;

    ULONG& n = *pn;

    if (n == 0)
    {
        if (pa == 0)  //query for required number
        {
            n = m;
            return S_OK;
        }

        return S_FALSE;  //means "insufficient number of array elements"
    }

    if (n < m)
    {
        n = 0;
        return S_FALSE;  //means "insufficient number of array elements"
    }

    if (pa == 0)
    {
        n = 0;
        return E_POINTER;
    }

    typedef Filter::outpins_t::const_iterator iter_t;

    iter_t i = outpins.begin();
    const iter_t j = outpins.end();

    IPin** k = pa;

    while (i != j)
    {
        Outpin* const p = *i++;
        assert(p);

        IPin*& q = *k++;

        q = p;
        q->AddRef();
    }

    n = m;
    return S_OK;
}


HRESULT Inpin::ReceiveConnection(
    IPin* pin,
    const AM_MEDIA_TYPE* pmt)
{
    if (pin == 0)
        return E_INVALIDARG;

    GraphUtil::IAsyncReaderPtr pReader;

    HRESULT hr = pin->QueryInterface(&pReader);

    if (hr != S_OK)
        return hr;

    Filter::Lock lock;

    hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_pFilter->m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    if (bool(m_pPinConnection))
        return VFW_E_ALREADY_CONNECTED;

    m_connection_mtv.Clear();

    if ((pmt != 0) && (pmt->majortype != GUID_NULL))
    {
        hr = QueryAccept(pmt);

        if (hr != S_OK)
            return VFW_E_TYPE_NOT_ACCEPTED;
    }

    hr = m_connection_mtv.Add(*pmt);

    if (FAILED(hr))
        return hr;

    hr = m_reader.SetSource(pReader);

    if (FAILED(hr))
        return hr;

    m_reader.m_sync_read = true;

#if 1
    hr = m_pFilter->Open();
#else
    for (;;)
    {
        hr = m_pFilter->Open();

        if (SUCCEEDED(hr))
            break;

        if (hr != VFW_E_BUFFER_UNDERFLOW)
            break;

        LONGLONG total, avail;

        const int status = m_reader.Length(&total, &avail);

        if (status < 0)
        {
            hr = VFW_E_RUNTIME_ERROR;
            break;
        }

        if (avail >= total)
            continue;

        hr = m_reader.Wait(*m_pFilter, avail, 1, 5000);

        if (FAILED(hr))
            break;
    }
#endif

    if (FAILED(hr))
    {
        m_reader.SetSource(0);
        return hr;
    }

    m_reader.m_sync_read = false;
    m_pPinConnection = pin;

    return S_OK;
}


HRESULT Inpin::EndOfStream()
{
    return E_UNEXPECTED;
}


HRESULT Inpin::BeginFlush()
{
    return E_UNEXPECTED;
}


HRESULT Inpin::EndFlush()
{
    return E_UNEXPECTED;
}


HRESULT Inpin::NewSegment(
    REFERENCE_TIME,
    REFERENCE_TIME,
    double)
{
    return E_UNEXPECTED;
}


HRESULT Inpin::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    if (pmt == 0)
        return E_INVALIDARG;

    const AM_MEDIA_TYPE& mt = *pmt;

    if (mt.majortype == MEDIATYPE_Stream)
        return S_OK;

    //TODO: accept only MEDIASUBTYPE_MKV?

    return S_FALSE;
}


HRESULT Inpin::OnDisconnect()
{
    const HRESULT hr = m_pFilter->OnDisconnectInpin();
    hr;
    assert(SUCCEEDED(hr));  //TODO

    m_reader.SetSource(0);

    return S_OK;
}


HRESULT Inpin::GetName(PIN_INFO& info) const
{
    const wchar_t name[] = L"WebM";

#if _MSC_VER >= 1400
    enum { namelen = sizeof(info.achName) / sizeof(WCHAR) };
    const errno_t e = wcscpy_s(info.achName, namelen, name);
    e;
    assert(e == 0);
#else
    wcscpy(info.achName, name);
#endif

    return S_OK;
}


}  //end namespace WebmSplit

