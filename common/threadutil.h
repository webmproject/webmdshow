// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_THREADUTIL_HPP__
#define __WEBMDSHOW_COMMON_THREADUTIL_HPP__

#include "debugutil.h"
#include "eventutil.h"

namespace WebmMfUtil
{

class SimpleThread
{
public:
    SimpleThread();
    virtual ~SimpleThread();
    bool Running();
    HRESULT Run(LPTHREAD_START_ROUTINE ptr_thread_func, LPVOID ptr_data);
protected:
    static UINT WINAPI ThreadWrapper_(LPVOID ptr_this);
    UINT thread_id_;
    UINT_PTR thread_hndl_;
    LPTHREAD_START_ROUTINE ptr_thread_func_;
    LPVOID ptr_user_thread_data_;
private:
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

class StoppableThread : public SimpleThread
{
public:
    virtual ~StoppableThread();
    HRESULT Create(StoppableThread** ptr_instance);
    bool StopRequested();
    void* GetUserData() const
    {
        return ptr_user_thread_data_;
    };
    HRESULT Stop();  // not implemented
    // TODO(tomfinegan): make Run virtual and override
private:
    StoppableThread();
    std::auto_ptr<EventWaiter> ptr_event_;
    DISALLOW_COPY_AND_ASSIGN(StoppableThread);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_THREADUTIL_HPP__