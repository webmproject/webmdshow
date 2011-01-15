#pragma once
#include "mkvparser.hpp"
#include <windows.h>
#include <mfidl.h>
#include <deque>
#include <vector>
#include <list>
//#include <functional>
#include <map>

class MkvReader : public mkvparser::IMkvReader
{
    MkvReader(const MkvReader&);
    MkvReader& operator=(const MkvReader&);

public:

    explicit MkvReader(IMFByteStream*);
    virtual ~MkvReader();

    HRESULT EnableBuffering(LONGLONG duration_reftime) const;

    int Read(long long position, long length, unsigned char* buffer);
    int Length(long long* total, long long* available);

    int LockPage(const mkvparser::BlockEntry*);
    void UnlockPage(const mkvparser::BlockEntry*);

    void ResetAvailable();

    HRESULT AsyncReadInit(
        LONGLONG pos,
        LONG len,
        IMFAsyncCallback* pCB);

    HRESULT AsyncReadCompletion(IMFAsyncResult*);
    HRESULT AsyncReadContinue(IMFAsyncCallback*);

    void Purge(ULONGLONG);

private:

    IMFByteStream* const m_pStream;

    struct Region;
    struct Page;

    typedef std::vector<Page> pages_vector_t;

    struct Page
    {
        int cRef;
        LONGLONG pos;
        Region* region;
    };

    struct Region
    {
        BYTE* ptr;
        pages_vector_t pages;
    };

    typedef std::list<Region> regions_t;
    regions_t m_regions;

    typedef std::multimap<LONGLONG, pages_vector_t::iterator> free_pages_t;
    free_pages_t m_free_pages;

    typedef std::deque<pages_vector_t::iterator> cache_t;
    cache_t m_cache;

    struct PageLess //: std::binary_function<Page, LONGLONG, bool>
    {
        bool operator()(cache_t::value_type lhs, LONGLONG pos) const
        {
            return (lhs->pos < pos);
        }

        bool operator()(LONGLONG pos, cache_t::value_type rhs) const
        {
            return (pos < rhs->pos);
        }

        bool operator()(cache_t::value_type lhs, cache_t::value_type rhs) const
        {
            return (lhs->pos < rhs->pos);
        }
    };

    SYSTEM_INFO m_info;
    QWORD m_length;
    LONGLONG m_avail;

    void Read(
        pages_vector_t::const_iterator,
        long long&,
        long&,
        unsigned char**) const;

    int InsertPage(
        cache_t::iterator next,
        LONGLONG pos,
        cache_t::iterator& curr);

    HRESULT AsyncReadPage(
        cache_t::iterator next,
        LONGLONG pos,
        IMFAsyncCallback* pCB,
        cache_t::iterator& curr);

    //async read
    LONGLONG m_async_pos;  //key of page supplying async buf
    LONG m_async_len;      //what remains to be read

    void CreateRegion();
    void DestroyRegions();

    //int PurgeFront();
    //int PurgeBack();
    void PurgeOne();

};

