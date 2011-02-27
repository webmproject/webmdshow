#include "mkvreader.hpp"
#include <cassert>
#include <algorithm>
#include <comdef.h>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif
#undef DEBUG_PURGE
//#define DEBUG_PURGE

MkvReader::MkvReader(IMFByteStream* pStream) :
    m_pStream(pStream),
    m_async_pos(-1),  //means "no async read in progress"
    m_async_len(-1)   //as above
{
    const ULONG n = m_pStream->AddRef();
    n;

    HRESULT hr;

    DWORD dw;

    hr = m_pStream->GetCapabilities(&dw);
    assert(SUCCEEDED(hr));
    assert(dw & MFBYTESTREAM_IS_READABLE);
    assert(dw & MFBYTESTREAM_IS_SEEKABLE);
    //TODO: check whether local, etc
    //TODO: could also check this earlier, in byte stream handler

    //TODO: check MFBYTESTREAM_IS_REMOTE
    //TODO: check MFBYTESTREAM_HAS_SLOW_SEEK

    GetSystemInfo(&m_info);
    //dwPageSize
    //dwAllocationGranularity

    //TODO: this crashed when I used the URL from MS:
    //MF_E_BYTESTREAM_UNKNOWN_LENGTH

    QWORD length;

    hr = m_pStream->GetLength(&length);

    m_length = SUCCEEDED(hr) ? length : -1;
    //m_length = -1;  //for debugging

    m_avail = 0;
}


MkvReader::~MkvReader()
{
    DestroyRegions();

    const ULONG n = m_pStream->Release();
    n;
}


HRESULT MkvReader::Close()
{
    return m_pStream->Close();
}


HRESULT MkvReader::GetCapabilities(DWORD& dw) const
{
    return m_pStream->GetCapabilities(&dw);
}


bool MkvReader::HasSlowSeek() const
{
#if 0
    return true;   //for debugging
#else
    DWORD dw;

    const HRESULT hr = m_pStream->GetCapabilities(&dw);

    if (FAILED(hr))
        return true;  //the more conservative choice, if we don't know

    return (dw & MFBYTESTREAM_HAS_SLOW_SEEK) ? true : false;
#endif
}


bool MkvReader::IsPartiallyDownloaded() const
{
    DWORD dw;

    const HRESULT hr = m_pStream->GetCapabilities(&dw);

    if (FAILED(hr))
        return true;  //the more conservative choice, if we don't know

    return (dw & MFBYTESTREAM_IS_PARTIALLY_DOWNLOADED) ? true : false;
}


HRESULT MkvReader::EnableBuffering(LONGLONG duration_reftime) const
{
    typedef IMFByteStreamBuffering Buffering;
    _COM_SMARTPTR_TYPEDEF(Buffering, __uuidof(Buffering));

    const BufferingPtr pBuffering(m_pStream);

    if (!pBuffering)
        return S_FALSE;

    MFBYTESTREAM_BUFFERING_PARAMS p;

    if (duration_reftime >= 0)
        p.qwPlayDuration = duration_reftime;
    else
        p.qwPlayDuration = 0;

    HRESULT hr = m_pStream->GetLength(&p.cbTotalFileSize);

    if (FAILED(hr))
        p.cbTotalFileSize = static_cast<QWORD>(-1);

    p.cbPlayableDataSize = p.cbTotalFileSize;

    MF_LEAKY_BUCKET_PAIR bb[1];

    if ((p.cbTotalFileSize <= 0) || (duration_reftime <= 0))
    {
        p.prgBuckets = 0;
        p.cBuckets = 0;
    }
    else
    {
        MF_LEAKY_BUCKET_PAIR& b = bb[0];

        const QWORD bits = p.cbTotalFileSize * 8ULL;
        const double secs = double(duration_reftime) / 10000000;

        const double bitrate = double(bits) / secs;

        b.dwBitrate = static_cast<DWORD>(bitrate);
        //b.dwBitrate *= 10;  //TODO: must handle datarate spikes somehow

        b.msBufferWindow = 2 * 5000;  //assume 5s clusters

        p.prgBuckets = bb;
        p.cBuckets = 1;
    }

    p.qwNetBufferingTime = 0;  //TODO: how should we synthesize this value?
    p.qwExtraBufferingTimeDuringSeek = 0;
    p.dRate = 1;

    hr = pBuffering->SetBufferingParams(&p);
    assert(SUCCEEDED(hr));  //TODO

    hr = pBuffering->EnableBuffering(TRUE);
    assert(SUCCEEDED(hr));  //TODO

    return S_OK;
}


