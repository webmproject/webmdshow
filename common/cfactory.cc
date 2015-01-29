// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "cfactory.h"
#include <cassert>

CFactory::CFactory(ULONG* pcLock, create_t create)
    : m_pcLock(pcLock),
      m_create(create),
      m_cRef(1)  //stack-allocated
{
}


CFactory::~CFactory()
{
}


HRESULT CFactory::QueryInterface(const IID& iid, void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == IID_IUnknown)
        pUnk = static_cast<IUnknown*>(this);

    else if (iid == IID_IClassFactory)
        pUnk = static_cast<IClassFactory*>(this);

    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG CFactory::AddRef()
{
    return InterlockedIncrement((LONG*)&m_cRef);
}


ULONG CFactory::Release()
{
    assert(m_cRef > 1);
    return InterlockedDecrement((LONG*)&m_cRef);
}


HRESULT CFactory::CreateInstance(
    IUnknown* pOuter,
    const IID& iid,
    void** ppv)
{
    return (*m_create)(this, pOuter, iid, ppv);
}


HRESULT CFactory::LockServer(BOOL b)
{
    if (b)
        InterlockedIncrement((LONG*)m_pcLock);
    else
        InterlockedDecrement((LONG*)m_pcLock);

    return S_OK;
}
