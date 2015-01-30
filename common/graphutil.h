// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#ifndef __strmif_h__
#include <strmif.h>
#endif
#ifndef _INC_COMDEF
#include <comdef.h>
#endif
#ifndef _STRING_
#include <string>
#endif

namespace GraphUtil
{
    _COM_SMARTPTR_TYPEDEF(IFilterGraph, __uuidof(IFilterGraph));
    _COM_SMARTPTR_TYPEDEF(IGraphBuilder, __uuidof(IGraphBuilder));
    _COM_SMARTPTR_TYPEDEF(IGraphConfig, __uuidof(IGraphConfig));
    _COM_SMARTPTR_TYPEDEF(IBaseFilter, __uuidof(IBaseFilter));
    _COM_SMARTPTR_TYPEDEF(IMediaFilter, __uuidof(IMediaFilter));
    _COM_SMARTPTR_TYPEDEF(IPin, __uuidof(IPin));
    _COM_SMARTPTR_TYPEDEF(IMemAllocator, __uuidof(IMemAllocator));
    _COM_SMARTPTR_TYPEDEF(IMemInputPin, __uuidof(IMemInputPin));
    _COM_SMARTPTR_TYPEDEF(IFileSourceFilter, __uuidof(IFileSourceFilter));
    _COM_SMARTPTR_TYPEDEF(IFileSinkFilter, __uuidof(IFileSinkFilter));
    _COM_SMARTPTR_TYPEDEF(IEnumPins, __uuidof(IEnumPins));
    _COM_SMARTPTR_TYPEDEF(IEnumMediaTypes, __uuidof(IEnumMediaTypes));
    _COM_SMARTPTR_TYPEDEF(IFilterMapper2, __uuidof(IFilterMapper2));
    _COM_SMARTPTR_TYPEDEF(IAsyncReader, __uuidof(IAsyncReader));

#ifdef __control_h__
    _COM_SMARTPTR_TYPEDEF(IMediaEvent, __uuidof(IMediaEvent));
    _COM_SMARTPTR_TYPEDEF(IMediaControl, __uuidof(IMediaControl));
#endif

    _COM_SMARTPTR_TYPEDEF(IMediaSeeking, __uuidof(IMediaSeeking));
    _COM_SMARTPTR_TYPEDEF(IMediaSample, __uuidof(IMediaSample));
    _COM_SMARTPTR_TYPEDEF(IMediaEventSink, __uuidof(IMediaEventSink));

    IPinPtr FindPin(IBaseFilter*, PIN_DIRECTION);
    IPinPtr FindPin(
        IBaseFilter*,
        PIN_DIRECTION,
        const GUID& majortype,
        const GUID* subtype = 0);

    IPinPtr FindOutpin(IBaseFilter*);
    IPinPtr FindInpin(IBaseFilter*);
    IPinPtr FindOutpinVideo(IBaseFilter*);
    IPinPtr FindOutpinAudio(IBaseFilter*);
    IPinPtr FindInpinVideo(IBaseFilter*);
    IPinPtr FindInpinAudio(IBaseFilter*);

    ULONG PinCount(IBaseFilter*);
    ULONG PinCount(IBaseFilter*, PIN_DIRECTION);
    ULONG InpinCount(IBaseFilter*);
    ULONG OutpinCount(IBaseFilter*);

    HRESULT ConnectDirect(
                IFilterGraph*,
                IBaseFilter* fOut,
                IBaseFilter* fIn,
                const AM_MEDIA_TYPE* pmt = 0);

    bool Match(IPin*, const GUID& majortype, const GUID* subtype = 0);

    std::wstring ToString(const GUID&);

    struct FourCCGUID : GUID
    {
        explicit FourCCGUID(const char*);
        explicit FourCCGUID(DWORD);

        static bool IsFourCC(const GUID&);
    };

}  //end namespace GraphUtil
