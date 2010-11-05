// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "mkvreader.hpp"
#include <cassert>

namespace WebmSplit
{

MkvReader::MkvReader()
{
}


MkvReader::~MkvReader()
{
}


void MkvReader::SetSource(IAsyncReader* pSource)
{
    m_pSource = pSource;
}


bool MkvReader::IsOpen() const
{
    return m_pSource;
}


#if 0
HRESULT MkvReader::MkvRead(
    LONGLONG start,
    LONG len,
    BYTE* ptr)
{
    //TODO: use aligned read so we can timeout the read
    return m_pSource->SyncRead(start, len, ptr);
}
#else
int MkvReader::Read(
    long long pos,
    long len,
    unsigned char* buf)
{
    if (!IsOpen())
        return -1;

    const HRESULT hr = m_pSource->SyncRead(pos, len, buf);
    return SUCCEEDED(hr) ? 0 : -1;
}
#endif


#if 0
HRESULT MkvReader::MkvLength(
    LONGLONG* pTotal,
    LONGLONG* pAvailable)
{
    return m_pSource->Length(pTotal, pAvailable);
}
#else
int MkvReader::Length(
    long long* pTotal,
    long long* pAvailable)
{
    if (!IsOpen())
        return -1;

    const HRESULT hr = m_pSource->Length(pTotal, pAvailable);
    return SUCCEEDED(hr) ? 0 : -1;
}
#endif


} //end namespace WebmSplit
