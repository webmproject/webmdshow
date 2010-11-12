#include "mkvreader.hpp"
#include <cassert>
//#include <mfapi.h>
//#include <mferror.h>
#include <algorithm>
//#include <utility>  //std::make_pair
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif

MkvReader::MkvReader(IMFByteStream* pStream) : m_pStream(pStream)
{
    const ULONG n = m_pStream->AddRef();
    n;

    HRESULT hr;

#ifdef _DEBUG
    DWORD dw;

    hr = m_pStream->GetCapabilities(&dw);
    assert(SUCCEEDED(hr));
    assert(dw & MFBYTESTREAM_IS_READABLE);
    assert(dw & MFBYTESTREAM_IS_SEEKABLE);
    //TODO: check whether local, etc
    //TODO: could also check this earlier, in byte stream handler

    //TODO: check MFBYTESTREAM_IS_REMOTE
    //TODO: check MFBYTESTREAM_HAS_SLOW_SEEK
    //TODO: check MFBYTESTREAM_IS_PARTIALLY_DOWNLOADED
#endif

    GetSystemInfo(&m_info);
    //dwPageSize
    //dwAllocationGranularity

    hr = m_pStream->GetLength(&m_length);
    assert(SUCCEEDED(hr));
}


MkvReader::~MkvReader()
{
    DestroyRegions();

    const ULONG n = m_pStream->Release();
    n;
}

int MkvReader::Read(long long pos, long len, unsigned char* buf)
{
    if (pos < 0)
        return -1;

    if (len <= 0)
        return 0;

    if (buf == 0)
        return -1;

    typedef cache_t::iterator iter_t;
    iter_t next;

    const DWORD page_size = m_info.dwPageSize;

    if (m_cache.empty() || ((*m_cache.front()).pos > pos))
        next = m_cache.begin();
    else
    {
        const iter_t i = m_cache.begin();
        const iter_t j = m_cache.end();

        next = std::upper_bound(i, j, pos, PageLess());
        assert(next != i);

        const cache_t::value_type page_iter = *--iter_t(next);
        const Page& page = *page_iter;
        const LONGLONG page_end = page.pos + page_size;

        if (pos < page_end)  //cache hit
            Read(page_iter, pos, len, &buf);
    }

    while (len > 0)
    {
        if ((next == m_cache.end()) || ((*next)->pos > pos))
        {
            const iter_t curr_iter = InsertPage(next, pos);
            const cache_t::value_type page_iter = *curr_iter;
            const Page& curr_page = *page_iter;

            next = ++iter_t(curr_iter);
            assert((next == m_cache.end()) || ((*next)->pos > curr_page.pos));

            Read(page_iter, pos, len, &buf);
        }
        else
        {
            const cache_t::value_type page_iter = *next++;
            const Page& page = *page_iter;
            assert(page.pos <= pos);
            assert(pos < (page.pos + page_size));

            Read(page_iter, pos, len, &buf);
        }
    }

    return 0;  //means all requested bytes were read
}


MkvReader::cache_t::iterator
MkvReader::InsertPage(
    cache_t::iterator next,
    LONGLONG pos)
{
    const DWORD page_size = m_info.dwPageSize;

    if (m_regions.size() < 4)  //arbitrary upper bound
        CreateRegion();        //make some free pages

    if (m_free_pages.empty())
    {
        LONGLONG next_pos;

        if (next == m_cache.end())
            next_pos = -1;
        else
        {
            const cache_t::value_type page_iter = *next;
            Page& page = *page_iter;

            next_pos = page.pos;
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
                assert(page.pos == next_pos);
                assert(page.cRef > 0);
            }
        }

        if (next_pos >= 0)
        {
            assert(next != m_cache.end());

            const cache_t::value_type page_iter = *next;

            Page& page = *page_iter;
            assert(page.pos == next_pos);
            assert(page.cRef > 0);

            --page.cRef;
        }
    }

    if (m_free_pages.empty())
        CreateRegion();

    const LONGLONG key = page_size * LONGLONG(pos / page_size);
    assert((next == m_cache.end()) || ((*next)->pos > key));

    //TODO: implement region affinity
    free_pages_t::iterator free_page = m_free_pages.find(key);

    if (free_page == m_free_pages.end())
        free_page = m_free_pages.begin();

    const pages_vector_t::iterator page_iter = free_page->second;
    m_free_pages.erase(free_page);

    Page& page = *page_iter;
    assert(page.cRef == 0);

    if (page.pos != key)
    {
        page.pos = key;

        QWORD curr;

        HRESULT hr = m_pStream->Seek(msoBegin, page.pos, 0, &curr);
        assert(SUCCEEDED(hr));
        assert(curr == QWORD(page.pos));

        const Region& r = *page.region;
        const pages_vector_t::size_type offset = page_iter - r.pages.begin();
        BYTE* const ptr = page.region->ptr + offset * size_t(page_size);

        ULONG cbRead;
        hr = m_pStream->Read(ptr, page_size,  &cbRead);
        assert(SUCCEEDED(hr));
        assert((cbRead == page_size) ||
               (QWORD(page.pos + cbRead) == m_length));
    }

    return m_cache.insert(next, page_iter);
}


