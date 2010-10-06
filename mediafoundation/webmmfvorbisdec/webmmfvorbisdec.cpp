#pragma warning(disable:4505)  //unreferenced local function removed
#include "clockable.hpp"
#include <mfidl.h>
#include <list>
#include "vorbis/codec.h"
#include "webmmfvorbisdec.hpp"
#include "webmtypes.hpp"
#include "vorbistypes.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <comdef.h>
#include <cassert>
#include <new>

_COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, __uuidof(IMFMediaBuffer));
_COM_SMARTPTR_TYPEDEF(IMF2DBuffer, __uuidof(IMF2DBuffer));


namespace WebmMfVorbisDecLib
{


HRESULT CreateDecoder(
    IClassFactory* pClassFactory,
    IUnknown* pOuter,
    const IID& iid,
    void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    *ppv = 0;

    if (pOuter)
        return CLASS_E_NOAGGREGATION;

    WebmMfVorbisDec* const p = new (std::nothrow) WebmMfVorbisDec(pClassFactory);

    if (p == 0)
        return E_OUTOFMEMORY;

    IUnknown* const pUnk = p;

    const HRESULT hr = pUnk->QueryInterface(iid, ppv);

    const ULONG cRef = pUnk->Release();
    cRef;

    return hr;
}


WebmMfVorbisDec::WebmMfVorbisDec(IClassFactory* pClassFactory) :
    m_pClassFactory(pClassFactory),
    m_cRef(1),
    m_pInputMediaType(0),
    m_pOutputMediaType(0),
    m_ogg_packet_count(0)
{
    HRESULT hr = m_pClassFactory->LockServer(TRUE);
    assert(SUCCEEDED(hr));

    hr = CLockable::Init();
    assert(SUCCEEDED(hr));

    ::memset(&m_vorbis_info, 0, sizeof vorbis_info);
    ::memset(&m_vorbis_comment, 0, sizeof vorbis_comment);
    ::memset(&m_vorbis_state, 0, sizeof vorbis_dsp_state);
    ::memset(&m_vorbis_block, 0, sizeof vorbis_block);
    ::memset(&m_ogg_packet, 0, sizeof ogg_packet);
}


WebmMfVorbisDec::~WebmMfVorbisDec()
{
    if (m_pInputMediaType)
    {
        const ULONG n = m_pInputMediaType->Release();
        n;
        assert(n == 0);

        m_pInputMediaType = 0;

        DestroyVorbisDecoder();
    }

    if (m_pOutputMediaType)
    {
        const ULONG n = m_pOutputMediaType->Release();
        n;
        assert(n == 0);

        m_pOutputMediaType = 0;
    }

    HRESULT hr = m_pClassFactory->LockServer(FALSE);
    assert(SUCCEEDED(hr));
}


HRESULT WebmMfVorbisDec::QueryInterface(
    const IID& iid,
    void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == __uuidof(IUnknown))
    {
        pUnk = this;  //must be nondelegating
    }
    else if (iid == __uuidof(IMFTransform))
    {
        pUnk = static_cast<IMFTransform*>(this);
    }
    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG WebmMfVorbisDec::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}


ULONG WebmMfVorbisDec::Release()
{
    if (LONG n = InterlockedDecrement(&m_cRef))
        return n;

    delete this;
    return 0;
}


HRESULT WebmMfVorbisDec::GetStreamLimits(
    DWORD* pdwInputMin,
    DWORD* pdwInputMax,
    DWORD* pdwOutputMin,
    DWORD* pdwOutputMax)
{
    if (pdwInputMin == 0)
        return E_POINTER;

    if (pdwInputMax == 0)
        return E_POINTER;

    if (pdwOutputMin == 0)
        return E_POINTER;

    if (pdwOutputMax == 0)
        return E_POINTER;

    DWORD& dwInputMin = *pdwInputMin;
    dwInputMin = 1;

    DWORD& dwInputMax = *pdwInputMax;
    dwInputMax = 1;

    DWORD& dwOutputMin = *pdwOutputMin;
    dwOutputMin = 1;

    DWORD& dwOutputMax = *pdwOutputMax;
    dwOutputMax = 1;

    return S_OK;
}


