// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "mkvfile.h"
#include <cassert>

namespace WebmSource
{

MkvFile::MkvFile() :
    m_hFile(INVALID_HANDLE_VALUE)
{
}


MkvFile::~MkvFile()
{
    const HRESULT hr = Close();
    hr;
    assert(SUCCEEDED(hr));
}


HRESULT MkvFile::SetPosition(LONGLONG pos) const
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


HRESULT MkvFile::Open(const wchar_t* strFileName)
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


HRESULT MkvFile::Close()
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


bool MkvFile::IsOpen() const
{
    return (m_hFile != INVALID_HANDLE_VALUE);
}


int MkvFile::Read(
    long long pos,
    long len,
    unsigned char* buf)
{
    if (pos < 0)
        return -1;

    if (len <= 0)
        return 0;

    if (!IsOpen())
        return -1;

    if (pos >= m_length)
        return -1;  //?

    const HRESULT hr = SetPosition(pos);
    assert(SUCCEEDED(hr));

    DWORD cbRead;
    const BOOL b = ReadFile(m_hFile, buf, len, &cbRead, 0);

    if (!b)
    {
        const DWORD e = GetLastError();
        e;

        return -1;
    }

    return (cbRead >= ULONG(len)) ? 0 : -1;
}


int MkvFile::Length(
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

} //end namespace WebmSource
