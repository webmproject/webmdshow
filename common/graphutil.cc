// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <control.h>
#include <uuids.h>
#include "graphutil.h"
#include "mediatypeutil.h"
#include <cassert>


GraphUtil::IPinPtr GraphUtil::FindOutpin(IBaseFilter* f)
{
    return FindPin(f, PINDIR_OUTPUT);
}


GraphUtil::IPinPtr GraphUtil::FindInpin(IBaseFilter* f)
{
    return FindPin(f, PINDIR_INPUT);
}


GraphUtil::IPinPtr GraphUtil::FindOutpinVideo(IBaseFilter* f)
{
    return FindPin(f, PINDIR_OUTPUT, MEDIATYPE_Video);
}


GraphUtil::IPinPtr GraphUtil::FindOutpinAudio(IBaseFilter* f)
{
    return FindPin(f, PINDIR_OUTPUT, MEDIATYPE_Audio);
}


GraphUtil::IPinPtr GraphUtil::FindInpinVideo(IBaseFilter* f)
{
    return FindPin(f, PINDIR_INPUT, MEDIATYPE_Video);
}


GraphUtil::IPinPtr GraphUtil::FindInpinAudio(IBaseFilter* f)
{
    return FindPin(f, PINDIR_INPUT, MEDIATYPE_Audio);
}


ULONG GraphUtil::InpinCount(IBaseFilter* f)
{
    return PinCount(f, PINDIR_INPUT);
}


ULONG GraphUtil::OutpinCount(IBaseFilter* f)
{
    return PinCount(f, PINDIR_OUTPUT);
}


GraphUtil::IPinPtr
GraphUtil::FindPin(IBaseFilter* f, PIN_DIRECTION dir_requested)
{
    assert(f);

    IEnumPinsPtr e;

    HRESULT hr = f->EnumPins(&e);

    if (FAILED(hr))
        return 0;

    assert(bool(e));

    for (;;)
    {
        IPinPtr p;

        hr = e->Next(1, &p, 0);

        if (hr != S_OK)
            return 0;

        assert(bool(p));

        PIN_DIRECTION dir_actual;

        hr = p->QueryDirection(&dir_actual);

        if (SUCCEEDED(hr) && (dir_actual == dir_requested))
            return p;
    }
}


GraphUtil::IPinPtr
GraphUtil::FindPin(
    IBaseFilter* f,
    PIN_DIRECTION dir_requested,
    const GUID& majortype,
    const GUID* subtype)
{
    assert(f);

    IEnumPinsPtr e;

    HRESULT hr = f->EnumPins(&e);

    if (FAILED(hr))
        return 0;

    assert(bool(e));

    for (;;)
    {
        IPinPtr p;

        hr = e->Next(1, &p, 0);

        if (hr != S_OK)
            return 0;

        assert(bool(p));

        PIN_DIRECTION dir_actual;

        hr = p->QueryDirection(&dir_actual);

        if (FAILED(hr) || (dir_actual != dir_requested))
            continue;

        if (Match(p, majortype, subtype))
            return p;
    }
}


ULONG GraphUtil::PinCount(IBaseFilter* f)
{
    assert(f);

    IEnumPinsPtr e;

    HRESULT hr = f->EnumPins(&e);

    if (FAILED(hr))
        return 0;

    assert(bool(e));

    ULONG n = 0;

    for (;;)
    {
        IPinPtr p;

        hr = e->Next(1, &p, 0);

        if (hr != S_OK)
            return n;

        assert(bool(p));

        ++n;
    }
}


ULONG GraphUtil::PinCount(IBaseFilter* f, PIN_DIRECTION dir_requested)
{
    assert(f);

    IEnumPinsPtr e;

    HRESULT hr = f->EnumPins(&e);

    if (FAILED(hr))
        return 0;

    assert(bool(e));

    ULONG n = 0;

    for (;;)
    {
        IPinPtr p;

        hr = e->Next(1, &p, 0);

        if (hr != S_OK)
            return n;

        assert(bool(p));

        PIN_DIRECTION dir_actual;

        hr = p->QueryDirection(&dir_actual);

        if (SUCCEEDED(hr) && (dir_actual == dir_requested))
            ++n;
    }
}


HRESULT GraphUtil::ConnectDirect(
    IFilterGraph* pGraph,
    IBaseFilter* fOut,
    IBaseFilter* fIn,
    const AM_MEDIA_TYPE* pmt)
{
    assert(pGraph);
    assert(fOut);
    assert(fIn);

    const IPinPtr pOut(FindOutpin(fOut));

    if (!bool(pOut))
        return E_FAIL;

    const IPinPtr pIn(FindInpin(fIn));

    if (!bool(pIn))
        return E_FAIL;

    return pGraph->ConnectDirect(pOut, pIn, pmt);
}


bool GraphUtil::Match(
    IPin* pin,
    const GUID& majortype,
    const GUID* subtype)
{
    assert(pin);

    IEnumMediaTypesPtr e;

    HRESULT hr = pin->EnumMediaTypes(&e);

    if (FAILED(hr))
        return false;

    assert(bool(e));

    for (;;)
    {
        AM_MEDIA_TYPE* pmt;

        hr = e->Next(1, &pmt, 0);

        if (hr != S_OK)
            return false;

        assert(pmt);

        const int bMajor = (pmt->majortype == majortype);
        const int bSubtype = subtype ? (pmt->subtype == *subtype) : 1;

        MediaTypeUtil::Free(pmt);

        if (bMajor && bSubtype)
            return true;
    }
}


std::wstring GraphUtil::ToString(const GUID& g)
{
    enum { cch = 39 };
    wchar_t str[cch];

    const int n = StringFromGUID2(g, str, cch);
    n;
    assert(n == 39);

    return str;
}


namespace GraphUtil
{

FourCCGUID::FourCCGUID(const char* str)
{
    assert(strlen(str) == 4);
    memcpy(&Data1, str, 4);
    Data2 = 0x0000;
    Data3 = 0x0010;
    Data4[0] = 0x80;
    Data4[1] = 0x00;
    Data4[2] = 0x00;
    Data4[3] = 0xAA;
    Data4[4] = 0x00;
    Data4[5] = 0x38;
    Data4[6] = 0x9B;
    Data4[7] = 0x71;
}

FourCCGUID::FourCCGUID(DWORD Data1_)
{
    Data1 = Data1_;
    Data2 = 0x0000;
    Data3 = 0x0010;
    Data4[0] = 0x80;
    Data4[1] = 0x00;
    Data4[2] = 0x00;
    Data4[3] = 0xAA;
    Data4[4] = 0x00;
    Data4[5] = 0x38;
    Data4[6] = 0x9B;
    Data4[7] = 0x71;
}


bool FourCCGUID::IsFourCC(const GUID& guid)
{
    if (guid.Data2 != 0x0000)
        return false;

    if (guid.Data3 != 0x0010)
        return false;

    if (guid.Data4[0] != 0x80)
        return false;

    if (guid.Data4[1] != 0x00)
        return false;

    if (guid.Data4[2] != 0x00)
        return false;

    if (guid.Data4[3] != 0xAA)
        return false;

    if (guid.Data4[4] != 0x00)
        return false;

    if (guid.Data4[5] != 0x38)
        return false;

    if (guid.Data4[6] != 0x9B)
        return false;

    if (guid.Data4[7] != 0x71)
        return false;

    return true;
}


} //end namespace GraphUtil