int MkvReader::Read(long long pos, long len, unsigned char* buf)
{
    assert(m_async_pos < 0);
    assert(m_async_len < 0);

    if (pos < 0)
        return -1;

    if ((m_length >= 0) && (pos > m_length))
        return -1;

    if (len <= 0)
        return 0;

    if ((m_length >= 0) && ((pos + len) > m_length))
        return -1;

    if (buf == 0)
        return -1;

    typedef cache_t::iterator iter_t;
    iter_t next;

    //const DWORD page_size = m_info.dwPageSize;

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
        assert(page.pos >= 0);

        const ULONG page_size = page.len;
        assert(page_size > 0);

        const LONGLONG page_end = page.pos + page_size;

        if (pos < page_end)  //cache hit
            Read(page_iter, pos, len, &buf);
    }

    while (len > 0)
    {
        if ((next == m_cache.end()) || ((*next)->pos > pos))
        {
#if 1
            return mkvparser::E_BUFFER_NOT_FULL;
#else
            iter_t curr_iter;

            const int status = InsertPage(next, pos, curr_iter);
            assert(status == 0);

            if (status)
                return status;

            const cache_t::value_type page_iter = *curr_iter;
            const Page& curr_page = *page_iter;

            next = ++iter_t(curr_iter);
            assert((next == m_cache.end()) || ((*next)->pos > curr_page.pos));

            Read(page_iter, pos, len, &buf);
#endif
        }
        else
        {
            const cache_t::value_type page_iter = *next++;
            const Page& page = *page_iter;
            assert(page.pos <= pos);
            assert(page.len > 0);
            assert(pos < (page.pos + page.len));

            Read(page_iter, pos, len, &buf);
        }
    }

    return 0;  //means all requested bytes were read
}


#if 0
int MkvReader::InsertPage(
    cache_t::iterator next,
    LONGLONG pos,
    cache_t::iterator& curr)
{
    assert(m_async_len <= 0);

    const DWORD page_size = m_info.dwPageSize;

#if 0
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
#endif

    const LONGLONG key = page_size * LONGLONG(pos / page_size);
    assert((next == m_cache.end()) || ((*next)->pos > key));

    free_pages_t::iterator free_page = m_free_pages.find(key);

    if (free_page == m_free_pages.end())  //not found
        return mkvparser::E_BUFFER_NOT_FULL;

    const pages_vector_t::iterator page_iter = free_page->second;
    m_free_pages.erase(free_page);

    Page& page = *page_iter;
    assert(page.cRef == 0);
    assert(page.pos == key);
    assert(page.len > 0);

    curr = m_cache.insert(next, page_iter);
    return 0;  //success
}
#endif


void MkvReader::CreateRegion()
{
    const DWORD region_size = m_info.dwAllocationGranularity;
    const DWORD page_size = m_info.dwPageSize;

    //const DWORD type = MEM_COMMIT | MEM_RESERVE;
    //const DWORD protect = PAGE_READWRITE;
    //void* const ptr = VirtualAlloc(0, region_size, type, protect);
    void* const ptr = HeapAlloc(GetProcessHeap(), 0, region_size);
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
        page.len = 0;   //means "no data on this page"
        page.cRef = 0;

        const pages_vector_t::iterator iter = r.pages.begin() + i;
        const free_pages_t::value_type value(-1, iter);

        m_free_pages.insert(value);
    }