void MkvReader::CreateRegion()
{
    const DWORD region_size = m_info.dwAllocationGranularity;
    const DWORD page_size = m_info.dwPageSize;

    const DWORD type = MEM_COMMIT | MEM_RESERVE;
    const DWORD protect = PAGE_READWRITE;

    void* const ptr = VirtualAlloc(0, region_size, type, protect);
    assert(ptr);  //TODO

    m_regions.push_back(Region());
    Region& r = m_regions.back();

    r.ptr = static_cast<BYTE*>(ptr);

    const DWORD n = region_size / page_size;
    r.pages.resize(n);

    typedef pages_vector_t::iterator iter_t;

    for (DWORD i = 0; i < n; ++i)
    {
        Page& page = r.pages[i];

        page.region = &r;
        page.pos = -1;  //means "don't have data on this page"
        //page.index = i;
        page.cRef = 0;

        const pages_vector_t::iterator iter = r.pages.begin() + i;
        const free_pages_t::value_type value(-1, iter);

        m_free_pages.insert(value);
    }
}


void MkvReader::Read(
    pages_vector_t::const_iterator page_iter,
    long long& pos,
    long& requested_len,
    unsigned char** pdst) const
{
    const Page& page = *page_iter;
    assert(pos >= page.pos);

    const LONG page_size = m_info.dwPageSize;

    const LONGLONG off_ = pos - page.pos;
    assert(off_ >= 0);
    assert(off_ <= LONG_MAX);

    const LONG page_off = static_cast<LONG>(off_);  //within page
    assert(page_off < page_size);

    const LONG page_len = page_size - page_off;  //what remains on page
    const long len = (requested_len <= page_len) ? requested_len : page_len;

    if (pdst)
    {
        unsigned char*& dst = *pdst;
        const Region& r = *page.region;
        const pages_vector_t::size_type offset = page_iter - r.pages.begin();

        const BYTE* const page_base = r.ptr + offset * size_t(page_size);
        const BYTE* const src = page_base + page_off;

        memcpy(dst, src, len);
        dst += len;
    }

    pos += len;
    requested_len -= len;
}


int MkvReader::Length(long long* total, long long* avail)
{
    if (total == 0)
        return -1;

    if (avail == 0)
        return -1;

    *total = m_length;
    *avail = m_length;

    return 0;
}


void MkvReader::LockPage(const mkvparser::BlockEntry* pBE)
{
    if (pBE == 0)
        return;

    if (pBE->EOS())
        return;

    const mkvparser::Block* const pBlock = pBE->GetBlock();
    assert(pBlock);

    long long pos = pBlock->m_start;
    long len = static_cast<long>(pBlock->m_size);

    typedef cache_t::iterator iter_t;
    iter_t next;

    const DWORD page_size = m_info.dwPageSize;

    int n = 0;

    if (m_cache.empty() || (pos < (*m_cache.front()).pos))
        next = m_cache.begin();
    else
    {
        const iter_t i = m_cache.begin();
        const iter_t j = m_cache.end();

        next = std::upper_bound(i, j, pos, PageLess());
        assert(next != i);

        const cache_t::value_type page_iter = *--iter_t(next);
        Page& page = *page_iter;
        const LONGLONG page_end = page.pos + page_size;

        if (pos < page_end)  //cache hit
        {
            Read(page_iter, pos, len, 0);
            ++page.cRef;
            ++n;
        }
    }

    while (len > 0)
    {
        if ((next == m_cache.end()) || ((*next)->pos > pos))
        {
            const iter_t curr = InsertPage(next, pos);
            const cache_t::value_type page_iter = *curr;
            Page& page = *page_iter;

            next = ++iter_t(curr);
            assert((next == m_cache.end()) || ((*next)->pos > page.pos));

            Read(page_iter, pos, len, 0);
            ++page.cRef;
            ++n;
        }
        else
        {
            const cache_t::value_type page_iter = *next++;
            Page& page = *page_iter;
            assert(page.pos <= pos);
            assert(pos < (page.pos + page_size));

            Read(page_iter, pos, len, 0);
            ++page.cRef;
            ++n;
        }
    }

#ifdef _DEBUG
    if (n >= 10)  //to debug PurgeOne
    {
        odbgstream os;
        os << "MkvReader::LockPage: large multi-page lock: n="
           << n
           << " block.start=" << pBlock->m_start
           << " block.size=" << pBlock->m_size
           << endl;
    }
#endif
}


