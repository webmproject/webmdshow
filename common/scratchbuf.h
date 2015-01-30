// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_SCRATCHBUF_HPP__
#define __WEBMDSHOW_COMMON_SCRATCHBUF_HPP__

#pragma once

#include <vector>

#include "chromium/base/basictypes.h"

namespace WebmUtil
{

class ScratchBuf
{
public:
    ScratchBuf();
    virtual ~ScratchBuf();

    int32 Fill(uint8 val, int32 length);

    int32 Erase(uint32 offset, int32 length);
    int32 Erase(uint64 offset, int32 length);

    int32 Rewrite(uint32 offset, const uint8* ptr_data, int32 length);
    int32 Rewrite(uint64 offset, const uint8* ptr_data, int32 length);

    void Write(const uint8* ptr_data, int32 length);
    void Write4Float(float val);
    void Write1String(const char* ptr_str);
    void Write1UTF8(const wchar_t* ptr_str);

    virtual void Write8UInt(uint64 val);
    virtual void Write4UInt(uint32 val);
    virtual void Write2UInt(uint16 val);
    virtual void Write1UInt(uint8 val);
    virtual void WriteUInt(uint64 val, int32 size);

    const uint8* GetBufferPtr() const;
    uint64 GetBufferLength() const;

    void Reset();

protected:
    std::vector<uint8> buf_;

private:
    DISALLOW_COPY_AND_ASSIGN(ScratchBuf);
};

class EbmlScratchBuf : public ScratchBuf
{
public:
    EbmlScratchBuf();
    virtual ~EbmlScratchBuf();

    void Serialize8UInt(uint64 val);
    void Serialize4Float(float val);
    void Serialize4UInt(uint32 val);
    void Serialize2UInt(uint16 val);
    void Serialize1UInt(uint8 val);

    int32 RewriteID(uint32 offset, uint32 id, int32 length);
    int32 RewriteID(uint64 offset, uint32 id, int32 length);
    int32 RewriteUInt(uint32 offset, uint64 val, int32 length);
    int32 RewriteUInt(uint64 offset, uint64 val, int32 length);

    virtual void Write8UInt(uint64 val);
    virtual void Write4UInt(uint32 val);
    virtual void Write2UInt(uint16 val);
    virtual void Write1UInt(uint8 val);
    virtual void WriteUInt(uint64 val, int32 size);

    void WriteID4(uint32 id);
    void WriteID3(uint32 id);
    void WriteID2(uint16 id);
    void WriteID1(uint8 id);
private:
    DISALLOW_COPY_AND_ASSIGN(EbmlScratchBuf);
};

} // WebmUtil namespace

#endif // __WEBMDSHOW_COMMON_SCRATCHBUF_HPP__
