// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "cmemallocator.h"
#include <vfwmsgs.h>
#include <cassert>
#include <new>


HRESULT CMemAllocator::CreateInstance(
    ISampleFactory* pFactory,
    IMemAllocator** ppResult)
{
    if (pFactory == 0)
        return E_INVALIDARG;

    if (ppResult == 0)
        return E_POINTER;

    IMemAllocator*& pResult = *ppResult;
    pResult = 0;

    CMemAllocator* const p = new (std::nothrow) CMemAllocator(pFactory);

    if (p == 0)
        return E_OUTOFMEMORY;

    pResult = p;
    return S_OK;
}


CMemAllocator::CMemAllocator(ISampleFactory* pFactory) :
    m_pSampleFactory(pFactory),
    m_cRef(1),
    m_cActive(-1),  //means "properties not set"
    m_bCommitted(false)
{
    m_hCond = CreateEvent(0, 0, 0, 0);
    assert(m_hCond);  //TODO

    const HRESULT hr = CLockable::Init();
    hr;
    assert(SUCCEEDED(hr));  //TODO
}


CMemAllocator::~CMemAllocator()
{
    assert(m_cActive <= 0);
    assert(m_samples.empty());
    assert(!m_bCommitted);

    const BOOL b = CloseHandle(m_hCond);
    b;
    assert(b);

    m_pSampleFactory->Destroy(this);
}


HRESULT CMemAllocator::QueryInterface(const IID& iid, void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == __uuidof(IUnknown))
        pUnk = this;

    else if (iid == __uuidof(IMemAllocator))
        pUnk = static_cast<IMemAllocator*>(this);

    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG CMemAllocator::AddRef()
{
    return InterlockedIncrement((LONG*)&m_cRef);
}


ULONG CMemAllocator::Release()
{
    if (LONG n = InterlockedDecrement((LONG*)&m_cRef))
        return n;

    delete this;
    return 0;
}


ULONG CMemAllocator::GetCount() const
{
    return m_cRef;
}


HRESULT CMemAllocator::SetProperties(
    ALLOCATOR_PROPERTIES* pPreferred,
    ALLOCATOR_PROPERTIES* pActual)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bCommitted)
        return VFW_E_ALREADY_COMMITTED;

    if (m_cActive > 0)
        return VFW_E_BUFFERS_OUTSTANDING;

    if (pPreferred == 0)  //weird
    {
        m_props.cBuffers = 0;
        m_props.cbBuffer = 0;
        m_props.cbAlign = 1;
        m_props.cbPrefix = 0;
    }
    else
    {
        m_props = *pPreferred;

        if (m_props.cBuffers < 0)
            m_props.cBuffers = 0;

        if (m_props.cbBuffer < 0)
            m_props.cbBuffer = 0;

        if (m_props.cbAlign <= 0)
            m_props.cbAlign = 1;

        if (m_props.cbPrefix < 0)
            m_props.cbPrefix = 0;
    }

    if (pActual)
        *pActual = m_props;

    m_cActive = 0;  //means "properties have been set"

    return S_OK;
}


HRESULT CMemAllocator::GetProperties(ALLOCATOR_PROPERTIES* p)
{
    if (p == 0)
        return E_POINTER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_cActive < 0)
        return VFW_E_SIZENOTSET;

    *p = m_props;
    return S_OK;
}


HRESULT CMemAllocator::Commit()
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_bCommitted)
        return S_OK;

    if (m_cActive > 0)
        return VFW_E_BUFFERS_OUTSTANDING;

    if (m_cActive < 0)
        return VFW_E_SIZENOTSET;

    assert(m_samples.empty());

    for (long i = 0; i < m_props.cBuffers; ++i)
    {
        hr = CreateSample();

        if (FAILED(hr))
            return hr;
    }

    m_bCommitted = true;

    return S_OK;
}


