// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "mkvparser.hpp"
#include "mkvparserstreamreader.h"
#include "graphutil.h"
#include <list>
#include <map>
//#include <set>
#include <deque>

class CLockable;

namespace WebmSplit
{

class MkvReader : public mkvparser::IStreamReader
{
    MkvReader(const MkvReader&);
    MkvReader& operator=(const MkvReader&);

public:
    MkvReader();
    virtual ~MkvReader();

    HRESULT SetSource(IAsyncReader*);
    bool IsOpen() const;

    int Read(long long pos, long len, unsigned char* buf);
    int Length(long long* total, long long* available);

    HRESULT LockPages(const mkvparser::BlockEntry*);
    void UnlockPages(const mkvparser::BlockEntry*);

    HRESULT Wait(CLockable&, LONGLONG pos, LONG size, DWORD timeout_ms);

    HRESULT BeginFlush();
    HRESULT EndFlush();

    bool m_sync_read;

private:
    ALLOCATOR_PROPERTIES m_props;
    GraphUtil::IMemAllocatorPtr m_pAllocator;
    GraphUtil::IAsyncReaderPtr m_pSource;

    struct Page
    {
        int cRef;
        GraphUtil::IMediaSamplePtr pSample;

        LONGLONG GetPos() const;
    };

    typedef std::list<Page> pages_list_t;
    pages_list_t m_pages;

    typedef std::multimap<LONGLONG, pages_list_t::iterator> free_pages_t;
    free_pages_t m_free_pages;

    typedef std::deque<pages_list_t::iterator> cache_t;
    cache_t m_cache;

    struct PageLess
    {
        bool operator()(cache_t::value_type lhs, LONGLONG pos) const
        {
            return (lhs->GetPos() < pos);
        }

        bool operator()(LONGLONG pos, cache_t::value_type rhs) const
        {
            return (pos < rhs->GetPos());
        }

        bool operator()(cache_t::value_type lhs, cache_t::value_type rhs) const
        {
            return (lhs->GetPos() < rhs->GetPos());
        }
    };

    HRESULT Commit();
    HRESULT Decommit();

    void Read(
        pages_list_t::const_iterator,
        long long&,
        long&,
        unsigned char**) const;

    int InsertPage(cache_t::iterator, LONGLONG, cache_t::iterator&);

    void FreeOne(cache_t::iterator&);
    void PurgeOne();

#if 0 //def _DEBUG
    LONGLONG m_total;
    LONGLONG m_avail;
#endif

};


}  //end namespace WebmSplit
