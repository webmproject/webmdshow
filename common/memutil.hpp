// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_MEMUTIL_HPP__
#define __WEBMDSHOW_COMMON_MEMUTIL_HPP__

#include "debugutil.hpp"

namespace WebmUtil
{

template <typename BufferType>
class auto_array
{
public:
    explicit auto_array(BufferType* ptr_buf, size_t count) :
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

} // WebmUtil namespace

#endif // __WEBMDSHOW_COMMON_MEMUTIL_HPP__