// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <cassert>
#include <new>

#include <windows.h>
#include <windowsx.h>
#include <mfplay.h>
#include <mferror.h>

#include "debugutil.h"
#include "mfplayerutil.h"

namespace WebmMfUtil
{

HRESULT MfPlayerCallback::CreateInstance(MfPlayerCallbackFunc callback_func,
                                         UINT_PTR user_data,
                                         MfPlayerCallback** ptr_instance)
{
    if (!callback_func)
        return E_INVALIDARG;

    *ptr_instance = new MfPlayerCallback(callback_func, user_data);
    assert(*ptr_instance);
    
    if (!*ptr_instance)
        return E_OUTOFMEMORY;

    return S_OK;
}

MfPlayerCallback::MfPlayerCallback(MfPlayerCallbackFunc callback_func,
                                   UINT_PTR callback_data):
  callback_data_(callback_data),
  callback_func_(callback_func)
{
    AddRef();
}

MfPlayerCallback::~MfPlayerCallback()
{
    callback_func_ = NULL;
    callback_data_ = 0;
}

UINT MfPlayerCallback::AddRef()
{
    return InterlockedIncrement(&ref_count_);
}

UINT MfPlayerCallback::Release()
{
    UINT ref_count = InterlockedDecrement(&ref_count_);
    if (ref_count == 0)
        delete this;
    return ref_count;
}

void MfPlayerCallback::OnPlayerStateChange(int new_state)
{
    if (callback_func_)
        callback_func_(callback_data_, new_state);
}

} // WebmMfUtil namespace