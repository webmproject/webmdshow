// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once

[
    uuid(ED3110FA-5211-11DF-94AF-0026B977EEAA)
]
interface IVP8Sample : IUnknown
{

    struct Frame
    {
        BYTE* buf;
        long buflen;
        long off;
        long len;  //length of payload
        bool key;
        REFERENCE_TIME start;
        REFERENCE_TIME stop;
    };

    virtual Frame& GetFrame() = 0;

};
