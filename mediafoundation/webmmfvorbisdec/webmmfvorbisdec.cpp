// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

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
#include "debugutil.hpp"
#include "memutil.hpp"
#include "vorbisdecoder.hpp"
#include "vorbistypes.hpp"
#include "webmtypes.hpp"
#include "webmmfvorbisdec.hpp"

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
    m_block_align(0),
    m_decode_start_time(-1),
    m_total_samples_decoded(0),
    m_mediatime_decoded(-1),
    m_mediatime_recvd(-1),
    m_min_output_threshold(1000000LL), // .1 second
    m_drain(false),
    m_post_process_samples(false),
    m_scratch(NULL, 0)
{
    HRESULT hr = m_pClassFactory->LockServer(TRUE);
    assert(SUCCEEDED(hr));

    hr = CLockable::Init();
    assert(SUCCEEDED(hr));
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
    info.dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES;
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

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    MFT_OUTPUT_STREAM_INFO& info = *p_info;

    info.dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES;
                   //MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER;
                   //MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE  //TODO
                   //MFT_OUTPUT_STREAM_PROVIDES_SAMPLES   //TODO
                   //MFT_OUTPUT_STREAM_OPTIONAL


    assert(m_output_mediatype);

    UINT32 samples_per_sec;
    hr = m_output_mediatype->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,
                                       &samples_per_sec);
    // 1/4 second of samples * block align
    info.cbSize = m_block_align * ((samples_per_sec + 3) / 4);
    info.cbAlignment = 0;

    assert((info.cbSize % m_block_align) == 0);

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

        hr = pmt->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        assert(SUCCEEDED(hr));

        hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
        assert(SUCCEEDED(hr));

        hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
        assert(SUCCEEDED(hr));
    }
    else
    {
        hr = GetOutputMediaType(dwTypeIndex, pp);
        assert(SUCCEEDED(hr) || hr == MF_E_NO_MORE_TYPES);
    }

    return hr;
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

    // Check to see if input matches output
    UINT32 channels_in;
    hr = m_input_mediatype->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels_in);
    if (FAILED(hr))
        return hr;

    UINT32 channels_out;
    hr = m_output_mediatype->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS,
      &channels_out);
    if (FAILED(hr))
        return hr;

    if (channels_in == channels_out)
    {
        GUID subtype_out;
        hr = m_output_mediatype->GetGUID(MF_MT_SUBTYPE, &subtype_out);
        if (FAILED(hr))
            return hr;

        if (subtype_out != MFAudioFormat_Float)
        {
            m_post_process_samples = true;
        }
    }
    else
    {
        m_post_process_samples = true;
    }

    CHK(hr, m_output_mediatype->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,
                                          &m_block_align));
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
        //DBGLOG("MFT_MESSAGE_COMMAND_FLUSH");

        m_decode_start_time = -1;

        while (!m_mf_input_samples.empty())
        {
            IMFSample* p_sample = m_mf_input_samples.front();
            assert(p_sample);
            p_sample->Release();
            m_mf_input_samples.pop_front();
        }

        m_vorbis_decoder.Flush();

        break;

    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
        //DBGLOG("MFT_MESSAGE_NOTIFY_BEGIN_STREAMING");
        break;

    case MFT_MESSAGE_NOTIFY_END_STREAMING:
        //DBGLOG("MFT_MESSAGE_NOTIFY_END_STREAMING");
        break;

    case MFT_MESSAGE_COMMAND_DRAIN:
        // Drain: Tells the MFT not to accept any more input until
        // all of the pending output has been processed.
        //DBGLOG("MFT_MESSAGE_COMMAND_DRAIN");
        m_drain = true;
        break;

    case MFT_MESSAGE_SET_D3D_MANAGER:
        // The pipeline should never send this message unless the MFT
        // has the MF_SA_D3D_AWARE attribute set to TRUE. However, if we
        // do get this message, it's invalid and we don't implement it.
        hr = E_NOTIMPL;
        //DBGLOG("MFT_MESSAGE_SET_D3D_MANAGER");
        break;

    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
        //DBGLOG("MFT_MESSAGE_NOTIFY_END_OF_STREAM");
        break;

    case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
        //DBGLOG("MFT_MESSAGE_NOTIFY_START_OF_STREAM");
        break;
    }

    return hr;
}

