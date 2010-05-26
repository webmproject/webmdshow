// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license and patent
// grant that can be found in the LICENSE file in the root of the source
// tree. All contributing project authors may be found in the AUTHORS
// file in the root of the source tree.

#pragma once
#include "webmmuxinpin.hpp"

namespace WebmMux
{

class StreamAudio;

class InpinAudio : public Inpin
{
    InpinAudio(const InpinAudio&);
    InpinAudio& operator=(const InpinAudio&);

public:

    explicit InpinAudio(Filter*);
    ~InpinAudio();

    HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE*);
    HRESULT STDMETHODCALLTYPE GetAllocatorRequirements(ALLOCATOR_PROPERTIES*);

protected:

   HRESULT OnInit();
   void OnFinal();
   HANDLE GetOtherHandle() const;

};

}  //end namespace WebmMux
