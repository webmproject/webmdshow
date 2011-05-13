#include <algorithm>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "scratchbuf.hpp"
#include "webmconstants.hpp"

namespace
{
    uint16 byteswap16(uint16 val)
    {
#if 0
        union {
          uint16 ui16;
          uint8 ui8[2];
        } original, swapped;
        original.ui16 = val;
        swapped.ui8[0] = original.ui8[1];
        swapped.ui8[1] = original.ui8[0];
        return swapped.ui16;
#else
        return (val >> 8) | (val << 8);
#endif
    }
    uint32 byteswap32(uint32 val)
    {
        return ((val >> 24) & 0xFF) |       // byte 3 to byte 0
               ((val << 24) & 0xFF000000) | // byte 0 to byte 3
               ((val << 8)  & 0xFF0000) |   // byte 1 to byte 2
               ((val >> 8)  & 0xFF00);      // byte 2 to byte 1
    }
    uint64 byteswap24(uint64 val)
    {
        return ((val >> 16) & 0xFF) |       // byte 2 to byte 0
               (((val >> 8) & 0xFF) << 8) | // copy byte 1
               ((val << 16) & 0xFF0000);    // byte 0 to byte 2
    }
    uint64 byteswap64(uint64 val)
    {
        // use a union to work with the uint64 as if it's 2 uint32's
        union {
            uint64 ui64;
            uint32 ui32[2];
        } original, swapped;

        original.ui64 = val;
        swapped.ui32[0] = byteswap32(original.ui32[1]);
        swapped.ui32[1] = byteswap32(original.ui32[0]);

        return swapped.ui64;
    }
}

TEST(ScratchTestUtil, ByteSwap16)
{
    uint16 original = 0xADDE;
    uint16 expected = 0xDEAD;
    uint16 test_val = byteswap16(original);
    ASSERT_EQ(expected, test_val);
}

TEST(ScratchTestUtil, ByteSwap24)
{
    uint64 original = 0xF0FECA;
    uint64 expected = 0xCAFEF0;
    uint64 test_val = byteswap24(original);
    ASSERT_EQ(expected, test_val);
}

TEST(ScratchTestUtil, ByteSwap32)
{
    uint32 original = 0xEFBEADDE;
    uint32 expected = 0xDEADBEEF;
    uint32 test_val = byteswap32(original);
    ASSERT_EQ(expected, test_val);
}

TEST(ScratchTestUtil, ByteSwap64)
{
    uint64 original = 0xEFBEADDE0DF0AD0B;
    uint64 expected = 0x0BADF00DDEADBEEF;
    uint64 test_val = byteswap64(original);
    ASSERT_EQ(expected, test_val);
}

TEST(ScratchBuf, LengthTests)
{
    using WebmUtil::ScratchBuf;
    ScratchBuf test_buf;

    test_buf.Write1UInt(kint8max);
    ASSERT_EQ(1, test_buf.GetBufferLength());
    test_buf.Reset();

    test_buf.Write2UInt(kint16max);
    ASSERT_EQ(2, test_buf.GetBufferLength());
    test_buf.Reset();

    test_buf.WriteUInt(0x00000FFF, 3);
    ASSERT_EQ(3, test_buf.GetBufferLength());
    test_buf.Reset();

    test_buf.Write4UInt(0xFFFF);
    ASSERT_EQ(4, test_buf.GetBufferLength());
    test_buf.Reset();

    test_buf.Write8UInt(kint64max);
    ASSERT_EQ(8, test_buf.GetBufferLength());
    test_buf.Reset();

    test_buf.Write4Float(1.1f);
    ASSERT_EQ(4, test_buf.GetBufferLength());
    test_buf.Reset();
}