HRESULT WebmMfVorbisDec::ProcessInput(DWORD dwInputStreamID,
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

    if (count == 0)  //weird
        return S_OK;

    //if (count > 1)
    //    return E_INVALIDARG;

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
    m_mf_input_samples.push_back(pSample);

    // clear the drain flag
    m_drain = false;

    return S_OK;
}

HRESULT WebmMfVorbisDec::DecodeVorbisFormat2Sample(IMFSample* p_sample)
{
    assert(p_sample);

    DWORD count;

    HRESULT status = p_sample->GetBufferCount(&count);

    if (FAILED(status))
        return status;

    for (DWORD idx = 0; idx < count; ++idx)
    {
        IMFMediaBufferPtr buf;

        status = p_sample->GetBufferByIndex(idx, &buf);

        if (FAILED(status))
            return status;

        BYTE* p_data;
        DWORD data_max_size, data_len;

        status = buf->Lock(&p_data, &data_max_size, &data_len);

        if (FAILED(status))
            return status;

        const HRESULT status2 = m_vorbis_decoder.Decode(p_data, data_len);

        status = buf->Unlock();
        assert(SUCCEEDED(status));

        if (FAILED(status2))  //decode status
            return status2;
    }

    return S_OK;
}

DWORD get_mf_buffer_capacity(IMFSample* ptr_sample, DWORD buf_index)
{
    if (!ptr_sample)
    {
        DBGLOG("ERROR NULL ptr_sample.");
        return 0;
    }
    IMFMediaBufferPtr ptr_buffer;
    HRESULT hr;
    CHK(hr, ptr_sample->GetBufferByIndex(buf_index, &ptr_buffer));
    if (FAILED(hr))
    {
        return 0;
    }
    DWORD buffer_capacity = 0;
    CHK(hr, ptr_buffer->GetMaxLength(&buffer_capacity));
    return buffer_capacity;
}

UINT32 get_block_align_from_mediatype(IMFMediaType* ptr_type)
{
    if (!ptr_type)
    {
        DBGLOG("ERROR NULL ptr_type.");
        return 0;
    }
    UINT32 block_align = 0;
    HRESULT hr;
    CHK(hr, ptr_type->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &block_align));
    return block_align;
}

HRESULT WebmMfVorbisDec::ProcessLibVorbisOutput(IMFSample* p_sample,
                                                UINT32 samples_to_process)
{
    // try to get a buffer from the output sample...
    IMFMediaBufferPtr mf_output_buffer;
    HRESULT status = p_sample->GetBufferByIndex(0, &mf_output_buffer);
    // and complain bitterly if unable
    if (FAILED(status) || !bool(mf_output_buffer))
    {
        return E_INVALIDARG;
    }
    DWORD mf_storage_limit = get_mf_buffer_capacity(p_sample, 0);
    if (0 == mf_storage_limit)
    {
        DBGLOG("ERROR unable to obtain output buffer capacity.");
        return E_UNEXPECTED;
    }
    UINT32 block_align;
    assert(m_output_mediatype);
    status = m_output_mediatype->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,
                                           &block_align);
    assert(SUCCEEDED(status));
    if (FAILED(status))
    {
        return status;
    }
    // adjust number of bytes to consume based on number of samples caller
    // requested: shoving it all in just because it fits isn't what we want...
    const DWORD max_bytes_to_consume = samples_to_process * block_align;
    assert(max_bytes_to_consume <= mf_storage_limit);
    if (!m_post_process_samples)
    {
        BYTE* p_mf_buffer_data = NULL;
        DWORD mf_data_len = 0;
        status = mf_output_buffer->Lock(&p_mf_buffer_data, &mf_storage_limit,
                                        &mf_data_len);
        if (FAILED(status))
            return status;

        float* const ptr_mf_buffer =
            reinterpret_cast<float*>(p_mf_buffer_data);

        status = m_vorbis_decoder.ConsumeOutputSamples(ptr_mf_buffer,
                                                       samples_to_process);

        assert(SUCCEEDED(status));

        const HRESULT unlock_status = mf_output_buffer->Unlock();
        assert(SUCCEEDED(unlock_status));
        if (FAILED(unlock_status))
        {
            return unlock_status;
        }
    }
    else
    {
        const DWORD vorbis_bytes_to_consume = samples_to_process *
            m_vorbis_decoder.GetVorbisChannels() * sizeof(float);

        if (m_scratch.size() < vorbis_bytes_to_consume)
        {
            m_scratch.reset(
                new (std::nothrow) unsigned char[vorbis_bytes_to_consume],
                vorbis_bytes_to_consume);
            if (!m_scratch)
            {
                return E_OUTOFMEMORY;
            }
        }

        float* const p_vorbis_float_buffer =
            reinterpret_cast<float*>(m_scratch.get());

        status = m_vorbis_decoder.ConsumeOutputSamples(p_vorbis_float_buffer,
                                                       samples_to_process);

        assert(SUCCEEDED(status));

        BYTE* p_mf_buffer_data = NULL;
        DWORD mf_data_len = 0;
        status = mf_output_buffer->Lock(&p_mf_buffer_data, &mf_storage_limit,
                                        &mf_data_len);
        if (FAILED(status))
            return status;

        if (!PostProcessSamples(p_vorbis_float_buffer,
                                samples_to_process,
                                p_mf_buffer_data))
        {
            return E_INVALIDARG;
        }

        const HRESULT unlock_status = mf_output_buffer->Unlock();
        assert(SUCCEEDED(unlock_status));
        if (FAILED(unlock_status))
        {
            return unlock_status;
        }
    }

    const UINT32 bytes_written = max_bytes_to_consume;

    status = mf_output_buffer->SetCurrentLength(bytes_written);
    assert(SUCCEEDED(status));

    return status;
}

