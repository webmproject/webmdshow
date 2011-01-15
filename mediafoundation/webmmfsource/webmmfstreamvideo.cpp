#include "webmmfsource.hpp"
#include "webmmfstream.hpp"
#include "webmmfstreamvideo.hpp"
#include "webmtypes.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <cassert>
#include <limits>
#include <cmath>
#include <comdef.h>
#include <vfwmsgs.h>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
#endif

_COM_SMARTPTR_TYPEDEF(IMFStreamDescriptor, __uuidof(IMFStreamDescriptor));
_COM_SMARTPTR_TYPEDEF(IMFMediaType, __uuidof(IMFMediaType));
_COM_SMARTPTR_TYPEDEF(IMFMediaTypeHandler, __uuidof(IMFMediaTypeHandler));
_COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, __uuidof(IMFMediaBuffer));
_COM_SMARTPTR_TYPEDEF(IMFSample, __uuidof(IMFSample));

namespace WebmMfSourceLib
{

HRESULT WebmMfStreamVideo::CreateStreamDescriptor(
    const mkvparser::Track* pTrack_,
    IMFStreamDescriptor*& pDesc)
{
    assert(pTrack_);
    assert(pTrack_->GetType() == 1);

    typedef mkvparser::VideoTrack VT;
    const VT* const pTrack = static_cast<const VT*>(pTrack_);

    const char* const codec = pTrack->GetCodecId();
    assert(codec);

    if (_stricmp(codec, "V_VP8") == 0)
        __noop;
    //else if (_stricmp(codec, "V_ON2VP8") == 0)  //legacy
    //    __noop;
    else  //weird
    {
        pDesc = 0;
        return E_FAIL;
    }

    const LONGLONG id_ = pTrack->GetNumber();
    const DWORD id = static_cast<DWORD>(id_);

    IMFMediaTypePtr pmt;

    HRESULT hr = MFCreateMediaType(&pmt);
    assert(SUCCEEDED(hr));
    assert(pmt);

    hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    assert(SUCCEEDED(hr));

    hr = pmt->SetGUID(MF_MT_SUBTYPE, WebmTypes::MEDIASUBTYPE_VP80);
    assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, FALSE);
    assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_COMPRESSED, TRUE);
    assert(SUCCEEDED(hr));

    UINT32 numer, denom;

    hr = GetFrameRate(pTrack, numer, denom);

    if (hr == S_OK)
    {
        hr = MFSetAttributeRatio(pmt, MF_MT_FRAME_RATE, numer, denom);
        assert(SUCCEEDED(hr));
    }

    const __int64 ww = pTrack->GetWidth();
    assert(ww > 0);

    const UINT32 w = static_cast<UINT32>(ww);

    const __int64 hh = pTrack->GetHeight();
    assert(hh > 0);

    const UINT32 h = static_cast<UINT32>(hh);

    hr = MFSetAttributeSize(pmt, MF_MT_FRAME_SIZE, w, h);
    assert(SUCCEEDED(hr));

    //http://msdn.microsoft.com/en-us/library/bb530115%28v=VS.85%29.aspx
    //http://msdn.microsoft.com/en-us/library/ms704767%28v=VS.85%29.aspx

    hr = MFSetAttributeRatio(pmt, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    assert(SUCCEEDED(hr));

    IMFMediaType* mtv[1] = { pmt };

    hr = MFCreateStreamDescriptor(id, 1, mtv, &pDesc);
    assert(SUCCEEDED(hr));
    assert(pDesc);

    IMFMediaTypeHandlerPtr ph;

    hr = pDesc->GetMediaTypeHandler(&ph);
    assert(SUCCEEDED(hr));
    assert(ph);

    hr = ph->SetCurrentMediaType(pmt);
    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfStreamVideo::GetFrameRate(
    const mkvparser::VideoTrack* pTrack,
    UINT32& numer,
    UINT32& denom)
{
    mkvparser::Segment* const pSegment = pTrack->m_pSegment;

    double frame_rate = pTrack->GetFrameRate();

    if ((frame_rate <= 0) || (frame_rate > std::numeric_limits<UINT32>::max()))
    {
        using namespace mkvparser;

        long result = pSegment->LoadCluster();

        const BlockEntry* pFirst;
        result = pTrack->GetFirst(pFirst);

        if ((pFirst == 0) || pFirst->EOS())
            return E_FAIL;

        const LONGLONG t0 = pFirst->GetBlock()->GetTime(pFirst->GetCluster());

        int count = 0;
        const BlockEntry* pLast = pFirst;

        while (count < 10)
        {
            const BlockEntry* const pCurr = pLast;
            const BlockEntry* pNext;

            result = pTrack->GetNext(pCurr, pNext);

            if (result == 0)  //success
            {
                ++count;
                pLast = pNext;

                continue;
            }

            if (result != mkvparser::E_BUFFER_NOT_FULL)
                break;

            result = pSegment->LoadCluster();
        }

        if (count <= 0)
            return E_FAIL;

        const LONGLONG t1 = pLast->GetBlock()->GetTime(pLast->GetCluster());

        if (t1 <= t0)
            return E_FAIL;

        const LONGLONG ns = t1 - t0;
        assert(ns > 0);

        frame_rate = (double(count) * 1000000000) / double(ns);
    }

    denom = 1;

    for (;;)
    {
        const double r = frame_rate * denom;

        double int_part;
        const double frac_part = modf(r, &int_part);

        //I think the 0 test is valid (because 0 is a model number), but
        //if not, then you can cast it to a integer and compare that way.
        //
        //http://www.cygnus-software.com/papers/comparingfloats/
        //  Comparing%20floating%20point%20numbers.htm

        if ((frac_part == 0) ||
            (denom == 1000000000) ||
            ((10 * int_part) > std::numeric_limits<UINT32>::max()))
        {
            numer = static_cast<UINT32>(int_part);
            break;
        }

        denom *= 10;
    }

    return S_OK;
}



HRESULT WebmMfStreamVideo::CreateStream(
    IMFStreamDescriptor* pSD,
    WebmMfSource* pSource,
    const mkvparser::Track* pTrack_,
    WebmMfStream*& pStream)
{
    assert(pTrack_);
    assert(pTrack_->GetType() == 1);

    typedef mkvparser::VideoTrack VT;
    const VT* const pTrack = static_cast<const VT*>(pTrack_);

    pStream = new (std::nothrow) WebmMfStreamVideo(pSource, pSD, pTrack);
    assert(pStream);  //TODO

    //TODO: handle time

    return pStream ? S_OK : E_OUTOFMEMORY;
}


WebmMfStreamVideo::WebmMfStreamVideo(
    WebmMfSource* pSource,
    IMFStreamDescriptor* pDesc,
    const mkvparser::VideoTrack* pTrack) :
    WebmMfStream(pSource, pDesc, pTrack),
    m_rate(1),
    m_thin_ns(-2)
{
    m_curr.pBE = 0;
    m_curr.pCP = 0;
    m_curr.pTP = 0;

    m_pLocked = 0;
}


WebmMfStreamVideo::~WebmMfStreamVideo()
{
}


#if 0
void WebmMfStreamVideo::SetCurrBlock(const mkvparser::BlockEntry* pCurr)
{
    m_curr.pBE = pCurr;
    m_curr.pCP = 0;
    m_curr.pTP = 0;
}
#endif


HRESULT WebmMfStreamVideo::Seek(
    const PROPVARIANT& var,
    const SeekInfo& info,
    bool bStart)
{
    assert(m_pLocked == 0);

    m_bDiscontinuity = true;
    m_curr = info;
    m_pNextBlock = 0;  //means "we don't know yet"

    assert(var.vt == VT_I8);

    const LONGLONG reftime = var.hVal.QuadPart;
    assert(reftime >= 0);

    const mkvparser::BlockEntry* const pBE = info.pBE;

    if ((pBE != 0) && !pBE->EOS())
    {
        const mkvparser::Block* const pB = pBE->GetBlock();
        assert(pB);
        assert(pB->IsKey());

        const mkvparser::Cluster* const pCluster = pBE->GetCluster();

        const LONGLONG time_ns = pB->GetTime(pCluster);
        assert(time_ns >= 0);

        if (m_thin_ns >= -1)  //rate change has been requested
            m_thin_ns = time_ns;
    }

    if (bStart)
        return OnStart(var);
    else
        return OnSeek(var);
}


void WebmMfStreamVideo::GetSeekInfo(LONGLONG time_ns, SeekInfo& i) const
{
#if 0  //TODO: RESTORE THIS
    mkvparser::Segment* const pSegment = m_pTrack->m_pSegment;

    if (const mkvparser::Cues* pCues = pSegment->GetCues())
    {
        const bool bFound = pCues->Find(time_ns, m_pTrack, i.pCP, i.pTP);

        if (bFound)
        {
            i.pBE = pCues->GetBlock(i.pCP, i.pTP);

            if ((i.pBE != 0) && !i.pBE->EOS())
                return;
        }
    }
#endif

    i.pCP = 0;
    i.pTP = 0;

    //TODO: this isn't good enough, because it returns the
    //last keyframe on the cluster.  We need to seek within
    //a cluster; e.g. the lions clip.
    //
    //TODO: it's also not good enough for another reason:
    //we find the closest cluster (loaded, not preloaded),
    //but I don't think we have a guarantee that that
    //cluster, or the previous clusters we search, are
    //loaded in the cache.
    const long status = m_pTrack->Seek(time_ns, i.pBE);
    assert(status >= 0);
}


bool WebmMfStreamVideo::IsEOS() const
{
    const mkvparser::BlockEntry* const pCurr = m_curr.pBE;
    return ((pCurr == 0) || pCurr->EOS());
}


HRESULT WebmMfStreamVideo::GetSample(IUnknown* pToken)
{
    const mkvparser::BlockEntry* const pCurr = m_curr.pBE;
    assert(pCurr);
    assert(!pCurr->EOS());
    assert(m_pLocked);
    assert(m_pLocked == pCurr);

    const mkvparser::Block* const pCurrBlock = pCurr->GetBlock();
    assert(pCurrBlock);
    assert(pCurrBlock->GetTrackNumber() == m_pTrack->GetNumber());

    const mkvparser::Cluster* const pCurrCluster = pCurr->GetCluster();
    assert(pCurrCluster);

    SeekInfo next;

    if (m_thin_ns < 0)  //no thinning
    {
        if (m_pNextBlock == 0)
        {
            const HRESULT hr = GetNextBlock(pCurr);

            if (FAILED(hr))  //no next entry found on current cluster
                return hr;
        }

        next.pBE = m_pNextBlock;
        next.pCP = 0;
        next.pTP = 0;
    }
    else
    {
        assert(false);  //TODO: restore this

#ifdef _DEBUG
        {
            const mkvparser::Block* const pBlock = pCurr->GetBlock();
            assert(pBlock);

            const LONGLONG ns = pBlock->GetTime(pCurrCluster);

            const double c = double(ns) / 1000000000;
            const double t = double(m_thin_ns) / 1000000000;

            odbgstream os;
            os << "WebmMfSTreamVideo::PopulateSample: thin[sec]=" << t
               << " curr[sec]=" << c
               << " curr-thin=" << (c - t)
               << " rate=" << m_rate
               << endl;
        }
#endif

        assert(m_curr.pCP);
        assert(m_curr.pTP);

        const mkvparser::Cues* const pCues = m_pTrack->m_pSegment->GetCues();
        assert(pCues);

        if (m_rate <= 1)
        {
            next.pCP = pCues->GetNext(m_curr.pCP);

            if (next.pCP == 0)
            {
                next.pTP = 0;
                next.pBE = 0;
            }
            else
            {
                next.pTP = next.pCP->Find(m_pTrack);
                assert(next.pTP);  //TODO

                next.pBE = pCues->GetBlock(next.pCP, next.pTP);
            }
        }
        else
        {
            const double delta_ns_ = double(m_rate) * 1000000000.0;
            const LONGLONG delta_ns = static_cast<LONGLONG>(delta_ns_);
            LONGLONG next_ns = m_thin_ns + delta_ns;  //target

            typedef mkvparser::CuePoint CP;
            typedef mkvparser::CuePoint::TrackPosition TP;
            typedef mkvparser::BlockEntry BE;
            typedef mkvparser::Block B;

            const CP* pCurr = m_curr.pCP;
            assert(pCurr);

            for (;;)
            {
                const CP* const pNext = pCues->GetNext(pCurr);

                if (pNext == 0)  //no more cue points: done
                {
                    next.pTP = 0;
                    next.pBE = 0;

                    break;
                }

                const TP* const pTP = pNext->Find(m_pTrack);
                assert(pTP);  //TODO

                const BE* const pBE = pCues->GetBlock(pNext, pTP);
                assert(pBE);  //TODO
                assert(!pBE->EOS());

                const B* const pB = pBE->GetBlock();
                assert(pB);

                const LONGLONG time_ns = pB->GetTime(pBE->GetCluster());

                if (time_ns >= next_ns)
                {
                    next.pCP = pNext;
                    next.pTP = pTP;
                    next.pBE = pBE;

                    m_thin_ns = next_ns;

                    for (;;)
                    {
                        next_ns = m_thin_ns + delta_ns;

                        if (next_ns > time_ns)
                            break;

                        m_thin_ns = next_ns;
                    }

                    break;
                }

                pCurr = pNext;
            }
        }
    }

    IMFSamplePtr pSample;

    HRESULT hr = MFCreateSample(&pSample);
    assert(SUCCEEDED(hr));  //TODO
    assert(pSample);

    if (pToken)
    {
        hr = pSample->SetUnknown(MFSampleExtension_Token, pToken);
        assert(SUCCEEDED(hr));
    }

    mkvparser::IMkvReader* const pReader = pCurrCluster->m_pSegment->m_pReader;

    const int frame_count = pCurrBlock->GetFrameCount();
    assert(frame_count > 0);  //TODO

    for (int i = 0; i < frame_count; ++i)
    {
        const mkvparser::Block::Frame& f = pCurrBlock->GetFrame(i);

        const long cbBuffer = f.len;
        assert(cbBuffer > 0);

        IMFMediaBufferPtr pBuffer;

        hr = MFCreateMemoryBuffer(cbBuffer, &pBuffer);
        assert(SUCCEEDED(hr));
        assert(pBuffer);

        BYTE* ptr;
        DWORD cbMaxLength;

        hr = pBuffer->Lock(&ptr, &cbMaxLength, 0);
        assert(SUCCEEDED(hr));
        assert(ptr);
        assert(cbMaxLength >= DWORD(cbBuffer));

        const long status = f.Read(pReader, ptr);
        assert(status == 0);

        hr = pBuffer->SetCurrentLength(cbBuffer);
        assert(SUCCEEDED(hr));

        hr = pBuffer->Unlock();
        assert(SUCCEEDED(hr));

        hr = pSample->AddBuffer(pBuffer);
        assert(SUCCEEDED(hr));
    }

    const bool bKey = pCurrBlock->IsKey();

    if (bKey)
    {
        assert(frame_count == 1);  //TODO

        hr = pSample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
        assert(SUCCEEDED(hr));
    }

    if (m_bDiscontinuity)
    {
        //TODO: resolve whether to set this for first of the preroll samples,
        //or wait until last of preroll samples has been pushed downstream.

        hr = pSample->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
        assert(SUCCEEDED(hr));

        m_bDiscontinuity = false;
    }

    const bool bInvisible = pCurrBlock->IsInvisible();

    const LONGLONG curr_ns = pCurrBlock->GetTime(pCurrCluster);
    const LONGLONG sample_time = curr_ns / 100;  //reftime units

    hr = pSample->SetSampleTime(sample_time);
    assert(SUCCEEDED(hr));

    const LONGLONG preroll_ns = m_pSource->m_preroll_ns;

    if (bInvisible || ((preroll_ns >= 0) && (curr_ns < preroll_ns)))
    {
        //TODO: handle this for audio too

        //TODO: it's not clear whether we need to do
        //this if we're thinning.  No great harm as is,
        //since all it means is that immediately following
        //a seek the user sees a different keyframe.

        hr = pSample->SetUINT32(WebmTypes::WebMSample_Preroll, TRUE);
        assert(SUCCEEDED(hr));
    }

    //TODO: list of attributes here:
    //http://msdn.microsoft.com/en-us/library/dd317906%28v=VS.85%29.aspx
    //http://msdn.microsoft.com/en-us/library/aa376629%28v=VS.85%29.aspx

    const mkvparser::BlockEntry* const pNextEntry = m_pNextBlock;  //TODO

    const mkvparser::Segment* const pSegment = m_pTrack->m_pSegment;
    const mkvparser::SegmentInfo* const pInfo = pSegment->GetInfo();

    LONGLONG duration_ns = -1;

    if ((pNextEntry != 0) && !pNextEntry->EOS())
    {
        const mkvparser::Block* const b = pNextEntry->GetBlock();
        assert(b);

        const mkvparser::Cluster* const c = pNextEntry->GetCluster();

        const LONGLONG next_ns = b->GetTime(c);
        assert(next_ns >= curr_ns);

        duration_ns = next_ns - curr_ns;
        assert(duration_ns >= 0);
    }
    else if (pInfo)
    {
        const LONGLONG next_ns = pInfo->GetDuration();

        if ((next_ns >= 0) && (next_ns > curr_ns))
            duration_ns = next_ns - curr_ns;
    }

    if (duration_ns < 0)
    {
        typedef mkvparser::VideoTrack VT;
        const VT* const pVT = static_cast<const VT*>(m_pTrack);

        double frame_rate = pVT->GetFrameRate();

        if (frame_rate <= 0)
            frame_rate = 10;  //100ms

        const double ns = 1000000000.0 / frame_rate;
        const LONGLONG ns_per_frame = static_cast<LONGLONG>(ns);

        duration_ns = LONGLONG(frame_count) * ns_per_frame;
    }

    if (duration_ns >= 0)
    {
        const LONGLONG sample_duration = duration_ns / 100;
        assert(sample_duration >= 0);

        hr = pSample->SetSampleDuration(sample_duration);
        assert(SUCCEEDED(hr));
    }

    m_curr = next;

    MkvReader& f = m_pSource->m_file;

    f.UnlockPage(m_pLocked);
    m_pLocked = 0;

    const int status = f.LockPage(next.pBE);
    assert(status == 0);

    if (status == 0)
        m_pLocked = next.pBE;

    m_pNextBlock = 0;

    return ProcessSample(pSample);
}


#if 0
HRESULT WebmMfStreamVideo::PopulateSample(IMFSample* pSample)
{
    assert(pSample);

    const mkvparser::BlockEntry* const pCurr = m_curr.pBE;

    if ((pCurr == 0) || pCurr->EOS())
        return S_FALSE;

    const mkvparser::Block* const pCurrBlock = pCurr->GetBlock();
    assert(pCurrBlock);
    assert(pCurrBlock->GetTrackNumber() == m_pTrack->GetNumber());

    const mkvparser::Cluster* const pCurrCluster = pCurr->GetCluster();
    assert(pCurrCluster);

#ifdef _DEBUG
    if (m_thin_ns >= 0)
    {
        const mkvparser::Block* const pBlock = pCurr->GetBlock();
        assert(pBlock);

        const LONGLONG ns = pBlock->GetTime(pCurrCluster);

        const double c = double(ns) / 1000000000;
        const double t = double(m_thin_ns) / 1000000000;

        odbgstream os;
        os << "WebmMfSTreamVideo::PopulateSample: thin[sec]=" << t
           << " curr[sec]=" << c
           << " curr-thin=" << (c - t)
           << " rate=" << m_rate
           << endl;
    }
#endif

    SeekInfo next;

    if (m_thin_ns < 0)  //no thinning
    {
        const long result = m_pTrack->GetNext(pCurr, next.pBE);

        if (result == mkvparser::E_BUFFER_NOT_FULL)
            return VFW_E_BUFFER_UNDERFLOW;

        assert(result >= 0);  //TODO
    }
    else
    {
        assert(m_curr.pCP);
        assert(m_curr.pTP);

        const mkvparser::Cues* const pCues = m_pTrack->m_pSegment->GetCues();
        assert(pCues);

        if (m_rate <= 1)
        {
            next.pCP = pCues->GetNext(m_curr.pCP);

            if (next.pCP == 0)
            {
                next.pTP = 0;
                next.pBE = 0;
            }
            else
            {
                next.pTP = next.pCP->Find(m_pTrack);
                assert(next.pTP);  //TODO

                next.pBE = pCues->GetBlock(next.pCP, next.pTP);
            }
        }
        else
        {
            const double delta_ns_ = double(m_rate) * 1000000000.0;
            const LONGLONG delta_ns = static_cast<LONGLONG>(delta_ns_);
            LONGLONG next_ns = m_thin_ns + delta_ns;  //target

            typedef mkvparser::CuePoint CP;
            typedef mkvparser::CuePoint::TrackPosition TP;
            typedef mkvparser::BlockEntry BE;
            typedef mkvparser::Block B;

            const CP* pCurr = m_curr.pCP;
            assert(pCurr);

            for (;;)
            {
                const CP* const pNext = pCues->GetNext(pCurr);

                if (pNext == 0)  //no more cue points: done
                {
                    next.pTP = 0;
                    next.pBE = 0;

                    break;
                }

                const TP* const pTP = pNext->Find(m_pTrack);
                assert(pTP);  //TODO

                const BE* const pBE = pCues->GetBlock(pNext, pTP);
                assert(pBE);  //TODO
                assert(!pBE->EOS());

                const B* const pB = pBE->GetBlock();
                assert(pB);

                const LONGLONG time_ns = pB->GetTime(pBE->GetCluster());

                if (time_ns >= next_ns)
                {
                    next.pCP = pNext;
                    next.pTP = pTP;
                    next.pBE = pBE;

                    m_thin_ns = next_ns;

                    for (;;)
                    {
                        next_ns = m_thin_ns + delta_ns;

                        if (next_ns > time_ns)
                            break;

                        m_thin_ns = next_ns;
                    }

                    break;
                }

                pCurr = pNext;
            }
        }
    }

    HRESULT hr;

    mkvparser::IMkvReader* const pReader = pCurrCluster->m_pSegment->m_pReader;

    const int frame_count = pCurrBlock->GetFrameCount();
    assert(frame_count > 0);  //TODO

    for (int i = 0; i < frame_count; ++i)
    {
        const mkvparser::Block::Frame& f = pCurrBlock->GetFrame(i);

        const long cbBuffer = f.len;
        assert(cbBuffer > 0);

        IMFMediaBufferPtr pBuffer;

        hr = MFCreateMemoryBuffer(cbBuffer, &pBuffer);
        assert(SUCCEEDED(hr));
        assert(pBuffer);

        BYTE* ptr;
        DWORD cbMaxLength;

        hr = pBuffer->Lock(&ptr, &cbMaxLength, 0);
        assert(SUCCEEDED(hr));
        assert(ptr);
        assert(cbMaxLength >= DWORD(cbBuffer));

        const long status = f.Read(pReader, ptr);
        assert(status == 0);

        hr = pBuffer->SetCurrentLength(cbBuffer);
        assert(SUCCEEDED(hr));

        hr = pBuffer->Unlock();
        assert(SUCCEEDED(hr));

        hr = pSample->AddBuffer(pBuffer);
        assert(SUCCEEDED(hr));
    }

    const bool bKey = pCurrBlock->IsKey();

    if (bKey)
    {
        assert(frame_count == 1);  //TODO

        hr = pSample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
        assert(SUCCEEDED(hr));
    }

    if (m_bDiscontinuity)
    {
        //TODO: resolve whether to set this for first of the preroll samples,
        //or wait until last of preroll samples has been pushed downstream.

        hr = pSample->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
        assert(SUCCEEDED(hr));

        m_bDiscontinuity = false;
    }

    const bool bInvisible = pCurrBlock->IsInvisible();

    const LONGLONG curr_ns = pCurrBlock->GetTime(pCurrCluster);
    const LONGLONG sample_time = curr_ns / 100;  //reftime units

    hr = pSample->SetSampleTime(sample_time);
    assert(SUCCEEDED(hr));

    const LONGLONG preroll_ns = m_pSource->m_preroll_ns;

    if (bInvisible || ((preroll_ns >= 0) && (curr_ns < preroll_ns)))
    {
        //TODO: handle this for audio too

        //TODO: it's not clear whether we need to do
        //this if we're thinning.  No great harm as is,
        //since all it means is that immediately following
        //a seek the user sees a different keyframe.

        hr = pSample->SetUINT32(WebmTypes::WebMSample_Preroll, TRUE);
        assert(SUCCEEDED(hr));
    }

    //TODO: list of attributes here:
    //http://msdn.microsoft.com/en-us/library/dd317906%28v=VS.85%29.aspx
    //http://msdn.microsoft.com/en-us/library/aa376629%28v=VS.85%29.aspx

    //const mkvparser::BlockEntry* const pNextEntry = next.pBE;
    const mkvparser::BlockEntry* pNextEntry;

    const long result = m_pTrack->GetNext(m_curr.pBE, pNextEntry);
    assert(result >= 0);  //TODO

    const mkvparser::Segment* const pSegment = m_pTrack->m_pSegment;
    const mkvparser::SegmentInfo* const pInfo = pSegment->GetInfo();

    LONGLONG duration_ns = -1;

    if ((pNextEntry != 0) && !pNextEntry->EOS())
    {
        const mkvparser::Block* const b = pNextEntry->GetBlock();
        assert(b);

        const mkvparser::Cluster* const c = pNextEntry->GetCluster();

        const LONGLONG next_ns = b->GetTime(c);
        assert(next_ns >= curr_ns);

        duration_ns = next_ns - curr_ns;
        assert(duration_ns >= 0);
    }
    else if (pInfo)
    {
        const LONGLONG next_ns = pInfo->GetDuration();

        if ((next_ns >= 0) && (next_ns > curr_ns))
            duration_ns = next_ns - curr_ns;
    }

    if (duration_ns < 0)
    {
        typedef mkvparser::VideoTrack VT;
        const VT* const pVT = static_cast<const VT*>(m_pTrack);

        double frame_rate = pVT->GetFrameRate();

        if (frame_rate <= 0)
            frame_rate = 10;  //100ms

        const double ns = 1000000000.0 / frame_rate;
        const LONGLONG ns_per_frame = static_cast<LONGLONG>(ns);

        duration_ns = LONGLONG(frame_count) * ns_per_frame;
    }

    if (duration_ns >= 0)
    {
        const LONGLONG sample_duration = duration_ns / 100;
        assert(sample_duration >= 0);

        hr = pSample->SetSampleDuration(sample_duration);
        assert(SUCCEEDED(hr));
    }

    m_curr = next;

    MkvReader& f = m_pSource->m_file;

    f.UnlockPage(m_pLocked);
    m_pLocked = next.pBE;
    f.LockPage(m_pLocked);

    return S_OK;
}
#endif


void WebmMfStreamVideo::OnStop()
{
    MkvReader& f = m_pSource->m_file;

    f.UnlockPage(m_pLocked);
    m_pLocked = 0;
}



const mkvparser::BlockEntry* WebmMfStreamVideo::GetCurrBlock() const
{
    return m_curr.pBE;
}


void WebmMfStreamVideo::SetRate(BOOL bThin, float rate)
{
    assert(rate >= 0);  //TODO
    m_rate = rate;

    if (!bThin)
    {
        m_thin_ns = -2;
        return;
    }

    if (m_pSource->m_state == WebmMfSource::kStateStopped)
    {
        m_thin_ns = -1;  //request
        return;
    }

    SeekInfo& i = m_curr;

    if ((i.pBE == 0) || i.pBE->EOS())  //we're already done
        return;

    const mkvparser::Cluster* const pCluster = i.pBE->GetCluster();
    assert(pCluster);
    assert(!pCluster->EOS());

    const mkvparser::Block* pBlock = i.pBE->GetBlock();
    assert(pBlock);

    const LONGLONG time_ns = pBlock->GetTime(pCluster);
    assert(time_ns >= 0);

    mkvparser::Segment* const pSegment = m_pTrack->m_pSegment;

    const mkvparser::Cues* const pCues = pSegment->GetCues();
    assert(pCues);  //req'd to support thinning

    const bool bFound = pCues->Find(time_ns, m_pTrack, i.pCP, i.pTP);
    assert(bFound);

    i.pBE = pCues->GetBlock(i.pCP, i.pTP);
    assert(i.pBE);  //TODO
    assert(!i.pBE->EOS());

    MkvReader& f = m_pSource->m_file;

    f.UnlockPage(m_pLocked);
    m_pLocked = i.pBE;

    const int status = f.LockPage(m_pLocked);
    status;
    assert(status == 0);

    pBlock = i.pBE->GetBlock();
    assert(pBlock);

    m_thin_ns = pBlock->GetTime(pCluster);
    assert(m_thin_ns >= 0);
}


int WebmMfStreamVideo::LockCurrBlock()
{
    const mkvparser::BlockEntry* const pCurr = m_curr.pBE;
    assert(pCurr);
    assert(!pCurr->EOS());
    assert(m_pLocked == 0);

    MkvReader& f = m_pSource->m_file;

    const int status = f.LockPage(pCurr);
    assert(status == 0);

    if (status)  //should never happen
        return status;

    m_pLocked = pCurr;
    return 0;  //succeeded
}


}  //end namespace WebmMfSourceLib
