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
#include "webmvorbisdecoderinpin.h"
#include "webmvorbisdecoderoutpin.h"
#include "clockable.h"

namespace WebmVorbisDecoderLib
{

class Filter : public IBaseFilter,
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

private:
    class CNondelegating : public IUnknown
    {
        CNondelegating(const CNondelegating&);
        CNondelegating& operator=(const CNondelegating&);

    public:

        Filter* const m_pFilter;
        LONG m_cRef;

        explicit CNondelegating(Filter*);
        virtual ~CNondelegating();

        HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
        ULONG STDMETHODCALLTYPE AddRef();
        ULONG STDMETHODCALLTYPE Release();

    };

    IClassFactory* const m_pClassFactory;
    CNondelegating m_nondelegating;
    IUnknown* const m_pOuter;  //decl must follow m_nondelegating
    REFERENCE_TIME m_start;
    IReferenceClock* m_clock;

public:
    FILTER_INFO m_info;
    FILTER_STATE m_state;
    Inpin m_inpin;
    Outpin m_outpin;

private:
    void OnStart();

};


}  //end namespace WebmVorbisDecoderLib