bool WebmMfVorbisDec::PostProcessSamples(float* const p_vorbis_float_buffer,
                                         UINT32 samples_to_process,
                                         BYTE* p_mf_buffer_data)
{
    UINT32 channels_to_convert = m_vorbis_decoder.GetVorbisChannels();

    UINT32 channels_out;
    HRESULT hr = m_output_mediatype->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS,
                                               &channels_out);
    if (FAILED(hr))
        return false;
    assert(channels_out <= channels_to_convert);

    GUID subtype_out;
    hr = m_output_mediatype->GetGUID(MF_MT_SUBTYPE, &subtype_out);
    if (FAILED(hr))
        return false;

    if (channels_out < channels_to_convert)
    {
        const bool convert_to_short =
            (subtype_out != MFAudioFormat_Float) ? true : false;

        float* src = static_cast<float*>(p_vorbis_float_buffer);
        float* dstfl = reinterpret_cast<float*>(p_mf_buffer_data);
        short* dst16 = reinterpret_cast<short*>(p_mf_buffer_data);

        // TODO(tomfinegan or fgalligan): setup a postprocessing function ptr
        //                                and always call through the ptr to
        //                                avoid most of the format checks
        //                                during playback.
        for (UINT32 i=0; i<samples_to_process; ++i)
        {
            float left = 0.0f;
            float right = 0.0f;

            if (channels_out == 2)
            {
                // Downmix to stereo
                switch (channels_to_convert)
                {
                case 8:
                    left = src[0] + src[3] + src[2] * 0.7f
                        + src[4] * 0.7f + src[6] * 0.7f;
                    right = src[1] + src[3] + src[2] * 0.7f
                        + src[5] * 0.7f + src[7] * 0.7f;
                    src += 8;
                    break;
                case 7:
                    left = src[0] + src[3] + src[2] * 0.7f
                        + src[4] * 0.7f + src[6] * 0.7f;
                    right = src[1] + src[3] + src[2] * 0.7f
                        + src[5] * 0.7f + src[6] * 0.7f;
                    src += 7;
                    break;
                case 6:
                    left = src[0] + src[3] + src[2] * 0.7f + src[4] * 0.7f;
                    right = src[1] + src[3] + src[2] * 0.7f + src[5] * 0.7f;
                    src += 6;
                    break;
                case 5:
                    left = src[0] + src[2] * 0.7f + src[3] * 0.7f;
                    right = src[1] + src[2] * 0.7f + src[4] * 0.7f;
                    src += 5;
                    break;
                case 4:
                    left = src[0] + src[2] * 0.7f + src[3] * 0.7f;
                    right = src[1] + src[2] * 0.7f + src[3] * 0.7f;
                    src += 4;
                    break;
                case 3:
                    left = src[0] + src[2] * 0.7f;
                    right = src[1] + src[2] * 0.7f;
                    src += 3;
                    break;
                }

                if (convert_to_short)
                {
                    int val = static_cast<int>(floor(left * 32767.f + .5f));
                    if (val > 32767)
                    {
                        val = 32767;
                    }
                    else if (val < -32768)
                    {
                        val = -32768;
                    }
                    *dst16++ = static_cast<short>(val);

                    val = static_cast<int>(floor(right * 32767.f + .5f));
                    if(val > 32767)
                    {
                        val = 32767;
                    }
                    else if(val < -32768)
                    {
                        val = -32768;
                    }
                    *dst16++ = static_cast<short>(val);
                }
                else
                {
                    *dstfl++ = left;
                    *dstfl++ = right;
                }
            }
            else if (channels_out == 6)
            {
                // fold to 5.1
                if (convert_to_short)
                {
                    for (int j=0; j<6; ++j)
                    {
                        int val = static_cast<int>(floor(src[j] *
                                                   32767.f + .5f));
                        if (val > 32767)
                        {
                            val = 32767;
                        }
                        else if (val < -32768)
                        {
                            val = -32768;
                        }
                        *dst16++ = static_cast<short>(val);
                    }
                }
                else
                {
                    memcpy(dstfl, src, 6 * sizeof(float));
                    dstfl += 6;
                }

                src += channels_to_convert;
            }
        }
    }
    else if (subtype_out != MFAudioFormat_Float)
    {
        const int samples_to_convert =
            samples_to_process * channels_to_convert;

        float* src = static_cast<float*>(p_vorbis_float_buffer);
        short* dst16 = reinterpret_cast<short*>(p_mf_buffer_data);

        for (int i=0; i<samples_to_convert; i++)
        {
            int val = static_cast<int>(floor(src[i] * 32767.f + .5f));
            if (val > 32767)
            {
                val = 32767;
            }
            else if (val < -32768)
            {
                val = -32768;
            }
            *dst16++ = static_cast<short>(val);
        }
    }

    return true;
}

