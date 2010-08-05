// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include <comdef.h>
#include <uuids.h>
#include "vp8encoderfilter.hpp"
#include "vp8encoderoutpin.hpp"
#include <vfwmsgs.h>
#include <amvideo.h>
#include <dvdmedia.h>
#include <cassert>
#ifdef _DEBUG
#include "odbgstream.hpp"
#include "iidstr.hpp"
using std::endl;
using std::dec;
using std::hex;
#endif


namespace VP8EncoderLib
{

Outpin::Outpin(
    Filter* pFilter,
    const wchar_t* id) :
    Pin(pFilter, PINDIR_OUTPUT, id)
{
}


Outpin::~Outpin()
{
    assert(!bool(m_pAllocator));
    assert(!bool(m_pInputPin));
}


HRESULT Outpin::Start()  //transition from stopped
{
    if (m_pPinConnection == 0)
        return S_FALSE;  //nothing we need to do

    if (bool(m_pAllocator))
    {
        assert(bool(m_pInputPin));

        const HRESULT hr = m_pAllocator->Commit();
        hr;
        assert(SUCCEEDED(hr));  //TODO
    }

    return S_OK;
}


void Outpin::Stop()  //transition to stopped
{
    if (m_pPinConnection == 0)
        return;  //nothing was done

    if (bool(m_pAllocator))
    {
        assert(bool(m_pInputPin));

        HRESULT hr = m_pAllocator->Decommit();
        assert(SUCCEEDED(hr));
    }
}


HRESULT Outpin::Connect(
    IPin* pin,
    const AM_MEDIA_TYPE* pmt)
{
    if (pin == 0)
        return E_POINTER;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    if (m_pFilter->m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    if (bool(m_pPinConnection))
        return VFW_E_ALREADY_CONNECTED;

    if (!bool(m_pFilter->m_inpin.m_pPinConnection))
        return VFW_E_NO_TYPES;  //VFW_E_NOT_CONNECTED?

    m_connection_mtv.Clear();

    if (pmt)
    {
        hr = QueryAccept(pmt);

        if (hr != S_OK)
            return VFW_E_TYPE_NOT_ACCEPTED;

        hr = pin->ReceiveConnection(this, pmt);

        if (FAILED(hr))
            return hr;

        const AM_MEDIA_TYPE& mt = *pmt;

        m_connection_mtv.Add(mt);
    }
    else
    {
        ULONG i = 0;
        const ULONG j = m_preferred_mtv.Size();

        while (i < j)
        {
            const AM_MEDIA_TYPE& mt = m_preferred_mtv[i];

            hr = pin->ReceiveConnection(this, &mt);

            if (SUCCEEDED(hr))
                break;

            ++i;
        }

        if (i >= j)
            return VFW_E_NO_ACCEPTABLE_TYPES;

        const AM_MEDIA_TYPE& mt = m_preferred_mtv[i];

        m_connection_mtv.Add(mt);
    }

    hr = PostConnect(pin);

    if (FAILED(hr))
        return hr;

    m_pPinConnection = pin;

    return S_OK;
}


HRESULT Outpin::OnDisconnect()
{
    m_pInputPin = 0;
    m_pAllocator = 0;

    return S_OK;
}


HRESULT Outpin::ReceiveConnection(
    IPin*,
    const AM_MEDIA_TYPE*)
{
    return E_UNEXPECTED;  //for input pins only
}


HRESULT Outpin::QueryInternalConnections(IPin** pa, ULONG* pn)
{
    if (pn == 0)
        return E_POINTER;

    Filter::Lock lock;

    HRESULT hr = lock.Seize(m_pFilter);

    if (FAILED(hr))
        return hr;

    ULONG& n = *pn;

    if (n == 0)
    {
        if (pa == 0)  //query for required number
        {
            n = 1;
            return S_OK;
        }

        return S_FALSE;  //means "insufficient number of array elements"
    }

    if (n < 1)
    {
        n = 0;
        return S_FALSE;  //means "insufficient number of array elements"
    }

    if (pa == 0)
    {
        n = 0;
        return E_POINTER;
    }

    IPin*& pPin = pa[0];

    pPin = &m_pFilter->m_inpin;
    pPin->AddRef();

    n = 1;
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


void Outpin::OnInpinConnect()
{
    const Inpin& inpin = m_pFilter->m_inpin;
    const BITMAPINFOHEADER& bmihIn = inpin.GetBMIH();

    const LONG ww = bmihIn.biWidth;
    assert(ww > 0);

    const LONG hh = labs(bmihIn.biHeight);
    assert(hh > 0);

    //TODO: does this really need to be a conditional expr?
    const LONG w = (ww % 16) ? 16 * ((ww + 15) / 16) : ww;
    const LONG h = (hh % 16) ? 16 * ((hh + 15) / 16) : hh;
    const LONG cbBuffer = w*h + 2*(w/2)*(h/2);

    m_preferred_mtv.Clear();

    AM_MEDIA_TYPE mt;

    mt.majortype = MEDIATYPE_Video;
    GetSubtype(mt.subtype);  //dispatch to subclass
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = TRUE;
    mt.lSampleSize = 0;
    mt.pUnk = 0;

    {
        VIDEOINFOHEADER vih;
        BITMAPINFOHEADER& bmih = vih.bmiHeader;

        mt.formattype = FORMAT_VideoInfo;
        mt.cbFormat = sizeof vih;
        mt.pbFormat = (BYTE*)&vih;

        SetRectEmpty(&vih.rcSource);  //TODO
        SetRectEmpty(&vih.rcTarget);  //TODO
        vih.dwBitRate = 0;
        vih.dwBitErrorRate = 0;
        vih.AvgTimePerFrame = inpin.GetAvgTimePerFrame();

        bmih.biSize = sizeof bmih;
        bmih.biWidth = ww;
        bmih.biHeight = hh;
        bmih.biPlanes = 1;  //because Microsoft says so
        bmih.biBitCount = 12;  //?
        bmih.biCompression = mt.subtype.Data1;

        bmih.biSizeImage = cbBuffer;
        bmih.biXPelsPerMeter = 0;
        bmih.biYPelsPerMeter = 0;
        bmih.biClrUsed = 0;
        bmih.biClrImportant = 0;

        m_preferred_mtv.Add(mt);
    }

    {
        VIDEOINFOHEADER2 vih;
        BITMAPINFOHEADER& bmih = vih.bmiHeader;

        mt.formattype = FORMAT_VideoInfo2;
        mt.cbFormat = sizeof vih;
        mt.pbFormat = (BYTE*)&vih;

        SetRectEmpty(&vih.rcSource);  //TODO
        SetRectEmpty(&vih.rcTarget);  //TODO
        vih.dwBitRate = 0;
        vih.dwBitErrorRate = 0;
        vih.AvgTimePerFrame = inpin.GetAvgTimePerFrame();
        vih.dwInterlaceFlags = 0;
        vih.dwCopyProtectFlags = 0;
        vih.dwPictAspectRatioX = w;  //TODO
        vih.dwPictAspectRatioY = h;  //TODO
        vih.dwReserved1 = 0;         //TODO
        vih.dwReserved2 = 0;         //TODO

        bmih.biSize = sizeof bmih;
        bmih.biWidth = ww;
        bmih.biHeight = hh;
        bmih.biPlanes = 1;  //because Microsoft says so
        bmih.biBitCount = 12;  //?
        bmih.biCompression = mt.subtype.Data1;

        bmih.biSizeImage = cbBuffer;
        bmih.biXPelsPerMeter = 0;
        bmih.biYPelsPerMeter = 0;
        bmih.biClrUsed = 0;
        bmih.biClrImportant = 0;

        m_preferred_mtv.Add(mt);
    }
}


HRESULT Outpin::OnInpinDisconnect()
{
    if (bool(m_pPinConnection))
    {
        IFilterGraph* const pGraph = m_pFilter->m_info.pGraph;
        assert(pGraph);

        HRESULT hr = pGraph->Disconnect(m_pPinConnection);
        assert(SUCCEEDED(hr));

        hr = pGraph->Disconnect(this);
        assert(SUCCEEDED(hr));

        assert(!bool(m_pPinConnection));
    }

    SetDefaultMediaTypes();

    return S_OK;
}


HRESULT Outpin::InitAllocator(
    IMemInputPin* pInputPin,
    IMemAllocator* pAllocator)
{
    assert(pInputPin);
    assert(pAllocator);

    ALLOCATOR_PROPERTIES props, actual;

    props.cBuffers = -1;    //number of buffers
    props.cbBuffer = -1;    //size of each buffer, excluding prefix
    props.cbAlign = -1;     //applies to prefix, too
    props.cbPrefix = -1;    //imediasample::getbuffer does NOT include prefix

    HRESULT hr = pInputPin->GetAllocatorRequirements(&props);

    if (props.cBuffers <= 0)
        props.cBuffers = 1;

    const BITMAPINFOHEADER& bmih = GetBMIH();

    LONG w = bmih.biWidth;
    assert(w > 0);

    LONG h = labs(bmih.biHeight);
    assert(h > 0);

    if (w % 16)
        w = 16 * ((w + 15) / 16);

    if (h % 16)
        h = 16 * ((h + 15) / 16);

    const long cbBuffer = w*h + 2*(w/2)*(h/2);

    if (props.cbBuffer < cbBuffer)
        props.cbBuffer = cbBuffer;

    if (props.cbAlign <= 0)
        props.cbAlign = 1;

    if (props.cbPrefix < 0)
        props.cbPrefix = 0;

    hr = pAllocator->SetProperties(&props, &actual);

    if (FAILED(hr))
        return hr;

    hr = pInputPin->NotifyAllocator(pAllocator, 0);  //allow writes

    if (FAILED(hr) && (hr != E_NOTIMPL))
        return hr;

    m_pInputPin = pInputPin;
    m_pAllocator = pAllocator;

    return S_OK;  //success
}


}  //end namespace VP8EncoderLib
