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
#include <cmath>
#include <vector>

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
    ::memset(&m_wave_format, 0, sizeof WAVEFORMATEX);
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

    info.cbSize = 0;
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

    if (dwTypeIndex > 0)
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

    if (m_pInputMediaType == 0)
    {
        hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        assert(SUCCEEDED(hr));

        hr = pmt->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        assert(SUCCEEDED(hr));

        hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
        assert(SUCCEEDED(hr));

        hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
        assert(SUCCEEDED(hr));
    }
    else
    {
        hr = MFInitMediaTypeFromWaveFormatEx(pmt, &m_wave_format,
                                             sizeof WAVEFORMATEX);
        assert(SUCCEEDED(hr));
        // TODO(tomfinegan): ensure MFInitMediaTypeFromWaveFormatEx sets all
        //                   necessary flags
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

    if (g != VorbisTypes::MEDIASUBTYPE_Vorbis2)
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

    hr = MFCreateMediaType(&m_pOutputMediaType);
    assert(SUCCEEDED(hr));  //TODO
    if (FAILED(hr))
        return hr;

    hr = MFInitMediaTypeFromWaveFormatEx(m_pOutputMediaType, &m_wave_format,
                                         sizeof WAVEFORMATEX);
    if (FAILED(hr))
        return hr;

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

    if (g != MFMediaType_Audio)
        return MF_E_INVALIDMEDIATYPE;

    hr = pmt->GetGUID(MF_MT_SUBTYPE, &g);

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    // TODO(tomfinegan): confirm this works w/topoedit and wmp
    if (MFCompareFullToPartialMediaType(pmt, m_pOutputMediaType) != TRUE)
      return MF_E_INVALIDMEDIATYPE;

    if (ValidatePCMAudioType(pmt) != S_OK)
      return MF_E_INVALIDMEDIATYPE;

    // TODO(tomfinegan): add MFAudioFormat_Float support?

    if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    // TODO(tomfinegan): is this next bit necessary?  After going through all
    //                   the validation above it seems silly.
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

    if (m_pInputMediaType == 0)
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

    // TODO(tomfinegan): confirm that NextOggPacket is adequate for use here
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

HRESULT WebmMfVorbisDec::ConvertLibVorbisOutputPCMSamples(
    IMFSample* p_mf_output_sample,
    double* p_out_samples_decoded)
{
    if (NULL == p_out_samples_decoded)
        return E_INVALIDARG;

    // try to get a buffer from the output sample...
    IMFMediaBufferPtr mf_output_buffer;
    HRESULT status = p_mf_output_sample->GetBufferByIndex(0, &mf_output_buffer);

    // and complain bitterly if unable
    if (FAILED(status) || !bool(mf_output_buffer))
        return E_INVALIDARG;

    // prepare for sample conversion (libvorbis outputs floats, we want integer
    // samples)
    float** pp_pcm;
    int samples = vorbis_synthesis_pcmout(&m_vorbis_state, &pp_pcm);
    if (samples == 0)
      return MF_E_TRANSFORM_NEED_MORE_INPUT;

    UINT32 storage_space_needed = samples * m_vorbis_info.channels *
                                  m_wave_format.nSamplesPerSec;
    DWORD mf_storage_limit;
    status = mf_output_buffer->GetMaxLength(&mf_storage_limit);
    if (FAILED(status))
        return status;

    BYTE* p_mf_buffer_data = NULL;
    DWORD mf_data_len = 0;
    if (storage_space_needed > mf_storage_limit)
    {
        // TODO(tomfinegan): do I have to queue extra data, or is it alright to
        //                   replace the sample buffer?
        status = p_mf_output_sample->RemoveBufferByIndex(0);
        assert(SUCCEEDED(status));
        if (FAILED(status))
            return status;

        IMFMediaBuffer* p_buffer = NULL;
        status = CreateMediaBuffer(storage_space_needed, &p_buffer);

        if (FAILED(status))
            return status;

        mf_output_buffer = p_buffer;
    }

    status = mf_output_buffer->Lock(&p_mf_buffer_data, &mf_storage_limit,
                                    &mf_data_len);
    if (FAILED(status))
        return status;

    // from the vorbis decode sample, plus some edits
    // |pp_pcm| is a multichannel float vector.  In stereo, for example,
    // pp_pcm[0] is left, and pp_pcm[1] is right.  samples is the size of each
    // channel.  Convert the float values (-1.<=range<=1.) to whatever PCM
    // format and write it out

    // TODO(tomfinegan): factor resample out into 8-bit/16-bit versions

    INT16 *p_out_samples = reinterpret_cast<INT16*>(p_mf_buffer_data);
    float* p_curr_channel_samples;
    // convert floats to 16 bit signed ints (host order) and interleave
    for (int i = 0; i < m_vorbis_info.channels; ++i)
    {
        p_out_samples += i;
        p_curr_channel_samples = pp_pcm[i];
        for (int sample = 0; sample < samples; sample += m_vorbis_info.channels)
        {
            p_out_samples[sample] = static_cast<INT16>(
              floor(p_curr_channel_samples[sample] * 32767.f + .5f));
        }
    }

    status = mf_output_buffer->SetCurrentLength(storage_space_needed);
    assert(SUCCEEDED(status));
    if (FAILED(status))
        return status;

    status = mf_output_buffer->Unlock();
    assert(SUCCEEDED(status));
    if (FAILED(status))
        return status;

    *p_out_samples_decoded = samples;

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

    if (m_pInputMediaType == 0)
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
    LONGLONG start_time;
    status = p_mf_input_sample->GetSampleTime(&start_time);

    double samples = 0;
    status = ConvertLibVorbisOutputPCMSamples(p_mf_output_sample, &samples);
    if (SUCCEEDED(status) || status == MF_E_TRANSFORM_NEED_MORE_INPUT)
        p_mf_input_sample->Release();
    if (FAILED(status))
        return status;


    if (SUCCEEDED(status))
    {
        assert(start_time >= 0);

        status = p_mf_output_sample->SetSampleTime(start_time);
        assert(SUCCEEDED(status));
    }

    // set |p_mf_output_sample| duration to the duration of the pcm samples
    // output by libvorbis
    double seconds_decoded = (double)samples / (double)m_vorbis_info.rate;
    LONGLONG mediatime_decoded = (LONGLONG)(seconds_decoded / 10000000.0f);
    status = p_mf_output_sample->SetSampleDuration(mediatime_decoded);
    assert(SUCCEEDED(status));

    return S_OK;
}

HRESULT WebmMfVorbisDec::NextOggPacket(BYTE* p_packet, DWORD packet_size)
{
    if (!p_packet || packet_size == 0)
        return E_INVALIDARG;

    m_ogg_packet.b_o_s = (m_ogg_packet_count == 0);
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

    // store the vorbis format information in our internal WAVEFORMATEX
    // TODO(tomfinegan): add WAVEFORMATEXTENSIBLE/multi channel stereo support
    // TODO(tomfinegan): use PCMWAVEFORMAT instead?  I prefer using WAVEFORMATEX
    //                   because it allows use of
    //                   MFInitMediaTypeFromWaveFormatEx elsewhere...
    m_wave_format.cbSize = 0;

    m_wave_format.nChannels = static_cast<WORD>(m_vorbis_info.channels);
    m_wave_format.nSamplesPerSec = static_cast<WORD>(m_vorbis_info.rate);

    // TODO(tomfinegan): read |m_wave_format.wBitsPerSample| from selected
    //                   media type
    m_wave_format.wBitsPerSample = 16;

    m_wave_format.nBlockAlign = m_wave_format.nChannels *
                                (m_wave_format.wBitsPerSample / 8);
    m_wave_format.nAvgBytesPerSec = m_wave_format.nBlockAlign *
                                    m_wave_format.nSamplesPerSec;
    m_wave_format.wFormatTag = WAVE_FORMAT_PCM;

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
}

HRESULT WebmMfVorbisDec::ValidatePCMAudioType(IMFMediaType *pmt)
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
    assert(MFAudioFormat_PCM == subtype);
    if (MFAudioFormat_PCM != subtype)
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

    status = pmt->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &wBitsPerSample);
    // TODO(tomfinegan): support 8 bit samples?
    assert(/*wBitsPerSample == 8 ||*/ wBitsPerSample == 16);
    if (FAILED(status) || /*wBitsPerSample != 8 &&*/ wBitsPerSample != 16)
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


}  //end namespace WebmMfVorbisDecLib