REFERENCE_TIME WebmMfVorbisDec::SamplesToMediaTime(UINT64 sample_count) const
{
    const double kHz = 10000000.0;

    UINT32 samples_per_sec;
    assert(m_output_mediatype);
    HRESULT hr = m_output_mediatype->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,
                                               &samples_per_sec);
    assert(SUCCEEDED(hr));

    return REFERENCE_TIME((double(sample_count) /
                          double(samples_per_sec)) * kHz);
}

UINT64 WebmMfVorbisDec::MediaTimeToSamples(REFERENCE_TIME media_time) const
{
    const double kHz = 10000000.0;

    UINT32 samples_per_sec;
    assert(m_output_mediatype);
    HRESULT hr = m_output_mediatype->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,
                                               &samples_per_sec);
    assert(SUCCEEDED(hr));

    return static_cast<UINT64>((double(media_time) * samples_per_sec) / kHz);
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

    HRESULT hr;
    CHK(hr, lock.Seize(this));
    if (FAILED(hr))
    {
        return hr;
    }

    if (m_input_mediatype == 0)
    {
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    if (cOutputBufferCount == 0)
    {
        return E_INVALIDARG;
    }

    if (pOutputSamples == 0)
    {
        return E_INVALIDARG;
    }

    // make sure we have an input sample to work on
    if (m_mf_input_samples.empty())
    {
        return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }

    //TODO: check if cOutputSamples > 1 ?

    // confirm we have an output sample before we pop an input sample off the
    // front of |m_mf_input_samples|
    MFT_OUTPUT_DATA_BUFFER& data = pOutputSamples[0];
    //data.dwStreamID should equal 0, but we ignore it

    IMFSample* const p_mf_output_sample = data.pSample;

    if (!p_mf_output_sample)
    {
        return E_INVALIDARG;
    }

    DWORD count;

    CHK(hr, p_mf_output_sample->GetBufferCount(&count));

    if (SUCCEEDED(hr) && (count != 1))  //TODO: handle this?
    {
        return E_INVALIDARG;
    }

    IMFSample* const p_mf_input_sample = m_mf_input_samples.front();
    assert(p_mf_input_sample);

    CHK(hr, DecodeVorbisFormat2Sample(p_mf_input_sample));

    if (FAILED(hr))
    {
        return hr;
    }

    // store input start time for logging/debugging
    LONGLONG input_start_time = 0;
    CHK(hr, p_mf_input_sample->GetSampleTime(&input_start_time));
    assert(SUCCEEDED(hr));
    assert(input_start_time >= 0);
    if (FAILED(hr))
    {
        return hr;
    }

    if (m_decode_start_time < 0)
    {
        m_total_samples_decoded = 0;
        m_decode_start_time = input_start_time;

        // DEBUG
        m_mediatime_decoded = 0;
        m_mediatime_recvd = 0;

        m_start_time = m_decode_start_time;

        //DBGLOG("m_decode_start_time="
        //       << REFTIMETOSECONDS(m_decode_start_time));
    }

    LONGLONG input_duration = 0;
    hr = p_mf_input_sample->GetSampleDuration(&input_duration);
    if (FAILED(hr) && MF_E_NO_SAMPLE_DURATION != hr)
    {
        //DBGLOG("no duration on input sample!");
        assert(SUCCEEDED(hr));
    }

    m_mediatime_recvd += input_duration;

    // media sample data has been passed to libvorbis; pop/release
    m_mf_input_samples.pop_front();
    p_mf_input_sample->Release();

    //DBGLOG("IN start_time=" << REFTIMETOSECONDS(input_start_time) <<
    //       " duration=" << REFTIMETOSECONDS(input_duration));

    // get output buffer size, and use it to calculate |samples_to_process| to
    // ensure we never try to process more samples than will fit in the output
    // buffer

    const DWORD buffer_capacity_bytes =
        get_mf_buffer_capacity(p_mf_output_sample, 0);
    const DWORD buffer_capacity_samples =
        (buffer_capacity_bytes + (m_block_align - 1)) / m_block_align;

    UINT32 samples_available;
    CHK(hr, m_vorbis_decoder.GetOutputSamplesAvailable(&samples_available));

    bool need_more_input =
        SamplesToMediaTime(samples_available) < m_min_output_threshold;

    if (m_drain)
    {
        need_more_input = false;
    }

    if (samples_available < 1 || need_more_input)
    {
        return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }

    // Ensure we never try to process more samples than will fit in our output
    // buffer -- abuse |buffer_capacity_samples| to cap |samples_to_process| if
    // needed.
    const DWORD samples_to_process =
        buffer_capacity_samples > samples_available ?
            samples_available : buffer_capacity_samples;

    assert(samples_to_process * m_block_align <= buffer_capacity_bytes);

    CHK(hr, ProcessLibVorbisOutput(p_mf_output_sample, samples_to_process));
    if (FAILED(hr))
    {
        return hr;
    }

    // |m_start_time| is total samples decoded converted to media time
    CHK(hr, p_mf_output_sample->SetSampleTime(m_start_time));
    assert(SUCCEEDED(hr));

    // update running sample and time totals
    m_total_samples_decoded += samples_to_process;
    LONGLONG mediatime_decoded = SamplesToMediaTime(samples_to_process);
    m_mediatime_decoded += mediatime_decoded;

    //DBGLOG("OUT start_time=" << REFTIMETOSECONDS(m_start_time) <<
    //       " duration (seconds)=" << REFTIMETOSECONDS(mediatime_decoded) <<
    //       " duration (samples)=" << samples_available <<
    //       " end time=" <<
    //       REFTIMETOSECONDS(m_start_time + mediatime_decoded));
    //DBGLOG("m_total_samples_decoded=" << m_total_samples_decoded);
    //DBGLOG("total time recvd (seconds)=" <<
    //       REFTIMETOSECONDS(m_mediatime_recvd) <<
    //       " total time decoded (seconds)=" <<
    //       REFTIMETOSECONDS(m_mediatime_decoded));
    //DBGLOG("lag (seconds)=" <<
    //       REFTIMETOSECONDS(m_mediatime_recvd - m_mediatime_decoded) <<
    //       " lag (samples)=" <<
    //       MediaTimeToSamples(m_mediatime_recvd - m_mediatime_decoded));

    // update |m_start_time| for the next time through |ProcessOutput|
    const LONGLONG start_time = m_decode_start_time +
                                SamplesToMediaTime(m_total_samples_decoded);

    const LONGLONG duration = start_time - m_start_time;

    CHK(hr, p_mf_output_sample->SetSampleDuration(duration));
    assert(SUCCEEDED(hr));

    m_start_time = start_time;

    return S_OK;
}


