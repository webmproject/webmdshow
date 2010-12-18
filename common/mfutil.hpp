// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_MFUTIL_HPP__
#define __WEBMDSHOW_COMMON_MFUTIL_HPP__

namespace WebmMfUtil
{

HRESULT get_major_type(IMFStreamDescriptor* ptr_desc, GUID* ptr_type);

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_MFUTIL_HPP__
