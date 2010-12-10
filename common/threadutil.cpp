// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <windowsx.h>
#include <process.h>

#include "debugutil.hpp"
#include "threadutil.hpp"

namespace WebmMfUtil
{

HRESULT SimpleThread::Create(SimpleThread** ptr_instance)
{
    *ptr_instance = new SimpleThread();
    if (!*ptr_instance)
        return E_OUTOFMEMORY;
    (*ptr_instance)->AddRef();
    return S_OK;
}

SimpleThread::SimpleThread():
  ptr_user_thread_data_(NULL),
  ptr_thread_func_(NULL),
  ptr_thread_(0),
  ref_count_(0),
  thread_id_(0)
{
}

SimpleThread::~SimpleThread()
{
}

UINT SimpleThread::AddRef()
{
    return InterlockedIncrement(&ref_count_);
}

UINT SimpleThread::Release()
{
    UINT ref_count = InterlockedDecrement(&ref_count_);
    if (ref_count == 0)
    {
        delete this;
    }
    return ref_count;
}

HRESULT SimpleThread::Run(LPTHREAD_START_ROUTINE ptr_thread_func,
                          LPVOID ptr_data)
{
    if (!ptr_thread_func)
        return E_INVALIDARG;

    ptr_thread_func_ = ptr_thread_func;
    ptr_user_thread_data_ = ptr_data;

    HRESULT hr = E_FAIL;

    ptr_thread_ = _beginthreadex(NULL, 0, ThreadWrapper_,
                                 reinterpret_cast<LPVOID>(this), 0,
                                 &thread_id_);

    if (0 != ptr_thread_ && -1 != ptr_thread_)
        hr = S_OK;

    return hr;
}

UINT WINAPI SimpleThread::ThreadWrapper_(LPVOID ptr_this)
{
    if (!ptr_this)
        return EXIT_FAILURE;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        DBGLOG("CoInitializeEx failed");
        return hr;
    }

    SimpleThread* ptr_sthread = reinterpret_cast<SimpleThread*>(ptr_this);

    if (!ptr_sthread || !ptr_sthread->ptr_thread_func_)
        return EXIT_FAILURE;

    DWORD thread_result =
        ptr_sthread->ptr_thread_func_(ptr_sthread->ptr_user_thread_data_);

    CoUninitialize();

    ptr_sthread->Release();

    return thread_result;
}

} // WebmMfUtil namespace