HRESULT WebmMfVorbisDec::CreateVorbisDecoder(IMFMediaType* p_media_type)
{
    //
    // p_media_type has already been used extensively in the caller
    BYTE* p_format_blob = NULL;
    UINT32 blob_size = 0;
    HRESULT status = p_media_type->GetAllocatedBlob(MF_MT_USER_DATA,
                                                    &p_format_blob,
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
    while (m_mf_input_samples.empty() == false)
    {
        IMFSample* p_mf_input_sample = m_mf_input_samples.front();
        m_mf_input_samples.pop_front();

        assert(p_mf_input_sample);
        p_mf_input_sample->Release();
    }
}

HRESULT WebmMfVorbisDec::ValidateOutputFormat(IMFMediaType *pmt)
{
    // Get attributes from the media type.
    // Each of these attributes is required for uncompressed PCM
    // audio, so fail if any are not present.

    GUID majorType;
    HRESULT status = pmt->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
    assert(SUCCEEDED(status) && MFMediaType_Audio == majorType);
    if (FAILED(status) || MFMediaType_Audio != majorType)
        return MF_E_INVALIDMEDIATYPE;

    GUID subtype;
    status = pmt->GetGUID(MF_MT_SUBTYPE, &subtype);
    assert(SUCCEEDED(status) &&
        (MFAudioFormat_Float == subtype || MFAudioFormat_PCM == subtype));
    if (FAILED(status) ||
        (MFAudioFormat_Float != subtype && MFAudioFormat_PCM != subtype) )
        return MF_E_INVALIDMEDIATYPE;

    UINT32 output_channels = 0;
    status = pmt->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &output_channels);
    assert(SUCCEEDED(status));

    assert((output_channels >= 1) && (output_channels <= 8));
    if ((output_channels < 1) || (output_channels > 8))
    {
        return MF_E_INVALIDMEDIATYPE;
    }

    UINT32 output_rate = 0;
    status = pmt->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &output_rate);
    assert(SUCCEEDED(status));

    UINT32 vorbis_rate = m_vorbis_decoder.GetVorbisRate();

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
        assert(wBitsPerSample == 16);
        if (wBitsPerSample != 16)
            return MF_E_INVALIDMEDIATYPE;
    }
    else
        return MF_E_INVALIDMEDIATYPE;

    UINT32 block_align;
    status = pmt->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &block_align);
    if (FAILED(status))
        return MF_E_INVALIDMEDIATYPE;

    // Check possible overflow...
    // Is (sample_rate * nBlockAlign > MAXDWORD) ?
    assert(output_rate <= (MAXDWORD / block_align));
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