HRESULT WebmMfVorbisDec::GetStreamCount(
    DWORD* pcInputStreams,
    DWORD* pcOutputStreams)
{
    if (pcInputStreams == 0)
        return E_POINTER;

    if (pcOutputStreams == 0)
        return E_POINTER;

    DWORD& cInputStreams = *pcInputStreams;
    cInputStreams = 1;

    DWORD& cOutputStreams = *pcOutputStreams;
    cOutputStreams = 1;

    return S_OK;
}


HRESULT WebmMfVorbisDec::GetStreamIDs(
    DWORD,
    DWORD* pdwInputIDs,
    DWORD,
    DWORD* pdwOutputIDs)
{
    if (pdwInputIDs)
        *pdwInputIDs = 0;

    if (pdwOutputIDs)
        *pdwOutputIDs = 0;

    return E_NOTIMPL;
}


HRESULT WebmMfVorbisDec::GetInputStreamInfo(
    DWORD dwInputStreamID,
    MFT_INPUT_STREAM_INFO* pStreamInfo)
{
    if (pStreamInfo == 0)
        return E_POINTER;

    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    MFT_INPUT_STREAM_INFO& info = *pStreamInfo;

    //LONGLONG hnsMaxLatency;
    //DWORD dwFlags;
    //DWORD cbSize;
    //DWORD cbMaxLookahead;
    //DWORD cbAlignment;

    info.cbMaxLookahead = 0;
    info.hnsMaxLatency = 0;

    //enum _MFT_INPUT_STREAM_INFO_FLAGS
    // { MFT_INPUT_STREAM_WHOLE_SAMPLES = 0x1,
    // MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER = 0x2,
    // MFT_INPUT_STREAM_FIXED_SAMPLE_SIZE = 0x4,
    // MFT_INPUT_STREAM_HOLDS_BUFFERS    = 0x8,
    // MFT_INPUT_STREAM_DOES_NOT_ADDREF = 0x100,
    // MFT_INPUT_STREAM_REMOVABLE= 0x200,
    // MFT_INPUT_STREAM_OPTIONAL = 0x400,
    // MFT_INPUT_STREAM_PROCESSES_IN_PLACE = 0x800
    // };

    info.dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES |
                   MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER;

    info.cbSize = 0;       //input size is variable
    info.cbAlignment = 0;  //no specific alignment requirements

    return S_OK;
}


