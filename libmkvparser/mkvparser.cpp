// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "mkvparser.hpp"
#include <cassert>
#include <algorithm>
#include <limits>
#include <climits>
#include <vfwmsgs.h>
#include <sstream>
#include <iomanip>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
using std::hex;
#endif

__int64 MkvParser::ReadUInt(
    IMkvFile* pFile,
    LONGLONG pos,
    long& len)
{
    assert(pFile);
    assert(pos >= 0);

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);
    assert(SUCCEEDED(hr));
    assert(pos < available);
    assert((available - pos) >= 1);  //assume here max u-int len is 8

    BYTE b;

    hr = pFile->MkvRead(pos, 1, &b);

    if (FAILED(hr))
        return __int64(hr);

    assert(hr == S_OK);

    if (b & 0x80)       //1000 0000
    {
        len = 1;
        b &= 0x7F;      //0111 1111
    }
    else if (b & 0x40)  //0100 0000
    {
        len = 2;
        b &= 0x3F;      //0011 1111
    }
    else if (b & 0x20)  //0010 0000
    {
        len = 3;
        b &= 0x1F;      //0001 1111
    }
    else if (b & 0x10)  //0001 0000
    {
        len = 4;
        b &= 0x0F;      //0000 1111
    }
    else if (b & 0x08)  //0000 1000
    {
        len = 5;
        b &= 0x07;      //0000 0111
    }
    else if (b & 0x04)  //0000 0100
    {
        len = 6;
        b &= 0x03;      //0000 0011
    }
    else if (b & 0x02)  //0000 0010
    {
        len = 7;
        b &= 0x01;      //0000 0001
    }
    else
    {
        assert(b & 0x01);  //0000 0001
        len = 8;
        b = 0;             //0000 0000
    }

    assert((available - pos) >= len);

    __int64 result = b;
    ++pos;

    for (long i = 1; i < len; ++i)
    {
        hr = pFile->MkvRead(pos, 1, &b);

        if (FAILED(hr))
            return __int64(hr);

        assert(hr == S_OK);

        result <<= 8;
        result |= b;

        ++pos;
    }

    return result;
}


__int64 MkvParser::GetUIntLength(
    IMkvFile* pFile,
    LONGLONG pos,
    long& len)
{
    assert(pFile);
    assert(pos >= 0);

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);
    assert(SUCCEEDED(hr));
    assert(available <= total);

    if (pos >= available)
        return pos;  //too few bytes available

    BYTE b;

    hr = pFile->MkvRead(pos, 1, &b);

    if (FAILED(hr))
        return __int64(hr);

    assert(hr == S_OK);

    if (b == 0)  //we can't handle u-int values larger than 8 bytes
        return VFW_E_INVALID_FILE_FORMAT;

    BYTE m = 0x80;
    len = 1;

    while (!(b & m))
    {
        m >>= 1;
        ++len;
    }

    return 0;  //success
}


__int64 MkvParser::SyncReadUInt(
    IMkvFile* pFile,
    LONGLONG pos,
    LONGLONG stop,
    long& len)
{
    assert(pFile);

    if (pos >= stop)
        return VFW_E_INVALID_FILE_FORMAT;

    BYTE b;

    HRESULT hr = pFile->MkvRead(pos, 1, &b);

    if (FAILED(hr))
        return hr;

    if (hr != S_OK)
        return VFW_E_BUFFER_UNDERFLOW;

    if (b == 0)  //we can't handle u-int values larger than 8 bytes
        return VFW_E_INVALID_FILE_FORMAT;

    BYTE m = 0x80;
    len = 1;

    while (!(b & m))
    {
        m >>= 1;
        ++len;
    }

    if ((pos + len) > stop)
        return VFW_E_INVALID_FILE_FORMAT;

    __int64 result = b & (~m);
    ++pos;

    for (int i = 1; i < len; ++i)
    {
        hr = pFile->MkvRead(pos, 1, &b);

        if (FAILED(hr))
            return hr;

        if (hr != S_OK)
            return VFW_E_BUFFER_UNDERFLOW;

        result <<= 8;
        result |= b;

        ++pos;
    }

    return result;
}


__int64 MkvParser::UnserializeUInt(
    IMkvFile* pFile,
    LONGLONG pos,
    __int64 size)
{
    assert(pFile);
    assert(pos >= 0);
    assert(size > 0);
    assert(size <= 8);

    //LONGLONG total, available;
    //HRESULT hr = pFile->MkvLength(&total, &available);
    //assert(SUCCEEDED(hr));
    //assert(available <= total);
    //assert((pos + size) <= available);

    __int64 result = 0;

    for (__int64 i = 0; i < size; ++i)
    {
        BYTE b;

        const HRESULT hr = pFile->MkvRead(pos, 1, &b);

        if (FAILED(hr))
            return hr;

        result <<= 8;
        result |= b;

        ++pos;
    }

    return result;
}


float MkvParser::Unserialize4Float(
    IMkvFile* pFile,
    LONGLONG pos)
{
    assert(pFile);
    assert(pos >= 0);

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);
    assert(SUCCEEDED(hr));
    assert(available <= total);
    assert((pos + 4) <= available);

    float result;

    BYTE* const p = (BYTE*)&result;
    BYTE* q = p + 4;

    for (;;)
    {
        hr = pFile->MkvRead(pos, 1, --q);
        assert(hr == S_OK);

        if (q == p)
            break;

        ++pos;
    }

    return result;
}


double MkvParser::Unserialize8Double(
    IMkvFile* pFile,
    LONGLONG pos)
{
    assert(pFile);
    assert(pos >= 0);

    double result;

    BYTE* const p = (BYTE*)&result;
    BYTE* q = p + 8;

    for (;;)
    {
        const HRESULT hr = pFile->MkvRead(pos, 1, --q);
        hr;
        assert(hr == S_OK);

        if (q == p)
            break;

        ++pos;
    }

    return result;
}


signed char MkvParser::Unserialize1SInt(
    IMkvFile* pFile,
    LONGLONG pos)
{
    assert(pFile);
    assert(pos >= 0);

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);
    assert(SUCCEEDED(hr));
    assert(available <= total);
    assert(pos < available);

    signed char result;

    hr = pFile->MkvRead(pos, 1, (BYTE*)&result);
    assert(hr == S_OK);

    return result;
}


SHORT MkvParser::Unserialize2SInt(
    IMkvFile* pFile,
    LONGLONG pos)
{
    assert(pFile);
    assert(pos >= 0);

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);
    assert(SUCCEEDED(hr));
    assert(available <= total);
    assert((pos + 2) <= available);

    SHORT result;

    BYTE* const p = (BYTE*)&result;
    BYTE* q = p + 2;

    for (;;)
    {
        hr = pFile->MkvRead(pos, 1, --q);
        assert(hr == S_OK);

        if (q == p)
            break;

        ++pos;
    }

    return result;
}


bool MkvParser::Match(
    IMkvFile* pFile,
    LONGLONG& pos,
    ULONG id_,
    __int64& val)
{
    assert(pFile);
    assert(pos >= 0);

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);
    hr;
    assert(SUCCEEDED(hr));
    assert(available <= total);

    long len;

    const __int64 id = ReadUInt(pFile, pos, len);
    assert(id >= 0);
    assert(len > 0);
    assert(len <= 8);
    assert((pos + len) <= available);

    if (id != id_)
        return false;

    pos += len;  //consume id

    const __int64 size = ReadUInt(pFile, pos, len);
    assert(size >= 0);
    assert(size <= 8);
    assert(len > 0);
    assert(len <= 8);
    assert((pos + len) <= available);

    pos += len;  //consume length of size of payload

    val = UnserializeUInt(pFile, pos, size);
    assert(val >= 0);

    pos += size;  //consume size of payload

    return true;
}


bool MkvParser::Match(
    IMkvFile* pFile,
    LONGLONG& pos,
    ULONG id_,
    std::string& val)
{
    assert(pFile);
    assert(pos >= 0);

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);
    assert(SUCCEEDED(hr));
    assert(available <= total);

    long len;

    const __int64 id = ReadUInt(pFile, pos, len);
    assert(id >= 0);
    assert(len > 0);
    assert(len <= 8);
    assert((pos + len) <= available);

    if (id != id_)
        return false;

    pos += len;  //consume id

    const __int64 size = ReadUInt(pFile, pos, len);
    assert(size >= 0);
    assert(len > 0);
    assert(len <= 8);
    assert((pos + len) <= available);

    pos += len;  //consume length of size of payload
    assert((pos + size) <= available);

    val.clear();

    const bytes_t::size_type size_ = static_cast<bytes_t::size_type>(size);
    val.reserve(size_);

    for (__int64 i = 0; i < size; ++i)
    {
        char c;

        hr = pFile->MkvRead(pos + i, 1, (BYTE*)&c);
        assert(hr == S_OK);

        if (c == '\0')
            break;

        val.append(1, c);
    }

    pos += size;  //consume size of payload

    return true;
}


bool MkvParser::Match(
    IMkvFile* pFile,
    LONGLONG& pos,
    ULONG id,
    std::wstring& val)
{
    std::string str;

    if (!Match(pFile, pos, id, str))
        return false;

    const int cch = MultiByteToWideChar(
                        CP_UTF8,
                        0,  //TODO: MB_ERR_INVALID_CHARS
                        str.c_str(),
                        -1,  //include NUL terminator in result
                        0,
                        0);  //request length

    assert(cch > 0);

    const size_t cb = cch * sizeof(wchar_t);
    wchar_t* const wstr = (wchar_t*)_alloca(cb);

    const int cch2 = MultiByteToWideChar(
                        CP_UTF8,
                        0,  //TODO: MB_ERR_INVALID_CHARS
                        str.c_str(),
                        -1,
                        wstr,
                        cch);

    cch2;
    assert(cch2 > 0);
    assert(cch2 == cch);

    val.assign(wstr);
    return true;
}


bool MkvParser::Match(
    IMkvFile* pFile,
    LONGLONG& pos,
    ULONG id_,
    bytes_t& val)
{
    assert(pFile);
    assert(pos >= 0);

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);
    assert(SUCCEEDED(hr));
    assert(available <= total);

    long len;

    const __int64 id = ReadUInt(pFile, pos, len);
    assert(id >= 0);
    assert(len > 0);
    assert(len <= 8);
    assert((pos + len) <= available);

    if (id != id_)
        return false;

    pos += len;  //consume id

    const __int64 size = ReadUInt(pFile, pos, len);
    assert(size >= 0);
    assert(len > 0);
    assert(len <= 8);
    assert((pos + len) <= available);

    pos += len;  //consume length of size of payload
    assert((pos + size) <= available);

    val.clear();

    const bytes_t::size_type size_ = static_cast<bytes_t::size_type>(size);
    val.reserve(size_);

    for (__int64 i = 0; i < size; ++i)
    {
        BYTE b;

        hr = pFile->MkvRead(pos + i, 1, &b);
        assert(hr == S_OK);

        val.push_back(b);
    }

    pos += size;  //consume size of payload
    return true;
}


