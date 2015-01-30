// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <Windows.h> // TODO(tomfinegan): make Windows.h include conditional

#include <cassert>

#include "memutil.h"
#include "scratchbuf.h"
#include "webmconstants.h"

namespace WebmUtil
{

template <typename Val>
void SerializeNum(std::vector<uint8>& buf_, const Val* ptr, int32 size)
{
    assert(ptr);

    const uint8* read_ptr = reinterpret_cast<const uint8*>(ptr);

    for (int32 i = 0; i < size; ++i)
    {
        buf_.push_back(*read_ptr++);
    }
}

template <typename Val>
void ByteSwapAndSerializeNum(std::vector<uint8>& buf_, const Val* ptr,
                             int32 size)
{
    assert(ptr);

    const uint8* read_ptr = reinterpret_cast<const uint8*>(ptr);

    read_ptr += size - 1;

    for (int32 i = 0; i < size; ++i)
    {
        buf_.push_back(*read_ptr--);
    }
}

template <typename Val>
void EbmlSerializeNum(std::vector<uint8>& buf_, const Val* ptr, int32 size)
{
    assert(ptr);

    const int64 val = static_cast<const int64>(*ptr);
    for (int32 i = 1; i <= size; ++i)
    {
        // ebml is big endian, convert
        const int32 byte_count = size - i;
        const int32 bit_count = byte_count * 8;

        const int64 shifted_val = val >> bit_count;
        const uint8 byte = static_cast<uint8>(shifted_val);

        buf_.push_back(byte);
    }
}

template <typename Val>
void EbmlSerializeNumAtOffset(std::vector<uint8>& buf_, const Val* ptr,
                              int32 size, uint32 offset)
{
    assert(ptr);
    assert(offset + size <= buf_.size());

    uint8* write_ptr = &buf_[offset];

    const int64 val = static_cast<const int64>(*ptr);
    for (int32 i = 1; i <= size; ++i)
    {
        // ebml is big endian, convert
        const int32 byte_count = size - i;
        const int32 bit_count = byte_count * 8;

        const int64 shifted_val = val >> bit_count;
        const uint8 byte = static_cast<uint8>(shifted_val);

        *write_ptr++ = byte;
    }
}

void EbmlSerializeFloat(std::vector<uint8>& buf_, float val)
{
    const uint32* val_ui32 = reinterpret_cast<const uint32*>(&val);
    EbmlSerializeNum(buf_, val_ui32, sizeof(uint32));
}

}

WebmUtil::ScratchBuf::ScratchBuf()
{
}

WebmUtil::ScratchBuf::~ScratchBuf()
{
}

int WebmUtil::ScratchBuf::Fill(uint8 val, int32 length)
{
    for (int32 i = 0; i < length; ++i)
    {
        buf_.push_back(val);
    }
    return length;
}

int WebmUtil::ScratchBuf::Erase(uint32 offset, int32 length)
{
    // no erasing past the end!
    assert(buf_.size() >= (offset + length));
    // erase range using iterators
    typedef std::vector<uint8>::iterator viter_t;
    viter_t start_ptr = buf_.begin() + offset;
    viter_t end_ptr = start_ptr + length;
    buf_.erase(start_ptr, end_ptr);
    // return the new size of the buffer
    return static_cast<int>(buf_.size());
}

int WebmUtil::ScratchBuf::Erase(uint64 offset, int32 length)
{
    return Erase(static_cast<uint32>(offset), length);
}

int WebmUtil::ScratchBuf::Rewrite(uint32 offset, const uint8* read_ptr,
                                  int32 length)
{
    assert(read_ptr);
    assert(offset <= buf_.size());
    assert(offset + length <= buf_.size());
    typedef std::vector<uint8>::iterator BufIterator;

    BufIterator write_ptr = buf_.begin() + offset;

    int num_bytes = 0;
    for (; num_bytes < length; ++num_bytes)
    {
        *write_ptr++ = *read_ptr++;
    }
    return num_bytes;
}

int WebmUtil::ScratchBuf::Rewrite(uint64 offset, const uint8* read_ptr,
                                  int32 length)
{
    return Rewrite(static_cast<uint32>(offset), read_ptr, length);
}

void WebmUtil::ScratchBuf::Write(const uint8* read_ptr, int32 length)
{
    int64 slen = length;
    for (int64 i = 0; i < slen; ++i)
    {
        buf_.push_back(*read_ptr++);
    }
}