void MkvReader::UnlockPage(const mkvparser::BlockEntry* pBE)
{
    if (pBE == 0)
        return;

    if (pBE->EOS())
        return;

    const mkvparser::Block* const pBlock = pBE->GetBlock();
    assert(pBlock);

    long long pos = pBlock->m_start;
    long len = static_cast<long>(pBlock->m_size);

    const DWORD page_size = m_info.dwPageSize;

    assert(!m_cache.empty());
    assert((*m_cache.front()).pos <= pos);

    typedef cache_t::iterator iter_t;

    const iter_t i = m_cache.begin();
    const iter_t j = m_cache.end();

    iter_t next = std::upper_bound(i, j, pos, PageLess());
    assert(next != i);

    {
        const cache_t::value_type page_iter = *--iter_t(next);
        Page& page = *page_iter;
        const LONGLONG page_end = page.pos + page_size;

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
        assert((*next)->pos <= pos);

        const cache_t::value_type page_iter = *next++;
        Page& page = *page_iter;
        assert(page.pos <= pos);
        assert(pos < (page.pos + page_size));
        assert(page.cRef > 0);

        Read(page_iter, pos, len, 0);
        --page.cRef;
    }
}


int MkvReader::PurgeFront()
{
    int n = 0;

    while (!m_cache.empty())
    {
        const cache_t::value_type page_iter = m_cache.front();
        Page& page = *page_iter;

        if (page.cRef > 0)
            break;

        m_cache.pop_front();

        const free_pages_t::value_type value(page.pos, page_iter);
        m_free_pages.insert(value);

        ++n;
    }

    return n;
}


int MkvReader::PurgeBack()
{
    DWORD idx = 0;
    const DWORD m = m_info.dwAllocationGranularity / m_info.dwPageSize;

    int n = 0;

    while (!m_cache.empty() && (idx < m))
    {
        const cache_t::value_type page_iter = m_cache.back();
        Page& page = *page_iter;

        if (page.cRef > 0)
            break;

        m_cache.pop_back();

        const free_pages_t::value_type value(page.pos, page_iter);
        m_free_pages.insert(value);

        ++idx;
        ++n;
    }

    return n;
}


#if 0
void MkvReader::Purge()
{
    PurgeFront();
    PurgeBack();

    const DWORD m = m_info.dwAllocationGranularity / m_info.dwPageSize;

    if (m_cache.size() > (2 * m))  //TODO: might require more tuning
    {
        typedef cache_t::iterator iter_t;
        iter_t cache_iter = --iter_t(m_cache.end());

        DWORD i = 0;

        while (i < m)
        {
            assert(cache_iter != m_cache.end());

            const cache_t::value_type page_iter = *cache_iter;
            const Page& page = *page_iter;

            odbgstream os;
            os << "page.cRef=" << page.cRef
               << " i=" << i
               << " cache.size=" << m_cache.size()
               << " index=" << (cache_iter - m_cache.begin())
               << endl;

            if (cache_iter == m_cache.begin())
            {
                if (page.cRef == 0)
                {
                    m_cache.erase(cache_iter);

                    const free_pages_t::value_type value(page.pos, page_iter);
                    m_free_pages.insert(value);
                }

                break;
            }

            if (page.cRef)
                --cache_iter;
            else
            {
                const cache_t::size_type index = cache_iter - m_cache.begin();
                assert(index > 0);

                m_cache.erase(cache_iter);
                cache_iter = m_cache.begin() + index - 1;

                const free_pages_t::value_type value(page.pos, page_iter);
                m_free_pages.insert(value);

                ++i;
            }
        }
    }

#ifdef _DEBUG
    odbgstream os;
    os << "MkvReader::Purge:"
       << " free_pages.size=" << m_free_pages.size()
       << " cache.size=" << m_cache.size()
       << " regions.size=" << m_regions.size()
       << endl;
#endif
}
#endif


void MkvReader::DestroyRegions()
{
    m_cache.clear();
    m_free_pages.clear();

    while (!m_regions.empty())
    {
        Region& r = m_regions.front();

        const BOOL b = VirtualFree(r.ptr, 0, MEM_RELEASE);
        assert(b);

        m_regions.pop_front();
    }

#ifdef _DEBUG
    odbgstream os;
    os << "MkvReader::Clear" << endl;
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

            const free_pages_t::value_type value(page.pos, page_iter);
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

            const free_pages_t::value_type value(page.pos, page_iter);
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

                const free_pages_t::value_type value(page.pos, page_iter);
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
