// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "CVP8Sample.h"
#include <new>
#include <cassert>
#include <vfwmsgs.h>


HRESULT CVP8Sample::CreateAllocator(IMemAllocator** pp)
{
    if (pp == 0)
        return E_POINTER;

    IMemAllocator*& pResult = *pp;
    pResult = 0;

    SampleFactory* pSampleFactory = new (std::nothrow) SampleFactory;

    if (pSampleFactory == 0)
        return E_OUTOFMEMORY;

    const HRESULT hr = CMemAllocator::CreateInstance(pSampleFactory, &pResult);

    if (FAILED(hr))
    {
        pSampleFactory->Destroy(0);
        return VFW_E_NO_ALLOCATOR;
    }

    return S_OK;
}


HRESULT CVP8Sample::GetFrame(CMemAllocator* pAlloc, IVP8Sample::Frame& f)
{
    CMemAllocator::Lock lock;

    HRESULT hr = lock.Seize(pAlloc);

    if (FAILED(hr))
        return hr;

    ALLOCATOR_PROPERTIES props;

    hr = pAlloc->GetProperties(&props);
    assert(SUCCEEDED(hr));
    assert(props.cBuffers > 0);
    assert(props.cbBuffer > 0);
    assert(props.cbAlign >= 1);
    assert(props.cbPrefix >= 0);

    const long buflen = props.cbAlign - 1 + props.cbPrefix + props.cbBuffer;

    CMemAllocator::ISampleFactory* const pFactory_ = pAlloc->m_pSampleFactory;
    assert(pFactory_);

    SampleFactory* const pFactory = static_cast<SampleFactory*>(pFactory_);
    SampleFactory::frames_t& pool = pFactory->m_pool;

    if (!pool.empty())
    {
        f = pool.front();
        assert(f.buf);
        assert(f.buflen >= buflen);

        pool.pop_front();
    }
    else
    {
        BYTE* const buf = new (std::nothrow) BYTE[buflen];

        if (buf == 0)
            return E_OUTOFMEMORY;

        f.buf = buf;
        f.buflen = buflen;

        long off = props.cbPrefix;

        if (intptr_t n = intptr_t(buf) % props.cbAlign)
            off += props.cbAlign - n;

        f.off = off;
    }

    BYTE* const ptr = f.buf + f.off;
    ptr;
    assert(intptr_t(ptr - props.cbPrefix) % props.cbAlign == 0);

    return S_OK;
}


CVP8Sample::SampleFactory::SampleFactory()
{
}


CVP8Sample::SampleFactory::~SampleFactory()
{
    PurgePool();
}

HRESULT CVP8Sample::SampleFactory::CreateSample(
    CMemAllocator* pAllocator,
    IMemSample*& pResult)
{
    pResult = 0;

    CVP8Sample* pSample;

    const HRESULT hr = CVP8Sample::CreateInstance(pAllocator, pSample);

    if (FAILED(hr))
        return hr;

    assert(pSample);
    assert(pSample->GetCount() == 0);

    pResult = pSample;
    return S_OK;
}


HRESULT CVP8Sample::SampleFactory::InitializeSample(IMemSample* p)
{
    assert(p);

    const HRESULT hr = p->Initialize();
    assert(SUCCEEDED(hr));
    assert(p->GetCount() == 0);

    return S_OK;
}


