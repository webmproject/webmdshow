// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "oggparser.h"
//#include "mkvparserstreamreader.h"

namespace WebmOggSource
{

//class OggFile : public mkvparser::IStreamReader
class OggFile : public oggparser::IOggReader
{
    OggFile(const OggFile&);
    OggFile& operator=(const OggFile&);

public:
    OggFile();
    virtual ~OggFile();

    HRESULT Open(const wchar_t*);
    HRESULT Close();
    bool IsOpen() const;

    long Read(long long pos, long len, unsigned char* buf);
    //int Length(long long* total, long long* available);

private:
    HANDLE m_hFile;
    LONGLONG m_length;

    HRESULT SetPosition(LONGLONG) const;

};


}  //end namespace WebmOggSource