bool MkvParser::Match(
    IMkvFile* pFile,
    LONGLONG& pos,
    ULONG id_,
    double& val)
{
    assert(pFile);
    assert(pos >= 0);

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);
    hr;
    assert(SUCCEEDED(hr));
    assert(available <= total);

    long idlen;
    const __int64 id = ReadUInt(pFile, pos, idlen);
    assert(id >= 0);  //TODO

    if (id != id_)
        return false;

    long sizelen;
    const __int64 size = ReadUInt(pFile, pos + idlen, sizelen);

    switch (size)
    {
        case 4:
        case 8:
            break;

        default:
            return false;
    }

    pos += idlen + sizelen;  //consume id and size fields
    assert((pos + size) <= available);

    if (size == 4)
        val = Unserialize4Float(pFile, pos);
    else
    {
        assert(size == 8);
        val = Unserialize8Double(pFile, pos);
    }

    pos += size;  //consume size of payload

    return true;
}


bool MkvParser::Match(
    IMkvFile* pFile,
    LONGLONG& pos,
    ULONG id_,
    SHORT& val)
{
    assert(pFile);
    assert(pos >= 0);

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);
    hr;
    assert(SUCCEEDED(hr));
    assert(available <= total);

    long len;

    const __int64 id = ReadUInt(pFile, pos, len);
    assert(id >= 0);
    assert((pos + len) <= available);

    if (id != id_)
        return false;

    pos += len;  //consume id

    const __int64 size = ReadUInt(pFile, pos, len);
    assert(size <= 2);
    assert((pos + len) <= available);

    pos += len;  //consume length of size of payload
    assert((pos + size) <= available);

    //TODO: generalize this to work for any size signed int
    if (size == 1)
        val = Unserialize1SInt(pFile, pos);
    else
        val = Unserialize2SInt(pFile, pos);

    pos += size;  //consume size of payload

    return true;
}


namespace MkvParser
{


__int64 EBMLHeader::Parse(
    IMkvFile* pFile,
    LONGLONG& pos)
{
    assert(pFile);

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);

    if (FAILED(hr))
        return hr;

    pos = 0;
    LONGLONG end = std::min<LONGLONG>(1024, available);

    for (;;)
    {
        BYTE b = 0;

        while (pos < end)
        {
            hr = pFile->MkvRead(pos, 1, &b);

            if (FAILED(hr))
                return hr;

            if (b == 0x1A)
                break;

            ++pos;
        }

        if (b != 0x1A)
        {
            if ((pos >= 1024) ||
                (available >= total) ||
                ((total - available) < 5))
            {
                return VFW_E_INVALID_FILE_FORMAT;
            }

            return available + 5;  //5 = 4-byte ID + 1st byte of size
        }

        if ((total - pos) < 5)
            return VFW_E_INVALID_FILE_FORMAT;

        if ((available - pos) < 5)
            return pos + 5;  //try again later

        long len;
        const __int64 result = ReadUInt(pFile, pos, len);

        if (result < 0)  //error
            return result;

        if (result == 0x0A45DFA3)  //ReadId masks-off length indicator bits
        {
            assert(len == 4);
            pos += len;
            break;
        }

        ++pos;  //throw away just the 0x1A byte, and try again
    }

    long len;
    __int64 result = GetUIntLength(pFile, pos, len);

    if (result < 0)  //error
        return result;

    if (result > 0)  //need more data
        return result;

    assert(len > 0);
    assert(len <= 8);

    if ((total -  pos) < len)
        return VFW_E_INVALID_FILE_FORMAT;

    if ((available - pos) < len)
        return pos + len;  //try again later

    result = ReadUInt(pFile, pos, len);

    if (result < 0)  //error
        return result;

    pos += len;  //consume u-int

    if ((total - pos) < result)
        return VFW_E_INVALID_FILE_FORMAT;

    if ((available - pos) < result)
        return pos + result;

    end = pos + result;

    m_version = 1;
    m_readVersion = 1;
    m_maxIdLength = 4;
    m_maxSizeLength = 8;
    m_docType = "matroska";
    m_docTypeVersion = 1;
    m_docTypeReadVersion = 1;

    while (pos < end)
    {
        if (Match(pFile, pos, 0x0286, m_version))
            __noop;

        else if (Match(pFile, pos, 0x02F7, m_readVersion))
            __noop;

        else if (Match(pFile, pos, 0x02F2, m_maxIdLength))
            __noop;

        else if (Match(pFile, pos, 0x02F3, m_maxSizeLength))
            __noop;

        else if (Match(pFile, pos, 0x0282, m_docType))
            __noop;

        else if (Match(pFile, pos, 0x0287, m_docTypeVersion))
            __noop;

        else if (Match(pFile, pos, 0x0285, m_docTypeReadVersion))
            __noop;

        else
        {
            result = ReadUInt(pFile, pos, len);
            assert(result > 0);
            assert(len > 0);
            assert(len <= 8);

            pos += len;
            assert(pos < end);

            result = ReadUInt(pFile, pos, len);
            assert(result >= 0);
            assert(len > 0);
            assert(len <= 8);

            pos += len + result;
            assert(pos <= end);
        }
    }

    assert(pos == end);

    return 0;
}


Segment::Segment(
    IMkvFile* pFile,
    __int64 start,
    __int64 size) :
    m_pFile(pFile),
    m_start(start),
    m_size(size),
    m_pos(start),
    //m_pSeekHead(0),
    m_pInfo(0),
    m_pTracks(0),
    m_pCues(0)
    //m_index(0)
{
}


Segment::~Segment()
{
    while (!m_clusters.empty())
    {
        Cluster* pCluster = m_clusters.front();
        assert(pCluster);

        m_clusters.pop_front();
        delete pCluster;
    }

    delete m_pTracks;
    delete m_pInfo;
    //delete m_pSeekHead;
}


__int64 Segment::CreateInstance(
    IMkvFile* pFile,
    LONGLONG pos,
    Segment*& pSegment)
{
    assert(pFile);
    assert(pos >= 0);

    pSegment = 0;

    LONGLONG total, available;

    HRESULT hr = pFile->MkvLength(&total, &available);
    hr;
    assert(SUCCEEDED(hr));
    assert(available <= total);

    //I would assume that in practice this loop would execute
    //exactly once, but we allow for other elements (e.g. Void)
    //to immediately follow the EBML header.  This is fine for
    //the source filter case (since the entire file is available),
    //but in the splitter case over a network we should probably
    //just give up early.  We could for example decide only to
    //execute this loop a maximum of, say, 10 times.

    while (pos < total)
    {
        //Read ID

        long len;
        __int64 result = GetUIntLength(pFile, pos, len);

        if (result)  //error, or too few available bytes
            return result;

        if ((pos + len) > total)
            return VFW_E_INVALID_FILE_FORMAT;

        if ((pos + len) > available)
            return pos + len;

        //TODO: if we liberalize the behavior of ReadUInt, we can
        //probably eliminate having to use GetUIntLength here.
        const __int64 id = ReadUInt(pFile, pos, len);

        if (id < 0)  //error
            return id;

        pos += len;  //consume ID

        //Read Size

        result = GetUIntLength(pFile, pos, len);

        if (result)  //error, or too few available bytes
            return result;

        if ((pos + len) > total)
            return VFW_E_INVALID_FILE_FORMAT;

        if ((pos + len) > available)
            return pos + len;

        //TODO: if we liberalize the behavior of ReadUInt, we can
        //probably eliminate having to use GetUIntLength here.
        const __int64 size = ReadUInt(pFile, pos, len);

        if (size < 0)
            return size;

        pos += len;  //consume length of size of element

        //Pos now points to start of payload

        if ((pos + size) > total)
            return VFW_E_INVALID_FILE_FORMAT;

        if (id == 0x08538067)  //Segment ID
        {
            pSegment = new (std::nothrow) Segment(pFile, pos, size);
            assert(pSegment);  //TODO

            return 0;    //success
        }

        pos += size;  //consume payload
    }

    assert(pos == total);

    pSegment = new (std::nothrow) Segment(pFile, pos, 0);
    assert(pSegment);  //TODO

    return 0;  //success (sort of)
}


__int64 Segment::ParseHeaders()
{
    //Outermost (level 0) segment object has been constructed,
    //and pos designates start of payload.  We need to find the
    //inner (level 1) elements.

    LONGLONG total, available;

    HRESULT hr = m_pFile->MkvLength(&total, &available);
    hr;
    assert(SUCCEEDED(hr));
    assert(available <= total);

    const __int64 stop = m_start + m_size;
    assert(stop <= total);
    assert(m_pos <= stop);

    bool bQuit = false;

    while ((m_pos < stop) && !bQuit)
    {
        __int64 pos = m_pos;

        long len;
        __int64 result = GetUIntLength(m_pFile, pos, len);

        if (result)  //error, or too few available bytes
            return result;

        if ((pos + len) > stop)
            return VFW_E_INVALID_FILE_FORMAT;

        if ((pos + len) > available)
            return pos + len;

        const __int64 idpos = pos;
        const __int64 id = ReadUInt(m_pFile, idpos, len);

        if (id < 0)  //error
            return id;

        pos += len;  //consume ID

        //Read Size

        result = GetUIntLength(m_pFile, pos, len);

        if (result)  //error, or too few available bytes
            return result;

        if ((pos + len) > stop)
            return VFW_E_INVALID_FILE_FORMAT;

        if ((pos + len) > available)
            return pos + len;

        const __int64 size = ReadUInt(m_pFile, pos, len);

        if (size < 0)
            return size;

        pos += len;  //consume length of size of element

        //Pos now points to start of payload

        if ((pos + size) > stop)
            return VFW_E_INVALID_FILE_FORMAT;

        //We read EBML elements either in total or nothing at all.

        if ((pos + size) > available)
            return pos + size;

        if (id == 0x0549A966)  //Segment Info ID
        {
            assert(m_pInfo == 0);
            m_pInfo = new (std::nothrow) SegmentInfo(this, pos, size);
            assert(m_pInfo);  //TODO
        }
        else if (id == 0x0654AE6B)  //Tracks ID
        {
            assert(m_pTracks == 0);
            m_pTracks = new (std::nothrow) Tracks(this, pos, size);
            assert(m_pTracks);  //TODO
        }
        else if (id == 0x0C53BB6B)  //Cues ID
        {
            assert(m_pCues == 0);
            m_pCues = new (std::nothrow) Cues(this, pos, size);
            assert(m_pCues);  //TODO
        }
        else if (id == 0x0F43B675)  //Cluster ID
        {
            bQuit = true;
        }

        m_pos = pos + size;  //consume payload
    }

    assert(m_pos <= stop);

    return 0;  //success
}