void WebmUtil::ScratchBuf::Write4Float(float val)
{
    SerializeNum(buf_, &val, sizeof(float));
}

void WebmUtil::ScratchBuf::Write1String(const char* ptr_str)
{
    if (ptr_str)
    {
        const size_t size_ = strlen(ptr_str);
        assert(size_ <= 255);

        const uint8 size = static_cast<uint8>(size_);

        const uint8* ptr_uint = reinterpret_cast<const uint8*>(ptr_str);

        Write1UInt(size);
        Write(ptr_uint, size);
    }
}

void WebmUtil::ScratchBuf::Write1UTF8(const wchar_t* ptr_str)
{
    if (ptr_str)
    {
        const int32 utf8_byte_count =
            WideCharToMultiByte(CP_UTF8,
                                0,   // flags, 0 for UTF-8 conversion
                                ptr_str,
                                -1,  // assume NULL-terminated/calculate length
                                0,   // buf, NULL means calculate req'd storage
                                0,   // count
                                0,
                                0);

        assert(utf8_byte_count > 0);

        if (utf8_byte_count > 0)
        {
            using WebmUtil::auto_array;
            auto_array<uint8> buf (new uint8[utf8_byte_count],
                                   utf8_byte_count);

            const int32 bytes_converted =
                WideCharToMultiByte(CP_UTF8,
                                    0,
                                    ptr_str,
                                    -1,
                                    reinterpret_cast<char*>(buf.get()),
                                    utf8_byte_count,
                                    0,
                                    0);

            assert(bytes_converted == utf8_byte_count);
            assert(bytes_converted > 0);

            const int32 bytes_to_write = bytes_converted - 1; // exclude the \0
            assert(bytes_to_write <= 255);

            const uint8 str_size = static_cast<uint8>(bytes_to_write);
            Write1UInt(str_size);
            Write(buf, str_size);
        }
    }
}

void WebmUtil::ScratchBuf::Write8UInt(uint64 val)
{
    SerializeNum(buf_, &val, sizeof(uint64));
}

void WebmUtil::ScratchBuf::Write4UInt(uint32 val)
{
    SerializeNum(buf_, &val, sizeof(uint32));
}

void WebmUtil::ScratchBuf::Write2UInt(uint16 val)
{
    SerializeNum(buf_, &val, sizeof(uint16));
}

void WebmUtil::ScratchBuf::Write1UInt(uint8 val)
{
    SerializeNum(buf_, &val, sizeof(uint8));
}

void WebmUtil::ScratchBuf::WriteUInt(uint64 val, int32 size)
{
    SerializeNum(buf_, &val, size);
}

const uint8* WebmUtil::ScratchBuf::GetBufferPtr() const
{
    return &buf_[0];
}

uint64 WebmUtil::ScratchBuf::GetBufferLength() const
{
    return buf_.size();
}

void WebmUtil::ScratchBuf::Reset()
{
    buf_.clear();
}

WebmUtil::EbmlScratchBuf::EbmlScratchBuf()
{
}

WebmUtil::EbmlScratchBuf::~EbmlScratchBuf()
{
}

void WebmUtil::EbmlScratchBuf::Serialize8UInt(uint64 val)
{
    ByteSwapAndSerializeNum(buf_, &val, sizeof(uint64));
}


void WebmUtil::EbmlScratchBuf::Serialize4UInt(uint32 val)
{
    ByteSwapAndSerializeNum(buf_, &val, sizeof(uint32));
}


void WebmUtil::EbmlScratchBuf::Serialize2UInt(uint16 val)
{
    ByteSwapAndSerializeNum(buf_, &val, sizeof(uint16));
}


void WebmUtil::EbmlScratchBuf::Serialize1UInt(uint8 val)
{
    ByteSwapAndSerializeNum(buf_, &val, sizeof(uint8));
}

int WebmUtil::EbmlScratchBuf::RewriteID(uint32 offset, uint32 val, int32 size)
{
    assert(size > 0 && size <= 4);
    assert(offset + size <= buf_.size());

    switch (size)
    {
    case 1:
        assert(val <= WebmUtil::kEbmlMaxID1);
        break;
    case 2:
        assert(val <= WebmUtil::kEbmlMaxID2);
        break;
    case 3:
        assert(val <= WebmUtil::kEbmlMaxID3);
        break;
    case 4:
        assert(val <= WebmUtil::kEbmlMaxID4);
        break;
    default:
        assert(0);
    }

    EbmlSerializeNumAtOffset(buf_, &val, size, offset);
    return size;
}

