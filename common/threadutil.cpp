// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>
#include <windowsx.h>

#include "debugutil.hpp"
#include "threadutil.hpp"

namespace WebmMfUtil
{

SimpleThread::SimpleThread():
  ptr_user_thread_data_(NULL),
  ptr_thread_func_(NULL),
  thread_handle_(INVALID_HANDLE_VALUE),
  thread_id_(0)
{
}

SimpleThread::~SimpleThread()
{
    if (INVALID_HANDLE_VALUE != thread_handle_ && NULL != thread_handle_)
    {
        CloseHandle(thread_handle_);
        ptr_user_thread_data_ = NULL;
    }
}

HRESULT SimpleThread::Run(LPTHREAD_START_ROUTINE ptr_thread_func,
                          LPVOID ptr_data)
{
    if (!ptr_thread_func)
        return E_INVALIDARG;

    ptr_thread_func_ = ptr_thread_func;
    ptr_user_thread_data_ = ptr_data;

    HRESULT hr = E_FAIL;

    thread_handle_ = CreateThread(NULL, 0, ThreadWrapper_,
                                  reinterpret_cast<LPVOID>(this), 0,
                                  &thread_id_);

    if (thread_handle_ != NULL)
        hr = S_OK;

    return hr;
}

DWORD WINAPI SimpleThread::ThreadWrapper_(LPVOID ptr_this)
{
    if (!ptr_this)
        return EXIT_FAILURE;

    SimpleThread* ptr_sthread = reinterpret_cast<SimpleThread*>(ptr_this);

    if (!ptr_sthread || !ptr_sthread->ptr_thread_func_)
        return EXIT_FAILURE;

    return ptr_sthread->ptr_thread_func_(ptr_sthread->ptr_user_thread_data_);
}

} // WebmMfUtil namespace
