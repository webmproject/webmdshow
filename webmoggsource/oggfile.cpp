// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "oggfile.hpp"
#include <cassert>

namespace WebmOggSource
{

OggFile::OggFile() :
    m_hFile(INVALID_HANDLE_VALUE),
    m_length(0)
{
}


OggFile::~OggFile()
{
    const HRESULT hr = Close();
    hr;
    assert(SUCCEEDED(hr));
}


HRESULT OggFile::SetPosition(LONGLONG pos) const
{
    const BOOL b = SetFilePointerEx(
                    m_hFile,
                    reinterpret_cast<LARGE_INTEGER&>(pos),
                    0,
                    FILE_BEGIN);  //TODO: generalize

    if (b)
        return S_OK;

    const DWORD e = GetLastError();
    return HRESULT_FROM_WIN32(e);
}


HRESULT OggFile::Open(const wchar_t* strFileName)
{
    if (strFileName == 0)
        return E_INVALIDARG;

    if (m_hFile != INVALID_HANDLE_VALUE)
        return E_UNEXPECTED;

    m_hFile = CreateFile(
                strFileName,
                GENERIC_READ,
                FILE_SHARE_READ,
                0,  //security attributes
                OPEN_EXISTING,
                FILE_ATTRIBUTE_READONLY,
                0);

    if (m_hFile == INVALID_HANDLE_VALUE)
    {
        const DWORD e = GetLastError();
        return HRESULT_FROM_WIN32(e);
    }

    LARGE_INTEGER size;

    const BOOL b = GetFileSizeEx(m_hFile, &size);

    if (!b)
    {
        const DWORD e = GetLastError();
        Close();
        return HRESULT_FROM_WIN32(e);
    }

    m_length = size.QuadPart;
    assert(m_length >= 0);

    return S_OK;
}


HRESULT OggFile::Close()
{
    if (m_hFile == INVALID_HANDLE_VALUE)
        return S_FALSE;

    const BOOL b = CloseHandle(m_hFile);

    m_hFile = INVALID_HANDLE_VALUE;

    if (b)
        return S_OK;

    const DWORD e = GetLastError();
    return HRESULT_FROM_WIN32(e);
}


bool OggFile::IsOpen() const
{
    return (m_hFile != INVALID_HANDLE_VALUE);
}


long OggFile::Read(
    long long pos,
    long len,
    unsigned char* buf)
{
    if (!IsOpen())
        return -1;

    if (pos < 0)
        return -1;

    if (len < 0)
        return -1;

    if (pos > m_length)
        return oggparser::E_END_OF_FILE;

    //if (len == 0)
    //    return 0;  //success

    //if ((pos + len) > m_length)
    //    return oggparser::E_END_OF_FILE;

    const HRESULT hr = SetPosition(pos);

    if (FAILED(hr))
        return oggparser::E_READ_ERROR;

    DWORD cbRead;

    const BOOL b = ReadFile(m_hFile, buf, len, &cbRead, 0);

    if (!b)
    {
        const DWORD e = GetLastError();
        e;

        return oggparser::E_READ_ERROR;
    }

    //Testing for the End of a File
    //http://msdn.microsoft.com/en-us/library/aa365690(v=vs.85).aspx

    if (cbRead < DWORD(len))
        return oggparser::E_END_OF_FILE;

    return 0;  //success
}


#if 0

int OggFile::Length(
    long long* pTotal,
    long long* pAvailable)
{
    if (!IsOpen())
        return -1;

    if (pTotal)
        *pTotal = m_length;

    if (pAvailable)
        *pAvailable = m_length;

    return 0;  //success
}

#endif


} //end namespace WebmOggSource