#if 0 //def _DEBUG
    odbgstream os;
    os << "mkvreader::CreateRegion: regions.size="
       << m_regions.size()
       << " free_pages.size="
       << m_free_pages.size()
       << " cache.size="
       << m_cache.size()
       << endl;
#endif
}


void MkvReader::Read(
    pages_vector_t::const_iterator page_iter,
    long long& pos,
    long& requested_len,
    unsigned char** pdst) const
{
    assert(pos >= 0);
    assert((m_length < 0) || (pos < m_length));
    assert(requested_len >= 0);
    assert((m_length < 0) || ((pos + requested_len) <= m_length));

    const Page& page = *page_iter;
    assert(page.pos >= 0);
    assert(page.len > 0);
    assert(pos >= page.pos);
    assert(page.cRef >= 0);

    const LONGLONG off_ = pos - page.pos;
    assert(off_ >= 0);
    assert(off_ <= LONG_MAX);

    const ULONG page_off = static_cast<ULONG>(off_);  //within page
    assert(page_off < page.len);

    const LONG page_len = page.len - page_off;  //what remains on page
    assert(page_len > 0);

    const long len = (requested_len <= page_len) ? requested_len : page_len;

    if (pdst)
    {
        unsigned char*& dst = *pdst;
        const Region& r = *page.region;
        const pages_vector_t::size_type offset = page_iter - r.pages.begin();

        const ULONG page_size = m_info.dwPageSize;

        //again this assumes that page_size is same as dwPageSize
        //does giving the page a len possibly less than dwPageSize
        //break our algorithm?  The problem is that the page that is
        //mapped to the end of the file, then we'll have a hole in
        //the middle of the region.

        const BYTE* const page_base = r.ptr + offset * size_t(page_size);

        const BYTE* const src = page_base + page_off;

        memcpy(dst, src, len);
        dst += len;
    }

    //either we read from the middle of page, because this is
    //the first page of a multi-page read; or, we read from
    //the start of a page, because this is the first or later
    //page of a multi-page read

    //this can leave pos in the middle of a page, because we requested
    //fewer bytes than remain on this page, or put us just beyond this
    //page, because we requested all the bytes that remain on this page

    pos += len;
    assert((m_length < 0) || (pos <= m_length));

    //end represents the end of this page, irrespective of how many
    //bytes were requested by the caller

    const LONGLONG end = page.pos + page.len;
    assert((m_length < 0) || (end <= m_length));

    requested_len -= len;
    assert(requested_len >= 0);
}


int MkvReader::Length(long long* total, long long* avail)
{
    if (total == 0)
        return -1;

    if (avail == 0)
        return -1;

    *total = m_length;
    *avail = m_avail;

    return 0;
}


