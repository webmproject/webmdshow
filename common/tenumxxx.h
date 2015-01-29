// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include <objbase.h>

template<class IEnumBase, class XXX>
class TEnumXXX : public IEnumBase
{
public:

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE Next(ULONG, XXX*, ULONG*);
    HRESULT STDMETHODCALLTYPE Skip(ULONG);
    HRESULT STDMETHODCALLTYPE Reset();
    //HRESULT STDMETHODCALLTYPE Clone(IEnumBase**);

protected:

    TEnumXXX();
    TEnumXXX(const TEnumXXX&);

    virtual ~TEnumXXX();

    virtual HRESULT GetCount(ULONG&) const = 0;
    virtual HRESULT GetItem(ULONG, XXX&) = 0;
    virtual void ReleaseItems(XXX*, ULONG);

private:

    TEnumXXX& operator=(const TEnumXXX&);

    ULONG m_cRef;
    ULONG m_index;

};


template<class IEnumBase, class XXX>
inline TEnumXXX<IEnumBase, XXX>::TEnumXXX()
    : m_cRef(1),
      m_index(0)
{
}


template<class IEnumBase, class XXX>
inline TEnumXXX<IEnumBase, XXX>::TEnumXXX(const TEnumXXX& rhs)
    : m_cRef(1),
      m_index(rhs.m_index)
{
}



template<class IEnumBase, class XXX>
inline TEnumXXX<IEnumBase, XXX>::~TEnumXXX()
{
}


template<class IEnumBase, class XXX>
inline HRESULT TEnumXXX<IEnumBase, XXX>::QueryInterface(
    const IID& iid,
    void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == __uuidof(IUnknown))
        pUnk = this;

    else if (iid == __uuidof(IEnumBase))
        pUnk = this;

    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


template<class IEnumBase, class XXX>
inline ULONG TEnumXXX<IEnumBase, XXX>::AddRef()
{
    return InterlockedIncrement((LONG*)&m_cRef);
}


template<class IEnumBase, class XXX>
inline ULONG TEnumXXX<IEnumBase, XXX>::Release()
{
    if (LONG n = InterlockedDecrement((LONG*)&m_cRef))
        return n;

    delete this;
    return 0;
}


template<class IEnumBase, class XXX>
inline HRESULT TEnumXXX<IEnumBase, XXX>::Next(
    ULONG c,
    XXX* pa,
    ULONG* pn)
{
    if (pn)
        *pn = 0;

    if (c == 0) //weird
        return S_OK;

    if (pa == 0)
        return E_POINTER;

    if ((c > 1) && (pn == 0))
        return E_INVALIDARG;

    ULONG index_max;

    HRESULT hr = GetCount(index_max);

    if (FAILED(hr))  //out of sync
        return hr;

    if (m_index >= index_max)
        return S_FALSE;

    const ULONG nn = index_max - m_index;
    const ULONG n = (c < nn) ? c : nn;

    ULONG i = 0;

    for (;;)
    {
        XXX& item = pa[i];

        hr = GetItem(m_index, item);

        if (FAILED(hr))
        {
            ReleaseItems(pa, i);
            return hr;
        }

        ++m_index;

        if (++i == n)
            break;
    }

    if (pn)
        *pn = n;

    return (n < c) ? S_FALSE : S_OK;
}


template<class IEnumBase, class XXX>
inline HRESULT TEnumXXX<IEnumBase, XXX>::Skip(ULONG n)
{
    ULONG max_index;

    const HRESULT hr = GetCount(max_index);

    if (FAILED(hr))
        return hr;

    m_index += n;

    return (m_index >= max_index) ? S_FALSE : S_OK;
}


template<class IEnumBase, class XXX>
inline HRESULT TEnumXXX<IEnumBase, XXX>::Reset()
{
    m_index = 0;
    return S_OK;
}


//template<class IEnumBase, class XXX>
//HRESULT TEnumXXX<IEnumBase, XXX>::Clone(IEnumBase** pp)
//{
//    if (pp == 0)
//        return E_POINTER;
//
//    IEnumBase*& p = *pp;
//
//    return
//}


template<class IEnumBase, class XXX>
inline void TEnumXXX<IEnumBase, XXX>::ReleaseItems(XXX*, ULONG)
{
}
