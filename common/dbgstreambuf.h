// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once

//#include <streambuf>

template<typename elem_t, typename traits_t>
class basic_dbgstreambuf : public std::basic_streambuf<elem_t, traits_t>
{
public:

    //typedef std::basic_streambuf<elem_t, traits_t> base_t;

    basic_dbgstreambuf();
    ~basic_dbgstreambuf();

protected:

    std::streamsize plen() const;
    std::streamsize ppos() const;
    void ppos(std::streamsize);

    int_type overflow(int_type c);

    std::streamsize xsputn(const elem_t*, std::streamsize);

    int sync();

    pos_type seekoff(
        off_type off,
        std::ios_base::seekdir way,
        std::ios_base::openmode which);

    pos_type seekpos(
        pos_type pos,
        std::ios_base::openmode which);

private:

    basic_dbgstreambuf(const basic_dbgstreambuf<elem_t, traits_t>&);

    basic_dbgstreambuf<elem_t, traits_t>&
        operator=(const basic_dbgstreambuf<elem_t, traits_t>&);

    //void resize(std::basic_string<TCHAR>::size_type);
    //std::basic_string<TCHAR> m_buf;
    elem_t* m_buf;

};


template<typename elem_t, typename traits_t>
inline basic_dbgstreambuf<elem_t, traits_t>::basic_dbgstreambuf()
    : m_buf(0)
{
    //resize(1);
}

template<typename elem_t, typename traits_t>
inline basic_dbgstreambuf<elem_t, traits_t>::~basic_dbgstreambuf()
{
    sync();
    setp(0, 0, 0);
    delete[] m_buf;
}


template<typename elem_t, typename traits_t>
inline std::streamsize basic_dbgstreambuf<elem_t, traits_t>::plen() const
{
    const ptrdiff_t result = epptr() - pptr();
    return static_cast<std::streamsize>(result);
}


template<typename elem_t, typename traits_t>
inline std::streamsize basic_dbgstreambuf<elem_t, traits_t>::ppos() const
{
    const ptrdiff_t result = pptr() - pbase();
    return static_cast<std::streamsize>(result);
}


template<typename elem_t, typename traits_t>
inline void basic_dbgstreambuf<elem_t, traits_t>::ppos(std::streamsize pos)
{
    pbump(int(pos) - int(ppos()));
}


template<typename elem_t>
void OutputDebugStringX(const elem_t*);

template<>
inline void OutputDebugStringX(const char* str)
{
    ::OutputDebugStringA(str);
}

template<>
inline void OutputDebugStringX(const wchar_t* str)
{
    ::OutputDebugStringW(str);
}


template<typename elem_t, typename traits_t>
inline typename basic_dbgstreambuf<elem_t, traits_t>::int_type
basic_dbgstreambuf<elem_t, traits_t>::overflow(int_type c_)
{
    if (traits_t::eq_int_type(traits_t::eof(), c_))
        return traits_t::eof();

    const elem_t c = traits_t::to_char_type(c_);

    //sync();
    //NOTE: No, we can't do this here, since dbgview will
    //break the text across lines when the auto-scroll
    //option is enabled.

    const ptrdiff_t oldlen = epptr() - pbase();
    const ptrdiff_t newlen = oldlen ? 2 * oldlen : 1;

    if (elem_t* const newbuf = new (std::nothrow) elem_t[newlen + 1])
    {
        const std::streamsize pos = ppos();

#if _MSC_VER >= 1400
        const size_t size_in_bytes = newlen * sizeof(elem_t);
        const size_t pos_ = static_cast<size_t>(pos);
        traits_t::_Copy_s(newbuf, size_in_bytes, pbase(), pos_);
#else
        traits_t::copy(newbuf, pbase(), pos);
#endif

        setp(newbuf, newbuf + pos, newbuf + newlen);

        delete[] m_buf;
        m_buf = newbuf;

        *pptr() = c;
        pbump(1);

        return traits_t::not_eof(c_);
    }

    if (oldlen)
    {
        sync();

        *pbase() = c;
        pbump(1);
    }
    else
    {
        const elem_t str[2] = { c, elem_t() };
        OutputDebugStringX(str);
    }

    return traits_t::not_eof(c_);
}