int MkvReader::LockPage(const mkvparser::BlockEntry* pBE)
{
    assert(m_async_pos < 0);
    assert(m_async_len < 0);

    if (pBE == 0)
        return 0;

    if (pBE->EOS())
        return 0;

    const mkvparser::Block* const pBlock = pBE->GetBlock();
    assert(pBlock);

    long long pos = pBlock->m_start;
    long len = static_cast<long>(pBlock->m_size);

    typedef cache_t::iterator iter_t;
    iter_t next;

    const DWORD page_size = m_info.dwPageSize;

    cache_t pages;

    if (m_cache.empty() || (pos < (*m_cache.front()).pos))
        next = m_cache.begin();
    else
    {
        const iter_t i = m_cache.begin();
        const iter_t j = m_cache.end();

        next = std::upper_bound(i, j, pos, PageLess());
        assert(next != i);

        const cache_t::value_type page_iter = *--iter_t(next);
        //Page& page = *page_iter;
        const LONGLONG page_end = page_iter->pos + page_size;

        if (pos < page_end)  //cache hit
        {
            Read(page_iter, pos, len, 0);
            pages.push_back(page_iter);  //++page.cRef;
        }
    }

    while (len > 0)
    {
        if ((next == m_cache.end()) || ((*next)->pos > pos))
        {
#if 1
            return mkvparser::E_BUFFER_NOT_FULL;
#else
            iter_t curr;

            const int status = InsertPage(next, pos, curr);
            assert(status == 0);

            if (status)  //should never happen
                return status;

            const cache_t::value_type page_iter = *curr;
            Page& page = *page_iter;

            next = ++iter_t(curr);
            assert((next == m_cache.end()) || ((*next)->pos > page.pos));

            Read(page_iter, pos, len, 0);
            pages.push_back(page_iter);  //++page.cRef;
#endif
        }
        else
        {
            const cache_t::value_type page_iter = *next++;
            Page& page = *page_iter;
            assert(page.pos <= pos);
            assert(pos < (page.pos + page_size));

            Read(page_iter, pos, len, 0);
            pages.push_back(page_iter);  //++page.cRef;
        }
    }

    iter_t pages_iter = pages.begin();
    const iter_t pages_end = pages.end();

    while (pages_iter != pages_end)
    {
        const cache_t::value_type page_iter = *pages_iter++;
        Page& page = *page_iter;
        ++page.cRef;
    }

    return 0;  //success
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


#if 0
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
#if 0 //def _DEBUG
    odbgstream os;
    os << "MkvReader::DestroyRegions: cache.size="
       << m_cache.size()
       << " free_pages.size="
       << m_free_pages.size()
       << " regions.size="
       << m_regions.size()
       << endl;
#endif

    m_cache.clear();
    m_free_pages.clear();

    while (!m_regions.empty())
    {
        Region& r = m_regions.front();

        //const BOOL b = VirtualFree(r.ptr, 0, MEM_RELEASE);
        const BOOL b = HeapFree(GetProcessHeap(), 0, r.ptr);
        assert(b);

        m_regions.pop_front();
    }
}


