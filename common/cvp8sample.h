// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "cmemallocator.h"
#include "imemsample.h"
#include "ivp8sample.h"
#include <list>

class CVP8Sample : public IMediaSample,
                   public IMemSample,
                   public IVP8Sample
{
    CVP8Sample(const CVP8Sample&);
    CVP8Sample& operator=(const CVP8Sample&);

protected:

    explicit CVP8Sample(CMemAllocator*);
    virtual ~CVP8Sample();

public:

    static HRESULT CreateAllocator(IMemAllocator**);
    static HRESULT GetFrame(CMemAllocator*, IVP8Sample::Frame&);

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    //IVP8Sample interface:

    Frame& GetFrame();

    //IMemSample interface:

    ULONG STDMETHODCALLTYPE GetCount();
    HRESULT STDMETHODCALLTYPE Initialize();
    HRESULT STDMETHODCALLTYPE Finalize();
    HRESULT STDMETHODCALLTYPE Destroy();

    //IMediaSample interface:

    HRESULT STDMETHODCALLTYPE GetPointer(
        BYTE** ppBuffer);

    long STDMETHODCALLTYPE GetSize();

    HRESULT STDMETHODCALLTYPE GetTime(
        REFERENCE_TIME* pTimeStart,
        REFERENCE_TIME* pTimeEnd);

    HRESULT STDMETHODCALLTYPE SetTime(
        REFERENCE_TIME* pTimeStart,
        REFERENCE_TIME* pTimeEnd);

    HRESULT STDMETHODCALLTYPE IsSyncPoint();

    HRESULT STDMETHODCALLTYPE SetSyncPoint(
        BOOL bIsSyncPoint);

    HRESULT STDMETHODCALLTYPE IsPreroll();

    HRESULT STDMETHODCALLTYPE SetPreroll(
        BOOL bIsPreroll);

    long STDMETHODCALLTYPE GetActualDataLength();

    HRESULT STDMETHODCALLTYPE SetActualDataLength(long);

    HRESULT STDMETHODCALLTYPE GetMediaType(
        AM_MEDIA_TYPE** ppMediaType);

    HRESULT STDMETHODCALLTYPE SetMediaType(
        AM_MEDIA_TYPE* pMediaType);

    HRESULT STDMETHODCALLTYPE IsDiscontinuity();

    HRESULT STDMETHODCALLTYPE SetDiscontinuity(
        BOOL bDiscontinuity);

    HRESULT STDMETHODCALLTYPE GetMediaTime(
        LONGLONG* pTimeStart,
        LONGLONG* pTimeEnd);

    HRESULT STDMETHODCALLTYPE SetMediaTime(
        LONGLONG* pTimeStart,
        LONGLONG* pTimeStop);

protected:

    CMemAllocator* const m_pAllocator;
    ULONG m_cRef;

    struct SampleFactory : CMemAllocator::ISampleFactory
    {
    private:
        SampleFactory(const SampleFactory&);
        SampleFactory& operator=(const SampleFactory&);

    protected:
        virtual ~SampleFactory();

    public:
        SampleFactory();

        HRESULT CreateSample(CMemAllocator*, IMemSample*&);
        HRESULT InitializeSample(IMemSample*);
        HRESULT FinalizeSample(IMemSample*);
        HRESULT DestroySample(IMemSample*);
        HRESULT Destroy(CMemAllocator*);

        typedef std::list<IVP8Sample::Frame> frames_t;
        frames_t m_pool;  //for reuse

    private:
        void PurgePool();

    };

    static HRESULT CreateInstance(CMemAllocator*, CVP8Sample*&);

private:

    IVP8Sample::Frame m_frame;
    bool m_preroll;
    bool m_discontinuity;

};
