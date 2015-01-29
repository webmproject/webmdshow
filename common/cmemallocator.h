// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include <strmif.h>
#include "clockable.h"
#include "imemsample.h"
#include <list>

class CMemAllocator : public IMemAllocator,
                      public CLockable
{
    CMemAllocator(const CMemAllocator&);
    CMemAllocator& operator=(const CMemAllocator&);

public:

    interface ISampleFactory
    {
        virtual HRESULT CreateSample(CMemAllocator*, IMemSample*&) = 0;
        virtual HRESULT InitializeSample(IMemSample*) = 0;
        virtual HRESULT FinalizeSample(IMemSample*) = 0;
        virtual HRESULT DestroySample(IMemSample*) = 0;
        virtual HRESULT Destroy(CMemAllocator*) = 0;
    };

protected:

    explicit CMemAllocator(ISampleFactory*);
    virtual ~CMemAllocator();

public:

    ISampleFactory* const m_pSampleFactory;

    static HRESULT CreateInstance(ISampleFactory*, IMemAllocator**);

    ULONG GetCount() const;

    //IUnknown

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    //IMemAllocator

    HRESULT STDMETHODCALLTYPE SetProperties(
        ALLOCATOR_PROPERTIES*,
        ALLOCATOR_PROPERTIES*);

    HRESULT STDMETHODCALLTYPE GetProperties(
        ALLOCATOR_PROPERTIES*);

    HRESULT STDMETHODCALLTYPE Commit();

    HRESULT STDMETHODCALLTYPE Decommit();

    HRESULT STDMETHODCALLTYPE GetBuffer(
        IMediaSample**,
        REFERENCE_TIME*,
        REFERENCE_TIME*,
        DWORD);

    HRESULT STDMETHODCALLTYPE ReleaseBuffer(IMediaSample*);

private:

    ULONG m_cRef;
    HANDLE m_hCond;
    ALLOCATOR_PROPERTIES m_props;
    bool m_bCommitted;
    long m_cActive;

    typedef std::list<IMemSample*> samples_t;
    samples_t m_samples;

    HRESULT CreateSample();
    IMediaSample* GetSample();

};
