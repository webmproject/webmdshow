// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_MFPLAYERUTIL_HPP__
#define __WEBMDSHOW_COMMON_MFPLAYERUTIL_HPP__

namespace WebmMfUtil
{

typedef void(*MfPlayerCallbackFunc)(UINT_PTR callback_data, int new_state);

class MfPlayerCallback
{
public:
    static HRESULT CreateInstance(MfPlayerCallbackFunc callback_fn, 
                                  UINT_PTR callback_data, 
                                  MfPlayerCallback** ptr_instance);
    ~MfPlayerCallback();
    UINT AddRef();
    UINT Release();
    void OnPlayerStateChange(int new_state);
private:
    explicit MfPlayerCallback(MfPlayerCallbackFunc callback_fn,
                              UINT_PTR user_data);
    MfPlayerCallbackFunc callback_func_;
    UINT ref_count_;
    UINT_PTR callback_data_;
    DISALLOW_COPY_AND_ASSIGN(MfPlayerCallback);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_MFPLAYERUTIL_HPP__