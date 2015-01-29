// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "tenumxxx.h"
#include <vector>

class CMediaTypes
{
    CMediaTypes(const CMediaTypes&);
    CMediaTypes& operator=(const CMediaTypes&);

public:

    CMediaTypes();  //statically allocated and destroyed
    ~CMediaTypes();

    HRESULT Clear();
    HRESULT Add(const AM_MEDIA_TYPE&);

    ULONG Size() const;
    bool Empty() const { return m_vec.empty(); }

    const AM_MEDIA_TYPE& operator[](ULONG) const;
    AM_MEDIA_TYPE& operator[](ULONG);

    HRESULT Copy(ULONG, AM_MEDIA_TYPE&) const;
    HRESULT Create(ULONG, AM_MEDIA_TYPE*&) const;

    HRESULT CreateEnum(IPin*, IEnumMediaTypes**);

private:

    HRESULT GetCount(ULONG&) const;
    HRESULT GetItem(ULONG, AM_MEDIA_TYPE*&);
    void ReleaseItems(AM_MEDIA_TYPE**, ULONG);

    typedef std::vector<AM_MEDIA_TYPE> vec_t;
    vec_t m_vec;

    typedef TEnumXXX<IEnumMediaTypes, AM_MEDIA_TYPE*> base_t;

    class CEnumMediaTypes : public base_t
    {
    public:

        CEnumMediaTypes(IPin*, CMediaTypes*);

    private:

        CEnumMediaTypes(const CEnumMediaTypes&);

        ~CEnumMediaTypes();

        CEnumMediaTypes& operator=(const CEnumMediaTypes&);

    public:

        HRESULT STDMETHODCALLTYPE Clone(IEnumMediaTypes**);

    protected:

        HRESULT GetCount(ULONG&) const;
        HRESULT GetItem(ULONG, AM_MEDIA_TYPE*&);
        void ReleaseItems(AM_MEDIA_TYPE**, ULONG);

    private:

        IPin* const m_pPin;
        CMediaTypes* const m_pMediaTypes;

    };

};

