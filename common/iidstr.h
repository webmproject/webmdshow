// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include <iosfwd>

class IIDStr
{
public:
    IIDStr(const IID&);
    const IID& m_iid;
private:
    IIDStr(const IIDStr&);
    IIDStr& operator=(const IIDStr&);
};

std::wostream& operator<<(std::wostream&, const IIDStr&);

inline IIDStr::IIDStr(const IID& iid) : m_iid(iid)
{
}
