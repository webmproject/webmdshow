#pragma once

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

    WebmMfVorbisDec(const WebmMfVorbisDec&);
    WebmMfVorbisDec& operator=(const WebmMfVorbisDec&);

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
    HRESULT ProcessLibVorbisOutputPcmSamples(IMFSample* p_mf_output_sample,
                                             int* p_out_samples_decoded);
    bool FormatSupported(bool is_input, IMFMediaType* p_mediatype);

    HRESULT ResetMediaType(bool reset_input);

    void SetOutputWaveFormat(GUID subtype);

    IClassFactory* const m_pClassFactory;
    LONG m_cRef;

    _COM_SMARTPTR_TYPEDEF(IMFMediaType, __uuidof(IMFMediaType));
    IMFMediaTypePtr m_input_mediatype;
    IMFMediaTypePtr m_output_mediatype;

    // TODO(tomfinegan): disambiguate the sample storage member names
    typedef std::list<IMFSample*> samples_t;
    samples_t m_samples;

    WAVEFORMATEX m_wave_format;
    REFERENCE_TIME m_total_time_decoded;
    REFERENCE_TIME m_stream_start_time;

    int m_audio_format_tag;

    VorbisDecoder m_vorbis_decoder;
};

}  //end namespace WebmMfVorbisDecLib
