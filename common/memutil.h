// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_MEMUTIL_HPP__
#define __WEBMDSHOW_COMMON_MEMUTIL_HPP__

#pragma once

#include "chromium/base/basictypes.h"

namespace WebmUtil
{

template <typename BufferType>
class auto_array
{
public:
    auto_array(BufferType* ptr_buf, size_t count) :
      buffer_(ptr_buf),
      num_elements_(count)
    {
    };
    ~auto_array()
    {
        if (buffer_)
        {
            delete[] buffer_;
            buffer_ = NULL;
        }
        num_elements_ = 0;
    };
    BufferType* get() const
    {
        return buffer_;
    };
    BufferType* operator->() const
    {
        return get();
    };
    BufferType& operator*() const
    {
        return *get();
    };
    operator bool() const
    {
        return buffer_ != NULL;
    };
    operator BufferType*() const
    {
        return get();
    };
    size_t size() const
    {
        return num_elements_;
    };
    void reset(BufferType* ptr_buf, size_t count)
    {
        if (buffer_)
        {
            delete[] buffer_;
            buffer_ = (BufferType)0;
        }
        num_elements_ = count;
        buffer_ = ptr_buf;
    };
private:
    BufferType* buffer_;
    size_t num_elements_;
    DISALLOW_COPY_AND_ASSIGN(auto_array);
};

// |auto_ref_counted_obj_ptr|, an auto_ptr like class for managing ref counted
// object pointers.
template <typename RefCountedObject>
class auto_ref_counted_obj_ptr // this name seems a crime against humanity...
{
public:
    auto_ref_counted_obj_ptr(RefCountedObject* ptr_obj) :
      ptr_obj_(ptr_obj)
    {
    };
    ~auto_ref_counted_obj_ptr()
    {
        safe_rel(ptr_obj_);
    };
    // allow user to take ownership of encapsulated pointer
    RefCountedObject* detach()
    {
        RefCountedObject* ptr_obj = ptr_obj_;
        ptr_obj_ = NULL;
        return ptr_obj;
    };
    RefCountedObject* get() const
    {
        return ptr_obj_;
    };
    // allow user to check pointer via 'if (ptr) ...'
    operator bool() const
    {
        return ptr_obj_ != NULL;
    };
    // allow pass by value of encapsulated pointer
    operator RefCountedObject* const()
    {
        return get();
    };
    // allow manipulation of object
    RefCountedObject* operator->() const
    {
        return get();
    };
    RefCountedObject& operator*() const
    {
        return *get();
    };
    // allow user to pass us directly to ref counted object creation functions
    RefCountedObject** operator&()
    {
        safe_rel(ptr_obj_);
        return &ptr_obj_;
    };
    // release old and assign new obj ptr
    void reset(RefCountedObject* ptr_obj)
    {
        safe_rel(ptr_obj_);
        ptr_obj_ = ptr_obj;
    };
private:
    RefCountedObject* ptr_obj_;
    DISALLOW_COPY_AND_ASSIGN(auto_ref_counted_obj_ptr);
};

template <typename COMOBJ>
ULONG safe_rel(COMOBJ*& comobj)
{
    ULONG refcnt = 0;
    if (comobj)
    {
        refcnt = comobj->Release();
        comobj = NULL;
    }
    return refcnt;
}

} // WebmUtil namespace

#endif // __WEBMDSHOW_COMMON_MEMUTIL_HPP__