HRESULT Segment::ParseCluster(Cluster*& pCluster, __int64& pos_) const
{
    //This is a const member function, which means that it doesn't
    //modify the segment object.  Instead we determine where the next
    //cluster is (loading the file into network cache as a side effect),
    //create a pre-loaded cluster object, and then return that to the
    //caller, who immediately calls AddCluster to update the segment
    //object (which is where the actual modification occurs).  The
    //reason for this roundabout way of doing things is that a read
    //from a network cache can block indefinitely (or at least for
    //unpredictable amounts of time), and we don't want to block the
    //streaming threads, which are busying pushing frames downstream,
    //so they must have unhindered access to the file.  It's OK if
    //if the worker thread blocks (that's the thread that calls
    //Segment::ParseCluster), because it only works in background.
    //Furthermore, the caller should have bound IMkvFile to some
    //abstraction that allows timed reads, so we anticipate read
    //errors here, especially when the worker thread must be terminated
    //in order to implement IBaseFilter::Stop.  Note that it doesn't
    //matter whether any read errors occur here, because we haven't
    //modified any segment state.  We either successfully parse the
    //entire cluster (and modify the segment state accordingly, in
    //Segment:AddCluster), or the parse here fails (and segment
    //state is not modified at all, because the worker thread
    //terminates itself without calling Segment::AddCluster).

    pCluster = 0;  //0 means "no cluster found"
    pos_ = -1;     //>= 0 means "we parsed something"

    const __int64 stop = m_start + m_size;
    assert(m_pos <= stop);

    __int64 pos = m_pos;  //how much of the file has been consumed
    __int64 off = -1;     //offset of cluster relative to segment

    while (pos < stop)
    {
        long len;
        const __int64 idpos = pos;

        const __int64 id = SyncReadUInt(m_pFile, pos, stop, len);

        if (id < 0)  //error
            return static_cast<HRESULT>(id);

        if (id == 0)
            return VFW_E_INVALID_FILE_FORMAT;

        pos += len;  //consume id
        assert(pos < stop);

        const __int64 size = SyncReadUInt(m_pFile, pos, stop, len);

        if (size < 0)  //error
            return static_cast<HRESULT>(size);

        pos += len;  //consume size
        assert(pos <= stop);

        //pos now points to start of payload

        if (size == 0)  //weird
            continue;   //throw away this (empty) element

        pos += size;  //consume payload
        assert(pos <= stop);

        if (id == 0x0F43B675)  //Cluster ID
        {
            off = idpos - m_start;  //>= 0 means "we found a cluster"
            break;
        }
    }

    assert(pos <= stop);

    //Indicate to caller how much of file has been consumed. This is
    //used later in AddCluster to adjust the current parse position
    //(the value cached in the segment object itself) to the
    //file position value just past the cluster we parsed.

    if (off < 0)  //we did not found any more clusters
    {
        pos_ = stop;
        return S_FALSE;  //pos_ >= 0 here means EOF (cluster is NULL)
    }

    //We found a cluster.  Now read something, to ensure that it is
    //fully loaded in the network cache.

    if (pos >= stop)  //we parsed the entire segment
    {
        //We did find a cluster, but it was very last element in the segment.
        //Our preference is that the loop above runs 1 1/2 times:
        //the first pass finds the cluster, and the second pass
        //finds the element the follows the cluster.  In this case, however,
        //we reached the end of the file without finding another element,
        //so we didn't actually read anything yet associated with "end of the
        //cluster".  And we must perform an actual read, in order
        //to guarantee that all of the data that belongs to this
        //cluster has been loaded into the network cache.  So instead
        //of reading the next element that follows the cluster, we
        //read the last byte of the cluster (which is also the last
        //byte in the file).

        //Read the last byte of the file. (Reading 0 bytes at pos
        //might work too -- it would depend on how the reader is
        //implemented.  Here we take the more conservative approach,
        //since this makes fewer assumptions about the network
        //reader abstraction.)

        BYTE b;

        const HRESULT hr = m_pFile->MkvRead(pos - 1, 1, &b);

        if (FAILED(hr))
            return hr;

        if (hr != S_OK)
            return VFW_E_BUFFER_UNDERFLOW;

        pos_ = stop;
    }
    else
    {
        long len;
        const __int64 idpos = pos;

        const __int64 id = SyncReadUInt(m_pFile, pos, stop, len);

        if (id < 0)  //error
            return static_cast<HRESULT>(id);

        if (id == 0)
            return VFW_E_INVALID_FILE_FORMAT;

        pos += len;  //consume id
        assert(pos < stop);

        const __int64 size = SyncReadUInt(m_pFile, pos, stop, len);

        if (size < 0)  //error
            return static_cast<HRESULT>(size);

        pos_ = idpos;
    }

    //We found a cluster, and it has been completely loaded into the
    //network cache.  (We can guarantee this because we actually read
    //the EBML tag that follows the cluster, or, if we reached EOF,
    //because we actually read the last byte of the cluster).

    Segment* const this_ = const_cast<Segment*>(this);
    const Cluster::index_t idx = m_clusters.size();

    pCluster = Cluster::Parse(this_, idx, off);  //create new cluster object
    assert(pCluster);

    return S_OK;
}


bool Segment::AddCluster(Cluster* pCluster, __int64 pos)
{
    //AddCluster completes the parsing work done by the worker
    //thread in Segment::ParseCluster.  pCluster is either the
    //new cluster object created (if one was found), or NULL
    //(meaning we've reached EOF without finding a cluster).
    //pos indicates how much of the file was consumed during the
    //parse.  It will have a value just beyond the end of the
    //cluster, so that the next time Segment::ParserCluster is
    //called, we begin the parse starting from that position.

    assert(pos >= m_start);

    const __int64 stop = m_start + m_size;
    assert(pos <= stop);

    if (pCluster)
        m_clusters.push_back(pCluster);

    m_pos = pos;  //m_pos >= stop is how we know we have all clusters

    return (pos >= stop);
}


HRESULT Segment::Load()
{
    //Outermost (level 0) segment object has been constructed,
    //and pos designates start of payload.  We need to find the
    //inner (level 1) elements.

    const __int64 stop = m_start + m_size;

#ifdef _DEBUG
    {
        LONGLONG total, available;

        HRESULT hr = m_pFile->MkvLength(&total, &available);
        assert(SUCCEEDED(hr));
        assert(available >= total);
        assert(stop <= total);
    }
#endif

    while (m_pos < stop)
    {
        __int64 pos = m_pos;

        long len;

        __int64 result = GetUIntLength(m_pFile, pos, len);

        if (result < 0)  //error
            return static_cast<HRESULT>(result);

        if ((pos + len) > stop)
            return VFW_E_INVALID_FILE_FORMAT;

        const __int64 idpos = pos;
        const __int64 id = ReadUInt(m_pFile, idpos, len);

        if (id < 0)  //error
            return static_cast<HRESULT>(id);

        pos += len;  //consume ID

        //Read Size

        result = GetUIntLength(m_pFile, pos, len);

        if (result < 0)  //error
            return static_cast<HRESULT>(result);

        if ((pos + len) > stop)
            return VFW_E_INVALID_FILE_FORMAT;

        const __int64 size = ReadUInt(m_pFile, pos, len);

        if (size < 0)  //error
            return static_cast<HRESULT>(size);

        pos += len;  //consume length of size of element

        //Pos now points to start of payload

        if ((pos + size) > stop)
            return VFW_E_INVALID_FILE_FORMAT;

        if (id == 0x0F43B675)  //Cluster ID
            break;

        if (id == 0x014D9B74)  //SeekHead ID
        {
            ParseSeekHead(pos, size);
        }
        else if (id == 0x0549A966)  //Segment Info ID
        {
            assert(m_pInfo == 0);
            m_pInfo = new (std::nothrow) SegmentInfo(this, pos, size);
            assert(m_pInfo);  //TODO
        }
        else if (id == 0x0654AE6B)  //Tracks ID
        {
            assert(m_pTracks == 0);
            m_pTracks = new (std::nothrow) Tracks(this, pos, size);
            assert(m_pTracks);  //TODO
        }

        m_pos = pos + size;  //consume payload
    }

    if (!m_clusters.empty())
        m_pos = stop;  //means "loading done, because we have all clusters"

    return S_OK;
}


void Segment::ParseSeekHead(__int64 start, __int64 size_)
{
    __int64 pos = start;
    const __int64 stop = start + size_;

    while (pos < stop)
    {
        long len;

        const __int64 id = ReadUInt(m_pFile, pos, len);
        assert(id >= 0);  //TODO
        assert((pos + len) <= stop);

        pos += len;  //consume ID

        const __int64 size = ReadUInt(m_pFile, pos, len);
        assert(size >= 0);
        assert((pos + len) <= stop);

        pos += len;  //consume Size field
        assert((pos + size) <= stop);

        if (id == 0x0DBB)  //SeekEntry ID
            ParseSeekEntry(pos, size);

        pos += size;  //consume payload
        assert(pos <= stop);
    }

    assert(pos == stop);
}


void Segment::ParseSecondarySeekHead(__int64 off)
{
    assert(off >= 0);
    assert(off < m_size);

    __int64 pos = m_start + off;
    const __int64 stop = m_start + m_size;
    stop;

    long len;

    __int64 result = GetUIntLength(m_pFile, pos, len);
    assert(result == 0);
    assert((pos + len) <= stop);

    const __int64 idpos = pos;

    const __int64 id = ReadUInt(m_pFile, idpos, len);
    id;
    assert(id == 0x014D9B74);  //SeekHead ID

    pos += len;  //consume ID
    assert(pos < stop);

    //Read Size

    result = GetUIntLength(m_pFile, pos, len);
    assert(result == 0);
    assert((pos + len) <= stop);

    const __int64 size = ReadUInt(m_pFile, pos, len);
    assert(size >= 0);

    pos += len;  //consume length of size of element
    assert((pos + size) <= stop);

    //Pos now points to start of payload

    ParseSeekHead(pos, size);
}


void Segment::ParseSeekEntry(__int64 start, __int64 size_)
{
    __int64 pos = start;

    const __int64 stop = start + size_;
    stop;

    long len;

    const __int64 seekIdId = ReadUInt(m_pFile, pos, len);
    seekIdId;
    assert(seekIdId == 0x13AB);  //SeekID ID
    assert((pos + len) <= stop);

    pos += len;  //consume id

    const __int64 seekIdSize = ReadUInt(m_pFile, pos, len);
    assert(seekIdSize >= 0);
    assert((pos + len) <= stop);

    pos += len;  //consume size

    const __int64 seekId = ReadUInt(m_pFile, pos, len);  //payload
    assert(seekId >= 0);
    assert(len == seekIdSize);
    assert((pos + len) <= stop);

    pos += seekIdSize;  //consume payload

    const __int64 seekPosId = ReadUInt(m_pFile, pos, len);
    seekPosId;
    assert(seekPosId == 0x13AC);  //SeekPos ID
    assert((pos + len) <= stop);

    pos += len;  //consume id

    const __int64 seekPosSize = ReadUInt(m_pFile, pos, len);
    assert(seekPosSize >= 0);
    assert((pos + len) <= stop);

    pos += len;  //consume size
    assert((pos + seekPosSize) <= stop);

    const __int64 seekOff = UnserializeUInt(m_pFile, pos, seekPosSize);
    assert(seekOff >= 0);
    assert(seekOff < m_size);

    pos += seekPosSize;  //consume payload
    assert(pos == stop);

    const __int64 seekPos = m_start + seekOff;
    seekPos;
    assert(seekPos < (m_start + m_size));

    //odbgstream os;
    //os << "ParseSeekEntry: id=0x" << hex << seekId << endl;

    if (seekId == 0x0F43B675)  //cluster id
    {
#if 0
        Cluster::Preload(this, m_clusters, seekOff);
#else
        const Cluster::index_t idx = m_clusters.size();

        Cluster* const pCluster = Cluster::Parse(this, idx, seekOff);
        assert(pCluster);  //TODO

        m_clusters.push_back(pCluster);
#endif
    }
    else if (seekId == 0x014D9B74)  //SeekHead ID
    {
        ParseSecondarySeekHead(seekOff);
    }
    else if (seekId == 0x0C53BB6B)  //Cues ID
    {
        ParseCues(seekOff);
    }
}


