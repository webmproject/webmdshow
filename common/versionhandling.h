// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef VERSIONHANDLING_HPP
#define VERSIONHANDLING_HPP

#include <iosfwd>

namespace VersionHandling
{
    void GetVersion(
            const wchar_t*,
            WORD& major,
            WORD& minor,
            WORD& revision,
            WORD& build);

    void GetVersion(const wchar_t*, std::wostream&);
}

#endif
