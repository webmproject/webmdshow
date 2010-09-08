#include "webmmfvp8dec.hpp"
#include "webmtypes.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <cassert>
#include <new>

namespace WebmMfVp8DecLib
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

    WebmMfVp8Dec* const p = new (std::nothrow) WebmMfVp8Dec(pClassFactory);

    if (p == 0)
        return E_OUTOFMEMORY;

    IUnknown* const pUnk = p;

    const HRESULT hr = pUnk->QueryInterface(iid, ppv);

    const ULONG cRef = pUnk->Release();
    cRef;

    return hr;
}


WebmMfVp8Dec::WebmMfVp8Dec(IClassFactory* pClassFactory) :
    m_pClassFactory(pClassFactory),
    m_cRef(1),
    m_pInputMediaType(0),
    m_pOutputMediaType(0)
{
    HRESULT hr = m_pClassFactory->LockServer(TRUE);
    assert(SUCCEEDED(hr));

    hr = CLockable::Init();
    assert(SUCCEEDED(hr));
}


WebmMfVp8Dec::~WebmMfVp8Dec()
{
    if (m_pInputMediaType)
    {
        const ULONG n = m_pInputMediaType->Release();
        n;
        assert(n == 0);
    }

    if (m_pOutputMediaType)
    {
        const ULONG n = m_pOutputMediaType->Release();
        n;
        assert(n == 0);
    }

    HRESULT hr = m_pClassFactory->LockServer(FALSE);
    assert(SUCCEEDED(hr));
}


HRESULT WebmMfVp8Dec::QueryInterface(
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


ULONG WebmMfVp8Dec::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}


ULONG WebmMfVp8Dec::Release()
{
    if (LONG n = InterlockedDecrement(&m_cRef))
        return n;

    delete this;
    return 0;
}


