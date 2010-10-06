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
//#ifdef _DEBUG
//#include "odbgstream.hpp"
//using std::endl;
//#endif

_COM_SMARTPTR_TYPEDEF(IMFStreamDescriptor, __uuidof(IMFStreamDescriptor));
_COM_SMARTPTR_TYPEDEF(IMFMediaType, __uuidof(IMFMediaType));
_COM_SMARTPTR_TYPEDEF(IMFMediaTypeHandler, __uuidof(IMFMediaTypeHandler));
_COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, __uuidof(IMFMediaBuffer));

namespace WebmMfSourceLib
{

HRESULT WebmMfStreamVideo::CreateStreamDescriptor(
    mkvparser::Track* pTrack_,
    IMFStreamDescriptor*& pDesc)
{
    assert(pTrack_);
    assert(pTrack_->GetType() == 1);

    using mkvparser::VideoTrack;
    VideoTrack* const pTrack = static_cast<VideoTrack*>(pTrack_);

    const char* const codec = pTrack->GetCodecId();
    assert(codec);

    if (_stricmp(codec, "V_VP8") != 0)  //weird
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

    const double frame_rate = pTrack->GetFrameRate();

    if ((frame_rate > 0) && (frame_rate <= std::numeric_limits<UINT32>::max()))
    {
        UINT32 numer;
        UINT32 denom = 1;

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


HRESULT WebmMfStreamVideo::CreateStream(
    IMFStreamDescriptor* pSD,
    WebmMfSource* pSource,
    mkvparser::Track* pTrack_,
    WebmMfStream*& pStream)
{
    assert(pTrack_);
    assert(pTrack_->GetType() == 1);

    using mkvparser::VideoTrack;
    VideoTrack* const pTrack = static_cast<VideoTrack*>(pTrack_);

    pStream = new (std::nothrow) WebmMfStreamVideo(pSource, pSD, pTrack);
    assert(pStream);  //TODO

    //TODO: handle time

    return pStream ? S_OK : E_OUTOFMEMORY;
}


WebmMfStreamVideo::WebmMfStreamVideo(
    WebmMfSource* pSource,
    IMFStreamDescriptor* pDesc,
    mkvparser::VideoTrack* pTrack) :
    WebmMfStream(pSource, pDesc, pTrack)
{
}


WebmMfStreamVideo::~WebmMfStreamVideo()
{
}


HRESULT WebmMfStreamVideo::OnPopulateSample(
    const mkvparser::BlockEntry* pNextEntry,
    IMFSample* pSample)
{
    assert(pSample);

    //TODO:
    //assert(m_pBaseCluster);
    //assert(!m_pBaseCluster->EOS());

    assert(m_pCurr);
    assert(!m_pCurr->EOS());

    //assert((pNextEntry == 0) ||
    //       pNextEntry->EOS() ||
    //       !pNextEntry->IsBFrame());

    const mkvparser::Block* const pCurrBlock = m_pCurr->GetBlock();
    assert(pCurrBlock);
    assert(pCurrBlock->GetTrackNumber() == m_pTrack->GetNumber());

    mkvparser::Cluster* const pCurrCluster = m_pCurr->GetCluster();
    assert(pCurrCluster);

    //TODO:
    //const LONGLONG basetime_ns = m_pBaseCluster->GetFirstTime();
    //assert(basetime_ns >= 0);

    const long cbBuffer = pCurrBlock->GetSize();
    assert(cbBuffer >= 0);

    IMFMediaBufferPtr pBuffer;

    HRESULT hr = MFCreateMemoryBuffer(cbBuffer, &pBuffer);
    assert(SUCCEEDED(hr));
    assert(pBuffer);

    BYTE* ptr;
    DWORD cbMaxLength;

    hr = pBuffer->Lock(&ptr, &cbMaxLength, 0);
    assert(SUCCEEDED(hr));
    assert(ptr);
    assert(cbMaxLength >= DWORD(cbBuffer));

    mkvparser::IMkvReader* const pReader = pCurrCluster->m_pSegment->m_pReader;

    long status = pCurrBlock->Read(pReader, ptr);
    assert(status == 0);  //all bytes were read

    hr = pBuffer->SetCurrentLength(cbBuffer);
    assert(SUCCEEDED(hr));

    hr = pBuffer->Unlock();
    assert(SUCCEEDED(hr));

    hr = pSample->AddBuffer(pBuffer);
    assert(SUCCEEDED(hr));

#if 0  //TODO

    hr = pSample->SetPreroll(0);
    assert(SUCCEEDED(hr));

#endif  //TODO

    const bool bKey = pCurrBlock->IsKey();

    if (bKey)
    {
        hr = pSample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
        assert(SUCCEEDED(hr));
    }

    const LONGLONG curr_ns = pCurrBlock->GetTime(pCurrCluster);
    const LONGLONG sample_time = curr_ns / 100;  //reftime units

    hr = pSample->SetSampleTime(sample_time);
    assert(SUCCEEDED(hr));

    const LONGLONG preroll_ns = m_pSource->m_preroll_ns;

    if ((preroll_ns >= 0) && (curr_ns < preroll_ns))
    {
        //TODO: handle this for audio too

        hr = pSample->SetUINT32(WebmTypes::WebMSample_Preroll, TRUE);
        assert(SUCCEEDED(hr));

        //odbgstream os;
        //os << "WebmMfSource::WebmMfStreamVideo:"
        //   << " sending PREROLL sample: preroll[sec]="
        //   << (double(preroll_ns) / 1000000000)
        //   << " curr[sec]="
        //   << (double(curr_ns) / 1000000000)
        //   << " cluster_idx=" << m_pCurr->GetCluster()->m_index;
        //
        //if (bKey)
        //    os << " KEY";
        //
        //os << endl;
    }

    //TODO: list of attributes here:
    //http://msdn.microsoft.com/en-us/library/dd317906%28v=VS.85%29.aspx
    //http://msdn.microsoft.com/en-us/library/aa376629%28v=VS.85%29.aspx

    //TODO: we can better here: synthesize duration of last block
    //in stream from the duration of the stream

    if ((pNextEntry != 0) && !pNextEntry->EOS())
    {
        const mkvparser::Block* const pNextBlock = pNextEntry->GetBlock();
        assert(pNextBlock);

        mkvparser::Cluster* const pNextCluster = pNextEntry->GetCluster();

        const __int64 next_ns = pNextBlock->GetTime(pNextCluster);
        assert(next_ns >= curr_ns);

        const LONGLONG sample_duration = (next_ns - curr_ns) / 100;

        hr = pSample->SetSampleDuration(sample_duration);
        assert(SUCCEEDED(hr));
    }

    return S_OK;
}


}  //end namespace WebmMfSourceLib
