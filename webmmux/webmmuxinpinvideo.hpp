// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "webmmuxinpin.hpp"

namespace WebmMux
{

class Context;
class StreamVideo;

class InpinVideo : public Inpin
{
    InpinVideo(const InpinVideo&);
    InpinVideo& operator=(const InpinVideo&);
    
public:

    explicit InpinVideo(Filter*);
    ~InpinVideo();
    
    HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE*);
    HRESULT STDMETHODCALLTYPE GetAllocatorRequirements(ALLOCATOR_PROPERTIES*);

protected:

    HRESULT OnInit();
    void OnFinal();

    HANDLE GetOtherHandle() const;
    
    HRESULT QueryAcceptVP80(const AM_MEDIA_TYPE&) const;
    HRESULT VetBitmapInfoHeader(const BITMAPINFOHEADER&) const;
    HRESULT GetAllocatorRequirementsVP80(ALLOCATOR_PROPERTIES&) const;

    static StreamVideo* CreateStreamVP80(Context&, const AM_MEDIA_TYPE&);

};
    
}  //end namespace WebmMux