HRESULT WebmMfVorbisDec::GetOutputStreamInfo(
    DWORD dwOutputStreamID,
    MFT_OUTPUT_STREAM_INFO* pStreamInfo)
{
    if (pStreamInfo == 0)
        return E_POINTER;

    if (dwOutputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    Lock lock;

    const HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    MFT_OUTPUT_STREAM_INFO& info = *pStreamInfo;

    //enum _MFT_OUTPUT_STREAM_INFO_FLAGS
    //  {   MFT_OUTPUT_STREAM_WHOLE_SAMPLES = 0x1,
       // MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER    = 0x2,
       // MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE   = 0x4,
       // MFT_OUTPUT_STREAM_DISCARDABLE = 0x8,
       // MFT_OUTPUT_STREAM_OPTIONAL    = 0x10,
       // MFT_OUTPUT_STREAM_PROVIDES_SAMPLES    = 0x100,
       // MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES = 0x200,
       // MFT_OUTPUT_STREAM_LAZY_READ   = 0x400,
       // MFT_OUTPUT_STREAM_REMOVABLE   = 0x800
    //  };

    //see Decoder sample in the SDK
    //decoder.cpp

    //The API says that the only flag that is meaningful prior to SetOutputType
    //is the OPTIONAL flag.  We need the channel count, sample rate, and sample
    // size before we can calculate cbSize

    info.dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES |
                   MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
                   MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;

    FrameSize size;

    info.cbSize = GetOutputBufferSize(size);
    info.cbAlignment = 0;

    return S_OK;
}


DWORD WebmMfVorbisDec::GetOutputBufferSize(FrameSize& s) const
{
    //MFT was already locked by caller

    if (m_pInputMediaType == 0)
        return 0; // no media type yet, just return 0

    const DWORD cb = 0;

    //TODO(tomfinegan): return Block align here?

    return cb;
}


HRESULT WebmMfVorbisDec::GetAttributes(IMFAttributes** pp)
{
    if (pp)
        *pp = 0;

    return E_NOTIMPL;
}


HRESULT WebmMfVorbisDec::GetInputStreamAttributes(
    DWORD,
    IMFAttributes** pp)
{
    if (pp)
        *pp = 0;

    return E_NOTIMPL;
}


HRESULT WebmMfVorbisDec::GetOutputStreamAttributes(
    DWORD,
    IMFAttributes** pp)
{
    if (pp)
        *pp = 0;

    return E_NOTIMPL;
}


HRESULT WebmMfVorbisDec::DeleteInputStream(DWORD)
{
    return E_NOTIMPL;
}


HRESULT WebmMfVorbisDec::AddInputStreams(
    DWORD,
    DWORD*)
{
    return E_NOTIMPL;
}


HRESULT WebmMfVorbisDec::GetInputAvailableType(
    DWORD dwInputStreamID,
    DWORD dwTypeIndex,
    IMFMediaType** pp)
{
     if (pp)
        *pp = 0;

    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (dwTypeIndex > 0)
        return MF_E_NO_MORE_TYPES;

    if (pp == 0)
        return E_POINTER;

    IMFMediaType*& pmt = *pp;

    HRESULT hr = MFCreateMediaType(&pmt);
    assert(SUCCEEDED(hr));
    assert(pmt);

    hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    assert(SUCCEEDED(hr));

    hr = pmt->SetGUID(MF_MT_SUBTYPE, VorbisTypes::FORMAT_Vorbis2);
    assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, FALSE);
    assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_COMPRESSED, TRUE);
    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfVorbisDec::GetOutputAvailableType(
    DWORD dwOutputStreamID,
    DWORD dwTypeIndex,
    IMFMediaType** pp)
{
     if (pp)
        *pp = 0;

    if (dwOutputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (dwTypeIndex > 1)
        return MF_E_NO_MORE_TYPES;

    if (pp == 0)
        return E_POINTER;

    IMFMediaType*& pmt = *pp;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    hr = MFCreateMediaType(&pmt);
    assert(SUCCEEDED(hr));
    assert(pmt);

    hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    assert(SUCCEEDED(hr));

    if (dwTypeIndex == 0)
    {
        hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YV12);
        assert(SUCCEEDED(hr));
    }
    else
    {
        assert(dwTypeIndex == 1);

        hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV);
        assert(SUCCEEDED(hr));
    }

    hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
    assert(SUCCEEDED(hr));

    if (m_pInputMediaType == 0)
        return S_OK;

    FrameRate r;

    hr = MFGetAttributeRatio(
            m_pInputMediaType,
            MF_MT_FRAME_RATE,
            &r.numerator,
            &r.denominator);

    if (SUCCEEDED(hr))
    {
        assert(r.denominator);
        assert(r.numerator);

        hr = MFSetAttributeRatio(
                pmt,
                MF_MT_FRAME_RATE,
                r.numerator,
                r.denominator);

        assert(SUCCEEDED(hr));
    }

    FrameSize s;

    hr = MFGetAttributeSize(
            m_pInputMediaType,
            MF_MT_FRAME_SIZE,
            &s.width,
            &s.height);

    assert(SUCCEEDED(hr));
    assert(s.width);
    assert(s.height);

    hr = MFSetAttributeSize(
            pmt,
            MF_MT_FRAME_SIZE,
            s.width,
            s.height);

    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfVorbisDec::SetInputType(
    DWORD dwInputStreamID,
    IMFMediaType* pmt,
    DWORD dwFlags)
{
    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (pmt == 0)
    {
        //TODO: disallow this case while we're playing?

        if (m_pInputMediaType)
        {
            const ULONG n = m_pInputMediaType->Release();
            n;
            assert(n == 0);

            m_pInputMediaType = 0;

            DestroyVorbisDecoder();
        }

        if (m_pOutputMediaType)
        {
            const ULONG n = m_pOutputMediaType->Release();
            n;
            assert(n == 0);

            m_pOutputMediaType = 0;
        }

        return S_OK;
    }

    //TODO: handle the case when already have an input media type
    //or output media type, or are already playing.  I don't
    //think we can change media types while we're playing.

    GUID g;

    hr = pmt->GetMajorType(&g);

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    if (g != MFMediaType_Audio)
        return MF_E_INVALIDMEDIATYPE;

    hr = pmt->GetGUID(MF_MT_SUBTYPE, &g);

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    if (g != VorbisTypes::FORMAT_Vorbis2)
        return MF_E_INVALIDMEDIATYPE;

    UINT32 channels;
    hr = pmt->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);

    if (FAILED(hr))
      return hr;

    if (channels == 0)
        return MF_E_INVALIDMEDIATYPE;

    // checking both rate fields seems excessive...
    double sample_rate;

    hr = pmt->GetDouble(MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND, &sample_rate);

    if (FAILED(hr))
      return hr;

    if (sample_rate < 1)
        return MF_E_INVALIDMEDIATYPE;

    // but why not, check them both:
    UINT32 int_rate;

    hr = pmt->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &int_rate);

    if (FAILED(hr))
      return hr;

    if (int_rate < 1)
        return MF_E_INVALIDMEDIATYPE;

    if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (m_pInputMediaType)
    {
        hr = m_pInputMediaType->DeleteAllItems();
        assert(SUCCEEDED(hr));

        DestroyVorbisDecoder();
    }
    else
    {
        hr = MFCreateMediaType(&m_pInputMediaType);

        if (FAILED(hr))
            return hr;
    }

    hr = pmt->CopyAllItems(m_pInputMediaType);

    if (FAILED(hr))
        return hr;

    hr = CreateVorbisDecoder(pmt);

    if (hr != S_OK)
      return E_FAIL;

    //TODO: resolve this
    assert(m_pOutputMediaType == 0);

    //TODO:
    //We could update the preferred ("available") output media types,
    //now that we know the frame rate and frame size, etc.

    hr = MFCreateMediaType(&m_pOutputMediaType);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
    assert(SUCCEEDED(hr));  //TODO

    return S_OK;
}