#if 0
void MkvReader::PurgeOne()
{
    assert(m_async_len <= 0);  //no async read in progress

    if (m_cache.empty())
        return;

    {
        const cache_t::value_type page_iter = m_cache.front();
        const Page& page = *page_iter;
        assert(page.cRef >= 0);

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
        assert(page.cRef >= 0);

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
            assert(page.cRef >= 0);

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
#endif


void MkvReader::Purge(LONGLONG pos)
{
#ifdef DEBUG_PURGE
    const free_pages_t::size_type old_size = m_free_pages.size();

    LONGLONG old_first_pos = -1;

    if (!m_cache.empty())
    {
        const cache_t::value_type page_iter = m_cache.front();
        const Page& page = *page_iter;
        old_first_pos = page.pos;
    }
#endif

    int i = 0;
    enum { max_count = 16 };

    while (!m_cache.empty() && (i < max_count))
    {
        const cache_t::value_type page_iter = m_cache.front();

        const Page& page = *page_iter;

        if (page.cRef < 0)  //async read in progress
            break;

        if (page.cRef > 0)  //locked
            break;

        assert(page.pos >= 0);
        assert(page.len > 0);

        const ULONG page_size = page.len; //m_info.dwPageSize;
        const LONGLONG page_end = page.pos + page_size;

        if ((pos >= 0) && (pos < page_end))
            break;

        m_cache.pop_front();

        const free_pages_t::value_type value(page.pos, page_iter);
        m_free_pages.insert(value);

        ++i;
    }

#ifdef DEBUG_PURGE
    const free_pages_t::size_type new_size = m_free_pages.size();
    const free_pages_t::size_type n = new_size - old_size;

    if (n)
    {
        LONGLONG new_first_pos = -1;

        if (!m_cache.empty())
        {
            const cache_t::value_type page_iter = m_cache.front();
            const Page& page = *page_iter;
            new_first_pos = page.pos;
        }

        odbgstream os;
        os << "mkvreader::purge: n=" << n
           << " cache.size=" << m_cache.size()
           << " free.size=" << new_size
           << " regions.size=" << m_regions.size()
           << " old_first_pos=" << old_first_pos
           << " new_first_pos=" << new_first_pos
           << endl;
    }
#endif
}


void MkvReader::Clear()
{
    while (!m_cache.empty())
    {
        const cache_t::value_type page_iter = m_cache.front();

        const Page& page = *page_iter;

        if (page.cRef < 0)  //async read in progress
            break;

        if (page.cRef > 0)  //locked
            break;

        assert(page.pos >= 0);
        assert(page.len > 0);

        m_cache.pop_front();

        const free_pages_t::value_type value(page.pos, page_iter);
        m_free_pages.insert(value);
    }

    m_avail = 0;

#if 0 //def _DEBUG
    odbgstream os;
    os << "mkvreader::clear:"
       << " cache.size=" << m_cache.size()
       << " free.size=" << m_free_pages.size()
       << " regions.size=" << m_regions.size()
       << endl;
#endif
}


HRESULT MkvReader::Seek(LONGLONG pos_)
{
    if (pos_ < 0)
        return E_INVALIDARG;

    if ((m_length >= 0) && (pos_ > m_length))
        return E_INVALIDARG;

    const DWORD page_size = m_info.dwPageSize;
    const QWORD pos = page_size * QWORD(pos_ / page_size);

    return m_pStream->SetCurrentPosition(pos);
}


LONGLONG MkvReader::GetCurrentPosition() const
{
    QWORD pos;

    const HRESULT hr = m_pStream->GetCurrentPosition(&pos);

    if (FAILED(hr))
        return -1;

    return pos;
}


void MkvReader::ResetAvailable(LONGLONG avail)
{
    m_avail = avail;
    assert(m_avail >= 0);
}


LONGLONG MkvReader::GetAvailable() const
{
    return m_avail;
}


bool MkvReader::IsFreeEmpty() const
{
    return m_free_pages.empty();
}


DWORD MkvReader::GetPageSize() const
{
    return m_info.dwPageSize;
}

void MkvReader::AllocateFree(ULONG len)
{
    const DWORD page_size = m_info.dwPageSize;
    const DWORD page_count = (len + page_size - 1) / page_size;

    while (m_free_pages.size() < page_count)
        CreateRegion();
}


HRESULT MkvReader::AsyncReadInit(
    LONGLONG pos,
    LONG len,
    IMFAsyncCallback* pCB)
{
    assert(pos >= 0);
    assert((m_length < 0) || (pos <= m_length));
    assert(len > 0);
    assert(pCB);
    assert(m_async_pos < 0);
    assert(m_async_len < 0);

    if (m_length >= 0)
    {
        assert(pos <= m_length);

        if (pos >= m_length)  //EOF
        {
            m_avail = m_length;  //kind of bogus
            return S_OK;
        }

        const LONGLONG end = pos + len;

        if (end > m_length)
            len -= static_cast<LONG>(end - m_length);

        assert((pos + len) <= m_length);
    }

    const LONGLONG end = pos + len;

    QWORD curr_pos;

    HRESULT hr = m_pStream->GetCurrentPosition(&curr_pos);
    assert(SUCCEEDED(hr));  //TODO
    //assert((curr_pos % m_info.dwPageSize) == 0);

    //QWORD next_pos = curr_pos;

    if (QWORD(pos) > curr_pos)
    {
        const LONGLONG pad_len = pos - curr_pos;
        assert(pad_len <= LONG_MAX);

        //odbgstream os;
        //
        //os << "AsyncReadInit: curr_pos=" << curr_pos
        //   << " requested pos=" << pos
        //   << " requested-curr=" << (pos - curr_pos)
        //   << " requested-curr(pages)="
        //   << (((pos - curr_pos) + m_info.dwPageSize - 1) /
        //        m_info.dwPageSize)
        //   << " requested len=" << len
        //   << " pad len=" << pad_len
        //   << " adjusted len=" << (len + pad_len)
        //   << endl;

        pos = curr_pos;
        len += static_cast<LONG>(pad_len);
        assert((pos + len) == end);
    }

    m_async_pos = pos;
    m_async_len = len;

    return AsyncReadContinue(pCB);

#if 0
    typedef cache_t::iterator iter_t;
    iter_t next;

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
        assert(page.pos >= 0);

        const ULONG page_size = page.len;
        assert(page_size > 0);

        const LONGLONG page_end = page.pos + page_size;

        if (pos < page_end)  //cache hit
        {
            Read(page_iter, pos, len, 0);

            //if (m_avail < pos)
            //    m_avail = pos;

            //TODO: move this outside of this predicate block?
            if (next_pos < QWORD(page_end))
                next_pos = page_end;
        }
    }

    while (len > 0)
    {
        if ((next == m_cache.end()) || ((*next)->pos > pos))
        {
            iter_t curr;

            const HRESULT hr = AsyncReadPage(next, pos, pCB, curr);

            if (FAILED(hr))
                return hr;

            const cache_t::value_type page_iter = *curr;

            Page& page = *page_iter;
            assert(page.pos <= pos);

            if (hr == S_FALSE)  //async read in progress
            {
                assert(page.cRef < 0);

                m_async_pos = pos;
                m_async_len = len;

                return S_FALSE;  //tell caller to wait for completion
            }

            next = ++iter_t(curr);
            assert((next == m_cache.end()) || ((*next)->pos > page.pos));

            Read(page_iter, pos, len, 0);

            //if (m_avail < pos)
            //    m_avail = pos;

            const LONGLONG page_end = page.pos + page.len;

            if (next_pos < QWORD(page_end))
                next_pos = page_end;
        }
        else
        {
            const cache_t::value_type page_iter = *next++;
            Page& page = *page_iter;
            assert(page.pos <= pos);
            assert(page.len > 0);
            assert(pos < (page.pos + page.len));

            Read(page_iter, pos, len, 0);

            //if (m_avail < pos)
            //    m_avail = pos;

            const LONGLONG page_end = page.pos + page.len;

            if (next_pos < QWORD(page_end))
                next_pos = page_end;
        }
    }

    m_avail = end;

    if (next_pos > curr_pos)
    {
        hr = m_pStream->SetCurrentPosition(next_pos);
        assert(SUCCEEDED(hr));
    }

    return S_OK;
#endif
}


HRESULT MkvReader::AsyncReadContinue(
    IMFAsyncCallback* pCB)
{
    assert(pCB);
    //assert(!m_cache.empty());

    LONG& len = m_async_len;
    assert(len > 0);  //TODO: relax this

    LONGLONG& pos = m_async_pos;
    //assert(pos > (*m_cache.front()).pos);
    //assert((pos % m_info.dwPageSize) == 0);
    assert((m_length < 0) || (pos < m_length));
    assert((m_length < 0) || ((pos + len) <= m_length));

    //const LONGLONG end = pos + len;

    typedef cache_t::iterator iter_t;
    iter_t next;

    //QWORD curr_pos;
    //HRESULT hr = m_pStream->GetCurrentPosition(&curr_pos);
    //assert(SUCCEEDED(hr));  //TODO
    //QWORD next_pos = curr_pos;
    LONGLONG last_pos = -1;

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
        assert(page.cRef >= 0);
        assert(page.pos <= pos);
        assert(page.len > 0);

        const LONGLONG page_end = page.pos + page.len;

        if (pos < page_end)  //cache hit
        {
            Read(page_iter, pos, len, 0);

            //if (m_avail < pos)
            //    m_avail = pos;

            //if (next_pos < QWORD(page_end))
            //    next_pos = page_end;
        }

        last_pos = page_end;

        if (last_pos > m_avail)
            m_avail = last_pos;
    }

    while (len > 0)
    {
        if ((next == m_cache.end()) || ((*next)->pos > pos))
        {
            iter_t curr;

            const HRESULT hr = AsyncReadPage(next, pos, pCB, curr);

            if (FAILED(hr))
                return hr;

            const cache_t::value_type page_iter = *curr;

            Page& page = *page_iter;
            assert(page.pos <= pos);

            if (hr == S_FALSE)  //async read in progress
            {
                assert(page.cRef < 0);
                return S_FALSE;  //tell caller to wait for completion
            }

            next = ++iter_t(curr);
            assert((next == m_cache.end()) || ((*next)->pos > page.pos));

            Read(page_iter, pos, len, 0);

            //if (m_avail < pos)
            //    m_avail = pos;

            const LONGLONG page_end = page.pos + page.len;

            //if (next_pos < QWORD(page_end))
            //    next_pos = page_end;

            last_pos = page_end;
        }
        else
        {
            const cache_t::value_type page_iter = *next++;

            Page& page = *page_iter;
            assert(page.cRef >= 0);
            assert(page.pos <= pos);
            assert(page.len > 0);
            assert(pos < (page.pos + page.len));

            Read(page_iter, pos, len, 0);

            //if (m_avail < pos)
            //    m_avail = pos;

            const LONGLONG page_end = page.pos + page.len;

            //if (next_pos < QWORD(page_end))
            //    next_pos = page_end;

            last_pos = page_end;
        }

        if (last_pos > m_avail)
            m_avail = last_pos;
    }

    assert(last_pos >= 0);
    assert(m_avail >= last_pos);

    pos = -1;
    len = -1;

    QWORD curr_pos;

    HRESULT hr = m_pStream->GetCurrentPosition(&curr_pos);
    assert(SUCCEEDED(hr));  //TODO

    if (QWORD(last_pos) > curr_pos)
    {
        hr = m_pStream->SetCurrentPosition(last_pos);
        assert(SUCCEEDED(hr));
    }

    return S_OK;
}


