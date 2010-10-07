#pragma once

namespace WebmMfVp8DecLib
{

class WebmMfVp8Dec : public IMFTransform,
                     //TODO: public IVP8PostProcessing,
                     public IMFRateControl,
                     public IMFRateSupport,
                     public IMFGetService,
                     public CLockable
{
    friend HRESULT CreateDecoder(
            IClassFactory*,
            IUnknown*,
            const IID&,
            void**);

    WebmMfVp8Dec(const WebmMfVp8Dec&);
    WebmMfVp8Dec& operator=(const WebmMfVp8Dec&);

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

    //IMFRateControl

    HRESULT STDMETHODCALLTYPE SetRate(BOOL, float);

    HRESULT STDMETHODCALLTYPE GetRate(BOOL*, float*);

    //IMFRateSupport

    HRESULT STDMETHODCALLTYPE GetSlowestRate(MFRATE_DIRECTION, BOOL, float*);

    HRESULT STDMETHODCALLTYPE GetFastestRate(MFRATE_DIRECTION, BOOL, float*);

    HRESULT STDMETHODCALLTYPE IsRateSupported(BOOL, float, float*);

    //IMFGetService

    HRESULT STDMETHODCALLTYPE GetService(REFGUID, REFIID, LPVOID*);

private:

    explicit WebmMfVp8Dec(IClassFactory*);
    virtual ~WebmMfVp8Dec();

    IClassFactory* const m_pClassFactory;
    LONG m_cRef;

    struct FrameSize { UINT32 width; UINT32 height; };
    struct FrameRate { UINT32 numerator; UINT32 denominator; };

    IMFMediaType* m_pInputMediaType;
    IMFMediaType* m_pOutputMediaType;

    typedef std::list<IMFSample*> samples_t;
    samples_t m_samples;

    vpx_codec_ctx_t m_ctx;

    float m_rate;
    BOOL m_bThin;

    DWORD GetOutputBufferSize(FrameSize&) const;
    HRESULT GetFrame(BYTE*, ULONG, const GUID&);

};

}  //end namespace WebmMfVp8DecLib
