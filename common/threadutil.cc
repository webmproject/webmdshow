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

#include <memory>

#include "debugutil.h"
#include "eventutil.h"
#include "threadutil.h"

namespace WebmMfUtil
{

SimpleThread::SimpleThread():
  ptr_user_thread_data_(NULL),
  ptr_thread_func_(NULL),
  thread_hndl_(0),
  thread_id_(0)
{
}

SimpleThread::~SimpleThread()
{
}

bool SimpleThread::Running()
{
    HANDLE thread_handle = reinterpret_cast<HANDLE>(thread_hndl_);
    if (thread_handle && INVALID_HANDLE_VALUE != thread_handle)
    {
        HRESULT hr = zero_cowait(thread_handle);
        if (SUCCEEDED(hr))
        {
            return false;
        }
    }
    return true;
}

HRESULT SimpleThread::Run(LPTHREAD_START_ROUTINE ptr_thread_func,
                          LPVOID ptr_data)
{
    if (!ptr_thread_func)
        return E_INVALIDARG;

    ptr_thread_func_ = ptr_thread_func;
    ptr_user_thread_data_ = ptr_data;

    HRESULT hr = E_FAIL;

    thread_hndl_ = _beginthreadex(NULL, 0, ThreadWrapper_,
                                  reinterpret_cast<LPVOID>(this), 0,
                                  &thread_id_);

    if (0 != thread_hndl_ && -1 != thread_hndl_)
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

    return thread_result;
}


HRESULT RefCountedThread::Create(RefCountedThread** ptr_instance)
{
    *ptr_instance = new RefCountedThread();
    if (!*ptr_instance)
        return E_OUTOFMEMORY;
    (*ptr_instance)->AddRef();
    return S_OK;
}

RefCountedThread::RefCountedThread():
  ref_count_(0)
{
}

RefCountedThread::~RefCountedThread()
{
}

UINT RefCountedThread::AddRef()
{
    return InterlockedIncrement(&ref_count_);
}

UINT RefCountedThread::Release()
{
    UINT ref_count = InterlockedDecrement(&ref_count_);
    if (ref_count == 0)
    {
        delete this;
    }
    return ref_count;
}

StoppableThread::StoppableThread():
  ptr_event_(NULL)
{
}

StoppableThread::~StoppableThread()
{
}

HRESULT StoppableThread::Create(StoppableThread **ptr_instance)
{
    StoppableThread* ptr_thread = new (std::nothrow) StoppableThread();
    if (!ptr_thread)
    {
        DBGLOG("NULL thread, no memory!");
        return E_OUTOFMEMORY;
    }
    ptr_event_.reset(new (std::nothrow) EventWaiter());
    if (!ptr_event_.get())
    {
        DBGLOG("NULL event, no memory!");
        return E_OUTOFMEMORY;
    }
    HRESULT hr;
    CHK(hr, ptr_event_->Create());
    if (SUCCEEDED(hr))
    {
        *ptr_instance = ptr_thread;
    }
    return hr;
}

bool StoppableThread::StopRequested()
{
    if (ptr_event_.get() && Running())
    {
        if (FAILED(ptr_event_->ZeroWait()))
        {
            return false;
        }
    }
    return true;
}

} // WebmMfUtil namespace
