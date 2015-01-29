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
#include <vector>

class CMediaTypes;

namespace mkvparser
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
    void Stop();

    std::wstring GetId() const;  //IPin::QueryId
    std::wstring GetName() const;  //IPin::QueryPinInfo
    virtual void GetMediaTypes(CMediaTypes&) const = 0;
    virtual HRESULT QueryAccept(const AM_MEDIA_TYPE*) const = 0;

    virtual HRESULT SetConnectionMediaType(const AM_MEDIA_TYPE&);
    HRESULT UpdateAllocatorProperties(ALLOCATOR_PROPERTIES&) const;

    //HRESULT Preload();  //exactly one cluster

    HRESULT GetSampleCount(long&);

    typedef std::vector<IMediaSample*> samples_t;

    HRESULT PopulateSamples(const samples_t&);
    static void Clear(samples_t&);

    //__int64 GetDuration() const;
    //__int64 GetCurrPosition() const;
    //__int64 GetStopPosition() const;
    __int64 GetCurrTime() const;
    __int64 GetStopTime() const;

    //HRESULT GetAvailable(LONGLONG*) const;

    LONGLONG GetSeekTime(LONGLONG currTime, DWORD dwCurr) const;
    //convert from reftime to ns

    void SetCurrPosition(
        //const Cluster*,
        LONGLONG base_time_ns,
        const BlockEntry*);

    void SetStopPosition(LONGLONG, DWORD);
    void SetStopPositionEOS();

    ULONG GetClusterCount() const;

    const Track* const m_pTrack;
    static std::wstring ConvertFromUTF8(const char*);

protected:
    explicit Stream(const Track*);
    bool m_bDiscontinuity;
    const BlockEntry* m_pCurr;
    const BlockEntry* m_pStop;
    //const Cluster* m_pBase;
    LONGLONG m_base_time_ns;

    virtual std::wostream& GetKind(std::wostream&) const = 0;

    HRESULT InitCurr();

    virtual long GetBufferSize() const = 0;
    virtual long GetBufferCount() const = 0;

    virtual void OnPopulateSample(
                const BlockEntry*,
                const samples_t&) const = 0;

private:

    const BlockEntry* m_pLocked;
    HRESULT SetCurr(const mkvparser::BlockEntry*);

};

}  //end namespace mkvparser
