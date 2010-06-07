// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "mkvparser.hpp"

namespace WebmSource
{

class MkvFile : public MkvParser::IMkvFile
{
    MkvFile(const MkvFile&);
    MkvFile& operator=(const MkvFile&);
    
public:
    MkvFile();
    virtual ~MkvFile();
    
    HRESULT Open(const wchar_t*);
    HRESULT Close();
    bool IsOpen() const;
    
    HRESULT MkvRead( 
        LONGLONG,
        LONG,
        BYTE*);
    
    HRESULT MkvLength( 
        LONGLONG*,
        LONGLONG*);
    
private:
    HANDLE m_hFile;
    LONGLONG m_length;    

    HRESULT SetPosition(LONGLONG) const;

};


}  //end namespace WebmSource
