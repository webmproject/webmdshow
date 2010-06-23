// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "webmmuxebmlio.hpp"
#include <cassert>
#include <limits>
#include <malloc.h>  //_malloca


EbmlIO::File::File() : m_pStream(0)
{
}


EbmlIO::File::~File()
{
    assert(m_pStream == 0);
}


void EbmlIO::File::SetStream(IStream* p)
{
    assert((m_pStream == 0) || (p == 0));
    m_pStream = p;
}


IStream* EbmlIO::File::GetStream() const
{
    return m_pStream;
}


HRESULT EbmlIO::File::SetSize(__int64 size)
{
    return EbmlIO::SetSize(m_pStream, size);
}


__int64 EbmlIO::File::SetPosition(
    __int64 pos,
    STREAM_SEEK origin)
{
    return EbmlIO::SetPosition(m_pStream, pos, origin);
}


__int64 EbmlIO::File::GetPosition() const
{
    File* const const_file = const_cast<File*>(this);
    return const_file->SetPosition(0, STREAM_SEEK_CUR);
}



//void EbmlIO::File::Serialize(
//    const void* buf,
//    ULONG len)
//{
//    EbmlIO::Serialize(m_pStream, buf, len);
//}


void EbmlIO::File::Write(const void* buf, ULONG cb)
{
    EbmlIO::Write(m_pStream, buf, cb);
}


void EbmlIO::File::Serialize8UInt(__int64 val)
{
    EbmlIO::Serialize(m_pStream, &val, 8);
}



void EbmlIO::File::Serialize4UInt(ULONG val)
{
    EbmlIO::Serialize(m_pStream, &val, 4);
}


void EbmlIO::File::Serialize2UInt(USHORT val)
{
    EbmlIO::Serialize(m_pStream, &val, 2);
}


void EbmlIO::File::Serialize1UInt(BYTE val)
{
    EbmlIO::Serialize(m_pStream, &val, 1);
}


void EbmlIO::File::Serialize2SInt(SHORT val)
{
    EbmlIO::Serialize(m_pStream, &val, 2);
}


void EbmlIO::File::Serialize4Float(float val)
{
    EbmlIO::Serialize(m_pStream, &val, 4);
}


void EbmlIO::File::WriteID4(ULONG id)
{
    EbmlIO::WriteID4(m_pStream, id);
}


void EbmlIO::File::WriteID3(ULONG id)
{
    EbmlIO::WriteID3(m_pStream, id);
}


void EbmlIO::File::WriteID2(USHORT id)
{
    EbmlIO::WriteID2(m_pStream, id);
}


void EbmlIO::File::WriteID1(BYTE id)
{
    EbmlIO::WriteID1(m_pStream, id);
}


ULONG EbmlIO::File::ReadID4()
{
    return EbmlIO::ReadID4(m_pStream);
}


void EbmlIO::File::Write8UInt(__int64 val)
{
    EbmlIO::Write8UInt(m_pStream, val);
}


void EbmlIO::File::Write4UInt(ULONG val)
{
    EbmlIO::Write4UInt(m_pStream, val);
}


void EbmlIO::File::Write2UInt(USHORT val)
{
    EbmlIO::Write2UInt(m_pStream, val);
}


void EbmlIO::File::Write1UInt(BYTE val)
{
    EbmlIO::Write1UInt(m_pStream, val);
}


void EbmlIO::File::Write1String(const char* str)
{
    EbmlIO::Write1String(m_pStream, str);
}


//void EbmlIO::File::Write1String(const char* str, size_t len)
//{
//    EbmlIO::Write1String(m_pStream, str, len);
//}


void EbmlIO::File::Write1UTF8(const wchar_t* str)
{
    EbmlIO::Write1UTF8(m_pStream, str);
}


HRESULT EbmlIO::SetSize(IStream* pStream, __int64 size_)
{
    assert(pStream);
    assert(size_ >= 0);

    ULARGE_INTEGER size;
    size.QuadPart = size_;

    return pStream->SetSize(size);
}


__int64 EbmlIO::SetPosition(
    IStream* pStream,
    __int64 move_,
    STREAM_SEEK origin)
{
    assert(pStream);

    LARGE_INTEGER move;
    move.QuadPart = move_;

    ULARGE_INTEGER newpos;

    const HRESULT hr = pStream->Seek(move, origin, &newpos);
    assert(SUCCEEDED(hr));
    hr;

    return newpos.QuadPart;
}


void EbmlIO::Write(
    ISequentialStream* pStream,
    const void* buf,
    ULONG cb)
{
    assert(pStream);

    ULONG cbWritten;

    const HRESULT hr = pStream->Write(buf, cb, &cbWritten);
    assert(SUCCEEDED(hr));
    assert(cbWritten == cb);
    hr;
}



void EbmlIO::Write8UInt(ISequentialStream* pStream, __int64 val)
{
    assert(val <= 0x00FFFFFFFFFFFFFE);  //0000 000x 1111 1111 ...
    val |= 0x0100000000000000;          //always write 8 bytes

    Serialize(pStream, &val, 8);
}


