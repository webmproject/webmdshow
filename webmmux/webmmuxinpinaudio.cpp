// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webmmuxfilter.hpp"
#include "webmmuxstreamaudiovorbis.hpp"
#include "webmmuxstreamaudiovorbisogg.hpp"
#include "vorbistypes.hpp"
#include <uuids.h>
#include <vfwmsgs.h>
#include <cassert>

namespace WebmMuxLib
{

InpinAudio::InpinAudio(Filter* p) :
    Inpin(p, L"audio")
{
    CMediaTypes& mtv = m_preferred_mtv;

    StreamAudioVorbis::GetMediaTypes(mtv);
}


InpinAudio::~InpinAudio()
{
}


HRESULT InpinAudio::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    if (pmt == 0)
        return E_INVALIDARG;

    const AM_MEDIA_TYPE& mt = *pmt;

    if (StreamAudioVorbis::QueryAccept(mt))
        return S_OK;

    if (StreamAudioVorbisOgg::QueryAccept(mt))
        return S_OK;

    return S_FALSE;
}


HRESULT InpinAudio::GetAllocatorRequirements(ALLOCATOR_PROPERTIES*)
{
#if 1
    return E_NOTIMPL;
#else
    if (p == 0)
        return E_POINTER;

    assert(bool(m_pPinConnection));
    assert(!m_connection_mtv.Empty());

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    ALLOCATOR_PROPERTIES& props = *p;

    if (mt.subtype == VorbisTypes::MEDIASUBTYPE_Vorbis2)
        return StreamAudioVorbis::GetAllocatorRequirements(mt, props);

    else if (mt.subtype == VorbisTypes::MEDIASUBTYPE_Vorbis)
        return StreamAudioVorbisOgg::GetAllocatorRequirements(mt, props);

    return VFW_E_NOT_CONNECTED;
#endif
}



HRESULT InpinAudio::OnInit()
{
    assert(bool(m_pPinConnection));
    assert(!m_connection_mtv.Empty());

    Context& ctx = m_pFilter->m_ctx;
    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];

    StreamAudio* pStream;

    if (mt.subtype == VorbisTypes::MEDIASUBTYPE_Vorbis2)
        pStream = StreamAudioVorbis::CreateStream(ctx, mt);

    else if (mt.subtype == VorbisTypes::MEDIASUBTYPE_Vorbis)
        pStream = StreamAudioVorbisOgg::CreateStream(ctx, mt);

    else
        return E_FAIL;  //should never happen

    ctx.SetAudioStream(pStream);  //TODO: accept multiple audio streams
    m_pStream = pStream;

    return S_OK;
}


void InpinAudio::OnFinal()
{
   Context& ctx = m_pFilter->m_ctx;
   ctx.SetAudioStream(0);
}


HANDLE InpinAudio::GetOtherHandle() const
{
    const InpinVideo& iv = m_pFilter->m_inpin_video;
    return iv.m_hSample;
}


} //end namespace WebmMuxLib
