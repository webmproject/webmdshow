// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "vp8encoderoutpin.hpp"

namespace VP8EncoderLib
{
class Filter;

class OutpinPreview : public Outpin
{
    OutpinPreview(const OutpinPreview&);
    OutpinPreview& operator=(const OutpinPreview&);
    
protected:
    std::wstring GetName() const;
    
public:    
    explicit OutpinPreview(Filter*);
    virtual ~OutpinPreview();    

    //IUnknown interface:
    
    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    
    //IPin interface:

    //HRESULT STDMETHODCALLTYPE Connect(IPin*, const AM_MEDIA_TYPE*);

    HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE*);
    
    //local functions
    
    void Render(CLockable::Lock&, const vpx_image_t*);

protected:
    void SetDefaultMediaTypes();
    HRESULT GetAllocator(IMemInputPin*, IMemAllocator**) const;
    void GetSubtype(GUID&) const;
    
};


}  //end namespace VP8EncoderLib
