// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webmmuxfilter.hpp"
#include <uuids.h>
#include <amvideo.h>   //VIDEOINFOHEADER
#include <dvdmedia.h>  //VIDEOINFOHEADER2
#include "webmmuxstreamvideovpx.hpp"
#include "webmtypes.hpp"
#include <cassert>

namespace WebmMuxLib
{

InpinVideo::InpinVideo(Filter* p) :
    Inpin(p, L"video")
{
    AM_MEDIA_TYPE mt;

    mt.majortype = MEDIATYPE_Video;
    //mt.subtype = (see below)

    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = TRUE;
    mt.lSampleSize = 0;
    mt.formattype = GUID_NULL;
    mt.pUnk = 0;
    mt.cbFormat = 0;
    mt.pbFormat = 0;

    mt.subtype = WebmTypes::MEDIASUBTYPE_VP80;
    m_preferred_mtv.Add(mt);
}


InpinVideo::~InpinVideo()
{
}


HRESULT InpinVideo::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    if (pmt == 0)
        return E_INVALIDARG;

    const AM_MEDIA_TYPE& mt = *pmt;

    if (mt.majortype != MEDIATYPE_Video)
        return S_FALSE;

    if (mt.subtype == WebmTypes::MEDIASUBTYPE_VP80)
        return QueryAcceptVP80(mt);

    //TODO: more vetting here

    return S_FALSE;
}



HRESULT InpinVideo::QueryAcceptVP80(const AM_MEDIA_TYPE& mt) const
{
    if (mt.pbFormat == 0)
        return S_FALSE;

    if (mt.formattype == FORMAT_VideoInfo)
    {
        if (mt.cbFormat < sizeof(VIDEOINFOHEADER))
            return S_FALSE;

        const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);

        return VetBitmapInfoHeader(vih.bmiHeader);
    }

    if (mt.formattype == FORMAT_VideoInfo2)
    {
        if (mt.cbFormat < sizeof(VIDEOINFOHEADER2))
            return S_FALSE;

        const VIDEOINFOHEADER2& vih = (VIDEOINFOHEADER2&)(*mt.pbFormat);

        return VetBitmapInfoHeader(vih.bmiHeader);
    }

    return S_FALSE;
}


HRESULT InpinVideo::VetBitmapInfoHeader(const BITMAPINFOHEADER& bmih) const
{
    if (bmih.biSize < sizeof(BITMAPINFOHEADER))
        return S_FALSE;

    //TODO: we should verify that the sum of the offset to the BMIH and
    //its size is less than cbFormat.

    if (bmih.biWidth <= 0)
        return S_FALSE;

    if (bmih.biHeight <= 0)
        return S_FALSE;

    return S_OK;
}


HRESULT InpinVideo::GetAllocatorRequirements(ALLOCATOR_PROPERTIES* p)
{
    if (p == 0)
        return E_POINTER;

    ALLOCATOR_PROPERTIES& props = *p;

    assert(m_connection_mtv.Size() == 1);

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];

    if (mt.subtype == WebmTypes::MEDIASUBTYPE_VP80)
        return GetAllocatorRequirementsVP80(props);

    assert(false);
    return E_FAIL;
}


HRESULT InpinVideo::GetAllocatorRequirementsVP80(
    ALLOCATOR_PROPERTIES& props) const
{
    //We hold onto samples, so at least as many samples in the as we expect
    //to exist in a cluster.  Assume pessimistically that the framerate is
    //30 fps, and that keyframes arrive no slower than once every 3 seconds.
    //
    //TODO: If these assumptions are incorrect, or the upstream pin does not
    //honor are allocator requirements, then the stream will stall.  We should
    //have a graceful way of handling these cases.

    props.cBuffers = 3 * 30;

    props.cbBuffer = 0;  //let upstream pin decide size
    props.cbAlign = 0;
    props.cbPrefix = 0;

    return S_OK;
}


HRESULT InpinVideo::OnInit()
{
    assert(bool(m_pPinConnection));
    assert(m_pStream == 0);
    assert(m_connection_mtv.Size() == 1);

    const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
    assert(mt.majortype == MEDIATYPE_Video);

    Context& ctx = m_pFilter->m_ctx;
    StreamVideo* pStream;

    if (mt.subtype == WebmTypes::MEDIASUBTYPE_VP80)
        pStream = CreateStreamVP80(ctx, mt);
    else
    {
        assert(false);
        return E_FAIL;
    }

    assert(pStream);

    ctx.SetVideoStream(pStream);
    m_pStream = pStream;

    return S_OK;
}


StreamVideo*
InpinVideo::CreateStreamVP80(Context& ctx, const AM_MEDIA_TYPE& mt)
{
    return new (std::nothrow) StreamVideoVPx(ctx, mt);
}


void InpinVideo::OnFinal()
{
   Context& ctx = m_pFilter->m_ctx;
   ctx.SetVideoStream(0);
}


HANDLE InpinVideo::GetOtherHandle() const
{
    const InpinAudio& ia = m_pFilter->m_inpin_audio;
    return ia.m_hSample;
}


} //end namespace WebmMuxLib