template<typename elem_t, typename traits_t>
inline std::streamsize basic_dbgstreambuf<elem_t, traits_t>::xsputn(
    const elem_t* str,
    std::streamsize n)
{
    if (n <= plen())
    {
#if _MSC_VER >= 1400
        const size_t plen_ = static_cast<size_t>(plen());
        const size_t size_in_bytes = plen_ * sizeof(elem_t);
        const size_t nn = static_cast<size_t>(n);
        traits_t::_Copy_s(pptr(), size_in_bytes, str, nn);
#else
        traits_t::copy(pptr(), str, n);
#endif

        const int off = static_cast<int>(n);
        pbump(off);

        return n;
    }

    const std::streamsize pos = ppos();
    const std::streamsize newlen = pos + n;
    const size_t newlen_ = static_cast<size_t>(newlen);

    if (elem_t* const newbuf = new (std::nothrow) elem_t[newlen_ + 1])
    {
#if _MSC_VER >= 1400
        size_t size_in_bytes = newlen_ * sizeof(elem_t);
        const size_t pos_ = static_cast<size_t>(pos);
        traits_t::_Copy_s(newbuf, size_in_bytes, pbase(), pos_);
#else
        traits_t::copy(newbuf, pbase(), pos);
#endif

        setp(newbuf, newbuf + pos, newbuf + newlen);

        delete[] m_buf;
        m_buf = newbuf;

#if _MSC_VER >= 1400
        const size_t plen_ = static_cast<size_t>(plen());
        size_in_bytes = plen_ * sizeof(elem_t);
        const size_t nn = static_cast<size_t>(n);
        traits_t::_Copy_s(pptr(), size_in_bytes, str, nn);
#else
        traits_t::copy(pptr(), str, n);
#endif

        const int off = static_cast<int>(n);
        pbump(off);

        return n;
    }

    const ptrdiff_t oldlen_ = epptr() - pbase();

    if (oldlen_ == 0)
    {
        elem_t buf[2];

        buf[1] = elem_t();

        for (std::streamsize i = 0; i < n; ++i)
        {
            buf[0] = *str++;
            OutputDebugStringX(buf);
        }

        return n;
    }

    std::streamsize nn = n;

    if (std::streamsize len = plen())
    {
#if _MSC_VER >= 1400
        const size_t len_ = static_cast<size_t>(len);
        const size_t size_in_bytes = len_ * sizeof(elem_t);
        traits_t::_Copy_s(pptr(), size_in_bytes, str, len_);
#else
        traits_t::copy(pptr(), str, len);
#endif

        const int off = static_cast<int>(len);
        pbump(off);

        str += len;
        nn -= len;
    }

    const std::streamsize oldlen = static_cast<std::streamsize>(oldlen_);

#if _MSC_VER >= 1400
    const size_t size_in_bytes = oldlen_ * sizeof(elem_t);
#endif

    for (;;)
    {
        sync();

        if (nn <= oldlen)
        {
#if _MSC_VER >= 1400
            const size_t nnn = static_cast<size_t>(nn);
            traits_t::_Copy_s(pbase(), size_in_bytes, str, nnn);
#else
            traits_t::copy(pbase(), str, nn);
#endif

            const int off = static_cast<int>(nn);
            pbump(off);

            return n;
        }

#if _MSC_VER >= 1400
        traits_t::_Copy_s(pbase(), size_in_bytes, str, oldlen_);
#else
        traits_t::copy(pbase(), str, oldlen);
#endif

        const int off = static_cast<int>(oldlen);
        pbump(off);

        str += oldlen;
        nn -= oldlen;
    }
}


template<typename elem_t, typename traits_t>
inline int basic_dbgstreambuf<elem_t, traits_t>::sync()
{
    if (ppos() == 0)  //avoid unnecessary carriage return in dbgview
        return 0;

    *pptr() = elem_t();
    OutputDebugStringX(pbase());

    ppos(0);

    return 0;
}


//template<typename elem_t, typename traits_t>
//inline void basic_dbgstreambuf<elem_t, traits_t>::resize(
//   std::basic_string<TCHAR>::size_type n)
//{
//    m_buf.resize(n);
//
//    const TCHAR* const const_buf = m_buf.c_str();
//    TCHAR* const buf = const_cast<TCHAR*>(const_buf);
//
//    setp(buf, buf, buf + n);
//}


template<typename elem_t, typename traits_t>
inline typename basic_dbgstreambuf<elem_t, traits_t>::pos_type
basic_dbgstreambuf<elem_t, traits_t>::seekoff(
    off_type off,
    std::ios_base::seekdir way,
    std::ios_base::openmode which)
{
    off_type pos = -1;

    if (which & std::ios_base::out)
    {
       const ptrdiff_t buflen = epptr() - pbase();

        switch (way)
        {
            case std::ios_base::beg:
                pos = off;
                break;

            case std::ios_base::cur:
                pos = off_type(ppos()) + off;
                break;

            case std::ios_base::end:
                pos = off_type(buflen) + off;
                break;
        }

        if (pos < 0)
            pos = -1;
        else if (pos > off_type(buflen))
            pos = -1;
        else
            ppos(pos);
    }

    return pos;
}


template<typename elem_t, typename traits_t>
inline typename basic_dbgstreambuf<elem_t, traits_t>::pos_type
basic_dbgstreambuf<elem_t, traits_t>::seekpos(
    pos_type pos_,
    std::ios_base::openmode which)
{
    off_type pos = -1;

    if (which & std::ios_base::out)
    {
        pos = pos_;

        const ptrdiff_t buflen = epptr() - pbase();

        if (pos < 0)
            pos = -1;
        else if (pos > off_type(buflen))
            pos = -1;
        else
            ppos(pos);
    }

    return pos;
}