TEST(ScratchBuf, ValueTests)
{
    using WebmUtil::ScratchBuf;
    ScratchBuf test_buf;

    test_buf.Write1UInt(kint8max);
    int8 test_val1 = 0;
    ::memcpy(&test_val1, test_buf.GetBufferPtr(), sizeof(int8));
    ASSERT_EQ(kint8max, test_val1);
    test_buf.Reset();

    test_buf.Write2UInt(kint16max);
    int16 test_val2 = 0;
    ::memcpy(&test_val2, test_buf.GetBufferPtr(), sizeof(int16));
    ASSERT_EQ(kint16max, test_val2);
    test_buf.Reset();

    const uint32 kuint24max = 0x00000FFF;
    test_buf.WriteUInt(kuint24max, 3);
    uint32 test_val3 = 0;
    ::memcpy(&test_val3, test_buf.GetBufferPtr(), 3);
    ASSERT_EQ(kuint24max, test_val3);
    test_buf.Reset();

    test_buf.Write4UInt(kint32max);
    int32 test_val4 = 0;
    ::memcpy(&test_val4, test_buf.GetBufferPtr(), sizeof(int32));
    ASSERT_EQ(kint32max, test_val4);
    test_buf.Reset();

    test_buf.Write8UInt(kint64max);
    int64 test_val5 = 0;
    ::memcpy(&test_val5, test_buf.GetBufferPtr(), sizeof(int64));
    ASSERT_EQ(kint64max, test_val5);
    test_buf.Reset();

    test_buf.Write4Float(1.1f);
    float test_val6 = 0;
    ::memcpy(&test_val6, test_buf.GetBufferPtr(), sizeof(float));
    ASSERT_EQ(1.1f, test_val6);
    test_buf.Reset();
}

TEST(ScratchBuf, StringTest)
{
    using WebmUtil::ScratchBuf;
    ScratchBuf test_buf;

    const char* const original_str = "webm";
    const char* const expected_str = "webm";
    const int32 test_len = 4;
    test_buf.Write1String(original_str);
    uint8 len_actual = 0, len_expected = test_len;
    // first byte should be the length of the string
    ::memcpy(&len_actual, test_buf.GetBufferPtr(), sizeof(uint8));
    ASSERT_EQ(len_expected, len_actual);
    // next four should be the string passed to |Write1String|
    const size_t compare_len = test_len;
    bool strings_match = ::memcmp(expected_str, test_buf.GetBufferPtr()+1,
                                  compare_len) == 0;
    ASSERT_TRUE(strings_match);
}

TEST(ScratchBuf, Utf8Test)
{
    using WebmUtil::ScratchBuf;
    ScratchBuf test_buf;

    const wchar_t* const original_str = L"webm";
    const char* const expected_str = "webm";
    const int32 test_len = 4;
    test_buf.Write1UTF8(original_str);
    uint8 len_actual = 0, len_expected = test_len;
    // first byte should be the length of the string
    ::memcpy(&len_actual, test_buf.GetBufferPtr(), sizeof(uint8));
    ASSERT_EQ(len_expected, len_actual);
    // next four should be the string passed to |Write1String|
    const size_t compare_len = test_len;
    bool strings_match = ::memcmp(expected_str, test_buf.GetBufferPtr()+1,
                                  compare_len) == 0;
    ASSERT_TRUE(strings_match);
}

bool ValidateScratchBufWStr(const WebmUtil::ScratchBuf* ptr_buf,
                            const wchar_t* ptr_str,
                            const int32 length)
{
    EXPECT_NE((WebmUtil::ScratchBuf*)NULL, ptr_buf);
    if (ptr_buf)
    {
        const wchar_t* read_ptr =
            reinterpret_cast<const wchar_t*>(ptr_buf->GetBufferPtr());
        using std::wstring;
        wstring test_wstr;
        for (int16 i = 0; i < length; ++i)
        {
            test_wstr.push_back(*read_ptr++);
        }
        EXPECT_STREQ(ptr_str, test_wstr.c_str());

        if (wcsncmp(ptr_str, test_wstr.c_str(), length) == 0)
        {
            return true;
        }
    }
    return false;
}

TEST(ScratchBuf, WriteTest)
{
    const wchar_t test_str[22] = L"I am your test fodder";

    const uint8* test_read_ptr = reinterpret_cast<const uint8*>(&test_str[0]);
    const int16 test_str_byte_count = arraysize(test_str) * 2;

    using WebmUtil::ScratchBuf;
    ScratchBuf test_buf;
    test_buf.Write(test_read_ptr, test_str_byte_count);

    const int64 scratch_buf_len = test_buf.GetBufferLength();
    // since we're using wchars, verify that bytes copied is an even number
    ASSERT_EQ(0, (scratch_buf_len % 2));
    // verify length is correct
    ASSERT_EQ(test_str_byte_count, scratch_buf_len);
    // validate buf contents
    ASSERT_TRUE(
        ValidateScratchBufWStr(&test_buf, &test_str[0], arraysize(test_str)));
}