HRESULT WebmMfVorbisDec::SetOutputType(
    DWORD dwOutputStreamID,
    IMFMediaType* pmt,
    DWORD dwFlags)
{
    if (dwOutputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (pmt == 0)
    {
        //TODO: disallow this case while we're playing?

        if (m_pOutputMediaType)
        {
            const ULONG n = m_pOutputMediaType->Release();
            n;
            assert(n == 0);

            m_pOutputMediaType = 0;
        }

        return S_OK;
    }

    if (m_pInputMediaType == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    GUID g;

    hr = pmt->GetMajorType(&g);

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    if (g != MFMediaType_Video)
        return MF_E_INVALIDMEDIATYPE;

    hr = pmt->GetGUID(MF_MT_SUBTYPE, &g);

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    if ((g != MFVideoFormat_YV12) &&
        (g != MFVideoFormat_IYUV))
    {
        //TODO: add I420 support

        return MF_E_INVALIDMEDIATYPE;
    }

    //hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    //assert(SUCCEEDED(hr));

    //hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
    //assert(SUCCEEDED(hr));

    FrameRate r_out;

    hr = MFGetAttributeRatio(
            pmt,
            MF_MT_FRAME_RATE,
            &r_out.numerator,
            &r_out.denominator);

    if (SUCCEEDED(hr))
    {
        FrameRate r_in;

        hr = MFGetAttributeRatio(
                m_pInputMediaType,
                MF_MT_FRAME_RATE,
                &r_in.numerator,
                &r_in.denominator);

        if (SUCCEEDED(hr))
        {
            if (r_out.denominator == 0)
                return MF_E_INVALIDMEDIATYPE;

            if (r_out.numerator == 0)
                return MF_E_INVALIDMEDIATYPE;

            const UINT64 n_in = r_in.numerator;
            const UINT64 d_in = r_in.denominator;

            const UINT64 n_out = r_out.numerator;
            const UINT64 d_out = r_out.denominator;

            // n_in / d_in = n_out / d_out
            // n_in * d_out = n_out * d_in

            if ((n_in * d_out) != (n_out * d_in))
                return MF_E_INVALIDMEDIATYPE;
        }
    }

    FrameSize s_out;

    hr = MFGetAttributeSize(
            pmt,
            MF_MT_FRAME_SIZE,
            &s_out.width,
            &s_out.height);

    if (SUCCEEDED(hr))
    {
        if (s_out.width == 0)
            return MF_E_INVALIDMEDIATYPE;

        //TODO: check whether width is odd

        if (s_out.height == 0)
            return MF_E_INVALIDMEDIATYPE;

        FrameSize s_in;

        hr = MFGetAttributeSize(
                m_pInputMediaType,
                MF_MT_FRAME_SIZE,
                &s_in.width,
                &s_in.height);

        assert(SUCCEEDED(hr));

        if (s_out.width != s_in.width)
            return MF_E_INVALIDMEDIATYPE;

        if (s_out.height != s_in.height)
            return MF_E_INVALIDMEDIATYPE;
    }

    if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (m_pOutputMediaType)
    {
        hr = m_pOutputMediaType->DeleteAllItems();
        assert(SUCCEEDED(hr));
    }
    else
    {
        hr = MFCreateMediaType(&m_pOutputMediaType);

        if (FAILED(hr))
            return hr;
    }

    hr = pmt->CopyAllItems(m_pOutputMediaType);

    if (FAILED(hr))
        return hr;

    //TODO: do something

    return S_OK;
}


HRESULT WebmMfVorbisDec::GetInputCurrentType(
    DWORD dwInputStreamID,
    IMFMediaType** pp)
{
    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (pp)
        *pp = 0;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pInputMediaType == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    IMFMediaType*& p = *pp;

    hr = MFCreateMediaType(&p);

    if (FAILED(hr))
        return hr;

    return m_pInputMediaType->CopyAllItems(p);
}


HRESULT WebmMfVorbisDec::GetOutputCurrentType(
    DWORD dwOutputStreamID,
    IMFMediaType** pp)
{
    if (dwOutputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (pp)
        *pp = 0;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //TODO: synthesize from input media type?

    if (m_pOutputMediaType == 0)  //TODO: liberalize?
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    IMFMediaType*& p = *pp;

    hr = MFCreateMediaType(&p);

    if (FAILED(hr))
        return hr;

    return m_pOutputMediaType->CopyAllItems(p);
}


HRESULT WebmMfVorbisDec::GetInputStatus(
    DWORD dwInputStreamID,
    DWORD* pdwFlags)
{
    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pInputMediaType == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    //TODO: check output media type too?

    if (pdwFlags == 0)
        return E_POINTER;

    DWORD& dwFlags = *pdwFlags;

#if 0
    const vpx_image_t* const f = vpx_codec_get_frame(&m_ctx, &m_iter);

    dwFlags = (f == 0) ? MFT_INPUT_STATUS_ACCEPT_DATA : 0;
#else
    dwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;  //because we always queue
#endif

    return S_OK;
}


HRESULT WebmMfVorbisDec::GetOutputStatus(DWORD*)
{
#if 1
    return E_NOTIMPL;
#else
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pInputMediaType == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    //TODO: check output media type too?

    if (pdwFlags == 0)
        return E_POINTER;

    DWORD& dwFlags = *pdwFlags;

    const vpx_image_t* const f = vpx_codec_get_frame(&m_ctx, &m_iter);

    dwFlags = f ? MFT_OUTPUT_STATUS_SAMPLE_READY : 0;

    //TODO: alternatively, we could return E_NOTIMPL, which
    //forces client to call ProcessOutput to determine whether
    //a sample is ready.

    return S_OK;
#endif
}


HRESULT WebmMfVorbisDec::SetOutputBounds(
    LONGLONG /* hnsLowerBound */ ,
    LONGLONG /* hnsUpperBound */ )
{
    return E_NOTIMPL;  //TODO
}


HRESULT WebmMfVorbisDec::ProcessEvent(
    DWORD dwInputStreamID,
    IMFMediaEvent*)
{
    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    return E_NOTIMPL;  //TODO
}


HRESULT WebmMfVorbisDec::ProcessMessage(
    MFT_MESSAGE_TYPE,
    ULONG_PTR)
{
    return S_OK;  //TODO
}


HRESULT WebmMfVorbisDec::ProcessInput(
    DWORD dwInputStreamID,
    IMFSample* pSample,
    DWORD)
{
    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (pSample == 0)
        return E_INVALIDARG;

    DWORD count;

    HRESULT hr = pSample->GetBufferCount(&count);
    assert(SUCCEEDED(hr));

    if (count == 0)
        return S_OK;  //TODO: is this an error?

    if (count > 1)
        return E_INVALIDARG;

    //TODO: check duration
    //TODO: check timestamp

    Lock lock;

    hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pInputMediaType == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    pSample->AddRef();
    m_samples.push_back(pSample);

    return S_OK;  //TODO!
}


HRESULT WebmMfVorbisDec::ProcessOutput(
    DWORD dwFlags,
    DWORD cOutputBufferCount,
    MFT_OUTPUT_DATA_BUFFER* pOutputSamples,
    DWORD* pdwStatus)
{
    if (pdwStatus)
        *pdwStatus = 0;

    if (dwFlags)
        return E_INVALIDARG;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pInputMediaType == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (m_samples.empty())
        return MF_E_TRANSFORM_NEED_MORE_INPUT;

    if (cOutputBufferCount == 0)
        return E_INVALIDARG;

    if (pOutputSamples == 0)
        return E_INVALIDARG;

    //TODO: check if cOutputSamples > 1 ?

    MFT_OUTPUT_DATA_BUFFER& data = pOutputSamples[0];

    //data.dwStreamID should equal 0, but we ignore it

    IMFSample* const pSample_out = data.pSample;

    if (pSample_out == 0)
        return E_INVALIDARG;

    DWORD count;

    hr = pSample_out->GetBufferCount(&count);

    if (SUCCEEDED(hr) && (count != 1))
        return E_INVALIDARG;

    IMFMediaBufferPtr buf_out;

    hr = pSample_out->GetBufferByIndex(0, &buf_out);

    if (FAILED(hr) || !bool(buf_out))
        return E_INVALIDARG;

    IMFSample* const pSample_in = m_samples.front();
    assert(pSample_in);

    m_samples.pop_front();

    IMFMediaBufferPtr buf_in;

    hr = pSample_in->GetBufferByIndex(0, &buf_in);
    assert(SUCCEEDED(hr));
    assert(buf_in);

    BYTE* ptr;
    DWORD len;

    hr = buf_in->Lock(&ptr, 0, &len);
    assert(SUCCEEDED(hr));
    assert(ptr);
    assert(len);

    const vpx_codec_err_t e = vpx_codec_decode(&m_ctx, ptr, len, 0, 0);
    assert(e == VPX_CODEC_OK);  //TODO

    hr = buf_in->Unlock();
    assert(SUCCEEDED(hr));

    assert(m_pOutputMediaType);

    GUID subtype;

    hr = m_pOutputMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
    assert(SUCCEEDED(hr));
    assert((subtype == MFVideoFormat_YV12) || (subtype == MFVideoFormat_IYUV));

    //TODO:
    //MFVideoFormat_I420

    const DWORD fcc = subtype.Data1;

    FrameSize frame_size;

    const DWORD cbFrameLen = GetOutputBufferSize(frame_size);
    assert(cbFrameLen > 0);

    //The sequence of querying for the output buffer is described on
    //the page "Uncompressed Video Buffers".
    //http://msdn.microsoft.com/en-us/library/aa473821%28v=VS.85%29.aspx

    IMF2DBuffer* buf2d_out;

    hr = buf_out->QueryInterface(&buf2d_out);

    if (SUCCEEDED(hr))
    {
        assert(buf2d_out);

        LONG stride_out;

        hr = buf2d_out->Lock2D(&ptr, &stride_out);
        assert(SUCCEEDED(hr));
        assert(ptr);
        assert(stride_out > 0);  //top-down DIBs are positive, right?

        hr = GetFrame(ptr, stride_out, subtype);
        assert(SUCCEEDED(hr));

        //TODO: set output buffer length?

        hr = buf2d_out->Unlock2D();
        assert(SUCCEEDED(hr));

        buf2d_out->Release();
        buf2d_out = 0;
    }
    else
    {
        DWORD cbMaxLen;

        hr = buf_out->Lock(&ptr, &cbMaxLen, 0);
        assert(SUCCEEDED(hr));
        assert(ptr);

        assert(cbMaxLen >= cbFrameLen);

        //TODO: verify stride of output buffer
        //The page "Uncompressed Video Buffers" here:
        //http://msdn.microsoft.com/en-us/library/aa473821%28v=VS.85%29.aspx
        //explains how to calculate the "minimum stride":
        //  MF_MT_DEFAULT_STRIDE
        //  or, MFGetStrideForBitmapInfoHader
        //  or, calculate it yourself

        INT32 stride_out;

        hr = pSample_out->GetUINT32(
                MF_MT_DEFAULT_STRIDE,
                (UINT32*)&stride_out);

        if (SUCCEEDED(hr) && (stride_out != 0))
            assert(stride_out > 0);
        else
        {
            const DWORD w = frame_size.width;
            LONG stride_out_;

            hr = MFGetStrideForBitmapInfoHeader(fcc, w, &stride_out_);

            if (SUCCEEDED(hr) && (stride_out_ != 0))
            {
                assert(stride_out_ > 0);
                stride_out = stride_out_;
            }
            else
            {
                assert((w % 2) == 0);  //TODO
                stride_out = w;  //TODO: is this correct???
            }
        }

        hr = GetFrame(ptr, stride_out, subtype);
        assert(SUCCEEDED(hr));

        hr = buf_out->SetCurrentLength(cbFrameLen);
        assert(SUCCEEDED(hr));

        hr = buf_out->Unlock();
        assert(SUCCEEDED(hr));
    }

    LONGLONG t;

    hr = pSample_in->GetSampleTime(&t);

    if (SUCCEEDED(hr))
    {
        assert(t >= 0);

        hr = pSample_out->SetSampleTime(t);
        assert(SUCCEEDED(hr));
    }

    hr = pSample_in->GetSampleDuration(&t);

    if (SUCCEEDED(hr))
    {
        assert(t >= 0);  //TODO: move this into predicate?

        hr = pSample_out->SetSampleDuration(t);
        assert(SUCCEEDED(hr));
    }

    pSample_in->Release();

    return S_OK;
}


HRESULT WebmMfVorbisDec::GetFrame(
    BYTE* pOutBuf,
    ULONG strideOut,
    const GUID& subtype)
{
    assert(pOutBuf);
    assert(strideOut);
    assert((strideOut % 2) == 0);  //TODO: resolve this issue

    vpx_codec_iter_t iter = 0;

    const vpx_image_t* f = vpx_codec_get_frame(&m_ctx, &iter);
    assert(f);  //TODO: this will fail if alt-ref frame ("invisible")

    //Y

    const BYTE* pInY = f->planes[PLANE_Y];
    assert(pInY);

    unsigned int wIn = f->d_w;
    unsigned int hIn = f->d_h;

    BYTE* pOut = pOutBuf;

    const int strideInY = f->stride[PLANE_Y];

    for (unsigned int y = 0; y < hIn; ++y)
    {
        memcpy(pOut, pInY, wIn);
        pInY += strideInY;
        pOut += strideOut;
    }

    strideOut /= 2;

    wIn = (wIn + 1) / 2;
    hIn = (hIn + 1) / 2;

    const BYTE* pInV = f->planes[PLANE_V];
    assert(pInV);

    const int strideInV = f->stride[PLANE_V];

    const BYTE* pInU = f->planes[PLANE_U];
    assert(pInU);

    const int strideInU = f->stride[PLANE_U];

    if (subtype == MFVideoFormat_YV12)
    {
        //V

        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInV, wIn);
            pInV += strideInV;
            pOut += strideOut;
        }

        //U

        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInU, wIn);
            pInU += strideInU;
            pOut += strideOut;
        }
    }
    else
    {
        //U

        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInU, wIn);
            pInU += strideInU;
            pOut += strideOut;
        }

        //V

        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInV, wIn);
            pInV += strideInV;
            pOut += strideOut;
        }
    }

    f = vpx_codec_get_frame(&m_ctx, &iter);
    assert(f == 0);

    return S_OK;
}