void Segment::ParseCues(__int64 off)
{
    assert(off >= 0);
    assert(off < m_size);

    __int64 pos = m_start + off;
    const __int64 stop = m_start + m_size;
    stop;

    long len;

    __int64 result = GetUIntLength(m_pFile, pos, len);
    assert(result == 0);
    assert((pos + len) <= stop);

    const __int64 idpos = pos;

    const __int64 id = ReadUInt(m_pFile, idpos, len);
    id;
    assert(id == 0x0C53BB6B);  //Cues ID

    pos += len;  //consume ID
    assert(pos < stop);

    //Read Size

    result = GetUIntLength(m_pFile, pos, len);
    assert(result == 0);
    assert((pos + len) <= stop);

    const __int64 size = ReadUInt(m_pFile, pos, len);
    assert(size >= 0);

    pos += len;  //consume length of size of element
    assert((pos + size) <= stop);

    //Pos now points to start of payload

    assert(m_pCues == 0);
    m_pCues = new (std::nothrow) Cues(this, pos, size);
    assert(m_pCues);  //TODO
}


Cues::Cues(Segment* pSegment, __int64 start_, __int64 size_) :
    m_pSegment(pSegment),
    m_start(start_),
    m_size(size_)
{
    IMkvFile* const pFile = m_pSegment->m_pFile;

    const __int64 stop = m_start + m_size;
    __int64 pos = m_start;

    while (pos < stop)
    {
        long len;

        const __int64 id = ReadUInt(pFile, pos, len);
        assert(id >= 0);  //TODO
        assert((pos + len) <= stop);

        pos += len;  //consume ID

        const __int64 size = ReadUInt(pFile, pos, len);
        assert(size >= 0);
        assert((pos + len) <= stop);

        pos += len;  //consume Size field
        assert((pos + size) <= stop);

        if (id == 0x3B)  //CuePoint ID
        {
            CuePoint p;
            p.Parse(pFile, pos, size);

#ifdef _DEBUG
            if (!m_cue_points.empty())
            {
                const CuePoint& b = m_cue_points.back();
                assert(p.m_timecode > b.m_timecode);
            }
#endif

            m_cue_points.push_back(p);
        }

        pos += size;  //consume payload
        assert(pos <= stop);
    }

    assert(pos == stop);
}


bool Cues::Find(
    __int64 time_ns,
    const Track* pTrack,
    const CuePoint*& pCP,
    const CuePoint::TrackPosition*& pTP) const
{
    assert(time_ns >= 0);
    assert(pTrack);

    if (m_cue_points.empty())
        return false;

    typedef cue_points_t::const_iterator iter_t;

    const iter_t i = m_cue_points.begin();

    pCP = &*i;

    if (time_ns <= pCP->GetTime(m_pSegment))
    {
        pTP = pCP->Find(pTrack);
        return (pTP != 0);
    }

    const iter_t j = m_cue_points.end();

    const CuePoint::CompareTime pred(m_pSegment);

    const iter_t k = std::upper_bound(i, j, time_ns, pred);
    assert(k != i);

    pCP = &*--iter_t(k);
    assert(pCP->GetTime(m_pSegment) <= time_ns);

    pTP = pCP->Find(pTrack);
    return (pTP != 0);
}


bool Cues::FindNext(
    __int64 time_ns,
    const Track* pTrack,
    const CuePoint*& pCP,
    const CuePoint::TrackPosition*& pTP) const
{
    pCP = 0;
    pTP = 0;

    if (m_cue_points.empty())  //weird
        return false;

    typedef cue_points_t::const_iterator iter_t;

    const iter_t i = m_cue_points.begin();
    const iter_t j = m_cue_points.end();

    const CuePoint::CompareTime pred(m_pSegment);

    const iter_t k = std::upper_bound(i, j, time_ns, pred);

    if (k == j)  //time_ns is greater than max cue point
        return false;

    pCP = &*k;
    assert(pCP->GetTime(m_pSegment) > time_ns);

    pTP = pCP->Find(pTrack);

    return (pTP != 0);
}


void CuePoint::Parse(IMkvFile* pFile, __int64 start_, __int64 size_)
{
    const __int64 stop = start_ + size_;
    __int64 pos = start_;

    m_timecode = -1;

    while (pos < stop)
    {
        long len;

        const __int64 id = ReadUInt(pFile, pos, len);
        assert(id >= 0);  //TODO
        assert((pos + len) <= stop);

        pos += len;  //consume ID

        const __int64 size = ReadUInt(pFile, pos, len);
        assert(size >= 0);
        assert((pos + len) <= stop);

        pos += len;  //consume Size field
        assert((pos + size) <= stop);

        if (id == 0x33)  //CueTime ID
            m_timecode = UnserializeUInt(pFile, pos, size);

        else if (id == 0x37) //CueTrackPosition(s) ID
            ParseTrackPosition(pFile, pos, size);

        pos += size;  //consume payload
        assert(pos <= stop);
    }

    assert(m_timecode >= 0);
    assert(!m_track_positions.empty());
}


void CuePoint::ParseTrackPosition(
    IMkvFile* pFile,
    __int64 start_,
    __int64 size_)
{
    const __int64 stop = start_ + size_;
    __int64 pos = start_;

    TrackPosition p;

    p.m_track = -1;
    p.m_pos = -1;
    p.m_block = -1;

    while (pos < stop)
    {
        long len;

        const __int64 id = ReadUInt(pFile, pos, len);
        assert(id >= 0);  //TODO
        assert((pos + len) <= stop);

        pos += len;  //consume ID

        const __int64 size = ReadUInt(pFile, pos, len);
        assert(size >= 0);
        assert((pos + len) <= stop);

        pos += len;  //consume Size field
        assert((pos + size) <= stop);

        if (id == 0x77)  //CueTrack ID
            p.m_track = UnserializeUInt(pFile, pos, size);

        else if (id == 0x71)  //CueClusterPos ID
            p.m_pos = UnserializeUInt(pFile, pos, size);

        else if (id == 0x1378)  //CueBlockNumber
            p.m_block = UnserializeUInt(pFile, pos, size);

        pos += size;  //consume payload
        assert(pos <= stop);
    }

    assert(p.m_track > 0);
    assert(p.m_pos >= 0);
    assert(p.m_block != 0);

    m_track_positions.push_back(p);
}


const CuePoint::TrackPosition* CuePoint::Find(const Track* pTrack) const
{
    assert(pTrack);

    const ULONG n = pTrack->GetNumber();

    typedef track_positions_t::const_iterator iter_t;

    iter_t i = m_track_positions.begin();
    const iter_t j = m_track_positions.end();

    while (i != j)
    {
        const TrackPosition& p = *i++;

        if (p.m_track == n)
            return &p;
    }

    return 0;  //no matching track number found
}


__int64 CuePoint::GetTime(Segment* pSegment) const
{
    assert(pSegment);

    const SegmentInfo* const pInfo = pSegment->GetInfo();
    assert(pInfo);

    const __int64 scale = pInfo->GetTimeCodeScale();
    assert(scale >= 1);

    const __int64 time = scale * m_timecode;

    return time;
}


__int64 Segment::Unparsed() const
{
    const __int64 stop = m_start + m_size;

    const __int64 result = stop - m_pos;
    assert(result >= 0);

    return result;
}


#if 0  //NOTE: too inefficient
__int64 Segment::Load(__int64 time_ns)
{
    if (Unparsed() <= 0)
        return 0;

    while (m_clusters.empty())
    {
        const __int64 result = Parse();

        if (result)  //error, or not enough bytes available
            return result;

        if (Unparsed() <= 0)
            return 0;
    }

    while (m_clusters.back()->GetTime() < time_ns)
    {
        const __int64 result = Parse();

        if (result)  //error, or not enough bytes available
            return result;

        if (Unparsed() <= 0)
            return 0;
    }

    return 0;
}
#endif


Cluster* Segment::GetFirst()
{
    const Cluster::clusters_t& cc = m_clusters;

    if (cc.empty())
        return &m_eos;

    Cluster* const pCluster = cc.front();
    assert(pCluster);

    return pCluster;
}


Cluster* Segment::GetLast()
{
    const Cluster::clusters_t& cc = m_clusters;

    if (cc.empty())
        return &m_eos;

    Cluster* const pCluster = cc.back();
    assert(pCluster);

    return pCluster;
}


ULONG Segment::GetCount() const
{
    const Cluster::clusters_t::size_type result = m_clusters.size();
    return static_cast<ULONG>(result);
}


Cluster* Segment::GetNext(const Cluster* pCurr)
{
    assert(pCurr);

    Cluster::clusters_t& cc = m_clusters;
    assert(!cc.empty());

    Cluster::index_t idx = pCurr->m_index;
    assert(idx < cc.size());
    assert(cc[idx] == pCurr);

    ++idx;

    if (idx >= cc.size())
        return &m_eos;

    Cluster* const pNext = cc[idx];
    assert(pNext);

    return pNext;
}


Cluster* Segment::GetPrevious(const Cluster* pCurr)
{
    assert(pCurr);

    Cluster::clusters_t& cc = m_clusters;
    assert(!cc.empty());

    Cluster::index_t idx = pCurr->m_index;
    assert(idx < cc.size());
    assert(cc[idx] == pCurr);

    if (idx == 0)
        return 0;  //no previous cluster

    Cluster* const pPrev = cc[--idx];
    assert(pPrev);
    assert(pPrev->m_index == idx);

    return pPrev;
}


Cluster* Segment::GetCluster(__int64 time_ns)
{
    if (m_clusters.empty())
        return &m_eos;

    typedef Cluster::clusters_t::const_iterator iter_t;

    const iter_t i = m_clusters.begin();

    {
        Cluster* const pCluster = *i;
        assert(pCluster);

        if (time_ns <= pCluster->GetTime())
            return pCluster;
    }

    const iter_t j = m_clusters.end();

    const iter_t k = std::upper_bound(i, j, time_ns, Cluster::CompareTime());
    assert(k != i);

    Cluster* const pCluster = *--iter_t(k);
    assert(pCluster);
    assert(pCluster->GetTime() <= time_ns);

    return pCluster;
}