HRESULT MkvReader::AsyncReadCancel()
{
    m_async_len = -1;
    m_async_pos = -1;

    return S_OK;
}


HRESULT MkvReader::AsyncReadCompletion(IMFAsyncResult* pResult)
{
    assert(pResult);
    assert(m_async_pos >= 0);
    assert((m_length < 0) || (m_async_pos < m_length));
    assert(m_async_len > 0);
    assert((m_length < 0) || ((m_async_pos + m_async_len) <= m_length));
    assert(!m_cache.empty());

    typedef cache_t::iterator iter_t;

    const iter_t i = m_cache.begin();
    const iter_t j = m_cache.end();

    const iter_t next = std::upper_bound(i, j, m_async_pos, PageLess());
    assert(next != i);

    const iter_t curr = --iter_t(next);
    const cache_t::value_type page_iter = *curr;

    Page& page = *page_iter;
    assert(page.pos >= 0);
    assert(page.pos <= m_async_pos);
    assert(page.cRef < 0);  //async read in progress

    page.cRef = 0;  //unmark this page, now that I/O is complete

    ULONG cbRead;

    HRESULT hr = m_pStream->EndRead(pResult, &cbRead);
    assert(FAILED(hr) || (cbRead <= m_info.dwPageSize));

    if (SUCCEEDED(hr))
    {
        page.len = cbRead;
        assert(page.len <= m_info.dwPageSize);

#if 1 //def _DEBUG
        //odbgstream os;
        //os << "AsyncReadCompletion: just called EndRead; cbRead="
        //   << cbRead
        //   << " GetCurrPos=";

        QWORD new_pos;

        hr = m_pStream->GetCurrentPosition(&new_pos);
        assert(SUCCEEDED(hr));
        assert(new_pos >= QWORD(page.pos + page.len));

        //os << new_pos << endl;
#endif

        if (page.len < m_info.dwPageSize)
        {
            //We read fewer bytes than requested.  This is a normal event,
            //such as when we read the very last page of the file.

            const LONGLONG length = page.pos + page.len;
            assert((m_length < 0) || (length <= m_length));

            if (m_length >= 0)  //length is defined
            {
                if (length < m_length)  //weird: fewer bytes than total length
                {
                    hr = E_FAIL;        //treat this as an I/O error

#ifdef _DEBUG
                    odbgstream os;
                    os << "\nmkvreader::AsyncReadCompletion: "
                       << "ERROR - incomplete async read\n"
                       << endl;
#endif
                }
            }
            else //network source with unknown length
            {
                m_length = length;
                assert(m_async_pos <= m_length);

                if ((m_async_pos + m_async_len) > m_length)
                    m_async_len = static_cast<LONG>(m_length - m_async_pos);
            }
        }
    }

    if (FAILED(hr) || (cbRead == 0))
    {
        m_cache.erase(curr);

        page.pos = -1;  //means "we don't have any data on this page"
        page.len = 0;

        const free_pages_t::value_type value(page.pos, page_iter);
        m_free_pages.insert(value);

        m_async_len = -1;
        m_async_pos = -1;

        return hr;
    }

    const LONGLONG last_pos = page.pos + page.len;

    if (last_pos > m_avail)
        m_avail = last_pos;

    Read(page_iter, m_async_pos, m_async_len, 0);
    assert(m_async_pos >= 0);
    assert(m_async_len >= 0);
    assert((m_length < 0) || (m_async_pos <= m_length));
    assert((m_length < 0) || ((m_async_pos + m_async_len) <= m_length));

    if (m_async_len > 0)
        return S_FALSE;

    m_async_pos = -1;
    m_async_len = -1;

    return S_OK;
}


