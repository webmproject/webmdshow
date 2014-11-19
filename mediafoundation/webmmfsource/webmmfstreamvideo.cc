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
#include <iomanip>
using std::endl;
using std::fixed;
using std::setprecision;
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
    WebmMfStream(pSource, pDesc, pTrack),
    m_pNextBlock(0),
    m_next_index(-1)
{
}


WebmMfStreamVideo::~WebmMfStreamVideo()
{
}


void WebmMfStreamVideo::OnDeselect()
{
    m_pNextBlock = 0;
    m_next_index = -1;
}


void WebmMfStreamVideo::OnSetCurrBlock()
{
    m_pNextBlock = 0;
    m_next_index = -1;
}


void WebmMfStreamVideo::SetCurrBlockIndex(const mkvparser::Cluster* pCluster)
{
    assert(m_curr.pBE == 0);
    assert(m_curr.pCluster == 0);
    assert(m_cluster_pos >= 0);
    assert(m_time_ns >= 0);

    if ((pCluster == 0) || pCluster->EOS())  //weird
    {
        m_curr.Init(m_pTrack->GetEOS());
        return;
    }

    //const LONGLONG cluster_pos = pCluster->GetPosition();
    //assert(cluster_pos >= 0);
    assert(pCluster->GetPosition() == m_cluster_pos);

    mkvparser::Segment* const pSegment = m_pSource->m_pSegment;

    const mkvparser::Cues* const pCues = pSegment->GetCues();
    assert(pCues);

    using mkvparser::BlockEntry;
    using mkvparser::CuePoint;

    const CuePoint* pCP;
    const CuePoint::TrackPosition* pTP;

    for (;;)
    {
        pCues->LoadCuePoint();

        pCP = pCues->GetLast();
        assert(pCP);

        if (pCP->GetTime(pSegment) >= m_time_ns)
            break;

        if (pCues->DoneParsing())
            break;
    }

    if (pCues->Find(m_time_ns, m_pTrack, pCP, pTP))
    {
        assert(pCP);
        assert(pTP);
        assert(pTP->m_track == m_pTrack->GetNumber());

        if ((pTP->m_pos == m_cluster_pos) && (pTP->m_block > 0))
        {
            m_curr.index = -1;  //means use pTP
            m_curr.pCluster = pCluster;
            m_curr.pBE = 0;  //means we lazy-init
            m_curr.pCP = pCP;
            m_curr.pTP = pTP;

            return;
        }
    }

    m_curr.index = 0;  //start search at beginning of cluster
    m_curr.pCluster = pCluster;
    m_curr.pBE = 0;  //must lazy-init
    m_curr.pCP = 0;
    m_curr.pTP = 0;
}


bool WebmMfStreamVideo::SetCurrBlockObject()  //return long value instead?
{
    //We have an index, and a cluster, which we must convert
    //to a block object.

    if (m_curr.pBE)  //weird
        return true;   //done

    const mkvparser::Cluster* const pCluster = m_curr.pCluster;
    assert(pCluster);
    assert(!pCluster->EOS());

    const LONGLONG tn = m_pTrack->GetNumber();

    if (m_curr.index < 0)  //synthesize value from cue point
    {
        assert(m_curr.pCP);
        assert(m_curr.pTP);
        assert(m_curr.pTP->m_track == tn);
        assert(m_curr.pTP->m_block > 0);

        const LONGLONG block_ = m_curr.pTP->m_block;
        assert(block_ > 0);
        assert(block_ <= LONG_MAX);

        const LONG block = static_cast<LONG>(block_);
        const LONG index = block - 1;

        //We want to test the cue point now, to determine
        //whether it's pointing to a block for this track,
        //and that the block is also a keyframe.  If neither
        //of these conditions are true then we set the index
        //to 0 and start a linear search.

        const mkvparser::BlockEntry* pCurr;

        const long status = pCluster->GetEntry(index, pCurr);

        //Underflow is interpreted here to mean that we did not
        //find a block because more was left to parse.

        if (status < 0)
        {
            assert(status == mkvparser::E_BUFFER_NOT_FULL);
            return false;  //not done: need to do more parsing
        }

        //This is interpreted to mean that we did not find
        //a block because the entire cluster has been parsed,
        //but there were no more blocks on this cluster.

        if (status == 0)  //weird
        {
            m_curr.pBE = m_pTrack->GetEOS();
            return true;  //done
        }

        //We have an entry.

        assert(pCurr);
        assert(!pCurr->EOS());

        const mkvparser::Block* const pBlock = pCurr->GetBlock();
        assert(pBlock);

        const long long tc = m_curr.pCP->GetTimeCode();
        assert(tc >= 0);

        if ((pBlock->GetTrackNumber() == tn) &&
            (pBlock->GetTimeCode(pCluster) == tc) &&
            pBlock->IsKey())
        {
            m_curr.pBE = pCurr;
            return true;  //done
        }

        m_curr.index = 0;  //must do linear search
    }

    long& index = m_curr.index;
    assert(index >= 0);

    const LONGLONG tc = m_curr.pCP ? m_curr.pCP->GetTimeCode() : -1;

    for (;;)
    {
        const mkvparser::BlockEntry* pCurr;

        const long status = m_curr.pCluster->GetEntry(index, pCurr);

        //Underflow is interpreted here to mean that we did not
        //find a block because more was left to parse.

        if (status < 0)
        {
            assert(status == mkvparser::E_BUFFER_NOT_FULL);
            return false;  //not done: need to do more parsing
        }

        //This is interpreted to mean that we did not find
        //a block because the entire cluster has been parsed,
        //but there were no more blocks on this cluster.

        if (status == 0)  //weird
        {
            m_curr.pBE = m_pTrack->GetEOS();
            return true;  //done
        }

        //We have an entry.

        assert(pCurr);
        assert(!pCurr->EOS());

        const mkvparser::Block* const pBlock = pCurr->GetBlock();
        assert(pBlock);

        if (pBlock->GetTrackNumber() != tn)
        {
            ++index;
            continue;
        }

        if (tc < 0)  //this is not a cue point (pathological)
        {
            if (pBlock->IsKey())
            {
                m_curr.pBE = pCurr;
                return true;
            }

            ++index;
            continue;
        }

        const long long tc_ = pBlock->GetTimeCode(pCluster);

        if (tc_ < tc)
        {
            ++index;
            continue;
        }

        if ((tc_ > tc) || !pBlock->IsKey())
        {
            m_curr.pBE = m_pTrack->GetEOS();
            return true;
        }

        m_curr.pBE = pCurr;
        return true;
    }
}