HRESULT WebmMfVp8Dec::GetStreamLimits(
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


HRESULT WebmMfVp8Dec::GetStreamCount(
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


HRESULT WebmMfVp8Dec::GetStreamIDs(
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


HRESULT WebmMfVp8Dec::GetInputStreamInfo(
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

    info.hnsMaxLatency = 0;  //?
    //TODO: does lag-in-frames matter here?

    info.dwFlags = 0;  //TODO

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

    info.cbSize = 0;  //input size is variable

    info.cbMaxLookahead = 0;  //TODO

    info.cbAlignment = 0;  //no specific alignment requirements

    return S_OK;
}


HRESULT WebmMfVp8Dec::GetOutputStreamInfo(
    DWORD dwOutputStreamID,
    MFT_OUTPUT_STREAM_INFO* pStreamInfo)
{
    if (pStreamInfo == 0)
        return E_POINTER;

    if (dwOutputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    MFT_OUTPUT_STREAM_INFO& info = *pStreamInfo;

    //DWORD dwFlags;
    //DWORD cbSize;
    //DWORD cbAlignment;

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
    //MFT_OUTPUT_STREAM_WHOLE_SAMPLES |
    //MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
    //MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE ;

    info.dwFlags = 0;  //TODO

    info.cbSize = 0;  //TODO

    info.cbAlignment = 0;  //TODO

    return S_OK;
}


HRESULT WebmMfVp8Dec::GetAttributes(IMFAttributes** pp)
{
    if (pp)
        *pp = 0;

    return E_NOTIMPL;
}


HRESULT WebmMfVp8Dec::GetInputStreamAttributes(
    DWORD,
    IMFAttributes** pp)
{
    if (pp)
        *pp = 0;

    return E_NOTIMPL;
}


HRESULT WebmMfVp8Dec::GetOutputStreamAttributes(
    DWORD,
    IMFAttributes** pp)
{
    if (pp)
        *pp = 0;

    return E_NOTIMPL;
}


HRESULT WebmMfVp8Dec::DeleteInputStream(DWORD)
{
    return E_NOTIMPL;
}


HRESULT WebmMfVp8Dec::AddInputStreams(
    DWORD,
    DWORD*)
{
    return E_NOTIMPL;
}


HRESULT WebmMfVp8Dec::GetInputAvailableType(
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

    hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    assert(SUCCEEDED(hr));

    hr = pmt->SetGUID(MF_MT_SUBTYPE, WebmTypes::MEDIASUBTYPE_VP80);
    assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, FALSE);
    assert(SUCCEEDED(hr));

    hr = pmt->SetUINT32(MF_MT_COMPRESSED, TRUE);
    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfVp8Dec::GetOutputAvailableType(
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

    HRESULT hr = MFCreateMediaType(&pmt);
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

    //TODO: I assume these will be affected by the characteristics
    //of the input stream, when it is actually connected.

    return S_OK;
}


HRESULT WebmMfVp8Dec::SetInputType(
    DWORD dwInputStreamID,
    IMFMediaType* pmt,
    DWORD dwFlags)
{
    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (pmt == 0)
    {
        //TODO: disallow this case while we're playing?

        if (m_pInputMediaType)
        {
            const ULONG n = m_pInputMediaType->Release();
            n;
            assert(n == 0);
        }

        return S_OK;
    }

    GUID g;

    HRESULT hr = pmt->GetMajorType(&g);

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    if (g != MFMediaType_Video)
        return MF_E_INVALIDMEDIATYPE;

    hr = pmt->GetGUID(MF_MT_SUBTYPE, &g);

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    if (g != WebmTypes::MEDIASUBTYPE_VP80)
        return MF_E_INVALIDMEDIATYPE;

    //hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
    //assert(SUCCEEDED(hr));

    FrameRate r;

    hr = MFGetAttributeRatio(
            pmt,
            MF_MT_FRAME_RATE,
            &r.numerator,
            &r.denominator);

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    if (r.denominator == 0)
        return MF_E_INVALIDMEDIATYPE;

    if (r.numerator == 0)
        return MF_E_INVALIDMEDIATYPE;

    FrameSize s;

    hr = MFGetAttributeSize(
            pmt,
            MF_MT_FRAME_SIZE,
            &s.width,
            &s.height);

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    if (s.width == 0)
        return MF_E_INVALIDMEDIATYPE;

    //TODO: check whether width is odd

    if (s.height == 0)
        return MF_E_INVALIDMEDIATYPE;

    if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (m_pInputMediaType)
    {
        hr = m_pInputMediaType->DeleteAllItems();
        assert(SUCCEEDED(hr));
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

    //TODO: do something

    return S_OK;
}


HRESULT WebmMfVp8Dec::SetOutputType(
    DWORD dwOutputStreamID,
    IMFMediaType* pmt,
    DWORD dwFlags)
{
    if (dwOutputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (pmt == 0)
    {
        //TODO: disallow this case while we're playing?

        if (m_pOutputMediaType)
        {
            const ULONG n = m_pOutputMediaType->Release();
            n;
            assert(n == 0);
        }

        return S_OK;
    }

    GUID g;

    HRESULT hr = pmt->GetMajorType(&g);

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
        if (r_out.denominator == 0)
            return MF_E_INVALIDMEDIATYPE;

        if (r_out.numerator == 0)
            return MF_E_INVALIDMEDIATYPE;

        if (m_pInputMediaType)
        {
            FrameRate r_in;

            hr = MFGetAttributeRatio(
                    pmt,
                    MF_MT_FRAME_RATE,
                    &r_in.numerator,
                    &r_in.denominator);

            assert(SUCCEEDED(hr));

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

        if (m_pInputMediaType)
        {
            FrameSize s_in;

            hr = MFGetAttributeSize(
                    pmt,
                    MF_MT_FRAME_SIZE,
                    &s_in.width,
                    &s_in.height);

            assert(SUCCEEDED(hr));

            if (s_out.width != s_in.width)
                return MF_E_INVALIDMEDIATYPE;

            if (s_out.height != s_in.height)
                return MF_E_INVALIDMEDIATYPE;
        }
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


HRESULT WebmMfVp8Dec::GetInputCurrentType(
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


HRESULT WebmMfVp8Dec::GetOutputCurrentType(
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

    if (m_pOutputMediaType == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    IMFMediaType*& p = *pp;

    hr = MFCreateMediaType(&p);

    if (FAILED(hr))
        return hr;

    return m_pOutputMediaType->CopyAllItems(p);
}


HRESULT WebmMfVp8Dec::GetInputStatus(
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

    //TODO: just say yes for now
    dwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;

    return S_OK;
}


HRESULT WebmMfVp8Dec::GetOutputStatus(
    DWORD* pdwFlags)
{
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

    //TODO: just say yes for now
    dwFlags = MFT_OUTPUT_STATUS_SAMPLE_READY;

    //TODO: alternatively, we could return E_NOTIMPL, which
    //forces client to call ProcessOutput to determine whether
    //a sample is ready.

    return S_OK;
}


HRESULT WebmMfVp8Dec::SetOutputBounds(
    LONGLONG /* hnsLowerBound */ ,
    LONGLONG /* hnsUpperBound */ )
{
    return E_NOTIMPL;  //TODO
}


HRESULT WebmMfVp8Dec::ProcessEvent(
    DWORD dwInputStreamID,
    IMFMediaEvent*)
{
    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    return E_NOTIMPL;  //TODO
}


HRESULT WebmMfVp8Dec::ProcessMessage(
    MFT_MESSAGE_TYPE,
    ULONG_PTR)
{
    return S_OK;  //TODO
}


HRESULT WebmMfVp8Dec::ProcessInput(
    DWORD dwInputStreamID,
    IMFSample* pSample,
    DWORD)
{
    return S_OK;  //TODO!
}


HRESULT WebmMfVp8Dec::ProcessOutput(
    DWORD dwFlags,
    DWORD cOutputBufferCount,
    MFT_OUTPUT_DATA_BUFFER* pOutputSamples,
    DWORD* pdwStatus)
{
    return S_OK;  //TODO!
}


}  //end namespace WebmMfVp8DecLib
