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
//#include <vfwmsgs.h>
#include <propvarutil.h>
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
    //mkvparser::Segment* const pSegment = pTrack->m_pSegment;

    double frame_rate = pTrack->GetFrameRate();

    if (frame_rate <= 0)
        return E_FAIL;  //TODO

    if (frame_rate > std::numeric_limits<UINT32>::max())
        return E_FAIL;  //TODO

#if 0  //TODO: restore this
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
#endif

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

    return pStream ? S_OK : E_OUTOFMEMORY;
}


WebmMfStreamVideo::WebmMfStreamVideo(
    WebmMfSource* pSource,
    IMFStreamDescriptor* pDesc,
    const mkvparser::VideoTrack* pTrack) :
    WebmMfStream(pSource, pDesc, pTrack)
    //m_rate(1),
    //m_thin_ns(-2)
{
}


WebmMfStreamVideo::~WebmMfStreamVideo()
{
}


#if 0  //TODO: restore this
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
#else
HRESULT WebmMfStreamVideo::Start(const PROPVARIANT& var)
{
    if (!IsSelected())
        return S_FALSE;

    //assert(m_pLocked == 0);
    //m_bDiscontinuity = true;
    ////TODO: m_curr = info;
    //assert(m_curr.pBE);
    //m_pNextBlock = 0;  //means "we don't know yet"

    assert(var.vt == VT_I8);

    const LONGLONG reftime = var.hVal.QuadPart;
    assert(reftime >= 0);

#if 0  //TODO: restore this
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
#endif

    return OnStart(var);
}
#endif


#if 0
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
#endif



void WebmMfStreamVideo::SetCurrBlockCompletion(
    const mkvparser::Cluster* pCluster)
{
    assert(m_curr.pBE == 0);
    assert(m_time_ns >= 0);
    assert(m_cluster_pos >= 0);
    //assert(m_time_ns >= 0);

    if ((pCluster == 0) ||
        pCluster->EOS() ||
        (pCluster->GetEntryCount() <= 0))
    {
        m_curr.Init(m_pTrack->GetEOS());  //weird
        return;
    }

    assert(pCluster->m_pos >= 0);

    mkvparser::Segment* const pSegment = m_pSource->m_pSegment;

    const mkvparser::Cues* const pCues = pSegment->GetCues();
    assert(pCues);

    using mkvparser::BlockEntry;
    using mkvparser::CuePoint;

    const CuePoint* pCP;
    const CuePoint::TrackPosition* pTP;

    if (pCues->Find(m_time_ns, m_pTrack, pCP, pTP))
    {
        assert(pCP);
        assert(pTP);
        assert(pTP->m_track == m_pTrack->GetNumber());

        if (pTP->m_pos == pCluster->m_pos)  //what we expect to be true
        {
            m_curr.pBE = pCluster->GetEntry(*pCP, *pTP);

            if (m_curr.pBE)  //can be false if bad cue point
            {
                m_curr.pCP = pCP;
                m_curr.pTP = pTP;

                return;
            }
        }
    }

    //if (const BlockEntry* pBE = pCluster->GetEntry(m_pTrack, m_time_ns))
    if (const BlockEntry* pBE = pCluster->GetEntry(m_pTrack))
        m_curr.Init(pBE);
    else
        m_curr.Init(m_pTrack->GetEOS());

    //m_cluster_pos = -1;
    //m_time_ns = -1;
}


