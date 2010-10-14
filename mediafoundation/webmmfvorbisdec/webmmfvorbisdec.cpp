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
    m_ogg_packet_count(0),
    m_total_time_decoded(0),
    m_audio_format_tag(WAVE_FORMAT_IEEE_FLOAT)
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
    ::memset(&m_wave_format, 0, sizeof WAVEFORMATEX);
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
                   MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER;
                   //MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
                   //MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;

    info.cbSize = m_wave_format.nAvgBytesPerSec / 2;
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
    assert(SUCCEEDED(hr));
    assert(pmt);

    if (FAILED(hr) || NULL == pmt)
        return E_OUTOFMEMORY;

    if (m_input_mediatype == 0)
    {
        hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        assert(SUCCEEDED(hr));


        if (dwTypeIndex == 0)
            hr = pmt->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        else
            hr = pmt->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);

        assert(SUCCEEDED(hr));

        hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
        assert(SUCCEEDED(hr));

        hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
        assert(SUCCEEDED(hr));
    }
    else
    {
        const GUID fmt =
            (dwTypeIndex == 0 ? MFAudioFormat_Float : MFAudioFormat_PCM);
        SetOutputWaveFormat(fmt);
        hr = MFInitMediaTypeFromWaveFormatEx(pmt, &m_wave_format,
                                             sizeof WAVEFORMATEX);
        assert(SUCCEEDED(hr));
    }

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

        if (m_input_mediatype)
        {
            m_input_mediatype = 0;
            assert(SUCCEEDED(hr));
            DestroyVorbisDecoder();
        }

        if (m_output_mediatype)
            m_output_mediatype = 0;

        return S_OK;
    }

    //TODO: handle the case when already have an input media type
    //or output media type, or are already playing.  I don't
    //think we can change media types while we're playing.

    if (FormatSupported(true, pmt) == false)
        return MF_E_INVALIDMEDIATYPE;

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

    hr = CreateVorbisDecoder(pmt);

    if (hr != S_OK)
      return E_FAIL;

    SetOutputWaveFormat(MFAudioFormat_Float);

    if (m_output_mediatype)
        m_output_mediatype = 0;

    hr = MFCreateMediaType(&m_output_mediatype);
    assert(SUCCEEDED(hr));
    if (FAILED(hr))
        return hr;

    hr = MFInitMediaTypeFromWaveFormatEx(m_output_mediatype, &m_wave_format,
                                         sizeof WAVEFORMATEX);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

HRESULT WebmMfVorbisDec::SetOutputType(DWORD dwOutputStreamID,
                                       IMFMediaType* pmt, DWORD dwFlags)
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

        if (m_output_mediatype)
            m_output_mediatype = 0;

        return S_OK;
    }

    if (m_input_mediatype == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FormatSupported(false, pmt) == false)
        return MF_E_INVALIDMEDIATYPE;

    if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    GUID subtype;
    hr = pmt->GetGUID(MF_MT_SUBTYPE, &subtype);
    assert(SUCCEEDED(hr));
    if (FAILED(hr))
        return hr;

    SetOutputWaveFormat(subtype);

    // update our copy of the output type: |m_output_mediatype|
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

