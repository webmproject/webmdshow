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

    int Read(long long position, long length, unsigned char* buffer);
    int Length(long long* total, long long* available);

    void LockPage(const mkvparser::BlockEntry*);
    void UnlockPage(const mkvparser::BlockEntry*);
    //void Purge();

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

    //TODO: implement region affinity
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

    QWORD m_length;
    SYSTEM_INFO m_info;

    void Read(
        pages_vector_t::const_iterator,
        long long&,
        long&,
        unsigned char**) const;

    cache_t::iterator InsertPage(cache_t::iterator, LONGLONG);

    void CreateRegion();
    void DestroyRegions();

    int PurgeFront();
    int PurgeBack();
    void PurgeOne();

};