TEST(ScratchBuf, RewriteTest)
{
    const wchar_t orig_str[22] = L"I am your test fodder";
    const wchar_t expected[22] = L" ate your test fodder";

    const uint8* orig_read_ptr = reinterpret_cast<const uint8*>(&orig_str[0]);
    const int16 orig_byte_count = arraysize(orig_str) * 2;

    using WebmUtil::ScratchBuf;
    ScratchBuf test_buf;
    test_buf.Write(orig_read_ptr, orig_byte_count);

    // validate buffer contents
    ASSERT_TRUE(
        ValidateScratchBufWStr(&test_buf, &orig_str[0], arraysize(orig_str)));

    const uint8* test_read_ptr2 = reinterpret_cast<const uint8*>(expected);
    const uint64 len_before_rewrite = test_buf.GetBufferLength();
    const size_t rewrite_offset = 0;
    test_buf.Rewrite(rewrite_offset, test_read_ptr2, sizeof(wchar_t)*4);
    ASSERT_EQ(len_before_rewrite, test_buf.GetBufferLength());

    // validate the contents post Rewrite
    ASSERT_TRUE(
        ValidateScratchBufWStr(&test_buf, &expected[0], arraysize(expected)));
}

TEST(ScratchBuf, EraseTest)
{
    using WebmUtil::ScratchBuf;
    ScratchBuf test_buf;

    // fill the buffer w/kEbmlVoid (0xEC)
    const int32 fodder_length = 100;
    test_buf.Fill(WebmUtil::kEbmlVoidID, fodder_length);

    // rewrite bytes just prior to the section we're going to erase
    const size_t rewrite_offset = 11;
    const uint8 rewrite_data[4] = {0x83, 0x9f, 0x42, 0x86};
    const int32 rewrite_length = 4;
    test_buf.Rewrite(rewrite_offset, &rewrite_data[0], rewrite_length);

    // erase some data
    const size_t erase_offset = rewrite_offset + rewrite_length;
    const int32 erase_length = 40;
    test_buf.Erase(erase_offset, erase_length);

    // sanity check: buffer length matches expectation
    ASSERT_EQ((fodder_length - erase_length), test_buf.GetBufferLength());

    // confirm the rewrite
    const uint8* ptr_buf = test_buf.GetBufferPtr();
    bool data_match = !::memcmp(ptr_buf + rewrite_offset, &rewrite_data[0],
                                rewrite_length);
    ASSERT_TRUE(data_match);

    // confirm the data after the rewritten section
    const uint8* ptr_orig_data = ptr_buf + rewrite_offset + rewrite_length;
    ASSERT_EQ(WebmUtil::kEbmlVoidID, *ptr_orig_data);
}

TEST(EbmlScratchBuf, ValueTests)
{
    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;

    const uint8 original_ui8 = 0x1F;
    const uint8 expected_ui8 = original_ui8 | 0x80;
    test_buf.Write1UInt(original_ui8);
    uint8 test_ui8 = 0;
    ::memcpy(&test_ui8, test_buf.GetBufferPtr(), sizeof(uint8));
    ASSERT_EQ(expected_ui8, test_ui8);
    test_buf.Reset();

    const uint16 original_ui16 = 0x00F0;
    const uint16 expected_ui16 = byteswap16(original_ui16 | 0x4000);
    test_buf.Write2UInt(original_ui16);
    uint16 test_ui16 = 0;
    ::memcpy(&test_ui16, test_buf.GetBufferPtr(), sizeof(uint16));
    ASSERT_EQ(expected_ui16, test_ui16);
    test_buf.Reset();

    // 3 byte unsigned int test
    const uint64 original_ui24 = 0x0F0000;
    const uint64 expected_ui24 = byteswap24(original_ui24 | 0x200000);
    test_buf.WriteUInt(original_ui24, 3);
    uint64 test_ui24 = 0;
    ::memcpy(&test_ui24, test_buf.GetBufferPtr(), 3);
    ASSERT_EQ(expected_ui24, test_ui24);
    test_buf.Reset();

    const uint32 original_ui32 = 0x0FB0ADDE;
    const uint32 expected_ui32 = byteswap32(original_ui32 | 0x10000000);
    test_buf.Write4UInt(original_ui32);
    uint32 test_ui32 = 0;
    ::memcpy(&test_ui32, test_buf.GetBufferPtr(), sizeof(uint32));
    ASSERT_EQ(expected_ui32, test_ui32);
    test_buf.Reset();

    const uint64 original_ui64 = 0x00F00DF0EFBEADDE;
    const uint64 expected_ui64 = byteswap64(original_ui64 | 0x0100000000000000);
    test_buf.Write8UInt(original_ui64);
    uint64 test_ui64 = 0;
    ::memcpy(&test_ui64, test_buf.GetBufferPtr(), sizeof(uint64));
    ASSERT_EQ(expected_ui64, test_ui64);
    test_buf.Reset();

    const float original_float = 1.101f;
    // EBML floats/ints are stored in big endian format, which means
    // |Write4Float| has to byte swap the value.  So, for a simple test
    // we write the float once, read it back, and then repeat the process
    // before ASSERT_EQ'ing.
    test_buf.Write4Float(original_float);
    float test_float = 0;
    ::memcpy(&test_float, test_buf.GetBufferPtr(), sizeof(float));
    test_buf.Reset();
    test_buf.Write4Float(test_float);
    ::memcpy(&test_float, test_buf.GetBufferPtr(), sizeof(float));
    ASSERT_EQ(original_float, test_float);
    test_buf.Reset();
}

