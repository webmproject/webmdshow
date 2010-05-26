// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license and patent
// grant that can be found in the LICENSE file in the root of the source
// tree. All contributing project authors may be found in the AUTHORS
// file in the root of the source tree.

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


HRESULT MkvReader::MkvRead( 
    LONGLONG start,
    LONG len,
    BYTE* ptr)
{
    //TODO: use aligned read so we can timeout the read
    return m_pSource->SyncRead(start, len, ptr);
}


HRESULT MkvReader::MkvLength( 
    LONGLONG* pTotal,
    LONGLONG* pAvailable)
{
    return m_pSource->Length(pTotal, pAvailable);
}


} //end namespace WebmSplit
