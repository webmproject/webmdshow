// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once

namespace MediaTypeUtil
{
    HRESULT Copy(const AM_MEDIA_TYPE& src, AM_MEDIA_TYPE& tgt);
    void Destroy(AM_MEDIA_TYPE&);

    HRESULT Create(const AM_MEDIA_TYPE& src, AM_MEDIA_TYPE*& ptgt);
    void Free(AM_MEDIA_TYPE*);

}  //end namespace MediaTypeUtil



