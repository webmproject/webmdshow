// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include "mkvparserstream.h"
#include "mkvparser.hpp"
#include "mkvparserstreamreader.h"
#include <cassert>
#include <sstream>
#include <iomanip>
#include <vfwmsgs.h>
#ifdef _DEBUG
#include "odbgstream.h"
using std::endl;
#endif

namespace mkvparser
{


Stream::Stream(const Track* pTrack) :
    m_pTrack(pTrack),
    m_pLocked(0)
{
    Init();
}


Stream::~Stream()
{
    SetCurr(0);
}


void Stream::Init()
{
    m_base_time_ns = -1;
    //m_pBase = 0;
    SetCurr(0);  //lazy init this later
    m_pStop = m_pTrack->GetEOS();  //means play entire stream
    m_bDiscontinuity = true;
}


void Stream::Stop()
{
    IMkvReader* const pReader_ = m_pTrack->m_pSegment->m_pReader;

    using mkvparser::IStreamReader;
    IStreamReader* const pReader = static_cast<IStreamReader*>(pReader_);

    pReader->UnlockPages(m_pLocked);
    m_pLocked = 0;
}


HRESULT Stream::SetCurr(const mkvparser::BlockEntry* pNext)
{
    IMkvReader* const pReader_ = m_pTrack->m_pSegment->m_pReader;

    using mkvparser::IStreamReader;
    IStreamReader* const pReader = static_cast<IStreamReader*>(pReader_);

    m_pCurr = pNext;

    pReader->UnlockPages(m_pLocked);
    m_pLocked = m_pCurr;

    const HRESULT hr = pReader->LockPages(m_pLocked);
    assert(SUCCEEDED(hr));
    return hr;
}


std::wstring Stream::GetId() const
{
    std::wostringstream os;

    GetKind(os);  //"Video" or "Audio"

    os << std::setw(3) << std::setfill(L'0') << m_pTrack->GetNumber();

    return os.str();
}


std::wstring Stream::GetName() const
{
    const Track* const t = m_pTrack;
    assert(t);

    if (const char* codecName = t->GetCodecNameAsUTF8())
        return ConvertFromUTF8(codecName);

    if (const char* name = t->GetNameAsUTF8())
        return ConvertFromUTF8(name);

    if (LONGLONG tn = t->GetNumber())
    {
        std::wostringstream os;
        os << L"Track" << tn;
        return os.str();
    }

    if (const char* codecId = t->GetCodecId())
    {
        std::wstring result;

        const char* p = codecId;

        while (*p)
            result += wchar_t(*p++);  //TODO: is this cast meaningful?

        return result;
    }

    return GetId();
}


#if 0
__int64 Stream::GetDuration() const
{
    Segment* const pSegment = m_pTrack->m_pSegment;
    assert(pSegment);

    const __int64 ns = pSegment->GetDuration();  //scaled (nanoseconds)
    assert(ns >= 0);

    const __int64 d = ns / 100;  //100-ns ticks

    return d;
}
#endif


#if 0
HRESULT Stream::GetAvailable(LONGLONG* pLatest) const
{
    if (pLatest == 0)
        return E_POINTER;

    LONGLONG& pos = *pLatest;  //units are current time format

    Segment* const pSegment = m_pTrack->m_pSegment;

    if (pSegment->Unparsed() <= 0)
        pos = GetDuration();
    else
    {
        const Cluster* const pCluster = pSegment->GetLast();

        if ((pCluster == 0) || pCluster->EOS())
            pos = 0;
        else
        {
            const __int64 ns = pCluster->GetTime();
            pos = ns / 100;  //TODO: reftime units are assumed here
        }
    }

    return S_OK;
}
#endif


//__int64 Stream::GetCurrPosition() const
//{
//    return GetCurrTime();  //TODO: for now only support reftime units
//}


__int64 Stream::GetCurrTime() const
{
    if (m_pCurr == 0)  //NULL means lazy init hasn't happened yet
        return 0;      //TODO: assumes track starts with t=0

    if (m_pCurr->EOS())
        return -1;

    const Block* const pBlock = m_pCurr->GetBlock();
    assert(pBlock);

    const __int64 ns = pBlock->GetTime(m_pCurr->GetCluster());
    assert(ns >= 0);

    const __int64 reftime = ns / 100;  //100-ns ticks

    return reftime;
}


//__int64 Stream::GetStopPosition() const
//{
//    return GetStopTime();  //TODO: for now we only support reftime units
//}


__int64 Stream::GetStopTime() const
{
    if (m_pStop == 0)  //interpreted to mean "play to end of stream"
        return -1;

    if (m_pStop->EOS())
        return -1;

    const Block* const pBlock = m_pStop->GetBlock();
    assert(pBlock);

    const __int64 ns = pBlock->GetTime(m_pStop->GetCluster());
    assert(ns >= 0);

    const __int64 reftime = ns / 100;  //100-ns ticks

    return reftime;
}


LONGLONG Stream::GetSeekTime(
    LONGLONG currpos_reftime,
    DWORD dwCurr_) const
{
    const DWORD dwCurrPos = dwCurr_ & AM_SEEKING_PositioningBitsMask;
    assert(dwCurrPos != AM_SEEKING_NoPositioning);  //handled by caller

    Segment* const pSegment = m_pTrack->m_pSegment;

    const __int64 currpos_ns = currpos_reftime * 100;
    //__int64 tCurr_ns;

    switch (dwCurrPos)
    {
        case AM_SEEKING_IncrementalPositioning:  //applies only to stop pos
        default:
            assert(false);
            return 0;

        case AM_SEEKING_AbsolutePositioning:
            return currpos_ns;

        case AM_SEEKING_RelativePositioning:
        {
            if (m_pCurr == 0)  //lazy init
                return currpos_ns;  //t=0 is assumed here

            else if (m_pCurr->EOS())
            {
                const __int64 duration_ns = pSegment->GetDuration();

                if (duration_ns >= 0)  //actually have a duration
                    return duration_ns + currpos_ns;

                return 0;  //TODO: is there a better value we can return here?
            }
            else
            {
                const Block* const pBlock = m_pCurr->GetBlock();
                assert(pBlock);

                return pBlock->GetTime(m_pCurr->GetCluster()) + currpos_ns;
            }
        }
    }
}


void Stream::SetCurrPosition(
    //const Cluster* pBase,
    LONGLONG base_time_ns,
    const BlockEntry* pCurr)
{
    //m_pBase = pBase;
    SetCurr(pCurr);
    m_base_time_ns = base_time_ns;
    m_bDiscontinuity = true;
}


void Stream::SetStopPosition(
    LONGLONG stoppos_reftime,
    DWORD dwStop_)
{
    const DWORD dwStopPos = dwStop_ & AM_SEEKING_PositioningBitsMask;
    assert(dwStopPos != AM_SEEKING_NoPositioning);  //handled by caller

    Segment* const pSegment = m_pTrack->m_pSegment;

    if (pSegment->GetCount() == 0)
    {
        m_pStop = m_pTrack->GetEOS();  //means "play to end"
        return;
    }

    if ((m_pCurr != 0) && m_pCurr->EOS())
    {
        m_pStop = m_pTrack->GetEOS();
        return;
    }

    __int64 tCurr_ns;

    if (m_pCurr == 0)  //lazy init
        tCurr_ns = 0;  //nanoseconds
    else
    {
        const Block* const pBlock = m_pCurr->GetBlock();

        tCurr_ns = pBlock->GetTime(m_pCurr->GetCluster());
        assert(tCurr_ns >= 0);
    }

    //const Cluster* const pFirst = pSegment->GetFirst();
    //const Cluster* const pCurrCluster = m_pBase ? m_pBase : pFirst;
    //pCurrCluster;
    //assert(pCurrCluster);
    //assert(!pCurrCluster->EOS());
    //assert(tCurr_ns >= pCurrCluster->GetTime());

    //const __int64 duration_ns = pSegment->GetDuration();
    //assert(duration_ns >= 0);

    const __int64 stoppos_ns = stoppos_reftime * 100;
    __int64 tStop_ns;

    switch (dwStopPos)
    {
        default:
            assert(false);
            return;

        case AM_SEEKING_AbsolutePositioning:
        {
            tStop_ns = stoppos_reftime;
            break;
        }
        case AM_SEEKING_RelativePositioning:
        {
            if ((m_pStop == 0) || m_pStop->EOS())
            {
                const __int64 duration_ns = pSegment->GetDuration();

                if (duration_ns <= 0)  //don't have a duration
                {
                    m_pStop = m_pTrack->GetEOS();  //means "play to end"
                    return;
                }

                tStop_ns = duration_ns + stoppos_ns;
            }
            else
            {
                const Block* const pBlock = m_pStop->GetBlock();
                assert(pBlock);

                tStop_ns = pBlock->GetTime(m_pStop->GetCluster()) + stoppos_ns;
            }

            break;
        }
        case AM_SEEKING_IncrementalPositioning:
        {
            if (stoppos_reftime <= 0)
            {
                m_pStop = m_pCurr;
                return;
            }

            tStop_ns = tCurr_ns + stoppos_ns;
            break;
        }
    }

    if (tStop_ns <= tCurr_ns)
    {
        m_pStop = m_pCurr;
        return;
    }

    //if (tStop_ns >= duration_ns)
    //{
    //    m_pStop = m_pTrack->GetEOS();
    //    return;
    //}

    //TODO: here we find a stop block whose time is aligned with
    //a cluster time.  We should really do better here, and find
    //the exact block that corresponds to the requested time.

    const Cluster* pStopCluster = pSegment->FindCluster(tStop_ns);
    assert(pStopCluster);

    if (pStopCluster == m_pCurr->GetCluster())
        pStopCluster = pSegment->GetNext(pStopCluster);

    m_pStop = pStopCluster->GetEntry(m_pTrack);
    assert((m_pStop == 0) ||
           m_pStop->EOS() ||
           (m_pStop->GetBlock()->GetTime(m_pStop->GetCluster()) >= tCurr_ns));
}


void Stream::SetStopPositionEOS()
{
    m_pStop = m_pTrack->GetEOS();
}


#if 0
HRESULT Stream::Preload()
{
    Segment* const pSegment = m_pTrack->m_pSegment;

    const long status = pSegment->LoadCluster();

    if (status < 0)  //error
        return E_FAIL;

    return S_OK;
}
#endif


HRESULT Stream::InitCurr()
{
    if (m_pCurr)
        return S_OK;

    //lazy-init of first block

    Segment* const pSegment = m_pTrack->m_pSegment;

    if (pSegment->GetCount() <= 0)
        return VFW_E_BUFFER_UNDERFLOW;

    const mkvparser::BlockEntry* pCurr;

    const long status = m_pTrack->GetFirst(pCurr);

    if (status == E_BUFFER_NOT_FULL)
        return VFW_E_BUFFER_UNDERFLOW;

    assert(status >= 0);  //success
    assert(pCurr);
    assert(pCurr->EOS() ||
           (m_pTrack->GetType() == 2) ||
           pCurr->GetBlock()->IsKey());

    SetCurr(pCurr);

    const Cluster* const pBase = pSegment->GetFirst();
    assert(pBase);
    assert(!pBase->EOS());

    m_base_time_ns = pBase->GetFirstTime();
    //assert(m_base_time_ns >= 0);

#ifdef _DEBUG
    if (!m_pCurr->EOS())
    {
        const Block* const pBlock = m_pCurr->GetBlock();

        const LONGLONG time_ns = pBlock->GetTime(m_pCurr->GetCluster());

        const LONGLONG dt_ns = time_ns - m_base_time_ns;
        assert(dt_ns >= 0);

        const double dt_sec = double(dt_ns) / 1000000000;
        assert(dt_sec >= 0);
    }
#endif

    return S_OK;
}


HRESULT Stream::UpdateAllocatorProperties(
    ALLOCATOR_PROPERTIES& props) const
{
    const long cBuffers = GetBufferCount();

    if (props.cBuffers <= cBuffers)  //to handle laced frames
        props.cBuffers = cBuffers;

    const long cbBuffer = GetBufferSize();

    if (props.cbBuffer < cbBuffer)
        props.cbBuffer = cbBuffer;

    if (props.cbAlign <= 0)
        props.cbAlign = 1;

    if (props.cbPrefix < 0)
        props.cbPrefix = 0;

    return S_OK;
}


void Stream::Clear(samples_t& samples)
{
    while (!samples.empty())
    {
        IMediaSample* const p = samples.back();
        assert(p);

        samples.pop_back();

        p->Release();
    }
}


HRESULT Stream::GetSampleCount(long& count)
{
    count = 0;

    HRESULT hr = InitCurr();

    if (FAILED(hr))
        return hr;

    if (m_pStop == 0)  //TODO: this test might not be req'd
    {
        if (m_pCurr->EOS())
            return S_FALSE;  //send EOS downstream
    }
    else if (m_pCurr == m_pStop)
    {
        return S_FALSE;  //EOS
    }

    const Block* const pCurrBlock = m_pCurr->GetBlock();
    assert(pCurrBlock);
    assert(pCurrBlock->GetTrackNumber() == m_pTrack->GetNumber());

    count = pCurrBlock->GetFrameCount();
    assert(count <= GetBufferCount());

    return S_OK;
}


HRESULT Stream::PopulateSamples(const samples_t& samples)
{
    //if (SendPreroll(pSample))
    //    return S_OK;

    HRESULT hr = InitCurr();

    if (FAILED(hr))
        return hr;

    if (m_pStop == 0)  //TODO: this test might not be req'd
    {
        if (m_pCurr->EOS())
            return S_FALSE;  //send EOS downstream
    }
    else if (m_pCurr == m_pStop)
    {
        return S_FALSE;  //EOS
    }

    assert(!m_pCurr->EOS());

    const BlockEntry* pNext;
    const long status = m_pTrack->GetNext(m_pCurr, pNext);

    if (status == E_BUFFER_NOT_FULL)
        return VFW_E_BUFFER_UNDERFLOW;

    assert(status >= 0);  //success
    assert(pNext);

    const Block* const pCurrBlock = m_pCurr->GetBlock();

    const Cluster* const pCurrCluster = m_pCurr->GetCluster();
    assert(pCurrCluster);

    const __int64 start_ns = pCurrBlock->GetTime(pCurrCluster);

    if (start_ns < 0)
    {
        SetCurr(pNext);  //throw curr block away
        return 2;  //no samples, but not EOS either
    }

    const LONGLONG base_ns = m_base_time_ns;
    //assert(base_ns >= 0);

    if (start_ns < base_ns)
    {
        SetCurr(pNext);  //throw curr block away
        return 2;  //no samples, but not EOS either
    }

    const int nFrames = pCurrBlock->GetFrameCount();

    if (nFrames <= 0)   //should never happen
    {
        SetCurr(pNext);  //throw curr block away
        return 2;  //no samples, but not EOS either
    }

    if (samples.size() != samples_t::size_type(nFrames))
        return 2;   //try again

    OnPopulateSample(pNext, samples);

    hr = SetCurr(pNext);
    m_bDiscontinuity = false;

    return hr;
}


//bool Stream::SendPreroll(IMediaSample*)
//{
//    return false;
//}


ULONG Stream::GetClusterCount() const
{
    return m_pTrack->m_pSegment->GetCount();
}


HRESULT Stream::SetConnectionMediaType(const AM_MEDIA_TYPE&)
{
    return S_OK;
}


std::wstring Stream::ConvertFromUTF8(const char* str)
{
    const int cch = MultiByteToWideChar(
                        CP_UTF8,
                        0,  //TODO: MB_ERR_INVALID_CHARS
                        str,
                        -1,  //include NUL terminator in result
                        0,
                        0);  //request length

    assert(cch > 0);

    const size_t cb = cch * sizeof(wchar_t);
    wchar_t* const wstr = (wchar_t*)_malloca(cb);

    const int cch2 = MultiByteToWideChar(
                        CP_UTF8,
                        0,  //TODO: MB_ERR_INVALID_CHARS
                        str,
                        -1,
                        wstr,
                        cch);

    cch2;
    assert(cch2 > 0);
    assert(cch2 == cch);

    return wstr;
}


}  //end namespace mkvparser