HRESULT WebmMfStreamVideo::GetSample(IUnknown* pToken)
{
    if (!IsSelected())
        return S_FALSE;

    if (IsShutdown())  //weird
    {
        MkvReader& f = m_pSource->m_file;

        f.UnlockPage(m_pLocked);
        m_pLocked = 0;

        return S_FALSE;
    }

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
    assert(!pCurrCluster->EOS());

    const LONGLONG curr_ns = pCurrBlock->GetTime(pCurrCluster);

    //odbgstream os;
    //os << "WebmMfStreamVideo::GetSample: curr.time[ns]="
    //   //<< (double(curr_ns) / 1000000000)
    //   << curr_ns
    //   << endl;

    if (m_pNextBlock == 0)
    {
        const HRESULT hr = GetNextBlock();

        //os << "WebmMfStreamVideo::GetSample: GetNextBlock.hr="
        //   << hr
        //   << endl;

        if (FAILED(hr))  //no next entry found on current cluster
            return hr;
    }

    //os << "WebmMfStreamVideo::GetSample: HAVE NEXT BLOCK" << endl;

    //MEStreamThinMode
    //http://msdn.microsoft.com/en-us/library/aa370815(v=VS.85).aspx

    if (m_thin_ns == -2)  //non-thinning mode requested
    {
        PROPVARIANT v;

        HRESULT hr = InitPropVariantFromBoolean(FALSE, &v);
        assert(SUCCEEDED(hr));

        hr = QueueEvent(MEStreamThinMode, GUID_NULL, S_OK, &v);
        assert(SUCCEEDED(hr));

        hr = PropVariantClear(&v);
        assert(SUCCEEDED(hr));

        m_thin_ns = -3;  //non-thinning mode
    }
    else if (m_thin_ns == -1)  //thining mode requested
    {
        PROPVARIANT v;

        HRESULT hr = InitPropVariantFromBoolean(TRUE, &v);
        assert(SUCCEEDED(hr));

        hr = QueueEvent(MEStreamThinMode, GUID_NULL, S_OK, &v);
        assert(SUCCEEDED(hr));

        hr = PropVariantClear(&v);
        assert(SUCCEEDED(hr));

        m_thin_ns = pCurrBlock->GetTime(pCurrCluster);
        assert(m_thin_ns >= 0);  //means we're in thinning mode
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

        //TODO: alternatively, we could tell the downstream component
        //what the requested time directly, and then let it make the
        //test above.

        hr = pSample->SetUINT32(WebmTypes::WebMSample_Preroll, TRUE);
        assert(SUCCEEDED(hr));
    }

    //TODO: list of attributes here:
    //http://msdn.microsoft.com/en-us/library/dd317906%28v=VS.85%29.aspx
    //http://msdn.microsoft.com/en-us/library/aa376629%28v=VS.85%29.aspx

    const mkvparser::BlockEntry* const pNextEntry = m_pNextBlock;
    assert(pNextEntry);

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

    MkvReader& f = m_pSource->m_file;

    f.UnlockPage(m_pLocked);
    m_pLocked = 0;

    if (m_thin_ns < 0)  //non-thinning mode
    {
        m_curr.Init(m_pNextBlock);

        const int status = f.LockPage(m_pNextBlock);
        assert(status == 0);

        if (status == 0)
            m_pLocked = m_pNextBlock;

        m_pNextBlock = 0;

        return ProcessSample(pSample);
    }

    //thinning mode

    m_pNextBlock = 0;

    const mkvparser::Cues* const pCues = pSegment->GetCues();
    assert(pCues);

    const mkvparser::CuePoint* pCP;
    const mkvparser::CuePoint::TrackPosition* pTP;

    if (m_curr.pCP == 0)
    {
        //TODO: do this at the time of the request, in order to
        //establish the invariant earlier.  If we are unable to
        //establish the invariant, then fail the SetRate call.

        if (!pCues->Find(m_thin_ns, m_pTrack, pCP, pTP))
        {
            m_curr.Init(m_pTrack->GetEOS());
            return ProcessSample(pSample);
        }

        assert(pCP);
        assert(pTP);

        for (;;)
        {
            m_time_ns = pCP->GetTime(pSegment);
            assert(m_time_ns >= 0);

            if (m_time_ns > m_thin_ns)
                break;

            pCP = pCues->GetNext(pCP);

            if (pCP == 0)  //no more queue points
                break;
        }

        if (pCP)
            pTP = pCP->Find(m_pTrack);
        else
            pTP = 0;

        if ((pCP == 0) || (pTP == 0))
        {
            m_curr.Init(m_pTrack->GetEOS());
            return ProcessSample(pSample);
        }

        m_cluster_pos = pTP->m_pos;

        m_curr.pBE = 0;
        m_curr.pCP = pCP;
        m_curr.pTP = pTP;

        m_thin_ns = m_time_ns;

        return ProcessSample(pSample);
    }

    if (m_rate <= 1)
    {
        assert(m_rate >= 0);

        pCP = pCues->GetNext(m_curr.pCP);

        if (pCP)
            pTP = pCP->Find(m_pTrack);
        else
            pTP = 0;

        if ((pCP == 0) || (pTP == 0))
        {
            m_curr.Init(m_pTrack->GetEOS());
            return ProcessSample(pSample);
        }

        m_curr.pBE = 0;
        m_curr.pCP = pCP;
        m_curr.pTP = pTP;

        const LONGLONG next_ns = pCP->GetTime(pSegment);
        assert(next_ns > m_time_ns);

        m_time_ns = next_ns;
        m_cluster_pos = pTP->m_pos;

        m_thin_ns = m_time_ns;  //doesn't really do much when rate <= 1

        return ProcessSample(pSample);
    }

    const double delta_ns_ = double(m_rate) * 1000000000.0;
    const LONGLONG delta_ns = static_cast<LONGLONG>(delta_ns_);
    LONGLONG next_ns = m_thin_ns + delta_ns;  //target

    for (;;)
    {
        pCP = pCues->GetNext(m_curr.pCP);

        if (pCP)
            pTP = pCP->Find(m_pTrack);
        else
            pTP = 0;

        if ((pCP == 0) || (pTP == 0))
        {
            m_curr.Init(m_pTrack->GetEOS());
            return ProcessSample(pSample);
        }

        m_time_ns = pCP->GetTime(pSegment);
        m_cluster_pos = pTP->m_pos;

        if (m_time_ns < next_ns)
        {
            m_curr.pCP = pCP;
            continue;
        }

        m_thin_ns = next_ns;

        for (;;)
        {
            next_ns = m_thin_ns + delta_ns;

            if (next_ns > m_time_ns)
                break;

            m_thin_ns = next_ns;
        }

        m_curr.pBE = 0;
        m_curr.pCP = pCP;
        m_curr.pTP = pTP;

        return ProcessSample(pSample);
    }
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


#if 0
void WebmMfStreamVideo::SetRate(BOOL bThin, float rate)
{
    assert(rate >= 0);  //TODO: implement reverse playback
    m_rate = rate;

    //m_thin_ns <= -3
    //  in not thinning mode
    //
    //m_thin_ns == -2
    //  we were in thinning mode
    //  not thinning already requested, but
    //  MEStreamThinMode has not been sent yet
    //
    //m_thin_ns == -1
    //  we were in not thinning mode
    //  thinning mode requested, but event
    //  hasn't been sent yet
    //
    //m_thin_ns >= 0
    //  in thinning mode
    //  thinning mode had been requested, and event
    //  has been sent

    if (bThin)
    {
        if (m_thin_ns <= -3)  //not thinning
        {
            m_thin_ns = -1;   //send notice of transition
            return;
        }

        if (m_thin_ns == -2)  //not thinning already requested
        {
            m_thin_ns = -1;   //go ahead and send notice of transition
            return;
        }

        //no change req'd here
    }
    else if (m_thin_ns <= -3)  //already not thinning
        return;

    else if (m_thin_ns == -2)  //not thinning already requested
        return;

    else if (m_thin_ns == -1)  //thinning requested
        m_thin_ns = -2;        //not thinning requested

    else
        m_thin_ns = -2;  //not thinning requested


#if 0 //TODO: restore this
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
#endif
}
#endif


}  //end namespace WebmMfSourceLib
