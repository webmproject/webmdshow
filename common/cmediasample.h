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

class CMediaSample : public IMediaSample,
                     public IMemSample
{
    CMediaSample(const CMediaSample&);
    CMediaSample& operator=(const CMediaSample&);

protected:

    explicit CMediaSample(CMemAllocator*);
    virtual ~CMediaSample();

    struct Factory : CMemAllocator::ISampleFactory
    {
        virtual ~Factory();

        HRESULT CreateSample(CMemAllocator*, IMemSample*&);
        HRESULT InitializeSample(IMemSample*);
        HRESULT FinalizeSample(IMemSample*);
        HRESULT DestroySample(IMemSample*);
        HRESULT Destroy(CMemAllocator*);
    };

public:

    static HRESULT CreateAllocator(IMemAllocator**);

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

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

    HRESULT Create();

private:

    bool m_sync_point;
    bool m_preroll;
    bool m_discontinuity;
    __int64 m_start_time;
    __int64 m_stop_time;
    __int64 m_media_start_time;
    __int64 m_media_stop_time;
    long m_actual_data_length;  //allocated memory holding actual data

    BYTE* m_buf;
    long m_buflen;  //how much memory was allocated
    long m_off;     //to satisfy alignment requirements

    AM_MEDIA_TYPE* m_pmt;

};
