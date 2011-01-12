// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_THREADUTIL_HPP__
#define __WEBMDSHOW_COMMON_THREADUTIL_HPP__

namespace WebmMfUtil
{

class SimpleThread
{
public:
    SimpleThread();
    virtual ~SimpleThread();
    HRESULT Run(LPTHREAD_START_ROUTINE ptr_thread_func, LPVOID ptr_data);
protected:
    static UINT WINAPI ThreadWrapper_(LPVOID ptr_this);
    UINT thread_id_;
    UINT_PTR ptr_thread_;
    LPTHREAD_START_ROUTINE ptr_thread_func_;
    LPVOID ptr_user_thread_data_;
    DISALLOW_COPY_AND_ASSIGN(SimpleThread);
};

class RefCountedThread : public SimpleThread
{
public:
    virtual ~RefCountedThread();
    static HRESULT Create(RefCountedThread** ptr_instance);
    UINT AddRef();
    UINT Release();
private:
    RefCountedThread();
    UINT ref_count_;
    DISALLOW_COPY_AND_ASSIGN(RefCountedThread);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_THREADUTIL_HPP__