void Segment::GetCluster(
    __int64 time_ns,
    Track* pTrack,
    Cluster*& pCluster,
    const BlockEntry*& pBlockEntry)
{
    assert(pTrack);

    if (m_clusters.empty())
    {
        pCluster = &m_eos;
        pBlockEntry = pTrack->GetEOS();

        return;
    }

    typedef Cluster::clusters_t::const_iterator iter_t;

    const iter_t i = m_clusters.begin();

    {
        pCluster = *i;
        assert(pCluster);

        if (time_ns <= pCluster->GetTime())
        {
            pBlockEntry = pCluster->GetEntry(pTrack);
            return;
        }
    }

    const iter_t j = m_clusters.end();

    if (pTrack->GetType() == 2)  //audio
    {
        //TODO: we could decide to use cues for this, as we do for video.
        //But we only use it for video because looking around for a keyframe
        //can get expensive.  Audio doesn't require anything special we a
        //straight cluster search is good enough (we assume).

        iter_t k = std::upper_bound(i, j, time_ns, Cluster::CompareTime());
        assert(k != i);

        pCluster = *--k;
        assert(pCluster);
        assert(pCluster->GetTime() <= time_ns);

        pBlockEntry = pCluster->GetEntry(pTrack);
        return;
    }

    assert(pTrack->GetType() == 1);  //video

    if (SearchCues(time_ns, pTrack, pCluster, pBlockEntry))
        return;

    iter_t k = std::upper_bound(i, j, time_ns, Cluster::CompareTime());
    assert(k != i);

    pCluster = *--k;
    assert(pCluster);
    assert(pCluster->GetTime() <= time_ns);

    {
        pBlockEntry = pCluster->GetEntry(pTrack);
        assert(pBlockEntry);

        if (!pBlockEntry->EOS())  //found a keyframe
        {
            const Block* const pBlock = pBlockEntry->GetBlock();
            assert(pBlock);

            //TODO: this isn't necessarily the keyframe we want,
            //since there might another keyframe on this same
            //cluster with a greater timecode that but that is
            //still less than the requested time.  For now we
            //simply return the first keyframe we find.

            if (pBlock->GetTime(pCluster) <= time_ns)
                return;
        }
    }

    const VideoTrack* const pVideo = static_cast<VideoTrack*>(pTrack);

    while (k != i)
    {
#if 0
        const LONGLONG dt = time_ns - pCluster->GetTime();
        assert(dt >= 0);

        if (dt >= 33000000000)  //33 sec
            k = i;  //don't bother searching anymore
        else
            --k;    //try previous cluster

        pCluster = *k;
        assert(pCluster);
        assert(pCluster->GetTime() <= time_ns);

        pBlockEntry = pCluster->GetEntry(pTrack);
        assert(pBlockEntry);

        if (!pBlockEntry->EOS())
            return;
#else
        pCluster = *--k;
        assert(pCluster);
        assert(pCluster->GetTime() <= time_ns);

        pBlockEntry = pCluster->GetMaxKey(pVideo);
        assert(pBlockEntry);

        if (!pBlockEntry->EOS())
            return;
#endif
    }

    //weird: we're on the first cluster, but no keyframe found
    //should never happen but we must return something anyway

    pCluster = &m_eos;
    pBlockEntry = pTrack->GetEOS();
}


bool Segment::SearchCues(
    __int64 time_ns_,
    Track* pTrack,
    Cluster*& pCluster,
    const BlockEntry*& pBlockEntry)
{
    if (m_pCues == 0)
        return false;

    if (m_clusters.empty())
        return false;

    //TODO:
    //search among cuepoints for time
    //if time is less then what's already loaded then
    //  return that cluster and we're done
    //else (time is greater than what's loaded)
    //  if time isn't "too far into the future" then
    //     pre-load the necessary clusters
    //     return that cluster and we're done
    //  else
    //     find (earlier) cue point corresponding to something
    //       already loaded in cache, and return that

    Cluster* const pLastCluster = m_clusters.back();
    assert(pLastCluster);
    assert(pLastCluster->m_pos);

    const __int64 last_pos = _abs64(pLastCluster->m_pos);
    last_pos;
    
    const __int64 last_ns = pLastCluster->GetTime();

    __int64 time_ns;

    if (Unparsed() <= 0)  //all clusters loaded
        time_ns = time_ns_;

    else if (time_ns_ < last_ns)
        time_ns = time_ns_;

    else
        time_ns = last_ns;

    const CuePoint* pCP;
    const CuePoint::TrackPosition* pTP;

    if (!m_pCues->Find(time_ns, pTrack, pCP, pTP))
        return false;  //weird

    assert(pCP);
    assert(pTP);
    assert(pTP->m_track == pTrack->GetNumber());
    assert(pTP->m_pos <= last_pos);

    typedef Cluster::clusters_t::const_iterator iter_t;

    const iter_t i = m_clusters.begin();
    assert(pTP->m_pos >= _abs64((*i)->m_pos));

    const iter_t j = m_clusters.end();

    const Cluster::ComparePos pred;

    const iter_t k = std::upper_bound(i, j, pTP->m_pos, pred);
    assert(k != i);

    pCluster = *--iter_t(k);
    assert(pCluster);
    assert(pCluster->m_pos);
    assert(_abs64(pCluster->m_pos) == pTP->m_pos);

    pBlockEntry = pCluster->GetEntry(*pCP, *pTP);
    assert(pBlockEntry);

    return true;
}


const Tracks* Segment::GetTracks() const
{
    return m_pTracks;
}


const SegmentInfo* Segment::GetInfo() const
{
    return m_pInfo;
}


const Cues* Segment::GetCues() const
{
    return m_pCues;
}


__int64 Segment::GetDuration() const
{
    assert(m_pInfo);
    return m_pInfo->GetDuration();
}


SegmentInfo::SegmentInfo(Segment* pSegment, __int64 start, __int64 size_) :
    m_pSegment(pSegment),
    m_start(start),
    m_size(size_)
{
    IMkvFile* const pFile = m_pSegment->m_pFile;

    __int64 pos = start;
    const __int64 stop = start + size_;

    m_timecodeScale = 1000000;
    m_duration = 0;

    while (pos < stop)
    {
        if (Match(pFile, pos, 0x0AD7B1, m_timecodeScale))
            assert(m_timecodeScale > 0);

        else if (Match(pFile, pos, 0x0489, m_duration))
            assert(m_duration >= 0);

        else if (Match(pFile, pos, 0x0D80, m_muxingApp))
            m_muxingApp;

        else if (Match(pFile, pos, 0x1741, m_writingApp))
            m_writingApp;

        else
        {
            long len;

            const __int64 id = ReadUInt(pFile, pos, len);
            id;
            assert(id >= 0);
            assert((pos + len) <= stop);

            pos += len;  //consume id
            assert((stop - pos) > 0);

            const __int64 size = ReadUInt(pFile, pos, len);
            assert(size >= 0);
            assert((pos + len) <= stop);

            pos += len + size;  //consume size and payload
            assert(pos <= stop);
        }
    }

    assert(pos == stop);
}


__int64 SegmentInfo::GetTimeCodeScale() const
{
    return m_timecodeScale;
}


__int64 SegmentInfo::GetDuration() const
{
    assert(m_duration >= 0);
    assert(m_timecodeScale >= 1);

    const double dd = double(m_duration) * double(m_timecodeScale);
    const __int64 d = static_cast<__int64>(dd);

    return d;
}


const wchar_t* SegmentInfo::GetMuxingApp() const
{
    if (m_muxingApp.empty())
        return 0;

    return m_muxingApp.c_str();
}


const wchar_t* SegmentInfo::GetWritingApp() const
{
    if (m_writingApp.empty())
        return 0;

    return m_writingApp.c_str();
}


Track::Track(Segment* pSegment, const Info& i) :
    m_pSegment(pSegment),
    m_info(i)
{
}


Track::~Track()
{
}


const BlockEntry* Track::GetEOS() const
{
    return &m_eos;
}


BYTE Track::GetType() const
{
    assert(m_info.type >= 1);
    assert(m_info.type <= 2);  //TODO

    const BYTE result = static_cast<BYTE>(m_info.type);
    return result;
}


ULONG Track::GetNumber() const
{
    assert(m_info.number >= 0);
    const ULONG result = static_cast<ULONG>(m_info.number);
    return result;
}


const wchar_t* Track::GetName() const
{
    if (m_info.name.empty())
        return 0;

    return m_info.name.c_str();
}


const wchar_t* Track::GetCodecName() const
{
    if (m_info.codecName.empty())
        return 0;

    return m_info.codecName.c_str();
}


const char* Track::GetCodecId() const
{
    if (m_info.codecId.empty())
        return 0;

    return m_info.codecId.c_str();
}


const bytes_t& Track::GetCodecPrivate() const
{
    return m_info.codecPrivate;
}


HRESULT Track::GetFirst(const BlockEntry*& pBlockEntry) const
{
    Cluster* pCluster = m_pSegment->GetFirst();

    for (int i = 0; i < 100; ++i)  //arbitrary upper bound to search
    {
        if ((pCluster == 0) || pCluster->EOS())
        {
            if (m_pSegment->Unparsed() <= 0)   //all clusters have been loaded
            {
                pBlockEntry = GetEOS();
                return S_FALSE;
            }

            pBlockEntry = 0;
            return VFW_E_BUFFER_UNDERFLOW;
        }

        pBlockEntry = pCluster->GetFirst();

        while (pBlockEntry)
        {
            const Block* const pBlock = pBlockEntry->GetBlock();
            assert(pBlock);

            if (pBlock->GetNumber() == m_info.number)
                return S_OK;

            pBlockEntry = pCluster->GetNext(pBlockEntry);
        }

        pCluster = m_pSegment->GetNext(pCluster);
    }

    //NOTE: if we get here, it means that we didn't find a block with
    //a matching track number after lots of searching, so we give
    //up trying.

    pBlockEntry = GetEOS();  //so we can return a non-NULL value
    return S_FALSE;
}


HRESULT Track::GetNextBlock(
    const BlockEntry* pCurrEntry,
    const BlockEntry*& pNextEntry) const
{
    assert(pCurrEntry);
    assert(!pCurrEntry->EOS());  //?
    assert(pCurrEntry->GetBlock()->GetNumber() == m_info.number);

#if 0
    const Cluster* const pCurrCluster = pCurrEntry->GetCluster();
    assert(pCurrCluster);
    assert(!pCurrCluster->EOS());

    pNextEntry = pCurrCluster->GetNext(pCurrEntry);

    while (pNextEntry)
    {
        const Block* const pNextBlock = pNextEntry->GetBlock();
        assert(pNextBlock);

        if (pNextBlock->GetNumber() == m_info.number)
            return S_OK;

        pNextEntry = pCurrCluster->GetNext(pNextEntry);
    }

    Segment* pSegment = pCurrCluster->m_pSegment;
    Cluster* const pNextCluster = pSegment->GetNext(pCurrCluster);

    if ((pNextCluster == 0) || pNextCluster->EOS())
    {
        if (pSegment->Unparsed() <= 0)   //all clusters have been loaded
        {
            pNextEntry = GetEOS();
            return S_FALSE;
        }

        pNextEntry = 0;
        return VFW_E_BUFFER_UNDERFLOW;
    }

    pNextEntry = pNextCluster->GetFirst();

    while (pNextEntry)
    {
        const Block* const pNextBlock = pNextEntry->GetBlock();
        assert(pNextBlock);

        if (pNextBlock->GetNumber() == m_info.number)
            return S_OK;

        pNextEntry = pNextCluster->GetNext(pNextEntry);
    }

    //TODO: what has happened here is that we did not find a block
    //with a matching track number on the next cluster.  It might
    //be the case that some cluster beyond the next cluster
    //contains a block having a matching track number, but for
    //now we terminate the search immediately.  We do this so that
    //we don't end up searching the entire file looking for the
    //next block.  Another possibility is to try searching for the next
    //block in a small, fixed number of clusters (intead searching
    //just the next one), or to terminate the search when when the
    //there is a large gap in time, or large gap in file position.  It
    //might very well be the case that the approach we use here is
    //unnecessarily conservative.

    //TODO: again, here's a case where we need to return the special
    //EOS block.  Or something.  It's OK if pNext is NULL, because
    //we only need it to set the stop time of the media sample.
    //(The start time is determined from pCurr, which is non-NULL
    //and non-EOS.)  The problem is when we set pCurr=pNext; when
    //pCurr has the value NULL we interpret that to mean that we
    //haven't fully initialized pCurr and we attempt to set it to
    //point to the first block for this track.  But that's not what
    //we want at all; we want the next call to PopulateSample to
    //return end-of-stream, not (re)start from the beginning.
    //
    //One work-around is to send EOS immediately.  We would send
    //the EOS the next pass anyway, so maybe it's no great loss.  The
    //only problem is that if this the stream really does end one
    //cluster early (relative to other tracks), or the last frame
    //happens to be a keyframe ("CanSeekToEnd").
    //
    //The problem is that we need a way to mark as stream as
    //"at end of stream" without actually being at end of stream.
    //We need to give pCurr some value that means "you've reached EOS".
    //We can't synthesize the special EOS Cluster immediately
    //(when we first open the file, say), because we use the existance
    //of that special cluster value to mean that we've read all of
    //the clusters (this is a network download, so we can't know apriori
    //how many we have).
    //
    //Or, we could return E_FAIL, and set another bit in the stream
    //object itself, to indicate that it should send EOS earlier
    //than when (pCurr=pStop).
    //
    //Or, probably the best solution, when we actually load the
    //blocks into a cluster: if we notice that there's no block
    //for a track, we synthesize a nonce EOS block for that track.
    //That way we always have something to return.  But that will
    //only work for sequential scan???

    //pNext = 0;
    //return E_FAIL;
    pNextEntry = GetEOS();
    return S_FALSE;
#else
    Cluster* pCluster = pCurrEntry->GetCluster();
    assert(pCluster);
    assert(!pCluster->EOS());

    pNextEntry = pCluster->GetNext(pCurrEntry);

    for (int i = 0; i < 100; ++i)  //arbitrary upper bound to search
    {
        while (pNextEntry)
        {
            const Block* const pNextBlock = pNextEntry->GetBlock();
            assert(pNextBlock);

            if (pNextBlock->GetNumber() == m_info.number)
                return S_OK;

            pNextEntry = pCluster->GetNext(pNextEntry);
        }

        pCluster = m_pSegment->GetNext(pCluster);

        if ((pCluster == 0) || pCluster->EOS())
        {
            if (m_pSegment->Unparsed() <= 0)   //all clusters have been loaded
            {
                pNextEntry = GetEOS();
                return S_FALSE;
            }

            pNextEntry = 0;
            return VFW_E_BUFFER_UNDERFLOW;
        }

        pNextEntry = pCluster->GetFirst();
    }

    //NOTE: if we get here, it means that we didn't find a block with
    //a matching track number after lots of searching, so we give
    //up trying.

    pNextEntry = GetEOS();  //so we can return a non-NULL value
    return S_FALSE;
#endif
}


