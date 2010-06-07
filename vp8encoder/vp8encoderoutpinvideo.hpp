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

class OutpinVideo : public Outpin,
                    public IMediaSeeking
{
    OutpinVideo(const OutpinVideo&);
    OutpinVideo& operator=(const OutpinVideo&);
    
protected:
    std::wstring GetName() const;
    
public:    
    explicit OutpinVideo(Filter*);
    virtual ~OutpinVideo();    

    //IUnknown interface:
    
    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    
    //IPin interface:

    //HRESULT STDMETHODCALLTYPE Connect(IPin*, const AM_MEDIA_TYPE*);

    HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE*);

    //IMediaSeeking
    
    HRESULT STDMETHODCALLTYPE GetCapabilities(DWORD*);    
    HRESULT STDMETHODCALLTYPE CheckCapabilities(DWORD*);    
    HRESULT STDMETHODCALLTYPE IsFormatSupported(const GUID*);
    HRESULT STDMETHODCALLTYPE QueryPreferredFormat(GUID*);    
    HRESULT STDMETHODCALLTYPE GetTimeFormat(GUID*);    
    HRESULT STDMETHODCALLTYPE IsUsingTimeFormat(const GUID*);    
    HRESULT STDMETHODCALLTYPE SetTimeFormat(const GUID*);    
    HRESULT STDMETHODCALLTYPE GetDuration(LONGLONG*);    
    HRESULT STDMETHODCALLTYPE GetStopPosition(LONGLONG*);    
    HRESULT STDMETHODCALLTYPE GetCurrentPosition(LONGLONG*);    

    HRESULT STDMETHODCALLTYPE ConvertTimeFormat( 
        LONGLONG*,
        const GUID*,
        LONGLONG,
        const GUID*);
    
    HRESULT STDMETHODCALLTYPE SetPositions( 
        LONGLONG*,
        DWORD,
        LONGLONG*,
        DWORD);
    
    HRESULT STDMETHODCALLTYPE GetPositions( 
        LONGLONG*,
        LONGLONG*);
    
    HRESULT STDMETHODCALLTYPE GetAvailable( 
        LONGLONG*,
        LONGLONG*);
    
    HRESULT STDMETHODCALLTYPE SetRate(double);    
    HRESULT STDMETHODCALLTYPE GetRate(double*);    
    HRESULT STDMETHODCALLTYPE GetPreroll(LONGLONG*);
    
    //local functions
    
    HRESULT GetFrame(IVP8Sample::Frame&);
    
protected:
    void SetDefaultMediaTypes();
    HRESULT GetAllocator(IMemInputPin*, IMemAllocator**) const;
    void GetSubtype(GUID&) const;
    
};


}  //end namespace VP8EncoderLib
