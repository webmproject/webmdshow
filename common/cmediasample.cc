// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "cmediasample.h"
#include "mediatypeutil.h"
#include <new>
#include <cassert>
#include <vfwmsgs.h>


CMediaSample::Factory::~Factory()
{
}


HRESULT CMediaSample::Factory::CreateSample(
    CMemAllocator* pAllocator,
    IMemSample*& pResult)
{
    assert(pAllocator);
    pResult = 0;

    CMediaSample* const pSample = new (std::nothrow) CMediaSample(pAllocator);

    if (pSample == 0)
        return E_OUTOFMEMORY;

    HRESULT hr = pSample->Create();

    if (FAILED(hr))
    {
        delete pSample;
        return hr;
    }

    assert(pSample->m_cRef == 0);

    //We don't bother to Initialize here.  The purpose of CreateSample is
    //simply to create a new sample object to add to allocator's pool,
    //during Commit.  The sample will get propertly initialized later,
    //during GetBuffer.

    pResult = pSample;

    return S_OK;
}


HRESULT CMediaSample::Factory::InitializeSample(IMemSample* pSample)
{
    assert(pSample);
    return pSample->Initialize();
}


HRESULT CMediaSample::Factory::FinalizeSample(IMemSample* pSample)
{
    assert(pSample);
    return pSample->Finalize();
}


HRESULT CMediaSample::Factory::DestroySample(IMemSample* pSample)
{
    assert(pSample);
    return pSample->Destroy();
}


HRESULT CMediaSample::Factory::Destroy(CMemAllocator*)
{
    delete this;
    return S_OK;
}


HRESULT CMediaSample::CreateAllocator(IMemAllocator** pp)
{
    if (pp == 0)
        return E_POINTER;

    IMemAllocator*& p = *pp;
    p = 0;

    Factory* const pFactory = new (std::nothrow) Factory;

    if (pFactory == 0)
        return E_OUTOFMEMORY;

    const HRESULT hr = CMemAllocator::CreateInstance(pFactory, pp);

    if (FAILED(hr))
        delete pFactory;

    return hr;
}


HRESULT CMediaSample::Create()
{
    assert(m_cRef == 0);
    assert(m_buf == 0);
    assert(m_buflen == 0);

    ALLOCATOR_PROPERTIES props;

    HRESULT hr = m_pAllocator->GetProperties(&props);
    hr;
    assert(SUCCEEDED(hr));
    assert(props.cbAlign >= 1);
    assert(props.cbPrefix >= 0);
    assert(props.cbBuffer >= 0);

    const long buflen = props.cbAlign - 1 + props.cbPrefix + props.cbBuffer;
    BYTE* const buf = new (std::nothrow) BYTE[buflen];

    if (buf == 0)
        return E_OUTOFMEMORY;

    m_buf = buf;
    m_buflen = buflen;

    long off = props.cbPrefix;

    if (intptr_t n = intptr_t(buf) % props.cbAlign)
        off += props.cbAlign - n;

    m_off = off;

    return S_OK;
}


ULONG CMediaSample::GetCount()
{
    return m_cRef;
}


HRESULT CMediaSample::Initialize()
{
    assert(m_buf);
    assert(m_pmt == 0);

    m_cRef = 0;
    m_sync_point = false;
    m_actual_data_length = 0;
    m_preroll = false;
    m_discontinuity = false;
    m_start_time = LLONG_MAX;
    m_stop_time = LLONG_MAX;
    m_media_start_time = LLONG_MAX;
    m_media_stop_time = LLONG_MAX;

    return S_OK;
}


HRESULT CMediaSample::Finalize()
{
    MediaTypeUtil::Free(m_pmt);
    m_pmt = 0;

    return S_OK;
}


HRESULT CMediaSample::Destroy()
{
    delete this;
    return S_OK;
}


CMediaSample::CMediaSample(CMemAllocator* p) :
    m_pAllocator(p),
    m_cRef(0),  //allocator will adjust
    m_buf(0),
    m_buflen(0),
    m_off(0),
    m_pmt(0)
{
}


CMediaSample::~CMediaSample()
{
    Finalize();  //deallocate media type
    delete[] m_buf;
}


HRESULT CMediaSample::QueryInterface(const IID& iid, void** ppv)
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

    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG CMediaSample::AddRef()
{
    return InterlockedIncrement((LONG*)&m_cRef);
}


