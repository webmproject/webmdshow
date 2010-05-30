// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license and patent
// grant that can be found in the LICENSE file in the root of the source
// tree. All contributing project authors may be found in the AUTHORS
// file in the root of the source tree.

#include <strmif.h>
#include <comdef.h>
#include <uuids.h>
#include "vp8encoderfilter.hpp"
#include "vp8encoderoutpin.hpp"
#include <vfwmsgs.h>
#include <amvideo.h>
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
        
    assert(bool(m_pAllocator));
    assert(bool(m_pInputPin));
    
    const HRESULT hr = m_pAllocator->Commit();
    hr;
    assert(SUCCEEDED(hr));  //TODO
    
    return S_OK;
}


void Outpin::Stop()  //transition to stopped
{
    if (m_pPinConnection == 0)
        return;  //nothing was done
        
    assert(bool(m_pAllocator));
    assert(bool(m_pInputPin));
    
    HRESULT hr = m_pAllocator->Decommit();
    assert(SUCCEEDED(hr));
}


HRESULT Outpin::Connect(
    IPin* pin,
    const AM_MEDIA_TYPE* pmt)
{
    if (pin == 0)
        return E_POINTER;
        
    GraphUtil::IMemInputPinPtr pInputPin;

    HRESULT hr = pin->QueryInterface(&pInputPin);
    
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
    
    assert(!bool(m_pAllocator));
    
    hr = GetAllocator(pInputPin);

    if (FAILED(hr))
        return hr;
        
    assert(bool(m_pAllocator));
        
    m_pInputPin = pInputPin;
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


}  //end namespace VP8EncoderLib