TEST(EbmlScratchBuf, WriteTest)
{
    const wchar_t test_str[22] = L"I am your test fodder";

    const uint8* test_read_ptr = reinterpret_cast<const uint8*>(&test_str[0]);
    const int16 test_str_byte_count = arraysize(test_str) * 2;

    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;
    test_buf.Write(test_read_ptr, test_str_byte_count);

    const int64 scratch_buf_len = test_buf.GetBufferLength();
    // since we're using wchars, verify that bytes copied is an even number
    ASSERT_EQ(0, (scratch_buf_len % 2));
    // verify length is correct
    ASSERT_EQ(test_str_byte_count, scratch_buf_len);
    // validate buf contents
    ASSERT_TRUE(
        ValidateScratchBufWStr(&test_buf, &test_str[0], arraysize(test_str)));
}

TEST(EbmlScratchBuf, RewriteTest)
{
    const wchar_t orig_str[22] = L"I am your test fodder";
    const wchar_t expected[22] = L" ate your test fodder";

    const uint8* orig_read_ptr = reinterpret_cast<const uint8*>(&orig_str[0]);
    const int16 orig_byte_count = arraysize(orig_str) * 2;

    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;
    test_buf.Write(orig_read_ptr, orig_byte_count);

    // validate buffer contents
    ASSERT_TRUE(
        ValidateScratchBufWStr(&test_buf, &orig_str[0], arraysize(orig_str)));

    const uint8* test_read_ptr2 = reinterpret_cast<const uint8*>(expected);
    const uint64 len_before_rewrite = test_buf.GetBufferLength();
    const size_t rewrite_offset = 0;
    test_buf.Rewrite(rewrite_offset, test_read_ptr2, sizeof(wchar_t)*4);
    ASSERT_EQ(len_before_rewrite, test_buf.GetBufferLength());

    // validate the contents post Rewrite
    ASSERT_TRUE(
        ValidateScratchBufWStr(&test_buf, &expected[0], arraysize(expected)));
}

TEST(EbmlScratchBuf, Rewrite1UIntTest)
{
    uint8 original_bytes[5] = {0};
    uint8 write_val = 0x20;
    uint8 expected_bytes[5] = {0x0, 0x0, 0x0, write_val | 0x80, 0x0};
    size_t rewrite_offset = 3;

    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;
    test_buf.Write(&original_bytes[0], arraysize(original_bytes));
    test_buf.RewriteUInt(rewrite_offset, write_val, 1);

    int compare_result = memcmp(
        &expected_bytes[0], test_buf.GetBufferPtr(),
        static_cast<size_t>(test_buf.GetBufferLength()));
    ASSERT_EQ(arraysize(expected_bytes), test_buf.GetBufferLength());
    ASSERT_EQ(0, compare_result);
}