void EbmlIO::Write4UInt(ISequentialStream* pStream, ULONG val)
{
    assert(val <= 0x0FFFFFFE);  //000x 1111 1111 ...
    val |= 0x10000000;  //always write 4 bytes

    Serialize(pStream, &val, 4);
}


void EbmlIO::Write2UInt(ISequentialStream* pStream, USHORT val)
{
    assert(val <= 0x3FFE);  //0x11 1111 1111 1110
    val |= 0x4000;          //always write 2 bytes

    Serialize(pStream, &val, 2);
}


void EbmlIO::Write1UInt(ISequentialStream* pStream, BYTE val)
{
    assert(val <= 0x7E);  //x111 1110
    val |= 0x80;          //always write 1 byte

    Serialize(pStream, &val, 1);
}


void EbmlIO::WriteID4(ISequentialStream* pStream, ULONG id)
{
    assert(pStream);
    assert(id & 0x10000000);  //always write 4 bytes
    assert(id <= 0x1FFFFFFE);

    Serialize(pStream, &id, 4);
}


ULONG EbmlIO::ReadID4(ISequentialStream* pStream)
{
    assert(pStream);

    ULONG id;

    BYTE* const p = reinterpret_cast<BYTE*>(&id);
    BYTE* q = p + 4;

    for (;;)
    {
        ULONG cb;

        const HRESULT hr = pStream->Read(--q, 1, &cb);
        assert(hr == S_OK);
        assert(cb == 1);
        hr;

        if (q == p)
            break;
    }

    assert(id & 0x10000000);
    assert(id <= 0x1FFFFFFE);

    return id;
}


void EbmlIO::WriteID3(ISequentialStream* pStream, ULONG id)
{
    assert(pStream);
    assert(id & 0x200000);  //always write 3 bytes
    assert(id <= 0x3FFFFE);

    Serialize(pStream, &id, 3);
}


void EbmlIO::WriteID2(ISequentialStream* pStream, USHORT id)
{
    assert(pStream);
    assert(id & 0x4000);  //always write 2 bytes
    assert(id <= 0x7FFE);

    Serialize(pStream, &id, 2);
}


void EbmlIO::WriteID1(ISequentialStream* pStream, BYTE id)
{
    assert(pStream);
    assert(id & 0x80);  //always write 1 byte
    assert(id <= 0xFE);

    Serialize(pStream, &id, 1);
}


void EbmlIO::Write1String(
    ISequentialStream* pStream,
    const char* str)
{
    assert(pStream);
    assert(str);

    const size_t size_ = strlen(str);
    assert(size_ <= 255);

    const BYTE size = static_cast<BYTE>(size_);

    Write1UInt(pStream, size);
    Write(pStream, str, size);
}


#if 0
void EbmlIO::Write1String(
    ISequentialStream* pStream,
    const char* str,
    size_t buflen)
{
    assert(pStream);
    assert(str);

    const size_t strlen_ = strlen(str);
    const size_t size_ = (strlen_ >= buflen) ? strlen_ : buflen;
    assert(size_ <= 255);

    Write1UInt(pStream, static_cast<BYTE>(size_));
    Write(pStream, str, static_cast<ULONG>(strlen_));

    if (strlen_ >= buflen)
        return;

    const BYTE b = 0;

    const size_t count = buflen - strlen_;

    for (size_t i = 0; i < count; ++i)
        Write(pStream, &b, 1);
}
#endif



void EbmlIO::Write1UTF8(
    ISequentialStream* pStream,
    const wchar_t* str)
{
    assert(pStream);
    assert(str);

    const int cb = WideCharToMultiByte(
                    CP_UTF8,
                    0,   //flags (must be 0 for UTF-8 conversion)
                    str,
                    -1,  //assume NUL-terminated, and calculate length
                    0,   //buf
                    0,   //count
                    0,
                    0);

    assert(cb > 0);

    char* const buf = (char*)_malloca(cb);

    const int n = WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    str,
                    -1,
                    buf,
                    cb,
                    0,
                    0);

    assert(n == cb);
    assert(n > 0);

    const size_t nn = n - 1;
    assert(nn <= 255);

#ifdef _DEBUG
    const size_t strlen_ = strlen(buf);
    assert(strlen_ == nn);
    assert(buf[nn] == '\0');
#endif

    const BYTE size = static_cast<BYTE>(nn);

    Write1UInt(pStream, size);
    Write(pStream, buf, size);
}


void EbmlIO::Serialize(
    ISequentialStream* pStream,
    const BYTE* p,
    const BYTE* q)
{
    assert(pStream);
    assert(p);
    assert(q);
    assert(q >= p);

    while (q != p)
    {
        --q;

        ULONG cbWritten;

        const HRESULT hr = pStream->Write(q, 1, &cbWritten);
        assert(SUCCEEDED(hr));
        assert(cbWritten == 1);
        hr;
    }
}


void EbmlIO::Serialize(
    ISequentialStream* pStream,
    const void* buf,
    ULONG len)
{
    assert(buf);

    const BYTE* const p = static_cast<const BYTE*>(buf);
    const BYTE* const q = p + len;

    Serialize(pStream, p, q);
}
