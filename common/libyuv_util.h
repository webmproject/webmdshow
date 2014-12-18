// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBMDSHOW_COMMON_LIBYUV_UTIL_H_
#define WEBMDSHOW_COMMON_LIBYUV_UTIL_H_

#include <stdint.h>

#include "vpx/vpx_image.h"

namespace webmdshow {

// Scales |source| to |width|x|height|. |source| must be VPX_IMG_FMT_I420 or
// VPX_IMG_FMT_YV12. |target| will be allocated if necessary. Caller owns
// any allocated memory. Returns true upon success.
bool LibyuvScaleI420(uint32_t width, uint32_t height,
                     const vpx_image_t* source, vpx_image_t** target);

}  // namespace webmdshow

#endif  // WEBMDSHOW_COMMON_LIBYUV_UTIL_H_
