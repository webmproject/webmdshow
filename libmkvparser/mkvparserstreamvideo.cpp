// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "mkvparserstreamvideo.hpp"
#include "mkvparser.hpp"
#include "webmtypes.hpp"
#include "graphutil.hpp"
#include <cassert>
#include <amvideo.h>
#include <dvdmedia.h>
#include <uuids.h>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif

static const char* const s_CodecId_VP8 = "V_VP8";
static const char* const s_CodecId_ON2VP8 = "V_ON2VP8";
static const char* const s_CodecId_VFW = "V_MS/VFW/FOURCC";

namespace MkvParser
{


VideoStream* VideoStream::CreateInstance(VideoTrack* pTrack)
{
    assert(pTrack);

    const char* const id = pTrack->GetCodecId();
    assert(id);  //TODO

    if (_stricmp(id, s_CodecId_VP8) == 0)
        __noop;
    else if (_stricmp(id, s_CodecId_ON2VP8) == 0)
        __noop;
    else if (_stricmp(id, s_CodecId_VFW) == 0)
        __noop;
    else
        return 0;  //can't create a stream from this track

    //TODO: vet settings, etc

    //At least one cluster should have been loaded when we opened
    //the file.  We search the first cluster for a frame having
    //this track number, and use that to start from.  If no frame
    //with this track number is present in first cluster, then
    //we assume (correctly or incorrectly) that this file doesn't
    //have any frames from that track at all.

    VideoStream* const s = new (std::nothrow) VideoStream(pTrack);
    assert(s);  //TODO

    return s;
}


VideoStream::VideoStream(VideoTrack* pTrack) : Stream(pTrack)
{
}


std::wostream& VideoStream::GetKind(std::wostream& os) const
{
    return os << L"Video";
}


void VideoStream::GetMediaTypes(CMediaTypes& mtv) const
{
    mtv.Clear();

    const char* const id = m_pTrack->GetCodecId();
    assert(id);

    if ((_stricmp(id, s_CodecId_VP8) == 0) ||
        (_stricmp(id, s_CodecId_ON2VP8) == 0))
    {
        GetVp8MediaTypes(mtv);
    }
    else if (_stricmp(id, s_CodecId_VFW) == 0)
        GetVfwMediaTypes(mtv);
}


void VideoStream::GetVp8MediaTypes(CMediaTypes& mtv) const
{
    AM_MEDIA_TYPE mt;

    VIDEOINFOHEADER vih;
    BITMAPINFOHEADER& bmih = vih.bmiHeader;

    mt.majortype = MEDIATYPE_Video;
    mt.subtype = WebmTypes::MEDIASUBTYPE_VP80;
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = TRUE;
    mt.lSampleSize = 0;
    mt.formattype = FORMAT_VideoInfo;
    mt.pUnk = 0;
    mt.cbFormat = sizeof vih;
    mt.pbFormat = (BYTE*)&vih;

    SetRectEmpty(&vih.rcSource);  //TODO
    SetRectEmpty(&vih.rcTarget);
    vih.dwBitRate = 0;
    vih.dwBitErrorRate = 0;

    const VideoTrack* const pTrack = static_cast<VideoTrack*>(m_pTrack);

    const double r = pTrack->GetFrameRate();

    if (r <= 0)
        vih.AvgTimePerFrame = 0;
    else
    {
        const double tt = 10000000 / r;  //[ticks/sec] / [frames/sec]
        vih.AvgTimePerFrame = static_cast<__int64>(tt);
    }

    const __int64 w = pTrack->GetWidth();
    assert(w > 0);
    assert(w <= LONG_MAX);

    const __int64 h = pTrack->GetHeight();
    assert(h > 0);
    assert(h <= LONG_MAX);

    bmih.biSize = sizeof bmih;
    bmih.biWidth = static_cast<LONG>(w);
    bmih.biHeight = static_cast<LONG>(h);
    bmih.biPlanes = 1;
    bmih.biBitCount = 0;
    bmih.biCompression = mt.subtype.Data1;
    bmih.biSizeImage = 0;
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;

    mtv.Add(mt);
}


void VideoStream::GetVfwMediaTypes(CMediaTypes& mtv) const
{
    const bytes_t& cp = m_pTrack->GetCodecPrivate();
    const ULONG cp_size = static_cast<ULONG>(cp.size());
    cp_size;
    assert(cp_size >= sizeof(BITMAPINFOHEADER));

    AM_MEDIA_TYPE mt;

    VIDEOINFOHEADER vih;
    BITMAPINFOHEADER& bmih = vih.bmiHeader;

    memcpy(&bmih, &cp[0], sizeof bmih);
    assert(bmih.biSize >= sizeof(BITMAPINFOHEADER));

    mt.majortype = MEDIATYPE_Video;
    mt.subtype = GraphUtil::FourCCGUID(bmih.biCompression);
    mt.bFixedSizeSamples = FALSE;
    mt.bTemporalCompression = TRUE;
    mt.lSampleSize = 0;
    mt.formattype = FORMAT_VideoInfo;
    mt.pUnk = 0;
    mt.cbFormat = sizeof vih;
    mt.pbFormat = (BYTE*)&vih;

    SetRectEmpty(&vih.rcSource);  //TODO
    SetRectEmpty(&vih.rcTarget);
    vih.dwBitRate = 0;
    vih.dwBitErrorRate = 0;

    const VideoTrack* const pTrack = static_cast<VideoTrack*>(m_pTrack);

    const double r = pTrack->GetFrameRate();

    if (r <= 0)
        vih.AvgTimePerFrame = 0;
    else
    {
        const double tt = 10000000 / r;  //[ticks/sec] / [frames/sec]
        vih.AvgTimePerFrame = static_cast<__int64>(tt);
    }

    const __int64 w = pTrack->GetWidth();
    w;
    assert(w > 0);
    assert(w <= LONG_MAX);
    assert(w == bmih.biWidth);

    const __int64 h = pTrack->GetHeight();
    h;
    assert(h > 0);
    assert(h <= LONG_MAX);
    assert(h == bmih.biHeight);

    mtv.Add(mt);
}


HRESULT VideoStream::QueryAccept(const AM_MEDIA_TYPE* pmt) const
{
    if (pmt == 0)
        return E_INVALIDARG;

    const AM_MEDIA_TYPE& mt = *pmt;

    if (mt.majortype != MEDIATYPE_Video)
        return S_FALSE;

    const char* const id = m_pTrack->GetCodecId();
    assert(id);

    if ((_stricmp(id, s_CodecId_VP8) == 0) ||
        (_stricmp(id, s_CodecId_ON2VP8) == 0))
    {
        if (mt.subtype != WebmTypes::MEDIASUBTYPE_VP80)
            return S_FALSE;

        //TODO: more vetting here

        return S_OK;
    }

    if (_stricmp(id, s_CodecId_VFW) == 0)
    {
        if (!GraphUtil::FourCCGUID::IsFourCC(mt.subtype))
            return S_FALSE;

        const bytes_t& cp = m_pTrack->GetCodecPrivate();

        if (cp.size() < sizeof(BITMAPINFOHEADER))
            return S_FALSE;

        BITMAPINFOHEADER bmih;

        memcpy(&bmih, &cp[0], sizeof bmih);

        if (bmih.biSize < sizeof bmih)
            return S_FALSE;

        //TODO: more vetting here

        const DWORD fcc = mt.subtype.Data1;  //"VP80"

        if (fcc != bmih.biCompression)
            return S_FALSE;

        //NOTE: technically we don't need this check,
        //but for now WebM only officially supports VP80.
        //
        if (mt.subtype != WebmTypes::MEDIASUBTYPE_VP80)
            return S_FALSE;

        return S_OK;
    }

    return S_FALSE;
}


HRESULT VideoStream::UpdateAllocatorProperties(
    ALLOCATOR_PROPERTIES& props) const
{
    if (props.cBuffers <= 0)
        props.cBuffers = 1;

    const long size = GetBufferSize();

    if (props.cbBuffer < size)
        props.cbBuffer = size;

    if (props.cbAlign <= 0)
        props.cbAlign = 1;

    if (props.cbPrefix < 0)
        props.cbPrefix = 0;

    return S_OK;
}


long VideoStream::GetBufferSize() const
{
    const VideoTrack* const pTrack = static_cast<VideoTrack*>(m_pTrack);

    const __int64 w = pTrack->GetWidth();
    const __int64 h = pTrack->GetHeight();

    //TODO: we can do better here.  VPx is based on YV12, which would waste
    //less memory than assuming RGB32.
    const __int64 size_ = w * h * 4;  //RGB32 (worst case)
    assert(size_ <= LONG_MAX);

    const long size = static_cast<LONG>(size_);

    return size;
}


HRESULT VideoStream::OnPopulateSample(
    const BlockEntry* pNextEntry,
    IMediaSample* pSample)
{
    assert(pSample);
    assert(m_pBase);
    assert(!m_pBase->EOS());
    //assert(m_baseTime >= 0);
    //assert((m_baseTime % 100) == 0);
    assert(m_pCurr);
    assert(m_pCurr != m_pStop);
    assert(!m_pCurr->EOS());
    assert((pNextEntry == 0) ||
           pNextEntry->EOS() ||
           !pNextEntry->IsBFrame());

    const Block* const pCurrBlock = m_pCurr->GetBlock();
    assert(pCurrBlock);
    assert(pCurrBlock->GetNumber() == m_pTrack->GetNumber());

    Cluster* const pCurrCluster = m_pCurr->GetCluster();
    assert(pCurrCluster);

    assert((m_pStop == 0) ||
           m_pStop->EOS() ||
           (m_pStop->GetBlock()->GetTimeCode(m_pStop->GetCluster()) >
             pCurrBlock->GetTimeCode(pCurrCluster)));

#if 0
    const __int64 basetime_ns = m_pBase->GetTime();
    assert(basetime_ns >= 0);
    assert((basetime_ns % 100) == 0);
#else
    const __int64 basetime_ns = m_pBase->GetFirstTime();
    assert(basetime_ns >= 0);
    assert((basetime_ns % 100) == 0);
#endif

    const LONG srcsize = pCurrBlock->GetSize();
    assert(srcsize >= 0);

    const long tgtsize = pSample->GetSize();
    tgtsize;
    assert(tgtsize >= 0);
    assert(tgtsize >= srcsize);

    BYTE* ptr;

    HRESULT hr = pSample->GetPointer(&ptr);  //read srcsize bytes
    assert(SUCCEEDED(hr));
    assert(ptr);

    IMkvFile* const pFile = pCurrCluster->m_pSegment->m_pFile;

    hr = pCurrBlock->Read(pFile, ptr);
    assert(hr == S_OK);  //all bytes were read

    hr = pSample->SetActualDataLength(srcsize);

    hr = pSample->SetPreroll(0);
    assert(SUCCEEDED(hr));

    hr = pSample->SetMediaType(0);
    assert(SUCCEEDED(hr));

    hr = pSample->SetDiscontinuity(m_bDiscontinuity);
    assert(SUCCEEDED(hr));

    //set by caller:
    //m_bDiscontinuity = false;

    hr = pSample->SetMediaTime(0, 0);
    assert(SUCCEEDED(hr));

    const bool bKey = pCurrBlock->IsKey();

    hr = pSample->SetSyncPoint(bKey ? TRUE : FALSE);
    assert(SUCCEEDED(hr));

    //TODO: is there a better way to make this test?
    //We could genericize this, e.g. BlockEntry::HasTime().
    if (m_pCurr->IsBFrame())
    {
        assert(!bKey);

        hr = pSample->SetTime(0, 0);
        assert(SUCCEEDED(hr));
    }
    else
    {
        const __int64 start_ns = pCurrBlock->GetTime(pCurrCluster);
        assert(start_ns >= basetime_ns);
        assert((start_ns % 100) == 0);

        __int64 ns = start_ns - basetime_ns;
        __int64 start_reftime = ns / 100;

        __int64 stop_reftime;
        __int64* pstop_reftime;

        if ((pNextEntry == 0) || pNextEntry->EOS())
            pstop_reftime = 0;  //TODO: use duration of curr block
        else
        {
            const Block* const pNextBlock = pNextEntry->GetBlock();
            assert(pNextBlock);

            Cluster* const pNextCluster = pNextEntry->GetCluster();

            const __int64 stop_ns = pNextBlock->GetTime(pNextCluster);
            assert(stop_ns > start_ns);
            assert((stop_ns % 100) == 0);

            ns = stop_ns - basetime_ns;
            stop_reftime = ns / 100;
            assert(stop_reftime > start_reftime);

            pstop_reftime = &stop_reftime;
        }

        hr = pSample->SetTime(&start_reftime, pstop_reftime);
        assert(SUCCEEDED(hr));
    }

    //m_pCurr = pNextBlock;
    return S_OK;
}




}  //end namespace MkvParser
