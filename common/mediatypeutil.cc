// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "mediatypeutil.h"
//#include <new>
#include <vfwmsgs.h>

HRESULT MediaTypeUtil::Create(
    const AM_MEDIA_TYPE& src,
    AM_MEDIA_TYPE*& ptgt)
{
    ptgt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));

    if (ptgt == 0)
        return E_OUTOFMEMORY;

    const HRESULT hr = Copy(src, *ptgt);

    if (FAILED(hr))
    {
        CoTaskMemFree(ptgt);
        ptgt = 0;
    }

    return hr;
}


void MediaTypeUtil::Free(AM_MEDIA_TYPE* p)
{
    if (p)
    {
        Destroy(*p);
        CoTaskMemFree(p);
    }
}


HRESULT MediaTypeUtil::Copy(
    const AM_MEDIA_TYPE& src,
    AM_MEDIA_TYPE& tgt)
{
    tgt = src;

    if (src.cbFormat == 0)
    {
       tgt.pbFormat = 0;
       return S_OK;
    }

    tgt.pbFormat = (BYTE*)CoTaskMemAlloc(src.cbFormat);

    if (tgt.pbFormat == 0)
    {
        tgt.cbFormat = 0;
        return E_OUTOFMEMORY;
    }

    memcpy(tgt.pbFormat, src.pbFormat, src.cbFormat);

    return S_OK;
}


void MediaTypeUtil::Destroy(AM_MEDIA_TYPE& tgt)
{
    CoTaskMemFree(tgt.pbFormat);

    tgt.pbFormat = 0;
    tgt.cbFormat = 0;
}
