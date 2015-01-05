// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "libyuv_util.h"

#include <cassert>

#include "libyuv.h"

namespace webmdshow {

bool LibyuvScaleI420(uint32_t width, uint32_t height,
                     const vpx_image_t* source, vpx_image_t** target_image) {
  if (source->fmt != VPX_IMG_FMT_I420 && source->fmt != VPX_IMG_FMT_YV12) {
    assert(source->fmt == VPX_IMG_FMT_I420 || source->fmt == VPX_IMG_FMT_YV12);
    return false;
  }

  vpx_image_t* target = *target_image;
  if (target != NULL && (target->d_h != height || target->d_w != width)) {
    // The libvpx output image size changed; realloc needed.
    vpx_img_free(target);
    target = NULL;
  }

  if (target == NULL) {
    target = vpx_img_alloc(NULL, source->fmt, width, height, 16);
    if (target == NULL) {
      assert(target && "Out of memory.");
      return false;
    }
  }

  const int scale_status = libyuv::I420Scale(
      source->planes[VPX_PLANE_Y], source->stride[VPX_PLANE_Y],
      source->planes[VPX_PLANE_U], source->stride[VPX_PLANE_U],
      source->planes[VPX_PLANE_V], source->stride[VPX_PLANE_V],
      source->d_w, source->d_h,
      target->planes[VPX_PLANE_Y], target->stride[VPX_PLANE_Y],
      target->planes[VPX_PLANE_U], target->stride[VPX_PLANE_U],
      target->planes[VPX_PLANE_V], target->stride[VPX_PLANE_V],
      target->d_w, target->d_h,
      libyuv::kFilterBox);
  if (scale_status != 0) {
    assert(scale_status == 0 && "libyuv::I420Scale failed.");
    return false;
  }

  *target_image = target;
  return true;
}

}  // namespace webmdshow