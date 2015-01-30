// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "clockable.h"
#include <vfwmsgs.h>
#include <cassert>


CLockable::CLockable() :
    m_hMutex(0)
{
}


CLockable::~CLockable()
{
    Final();
}


HRESULT CLockable::Init()
{
    if (m_hMutex)  //weird
        return S_FALSE;

    m_hMutex = CreateMutex(0, 0, 0);

    if (m_hMutex)
        return S_OK;

    const DWORD e = GetLastError();
    return HRESULT_FROM_WIN32(e);
}


HRESULT CLockable::Final()
{
    if (m_hMutex == 0)
        return S_FALSE;

    const BOOL b = CloseHandle(m_hMutex);
    m_hMutex = 0;

    if (b)
        return S_OK;

    const DWORD e = GetLastError();
    return HRESULT_FROM_WIN32(e);
}


HRESULT CLockable::Seize(DWORD timeout_ms)
{
    if (m_hMutex == 0)
        return VFW_E_WRONG_STATE;

    DWORD index;

    const HRESULT hr = CoWaitForMultipleHandles(
                            0,  //wait flags
                            timeout_ms,
                            1,
                            &m_hMutex,
                            &index);

    //despite the "S" in this name, this is an error
    if (hr == RPC_S_CALLPENDING)
        return VFW_E_TIMEOUT;

    if (FAILED(hr))
        return hr;

    assert(index == 0);
    return S_OK;
}


HRESULT CLockable::Release()
{
    if (m_hMutex == 0)
        return VFW_E_WRONG_STATE;

    const BOOL b = ReleaseMutex(m_hMutex);

    if (b)
        return S_OK;

    const DWORD e = GetLastError();
    return HRESULT_FROM_WIN32(e);
}


CLockable::Lock::Lock() :
    m_pLockable(0)
{
}


CLockable::Lock::~Lock()
{
    Release();
}


HRESULT CLockable::Lock::Seize(CLockable* pLockable)
{
    if (pLockable == 0)
        return E_INVALIDARG;

    if (m_pLockable)
        return VFW_E_WRONG_STATE;

    const HRESULT hr = pLockable->Seize(5000);

    if (FAILED(hr))
        return hr;

    m_pLockable = pLockable;
    return S_OK;
}


HRESULT CLockable::Lock::Release()
{
    if (m_pLockable == 0)
        return S_FALSE;

    const HRESULT hr = m_pLockable->Release();

    m_pLockable = 0;

    return hr;
}