int WebmUtil::EbmlScratchBuf::RewriteID(uint64 offset, uint32 val,
                                        int32 length)
{
    return RewriteID(static_cast<uint32>(offset), val, length);
}

int WebmUtil::EbmlScratchBuf::RewriteUInt(uint32 offset, uint64 val,
                                          int32 size)
{
    if (size > 0)
    {
        const uint64 bits = 1UI64 << (size * 7);
        assert(val <= (bits - 2));

        val |= bits;
        EbmlSerializeNumAtOffset(buf_, &val, size, offset);
    }
    else
    {
        size = 1;
        LONGLONG bit;

        for (;;)
        {
            bit = 1UI64 << (size * 7);
            const uint64 max = bit - 2;

            if (val <= max)
                break;

            ++size;
        }

        assert(size <= 8);
        val |= bit;

        EbmlSerializeNumAtOffset(buf_, &val, size, offset);
    }

    return size;
}

int WebmUtil::EbmlScratchBuf::RewriteUInt(uint64 offset, uint64 val,
                                          int32 length)
{
    return RewriteUInt(static_cast<uint32>(offset), val, length);
}

void WebmUtil::EbmlScratchBuf::Serialize4Float(float val)
{
    EbmlSerializeFloat(buf_, val);
}

void WebmUtil::EbmlScratchBuf::Write8UInt(uint64 val)
{
    assert(val <= 0x00FFFFFFFFFFFFFE);  // 0000 000x 1111 1111 ...
    val |= 0x0100000000000000;          // always write 8 bytes
    EbmlSerializeNum(buf_, &val, sizeof(uint64));
}

void WebmUtil::EbmlScratchBuf::Write4UInt(uint32 val)
{
    assert(val <= 0x0FFFFFFE);  // 000x 1111 1111 ...
    val |= 0x10000000;  // always write 4 bytes
    EbmlSerializeNum(buf_, &val, sizeof(uint32));
}

void WebmUtil::EbmlScratchBuf::Write2UInt(uint16 val)
{
    assert(val <= 0x3FFE);  // 0x11 1111 1111 1110
    val |= 0x4000;          // always write 2 bytes
    EbmlSerializeNum(buf_, &val, sizeof(uint16));
}

void WebmUtil::EbmlScratchBuf::Write1UInt(uint8 val)
{
    assert(val <= 0x7E);  // x111 1110
    val |= 0x80;          // always write 1 byte
    EbmlSerializeNum(buf_, &val, sizeof(uint8));
}

void WebmUtil::EbmlScratchBuf::WriteUInt(uint64 val, int32 size)
{
    if (size > 0)
    {
        const uint64 bits = 1UI64 << (size * 7);
        assert(val <= (bits - 2));

        val |= bits;
        EbmlSerializeNum(buf_, &val, size);
    }
    else
    {
        size = 1;
        LONGLONG bit;

        for (;;)
        {
            bit = 1UI64 << (size * 7);
            const uint64 max = bit - 2;

            if (val <= max)
                break;

            ++size;
        }

        assert(size <= 8);
        val |= bit;

        EbmlSerializeNum(buf_, &val, size);
    }
}

void WebmUtil::EbmlScratchBuf::WriteID4(uint32 id)
{
    assert(id & 0x10000000);  // always write 4 bytes
    assert(id <= 0x1FFFFFFE);
    EbmlSerializeNum(buf_, &id, sizeof(uint32));
}

void WebmUtil::EbmlScratchBuf::WriteID3(uint32 id)
{
    assert(id & 0x200000);  //always write 3 bytes
    assert(id <= 0x3FFFFE);
    EbmlSerializeNum(buf_, &id, 3);
}

void WebmUtil::EbmlScratchBuf::WriteID2(uint16 id)
{
    assert(id & 0x4000);  // always write 2 bytes
    assert(id <= 0x7FFE);
    EbmlSerializeNum(buf_, &id, sizeof(uint16));
}

void WebmUtil::EbmlScratchBuf::WriteID1(uint8 id)
{
    assert(id & 0x80);  // always write 1 byte
    assert(id <= 0xFE);
    EbmlSerializeNum(buf_, &id, sizeof(uint8));
}
