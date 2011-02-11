// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once

#include "memutilfwd.hpp"

namespace WebmMfVorbisDecLib
{

class WebmMfVorbisDec : public IMFTransform,
                     public CLockable
{
    friend HRESULT CreateDecoder(
            IClassFactory*,
            IUnknown*,
            const IID&,
            void**);

public:

    //IUnknown

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    //IMFTransform

    HRESULT STDMETHODCALLTYPE GetStreamLimits(
        DWORD* pdwInputMinimum,
        DWORD* pdwInputMaximum,
        DWORD* pdwOutputMinimum,
        DWORD* pdwOutputMaximum);

    HRESULT STDMETHODCALLTYPE GetStreamCount(
        DWORD* pcInputStreams,
        DWORD* pcOutputStreams);

    HRESULT STDMETHODCALLTYPE GetStreamIDs(
        DWORD dwInputIDArraySize,
        DWORD* pdwInputIDs,
        DWORD dwOutputIDArraySize,
        DWORD* pdwOutputIDs);

    HRESULT STDMETHODCALLTYPE GetInputStreamInfo(
        DWORD dwInputStreamID,
        MFT_INPUT_STREAM_INFO* pStreamInfo);

    HRESULT STDMETHODCALLTYPE GetOutputStreamInfo(
        DWORD dwOutputStreamID,
        MFT_OUTPUT_STREAM_INFO* pStreamInfo);

    HRESULT STDMETHODCALLTYPE GetAttributes(IMFAttributes**);

    HRESULT STDMETHODCALLTYPE GetInputStreamAttributes(
        DWORD dwInputStreamID,
        IMFAttributes**);

    HRESULT STDMETHODCALLTYPE GetOutputStreamAttributes(
        DWORD dwOutputStreamID,
        IMFAttributes**);

    HRESULT STDMETHODCALLTYPE DeleteInputStream(DWORD dwStreamID);

    HRESULT STDMETHODCALLTYPE AddInputStreams(
        DWORD cStreams,
        DWORD* adwStreamIDs);

    HRESULT STDMETHODCALLTYPE GetInputAvailableType(
        DWORD dwInputStreamID,
        DWORD dwTypeIndex,
        IMFMediaType**);

    HRESULT STDMETHODCALLTYPE GetOutputAvailableType(
        DWORD dwOutputStreamID,
        DWORD dwTypeIndex,
        IMFMediaType**);

    HRESULT STDMETHODCALLTYPE SetInputType(
        DWORD dwInputStreamID,
        IMFMediaType*,
        DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetOutputType(
        DWORD dwOutputStreamID,
        IMFMediaType*,
        DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetInputCurrentType(
        DWORD dwInputStreamID,
        IMFMediaType**);

    HRESULT STDMETHODCALLTYPE GetOutputCurrentType(
        DWORD dwOutputStreamID,
        IMFMediaType**);

    HRESULT STDMETHODCALLTYPE GetInputStatus(
        DWORD dwInputStreamID,
        DWORD* pdwFlags);

    HRESULT STDMETHODCALLTYPE GetOutputStatus(
        DWORD* pdwFlags);

    HRESULT STDMETHODCALLTYPE SetOutputBounds(
        LONGLONG hnsLowerBound,
        LONGLONG hnsUpperBound);

    HRESULT STDMETHODCALLTYPE ProcessEvent(
        DWORD dwInputStreamID,
        IMFMediaEvent*);

    HRESULT STDMETHODCALLTYPE ProcessMessage(
        MFT_MESSAGE_TYPE,
        ULONG_PTR);

    HRESULT STDMETHODCALLTYPE ProcessInput(
        DWORD dwInputStreamID,
        IMFSample*,
        DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE ProcessOutput(
        DWORD dwFlags,
        DWORD cOutputBufferCount,
        MFT_OUTPUT_DATA_BUFFER* pOutputSamples,
        DWORD* pdwStatus);

private:

    explicit WebmMfVorbisDec(IClassFactory*);
    virtual ~WebmMfVorbisDec();

    HRESULT CreateVorbisDecoder(IMFMediaType* pmt);
    void DestroyVorbisDecoder();
    HRESULT NextOggPacket(BYTE* p_packet, DWORD packet_size);
    HRESULT ValidateOutputFormat(IMFMediaType *pmt);
    HRESULT CreateMediaBuffer(DWORD size, IMFMediaBuffer** pp_buffer);
    HRESULT DecodeVorbisFormat2Sample(IMFSample* p_mf_input_sample);
    HRESULT ProcessLibVorbisOutput(IMFSample* p_mf_output_sample,
                                   UINT32 samples_to_process);

    HRESULT GetOutputMediaType(DWORD dwTypeIndex, IMFMediaType** pp) const;
    HRESULT GenerateMediaType(IMFMediaType** pp,
                              int channels,
                              UINT32 mask,
                              UINT32 samples_per_sec,
                              GUID type) const;
    UINT32 GetChannelMask(int channels) const;
    bool FormatSupported(bool is_input, IMFMediaType* p_mediatype);

    HRESULT ResetMediaType(bool reset_input);

    bool PostProcessSamples(float* const p_vorbis_float_buffer,
                            UINT32 samples_to_process,
                            BYTE* p_mf_buffer_data);
    REFERENCE_TIME SamplesToMediaTime(UINT64 num_samples) const;
    UINT64 MediaTimeToSamples(REFERENCE_TIME media_time) const;

    // Scratch buffer used by the filter to convert samples from one format to
    // another.
    WebmUtil::auto_array<unsigned char> m_scratch;

    // when |m_drain| is true |m_min_output_threshold| is ignored, and we pass
    // all available samples downstream
    bool m_drain;
    // When |m_post_process_samples| is true the filter will need to process
    // the output from the decoder because the input format does not match
    // the output format.
    bool m_post_process_samples;

    IClassFactory* const m_pClassFactory;

    _COM_SMARTPTR_TYPEDEF(IMFMediaType, __uuidof(IMFMediaType));
    IMFMediaTypePtr m_input_mediatype;
    IMFMediaTypePtr m_output_mediatype;

    LONG m_cRef;

    typedef std::list<IMFSample*> mf_input_samples_t;
    mf_input_samples_t m_mf_input_samples;

    // minimum buffered uncompressed audio time that must be accumulated
    // before |ProcessOutput| will return samples to the caller
    const REFERENCE_TIME m_min_output_threshold;
    REFERENCE_TIME m_decode_start_time;
    REFERENCE_TIME m_start_time;
    // DEBUG
    REFERENCE_TIME m_mediatime_decoded;
    REFERENCE_TIME m_mediatime_recvd;

    UINT32 m_block_align;
    UINT64 m_total_samples_decoded;

    VorbisDecoder m_vorbis_decoder;

    DISALLOW_COPY_AND_ASSIGN(WebmMfVorbisDec);
};

}  //end namespace WebmMfVorbisDecLib
