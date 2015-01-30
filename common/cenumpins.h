// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include <strmif.h>

#include <vector>

#include "tenumxxx.h"

class CEnumPins : public TEnumXXX<IEnumPins, IPin*>
{
    virtual ~CEnumPins();
    CEnumPins(const CEnumPins&);
    CEnumPins& operator=(const CEnumPins&);

public:

    CEnumPins(IPin* const*, ULONG);

    template<typename T>
    explicit CEnumPins(T* const*, ULONG);

    static HRESULT CreateInstance(IPin* const*, ULONG, IEnumPins**);

    template<typename T>
    static HRESULT CreateInstance(T* const*, ULONG, IEnumPins**);

    HRESULT STDMETHODCALLTYPE Clone(IEnumPins**);

protected:

    HRESULT GetCount(ULONG&) const;
    HRESULT GetItem(ULONG, IPin*&);
    void ReleaseItems(IPin**, ULONG);

private:

    typedef std::vector<IPin*> pins_t;
    pins_t m_pins;

};


template<typename T>
inline CEnumPins::CEnumPins(T* const* i, ULONG n)
{
    m_pins.reserve(n);

    T* const* j = i + n;

    while (i != j)
    {
        IPin* const p = *i++;
        p->AddRef();

        m_pins.push_back(p);
    }
}

template<typename T>
inline HRESULT CEnumPins::CreateInstance(
    T* const* i,
    ULONG n,
    IEnumPins** pp)
{
    if (pp == 0)
        return E_POINTER;

    IEnumPins*& p = *pp;

    p = new (std::nothrow) CEnumPins(i, n);

    return p ? S_OK : E_OUTOFMEMORY;
}