HRESULT CMemAllocator::Decommit()
{
    //NOTE: If there are any threads waiting on
    //GetBuffer, then that op is supposed to
    //finish, returning an error.
    //
    //Further calls to GetBuffer should fail,
    //until Commit() is called.
    //
    //This op does *not* affect outstanding
    //buffers, so it's perfectly legal for
    //m_active to have a non-zero value.
    //
    //The purpose of this op is to prevent a
    //filter from getting any more samples.

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    m_bCommitted = false;

    while (!m_samples.empty())
    {
        IMemSample* const sample = m_samples.front();
        assert(sample);

        m_samples.pop_front();

        hr = m_pSampleFactory->DestroySample(sample);
        assert(SUCCEEDED(hr));
    }

    const BOOL b = SetEvent(m_hCond);
    b;
    assert(b);

    return S_OK;
}


HRESULT CMemAllocator::GetBuffer(
    IMediaSample** pp,
    REFERENCE_TIME*,
    REFERENCE_TIME*,
    DWORD flags)
{
    if (pp == 0)
        return E_POINTER;

    Lock lock;

    if (flags & AM_GBF_NOWAIT)
    {
        const HRESULT hr = lock.Seize(this);

        if (FAILED(hr))
            return hr;

        if (!m_bCommitted)
            return VFW_E_NOT_COMMITTED;

        assert(m_cActive >= 0);

        if (m_cActive >= m_props.cBuffers)  //no samples available
            return VFW_E_TIMEOUT;
    }
    else
    {
        for (;;)
        {
            HRESULT hr = lock.Seize(this);

            if (FAILED(hr))
                return hr;

            if (!m_bCommitted)
                return VFW_E_NOT_COMMITTED;

            assert(m_cActive >= 0);

            if (m_cActive < m_props.cBuffers)
                break;

            lock.Release();

            DWORD index;
            hr = CoWaitForMultipleHandles(
                    0, //wait all
                    INFINITE,
                    1,
                    &m_hCond,
                    &index);

            assert(hr == S_OK);
            assert(index == 0);
        }
    }

    IMediaSample*& p = *pp;
    p = GetSample();

    AddRef();  //the contribution of this (active) sample

    return S_OK;
}


HRESULT CMemAllocator::ReleaseBuffer(IMediaSample* p)
{
    if (p == 0)  //should only be called by sample, in its Release method
        return E_INVALIDARG;

    //TODO: use custom interface for this
    //assert(p->m_cRef == 0);
    //assert(p->m_pAllocator == this);

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    assert(m_cActive > 0);
    --m_cActive;

    IMemSample* pSample;

    hr = p->QueryInterface(&pSample);
    assert(SUCCEEDED(hr));
    assert(pSample);

    hr = m_pSampleFactory->FinalizeSample(pSample);
    assert(SUCCEEDED(hr));

    if (m_bCommitted)
        m_samples.push_back(pSample);
    else
    {
        hr = m_pSampleFactory->DestroySample(pSample);
        assert(SUCCEEDED(hr));
    }

    const BOOL b = SetEvent(m_hCond);
    b;
    assert(b);

    //Release lock now, in case this sample is holding
    //the last reference to this allocator.
    lock.Release();

    //Now it's safe to release the allocator, after
    //releasing the lock; if this causes the allocator
    //to be destroyed, then the lock won't attempt to
    //manipulate the allocator during its own destruction
    //(another thing to try would be to add a ref to the
    //allocator in the lock's ctor, and then release the
    //ref in its dtor.  This would prevent the destruction
    //of the allocator until the lock itself is destroyed).

    Release();  //the contribution of this sample

    return S_OK;
}


HRESULT CMemAllocator::CreateSample()
{
    IMemSample* pSample;

    const HRESULT hr = m_pSampleFactory->CreateSample(this, pSample);

    if (FAILED(hr))
        return hr;

    assert(pSample);
    assert(pSample->GetCount() == 0);

    m_samples.push_back(pSample);

    return S_OK;
}


IMediaSample* CMemAllocator::GetSample()
{
    assert(m_cActive >= 0);
    assert(m_cActive < m_props.cBuffers);
    assert(!m_samples.empty());

    IMemSample* const p = m_samples.front();
    assert(p);

    m_samples.pop_front();

    HRESULT hr = m_pSampleFactory->InitializeSample(p);
    assert(SUCCEEDED(hr));
    assert(p->GetCount() == 0);

    ++m_cActive;

    IMediaSample* pSample;

    hr = p->QueryInterface(&pSample);
    assert(SUCCEEDED(hr));
    assert(pSample);
    assert(p->GetCount() == 1);

    return pSample;
}
