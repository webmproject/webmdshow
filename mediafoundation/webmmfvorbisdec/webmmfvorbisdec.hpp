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

    IClassFactory* const m_pClassFactory;
    LONG m_cRef;

    struct FrameSize { UINT32 width; UINT32 height; };
    struct FrameRate { UINT32 numerator; UINT32 denominator; };

    IMFMediaType* m_pInputMediaType;
    IMFMediaType* m_pOutputMediaType;

    typedef std::list<IMFSample*> samples_t;
    samples_t m_samples;

    DWORD GetOutputBufferSize(FrameSize&) const;
    HRESULT GetFrame(BYTE*, ULONG, const GUID&);

    HRESULT CreateVorbisDecoder(IMFMediaType* pmt);
    void DestroyVorbisDecoder();
    HRESULT NextOggPacket(BYTE* p_packet, DWORD packet_size);

    // vorbis members
    vorbis_info m_vorbis_info; // contains static bitstream settings
    vorbis_comment m_vorbis_comment; // contains user comments
    vorbis_dsp_state m_vorbis_state; // decoder state
    vorbis_block m_vorbis_block; // working space for packet->PCM decode

    ogg_packet m_ogg_packet;
    DWORD m_ogg_packet_count;
};

}  //end namespace WebmMfVorbisDecLib
