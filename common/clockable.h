// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include <objbase.h>

class CLockable
{
    CLockable(const CLockable&);
    CLockable& operator=(const CLockable&);

protected:

    CLockable();
    virtual ~CLockable();

public:

    HRESULT Init();
    HRESULT Final();

    HRESULT Seize(DWORD timeout_ms);
    HRESULT Release();

    class Lock
    {
        Lock(const Lock&);
        Lock& operator=(const Lock&);

    public:
        Lock();
        ~Lock();

        HRESULT Seize(CLockable*);
        HRESULT Release();

    private:
        CLockable* m_pLockable;

    };

private:

    HANDLE m_hMutex;

};
