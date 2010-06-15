// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once

class MemFile
{
    MemFile(const MemFile&);
    MemFile& operator=(const MemFile&);

public:
    MemFile();
    ~MemFile();

    HRESULT Open(const wchar_t*);
    HRESULT Close();
    bool IsOpen() const;

    HRESULT GetView(const BYTE*&, LONGLONG&) const;

private:
    HANDLE m_hFile;
    HANDLE m_hMap;
    void* m_pView;
    LONGLONG m_size;

};