HRESULT WebmMfVorbisDec::GetOutputMediaType(DWORD dwTypeIndex,
                                            IMFMediaType** pp) const
{
    if (m_input_mediatype == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    UINT32 channels = 0;
    HRESULT hr = m_input_mediatype->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS,
                                              &channels);
    if (FAILED(hr) || channels == 0)
        return MF_E_INVALIDMEDIATYPE;

    UINT32 sample_rate;
    hr = m_input_mediatype->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,
                                      &sample_rate);

    if (FAILED(hr) || sample_rate < 1)
        return MF_E_INVALIDMEDIATYPE;

    UINT32 six_channel_mask = GetChannelMask(6);
    UINT32 two_channel_mask = GetChannelMask(2);

    switch (channels)
    {
    case 8:
    case 7:
        switch (dwTypeIndex)
        {
        case 0:
            // 7.1 or 6.1 audio float
            hr = GenerateMediaType(pp,
                                   channels,
                                   GetChannelMask(channels),
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            break;

        case 1:
            // 7.1 or 6.1 audio integer
            hr = GenerateMediaType(pp,
                                   channels,
                                   GetChannelMask(channels),
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_PCM);
            break;

        case 2:
            // 5.1 audio float folding
            hr = GenerateMediaType(pp,
                                   6,
                                   six_channel_mask,
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            break;

        case 3:
            // 5.1 audio integer folding
            hr = GenerateMediaType(pp,
                                   6,
                                   six_channel_mask,
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_PCM);
            break;

        case 4:
            // stereo audio float downmix
            hr = GenerateMediaType(pp,
                                   2,
                                   two_channel_mask,
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            break;

        case 5:
            // stereo audio integer downmix
            hr = GenerateMediaType(pp,
                                   2,
                                   two_channel_mask,
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_PCM);
            break;
        default:
            return MF_E_NO_MORE_TYPES;
        }
        break;

    case 6:
    case 5:
    case 4:
    case 3:
        switch (dwTypeIndex)
        {
        case 0:
            // 5.1, 5, 4, 3 audio float
            hr = GenerateMediaType(pp,
                                   channels,
                                   GetChannelMask(channels),
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            break;

        case 1:
            // 5.1, 5, 4, 3 audio integer
            hr = GenerateMediaType(pp,
                                   channels,
                                   GetChannelMask(channels),
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_PCM);
            break;

        case 2:
            // stereo audio float downmix
            hr = GenerateMediaType(pp,
                                   2,
                                   two_channel_mask,
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            break;

        case 3:
            // stereo audio integer downmix
            hr = GenerateMediaType(pp,
                                   2,
                                   two_channel_mask,
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_PCM);
            break;
        default:
            return MF_E_NO_MORE_TYPES;
        }
        break;

    case 2:
    case 1:
        switch (dwTypeIndex)
        {
        case 0:
            // stereo or mono audio float
            hr = GenerateMediaType(pp,
                                   channels,
                                   GetChannelMask(channels),
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            break;

        case 1:
            // stereo or mono audio integer
            hr = GenerateMediaType(pp,
                                   channels,
                                   GetChannelMask(channels),
                                   sample_rate,
                                   KSDATAFORMAT_SUBTYPE_PCM);
            break;
        default:
            return MF_E_NO_MORE_TYPES;
        }
        break;

    default:
        return MF_E_INVALIDMEDIATYPE;
    }

    return hr;
}

HRESULT WebmMfVorbisDec::GenerateMediaType(IMFMediaType** pp,
                                           int channels,
                                           UINT32 mask,
                                           UINT32 sample_rate,
                                           GUID type) const
{
    WAVEFORMATEXTENSIBLE wave;

    wave.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wave.Format.nChannels = static_cast<WORD>(channels);
    wave.Format.nSamplesPerSec = static_cast<DWORD>(sample_rate);

    if (type == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
    {
        wave.Format.wBitsPerSample = sizeof(float) * 8;
    }
    else
    {
        wave.Format.wBitsPerSample = 16;
    }

    wave.Format.nBlockAlign =
        wave.Format.nChannels * (wave.Format.wBitsPerSample / 8);
    wave.Format.nAvgBytesPerSec =
        wave.Format.nBlockAlign * wave.Format.nSamplesPerSec;
    wave.Format.cbSize =
        sizeof WAVEFORMATEXTENSIBLE - sizeof WAVEFORMATEX;

    wave.Samples.wValidBitsPerSample = wave.Format.wBitsPerSample;
    wave.dwChannelMask = mask;
    wave.SubFormat = type;

    IMFMediaType*& pmt = *pp;

    HRESULT hr = MFCreateMediaType(&pmt);
    assert(SUCCEEDED(hr));
    assert(pmt);

    if (FAILED(hr) || NULL == pmt)
        return E_OUTOFMEMORY;

    WAVEFORMATEX* pWave = reinterpret_cast<WAVEFORMATEX*>(&wave);
    hr = MFInitMediaTypeFromWaveFormatEx(pmt, pWave,
                                         sizeof WAVEFORMATEXTENSIBLE);
    assert(SUCCEEDED(hr));

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    return hr;
}


UINT32 WebmMfVorbisDec::GetChannelMask(int channels) const
{
    UINT32 mask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    switch (channels)
    {
        case 2:
            break;
        case 1:
            mask = SPEAKER_FRONT_CENTER;
            break;
        case 3:
            mask |= SPEAKER_FRONT_CENTER;
            break;
        case 4:
            mask |= SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
            break;
        case 5:
            mask |= SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT |
                    SPEAKER_BACK_RIGHT;
            break;
        case 6:
            mask |= SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
            break;
        case 7:
            mask |= SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                    SPEAKER_BACK_CENTER | SPEAKER_SIDE_LEFT |
                    SPEAKER_SIDE_RIGHT;
            break;
        case 8:
            mask |= SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
                    SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
            break;
        default:
            mask = 0;
    }

    return mask;
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

        if (FAILED(hr) || (MFAudioFormat_Float != g && MFAudioFormat_PCM != g))
            return false;

        if (ValidateOutputFormat(p_mediatype) != S_OK)
            return false;
    }

    // all tests pass, we support this format:
    return true;
}

}  //end namespace WebmMfVorbisDecLib
