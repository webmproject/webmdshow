// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "cmediatypes.h"
#include "mediatypeutil.h"
#include <new>
#include <vfwmsgs.h>


CMediaTypes::CEnumMediaTypes::CEnumMediaTypes(
    IPin* pPin,
    CMediaTypes* pMediaTypes) :
    m_pPin(pPin),
    m_pMediaTypes(pMediaTypes)
{
    m_pPin->AddRef();
}


CMediaTypes::CEnumMediaTypes::CEnumMediaTypes(
    const CEnumMediaTypes& rhs) :
    base_t(rhs),
    m_pPin(rhs.m_pPin),
    m_pMediaTypes(rhs.m_pMediaTypes)
{
    m_pPin->AddRef();
}


CMediaTypes::CEnumMediaTypes::~CEnumMediaTypes()
{
    m_pPin->Release();
}


HRESULT CMediaTypes::CEnumMediaTypes::Clone(
    IEnumMediaTypes** pp)
{
    if (pp == 0)
        return E_POINTER;

    IEnumMediaTypes*& p = *pp;

    p = new (std::nothrow) CEnumMediaTypes(*this);

    return p ? S_OK : E_OUTOFMEMORY;
}


HRESULT CMediaTypes::CEnumMediaTypes::GetCount(ULONG& n) const
{
    return m_pMediaTypes->GetCount(n);
}


HRESULT CMediaTypes::CEnumMediaTypes::GetItem(ULONG i, AM_MEDIA_TYPE*& p)
{
    return m_pMediaTypes->GetItem(i, p);
}


void CMediaTypes::CEnumMediaTypes::ReleaseItems(AM_MEDIA_TYPE** a, ULONG n)
{
    m_pMediaTypes->ReleaseItems(a, n);
}


HRESULT CMediaTypes::Add(const AM_MEDIA_TYPE& src)
{
    AM_MEDIA_TYPE tgt;

    const HRESULT hr = MediaTypeUtil::Copy(src, tgt);

    if (FAILED(hr))
        return hr;

    m_vec.push_back(tgt);

    return S_OK;
}


HRESULT CMediaTypes::Clear()
{
    while (!m_vec.empty())
    {
        MediaTypeUtil::Destroy(m_vec.back());
        m_vec.pop_back();
    }

    return S_OK;
}


ULONG CMediaTypes::Size() const
{
    const vec_t::size_type n = m_vec.size();
    return static_cast<ULONG>(n);
}

const AM_MEDIA_TYPE& CMediaTypes::operator[](ULONG i) const
{
    return m_vec.at(i);
}


AM_MEDIA_TYPE& CMediaTypes::operator[](ULONG i)
{
    return m_vec.at(i);
}


HRESULT CMediaTypes::Copy(ULONG i, AM_MEDIA_TYPE& mt) const
{
    return MediaTypeUtil::Copy(m_vec.at(i), mt);
}


HRESULT CMediaTypes::Create(ULONG i, AM_MEDIA_TYPE*& pmt) const
{
    return MediaTypeUtil::Create(m_vec.at(i), pmt);
}


HRESULT CMediaTypes::CreateEnum(
    IPin* pPin,
    IEnumMediaTypes** pp)
{
    if (pp == 0)
        return E_POINTER;

    IEnumMediaTypes*& p = *pp;

    p = new (std::nothrow) CEnumMediaTypes(pPin, this);

    return p ? S_OK : E_OUTOFMEMORY;
}


CMediaTypes::CMediaTypes()
{
}


CMediaTypes::~CMediaTypes()
{
    Clear();
}


HRESULT CMediaTypes::GetCount(ULONG& n) const
{
    const vec_t::size_type size = m_vec.size();
    n = static_cast<ULONG>(size);

    return S_OK;
}


HRESULT CMediaTypes::GetItem(ULONG i, AM_MEDIA_TYPE*& p)
{
    return MediaTypeUtil::Create(m_vec[i], p);
}


void CMediaTypes::ReleaseItems(AM_MEDIA_TYPE** a, ULONG n)
{
    for (ULONG i = 0; i < n; ++i)
        free(a[i]);
}
