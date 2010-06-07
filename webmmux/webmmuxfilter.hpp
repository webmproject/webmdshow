// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include <strmif.h>
#include <string>
#include "webmmuxinpinvideo.hpp"
#include "webmmuxinpinaudio.hpp"
#include "webmmuxoutpin.hpp"
#include "webmmuxcontext.hpp"
#include "clockable.hpp"

namespace WebmMux
{

class Filter : public IBaseFilter,
               public IMediaSeeking,
               public IAMFilterMiscFlags,
               public CLockable
{
    friend HRESULT CreateInstance(
            IClassFactory*,
            IUnknown*, 
            const IID&, 
            void**);
    
    Filter(IClassFactory*, IUnknown*);
    virtual ~Filter();
    
    Filter(const Filter&);
    Filter& operator=(const Filter&);
    
public:

    //IUnknown

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    
    //IBaseFilter

    HRESULT STDMETHODCALLTYPE GetClassID(CLSID*);    
    HRESULT STDMETHODCALLTYPE Stop();    
    HRESULT STDMETHODCALLTYPE Pause();    
    HRESULT STDMETHODCALLTYPE Run(REFERENCE_TIME);    
    HRESULT STDMETHODCALLTYPE GetState(DWORD, FILTER_STATE*);    
    HRESULT STDMETHODCALLTYPE SetSyncSource(IReferenceClock*);
    HRESULT STDMETHODCALLTYPE GetSyncSource(IReferenceClock**);
    HRESULT STDMETHODCALLTYPE EnumPins(IEnumPins**);    
    HRESULT STDMETHODCALLTYPE FindPin(LPCWSTR, IPin**);    
    HRESULT STDMETHODCALLTYPE QueryFilterInfo(FILTER_INFO*);    
    HRESULT STDMETHODCALLTYPE JoinFilterGraph(IFilterGraph*, LPCWSTR);    
    HRESULT STDMETHODCALLTYPE QueryVendorInfo(LPWSTR*);

    //IMediaSeeking interface:

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

    ULONG STDMETHODCALLTYPE GetMiscFlags();

private:

    class nondelegating_t : public IUnknown
    {
    public:
    
        Filter* const m_pFilter;
        LONG m_cRef;
        
        nondelegating_t(Filter*);
        virtual ~nondelegating_t();
        
        HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
        ULONG STDMETHODCALLTYPE AddRef();
        ULONG STDMETHODCALLTYPE Release();

    private:

        nondelegating_t(const nondelegating_t&);
        nondelegating_t& operator=(const nondelegating_t&);

    };
    
    IClassFactory* const m_pClassFactory;
    nondelegating_t m_nondelegating;
    IUnknown* const m_pOuter;  //decl must follow m_nondelegating
    REFERENCE_TIME m_start;
    IReferenceClock* m_clock;
    FILTER_INFO m_info;
    
public:

    FILTER_STATE m_state;
    InpinVideo m_inpin_video;
    InpinAudio m_inpin_audio;
    Outpin m_outpin;    
    Context m_ctx;
    
    HRESULT OnEndOfStream();

};

}  //end WebmMux