Track::EOSBlock::EOSBlock()
{
}


bool Track::EOSBlock::EOS() const
{
    return true;
}


Cluster* Track::EOSBlock::GetCluster() const
{
    return 0;
}


BlockEntry::index_t Track::EOSBlock::GetIndex() const
{
    return 0;
}


const Block* Track::EOSBlock::GetBlock() const
{
    return 0;
}


bool Track::EOSBlock::IsBFrame() const
{
    return false;
}


VideoTrack::VideoTrack(Segment* pSegment, const Info& i) :
    Track(pSegment, i),
    m_width(-1),
    m_height(-1),
    m_rate(-1)
{
    assert(i.type == 1);
    assert(i.number > 0);

    IMkvFile* const pFile = pSegment->m_pFile;

    const Settings& s = i.settings;
    assert(s.start >= 0);
    assert(s.size >= 0);

    __int64 pos = s.start;
    assert(pos >= 0);

    const __int64 stop = pos + s.size;

    while (pos < stop)
    {
#ifdef _DEBUG
        long len;
        const __int64 id = ReadUInt(pFile, pos, len);
        assert(id >= 0);  //TODO: handle error case
        assert((pos + len) <= stop);
#endif
        if (Match(pFile, pos, 0x30, m_width))
            __noop;

        else if (Match(pFile, pos, 0x3A, m_height))
            __noop;

        else if (Match(pFile, pos, 0x0383E3, m_rate))
            __noop;

        else
        {
            long len;
            const __int64 id = ReadUInt(pFile, pos, len);
            id;
            assert(id >= 0);  //TODO: handle error case
            assert((pos + len) <= stop);

            pos += len;  //consume id

            const __int64 size = ReadUInt(pFile, pos, len);
            assert(size >= 0);  //TODO: handle error case
            assert((pos + len) <= stop);

            pos += len;  //consume length of size
            assert((pos + size) <= stop);

            //pos now designates start of payload

            pos += size;  //consume payload
            assert(pos <= stop);
        }
    }

    return;
}


HRESULT VideoTrack::GetNextTime(
    const BlockEntry* pCurr,
    const BlockEntry* pNext,
    const BlockEntry*& pTime) const
{
    assert(pCurr);
    assert(!pCurr->EOS());
    assert(pNext);
    assert(pCurr != pNext);

    for (;;)
    {
        if (pNext->EOS() || !pNext->IsBFrame())
        {
            pTime = pNext;
            return S_OK;
        }

        pCurr = pNext;

        const HRESULT hr = GetNextBlock(pCurr, pNext);

        if (FAILED(hr))
            return hr;  //underflow

        assert(pNext);
    }
}


bool VideoTrack::VetEntry(const BlockEntry* pBlockEntry) const
{
    pBlockEntry;
    assert(pBlockEntry);

    const Block* const pBlock = pBlockEntry->GetBlock();
    assert(pBlock);
    assert(pBlock->GetNumber() == m_info.number);

    return pBlock->IsKey();
}



__int64 VideoTrack::GetWidth() const
{
    return m_width;
}


__int64 VideoTrack::GetHeight() const
{
    return m_height;
}


double VideoTrack::GetFrameRate() const
{
    return m_rate;
}


AudioTrack::AudioTrack(Segment* pSegment, const Info& i) :
    Track(pSegment, i),
    m_rate(8000),
    m_channels(1),
    m_bit_depth(-1)
{
    assert(i.type == 2);
    assert(i.number > 0);

    IMkvFile* const pFile = pSegment->m_pFile;

    const Settings& s = i.settings;
    assert(s.start >= 0);
    assert(s.size >= 0);

    __int64 pos = s.start;
    assert(pos >= 0);

    const __int64 stop = pos + s.size;

    while (pos < stop)
    {
#ifdef _DEBUG
        long len;
        const __int64 id = ReadUInt(pFile, pos, len);
        assert(id >= 0);  //TODO: handle error case
        assert((pos + len) <= stop);
#endif
        if (Match(pFile, pos, 0x35, m_rate))
            assert(m_rate > 0);

        else if (Match(pFile, pos, 0x1F, m_channels))
            assert(m_channels > 0);

        else if (Match(pFile, pos, 0x2264, m_bit_depth))
            assert(m_bit_depth > 0);

        else
        {
            long len;
            const __int64 id = ReadUInt(pFile, pos, len);
            id;
            assert(id >= 0);  //TODO: handle error case
            assert((pos + len) <= stop);

            pos += len;  //consume id

            const __int64 size = ReadUInt(pFile, pos, len);
            assert(size >= 0);  //TODO: handle error case
            assert((pos + len) <= stop);

            pos += len;  //consume length of size
            assert((pos + size) <= stop);

            //pos now designates start of payload

            pos += size;  //consume payload
            assert(pos <= stop);
        }
    }

    return;
}


HRESULT AudioTrack::GetNextTime(
    const BlockEntry* pCurr,
    const BlockEntry* pNextBlock,
    const BlockEntry*& pNextTime) const
{
    pCurr;
    assert(pCurr);
    assert(!pCurr->EOS());
    assert(pNextBlock);
    assert(pCurr != pNextBlock);

    pNextTime = pNextBlock;
    return S_OK;
}


bool AudioTrack::VetEntry(const BlockEntry* pBlockEntry) const
{
    pBlockEntry;
    assert(pBlockEntry);

    const Block* const pBlock = pBlockEntry->GetBlock();
    pBlock;
    assert(pBlock);
    assert(pBlock->GetNumber() == m_info.number);

    return true;
}


double AudioTrack::GetSamplingRate() const
{
    return m_rate;
}


__int64 AudioTrack::GetChannels() const
{
    return m_channels;
}


__int64 AudioTrack::GetBitDepth() const
{
    return m_bit_depth;
}


Tracks::Tracks(Segment* pSegment, __int64 start, __int64 size_) :
    m_pSegment(pSegment),
    m_start(start),
    m_size(size_)
{
    const __int64 stop = m_start + m_size;
    IMkvFile* const pFile = m_pSegment->m_pFile;

    __int64 pos = m_start;

    while (pos < stop)
    {
        long len;
        const __int64 id = ReadUInt(pFile, pos, len);
        assert(id >= 0);
        assert((pos + len) <= stop);

        pos += len;  //consume id

        const __int64 size = ReadUInt(pFile, pos, len);
        assert(size >= 0);
        assert((pos + len) <= stop);

        pos += len;  //consume length of size

        //pos now desinates start of element

        if (id == 0x2E)  //TrackEntry ID
            ParseTrackEntry(pos, size);

        pos += size;  //consume payload
        assert(pos <= stop);
    }
}


void Tracks::ParseTrackEntry(__int64 start, __int64 size)
{
    IMkvFile* const pFile = m_pSegment->m_pFile;

    __int64 pos = start;
    const __int64 stop = start + size;

#if 1
    Track::Info i;

    //TODO: use ctor for this
    i.number = -1;
    i.uid = -1;
    i.type = -1;

    Track::Settings videoSettings;
    videoSettings.start = -1;

    Track::Settings audioSettings;
    audioSettings.start = -1;
#else
    __int64 tn = -1;
    __int64 uid = -1;
    __int64 type = -1;
    std::wstring name;  //UTF8 converted to wchar[]
    std::string codecId;
    bytes_t codecPrivate;
    std::wstring codecName;
    bytes_t videoSettings;
    bytes_t audioSettings;
#endif

    while (pos < stop)
    {
#ifdef _DEBUG
        long len;
        const __int64 id = ReadUInt(pFile, pos, len);
        len;
        id;
#endif
        if (Match(pFile, pos, 0x57, i.number))
            assert(i.number > 0);
        else if (Match(pFile, pos, 0x33C5, i.uid))
            __noop;
        else if (Match(pFile, pos, 0x03, i.type))
            __noop;
        else if (Match(pFile, pos, 0x136E, i.name))
            __noop;
        else if (Match(pFile, pos, 0x06, i.codecId))
            __noop;
        else if (Match(pFile, pos, 0x23A2, i.codecPrivate))
            __noop;
        else if (Match(pFile, pos, 0x058688, i.codecName))
            __noop;
#if 0
        else if (Match(pFile, pos, 0x60, videoSettings))
            __noop;
        else if (Match(pFile, pos, 0x61, audioSettings))
            __noop;
#endif
        else
        {
            long len;

            const __int64 id = ReadUInt(pFile, pos, len);
            assert(id >= 0);  //TODO: handle error case
            assert((pos + len) <= stop);

            pos += len;  //consume id

            const __int64 size = ReadUInt(pFile, pos, len);
            assert(size >= 0);  //TODO: handle error case
            assert((pos + len) <= stop);

            pos += len;  //consume length of size
            const __int64 start = pos;

            pos += size;  //consume payload
            assert(pos <= stop);

            if (id == 0x60)
            {
                videoSettings.start = start;
                videoSettings.size = size;
            }
            else if (id == 0x61)
            {
                audioSettings.start = start;
                audioSettings.size = size;
            }
        }
    }

    assert(pos == stop);

    //TODO: propertly vet info.number, to ensure both its existence,
    //and that it is unique among all tracks.
    assert(i.number > 0);

    //TODO: vet settings, to ensure that video settings (0x60)
    //were specified when type = 1, and that audio settings (0x61)
    //were specified when type = 2.

    typedef tracks_map_t::iterator map_iter_t;
    typedef std::pair<map_iter_t, bool> map_status_t;

    typedef tracks_set_t::iterator set_iter_t;
    typedef std::pair<set_iter_t, bool> set_status_t;

    if (i.type == 1)  //video
    {
        //assert(audioSettings.empty());  //TODO
        //i.settings.swap(videoSettings);

        assert(audioSettings.start < 0);
        assert(videoSettings.start >= 0);

        i.settings = videoSettings;

        VideoTrack* const t = new (std::nothrow) VideoTrack(m_pSegment, i);
        assert(t);  //TODO

        //m_tracks.push_back(t);

        const ULONG tn = static_cast<ULONG>(i.number);
        const tracks_map_t::value_type value(tn, t);
        const map_status_t map_status = m_tracks_map.insert(value);
        assert(map_status.second);

        const set_status_t set_status = m_video_tracks_set.insert(t);
        assert(set_status.second);
    }
    else if (i.type == 2)  //audio
    {
        //assert(videoSettings.empty());  //TODO
        //i.settings.swap(audioSettings);

        assert(videoSettings.start < 0);
        assert(audioSettings.start >= 0);

        i.settings = audioSettings;

        AudioTrack* const t = new (std::nothrow) AudioTrack(m_pSegment, i);
        assert(t);  //TODO

        //m_tracks.push_back(t);

        const ULONG tn = static_cast<ULONG>(i.number);
        const tracks_map_t::value_type value(tn, t);
        const map_status_t map_status = m_tracks_map.insert(value);
        assert(map_status.second);

        const set_status_t set_status = m_audio_tracks_set.insert(t);
        assert(set_status.second);
    }
#ifdef _DEBUG
    else
    {
        DebugBreak();
    }
#endif

    return;
}


