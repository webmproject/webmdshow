// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma warning(disable:4702)  //unreachable code
#include <strmif.h>
#include "mkvreader.h"
#include <cassert>
#include <algorithm>
#include <vfwmsgs.h>
#include "clockable.h"
#pragma warning(default:4702)

namespace WebmSplit
{

MkvReader::MkvReader() : m_sync_read(true)
{
}


MkvReader::~MkvReader()
{
}


HRESULT MkvReader::SetSource(IAsyncReader* pSource)
{
    HRESULT hr;

    if (pSource == 0)
    {
        hr = Decommit();
        assert(SUCCEEDED(hr));

        m_pSource = 0;
        m_pAllocator = 0;

        return S_OK;
    }

#if 0 //def _DEBUG
    LONGLONG avail;

    hr = pSource->Length(&m_total, &avail);
    assert(SUCCEEDED(hr));
    assert(avail <= m_total);

    m_avail = 128 * 1024;

    if (m_avail > m_total)
        m_avail = m_total;
#endif

    // Get page size.
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const int kPageSize = info.dwPageSize;

    // The splitter is at the mercy of the filter graph manager, and is unable
    // to control how much data it will be asked to store. Configure the
    // allocator to support storage of up to 1 second of data at 5 MB/sec.
    const int k5MBPS = 5 * 1000 * 1000;
    const int kNumBuffers = k5MBPS / kPageSize;

    ALLOCATOR_PROPERTIES props = {0};
    props.cBuffers = kNumBuffers;
    props.cbBuffer = kPageSize;

    hr = pSource->RequestAllocator(0, &props, &m_pAllocator);

    if (FAILED(hr))
        return hr;

    hr = m_pAllocator->GetProperties(&m_props);
    assert(SUCCEEDED(hr));
    assert(m_props.cBuffers > 0);
    assert(m_props.cbBuffer > 0);

    if (FAILED(hr) || m_props.cBuffers < 1 || m_props.cbBuffer < 1)
      return VFW_E_NO_ALLOCATOR;

    m_pSource = pSource;

    hr = Commit();
    assert(SUCCEEDED(hr));

    return hr;
}


bool MkvReader::IsOpen() const
{
    return m_pSource;
}


HRESULT MkvReader::Commit()
{
    //This is called by the FGM thread, during the transition to Run/Paused
    //from Stopped, but before any other threads have been created.  Therefore
    //no thread synchronization is performed.

    assert(m_pages.empty());
    assert(m_free_pages.empty());

    if (m_pAllocator == 0)
        return VFW_E_NO_ALLOCATOR;

    const HRESULT hr = m_pAllocator->Commit();

    if (FAILED(hr))
        return hr;

    const long n = m_props.cBuffers;
    assert(n > 0);

    for (long i = 0; i < n; ++i)
    {
        Page page;

        page.cRef = 0;
        page.pSample = 0;

        m_pages.push_back(page);
    }

    typedef pages_list_t::iterator iter_t;

    iter_t iter = m_pages.begin();
    const iter_t iter_end = m_pages.end();

    while (iter != iter_end)
    {
        const LONGLONG key = iter->GetPos();
        const free_pages_t::value_type value(key, iter);

        m_free_pages.insert(value);
        ++iter;
    }

    return S_OK;
}


HRESULT MkvReader::Decommit()
{
    //This is called by the FGM thread, during the transition from Run/Paused
    //to stopped, but after any other threads have been destroyed.  Therefore
    //no thread synchronization is performed.

    m_free_pages.clear();

    while (!m_pages.empty())
    {
        Page& page = m_pages.front();
        assert(page.cRef == 0);

        if (page.pSample)
        {
            const ULONG n = page.pSample->Release();
            n;

            page.pSample = 0;
        }

        m_pages.pop_front();
    }

    if (m_pAllocator == 0)
        return S_OK;

    return m_pAllocator->Decommit();
}


int MkvReader::Read(
    long long pos,
    long len,
    unsigned char* buf)
{
    if (!IsOpen())
        return -1;

    if (m_sync_read)
    {
        const HRESULT hr = m_pSource->SyncRead(pos, len, buf);
        return SUCCEEDED(hr) ? 0 : -1;
    }

    if (pos < 0)
        return -1;

    if (len <= 0)
        return 0;

    if (buf == 0)
        return -1;

    typedef cache_t::iterator iter_t;
    iter_t next;

    const DWORD page_size = m_props.cbBuffer;

    if (m_cache.empty() || ((*m_cache.front()).GetPos() > pos))
        next = m_cache.begin();
    else
    {
        const iter_t i = m_cache.begin();
        const iter_t j = m_cache.end();

        next = std::upper_bound(i, j, pos, PageLess());
        assert(next != i);

        const cache_t::value_type page_iter = *--iter_t(next);
        const Page& page = *page_iter;
        const LONGLONG page_end = page.GetPos() + page_size;

        if (pos < page_end)  //cache hit
            Read(page_iter, pos, len, &buf);
    }

    while (len > 0)
    {
        if ((next == m_cache.end()) || ((*next)->GetPos() > pos))
        {
            iter_t curr_iter;

            const int status = InsertPage(next, pos, curr_iter);

            if (status < 0)  //error
                return status;

            const cache_t::value_type page_iter = *curr_iter;
            const Page& curr_page = *page_iter;

            next = ++iter_t(curr_iter);
            assert((next == m_cache.end()) ||
                   ((*next)->GetPos() > curr_page.GetPos()));

            Read(page_iter, pos, len, &buf);
        }
        else
        {
            const cache_t::value_type page_iter = *next++;
            const Page& page = *page_iter;
            assert(page.GetPos() <= pos);
            assert(pos < (page.GetPos() + page_size));

            Read(page_iter, pos, len, &buf);
        }
    }

    return 0;  //means all requested bytes were read
}


void MkvReader::Read(
    pages_list_t::const_iterator page_iter,
    long long& pos,
    long& requested_len,
    unsigned char** pdst) const
{
    const Page& page = *page_iter;

    const LONGLONG page_pos = page.GetPos();
    assert(pos >= page_pos);

    const LONG page_size = m_props.cbBuffer;

    const LONGLONG off_ = pos - page_pos;
    assert(off_ >= 0);
    assert(off_ <= LONG_MAX);

    const LONG page_off = static_cast<LONG>(off_);  //within page
    assert(page_off < page_size);

    const LONG page_len = page_size - page_off;  //what remains on page
    const long len = (requested_len <= page_len) ? requested_len : page_len;

    if (pdst)
    {
        assert(page.pSample);

        BYTE* ptr;

        const HRESULT hr = page.pSample->GetPointer(&ptr);
        assert(SUCCEEDED(hr));
        assert(ptr);

        const BYTE* const page_base = ptr;
        const BYTE* const src = page_base + page_off;

        unsigned char*& dst = *pdst;

        memcpy(dst, src, len);
        dst += len;
    }

    pos += len;
    requested_len -= len;
}


int MkvReader::InsertPage(
    cache_t::iterator next,
    LONGLONG pos,
    cache_t::iterator& cache_iter)
{
    FreeOne(next);

    if (m_free_pages.empty())  //error: all samples are busy
        return -1;  //generic error

    const DWORD page_size = m_props.cbBuffer;

    const LONGLONG page_pos = page_size * LONGLONG(pos / page_size);
    assert((next == m_cache.end()) || ((*next)->GetPos() > page_pos));

    free_pages_t::iterator free_page = m_free_pages.find(page_pos);

    if (free_page == m_free_pages.end())
        free_page = m_free_pages.begin();

    const pages_list_t::iterator page_iter = free_page->second;
    assert(page_iter->cRef == 0);

    if (page_iter->GetPos() == page_pos)
    {
        m_free_pages.erase(free_page);
        cache_iter = m_cache.insert(next, page_iter);
        return 0;  //success
    }

    LONGLONG total, available;

    const int status = Length(&total, &available);

    if (status < 0)
        return status;

    assert(available <= total);
    assert(page_pos < total);

    const LONGLONG page_end = page_pos + page_size;

    //TODO: there is a problem here.  The caller probably checked
    //already whether the attempted read would be past available,
    //and if not the he would call Read assuming it would not
    //fail.  However, we make a stronger test here, because we
    //test the position of the end of the page -- but this pos
    //is most likely beyond the pos tested by caller, so we
    //return E_BUFFER_NOT_FULL when the caller is not expecting
    //it.  We could just go ahead and read the entire page here,
    //but then we run the risk of a "long delay" because we
    //attempt a read beyong the available value.  One alternative
    //is for the caller to attempt to read the byte just beyond
    //his intended range.  Yet another possibility is to read
    //up to available (instead of requiring page_end), and try
    //to manage the fact that the page isn't full.  But that
    //won't work either, because reads must be aligned (you
    //must request and entire page).

    if ((page_end <= total) && (page_end > available))
        return mkvparser::E_BUFFER_NOT_FULL;

    HRESULT hr;

    Page& page = *page_iter;

    if (page.pSample == 0)
    {
        hr = m_pAllocator->GetBuffer(&page.pSample, 0, 0, 0);
        assert(SUCCEEDED(hr));
        assert(page.pSample);
    }

    LONGLONG st = page_pos * 10000000;
    LONGLONG sp = page_end * 10000000;

    hr = page.pSample->SetTime(&st, &sp);
    assert(SUCCEEDED(hr));

    hr = m_pSource->SyncReadAligned(page.pSample);

    if (FAILED(hr))  //VFW_S_WRONG_STATE
    {
        const ULONG cRef = page.pSample->Release();
        cRef;

        page.pSample = 0;

        return -1;  //generic error value
    }

    m_free_pages.erase(free_page);
    cache_iter = m_cache.insert(next, page_iter);
    return 0;  //success
}


int MkvReader::Length(
    long long* pTotal,
    long long* pAvailable)
{
    if (!IsOpen())
        return -1;

#if 0 //def _DEBUG
    assert(m_total >= 0);
    assert(m_avail <= m_total);

    if (m_avail < m_total)
    {
        m_avail += 1024;

        if (m_avail > m_total)
            m_avail = m_total;
    }

    *pTotal = m_total;
    *pAvailable = m_avail;

    return 0;
#else
    const HRESULT hr = m_pSource->Length(pTotal, pAvailable);

    if (FAILED(hr))
        return -1;

    return 0;
#endif
}


void MkvReader::PurgeOne()
{
    if (m_cache.empty())
        return;

    {
        const cache_t::value_type page_iter = m_cache.front();
        const Page& page = *page_iter;

        if (page.cRef <= 0)
        {
            m_cache.pop_front();

            const free_pages_t::value_type value(page.GetPos(), page_iter);
            m_free_pages.insert(value);

            return;
        }
    }

    {
        const cache_t::value_type page_iter = m_cache.back();
        const Page& page = *page_iter;

        if (page.cRef <= 0)
        {
            m_cache.pop_back();

            const free_pages_t::value_type value(page.GetPos(), page_iter);
            m_free_pages.insert(value);

            return;
        }
    }

    {
        typedef cache_t::iterator iter_t;
        iter_t cache_iter = --iter_t(m_cache.end());

        for (;;)
        {
            const cache_t::value_type page_iter = *cache_iter;
            const Page& page = *page_iter;

            if (page.cRef == 0)
            {
                m_cache.erase(cache_iter);

                const free_pages_t::value_type value(page.GetPos(), page_iter);
                m_free_pages.insert(value);

                return;
            }

            if (cache_iter == m_cache.begin())
                break;

            --cache_iter;
        }

        return;
    }
}


LONGLONG MkvReader::Page::GetPos() const
{
    if (pSample == 0)
        return -1;

    LONGLONG st, sp;

    const HRESULT hr = pSample->GetTime(&st, &sp);
    assert(SUCCEEDED(hr));
    assert(st >= 0);
    assert((st % 10000000) == 0);

    const LONGLONG pos = st / 10000000;
    return pos;
}


HRESULT MkvReader::Wait(
    CLockable& lock,
    LONGLONG start_pos,
    LONG size,
    DWORD timeout)
{
    assert(start_pos >= 0);
    assert(size > 0);

    //lock has already been seized

    cache_t::iterator next = m_cache.end();

    FreeOne(next);

    if (m_free_pages.empty())  //all samples are busy
        return E_FAIL;

    free_pages_t::iterator free_page = m_free_pages.begin();

    pages_list_t::iterator page_iter = free_page->second;

    m_free_pages.erase(free_page);

    //We now own this page.

    Page& page = *page_iter;
    assert(page.cRef == 0);

    HRESULT hr;

    if (page.pSample == 0)
    {
        hr = m_pAllocator->GetBuffer(&page.pSample, 0, 0, 0);
        assert(SUCCEEDED(hr));
        assert(page.pSample);
    }

#if 0
    LONGLONG total, avail;

    const int status = Length(&total, &avail);
    assert(status == 0);
    assert(avail <= total);
#endif

    const LONGLONG stop_pos = start_pos + LONGLONG(size) - 1;  //last byte

    const DWORD page_size = m_props.cbBuffer;
    const LONGLONG page_pos = page_size * LONGLONG(stop_pos / page_size);

    LONGLONG st = page_pos * 10000000;
    LONGLONG sp = (page_pos + page_size) * 10000000;

    hr = page.pSample->SetTime(&st, &sp);
    assert(SUCCEEDED(hr));
    assert(page.GetPos() == page_pos);

    hr = m_pSource->Request(page.pSample, 0);

    HRESULT hrWait;

    if (SUCCEEDED(hr))
    {
        IMediaSample* pSample;
        DWORD_PTR token;

        hr = lock.Release();
        assert(SUCCEEDED(hr));

        hrWait = m_pSource->WaitForNext(timeout, &pSample, &token);

        hr = lock.Seize(INFINITE);
        assert(SUCCEEDED(hr));

        if (SUCCEEDED(hrWait))
        {
            assert(pSample == page.pSample);
            assert(token == 0);

            pSample = 0;

            const free_pages_t::value_type value(page_pos, page_iter);
            m_free_pages.insert(value);

#if 0 //def _DEBUG
            const LONGLONG avail = page_pos + page_size;
            m_avail = (avail >= m_total) ? m_total : avail;
#endif

            return S_OK;
        }

        hrWait = E_FAIL;
    }

    //async read request failed, or was cancelled

    IMediaSample* pSample;
    DWORD_PTR token;

    for (;;)
    {
        hrWait = m_pSource->WaitForNext(0, &pSample, &token);

        if (pSample == 0)
            break;
    }

    const ULONG cRef = page.pSample->Release();
    cRef;

    page.pSample = 0;
    assert(page.GetPos() < 0);

    const free_pages_t::value_type value(-1, page_iter);
    m_free_pages.insert(value);

    return VFW_E_TIMEOUT;
}


HRESULT MkvReader::BeginFlush()
{
    return m_pSource->BeginFlush();
}


HRESULT MkvReader::EndFlush()
{
    return m_pSource->EndFlush();
}


void MkvReader::FreeOne(cache_t::iterator& next)
{
    if (!m_free_pages.empty())
        return;

    LONGLONG next_pos;

    if (next == m_cache.end())
        next_pos = -1;
    else
    {
        const cache_t::value_type page_iter = *next;
        Page& page = *page_iter;

        next_pos = page.GetPos();
        assert(next_pos >= 0);

        ++page.cRef;
    }

    PurgeOne();

    if (!m_free_pages.empty())  //success
    {
        if (next_pos < 0)
            next = m_cache.end();
        else
        {
            typedef cache_t::iterator iter_t;

            const iter_t i = m_cache.begin();
            const iter_t j = m_cache.end();

            next = std::lower_bound(i, j, next_pos, PageLess());
            assert(next != j);

            const cache_t::value_type page_iter = *next;
            const Page& page = *page_iter;
            page;
            assert(page.GetPos() == next_pos);
            assert(page.cRef > 0);
        }
    }

    if (next_pos >= 0)
    {
        assert(next != m_cache.end());

        const cache_t::value_type page_iter = *next;
        Page& page = *page_iter;
        assert(page.GetPos() == next_pos);
        assert(page.cRef > 0);

        --page.cRef;
    }
}


HRESULT MkvReader::LockPages(const mkvparser::BlockEntry* pBE)
{
    if (pBE == 0)
        return S_FALSE;

    if (pBE->EOS())
        return S_FALSE;

    const mkvparser::Block* const pBlock = pBE->GetBlock();
    assert(pBlock);

    LONGLONG pos = pBlock->m_start;
    long len = static_cast<long>(pBlock->m_size);

    typedef cache_t::iterator iter_t;
    iter_t next;

    const DWORD page_size = m_props.cbBuffer;

//    int n = 0;

    if (m_cache.empty() || (pos < (*m_cache.front()).GetPos()))
        next = m_cache.begin();
    else
    {
        const iter_t i = m_cache.begin();
        const iter_t j = m_cache.end();

        next = std::upper_bound(i, j, pos, PageLess());
        assert(next != i);

        const cache_t::value_type page_iter = *--iter_t(next);
        Page& page = *page_iter;
        const LONGLONG page_end = page.GetPos() + page_size;

        if (pos < page_end)  //cache hit
        {
            Read(page_iter, pos, len, 0);
            ++page.cRef;
//            ++n;
        }
    }

    while (len > 0)
    {
        if ((next == m_cache.end()) || ((*next)->GetPos() > pos))
        {
            iter_t curr_iter;

            const int status = InsertPage(next, pos, curr_iter);

            if (status < 0)  //error
                return status;

            if (curr_iter == m_cache.end())  //async read is req'd
                return VFW_E_BUFFER_UNDERFLOW;

            const cache_t::value_type page_iter = *curr_iter;
            Page& page = *page_iter;

            next = ++iter_t(curr_iter);
            assert((next == m_cache.end()) ||
                   ((*next)->GetPos() > page.GetPos()));

            Read(page_iter, pos, len, 0);
            ++page.cRef;
//            ++n;
        }
        else
        {
            const cache_t::value_type page_iter = *next++;
            Page& page = *page_iter;
            assert(page.GetPos() <= pos);
            assert(pos < (page.GetPos() + page_size));

            Read(page_iter, pos, len, 0);
            ++page.cRef;
//            ++n;
        }
    }

    return S_OK;
}


void MkvReader::UnlockPages(const mkvparser::BlockEntry* pBE)
{
    if (pBE == 0)
        return;

    if (pBE->EOS())
        return;

    const mkvparser::Block* const pBlock = pBE->GetBlock();
    assert(pBlock);

    LONGLONG pos = pBlock->m_start;
    long len = static_cast<long>(pBlock->m_size);

    const DWORD page_size = m_props.cbBuffer;

    assert(!m_cache.empty());
    assert((*m_cache.front()).GetPos() <= pos);

    typedef cache_t::iterator iter_t;

    const iter_t i = m_cache.begin();
    const iter_t j = m_cache.end();

    iter_t next = std::upper_bound(i, j, pos, PageLess());
    assert(next != i);

    {
        const cache_t::value_type page_iter = *--iter_t(next);
        Page& page = *page_iter;
        const LONGLONG page_end = page.GetPos() + page_size;

        if (pos < page_end)  //cache hit
        {
            assert(page.cRef > 0);

            Read(page_iter, pos, len, 0);
            --page.cRef;
        }
    }

    while (len > 0)
    {
        assert(next != m_cache.end());
        assert((*next)->GetPos() <= pos);

        const cache_t::value_type page_iter = *next++;
        Page& page = *page_iter;
        assert(page.GetPos() <= pos);
        assert(pos < (page.GetPos() + page_size));
        assert(page.cRef > 0);

        Read(page_iter, pos, len, 0);
        --page.cRef;
    }
}


} //end namespace WebmSplit
