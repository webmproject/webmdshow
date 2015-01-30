// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_OMAHAUTIL_HPP__
#define __WEBMDSHOW_COMMON_OMAHAUTIL_HPP__

//#include <comdef.h>
#include <objbase.h>

namespace WebmUtil
{

// Super simple public interface... just give us the App ID GUID
HRESULT set_omaha_usage_flags(const GUID& app_id);

} // WebmUtil namespace

#endif // __WEBMDSHOW_COMMON_OMAHAUTIL_HPP__