HRESULT MkvReader::AsyncReadPage(
    cache_t::iterator next,
    LONGLONG pos,
    IMFAsyncCallback* pCB,
    cache_t::iterator& curr)
{
    assert(pos >= 0);
    assert((m_length < 0) || (pos < m_length));

    const DWORD page_size = m_info.dwPageSize;

    //TODO: purge as necessary

    if (m_free_pages.empty())
        CreateRegion();

    const LONGLONG key = page_size * LONGLONG(pos / page_size);
    assert((next == m_cache.end()) || ((*next)->pos > key));
    assert((m_length < 0) || (key < m_length));

    free_pages_t::iterator free_page = m_free_pages.find(key);

    if (free_page == m_free_pages.end())  //key not found
        free_page = m_free_pages.begin(); //just pick one

    const pages_vector_t::iterator page_iter = free_page->second;

    Page& page = *page_iter;
    assert(page.cRef == 0);

    if (page.pos == key)  //re-use the free page as is
    {
        assert(page.len > 0);

        m_free_pages.erase(free_page);  //page is no longer free
        curr = m_cache.insert(next, page_iter);

        return S_OK;
    }

#if 1 //def _DEBUG
    QWORD new_pos;

    HRESULT hr = m_pStream->GetCurrentPosition(&new_pos);
    assert(SUCCEEDED(hr));
    assert(QWORD(key) >= new_pos);
#endif

    hr = m_pStream->SetCurrentPosition(key);

    if (FAILED(hr))
        return hr;

    const Region& r = *page.region;
    const pages_vector_t::size_type offset = page_iter - r.pages.begin();

    //this might require some tweaking, since we now allow page size
    //to vary across pages.
    BYTE* const ptr = page.region->ptr + offset * size_t(page_size);

    //we always request the max number of bytes for a page
    hr = m_pStream->BeginRead(ptr, page_size, pCB, 0);

    if (FAILED(hr))
        return hr;

    //os << "AsyncReadPage: just called BeginRead; cb=" << page_size
    //   << "; currpos+cb=" << (key + page_size)
    //   << "; GetCurrPos=";

    //hr = m_pStream->GetCurrentPosition(&new_pos);
    //assert(SUCCEEDED(hr));

    //os << new_pos << endl;

    page.pos = key;
    page.len = 0;    //we don't know actual len until async read completes
    page.cRef = -1;  //means "async read in progress"

    m_free_pages.erase(free_page);  //page is no longer free
    curr = m_cache.insert(next, page_iter);

    return S_FALSE;
}
