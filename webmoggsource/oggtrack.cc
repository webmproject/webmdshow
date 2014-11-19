#include <strmif.h>
#include "oggtrack.h"
#include "oggparser.h"
#include <sstream>
#include <iomanip>
#include <cassert>

namespace WebmOggSource
{

OggTrack::OggTrack(
    oggparser::OggStream* pStream,
    ULONG id) :
    m_pStream(pStream),
    m_id(id)
{
    //Init();
}


OggTrack::~OggTrack()
{
    //SetCurr(0);
}


void OggTrack::Reset()
{
    //m_base_time_ns = -1;
    //SetCurr(0);  //lazy init this later
    //m_pStop = m_pTrack->GetEOS();  //means play entire stream
    m_bDiscontinuity = true;
    m_pStream->Reset();
    OnReset();
}


std::wstring OggTrack::GetId() const
{
    std::wostringstream os;

    GetKind(os);  //"Video" or "Audio"

    os << std::setw(3) << std::setfill(L'0') << m_id;

    return os.str();
}


std::wstring OggTrack::GetName() const
{
    return GetCodecName();
}


#if 0

HRESULT OggTrack::InitCurr()
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

#endif


void OggTrack::Clear(samples_t& samples)
{
    while (!samples.empty())
    {
        IMediaSample* const p = samples.back();
        assert(p);

        samples.pop_back();

        p->Release();
    }
}


#if 0  //TODO

HRESULT OggTrack::GetSampleCount(long& count)
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

    return S_OK;
}

#endif  //TODO


#if 0  //TODO

HRESULT OggTrack::PopulateSamples(const samples_t& samples)
{
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
    assert(start_ns >= 0);

    const LONGLONG base_ns = m_base_time_ns;
    assert(base_ns >= 0);

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

    SetCurr(pNext);
    m_bDiscontinuity = false;

    return S_OK;
}

#endif  //TODO


}  //end namespace WebmOggSource