HRESULT CVP8Sample::SampleFactory::FinalizeSample(IMemSample* p)
{
    assert(p);

    //Note that FinalizeSample is called by the allocator while
    //it holds its own lock.  There's no special locking we need
    //to here, because the allocator owns the sample factory
    //object it was given when it (the allocator) was created.

    IVP8Sample* pSample;

    HRESULT hr = p->QueryInterface(&pSample);
    assert(SUCCEEDED(hr));
    assert(pSample);

    //NOTE: we don't bother releasing the IVP8Sample ptr, because
    //the sample is in the process of being destroyed.  We don't want
    //to trigger another call to IMemAllocator::ReleaseBuffer from
    //IMediaSample::Release.

    IVP8Sample::Frame& f = pSample->GetFrame();
    assert(f.buf);

    m_pool.push_back(f);

    f.buf = 0;

    hr = p->Finalize();
    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT CVP8Sample::SampleFactory::DestroySample(IMemSample* pSample)
{
    assert(pSample);
    return pSample->Destroy();
}


HRESULT CVP8Sample::SampleFactory::Destroy(CMemAllocator*)
{
    delete this;
    return S_OK;
}


void CVP8Sample::SampleFactory::PurgePool()
{
    while (!m_pool.empty())
    {
        IVP8Sample::Frame& f = m_pool.front();
        assert(f.buf);

        delete[] f.buf;

        m_pool.pop_front();
    }
}


HRESULT CVP8Sample::CreateInstance(
    CMemAllocator* pAllocator,
    CVP8Sample*& pSample)
{
    if (pAllocator == 0)
        return E_INVALIDARG;

    pSample = new (std::nothrow) CVP8Sample(pAllocator);

    return pSample ? S_OK : E_OUTOFMEMORY;
}


CVP8Sample::CVP8Sample(CMemAllocator* p)
    : m_pAllocator(p),
      m_cRef(0)  //allocator will adjust
{
    m_frame.buf = 0;
}


CVP8Sample::~CVP8Sample()
{
    assert(m_frame.buf == 0);
}


HRESULT CVP8Sample::QueryInterface(const IID& iid, void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == __uuidof(IUnknown))
        pUnk = static_cast<IMediaSample*>(this);

    else if (iid == __uuidof(IMediaSample))
        pUnk = static_cast<IMediaSample*>(this);

    else if (iid == __uuidof(IMemSample))
        pUnk = static_cast<IMemSample*>(this);

    else if (iid == __uuidof(IVP8Sample))
        pUnk = static_cast<IVP8Sample*>(this);

    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG CVP8Sample::AddRef()
{
    return InterlockedIncrement((LONG*)&m_cRef);
}


ULONG CVP8Sample::Release()
{
    assert(m_cRef > 0);

    if (LONG n = InterlockedDecrement((LONG*)&m_cRef))
        return n;

    m_pAllocator->ReleaseBuffer(this);
    return 0;
}


CVP8Sample::Frame& CVP8Sample::GetFrame()
{
    return m_frame;
}


ULONG CVP8Sample::GetCount()
{
    return m_cRef;
}


HRESULT CVP8Sample::Initialize()
{
    assert(m_frame.buf == 0);
    m_cRef = 0;

    return S_OK;
}


HRESULT CVP8Sample::Finalize()
{
    assert(m_frame.buf == 0);
    return S_OK;
}


HRESULT CVP8Sample::Destroy()
{
    delete this;
    return S_OK;
}



HRESULT CVP8Sample::GetPointer(BYTE** pp)
{
    if (pp == 0)
        return E_POINTER;

    BYTE*& p = *pp;

    const Frame& f = m_frame;
    assert(f.buf);
    assert(f.buflen >= 0);
    assert(f.off >= 0);
    assert(f.off <= f.buflen);

    p = f.buf + f.off;

#ifdef _DEBUG
    ALLOCATOR_PROPERTIES props;

    const HRESULT hr = m_pAllocator->GetProperties(&props);
    assert(SUCCEEDED(hr));
    assert(props.cbAlign >= 1);
    assert(props.cbPrefix >= 0);
    assert(intptr_t(p - props.cbPrefix) % props.cbAlign == 0);
#endif

    return S_OK;
}


long CVP8Sample::GetSize()
{
    const Frame& f = m_frame;
    assert(f.buf);
    assert(f.off <= f.buflen);

    const long size = f.buflen - f.off;
    assert(size >= 0);

#ifdef _DEBUG
    ALLOCATOR_PROPERTIES props;

    const HRESULT hr = m_pAllocator->GetProperties(&props);
    assert(SUCCEEDED(hr));
    assert(size >= props.cbBuffer);
#endif

    return size;
}


HRESULT CVP8Sample::GetTime(
    REFERENCE_TIME* pstart,
    REFERENCE_TIME* pstop)
{
    const Frame& f = m_frame;
    assert(f.buf);
    assert(f.start >= 0);

    if (pstart == 0)
        return E_POINTER;

    *pstart = f.start;

    if (pstop == 0)
        return S_OK;

    if (f.stop < f.start)  //no stop time
    {
        *pstop = f.start + 1;
        return VFW_S_NO_STOP_TIME;
    }

    *pstop = f.stop;
    return S_OK;
}


HRESULT CVP8Sample::SetTime(
    REFERENCE_TIME*,
    REFERENCE_TIME*)
{
    return E_NOTIMPL;
}


HRESULT CVP8Sample::IsSyncPoint()
{
    assert(m_frame.buf);
    return m_frame.key ? S_OK : S_FALSE;
}


HRESULT CVP8Sample::SetSyncPoint(BOOL)
{
    return E_NOTIMPL;
}


HRESULT CVP8Sample::IsPreroll()
{
    assert(m_frame.buf);

    //TODO:
    return m_preroll ? S_OK : S_FALSE;
}


HRESULT CVP8Sample::SetPreroll(BOOL b)
{
    assert(m_frame.buf);

    //TODO:
    m_preroll = bool(b != 0);
    return S_OK;
}


long CVP8Sample::GetActualDataLength()
{
    assert(m_frame.buf);
    assert(m_frame.len >= 0);

    return m_frame.len;
}


HRESULT CVP8Sample::SetActualDataLength(long)
{
    return E_NOTIMPL;
}


HRESULT CVP8Sample::GetMediaType(
    AM_MEDIA_TYPE** pp)
{
    if (pp == 0)
        return E_POINTER;

    AM_MEDIA_TYPE*& p = *pp;

    p = 0;
    return S_FALSE;  //means "no media type"
}


HRESULT CVP8Sample::SetMediaType(
    AM_MEDIA_TYPE*)
{
    return E_NOTIMPL;
}


HRESULT CVP8Sample::IsDiscontinuity()
{
    assert(m_frame.buf);

    //TODO:
    return m_discontinuity ? S_OK : S_FALSE;
}


HRESULT CVP8Sample::SetDiscontinuity(BOOL b)
{
    assert(m_frame.buf);

    //TODO:
    m_discontinuity = bool(b != 0);
    return S_OK;
}


HRESULT CVP8Sample::GetMediaTime(
    LONGLONG*,
    LONGLONG*)
{
    return VFW_E_MEDIA_TIME_NOT_SET;
}


HRESULT CVP8Sample::SetMediaTime(
    LONGLONG*,
    LONGLONG*)
{
    return E_NOTIMPL;
}
