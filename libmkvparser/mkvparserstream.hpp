// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include <string>
#include <iosfwd>

class CMediaTypes;

namespace MkvParser
{

class Track;
class BlockEntry;
class Cluster;

class Stream
{
    Stream(const Stream&);
    Stream& operator=(const Stream&);

public:
    virtual ~Stream();
    void Init();
    
    std::wstring GetId() const;  //IPin::QueryId
    std::wstring GetName() const;  //IPin::QueryPinInfo
    virtual void GetMediaTypes(CMediaTypes&) const = 0;
    virtual HRESULT QueryAccept(const AM_MEDIA_TYPE*) const = 0;
    
    virtual HRESULT SetConnectionMediaType(const AM_MEDIA_TYPE&);
    virtual HRESULT UpdateAllocatorProperties(ALLOCATOR_PROPERTIES&) const = 0;

    HRESULT Preload();
    HRESULT PopulateSample(IMediaSample*);    

    __int64 GetDuration() const;
    __int64 GetCurrPosition() const;
    __int64 GetStopPosition() const;
    __int64 GetCurrTime() const;
    __int64 GetStopTime() const;

    HRESULT GetAvailable(LONGLONG*) const;

    //NOTE: too inefficient
    //void LoadCurrPosition(LONGLONG, DWORD, __int64& parse_result);
    
    LONGLONG GetSeekTime(LONGLONG currTime, DWORD dwCurr) const;  //reftime to ns
    Cluster* GetSeekBase(LONGLONG ns) const;
    Cluster* SetCurrPosition(LONGLONG currTime, DWORD dwCurr);  //reftime
    void SetCurrPosition(Cluster*);

    void SetStopPosition(LONGLONG, DWORD);
    void SetStopPositionEOS();
    
    ULONG GetClusterCount() const;
    
    template<typename T, typename S, typename F>
    struct TCreateOutpins
    {
        F* f;
        
        typedef S* (*pfn_t)(T*);
        pfn_t pfn;
        
        TCreateOutpins(F* f_, pfn_t pfn_) : f(f_), pfn(pfn_) {}

        void operator()(T* t) const
        {
            if (S* s = (*pfn)(t))
                f->CreateOutpin(s);
        }
    };

    Track* const m_pTrack;

protected:
    explicit Stream(Track*);
    bool m_bDiscontinuity;
    const BlockEntry* m_pCurr;
    const BlockEntry* m_pStop;
    Cluster* m_pBase;

    virtual std::wostream& GetKind(std::wostream&) const = 0;
    virtual bool SendPreroll(IMediaSample*);
    virtual HRESULT OnPopulateSample(const BlockEntry* pNext, IMediaSample* pSample) = 0;

};

}  //end namespace MkvParser