TEST(EbmlScratchBuf, Rewrite2UIntTest)
{
    const uint16 original_ui16 = 0x00F0;
    const uint16 expected_ui16 = byteswap16(original_ui16 | 0x4000);
    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;
    // write once
    test_buf.Write2UInt(original_ui16);
    uint16 test_ui16 = 0;
    ::memcpy(&test_ui16, test_buf.GetBufferPtr(), sizeof(uint16));
    // validate
    ASSERT_EQ(expected_ui16, test_ui16);
    // rewrite
    size_t rewrite_offset = 0;
    test_buf.RewriteUInt(rewrite_offset, original_ui16, sizeof(uint16));
    ::memcpy(&test_ui16, test_buf.GetBufferPtr(), sizeof(uint16));
    ASSERT_EQ(expected_ui16, test_ui16);
    test_buf.Reset();
}

TEST(EbmlScratchBuf, Rewrite3UIntTest)
{
    const uint64 original_ui24 = 0x0F0000;
    const uint64 expected_ui24 = byteswap24(original_ui24 | 0x200000);
    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;
    // write once
    test_buf.WriteUInt(original_ui24, 3);
    uint64 test_ui24 = 0;
    ::memcpy(&test_ui24, test_buf.GetBufferPtr(), 3);
    // validate
    ASSERT_EQ(expected_ui24, test_ui24);
    // rewrite
    size_t rewrite_offset = 0;
    test_buf.RewriteUInt(rewrite_offset, original_ui24, 3);
    ::memcpy(&test_ui24, test_buf.GetBufferPtr(), 3);
    ASSERT_EQ(expected_ui24, test_ui24);
    test_buf.Reset();
}

TEST(EbmlScratchBuf, Rewrite4UIntTest)
{
    const uint32 original_ui32 = 0x0DF0EDFE;
    const uint32 expected_ui32 = byteswap32(original_ui32 | 0x10000000);
    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;
    // write once
    test_buf.WriteUInt(original_ui32, sizeof(uint32));
    uint32 test_ui32 = 0;
    ::memcpy(&test_ui32, test_buf.GetBufferPtr(), sizeof(uint32));
    // validate
    ASSERT_EQ(expected_ui32, test_ui32);
    // rewrite
    size_t rewrite_offset = 0;
    test_buf.RewriteUInt(rewrite_offset, original_ui32, sizeof(uint32));
    ::memcpy(&test_ui32, test_buf.GetBufferPtr(), sizeof(uint32));
    ASSERT_EQ(expected_ui32, test_ui32);
    test_buf.Reset();
}

TEST(EbmlScratchBuf, Rewrite8UIntTest)
{
    const uint64 original_ui64 = 0x00F0EDFEEFBEADDE;
    const uint64 expected_ui64 = byteswap64(original_ui64 | 0x0100000000000000);
    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;
    // write once
    test_buf.WriteUInt(original_ui64, sizeof(uint64));
    uint64 test_ui64 = 0;
    ::memcpy(&test_ui64, test_buf.GetBufferPtr(), sizeof(uint64));
    // validate
    ASSERT_EQ(expected_ui64, test_ui64);
    // rewrite
    size_t rewrite_offset = 0;
    test_buf.RewriteUInt(rewrite_offset, original_ui64, sizeof(uint64));
    ::memcpy(&test_ui64, test_buf.GetBufferPtr(), sizeof(uint64));
    ASSERT_EQ(expected_ui64, test_ui64);
    test_buf.Reset();
}

TEST(EbmlScratchBuf, RewriteID1Test)
{
    const uint32 original_id = WebmUtil::kEbmlVoidID;
    const uint32 expected_id = WebmUtil::kEbmlVoidID;
    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;
    // write some 0's
    const uint8 fodder = WebmUtil::kEbmlCodecIDID;
    const int32 test_len = 32;
    for (int32 i = 0; i < test_len; ++i)
    {
        test_buf.WriteID1(fodder);
    }
    // basic sanity check: the buf length is correct, right?
    ASSERT_EQ(test_len, test_buf.GetBufferLength());

    // pick an offset for the rewrite
    const size_t offset = 10 * sizeof(uint8);

    // grab the buffer pointer, and read the original value back
    const uint8* ptr_buf = test_buf.GetBufferPtr();
    const uint8 test_val1 = *(ptr_buf + offset);
    ASSERT_EQ(fodder, test_val1);

    // rewrite
    test_buf.RewriteID(offset, original_id, sizeof(uint8));

    // grab the buffer pointer, and read the value back
    ptr_buf = test_buf.GetBufferPtr();
    const uint8 test_val2 = *(ptr_buf + offset);
    ASSERT_EQ(expected_id, test_val2);
}