HRESULT WebmMfVorbisDec::NextOggPacket(BYTE* p_packet, DWORD packet_size)
{
    if (!p_packet || packet_size == 0)
        return E_INVALIDARG;

    m_ogg_packet.b_o_s = 0;
    m_ogg_packet.bytes = packet_size;
    m_ogg_packet.e_o_s = 0;
    m_ogg_packet.granulepos = 0;
    m_ogg_packet.packet = p_packet;
    m_ogg_packet.packetno = m_ogg_packet_count++;

    return S_OK;
}

HRESULT WebmMfVorbisDec::CreateVorbisDecoder(IMFMediaType* p_media_type)
{
    //
    // p_media_type has already been used extensively in the caller
    BYTE* p_format_blob = NULL;
    UINT32 blob_size = 0;
    status = p_media_type->GetAllocatedBlob(MF_MT_USER_DATA, p_format_blob,
                                            &blob_size);
    if (S_OK != status)
      return MF_E_INVALIDMEDIATYPE;
    if (NULL == p_format_blob)
      return MF_E_INVALIDMEDIATYPE;

    using VorbisTypes::VORBISFORMAT2;
    if (0 == blob_size || sizeof VORBISFORMAT2 >= blob_size)
      return MF_E_INVALIDMEDIATYPE;

    const VORBISFORMAT2& vorbis_format =
        *reinterpret_cast<VORBISFORMAT2*>(p_format_blob);

    BYTE* p_headers[3];
    p_headers[0] = p_format_blob + sizeof VORBISFORMAT2;
    p_headers[1] = p_headers[0] + vorbis_format.headerSize[0];
    p_headers[2] = p_headers[1] + vorbis_format.headerSize[1];

    vorbis_info_init(&m_vorbis_info);
    vorbis_comment_init(&m_vorbis_comment);

    // feed the 3 setup headers into libvorbis
    int vorbis_status = 0;
    for (header_num = 0; header_num < 3; ++header_num)
    {
        // create an ogg packet in m_ogg_packet with current header for data
        status = NextOggPacket(p_headers[header_num],
                               vorbis_format.headerSize[header_num]);
        if (FAILED(status))
          return MF_E_INVALIDMEDIATYPE;
        assert(m_ogg_packet.packetno == header_num);
        int vorbis_status = vorbis_synthesis_headerin(&m_vorbis_info,
                                                      &m_vorbis_comment,
                                                      &m_ogg_packet);
        if (vorbis_status < 0)
          return MF_E_INVALIDMEDIATYPE;
    }

    // final init steps, setup decoder state and vorbis block structs
    vorbis_status = vorbis_synthesis_init(&m_vorbis_state, &m_vorbis_info);
    if (vorbis_status != 0)
        return MF_E_INVALIDMEDIATYPE;

    vorbis_status = vorbis_block_init(&m_vorbis_state, &m_vorbis_block);
    if (vorbis_status != 0)
        return MF_E_INVALIDMEDIATYPE;

    return S_OK;
}

void WebmMfVorbisDec::DestroyVorbisDecoder()
{
    vorbis_block_clear(&m_vorbis_block);
    vorbis_dsp_clear(&m_vorbis_state);
    vorbis_comment_clear(&m_vorbis_comment);

    // note, from vorbis decoder sample: vorbis_info_clear must be last call
    vorbis_info_clear(&m_vorbis_info);

    ::memset(&m_vorbis_info, 0, sizeof vorbis_info);
    ::memset(&m_vorbis_comment, 0, sizeof vorbis_comment);
    ::memset(&m_vorbis_state, 0, sizeof vorbis_dsp_state);
    ::memset(&m_vorbis_block, 0, sizeof vorbis_block);
    ::memset(&m_ogg_packet, 0, sizeof ogg_packet);
}

}  //end namespace WebmMfVorbisDecLib
