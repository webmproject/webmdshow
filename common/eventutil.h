// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_EVENTUTIL_HPP__
#define __WEBMDSHOW_COMMON_EVENTUTIL_HPP__

namespace WebmMfUtil
{

class EventWaiter
{
public:
    EventWaiter();
    ~EventWaiter();
    HRESULT Create();
    HRESULT Set();
    HRESULT Wait();
    HRESULT MessageWait();
    HRESULT ZeroWait();
private:
    HANDLE event_handle_;
    DISALLOW_COPY_AND_ASSIGN(EventWaiter);
};

HRESULT infinite_cowait(HANDLE hndl);
HRESULT zero_cowait(HANDLE hndl);

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_EVENTUTIL_HPP__
