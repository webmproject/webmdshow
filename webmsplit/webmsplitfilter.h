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
#include <vector>
#include "webmsplitinpin.h"
#include "clockable.h"

namespace mkvparser
{
class IMkvReader;
class Cluster;
class Stream;
}

namespace WebmSplit
{

class Outpin;

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

    //local classes and methods

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
    FILTER_INFO m_info;

public:
    static const LONGLONG kNoSeek;

    FILTER_STATE m_state;
    Inpin m_inpin;
    const mkvparser::Cluster* m_pSeekBase;
    LONGLONG m_seekBase_ns;
    __int64 m_currTime;     //requested seek time
    __int64 m_seekTime_ns;  //actual seek time (normalized)

    typedef std::vector<Outpin*> outpins_t;
    outpins_t m_outpins;

    int GetConnectionCount() const;
    void SetCurrPosition(LONGLONG currTime, DWORD dwCurr, Outpin*);
    HRESULT OnDisconnectInpin();
    void OnStarvation(ULONG);

    HRESULT Open();
    void CreateOutpin(mkvparser::Stream*);

    bool InCache();

private:
    HANDLE m_hThread;
    mkvparser::Segment* m_pSegment;
    HANDLE m_hNewCluster;
    long m_cStarvation;

    static unsigned __stdcall ThreadProc(void*);
    unsigned Main();

    void Init();
    void Final();
    void OnStop();
    void OnStart();
    void OnNewCluster();
    void PopulateSamples(const HANDLE*, DWORD);

    void SetCurrPositionUsingSameTime(mkvparser::Stream*);
    void SetCurrPositionVideo(LONGLONG ns, mkvparser::Stream*);
    void SetCurrPositionAudio(LONGLONG ns, mkvparser::Stream*);

};

}  //end namespace WebmSplit