Tracks::~Tracks()
{
    m_video_tracks_set.clear();
    m_audio_tracks_set.clear();

    typedef tracks_map_t::iterator iter_t;

    iter_t i = m_tracks_map.begin();
    const iter_t j = m_tracks_map.end();

    while (i != j)
    {
        Track* pTrack = i->second;
        assert(pTrack);

        m_tracks_map.erase(i++);

        delete pTrack;
    }
}


Track* Tracks::GetTrack(ULONG idx) const
{
    typedef tracks_map_t::const_iterator iter_t;
    const iter_t iter = m_tracks_map.find(idx);

    if (iter == m_tracks_map.end())
        return 0;

    Track* const pTrack = iter->second;
    assert(pTrack);

    return pTrack;
}


void Cluster::Load()
{
    assert(m_pSegment);
    assert(m_pos);
    assert(m_size);

    if (m_pos > 0)  //loaded
    {
        assert(m_size > 0);
        assert(m_timecode >= 0);
        return;
    }

    assert(m_pos < 0);  //not loaded yet
    assert(m_size < 0);
    assert(m_timecode < 0);

    IMkvFile* const pFile = m_pSegment->m_pFile;

    m_pos *= -1;                                //relative to segment
    __int64 pos = m_pSegment->m_start + m_pos;  //absolute

    long len;

    const __int64 id_ = ReadUInt(pFile, pos, len);
    id_;
    assert(id_ >= 0);
    assert(id_ == 0x0F43B675);  //Cluster ID

    pos += len;  //consume id

    const __int64 size_ = ReadUInt(pFile, pos, len);
    assert(size_ >= 0);

    pos += len;  //consume size

    //const __int64 start = pos;  //of payload
    m_size = size_;

    const __int64 stop = pos + size_;

    __int64 timecode = -1;

    while (pos < stop)
    {
        if (Match(pFile, pos, 0x67, timecode))
            break;
        else
        {
            const __int64 id = ReadUInt(pFile, pos, len);
            assert(id >= 0);  //TODO
            assert((pos + len) <= stop);

            pos += len;  //consume id

            const __int64 size = ReadUInt(pFile, pos, len);
            assert(size >= 0);  //TODO
            assert((pos + len) <= stop);

            pos += len;  //consume size

            if (id == 0x20)  //BlockGroup ID
                break;

            if (id == 0x23)  //SimpleBlock ID
                break;

            pos += size;  //consume payload
            assert(pos <= stop);
        }
    }

    assert(pos <= stop);
    assert(timecode >= 0);

    m_timecode = timecode;
}


Cluster* Cluster::Parse(
    Segment* pSegment,
    index_t idx,
    __int64 off)
{
    assert(pSegment);
    assert(off >= 0);
    assert(off < pSegment->m_size);

    Cluster* const pCluster = new (std::nothrow) Cluster(pSegment, idx, -off);
    assert(pCluster);

    return pCluster;
}


Cluster::Cluster() :
    m_pSegment(0),
    m_index(0),
    m_pos(0),
    m_size(0),
    m_timecode(0)
{
}

Cluster::Cluster(
    Segment* pSegment,
    index_t idx,
    __int64 off) :
    m_pSegment(pSegment),
    m_index(idx),
    m_pos(off),
    m_size(-1),
    m_timecode(-1)
{
}


Cluster::~Cluster()
{
    while (!m_entries.empty())
    {
        BlockEntry* pBlockEntry = m_entries.front();
        assert(pBlockEntry);

        m_entries.pop_front();
        delete pBlockEntry;
    }
}

bool Cluster::EOS() const
{
    return (m_pSegment == 0);
}


__int64 Cluster::GetSize() const
{
    IMkvFile* const pFile = m_pSegment->m_pFile;

    const __int64 pos_ = m_pSegment->m_start + _abs64(m_pos);
    __int64 pos = pos_;

    long len;

    const __int64 id = ReadUInt(pFile, pos, len);
    id;
    assert(id >= 0);
    assert(id == 0x0F43B675);  //Cluster ID

    pos += len;  //consume id

    const __int64 size = ReadUInt(pFile, pos, len);
    assert(size > 0);
    assert((m_size < 0) || (m_size == size));

    pos += len;  //consume size

    //pos now points to start of payload

    pos += size;  //consume payload

    //pos now points to end of payload (stop pos)

    const __int64 result = pos - pos_;
    return result;
}


void Cluster::LoadBlockEntries()
{
    if (!m_entries.empty())
        return;

    assert(m_pSegment);
    assert(m_pos);
    assert(m_size);

    IMkvFile* const pFile = m_pSegment->m_pFile;

    if (m_pos < 0)
        m_pos *= -1;

    __int64 pos = m_pSegment->m_start + m_pos;

    {
        long len;

        const __int64 id = ReadUInt(pFile, pos, len);
        id;
        assert(id >= 0);
        assert(id == 0x0F43B675);  //Cluster ID

        pos += len;  //consume id

        const __int64 size = ReadUInt(pFile, pos, len);
        assert(size > 0);

        pos += len;  //consume size

        //pos now points to start of payload

        if (m_size >= 0)
            assert(size == m_size);
        else
            m_size = size;
    }

    const __int64 stop = pos + m_size;
    __int64 timecode = -1;  //of cluster itself
    __int64 off = -1;
    __int64 prev_size = -1;

    while (pos < stop)
    {
        if (Match(pFile, pos, 0x67, timecode))
        {
            if (m_timecode >= 0)
                assert(timecode == m_timecode);
            else
                m_timecode = timecode;
        }
        else if (Match(pFile, pos, 0x27, off))
        {
#ifdef _DEBUG
            assert(m_pos >= 0);
            assert(off == m_pos);
#endif
        }
        else if (Match(pFile, pos, 0x2B, prev_size))
        {
#ifdef _DEBUG
            Cluster* const pPrev = m_pSegment->GetPrevious(this);

            if (pPrev == 0)
                assert(prev_size == 0);
            else
            {
                const __int64 prev_size_ = pPrev->GetSize();
                prev_size_;
                assert(prev_size_ == prev_size);
            }
#endif
        }
        else
        {
            long len;

            const __int64 id = ReadUInt(pFile, pos, len);
            assert(id >= 0);  //TODO
            assert((pos + len) <= stop);

            pos += len;  //consume id

            const __int64 size = ReadUInt(pFile, pos, len);
            assert(size >= 0);  //TODO
            assert((pos + len) <= stop);

            pos += len;  //consume size

            if (id == 0x20)  //BlockGroup ID
                ParseBlockGroup(pos, size);
            else if (id == 0x23)  //SimpleBlock ID
                ParseSimpleBlock(pos, size);

            pos += size;  //consume payload
            assert(pos <= stop);
        }
    }

    assert(pos == stop);
    assert(timecode >= 0);
    assert(!m_entries.empty());
}



__int64 Cluster::GetTimeCode()
{
    Load();
    return m_timecode;
}


__int64 Cluster::GetTime()
{
    const __int64 tc = GetTimeCode();
    tc;
    assert(tc >= 0);

    const SegmentInfo* const pInfo = m_pSegment->GetInfo();
    assert(pInfo);

    const __int64 scale = pInfo->GetTimeCodeScale();
    assert(scale >= 1);

    const __int64 t = m_timecode * scale;

    return t;
}


__int64 Cluster::GetFirstTime()
{
    const BlockEntry* const pEntry = GetFirst();

    if (pEntry == 0)  //empty cluster
        return GetTime();

    const Block* const pBlock = pEntry->GetBlock();
    assert(pBlock);

    return pBlock->GetTime(this);
}



void Cluster::ParseBlockGroup(__int64 start, __int64 size)
{
    const BlockEntry::index_t idx = m_entries.size();

    BlockGroup* const pGroup =
        new (std::nothrow) BlockGroup(this, idx, start, size);

    assert(pGroup);  //TODO

#ifdef _DEBUG
    if (!m_entries.empty())
    {
        const BlockEntry* const pFirstEntry = m_entries.back();
        assert(pFirstEntry);

        const Block* const pFirstBlock = pFirstEntry->GetBlock();
        assert(pFirstBlock);

        const SHORT t0 = pFirstBlock->GetRelativeTimeCode();

        const Block* const pBlock = pGroup->GetBlock();
        assert(pBlock);

        const SHORT t = pBlock->GetRelativeTimeCode();
        assert(t >= t0);
    }
#endif

    m_entries.push_back(pGroup);
}


void Cluster::ParseSimpleBlock(__int64 start, __int64 size)
{
    const BlockEntry::index_t idx = m_entries.size();

    SimpleBlock* const pSimpleBlock =
        new (std::nothrow) SimpleBlock(this, idx, start, size);

    assert(pSimpleBlock);  //TODO

#ifdef _DEBUG
    if (!m_entries.empty())
    {
        const BlockEntry* const pFirstEntry = m_entries.back();
        assert(pFirstEntry);

        const Block* const pFirstBlock = pFirstEntry->GetBlock();
        assert(pFirstBlock);

        const SHORT t0 = pFirstBlock->GetRelativeTimeCode();

        const Block* const pBlock = pSimpleBlock->GetBlock();
        assert(pBlock);

        const SHORT t = pBlock->GetRelativeTimeCode();
        assert(t >= t0);
    }
#endif

    m_entries.push_back(pSimpleBlock);
}


const BlockEntry* Cluster::GetFirst()
{
    LoadBlockEntries();

    const BlockEntry::entries_t& ee = m_entries;
    ee;
    assert(!ee.empty());

    return ee.front();
}


