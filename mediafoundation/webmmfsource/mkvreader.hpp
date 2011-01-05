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

    //Call BeginRead with pos, len, async callback ptr, state ptr
    //pos is determined by cluster we're attempting to load
    //len (for a single page) is fixed
    //
    //When the Invoke method of the async call is called, we're
    //supposed to call EndRead.
    //
    //We'll have to put the page in some kind of pending area.
    //There will be multiple pages for a cluster: do they all go
    //in the pending area?  State object will have to keep track
    //of total posn range of all pages in this cluster.
    //
    //Can we use this reader object if an async read is in progress?
    //
    //This object needs to know pos and size of thing item to read
    //asynchronously.
    //
    //Call LoadCluster.  If status = success then we're done.
    //Otherwise we get a pos and size.  (This could be cluster ID,
    //or cluster size, or cluster payload.)
    //
    //If status = buffer underflow, then we must begin async read.
    //The hard problem is deciding how to handle the case when the
    //size requested is larger than a page.
    //
    //We could have the caller keep track of where segment::m_pos is
    //relative to cluster we're loading.
    //
    //We could issue a bunch of async read requests simultaneously.
    //This would require multiple async callback objects, however.
    //
    //We could use the state object to keep track of total number
    //of pages, and how many have been read so far.
    //
    //We could simply read a new page, and then retry.  This is
    //simple, but slightly less efficient, since we know how
    //much we need to read when the attempt the load.
    //
    //We have to do these things: (1) parse the file to determine
    //pos and size of the cluster, and (2) parse the cluster
    //to determine pos and size of each block, and (3) lock
    //the block in the cache so we can read frames from it.
    //
    //Locking/reading a block is the smaller problem, since
    //we have already parsed the file.  We're just need to
    //asynchronously load.  This does mean the locking a
    //block in the cache will now be an asynchrous operation.

    //class AsyncReadState
    //{
    //    AsyncReadState(const AsyncReadState&);
    //    AsyncReadState& operator=(const AsyncReadState&);
    //private:
    //    int m_index;  //which page we're on
    //    int m_total;  //how many pages total
    //};

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
    LONG m_async_len;     //what remains to be read

    void CreateRegion();
    void DestroyRegions();

    //int PurgeFront();
    //int PurgeBack();
    void PurgeOne();

};

