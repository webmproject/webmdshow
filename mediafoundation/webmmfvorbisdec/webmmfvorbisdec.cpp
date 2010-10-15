#pragma warning(disable:4505)  //unreferenced local function removed
#include <mfidl.h>
#include <list>
#include <vector>
#include <mfapi.h>
#include <mferror.h>
#include <comdef.h>
#include <cassert>
#include <new>
#include <cmath>

#include "clockable.hpp"
#include "vorbis/codec.h"
#include "webmtypes.hpp"
#include "vorbistypes.hpp"
#include "webmmfvorbisdec.hpp"

// keep the compiler quiet about do/while(0)'s used in log macros
#pragma warning(disable:4127)

#ifdef _DEBUG
#include "odbgstream.hpp"
#include "iidstr.hpp"
using std::endl;

#define DBGLOG(X) \
do { \
    wodbgstream wos; \
    wos << "["__FUNCTION__"] " << X << endl; \
} while(0)
#else
#define DBGLOG(X) do {} while(0)
#endif

_COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, __uuidof(IMFMediaBuffer));

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

    WebmMfVorbisDec* const p =
        new (std::nothrow) WebmMfVorbisDec(pClassFactory);

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
    m_ogg_packet_count(0),
    m_total_time_decoded(0)
    //m_audio_format_tag(WAVE_FORMAT_IEEE_FLOAT)
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
    //::memset(&m_wave_format, 0, sizeof WAVEFORMATEX);
}