const BlockEntry* Cluster::GetLast()
{
    LoadBlockEntries();

    const BlockEntry::entries_t& ee = m_entries;
    ee;
    assert(!ee.empty());

    return ee.back();
}


const BlockEntry* Cluster::GetNext(const BlockEntry* pEntry) const
{
    assert(pEntry);

    const BlockEntry::entries_t& ee = m_entries;
    assert(!ee.empty());

    BlockEntry::index_t idx = pEntry->GetIndex();
    assert(idx < ee.size());
    assert(ee[idx] == pEntry);

    ++idx;

    if (idx >= ee.size())
        return 0;

    return ee[idx];
}


const BlockEntry* Cluster::GetEntry(const Track* pTrack)
{
    assert(pTrack);

    if (m_pSegment == 0)  //EOS
        return pTrack->GetEOS();

    LoadBlockEntries();

    const BlockEntry::entries_t& ee = m_entries;
    ee;
    assert(!ee.empty());

    typedef BlockEntry::entries_t::const_iterator iter_t;

    iter_t i = ee.begin();
    const iter_t j = ee.end();

    while (i != j)
    {
        const BlockEntry* const pEntry = *i++;
        assert(pEntry);
        assert(!pEntry->EOS());

        const Block* const pBlock = pEntry->GetBlock();
        assert(pBlock);

        if (pBlock->GetNumber() != pTrack->GetNumber())
            continue;

        if (pTrack->VetEntry(pEntry))
            return pEntry;
    }

    return pTrack->GetEOS();  //no satisfactory block found
}


const BlockEntry*
Cluster::GetEntry(
    const CuePoint& cp,
    const CuePoint::TrackPosition& tp)
{
    assert(m_pSegment);

    LoadBlockEntries();

    const BlockEntry::entries_t& ee = m_entries;
    ee;
    assert(!ee.empty());

    typedef BlockEntry::entries_t::const_iterator iter_t;

    iter_t i = ee.begin();
    const iter_t j = ee.end();

    if (tp.m_block > 0)
    {
        assert(tp.m_block <= ee.size());

        const int block = static_cast<int>(tp.m_block);
        const int off = block - 1;

        const iter_t k = i + off;
        assert(k != j);

        const BlockEntry* const pEntry = *k;
        assert(pEntry);
        assert(!pEntry->EOS());

        const Block* const pBlock = pEntry->GetBlock();
        pBlock;
        assert(pBlock);
        assert(pBlock->GetNumber() == tp.m_track);

        const __int64 timecode = pBlock->GetTimeCode(this);
        timecode;
        assert(timecode == cp.m_timecode);

        return pEntry;
    }

    while (i != j)
    {
        const BlockEntry* const pEntry = *i++;
        assert(pEntry);
        assert(!pEntry->EOS());

        const Block* const pBlock = pEntry->GetBlock();
        assert(pBlock);

        if (pBlock->GetNumber() != tp.m_track)
            continue;

        const __int64 timecode = pBlock->GetTimeCode(this);

        if (timecode == cp.m_timecode)
            return pEntry;

        assert(timecode < cp.m_timecode);
    }

    return 0;  //no satisfactory block found
}


const BlockEntry* Cluster::GetMaxKey(const VideoTrack* pTrack)
{
    assert(pTrack);

    if (m_pSegment == 0)  //EOS
        return pTrack->GetEOS();

    LoadBlockEntries();

    const BlockEntry::entries_t& ee = m_entries;
    ee;
    assert(!ee.empty());

    typedef BlockEntry::entries_t::const_reverse_iterator iter_t;

    iter_t i = ee.rbegin();
    const iter_t j = ee.rend();

    while (i != j)
    {
        const BlockEntry* const pEntry = *i++;
        assert(pEntry);
        assert(!pEntry->EOS());

        const Block* const pBlock = pEntry->GetBlock();
        assert(pBlock);

        if (pBlock->GetNumber() != pTrack->GetNumber())
            continue;

        if (pBlock->IsKey())
            return pEntry;
    }

    return pTrack->GetEOS();  //no satisfactory block found
}


BlockEntry::BlockEntry()
{
}


BlockEntry::~BlockEntry()
{
}



SimpleBlock::SimpleBlock(
    Cluster* pCluster,
    index_t idx,
    __int64 start,
    __int64 size) :
    m_pCluster(pCluster),
    m_index(idx),
    m_block(start, size, pCluster->m_pSegment->m_pFile)
{
}


bool SimpleBlock::EOS() const
{
    return false;
}


Cluster* SimpleBlock::GetCluster() const
{
    return m_pCluster;
}


BlockEntry::index_t SimpleBlock::GetIndex() const
{
    return m_index;
}


const Block* SimpleBlock::GetBlock() const
{
    return &m_block;
}


bool SimpleBlock::IsBFrame() const
{
    return false;
}


BlockGroup::BlockGroup(
    Cluster* pCluster,
    index_t idx,
    __int64 start,
    __int64 size_) :
    m_pCluster(pCluster),
    m_index(idx),
    //m_start(start),
    //m_size(size_),
    m_prevTimeCode(0),
    m_nextTimeCode(0),
    m_pBlock(0)  //TODO: accept multiple blocks within a block group
{
    IMkvFile* const pFile = m_pCluster->m_pSegment->m_pFile;

    __int64 pos = start;
    const __int64 stop = start + size_;

    bool bSimpleBlock = false;

    while (pos < stop)
    {
        SHORT t;

        if (Match(pFile, pos, 0x7B, t))
        {
            if (t < 0)
                m_prevTimeCode = t;
            else if (t > 0)
                m_nextTimeCode = t;
            else
                assert(false);
        }
        else
        {
            long len;
            const __int64 id = ReadUInt(pFile, pos, len);
            assert(id >= 0);  //TODO
            assert((pos + len) <= stop);

            pos += len;  //consume ID

            const __int64 size = ReadUInt(pFile, pos, len);
            assert(size >= 0);  //TODO
            assert((pos + len) <= stop);

            pos += len;  //consume size

            switch (id)
            {
                case 0x23:  //SimpleBlock ID
                    bSimpleBlock = true;
                    //YES, FALL THROUGH TO NEXT CASE

                case 0x21:  //Block ID
                    ParseBlock(pos, size);
                    break;

                default:
                    break;
            }

            pos += size;  //consume payload
            assert(pos <= stop);
        }
    }

    assert(pos == stop);
    assert(m_pBlock);

    if (!bSimpleBlock)
        m_pBlock->SetKey(m_prevTimeCode >= 0);
}


BlockGroup::~BlockGroup()
{
    delete m_pBlock;
}


void BlockGroup::ParseBlock(__int64 start, __int64 size)
{
    IMkvFile* const pFile = m_pCluster->m_pSegment->m_pFile;

    Block* const pBlock = new (std::nothrow) Block(start, size, pFile);
    assert(pBlock);  //TODO

    //TODO: the Matroska spec says you have multiple blocks within the
    //same block group, with blocks ranked by priority (the flag bits).
    //I haven't ever seen such a file (mkvmux certainly doesn't make
    //one), so until then I'll just assume block groups contain a single
    //block.
#if 0
    m_blocks.push_back(pBlock);
#else
    assert(m_pBlock == 0);
    m_pBlock = pBlock;
#endif

#if 0
    Track* const pTrack = pBlock->GetTrack();
    assert(pTrack);

    pTrack->Insert(pBlock);
#endif
}


bool BlockGroup::EOS() const
{
    return false;
}


Cluster* BlockGroup::GetCluster() const
{
    return m_pCluster;
}


BlockEntry::index_t BlockGroup::GetIndex() const
{
    return m_index;
}


const Block* BlockGroup::GetBlock() const
{
    return m_pBlock;
}


SHORT BlockGroup::GetPrevTimeCode() const
{
    return m_prevTimeCode;
}


SHORT BlockGroup::GetNextTimeCode() const
{
    return m_nextTimeCode;
}


bool BlockGroup::IsBFrame() const
{
    return (m_nextTimeCode > 0);
}



Block::Block(__int64 start, __int64 size_, IMkvFile* pFile) :
    m_start(start),
    m_size(size_)
{
    __int64 pos = start;
    const __int64 stop = start + size_;

    long len;

    m_track = ReadUInt(pFile, pos, len);
    assert(m_track > 0);
    assert((pos + len) <= stop);

    pos += len;  //consume track number
    assert((stop - pos) >= 2);

    m_timecode = Unserialize2SInt(pFile, pos);

    pos += 2;
    assert((stop - pos) >= 1);

    const HRESULT hr = pFile->MkvRead(pos, 1, &m_flags);
    hr;
    assert(hr == S_OK);

    //if (id == 0x21)  //Block ID
    //    assert(m_flags == 0);  //TODO
    //else
    //{
    //    assert(id == 0x23);  //SimpleBlock ID
    //    assert((m_flags & 0x7F) == 0);  //TODO
    //}

    ++pos;
    assert(pos <= stop);

    m_frame_off = pos;

    const __int64 frame_size = stop - pos;
    assert(frame_size <= LONG_MAX);

    m_frame_size = static_cast<LONG>(frame_size);
}


//Block::Block(ULONG track) :
//    m_pGroup(0),
//    m_start(0),
//    m_size(0),
//    m_track(track),
//    m_timecode(0),
//    m_flags(0),
//    m_frame_off(0),
//    m_frame_size(0)
//{
//}


//bool Block::EOS() const
//{
//    return (m_pGroup == 0);
//}


//Track* Block::GetTrack() const
//{
//    const Segment* const pSegment = m_pGroup->m_pCluster->m_pSegment;
//    const Tracks* const pTracks = pSegment->GetTracks();
//
//    const ULONG tn = static_cast<ULONG>(m_track);
//    Track* const pTrack = pTracks->GetTrack(tn);
//
//    return pTrack;
//}


__int64 Block::GetTimeCode(Cluster* pCluster) const
{
    assert(pCluster);

    const __int64 tc0 = pCluster->GetTimeCode();
    assert(tc0 >= 0);

    const __int64 tc = tc0 + __int64(m_timecode);
    assert(tc >= 0);

    return tc;  //unscaled timecode units
}


__int64 Block::GetTime(Cluster* pCluster) const
{
    assert(pCluster);

    const __int64 tc = GetTimeCode(pCluster);

    const Segment* const pSegment = pCluster->m_pSegment;
    const SegmentInfo* const pInfo = pSegment->GetInfo();
    assert(pInfo);

    const __int64 scale = pInfo->GetTimeCodeScale();
    assert(scale >= 1);

    const __int64 ns = tc * scale;

    return ns;
}



ULONG Block::GetNumber() const
{
    assert(m_track > 0);
    assert(m_track <= ULONG_MAX);

    return static_cast<ULONG>(m_track);
}


SHORT Block::GetRelativeTimeCode() const
{
    return m_timecode;
}


bool Block::IsKey() const
{
    return ((m_flags & BYTE(1 << 7)) != 0);
}


void Block::SetKey(bool bKey)
{
    if (bKey)
        m_flags |= BYTE(1 << 7);
    else
        m_flags &= 0x7F;
}


LONG Block::GetSize() const
{
    return m_frame_size;
}


HRESULT Block::Read(IMkvFile* pFile, BYTE* buf) const
{
    assert(pFile);
    assert(buf);

    const HRESULT hr = pFile->MkvRead(m_frame_off, m_frame_size, buf);

    return hr;
}


}  //end namespace MkvParser
