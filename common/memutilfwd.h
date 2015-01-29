// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_MEMUTILFWD_HPP__
#define __WEBMDSHOW_COMMON_MEMUTILFWD_HPP__

namespace WebmUtil
{

template<typename BufferType> class auto_array;
template<typename RefCountedObj> class auto_ref_counted_obj_ptr;
template<typename COMOBJ> ULONG safe_rel(COMOBJ*& comobj);

}

#endif // __WEBMDSHOW_COMMON_MEMUTILFWD_HPP__