ULONG CMediaSample::Release()
{
    if (LONG n = InterlockedDecrement((LONG*)&m_cRef))
        return n;

    m_pAllocator->ReleaseBuffer(this);
    return 0;
}


HRESULT CMediaSample::GetPointer(BYTE** pp)
{
    if (pp == 0)
        return E_POINTER;

    BYTE*& p = *pp;

    assert(m_buf);
    assert(m_buflen >= 0);
    assert(m_off >= 0);
    assert(m_off <= m_buflen);

    p = m_buf + m_off;

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


long CMediaSample::GetSize()
{
    assert(m_off <= m_buflen);

    const long size = m_buflen - m_off;

#ifdef _DEBUG
    ALLOCATOR_PROPERTIES props;

    const HRESULT hr = m_pAllocator->GetProperties(&props);
    assert(SUCCEEDED(hr));
    assert(size >= props.cbBuffer);
#endif

    return size;
}


HRESULT CMediaSample::GetTime(
    REFERENCE_TIME* pstart,
    REFERENCE_TIME* pstop)
{
    if (m_start_time == LLONG_MAX)
        return VFW_E_SAMPLE_TIME_NOT_SET;

    if (pstart)
        *pstart = m_start_time;

    if (m_stop_time == LLONG_MAX)
        return VFW_S_NO_STOP_TIME;

    if (pstop)
        *pstop = m_stop_time;

    return S_OK;
}


HRESULT CMediaSample::SetTime(
    REFERENCE_TIME* pstart,
    REFERENCE_TIME* pstop)
{
    if (pstart)
        m_start_time = *pstart;
    else
        m_start_time = LLONG_MAX;

    if (pstop)
        m_stop_time = *pstop;
    else
        m_stop_time = LLONG_MAX;

    return S_OK;
}


HRESULT CMediaSample::IsSyncPoint()
{
    return m_sync_point ? S_OK : S_FALSE;
}


HRESULT CMediaSample::SetSyncPoint(BOOL b)
{
    m_sync_point = bool(b != 0);
    return S_OK;
}


HRESULT CMediaSample::IsPreroll()
{
    return m_preroll ? S_OK : S_FALSE;
}


HRESULT CMediaSample::SetPreroll(BOOL b)
{
    m_preroll = bool(b != 0);
    return S_OK;
}


long CMediaSample::GetActualDataLength()
{
    return m_actual_data_length;
}


HRESULT CMediaSample::SetActualDataLength(long len)
{
    m_actual_data_length = len;
    return S_OK;
}


HRESULT CMediaSample::GetMediaType(
    AM_MEDIA_TYPE** pp)
{
    if (pp == 0)
        return E_POINTER;

    AM_MEDIA_TYPE*& p = *pp;

    if (m_pmt)
        return MediaTypeUtil::Create(*m_pmt, p);

    p = 0;
    return S_FALSE;  //means "no media type"
}


HRESULT CMediaSample::SetMediaType(
    AM_MEDIA_TYPE* pmt)
{
    if (pmt == 0)
    {
        MediaTypeUtil::Free(m_pmt);
        m_pmt = 0;

        return S_OK;
    }

    return MediaTypeUtil::Create(*pmt, m_pmt);
}


HRESULT CMediaSample::IsDiscontinuity()
{
    return m_discontinuity ? S_OK : S_FALSE;
}


HRESULT CMediaSample::SetDiscontinuity(BOOL b)
{
    m_discontinuity = bool(b != 0);
    return S_OK;
}


HRESULT CMediaSample::GetMediaTime(
    LONGLONG* pstart,
    LONGLONG* pstop)
{
    if (m_media_start_time == LLONG_MAX)
        return VFW_E_MEDIA_TIME_NOT_SET;

    if (pstart)
        *pstart = m_media_start_time;

    if (pstop)
    {
        if (m_media_stop_time != LLONG_MAX)
            *pstop = m_media_stop_time;
        else  //kind of bugus
            *pstop = m_media_start_time + 1; //?
    }

    return S_OK;
}


HRESULT CMediaSample::SetMediaTime(
    LONGLONG* pstart,
    LONGLONG* pstop)
{
    if (pstart)
        m_media_start_time = *pstart;
    else
        m_media_start_time = LLONG_MAX;

    if (pstop)
        m_media_stop_time = *pstop;
    else  //kind of bogus
        m_media_stop_time = LLONG_MAX;

    return S_OK;
}