WebmMfVorbisDec::~WebmMfVorbisDec()
{
    if (m_input_mediatype)
        DestroyVorbisDecoder();

    HRESULT hr = m_pClassFactory->LockServer(FALSE);
    hr;
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

    // This MFT has a fixed number of streams.
    *pdwInputMin = 1;
    *pdwInputMax = 1;
    *pdwOutputMin = 1;
    *pdwOutputMax = 1;

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

    // This MFT has a fixed number of streams.
    *pcInputStreams = 1;
    *pcOutputStreams = 1;

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

    HRESULT hr = lock.Seize(this);

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
    //size before we can calculate cbSize.

    info.dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES |
                   MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER;
                   //MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
                   //MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;


    if (m_output_mediatype == 0)
        info.cbSize = 0;
    else
    {
        UINT32 bytes_per_sec;

        hr = m_output_mediatype->GetUINT32(
                MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
                &bytes_per_sec);

        if (SUCCEEDED(hr))
            info.cbSize = bytes_per_sec / 2;  //budget for 0.5 sec
        else
            info.cbSize = 0;
    }

    info.cbAlignment = 0;

    return S_OK;
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

    hr = pmt->SetGUID(MF_MT_SUBTYPE, VorbisTypes::MEDIASUBTYPE_Vorbis2);
    assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
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

    if (FAILED(hr))
        return hr;

    if (pmt == 0)  //weird
        return E_OUTOFMEMORY;

    hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    assert(SUCCEEDED(hr));

    if (dwTypeIndex == 0)
    {
        hr = pmt->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        assert(SUCCEEDED(hr));
    }
    else
    {
        hr = pmt->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        assert(SUCCEEDED(hr));
    }

    //TODO: need this?
    //MT_MF_ORIGINAL_WAVE_FORMAT_TAG

    hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
    assert(SUCCEEDED(hr));

    if (m_input_mediatype == 0)
        return S_OK;

    const int channels = m_vorbis_info.channels;
    assert(channels > 0);

    hr = pmt->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
    assert(SUCCEEDED(hr));

    UINT32 bytes_per_sample;

    if (dwTypeIndex == 0)  //IEEE_Float
        bytes_per_sample = sizeof(float);
    else
        bytes_per_sample = 2;  //TODO: handle 1-byte PCM

    const UINT32 bits_per_sample = 8 * bytes_per_sample;

    hr = pmt->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bits_per_sample);
    assert(SUCCEEDED(hr));

    const UINT32 block_align = channels * bytes_per_sample;

    hr = pmt->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, block_align);
    assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
    assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_SAMPLE_SIZE, block_align);
    assert(SUCCEEDED(hr));

    const long samples_per_sec = m_vorbis_info.rate;
    assert(samples_per_sec > 0);

    hr = pmt->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, samples_per_sec);
    assert(SUCCEEDED(hr));

    //TODO: need this too?
    //MF_MT_AUIOD_FLOAT_SAMPLES_PER_SEC

    const UINT32 avg_bytes_per_sec = block_align * samples_per_sec;

    hr = pmt->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, avg_bytes_per_sec);
    assert(SUCCEEDED(hr));

    //http://msdn.microsoft.com/en-us/library/aa376629%28v=VS.85%29.aspx

    if (channels == 6)  //TODO: handle other channel combinations
    {
        const UINT32 mask = SPEAKER_FRONT_LEFT |
                            SPEAKER_FRONT_RIGHT |
                            SPEAKER_FRONT_CENTER |
                            SPEAKER_LOW_FREQUENCY |
                            SPEAKER_BACK_LEFT |
                            SPEAKER_BACK_RIGHT;

        hr = pmt->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, mask);
        assert(SUCCEEDED(hr));
    }

    //TODO
    //MF_MT_AUDIO_FOLDDOWN_MATRIX
    //MF_MT_AUDIO_SAMPLES_PER_BLOCK
    //MF_MT_AUDIO_PREFER_WAVEFORMATEX

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

        m_input_mediatype = 0;
        DestroyVorbisDecoder();

        m_output_mediatype = 0;

        return S_OK;
    }

    //TODO: handle the case when already have an input media type
    //or output media type, or are already playing.  I don't
    //think we can change media types while we're playing.

    GUID g;

    hr = pmt->GetMajorType(&g);

    if (FAILED(hr) || (g != MFMediaType_Audio))
        return MF_E_INVALIDMEDIATYPE;

    hr = pmt->GetGUID(MF_MT_SUBTYPE, &g);

    if (FAILED(hr) || (g != VorbisTypes::MEDIASUBTYPE_Vorbis2))
        return MF_E_INVALIDMEDIATYPE;

    UINT32 channels;

    hr = pmt->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);

    if (FAILED(hr) || (channels == 0))
        return MF_E_INVALIDMEDIATYPE;

    UINT32 samples_per_sec;

    //TODO: assume for now we have integer sampling rate
    hr = pmt->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samples_per_sec);

    if (FAILED(hr) || (samples_per_sec == 0))
        return MF_E_INVALIDMEDIATYPE;

    //TODO: get blob

    if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (m_input_mediatype)
    {
        m_input_mediatype->DeleteAllItems();
        DestroyVorbisDecoder();
    }
    else
    {
        hr = MFCreateMediaType(&m_input_mediatype);

        if (FAILED(hr))
            return hr;
    }

    hr = pmt->CopyAllItems(m_input_mediatype);

    if (FAILED(hr))
        return hr;

    hr = CreateVorbisDecoder();

    if (hr != S_OK)
        return E_FAIL;

    m_output_mediatype = 0;

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

        m_output_mediatype = 0;
        return S_OK;
    }

    if (m_input_mediatype == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    GUID g;

    hr = pmt->GetMajorType(&g);

    if (FAILED(hr) || (g != MFMediaType_Audio))
        return MF_E_INVALIDMEDIATYPE;

    hr = pmt->GetGUID(MF_MT_SUBTYPE, &g);

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    if (g == MFAudioFormat_Float)
        __noop;
    else if (g == MFAudioFormat_PCM)
        __noop;
    else
        return MF_E_INVALIDMEDIATYPE;

    UINT32 channels;

    hr = pmt->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);

    if (FAILED(hr) || (channels != UINT32(m_vorbis_info.channels)))
        return MF_E_INVALIDMEDIATYPE;

    UINT32 samples_per_sec;

    //TODO: assume for now we have integer sampling rate
    hr = pmt->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samples_per_sec);

    if (FAILED(hr) || (samples_per_sec != UINT32(m_vorbis_info.rate)))
        return MF_E_INVALIDMEDIATYPE;

    //TODO: anything else that needs to be vetted

    if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (m_output_mediatype)
    {
        hr = m_output_mediatype->DeleteAllItems();
        assert(SUCCEEDED(hr));
    }
    else
    {
        hr = MFCreateMediaType(&m_output_mediatype);

        if (FAILED(hr))
            return hr;
    }

    hr = pmt->CopyAllItems(m_output_mediatype);

    if (FAILED(hr))
        return hr;

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

    if (m_input_mediatype == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    IMFMediaType*& p = *pp;

    hr = MFCreateMediaType(&p);

    if (FAILED(hr))
        return hr;

    return m_input_mediatype->CopyAllItems(p);
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

    if (m_output_mediatype == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    IMFMediaType*& p = *pp;

    hr = MFCreateMediaType(&p);

    if (FAILED(hr))
        return hr;

    return m_output_mediatype->CopyAllItems(p);
}

HRESULT WebmMfVorbisDec::GetInputStatus(DWORD dwInputStreamID, DWORD* pdwFlags)
{
    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_input_mediatype == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (pdwFlags == 0)
        return E_POINTER;

    DWORD& dwFlags = *pdwFlags;

    dwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;  //because we always queue

    return S_OK;
}

HRESULT WebmMfVorbisDec::GetOutputStatus(DWORD*)
{
    return E_NOTIMPL;
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

    return E_NOTIMPL;  //tell caller to push this event downstream
}

HRESULT WebmMfVorbisDec::ProcessMessage(MFT_MESSAGE_TYPE message, ULONG_PTR)
{
    switch (message)
    {
    case MFT_MESSAGE_COMMAND_FLUSH:
        DBGLOG("MFT_MESSAGE_COMMAND_FLUSH");
        m_total_time_decoded = 0;
        vorbis_synthesis_restart(&m_vorbis_state);
        m_output_samples.clear();

        while (!m_samples.empty())
        {
            IMFSample* const pSample = m_samples.front();
            assert(pSample);

            m_samples.pop_front();

            pSample->Release();
        }

        return S_OK;

    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
        DBGLOG("MFT_MESSAGE_NOTIFY_BEGIN_STREAMING");
        return S_OK;

    case MFT_MESSAGE_NOTIFY_END_STREAMING:
        DBGLOG("MFT_MESSAGE_NOTIFY_END_STREAMING");
        return S_OK;

    case MFT_MESSAGE_COMMAND_DRAIN:
        // Drain: Tells the MFT not to accept any more input until
        // all of the pending output has been processed.
        DBGLOG("MFT_MESSAGE_COMMAND_DRAIN");
        return S_OK;

    case MFT_MESSAGE_SET_D3D_MANAGER:
        // The pipeline should never send this message unless the MFT
        // has the MF_SA_D3D_AWARE attribute set to TRUE. However, if we
        // do get this message, it's invalid and we don't implement it.
        DBGLOG("MFT_MESSAGE_SET_D3D_MANAGER");
        return E_NOTIMPL;

    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
        DBGLOG("MFT_MESSAGE_NOTIFY_END_OF_STREAM");
        return S_OK;

    case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
        DBGLOG("MFT_MESSAGE_NOTIFY_START_OF_STREAM");
        return S_OK;

    default:
        return S_OK;
    }
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

    HRESULT status = pSample->GetBufferCount(&count);
    assert(SUCCEEDED(status));

    if (count == 0)
        return S_OK;  //TODO: is this an error?

    if (count > 1)
        return E_INVALIDARG;

    //TODO: check duration
    //TODO: check timestamp

    Lock lock;

    status = lock.Seize(this);

    if (FAILED(status))
        return status;

    if (m_input_mediatype == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    pSample->AddRef();
    m_samples.push_back(pSample);

    return S_OK;
}

HRESULT WebmMfVorbisDec::Decode(IMFSample* pSample)
{
    assert(pSample);

    IMFMediaBufferPtr pBuffer;

    HRESULT hr = pSample->GetBufferByIndex(0, &pBuffer);

    if (FAILED(hr))
        return E_INVALIDARG;

    BYTE* ptr;
    DWORD max_len, len;

    hr = pBuffer->Lock(&ptr, &max_len, &len);

    if (FAILED(hr) || (ptr == 0) || (len == 0))
        return E_FAIL;

    NextOggPacket(ptr, len);

    // start decoding the chunk of vorbis data we just wrapped in an ogg packet
    int vorbis_status = vorbis_synthesis(&m_vorbis_block, &m_ogg_packet);
    // TODO(tomfinegan): will vorbis_synthesis ever return non-zero?
    assert(vorbis_status == 0);
    if (vorbis_status != 0)
        return E_FAIL;

    vorbis_status = vorbis_synthesis_blockin(&m_vorbis_state, &m_vorbis_block);
    assert(vorbis_status == 0);
    if (vorbis_status != 0)
      return E_FAIL;

    hr = pBuffer->Unlock();
    assert(SUCCEEDED(hr));

    return S_OK;
}

namespace
{
    INT16 clip16(int val)
    {
        if (val > 32767)
        {
            val = 32767;
        }
        else if (val < -32768)
        {
            val = -32768;
        }

        return static_cast<INT16>(val);
    }
} // end anon namespace

HRESULT WebmMfVorbisDec::ProcessOutputSamples(
    IMFSample* pSample,
    int& samples_decoded)
{
    assert(pSample);

    // try to get a buffer from the output sample...
    IMFMediaBufferPtr pBuffer;

    HRESULT hr = pSample->GetBufferByIndex(0, &pBuffer);
    assert(SUCCEEDED(hr));
    assert(pBuffer);

    // Consume all PCM samples from libvorbis
    for (;;)
    {
        float** pp_pcm;
        const int samples = vorbis_synthesis_pcmout(&m_vorbis_state, &pp_pcm);

        if (samples <= 0)
            break;

        for (int sample = 0; sample < samples; ++sample)
        {
            for (int channel = 0; channel < m_vorbis_info.channels; ++channel)
            {
                m_output_samples.push_back(pp_pcm[channel][sample]);
            }
        }

        vorbis_synthesis_read(&m_vorbis_state, samples);
    }

    // Need more input if libvorbis didn't produce any output samples
    if (m_output_samples.empty())
        return MF_E_TRANSFORM_NEED_MORE_INPUT;

#if 0  //TODO
    const int total_samples = static_cast<int>(m_output_samples.size());

    UINT32 sample_size;  //bytes/sample

    hr = m_output_mediatype->GetUINT32(MF_MT_SAMPLE_SIZE, &sample_size);
    assert(SUCCEEDED(hr));
    assert(sample_size > 0);

    const UINT32 storage_space_needed = total_samples * sample_size;

    DWORD max_len;

    hr = pBuffer->GetMaxLength(&max_len);
    assert(SUCCEEDED(hr));
    assert(storage_space_needed <= max_len);  //TODO

    BYTE* p_mf_buffer_data = NULL;
    DWORD mf_data_len = 0;

    if (mf_storage_space_needed > mf_storage_limit)
    {
        assert(mf_storage_limit >= mf_storage_space_needed);
        DBGLOG("Reallocating buffer: mf_storage_limit=" << mf_storage_limit
          << " mf_storage_space_needed=" << mf_storage_space_needed);

        // TODO(tomfinegan): do I have to queue extra data, or is it alright to
        //                   replace the sample buffer?
        status = p_mf_output_sample->RemoveBufferByIndex(0);
        assert(SUCCEEDED(status));
        if (FAILED(status))
            return status;

        IMFMediaBuffer* p_buffer = NULL;
        status = CreateMediaBuffer(mf_storage_space_needed, &p_buffer);

        if (FAILED(status))
            return status;

        mf_output_buffer = p_buffer;
    }
#endif

    BYTE* dst;
    DWORD max_len;

    hr = pBuffer->Lock(&dst, &max_len, 0);
    assert(dst);
    //assert(max_len >= storage_space_needed);

    GUID g;

    hr = m_output_mediatype->GetGUID(MF_MT_SUBTYPE, &g);
    assert(SUCCEEDED(hr));

    const output_samples_t::size_type n = m_output_samples.size();

    if (g == MFAudioFormat_Float)
    {
        const size_t size = n * sizeof(float);
        assert(size <= max_len);

        const float* const src = &m_output_samples[0];

        memcpy(dst, src, size);

        hr = pBuffer->SetCurrentLength(size);
        assert(SUCCEEDED(hr));
    }
    else
    {
        assert(g == MFAudioFormat_PCM);

        const size_t size = n * 2;  //TODO: 2 bytes/sample assumed here
        assert(size <= max_len);

        typedef output_samples_t::const_iterator iter_t;

        iter_t iter = m_output_samples.begin();
        const iter_t iter_end = m_output_samples.end();

        // from the vorbis decode sample:
        // |pp_pcm| is a multichannel float vector.  In stereo, for example,
        // pp_pcm[0] is left, and pp_pcm[1] is right.  samples is the size of
        // each channel.  Convert the float values (-1.<=range<=1.) to whatever
        // PCM format and write it out

        // TODO(tomfinegan): factor resample out into 8-bit/16-bit versions

        INT16* const p_out_samples = reinterpret_cast<INT16*>(dst);

        for (int sample = 0; iter != iter_end; ++iter, ++sample)
        {
            p_out_samples[sample] = static_cast<INT16>(clip16((int)
                floor(*iter * 32767.f + .5f)));
        }

        hr = pBuffer->SetCurrentLength(size);
        assert(SUCCEEDED(hr));
    }

    m_output_samples.clear();

    hr = pBuffer->Unlock();
    assert(SUCCEEDED(hr));

    samples_decoded = n / m_vorbis_info.channels;  //TODO: verify this

    return S_OK;
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

    if (m_input_mediatype == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (cOutputBufferCount == 0)
        return E_INVALIDARG;

    if (pOutputSamples == 0)
        return E_INVALIDARG;

    if (m_samples.empty())
        return MF_E_TRANSFORM_NEED_MORE_INPUT;

    //TODO: check if cOutputSamples > 1 ?

    // confirm we have an output sample before we pop an input sample off the
    // front of |m_samples|
    MFT_OUTPUT_DATA_BUFFER& data = pOutputSamples[0];

    //data.dwStreamID should equal 0, but we ignore it

    IMFSample* const pOutputSample = data.pSample;

    if (pOutputSample == 0)
        return E_INVALIDARG;

    DWORD count;

    hr = pOutputSample->GetBufferCount(&count);

    if (SUCCEEDED(hr) && (count != 1))
        return E_INVALIDARG;

    IMFSample* pInputSample = m_samples.front();
    assert(pInputSample);

    hr = Decode(pInputSample);

    if (FAILED(hr))
        return hr;

    LONGLONG time;

    hr = pInputSample->GetSampleTime(&time);
    assert(SUCCEEDED(hr));
    assert(time >= 0);

#if 0
    status = p_mf_input_sample->GetSampleDuration(&duration);
    if (MF_E_NO_SAMPLE_DURATION == status)
    {
        // TODO(tomfinegan): determine how to properly detect end of stream
        m_end_of_stream_reached = true;
        // note: m_end_of_stream_reached is not yet used elsewhere...
        return S_OK;
    }
    assert(SUCCEEDED(status));

    DBGLOG("start_time=" << start_time << " duration=" << duration);
#endif

    m_samples.pop_front();

    pInputSample->Release();
    pInputSample = 0;

    int samples;

    hr = ProcessOutputSamples(pOutputSample, samples);

    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
        return hr;

    assert(SUCCEEDED(hr));

    //TODO
    //time = m_total_time_decoded;

    hr = pOutputSample->SetSampleTime(time);
    assert(SUCCEEDED(hr));

#if 0  //TODO
    // set |p_mf_output_sample| duration to the duration of the pcm samples
    // output by libvorbis
    double dmediatime_decoded = ((double)samples /
                                 (double)m_vorbis_info.rate) *
                                10000000.0f;
    LONGLONG mediatime_decoded = static_cast<LONGLONG>(dmediatime_decoded);

    hr = pOutputSample->SetSampleDuration(mediatime_decoded);
    assert(SUCCEEDED(hr));

    DBGLOG("mediatime_decoded=" << mediatime_decoded);
    DBGLOG("m_total_time_decoded=" << m_total_time_decoded);

    // TODO(tomfinegan): does |m_total_time_decoded| need reset after seeking?
    m_total_time_decoded += mediatime_decoded;
#endif

    return S_OK;
}

void WebmMfVorbisDec::NextOggPacket(BYTE* ptr, DWORD len)
{
    assert(ptr);
    assert(len);

    m_ogg_packet.b_o_s = (m_ogg_packet_count == 0);
    m_ogg_packet.bytes = len;

    // TODO(tomfinegan): implement End Of Stream handling
    m_ogg_packet.e_o_s = 0;
    m_ogg_packet.granulepos = 0;
    m_ogg_packet.packet = ptr;
    m_ogg_packet.packetno = m_ogg_packet_count++;
}

HRESULT WebmMfVorbisDec::CreateVorbisDecoder()
{
    BYTE* ptr;
    UINT32 len;

    IMFMediaType* const pmt = m_input_mediatype;
    assert(pmt);

    HRESULT hr = pmt->GetAllocatedBlob(MF_MT_USER_DATA, &ptr, &len);
    assert(SUCCEEDED(hr));
    assert(ptr);

    using VorbisTypes::VORBISFORMAT2;
    assert(len >= sizeof(VORBISFORMAT2));

    const VORBISFORMAT2& fmt = (VORBISFORMAT2&)(*ptr);

    BYTE* p_headers[3];
    p_headers[0] = ptr + sizeof(VORBISFORMAT2);
    p_headers[1] = p_headers[0] + fmt.headerSize[0];
    p_headers[2] = p_headers[1] + fmt.headerSize[1];

    vorbis_info_init(&m_vorbis_info);
    vorbis_comment_init(&m_vorbis_comment);

    for (int header_num = 0; header_num < 3; ++header_num)
    {
        assert(fmt.headerSize[header_num] > 0);

        // create an ogg packet in m_ogg_packet with current header for data
        NextOggPacket(p_headers[header_num], fmt.headerSize[header_num]);
        assert(m_ogg_packet.packetno == header_num);

        const int status = vorbis_synthesis_headerin(&m_vorbis_info,
                                                     &m_vorbis_comment,
                                                     &m_ogg_packet);
        assert(status >= 0);
    }

    // final init steps, setup decoder state...
    int status = vorbis_synthesis_init(&m_vorbis_state, &m_vorbis_info);
    assert(status == 0);

    // ... and vorbis block structs
    status = vorbis_block_init(&m_vorbis_state, &m_vorbis_block);
    assert(status == 0);

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

    while (!m_samples.empty())
    {
        IMFSample* const pSample = m_samples.front();
        assert(pSample);

        m_samples.pop_front();

        pSample->Release();
    }

    m_ogg_packet_count = 0;
    m_total_time_decoded = 0;
}

#if 0
HRESULT WebmMfVorbisDec::ValidatePcmAudioType(IMFMediaType *pmt)
{
    HRESULT status = S_OK;
    GUID majorType = GUID_NULL;
    GUID subtype = GUID_NULL;

    UINT32 nChannels = 0;
    UINT32 nSamplesPerSec = 0;
    UINT32 nAvgBytesPerSec = 0;
    UINT32 nBlockAlign = 0;
    UINT32 wBitsPerSample = 0;

    // Get attributes from the media type.
    // Each of these attributes is required for uncompressed PCM
    // audio, so fail if any are not present.

    status = pmt->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
    assert(MFMediaType_Audio == majorType);
    if (MFMediaType_Audio != majorType)
        return MF_E_INVALIDMEDIATYPE;

    status = pmt->GetGUID(MF_MT_SUBTYPE, &subtype);
    assert(MFAudioFormat_Float == subtype || MFAudioFormat_PCM == subtype);
    if (MFAudioFormat_Float != subtype && MFAudioFormat_PCM != subtype)
        return MF_E_INVALIDMEDIATYPE;

    status = pmt->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &nChannels);
    assert(nChannels == static_cast<UINT32>(m_vorbis_info.channels));
    if (nChannels != static_cast<UINT32>(m_vorbis_info.channels))
        return MF_E_INVALIDMEDIATYPE;

    status = pmt->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &nSamplesPerSec);
    assert(nSamplesPerSec == static_cast<UINT32>(m_vorbis_info.rate));
    if (nSamplesPerSec != static_cast<UINT32>(m_vorbis_info.rate))
        return MF_E_INVALIDMEDIATYPE;

    status = pmt->GetUINT32(
                MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
                &nAvgBytesPerSec);

    if (FAILED(status))
        return MF_E_INVALIDMEDIATYPE;

    status = pmt->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &nBlockAlign);
    if (FAILED(status))
        return MF_E_INVALIDMEDIATYPE;

    if (subtype == MFAudioFormat_Float)
    {
        status = pmt->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &wBitsPerSample);
        assert(wBitsPerSample == (sizeof(float) * 8));
        if (wBitsPerSample != (sizeof(float) * 8))
            return MF_E_INVALIDMEDIATYPE;
    }
    else if (subtype == MFAudioFormat_PCM)
    {
        status = pmt->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &wBitsPerSample);
        // TODO(tomfinegan): support 8 bit samples
        assert(/*wBitsPerSample == 8 ||*/ wBitsPerSample == 16);
        if (FAILED(status) || /*wBitsPerSample != 8 &&*/ wBitsPerSample != 16)
            return MF_E_INVALIDMEDIATYPE;
    }
    else
        return MF_E_INVALIDMEDIATYPE;

    // Make sure block alignment was calculated correctly.
    assert(nBlockAlign == nChannels * (wBitsPerSample / 8));
    if (nBlockAlign != nChannels * (wBitsPerSample / 8))
        return MF_E_INVALIDMEDIATYPE;

    // Check possible overflow...
    // Is (nSamplesPerSec * nBlockAlign > MAXDWORD) ?
    if (nSamplesPerSec > (MAXDWORD / nBlockAlign))
        return MF_E_INVALIDMEDIATYPE;

    // Make sure average bytes per second was calculated correctly.
    if (nAvgBytesPerSec != nSamplesPerSec * nBlockAlign)
        return MF_E_INVALIDMEDIATYPE;

    return S_OK;
}
#endif


#if 0 //xxx
HRESULT WebmMfVorbisDec::CreateMediaBuffer(DWORD size,
                                           IMFMediaBuffer** pp_buffer)
{
    HRESULT status = S_OK;
    IMFMediaBuffer *pBuffer = NULL;

    // Create the media buffer.
    status = MFCreateMemoryBuffer(size, &pBuffer);
    if (FAILED(status))
        return status;

    *pp_buffer = pBuffer;
    (*pp_buffer)->AddRef();

    return status;
}

#endif  //xxx

}  //end namespace WebmMfVorbisDecLib
