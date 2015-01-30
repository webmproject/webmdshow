// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "cenumpins.h"
#include <new>

CEnumPins::CEnumPins(IPin* const* a, ULONG n)
{
    m_pins.reserve(n);

    for (ULONG i = 0; i < n; ++i)
    {
        IPin* const p = a[i];
        p->AddRef();

        m_pins.push_back(p);
    }
}


CEnumPins::~CEnumPins()
{
    while (!m_pins.empty())
    {
        IPin* const p = m_pins.back();
        m_pins.pop_back();

        p->Release();
    }
}


CEnumPins::CEnumPins(const CEnumPins& rhs)
{
    const pins_t::size_type n = rhs.m_pins.size();

    m_pins.reserve(n);

    for (ULONG i = 0; i < n; ++i)
    {
        IPin* const p = rhs.m_pins[i];
        p->AddRef();

        m_pins.push_back(p);
    }
}


HRESULT CEnumPins::Clone(IEnumPins** pp)
{
    if (pp == 0)
        return E_POINTER;

    IEnumPins*& p = *pp;

    p = new (std::nothrow) CEnumPins(*this);

    return p ? S_OK : E_OUTOFMEMORY;
}


HRESULT CEnumPins::GetCount(ULONG& n) const
{
    const pins_t::size_type size = m_pins.size();
    n = static_cast<ULONG>(size);

    return S_OK;
}


HRESULT CEnumPins::GetItem(ULONG i, IPin*& p)
{
    p = m_pins[i];
    p->AddRef();

    return S_OK;
}


void CEnumPins::ReleaseItems(IPin** a, ULONG n)
{
    for (ULONG i = 0; i < n; ++i)
    {
        IPin* const p = a[i];
        p->Release();
    }
}


HRESULT CEnumPins::CreateInstance(
    IPin* const* a,
    ULONG n,
    IEnumPins** pp)
{
    if (pp == 0)
        return E_POINTER;

    IEnumPins*& p = *pp;

    p = new (std::nothrow) CEnumPins(a, n);

    return p ? S_OK : E_OUTOFMEMORY;
}


