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
#include "VorbisDecoder.hpp"
#include "webmtypes.hpp"
#include "vorbistypes.hpp"
#include "webmmfvorbisdec.hpp"

#ifdef _DEBUG
#include "odbgstream.hpp"
#include "iidstr.hpp"
using std::endl;

// keep the compiler quiet about do/while(0)'s used in log macros
#pragma warning(disable:4127)

#define DBGLOG(X) \
do { \
    wodbgstream wos; \
    wos << "["__FUNCTION__"] " << X << endl; \
} while(0)

#define REFTIMETOSECONDS(X) ((double)X / 10000000.0f)

#else
#define DBGLOG(X) do {} while(0)
#define REFTIMETOSECONDS(X) X
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
    m_total_time_decoded(0),
    m_stream_start_time(-1),
    m_audio_format_tag(WAVE_FORMAT_IEEE_FLOAT)
{
    HRESULT hr = m_pClassFactory->LockServer(TRUE);
    assert(SUCCEEDED(hr));

    hr = CLockable::Init();
    assert(SUCCEEDED(hr));

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

HRESULT WebmMfVorbisDec::QueryInterface(const IID& iid, void** ppv)
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

HRESULT WebmMfVorbisDec::GetStreamLimits(DWORD* pdwInputMin,
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

HRESULT WebmMfVorbisDec::GetStreamCount(DWORD* pcInputStreams,
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

HRESULT WebmMfVorbisDec::GetStreamIDs(DWORD, DWORD* pdwInputIDs, DWORD,
                                      DWORD* pdwOutputIDs)
{
    if (pdwInputIDs)
        *pdwInputIDs = 0;

    if (pdwOutputIDs)
        *pdwOutputIDs = 0;

    return E_NOTIMPL;
}

HRESULT WebmMfVorbisDec::GetInputStreamInfo(DWORD dwInputStreamID,
                                            MFT_INPUT_STREAM_INFO* pStreamInfo)
{
    if (pStreamInfo == 0)
        return E_POINTER;

    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    MFT_INPUT_STREAM_INFO& info = *pStreamInfo;

    info.cbMaxLookahead = 0;
    info.hnsMaxLatency = 0;
    info.dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES |
                   MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER;

    info.cbSize = 0;       //input size is variable
    info.cbAlignment = 0;  //no specific alignment requirements

    return S_OK;
}

HRESULT WebmMfVorbisDec::GetOutputStreamInfo(DWORD dwOutputStreamID,
                                             MFT_OUTPUT_STREAM_INFO* p_info)
{
    if (p_info == 0)
        return E_POINTER;

    if (dwOutputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    Lock lock;

    const HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    MFT_OUTPUT_STREAM_INFO& info = *p_info;

    info.dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES |
                   MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER;

    info.cbSize = (DWORD)((double)m_wave_format.nAvgBytesPerSec / 4.f);
    info.cbAlignment = 0;

    return S_OK;
}

HRESULT WebmMfVorbisDec::GetAttributes(IMFAttributes** pp)
{
    if (pp)
        *pp = 0;

    return E_NOTIMPL;
}

HRESULT WebmMfVorbisDec::GetInputStreamAttributes(DWORD, IMFAttributes** pp)
{
    if (pp)
        *pp = 0;

    return E_NOTIMPL;
}

HRESULT WebmMfVorbisDec::GetOutputStreamAttributes(DWORD, IMFAttributes** pp)
{
    if (pp)
        *pp = 0;

    return E_NOTIMPL;
}

HRESULT WebmMfVorbisDec::DeleteInputStream(DWORD)
{
    return E_NOTIMPL;
}

HRESULT WebmMfVorbisDec::AddInputStreams(DWORD, DWORD*)
{
    return E_NOTIMPL;
}

HRESULT WebmMfVorbisDec::GetInputAvailableType(DWORD dwInputStreamID,
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

HRESULT WebmMfVorbisDec::GetOutputAvailableType(DWORD dwOutputStreamID,
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

HRESULT WebmMfVorbisDec::SetInputType(DWORD dwInputStreamID,
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

    if (m_wave_format.nChannels > 2 &&
        (m_wave_format.nChannels != 6 && m_wave_format.nChannels != 8))
    {
        return MF_E_INVALIDMEDIATYPE;
    }

    hr = MFCreateMediaType(&m_output_mediatype);
    assert(SUCCEEDED(hr));
    if (FAILED(hr))
        return hr;

    hr = MFInitMediaTypeFromWaveFormatEx(m_output_mediatype, &m_wave_format,
                                         sizeof WAVEFORMATEX);
    if (FAILED(hr))
        return hr;

    if (m_wave_format.nChannels > 2)
    {
        if (m_wave_format.nChannels == 6)
        {
            UINT32 mask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT |
                          SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                          SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;

            hr = m_output_mediatype->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, mask);
            assert(SUCCEEDED(hr));
        }
        else // if (m_wave_format.nChannels == 8)
        {
            UINT32 mask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT |
                          SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                          SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
                          SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;

            hr = m_output_mediatype->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, mask);
            assert(SUCCEEDED(hr));
        }

        assert(SUCCEEDED(hr));
    }

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
        m_stream_start_time = -1;

        while (!m_samples.empty())
        {
            IMFSample* p_sample = m_samples.front();
            assert(p_sample);
            p_sample->Release();
            m_samples.pop_front();
        }

        m_vorbis_decoder.Flush();

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

    status = m_vorbis_decoder.Decode(p_input_sample_data,
                                     input_sample_data_len);

    mf_input_sample_buffer->Unlock();

    if (FAILED(status))
        return status;

    return S_OK;
}

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

    DWORD mf_storage_limit;
    status = mf_output_buffer->GetMaxLength(&mf_storage_limit);
    if (FAILED(status))
        return status;

    BYTE* p_mf_buffer_data = NULL;
    DWORD mf_data_len = 0;

    status = mf_output_buffer->Lock(&p_mf_buffer_data, &mf_storage_limit,
                                    &mf_data_len);
    if (FAILED(status))
        return status;

    UINT32 num_samples = 0, bytes_written =0;

    status = m_vorbis_decoder.ConsumeOutputSamples(p_mf_buffer_data,
                                                   mf_storage_limit,
                                                   &bytes_written,
                                                   &num_samples);

    HRESULT unlock_status = mf_output_buffer->Unlock();
    assert(SUCCEEDED(unlock_status));
    if (FAILED(unlock_status))
        return unlock_status;

    if (FAILED(status) && status != MF_E_TRANSFORM_NEED_MORE_INPUT)
    {
        m_vorbis_decoder.Flush();
        DBGLOG("VorbisDecoder Flush'd due to error!");
    }

    status = mf_output_buffer->SetCurrentLength(bytes_written);
    assert(SUCCEEDED(status));

    *p_out_samples_decoded = num_samples;

    return status;
}

HRESULT WebmMfVorbisDec::ProcessOutput(DWORD dwFlags, DWORD cOutputBufferCount,
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

    assert(p_mf_input_sample);

    status = DecodeVorbisFormat2Sample(p_mf_input_sample);

    if (FAILED(status))
        return status;

    // set |p_mf_output_sample| start time to input sample start time...
    LONGLONG start_time = 0;
    status = p_mf_input_sample->GetSampleTime(&start_time);
    assert(SUCCEEDED(status));
    assert(start_time >= 0);
    if (FAILED(status))
      return status;

    if (m_stream_start_time == -1)
    {
        m_stream_start_time = start_time;
        DBGLOG("m_stream_start_time=" << REFTIMETOSECONDS(m_stream_start_time));
    }

    LONGLONG duration;
    status = p_mf_input_sample->GetSampleDuration(&duration);
    if (FAILED(status) && MF_E_NO_SAMPLE_DURATION != status)
    {
        DBGLOG("no duration on input sample!");
        assert(SUCCEEDED(status));
    }

    // media sample data has been passed to libvorbis; pop/release
    p_mf_input_sample->Release();
    m_samples.pop_front();

    DBGLOG("IN start_time=" << REFTIMETOSECONDS(start_time) <<
           " duration=" << REFTIMETOSECONDS(duration));

    UINT32 num_samples_available;
    status = m_vorbis_decoder.GetOutputSamplesAvailable(&num_samples_available);

    if (num_samples_available == 0)
    {
        return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }

    int samples = 0;
    status = ProcessLibVorbisOutputPcmSamples(p_mf_output_sample, &samples);
    if (FAILED(status))
        return status;

    start_time = m_stream_start_time + m_total_time_decoded;

    status = p_mf_output_sample->SetSampleTime(start_time);
    assert(SUCCEEDED(status));

    // set |p_mf_output_sample| duration to the duration of the pcm samples
    // output by libvorbis
    double dmediatime_decoded = ((double)samples /
                                (double)m_wave_format.nSamplesPerSec) *
                                10000000.0f;

    LONGLONG mediatime_decoded = static_cast<LONGLONG>(dmediatime_decoded);

    status = p_mf_output_sample->SetSampleDuration(mediatime_decoded);
    assert(SUCCEEDED(status));

    m_total_time_decoded += mediatime_decoded;

    DBGLOG("OUT start_time=" << REFTIMETOSECONDS(start_time) <<
           " duration=" << REFTIMETOSECONDS(mediatime_decoded));
    DBGLOG("m_total_time_decoded=" << REFTIMETOSECONDS(m_total_time_decoded));
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

    const BYTE* p_headers[3];
    p_headers[0] = p_format_blob + sizeof VORBISFORMAT2;
    p_headers[1] = p_headers[0] + vorbis_format.headerSize[0];
    p_headers[2] = p_headers[1] + vorbis_format.headerSize[1];

    status = m_vorbis_decoder.CreateDecoder(p_headers,
                                            &vorbis_format.headerSize[0],
                                            VORBIS_SETUP_HEADER_COUNT);
    assert(SUCCEEDED(status));
    if (FAILED(status))
        return status;

    return S_OK;
}

void WebmMfVorbisDec::DestroyVorbisDecoder()
{
    m_vorbis_decoder.DestroyDecoder();
    while (m_samples.empty() == false)
    {
        IMFSample* p_mf_input_sample = m_samples.front();
        m_samples.pop_front();

        assert(p_mf_input_sample);
        p_mf_input_sample->Release();
    }
}

HRESULT WebmMfVorbisDec::ValidateOutputFormat(IMFMediaType *pmt)
{
    // Get attributes from the media type.
    // Each of these attributes is required for uncompressed PCM
    // audio, so fail if any are not present.

    GUID majorType = GUID_NULL;
    HRESULT status = pmt->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
    assert(MFMediaType_Audio == majorType);
    if (MFMediaType_Audio != majorType)
        return MF_E_INVALIDMEDIATYPE;

    GUID subtype = GUID_NULL;
    status = pmt->GetGUID(MF_MT_SUBTYPE, &subtype);
    assert(MFAudioFormat_Float == subtype || MFAudioFormat_PCM == subtype);
    if (MFAudioFormat_Float != subtype && MFAudioFormat_PCM != subtype)
        return MF_E_INVALIDMEDIATYPE;

    UINT32 output_channels = 0;
    status = pmt->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &output_channels);
    assert(SUCCEEDED(status));

    UINT32 vorbis_channels = m_vorbis_decoder.GetVorbisChannels();
    assert(output_channels == vorbis_channels);

    if (output_channels != vorbis_channels)
        return MF_E_INVALIDMEDIATYPE;

    UINT32 output_rate = 0;
    status = pmt->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &output_rate);
    assert(SUCCEEDED(status));

    UINT32 vorbis_rate = m_vorbis_decoder.GetVorbisRate();
    assert(output_rate == vorbis_rate);

    if (output_rate != vorbis_rate)
        return MF_E_INVALIDMEDIATYPE;

    UINT32 wBitsPerSample = 0;

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

    UINT32 block_align;
    status = pmt->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &block_align);
    if (FAILED(status))
        return MF_E_INVALIDMEDIATYPE;

    // Make sure block alignment was calculated correctly.
    assert(block_align == vorbis_channels * (wBitsPerSample / 8));
    if (block_align != vorbis_channels * (wBitsPerSample / 8))
        return MF_E_INVALIDMEDIATYPE;

    // Check possible overflow...
    // Is (sample_rate * nBlockAlign > MAXDWORD) ?
    if (output_rate > (MAXDWORD / block_align))
        return MF_E_INVALIDMEDIATYPE;

    UINT32 bytes_per_sec;
    status = pmt->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &bytes_per_sec);
    if (FAILED(status))
        return MF_E_INVALIDMEDIATYPE;

    // Make sure average bytes per second was calculated correctly.
    if (bytes_per_sec != output_rate * block_align)
        return MF_E_INVALIDMEDIATYPE;

    return S_OK;
}

void WebmMfVorbisDec::SetOutputWaveFormat(GUID subtype)
{
    m_wave_format.nChannels =
        static_cast<WORD>(m_vorbis_decoder.GetVorbisChannels());
    m_wave_format.nSamplesPerSec =
        static_cast<DWORD>(m_vorbis_decoder.GetVorbisRate());

    assert(subtype == MFAudioFormat_Float || subtype == MFAudioFormat_PCM);
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

    m_vorbis_decoder.SetOutputWaveFormat(m_audio_format_tag,
                                         m_wave_format.wBitsPerSample);

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

        if (ValidateOutputFormat(p_mediatype) != S_OK)
            return false;
    }

    // all tests pass, we support this format:
    return true;
}

}  //end namespace WebmMfVorbisDecLib