long WebmMfStreamVideo::GetNextBlock()
{
    if (m_pNextBlock)
        return 1;  //we have a next block

    const mkvparser::BlockEntry* const pCurr = m_curr.pBE;
    assert(pCurr);
    assert(!pCurr->EOS());
    assert((m_pLocked == 0) || (m_pLocked == pCurr));

    const LONGLONG tn = m_pTrack->GetNumber();

    const mkvparser::Cluster* const pCluster = pCurr->GetCluster();
    assert(pCluster);
    assert(!pCluster->EOS());

#if 0
    m_pNextBlock = pCluster->GetNext(pCurr);

    while (m_pNextBlock)
    {
        const mkvparser::Block* const pBlock = m_pNextBlock->GetBlock();
        assert(pBlock);

        if (pBlock->GetTrackNumber() == tn)
            return true;

        m_pNextBlock = pCluster->GetNext(m_pNextBlock);
    }

    return false;  //no, we do not have next block
#else
    if (m_next_index < 0)
        m_next_index = pCurr->GetIndex() + 1;

    for (;;)
    {
        const mkvparser::BlockEntry* pNext;

        const long status = pCluster->GetEntry(m_next_index, pNext);

        //status < 0
        //Underflow is interpreted here to mean that we did not
        //find a block because more was left to parse.

        if (status < 0)
            return status;  //error or underflow

        //status = 0
        //This is interpreted to mean that we did not find
        //a block because the entire cluster has been parsed,
        //but there were no more blocks on this cluster.

        if (status == 0)
        {
            m_next_index = 0;
            return 0;  //tell caller we need another cluster
        }

        //We have an entry.

        assert(pNext);
        assert(!pNext->EOS());

        const mkvparser::Block* const pBlock = pNext->GetBlock();
        assert(pBlock);

        if (pBlock->GetTrackNumber() == tn)
        {
            m_pNextBlock = pNext;
            return 1;  //we have a next block
        }

        ++m_next_index;
    }
#endif
}


long WebmMfStreamVideo::NotifyNextCluster(
    const mkvparser::Cluster* pCluster)
{
    if ((pCluster == 0) || pCluster->EOS())
    {
        m_pNextBlock = m_pTrack->GetEOS();
        return 1;  //done
    }

    const LONGLONG tn = m_pTrack->GetNumber();

#if 0
    m_pNextBlock = pNextCluster->GetFirst();

    while (m_pNextBlock)
    {
        const mkvparser::Block* const pBlock = m_pNextBlock->GetBlock();
        assert(pBlock);

        if (pBlock->GetTrackNumber() == tn)
            return true;  //done

        m_pNextBlock = pNextCluster->GetNext(m_pNextBlock);
    }

    return false;  //not done yet
#else
    assert(m_next_index >= 0);  //set in GetNextBlock

    for (;;)
    {
        const mkvparser::BlockEntry* pNext;

        const long status = pCluster->GetEntry(m_next_index, pNext);

        //Underflow is interpreted here to mean that we did not
        //find a block because more was left to parse.

        if (status < 0)
            return status;  //error or underflow

        //This is interpreted to mean that we did not find
        //a block because the entire cluster has been parsed,
        //but there were no more blocks on this cluster.

        if (status == 0)
        {
            m_next_index = 0;
            return 0;  //tell caller we need another cluster
        }

        //We have an entry.

        assert(pNext);
        assert(!pNext->EOS());

        const mkvparser::Block* const pBlock = pNext->GetBlock();
        assert(pBlock);

        if (pBlock->GetTrackNumber() == tn)
        {
            m_pNextBlock = pNext;
            return 1;  //we have a next block
        }

        ++m_next_index;
    }
#endif
}


