// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "mkvparser.hpp"
#include "graphutil.hpp"

namespace WebmSplit
{

class MkvReader : public MkvParser::IMkvFile
{
    MkvReader(const MkvReader&);
    MkvReader& operator=(const MkvReader&);
    
public:
    MkvReader();
    virtual ~MkvReader();
    
    void SetSource(IAsyncReader*);
    bool IsOpen() const;
    
    HRESULT MkvRead( 
        LONGLONG,
        LONG,
        BYTE*);
    
    HRESULT MkvLength( 
        LONGLONG*,
        LONGLONG*);
    
private:
    GraphUtil::IAsyncReaderPtr m_pSource;

};


}  //end namespace WebmSplit
