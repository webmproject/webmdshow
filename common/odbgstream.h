// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once

#include <ostream>
#include "dbgstreambuf.h"

template<typename elem_t, typename traits_t>
class basic_odbgstream : public std::basic_ostream<elem_t, traits_t>
{
public:

    typedef std::basic_ostream<elem_t, traits_t> base_t;
    typedef basic_dbgstreambuf<elem_t, traits_t> dbgstreambuf;

    basic_odbgstream();

    const dbgstreambuf* rdbuf() const { return &m_sb; }
    dbgstreambuf* rdbuf() { return &m_sb; }

private:

    basic_odbgstream(const basic_odbgstream<elem_t, traits_t>&);

    basic_odbgstream<elem_t, traits_t>&
        operator=(const basic_odbgstream<elem_t, traits_t>&);

    dbgstreambuf m_sb;

};


template<typename elem_t, typename traits_t>
inline basic_odbgstream<elem_t, traits_t>::basic_odbgstream()
    : base_t(0)
{
    base_t::rdbuf(&m_sb);
}


typedef basic_odbgstream<char, std::char_traits<char> > odbgstream;
typedef basic_odbgstream<wchar_t, std::char_traits<wchar_t> > wodbgstream;
