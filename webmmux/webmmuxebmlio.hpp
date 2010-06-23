// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once

namespace EbmlIO
{
    class File
    {
        File(const File&);
        File& operator=(const File&);

    public:

        File();
        ~File();

        void SetStream(IStream*);
        IStream* GetStream() const;

        HRESULT SetSize(__int64);

        __int64 SetPosition(__int64, STREAM_SEEK = STREAM_SEEK_SET);
        __int64 GetPosition() const;

        void Write(const void*, ULONG);

        void Serialize8UInt(__int64);
        void Serialize4UInt(ULONG);
        void Serialize2UInt(USHORT);
        void Serialize1UInt(BYTE);
        void Serialize2SInt(SHORT);
        void Serialize4Float(float);

        void WriteID4(ULONG);
        void WriteID3(ULONG);
        void WriteID2(USHORT);
        void WriteID1(BYTE);

        ULONG ReadID4();

        void Write8UInt(__int64);
        void Write4UInt(ULONG);
        void Write2UInt(USHORT);
        void Write1UInt(BYTE);

        void Write1String(const char*);
        //void Write1String(const char* str, size_t len);
        void Write1UTF8(const wchar_t*);

    private:

        IStream* m_pStream;

    };

    HRESULT SetSize(IStream*, __int64);
    __int64 SetPosition(IStream*, __int64, STREAM_SEEK);

    void Serialize(ISequentialStream*, const BYTE*, const BYTE*);
    void Serialize(ISequentialStream*, const void*, ULONG);
    void Write(ISequentialStream*, const void*, ULONG);

    void WriteID4(ISequentialStream*, ULONG);
    void WriteID3(ISequentialStream*, ULONG);
    void WriteID2(ISequentialStream*, USHORT);
    void WriteID1(ISequentialStream*, BYTE);

    ULONG ReadID4(ISequentialStream*);

    void Write8UInt(ISequentialStream*, __int64);
    void Write4UInt(ISequentialStream*, ULONG);
    void Write2UInt(ISequentialStream*, USHORT);
    void Write1UInt(ISequentialStream*, BYTE);

    void Write1String(ISequentialStream*, const char*);
    void Write1String(ISequentialStream*, const char*, size_t);
    void Write1UTF8(ISequentialStream*, const wchar_t*);

} //end namespace EbmlIO
