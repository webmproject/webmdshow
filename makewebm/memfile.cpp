// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <objbase.h>
#include "memfile.hpp"
#include <cassert>


MemFile::MemFile() :
    m_hFile(INVALID_HANDLE_VALUE),
    m_pMap(0),
    m_size(0)
{
}


MemFile::~MemFile()
{
    const HRESULT hr = Close();
    hr;
    assert(SUCCEEDED(hr));
}


HRESULT MemFile::Open(const wchar_t* strFileName)
{
    if (strFileName == 0)
        return E_INVALIDARG;

    if (m_hFile != INVALID_HANDLE_VALUE)
        return E_UNEXPECTED;

    assert(m_pMap == 0);

    m_hFile = CreateFile(
                strFileName,
                GENERIC_READ,
                FILE_SHARE_DELETE,
                0,  //security attributes
                OPEN_EXISTING,
                FILE_ATTRIBUTE_READONLY |
                  FILE_FLAG_DELETE_ON_CLOSE,
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

    m_size = size.QuadPart;
    assert(m_size >= 0);

    m_pMap = MapViewOfFile(m_hFile, FILE_MAP_READ, 0, 0, 0);
    assert(m_pMap);

    return S_OK;
}


HRESULT MemFile::Close()
{
    if (m_hFile == INVALID_HANDLE_VALUE)
        return S_FALSE;

    if (m_pMap)
    {
        const BOOL b = UnmapViewOfFile(m_pMap);
        assert(b);

        m_pMap = 0;
    }

    m_size = 0;

    const BOOL b = CloseHandle(m_hFile);

    m_hFile = INVALID_HANDLE_VALUE;

    if (b)
        return S_OK;

    const DWORD e = GetLastError();
    return HRESULT_FROM_WIN32(e);
}


bool MemFile::IsOpen() const
{
    return (m_hFile != INVALID_HANDLE_VALUE);
}


HRESULT MemFile::GetView(
    const BYTE*& buf,
    LONGLONG& len) const
{
    if (!IsOpen())
        return E_FAIL;

    buf = static_cast<const BYTE*>(m_pMap);
    len = m_size;

    return S_OK;
}