TEST(EbmlScratchBuf, RewriteID2Test)
{
    const uint32 test_id = WebmUtil::kEbmlCodecPrivateID;
    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;
    // write some 0's
    const uint16 fodder = WebmUtil::kEbmlDocTypeID;
    const int32 test_len = 32;
    for (int32 i = 0; i < test_len / 2; ++i)
    {
        test_buf.WriteID2(fodder);
    }
    // basic sanity check: the buf length is correct, right?
    ASSERT_EQ(test_len, test_buf.GetBufferLength());

    // pick an offset for the rewrite
    const size_t offset = 7 * sizeof(uint16);

    // grab the buffer pointer from |test_buf|, and copy the original value
    const uint8* ptr_buf = test_buf.GetBufferPtr();
    uint16 test_val1 = 0;
    ::memcpy(&test_val1, (ptr_buf + offset), sizeof(uint16));
    test_val1 = byteswap16(test_val1);
    ASSERT_EQ(fodder, test_val1);

    // rewrite
    test_buf.RewriteID(offset, test_id, sizeof(uint16));

    // grab the buffer pointer, and read the value back
    ptr_buf = test_buf.GetBufferPtr();
    uint16 test_val2 = 0;
    ::memcpy(&test_val2, (ptr_buf + offset), sizeof(uint16));
    test_val2 = byteswap16(test_val2);

    ASSERT_EQ(test_id, test_val2);
}

TEST(EbmlScratchBuf, RewriteID3Test)
{
    const uint32 test_id = WebmUtil::kEbmlTimeCodeScaleID;
    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;
    // write some 0's
    const uint32 fodder = WebmUtil::kEbmlVideoFrameRate;
    const int32 test_len = 24 * 3; // 24 ui24 vals
    const int32 test_uint24_count = 24;
    for (int32 i = 0; i < test_uint24_count; ++i)
    {
        test_buf.WriteID3(fodder);
    }
    // basic sanity check: the buf length is correct, right?
    ASSERT_EQ(test_len, test_buf.GetBufferLength());

    // pick an offset for the rewrite
    const size_t offset = 45; // 15th ui24

    // grab the buffer pointer from |test_buf|, and copy the original value
    const uint8* ptr_buf = test_buf.GetBufferPtr();
    uint64 test_val1 = 0;
    ::memcpy(&test_val1, (ptr_buf + offset), 3);
    test_val1 = byteswap24(test_val1);
    ASSERT_EQ(fodder, test_val1);

    // rewrite
    test_buf.RewriteID(offset, test_id, 3);

    // grab the buffer pointer, and read the value back
    ptr_buf = test_buf.GetBufferPtr();
    uint64 test_val2 = 0;
    ::memcpy(&test_val2, (ptr_buf + offset), 3);
    test_val2 = byteswap24(test_val2);

    ASSERT_EQ(test_id, test_val2);
}

TEST(EbmlScratchBuf, RewriteID4Test)
{
    const uint32 test_id = WebmUtil::kEbmlSeekHeadID;
    using WebmUtil::EbmlScratchBuf;
    EbmlScratchBuf test_buf;
    // write some 0's
    const uint32 fodder = WebmUtil::kEbmlCuesID;
    const int32 test_len = 128;
    for (int32 i = 0; i < 128 / 4; ++i)
    {
        test_buf.WriteID4(fodder);
    }
    // basic sanity check: the buf length is correct, right?
    ASSERT_EQ(test_len, test_buf.GetBufferLength());

    // pick an offset for the rewrite
    const size_t offset = 21 * sizeof(uint32);

    // grab the buffer pointer from |test_buf|, and copy the original value
    const uint8* ptr_buf = test_buf.GetBufferPtr();
    uint32 test_val1 = 0;
    ::memcpy(&test_val1, (ptr_buf + offset), sizeof(uint32));
    test_val1 = byteswap32(test_val1);
    ASSERT_EQ(fodder, test_val1);

    // rewrite
    test_buf.RewriteID(offset, test_id, sizeof(uint32));

    // grab the buffer pointer, and read the value back
    ptr_buf = test_buf.GetBufferPtr();
    uint32 test_val2 = 0;
    ::memcpy(&test_val2, (ptr_buf + offset), sizeof(uint32));
    test_val2 = byteswap32(test_val2);

    ASSERT_EQ(test_id, test_val2);
}
