// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webmmuxfilter.hpp"
//#include "cmemallocator.hpp"
#include "cmediasample.hpp"
#include "webmtypes.hpp"
#include <vfwmsgs.h>
#include <uuids.h>
#include <dvdmedia.h>
#include <cassert>

namespace WebmMuxLib
{


Outpin::Outpin(Filter* p) :
    Pin(p, L"outpin", PINDIR_OUTPUT)
{
    AM_MEDIA_TYPE mt;

    mt.majortype = MEDIATYPE_Stream;
    mt.subtype = WebmTypes::MEDIASUBTYPE_WEBM;

    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = FALSE;
    mt.lSampleSize = 0;
    mt.formattype = GUID_NULL;
    mt.pUnk = 0;
    mt.cbFormat = 0;
    mt.pbFormat = 0;

    m_preferred_mtv.Add(mt);
}


Outpin::~Outpin()
{
    assert(!bool(m_pStream));
    assert(!bool(m_pAllocator));
}


void Outpin::Init()
{
    if (bool(m_pAllocator))
    {
        const HRESULT hr = m_pAllocator->Commit();
        assert(SUCCEEDED(hr));  //TODO
        hr;
    }

    m_pFilter->m_ctx.Open(m_pStream);
}


void Outpin::Final()
{
    m_pFilter->m_ctx.Close();

    if (bool(m_pAllocator))
    {
        const HRESULT hr = m_pAllocator->Decommit();
        assert(SUCCEEDED(hr));  //TODO
        hr;
    }
}


HRESULT Outpin::QueryInterface(const IID& iid, void** ppv)
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


ULONG Outpin::AddRef()
{
    return m_pFilter->AddRef();
}


ULONG Outpin::Release()
{
    return m_pFilter->Release();
}


HRESULT Outpin::Connect(
    IPin* pin,
    const AM_MEDIA_TYPE* pmt)
{
    if (pin == 0)
        return E_POINTER;

    IStreamPtr pStream;

    HRESULT hr = pin->QueryInterface(&pStream);

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

    assert(!bool(m_pAllocator));
    assert(!bool(m_pStream));

    m_connection_mtv.Clear();

    if (pmt)
    {
        hr = QueryAccept(pmt);

        if (hr != S_OK)
            return VFW_E_TYPE_NOT_ACCEPTED;

        hr = pin->ReceiveConnection(this, pmt);

        if (FAILED(hr))
            return hr;

        hr = m_connection_mtv.Add(*pmt);

        if (FAILED(hr))
            return hr;
    }
    else
    {
        assert(m_preferred_mtv.Size() == 1);
        const AM_MEDIA_TYPE& mt = m_preferred_mtv[0];

        hr = pin->ReceiveConnection(this, &mt);

        if (FAILED(hr))
            return hr;

        hr = m_connection_mtv.Add(mt);

        if (FAILED(hr))
            return hr;
    }

    const GraphUtil::IMemInputPinPtr pMemInput(pin);

    if (bool(pMemInput))
    {
        hr = pMemInput->GetAllocator(&m_pAllocator);

        if (FAILED(hr))
        {
            //hr = CMemAllocator::CreateInstance(&m_pAllocator);
            hr = CMediaSample::CreateAllocator(&m_pAllocator);

            if (FAILED(hr))
                return VFW_E_NO_ALLOCATOR;
        }

        assert(bool(m_pAllocator));

        ALLOCATOR_PROPERTIES props;

        props.cBuffers = -1; //number of buffers
        props.cbBuffer = -1; //size of each buffer, excluding prefix
        props.cbAlign = -1;  //applies to prefix, too
        props.cbPrefix = -1; //imediasample::getbuffer does NOT include prefix

        hr = pMemInput->GetAllocatorRequirements(&props);

        if (props.cBuffers < 0)
            props.cBuffers = 0;

        if (props.cbBuffer < 0)
            props.cbBuffer = 0;

        if (props.cbAlign <= 0)
            props.cbAlign = 1;

        if (props.cbPrefix < 0)
            props.cbPrefix = 0;

        hr = m_pAllocator->SetProperties(&props, &m_props);

        if (FAILED(hr))
            return hr;

        hr = pMemInput->NotifyAllocator(m_pAllocator, 0);  //allow writes

        if (FAILED(hr) && (hr != E_NOTIMPL))
            return hr;
    }

    m_pStream = pStream;
    m_pPinConnection = pin;

    return S_OK;
}


HRESULT Outpin::ReceiveConnection(
    IPin*,
    const AM_MEDIA_TYPE*)
{
    return E_UNEXPECTED;  //for input pins only
}


HRESULT Outpin::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    if (pmt == 0)
        return E_POINTER;

    const AM_MEDIA_TYPE& mt = *pmt;

    if (mt.majortype == MEDIATYPE_Stream)
        return S_OK;

    return S_FALSE;
}


HRESULT Outpin::QueryInternalConnections(IPin** pa, ULONG* pn)
{
    if (pn == 0)
        return E_POINTER;

    if (*pn == 0)
    {
        if (pa == 0)  //query for required number
        {
            *pn = 2;
            return S_OK;
        }

        return S_FALSE;  //means "insufficient number of array elements"
    }

    *pn = 0;

    if (pa == 0)
        return E_POINTER;

    if (*pn < 2)
        return S_FALSE;  //means "insufficient number of array elements"

    IPin*& vpin = pa[0];

    vpin = &m_pFilter->m_inpin_video;
    vpin->AddRef();

    IPin*& apin = pa[1];

    apin = &m_pFilter->m_inpin_audio;
    apin->AddRef();

    *pn = 2;
    return S_OK;
}


HRESULT Outpin::EndOfStream()
{
    return E_UNEXPECTED;  //for inpins only
}


HRESULT Outpin::NewSegment(
    REFERENCE_TIME,
    REFERENCE_TIME,
    double)
{
    return E_UNEXPECTED;
}


HRESULT Outpin::BeginFlush()
{
    return E_UNEXPECTED;
}


HRESULT Outpin::EndFlush()
{
    return E_UNEXPECTED;
}


HRESULT Outpin::OnDisconnect()
{
    m_pStream = 0;
    m_pAllocator = 0;

    return S_OK;
}


HRESULT Outpin::GetCurrentPosition(LONGLONG& reftime) const
{
    if (!bool(m_pStream))
        return VFW_E_NOT_CONNECTED;

    const Context& ctx = m_pFilter->m_ctx;

    const ULONG tc = ctx.GetTimecode();
    const ULONG sc = ctx.GetTimecodeScale();
    const LONGLONG ns = LONGLONG(tc) * ULONG(sc);  //nanoseconds

    reftime = ns / 100;  //reftime units (100ns ticks)

    return S_OK;
}


} //end namespace WebmMuxLib