HRESULT WebmMfStreamVideo::GetSample(IUnknown* pToken)
{
    const mkvparser::BlockEntry* const pCurr = m_curr.pBE;
    assert(pCurr);
    assert(!pCurr->EOS());
    //assert(m_pLocked);
    //assert(m_pLocked == pCurr);
    assert((m_pLocked == 0) || (m_pLocked == pCurr));

    const mkvparser::Block* const pCurrBlock = pCurr->GetBlock();
    assert(pCurrBlock);
    assert(pCurrBlock->GetTrackNumber() == m_pTrack->GetNumber());

    const mkvparser::Cluster* const pCurrCluster = pCurr->GetCluster();
    assert(pCurrCluster);
    assert(!pCurrCluster->EOS());

    const LONGLONG curr_ns = pCurrBlock->GetTime(pCurrCluster);

    assert(m_pNextBlock);

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

        if (status >= 0)  //success
        {
            hr = pBuffer->SetCurrentLength(cbBuffer);
            assert(SUCCEEDED(hr));
        }

        hr = pBuffer->Unlock();
        assert(SUCCEEDED(hr));

        if (status < 0)  //error (weird)
            return E_FAIL;

        hr = pSample->AddBuffer(pBuffer);
        assert(SUCCEEDED(hr));
    }

    const bool bKey = pCurrBlock->IsKey();

    if (bKey)
    {
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

    //odbgstream os;
    //os << "Video::GetSample: time[reftime]=" << sample_time
    //   << " time[sec]="
    //   << fixed
    //   << setprecision(3)
    //   << (float(sample_time) / 10000000.0F)
    //   << endl;

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

#if 0
        //TODO: we need to handle this somehow

        const int status = f.LockPage(m_pNextBlock);
        assert(status == 0);

        if (status == 0)
            m_pLocked = m_pNextBlock;
#endif

        m_pNextBlock = 0;
        m_next_index = -1;

        return ProcessSample(pSample);
    }

    //thinning mode

    m_pNextBlock = 0;
    m_next_index = -1;

    const mkvparser::Cues* const pCues = pSegment->GetCues();
    assert(pCues);

    const mkvparser::CuePoint* pCP;
    const mkvparser::CuePoint::TrackPosition* pTP;

    if (m_curr.pCP == 0)
    {
        for (;;)
        {
            pCues->LoadCuePoint();

            pCP = pCues->GetLast();
            assert(pCP);

            if (pCP->GetTime(pSegment) >= m_time_ns)
                break;

            if (pCues->DoneParsing())
                break;
        }

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

            const mkvparser::CuePoint* const pCurr = pCP;

            for (;;)
            {
                pCP = pCues->GetNext(pCurr);

                if ((pCP != 0) || pCues->DoneParsing())
                    break;

                pCues->LoadCuePoint();
            }

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

        m_curr.index = -1;
        m_curr.pCluster = 0;
        m_curr.pBE = 0;
        m_curr.pCP = pCP;
        m_curr.pTP = pTP;

        m_thin_ns = m_time_ns;

        return ProcessSample(pSample);
    }

    if (m_rate <= 1)
    {
        assert(m_rate >= 0);

        for (;;)
        {
            pCP = pCues->GetNext(m_curr.pCP);

            if ((pCP != 0) || pCues->DoneParsing())
                break;

            pCues->LoadCuePoint();
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

        m_curr.index = -1;
        m_curr.pCluster = 0;
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
        for (;;)
        {
            pCP = pCues->GetNext(m_curr.pCP);

            if ((pCP != 0) || pCues->DoneParsing())
                break;

            pCues->LoadCuePoint();
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

        m_curr.index = -1;
        m_curr.pCluster = 0;
        m_curr.pBE = 0;
        m_curr.pCP = pCP;
        m_curr.pTP = pTP;

        return ProcessSample(pSample);
    }
}


bool WebmMfStreamVideo::GetSampleExtent(LONGLONG&, LONG&)
{
    return true;
}


void WebmMfStreamVideo::GetSampleExtentCompletion()
{
}


}  //end namespace WebmMfSourceLib