HRESULT WebmMfVorbisDec::GetInputCurrentType(DWORD dwInputStreamID,
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

HRESULT WebmMfVorbisDec::GetOutputCurrentType(DWORD dwOutputStreamID,
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

    //return E_NOTIMPL;  //TODO
    return S_OK;
}

HRESULT WebmMfVorbisDec::ProcessMessage(MFT_MESSAGE_TYPE message, ULONG_PTR)
{
    HRESULT hr = S_OK;

    switch (message)
    {
    case MFT_MESSAGE_COMMAND_FLUSH:
        DBGLOG("MFT_MESSAGE_COMMAND_FLUSH");
        m_total_time_decoded = 0;
        vorbis_synthesis_restart(&m_vorbis_state);
        m_vorbis_output_samples.clear();

        while (!m_samples.empty())
        {
            IMFSample* p_sample = m_samples.front();
            if (p_sample)
                p_sample->Release();
            m_samples.pop_front();
        }

        break;

    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
        DBGLOG("MFT_MESSAGE_NOTIFY_BEGIN_STREAMING");
        break;

    case MFT_MESSAGE_NOTIFY_END_STREAMING:
        DBGLOG("MFT_MESSAGE_NOTIFY_END_STREAMING");
        break;

    case MFT_MESSAGE_COMMAND_DRAIN:
        // Drain: Tells the MFT not to accept any more input until
        // all of the pending output has been processed.
        DBGLOG("MFT_MESSAGE_COMMAND_DRAIN");
        break;

    case MFT_MESSAGE_SET_D3D_MANAGER:
        // The pipeline should never send this message unless the MFT
        // has the MF_SA_D3D_AWARE attribute set to TRUE. However, if we
        // do get this message, it's invalid and we don't implement it.
        hr = E_NOTIMPL;
        DBGLOG("MFT_MESSAGE_SET_D3D_MANAGER");
        break;

    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
        DBGLOG("MFT_MESSAGE_NOTIFY_END_OF_STREAM");
        break;

    case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
        DBGLOG("MFT_MESSAGE_NOTIFY_START_OF_STREAM");
        break;
    }

    return hr;
}

HRESULT WebmMfVorbisDec::ProcessInput(DWORD dwInputStreamID, IMFSample* pSample,
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

    // addref on/store sample for use in ProcessOutput
    pSample->AddRef();
    m_samples.push_back(pSample);

    return S_OK;
}

HRESULT WebmMfVorbisDec::DecodeVorbisFormat2Sample(IMFSample* p_mf_input_sample)
{
    IMFMediaBufferPtr mf_input_sample_buffer;

    HRESULT status =
        p_mf_input_sample->GetBufferByIndex(0, &mf_input_sample_buffer);
    if (FAILED(status))
        return E_INVALIDARG;

    BYTE* p_input_sample_data = NULL;
    DWORD input_sample_data_max_size = 0, input_sample_data_len = 0;
    status = mf_input_sample_buffer->Lock(&p_input_sample_data,
                                          &input_sample_data_max_size,
                                          &input_sample_data_len);
    if (FAILED(status))
        return E_FAIL;

    status = NextOggPacket(p_input_sample_data, input_sample_data_len);
    if (FAILED(status))
        return E_FAIL;

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

    status = mf_input_sample_buffer->Unlock();
    assert(SUCCEEDED(status));

    if (FAILED(status))
        return E_FAIL;

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

HRESULT WebmMfVorbisDec::ProcessLibVorbisOutputPcmSamples(
    IMFSample* p_mf_output_sample,
    int* p_out_samples_decoded)
{
    if (NULL == p_out_samples_decoded)
        return E_INVALIDARG;

    // try to get a buffer from the output sample...
    IMFMediaBufferPtr mf_output_buffer;
    HRESULT status = p_mf_output_sample->GetBufferByIndex(0, &mf_output_buffer);

    // and complain bitterly if unable
    if (FAILED(status) || !bool(mf_output_buffer))
        return E_INVALIDARG;

    // Consume all PCM samples from libvorbis
    int samples = 0;
    float** pp_pcm;

    while ((samples = vorbis_synthesis_pcmout(&m_vorbis_state, &pp_pcm)) > 0)
    {
        for (int sample = 0; sample < samples; ++sample)
        {
            for (int channel = 0; channel < m_vorbis_info.channels; ++channel)
            {
                m_vorbis_output_samples.push_back(pp_pcm[channel][sample]);
            }
        }
        vorbis_synthesis_read(&m_vorbis_state, samples);
    }

    // Need more input if libvorbis didn't produce any output samples
    if (m_vorbis_output_samples.empty())
        return MF_E_TRANSFORM_NEED_MORE_INPUT;

    const vorbis_output_samples_t::size_type total_samples_ =
        m_vorbis_output_samples.size();
        
    const int total_samples = static_cast<int>(total_samples_);
    
    const UINT32 mf_storage_space_needed = total_samples *
                                     (m_wave_format.wBitsPerSample / 8);

    DWORD mf_storage_limit;
    status = mf_output_buffer->GetMaxLength(&mf_storage_limit);
    if (FAILED(status))
        return status;

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

    status = mf_output_buffer->Lock(&p_mf_buffer_data, &mf_storage_limit,
                                    &mf_data_len);
    if (FAILED(status))
        return status;

    if (m_wave_format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
    {
        memcpy(p_mf_buffer_data, &m_vorbis_output_samples[0],
               mf_storage_space_needed);
    }
    else
    {
        typedef vorbis_output_samples_t::const_iterator pcm_sample_iterator_t;
        pcm_sample_iterator_t pcm_iter = m_vorbis_output_samples.begin();
        pcm_sample_iterator_t pcm_end = m_vorbis_output_samples.end();

        // from the vorbis decode sample:
        // |pp_pcm| is a multichannel float vector.  In stereo, for example,
        // pp_pcm[0] is left, and pp_pcm[1] is right.  samples is the size of
        // each channel.  Convert the float values (-1.<=range<=1.) to whatever
        // PCM format and write it out

        // TODO(tomfinegan): factor resample out into 8-bit/16-bit versions

        INT16 *p_out_samples = reinterpret_cast<INT16*>(p_mf_buffer_data);

        for (int sample = 0; pcm_iter != pcm_end; ++pcm_iter, ++sample)
        {
            p_out_samples[sample] = static_cast<INT16>(clip16((int)
                floor(*pcm_iter * 32767.f + .5f)));
        }
    }

    // we consumed all samples: empty the vector
    m_vorbis_output_samples.clear();

    status = mf_output_buffer->SetCurrentLength(mf_storage_space_needed);
    assert(SUCCEEDED(status));
    if (FAILED(status))
        return status;

    status = mf_output_buffer->Unlock();
    assert(SUCCEEDED(status));
    if (FAILED(status))
        return status;

    *p_out_samples_decoded = total_samples / m_vorbis_info.channels;

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

    HRESULT status = lock.Seize(this);
    if (FAILED(status))
        return status;

    if (m_input_mediatype == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (cOutputBufferCount == 0)
        return E_INVALIDARG;

    if (pOutputSamples == 0)
        return E_INVALIDARG;

    // make sure we have an input sample to work on
    if (m_samples.empty())
        return MF_E_TRANSFORM_NEED_MORE_INPUT;

    //TODO: check if cOutputSamples > 1 ?

    // confirm we have an output sample before we pop an input sample off the
    // front of |m_samples|
    MFT_OUTPUT_DATA_BUFFER& data = pOutputSamples[0];

    //data.dwStreamID should equal 0, but we ignore it

    IMFSample* const p_mf_output_sample = data.pSample;

    if (p_mf_output_sample == 0)
        return E_INVALIDARG;

    DWORD count;

    status = p_mf_output_sample->GetBufferCount(&count);

    if (SUCCEEDED(status) && (count != 1))
        return E_INVALIDARG;

    IMFSample* const p_mf_input_sample = m_samples.front();
    m_samples.pop_front();

    assert(p_mf_input_sample);
    if (!p_mf_input_sample)
      return E_FAIL; // TODO(tomfinegan): we return
                     // MF_E_TRANSFORM_NEED_MORE_INPUT when m_samples is empty,
                     // this is a serious error...

    status = DecodeVorbisFormat2Sample(p_mf_input_sample);

    if (FAILED(status))
        return status;

    // set |p_mf_output_sample| start time to input sample start time...
    LONGLONG start_time = 0, duration = 0;
    status = p_mf_input_sample->GetSampleTime(&start_time);
    assert(SUCCEEDED(status));
    assert(start_time >= 0);
    if (FAILED(status))
      return status;

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

    int samples = 0;
    status = ProcessLibVorbisOutputPcmSamples(p_mf_output_sample, &samples);
    if (SUCCEEDED(status) || status == MF_E_TRANSFORM_NEED_MORE_INPUT)
        p_mf_input_sample->Release();
    if (FAILED(status))
        return status;

    start_time = m_total_time_decoded;

    status = p_mf_output_sample->SetSampleTime(start_time);
    assert(SUCCEEDED(status));

    // set |p_mf_output_sample| duration to the duration of the pcm samples
    // output by libvorbis
    double dmediatime_decoded = ((double)samples / (double)m_vorbis_info.rate) *
                                10000000.0f;
    LONGLONG mediatime_decoded = static_cast<LONGLONG>(dmediatime_decoded);

    status = p_mf_output_sample->SetSampleDuration(mediatime_decoded);
    assert(SUCCEEDED(status));

    DBGLOG("mediatime_decoded=" << mediatime_decoded);
    DBGLOG("m_total_time_decoded=" << m_total_time_decoded);

    // TODO(tomfinegan): does |m_total_time_decoded| need reset after seeking?
    m_total_time_decoded += mediatime_decoded;

    return S_OK;
}

HRESULT WebmMfVorbisDec::NextOggPacket(BYTE* p_packet, DWORD packet_size)
{
    if (!p_packet || packet_size == 0)
        return E_INVALIDARG;

    m_ogg_packet.b_o_s = (m_ogg_packet_count == 0);
    m_ogg_packet.bytes = packet_size;

    // TODO(tomfinegan): implement End Of Stream handling
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
    HRESULT status = p_media_type->GetAllocatedBlob(MF_MT_USER_DATA,
                                                    &p_format_blob, &blob_size);
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

    // feed the ident and comment headers into libvorbis
    int vorbis_status = 0;
    for (BYTE header_num = 0; header_num < 3; ++header_num)
    {
        assert(vorbis_format.headerSize[header_num] > 0);

        // create an ogg packet in m_ogg_packet with current header for data
        status = NextOggPacket(p_headers[header_num],
                               vorbis_format.headerSize[header_num]);
        if (FAILED(status))
            return MF_E_INVALIDMEDIATYPE;
        assert(m_ogg_packet.packetno == header_num);
        vorbis_status = vorbis_synthesis_headerin(&m_vorbis_info,
                                                  &m_vorbis_comment,
                                                  &m_ogg_packet);
        if (vorbis_status < 0)
            return MF_E_INVALIDMEDIATYPE;
    }

    // final init steps, setup decoder state...
    vorbis_status = vorbis_synthesis_init(&m_vorbis_state, &m_vorbis_info);
    if (vorbis_status != 0)
        return MF_E_INVALIDMEDIATYPE;

    // ... and vorbis block structs
    vorbis_status = vorbis_block_init(&m_vorbis_state, &m_vorbis_block);
    if (vorbis_status != 0)
        return MF_E_INVALIDMEDIATYPE;

    SetOutputWaveFormat(MFAudioFormat_Float);

    assert(m_samples.empty() == true);

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

    while (m_samples.empty() == false)
    {
        IMFSample* p_mf_input_sample = m_samples.front();
        m_samples.pop_front();

        assert(p_mf_input_sample);
        if (p_mf_input_sample)
          p_mf_input_sample->Release();
    }

    m_ogg_packet_count = 0;
    m_total_time_decoded = 0;
}

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

    status = pmt->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &nAvgBytesPerSec);
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

void WebmMfVorbisDec::SetOutputWaveFormat(GUID subtype)
{
     m_wave_format.nChannels = static_cast<WORD>(m_vorbis_info.channels);
     m_wave_format.nSamplesPerSec = static_cast<WORD>(m_vorbis_info.rate);

     if (subtype == MFAudioFormat_Float)
     {
         m_wave_format.wBitsPerSample = sizeof(float) * 8;
         m_wave_format.nBlockAlign = m_wave_format.nChannels * sizeof(float);
         m_audio_format_tag = m_wave_format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
     }
     else if (subtype == MFAudioFormat_PCM)
     {
         // TODO(tomfinegan): support 8 bits/sample
         m_wave_format.wBitsPerSample = 16;
         m_wave_format.nBlockAlign = m_wave_format.nChannels *
                                     (m_wave_format.wBitsPerSample / 8);
         m_audio_format_tag = m_wave_format.wFormatTag = WAVE_FORMAT_PCM;
     }
     else
     {
       // TODO(tomfinegan): add WAVEFORMATEXTENSIBLE/multi channel stereo support
       assert(0);
     }

     m_wave_format.nAvgBytesPerSec = m_wave_format.nBlockAlign *
                                     m_wave_format.nSamplesPerSec;
}

bool WebmMfVorbisDec::FormatSupported(bool is_input, IMFMediaType* p_mediatype)
{
    if (p_mediatype == NULL)
        return false;

    GUID g;
    HRESULT hr = p_mediatype->GetMajorType(&g);

    if (FAILED(hr) || g != MFMediaType_Audio)
        return false;

    UINT32 channels = 0;
    hr = p_mediatype->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);

    if (FAILED(hr) || channels == 0)
        return false;

    UINT32 sample_rate;
    hr = p_mediatype->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sample_rate);

    if (FAILED(hr) || sample_rate < 1)
        return false;

    if (is_input)
    {
        hr = p_mediatype->GetGUID(MF_MT_SUBTYPE, &g);

        if (FAILED(hr) || g != VorbisTypes::MEDIASUBTYPE_Vorbis2)
            return false;
    }
    else
    {
        hr = p_mediatype->GetGUID(MF_MT_SUBTYPE, &g);

        if (FAILED(hr) || g == GUID_NULL)
            return false;

        if (FAILED(hr) || (g != MFAudioFormat_Float && g != MFAudioFormat_PCM))
            return false;

        if (ValidatePcmAudioType(p_mediatype) != S_OK)
            return false;
    }

    // all tests pass, we support this format:
    return true;
}

}  //end namespace WebmMfVorbisDecLib
