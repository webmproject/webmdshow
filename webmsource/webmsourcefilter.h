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
#include "mkvfile.h"
#include "clockable.h"
#include <vector>

namespace mkvparser
{
class Segment;
class Cluster;
class Stream;
}

namespace WebmSource
{

class Outpin;

class Filter : public IBaseFilter,
               public IFileSourceFilter,
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

    //IFileSourceFilter

    HRESULT STDMETHODCALLTYPE Load(LPCOLESTR, const AM_MEDIA_TYPE*);
    HRESULT STDMETHODCALLTYPE GetCurFile(LPOLESTR*, AM_MEDIA_TYPE*);

    //IAMFilterMiscFlags

    ULONG STDMETHODCALLTYPE GetMiscFlags();


    //local classes and methods

    void CreateOutpin(mkvparser::Stream*);

private:

    class nondelegating_t : public IUnknown
    {
    public:

        Filter* const m_pFilter;
        LONG m_cRef;

        explicit nondelegating_t(Filter*);
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
    static const LONGLONG kNoSeek;

    FILTER_STATE m_state;
    MkvFile m_file;
    std::wstring m_filename;
    mkvparser::Segment* m_pSegment;
    const mkvparser::Cluster* m_pSeekBase;
    LONGLONG m_seekBase_ns;
    __int64 m_currTime;  //requested seek time (reftime units)
    LONGLONG m_seekTime_ns;  //actual seek time (normalized)

    typedef std::vector<Outpin*> pins_t;
    pins_t m_pins;

    int GetConnectionCount() const;
    void SetCurrPosition(LONGLONG currTime, DWORD dwCurr, Outpin*);

private:

    void OnStop();
    void OnStart();

    HRESULT CreateSegment();
    void PopulateSamples(const HANDLE*, DWORD);

};

}  //end namespace WebmSource

