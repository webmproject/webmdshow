#pragma warning(disable:4505)  //unreferenced local function removed
#include "clockable.hpp"
#include <mfidl.h>
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"
#include <list>
#include "webmmfvp8dec.hpp"
#include "webmtypes.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <comdef.h>
#include <cassert>
#include <new>
#ifdef _DEBUG
#include "odbgstream.hpp"
//#include "iidstr.hpp"
using std::endl;
using std::boolalpha;
#endif

_COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, __uuidof(IMFMediaBuffer));
_COM_SMARTPTR_TYPEDEF(IMF2DBuffer, __uuidof(IMF2DBuffer));


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

    IMFTransform* const pUnk = p;

    const HRESULT hr = pUnk->QueryInterface(iid, ppv);

    const ULONG cRef = pUnk->Release();
    cRef;

    return hr;
}


WebmMfVp8Dec::WebmMfVp8Dec(IClassFactory* pClassFactory) :
    m_pClassFactory(pClassFactory),
    m_cRef(1),
    m_pInputMediaType(0),
    m_pOutputMediaType(0),
    m_rate(1),
    m_bThin(FALSE)
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

        m_pInputMediaType = 0;

        const vpx_codec_err_t e = vpx_codec_destroy(&m_ctx);
        e;
        assert(e == VPX_CODEC_OK);
    }

    if (m_pOutputMediaType)
    {
        const ULONG n = m_pOutputMediaType->Release();
        n;
        assert(n == 0);

        m_pOutputMediaType = 0;
    }

    Flush();

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
        pUnk = static_cast<IMFTransform*>(this);  //must be nondelegating
    }
    else if (iid == __uuidof(IMFTransform))
    {
        pUnk = static_cast<IMFTransform*>(this);
    }
    else if (iid == __uuidof(IMFRateControl))
    {
        pUnk = static_cast<IMFRateControl*>(this);
    }
    else if (iid == __uuidof(IMFRateSupport))
    {
        pUnk = static_cast<IMFRateSupport*>(this);
    }
    else if (iid == __uuidof(IMFGetService))
    {
        pUnk = static_cast<IMFGetService*>(this);
    }
    else
    {
//#ifdef _DEBUG
//        wodbgstream os;
//        os << "WebmMfVp8Dec::QI: iid=" << IIDStr(iid) << std::endl;
//#endif

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

    info.cbMaxLookahead = 0;
    info.hnsMaxLatency = 0;  //TODO: Is 0 correct?
    //TODO: does lag-in-frames matter here?
    //See "_MFT_INPUT_STREAM_INFO_FLAGS Enumeration" for more info:
    //http://msdn.microsoft.com/en-us/library/ms703975%28v=VS.85%29.aspx

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
                   //MFT_INPUT_STREAM_DOES_NOT_ADDREF;

    info.cbSize = 0;       //input size is variable
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
    //is the OPTIONAL flag.  We need the frame dimensions, and the stride,
    //in order to calculte the cbSize value.

    info.dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES |
                   MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
                   MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;

    FrameSize size;

    info.cbSize = GetOutputBufferSize(size);
    info.cbAlignment = 0;

    return S_OK;
}


DWORD WebmMfVp8Dec::GetOutputBufferSize(FrameSize& s) const
{
    //MFT was already locked by caller

    //TODO: for now, assume width and height are specified
    //via the input media type.

    if (m_pInputMediaType == 0)
        return 0;

    HRESULT hr = MFGetAttributeSize(
                    m_pInputMediaType,
                    MF_MT_FRAME_SIZE,
                    &s.width,
                    &s.height);

    assert(SUCCEEDED(hr));

    const DWORD w = s.width;
    assert(w);

    const DWORD h = s.height;
    assert(h);

    const DWORD cb = w*h + 2*(((w+1)/2)*((h+1)/2));

    //TODO: this result does not account for stride

    return cb;
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

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    hr = MFCreateMediaType(&pmt);
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

    if (m_pInputMediaType == 0)
        return S_OK;

    FrameRate r;

    hr = MFGetAttributeRatio(
            m_pInputMediaType,
            MF_MT_FRAME_RATE,
            &r.numerator,
            &r.denominator);

    if (SUCCEEDED(hr))
    {
        assert(r.denominator);
        assert(r.numerator);

        hr = MFSetAttributeRatio(
                pmt,
                MF_MT_FRAME_RATE,
                r.numerator,
                r.denominator);

        assert(SUCCEEDED(hr));
    }

    FrameSize s;

    hr = MFGetAttributeSize(
            m_pInputMediaType,
            MF_MT_FRAME_SIZE,
            &s.width,
            &s.height);

    assert(SUCCEEDED(hr));
    assert(s.width);
    assert(s.height);

    hr = MFSetAttributeSize(
            pmt,
            MF_MT_FRAME_SIZE,
            s.width,
            s.height);

    assert(SUCCEEDED(hr));

    return S_OK;
}


HRESULT WebmMfVp8Dec::SetInputType(
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

            const vpx_codec_err_t e = vpx_codec_destroy(&m_ctx);
            e;
            assert(e == VPX_CODEC_OK);
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
        r.numerator = 0;  //means "not set"
    else
    {
        if (r.denominator == 0)
            return MF_E_INVALIDMEDIATYPE;

        if (r.numerator == 0)
            return MF_E_INVALIDMEDIATYPE;
    }

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

    if (s.width % 2)  //TODO
        return MF_E_INVALIDMEDIATYPE;

    if (s.height == 0)
        return MF_E_INVALIDMEDIATYPE;

    //TODO: do we need to check for odd height too?

    if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (m_pInputMediaType)
    {
        hr = m_pInputMediaType->DeleteAllItems();
        assert(SUCCEEDED(hr));

        const vpx_codec_err_t e = vpx_codec_destroy(&m_ctx);
        e;
        assert(e == VPX_CODEC_OK);
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

    //TODO: should this really be done here?

    vpx_codec_iface_t& vp8 = vpx_codec_vp8_dx_algo;

    const int flags = 0;  //TODO: VPX_CODEC_USE_POSTPROC;

    const vpx_codec_err_t err = vpx_codec_dec_init(
                                    &m_ctx,
                                    &vp8,
                                    0,
                                    flags);

    if (err == VPX_CODEC_MEM_ERROR)
        return E_OUTOFMEMORY;

    if (err != VPX_CODEC_OK)
        return E_FAIL;

    //const HRESULT hr = OnApplyPostProcessing();

    if (m_pOutputMediaType)
    {
        //TODO: Is this the correct behavior?

        const ULONG n = m_pOutputMediaType->Release();
        n;
        assert(n == 0);

        m_pOutputMediaType = 0;
    }

#if 0
    //TODO:
    //We could update the preferred ("available") output media types,
    //now that we know the frame rate and frame size, etc.

    hr = MFCreateMediaType(&m_pOutputMediaType);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YV12);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    assert(SUCCEEDED(hr));  //TODO

    hr = pmt->SetUINT32(MF_MT_COMPRESSED, FALSE);
    assert(SUCCEEDED(hr));  //TODO

    if (r.numerator)  //means "has been set"
    {
        hr = MFSetAttributeRatio(
                m_pOutputMediaType,
                MF_MT_FRAME_RATE,
                r.numerator,
                r.denominator);

        assert(SUCCEEDED(hr));  //TODO
    }

    hr = MFSetAttributeSize(
            m_pOutputMediaType,
            MF_MT_FRAME_SIZE,
            s.width,
            s.height);

    assert(SUCCEEDED(hr));  //TODO
#endif

    return S_OK;
}


HRESULT WebmMfVp8Dec::SetOutputType(
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

    if (g != MFMediaType_Video)
        return MF_E_INVALIDMEDIATYPE;

    hr = pmt->GetGUID(MF_MT_SUBTYPE, &g);

    if (FAILED(hr))
        return MF_E_INVALIDMEDIATYPE;

    if ((g != MFVideoFormat_YV12) &&
        (g != MFVideoFormat_IYUV))
    {
        //TODO: add I420 support

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
        FrameRate r_in;

        hr = MFGetAttributeRatio(
                m_pInputMediaType,
                MF_MT_FRAME_RATE,
                &r_in.numerator,
                &r_in.denominator);

        if (SUCCEEDED(hr))
        {
            if (r_out.denominator == 0)
                return MF_E_INVALIDMEDIATYPE;

            if (r_out.numerator == 0)
                return MF_E_INVALIDMEDIATYPE;

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

        FrameSize s_in;

        hr = MFGetAttributeSize(
                m_pInputMediaType,
                MF_MT_FRAME_SIZE,
                &s_in.width,
                &s_in.height);

        assert(SUCCEEDED(hr));

        if (s_out.width != s_in.width)
            return MF_E_INVALIDMEDIATYPE;

        if (s_out.height != s_in.height)
            return MF_E_INVALIDMEDIATYPE;
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

    //TODO: synthesize from input media type?

    if (m_pOutputMediaType == 0)  //TODO: liberalize?
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

    dwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;  //because we always queue

    return S_OK;
}


HRESULT WebmMfVp8Dec::GetOutputStatus(DWORD*)
{
#if 1
    return E_NOTIMPL;
#else
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

    const vpx_image_t* const f = vpx_codec_get_frame(&m_ctx, &m_iter);

    dwFlags = f ? MFT_OUTPUT_STATUS_SAMPLE_READY : 0;

    //TODO: alternatively, we could return E_NOTIMPL, which
    //forces client to call ProcessOutput to determine whether
    //a sample is ready.

    return S_OK;
#endif
}


HRESULT WebmMfVp8Dec::SetOutputBounds(
    LONGLONG /* hnsLowerBound */ ,
    LONGLONG /* hnsUpperBound */ )
{
    return E_NOTIMPL;  //TODO
}


HRESULT WebmMfVp8Dec::ProcessEvent(
    DWORD dwInputStreamID,
    IMFMediaEvent* /* pEvent */ )
{
    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

#if 0 //def _DEBUG
    if (pEvent)
    {
        MediaEventType t;

        HRESULT hr = pEvent->GetType(&t);
        assert(SUCCEEDED(hr));

        odbgstream os;
        os << "WebmMfVp8Dec::ProcessEvent: type=" << t << endl;
    }
#endif

    return E_NOTIMPL;  //TODO
}


HRESULT WebmMfVp8Dec::ProcessMessage(
    MFT_MESSAGE_TYPE m,
    ULONG_PTR)
{
#ifdef _DEBUG
    odbgstream os;
    os << "WebmMfVp8Dec::ProcessMessage(samples.size="
       << m_samples.size() << "): ";
#endif

    switch (m)
    {
        case MFT_MESSAGE_COMMAND_FLUSH:
#ifdef _DEBUG
            os << "COMMAND_FLUSH" << endl;
#endif

        //http://msdn.microsoft.com/en-us/library/dd940419%28v=VS.85%29.aspx

            Flush();
            return S_OK;

        case MFT_MESSAGE_COMMAND_DRAIN:
#ifdef _DEBUG
            os << "COMMAND_DRAIN" << endl;
#endif

        //http://msdn.microsoft.com/en-us/library/dd940418%28v=VS.85%29.aspx

        //TODO: input stream does not accept input in the MFT processes all
        //data from previous calls to ProcessInput.

            return S_OK;

        case MFT_MESSAGE_SET_D3D_MANAGER:
#ifdef _DEBUG
            os << "SET_D3D" << endl;
#endif

            return S_OK;

        case MFT_MESSAGE_DROP_SAMPLES:
#ifdef _DEBUG
            os << "DROP_SAMPLES" << endl;
#endif

            return S_OK;

        case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
#ifdef _DEBUG
            os << "NOTIFY_BEGIN_STREAMING" << endl;
#endif

        //http://msdn.microsoft.com/en-us/library/dd940421%28v=VS.85%29.aspx

        //TODO: init decoder library here, instead of during SetInputType

            return S_OK;

        case MFT_MESSAGE_NOTIFY_END_STREAMING:
#ifdef _DEBUG
            os << "NOTIFY_END_STREAMING" << endl;
#endif

        //http://msdn.microsoft.com/en-us/library/dd940423%28v=VS.85%29.aspx

        //NOTE: flush is not performed here

            return S_OK;

        case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
#ifdef _DEBUG
            os << "NOTIFY_EOS" << endl;
#endif

        //http://msdn.microsoft.com/en-us/library/dd940422%28v=VS.85%29.aspx

        //TODO: set discontinuity flag on first sample after this

            return S_OK;

        case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
#ifdef _DEBUG
            os << "NOTIFY_START_OF_STREAM" << endl;
#endif

            return S_OK;

        case MFT_MESSAGE_COMMAND_MARKER:
#ifdef _DEBUG
            os << "COMMAND_MARKER" << endl;
#endif

            return S_OK;

        default:
            return S_OK;
    }
}


HRESULT WebmMfVp8Dec::ProcessInput(
    DWORD dwInputStreamID,
    IMFSample* pSample,
    DWORD)
{
    if (dwInputStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (pSample == 0)
        return E_INVALIDARG;

    DWORD count;

    HRESULT hr = pSample->GetBufferCount(&count);
    assert(SUCCEEDED(hr));

    if (count == 0)
        return S_OK;  //TODO: is this an error?

    if (count > 1)
        return E_INVALIDARG;

    //TODO: check duration
    //TODO: check timestamp

    Lock lock;

    hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    if (m_pInputMediaType == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (m_pOutputMediaType == 0)  //TODO: need this check?
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    pSample->AddRef();
    m_samples.push_back(pSample);

    return S_OK;
}


HRESULT WebmMfVp8Dec::ProcessOutput(
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

#if 0 //def _DEBUG
    odbgstream os;
    os << "WebmMfVp8Dec::ProcessOutput (begin)" << endl;
#endif

    if (m_pInputMediaType == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (m_pOutputMediaType == 0)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (m_samples.empty())
        return MF_E_TRANSFORM_NEED_MORE_INPUT;

    if (cOutputBufferCount == 0)
        return E_INVALIDARG;

    if (pOutputSamples == 0)
        return E_INVALIDARG;

    //TODO: check if cOutputSamples > 1 ?

    MFT_OUTPUT_DATA_BUFFER& data = pOutputSamples[0];

    //data.dwStreamID should equal 0, but we ignore it

    IMFSample* const pSample_out = data.pSample;

    if (pSample_out == 0)
        return E_INVALIDARG;

    DWORD count;

    hr = pSample_out->GetBufferCount(&count);

    if (SUCCEEDED(hr) && (count != 1))
        return E_INVALIDARG;

    IMFMediaBufferPtr buf_out;

    hr = pSample_out->GetBufferByIndex(0, &buf_out);

    if (FAILED(hr) || !bool(buf_out))
        return E_INVALIDARG;

    _COM_SMARTPTR_TYPEDEF(IMFSample, __uuidof(IMFSample));

    const IMFSamplePtr pSample_in(m_samples.front(), false);  //don't addref
    assert(bool(pSample_in));

    m_samples.pop_front();

    IMFMediaBufferPtr buf_in;

    hr = pSample_in->GetBufferByIndex(0, &buf_in);
    assert(SUCCEEDED(hr));
    assert(buf_in);

    BYTE* ptr;
    DWORD len;

    hr = buf_in->Lock(&ptr, 0, &len);
    assert(SUCCEEDED(hr));
    assert(ptr);
    assert(len);

    const vpx_codec_err_t e = vpx_codec_decode(&m_ctx, ptr, len, 0, 0);
    //assert(e == VPX_CODEC_OK);  //TODO

    hr = buf_in->Unlock();
    assert(SUCCEEDED(hr));

    if (e != VPX_CODEC_OK)
        return MF_E_INVALID_STREAM_DATA;  //send Media Event too?  How?

    UINT32 bPreroll;

    hr = pSample_in->GetUINT32(WebmTypes::WebMSample_Preroll, &bPreroll);

    if (SUCCEEDED(hr) && (bPreroll != FALSE))
    {
#if 0
        odbgstream os;
        os << "WebmMfVp8Dec::ProcessOutput: received PREROLL flag";

        LONGLONG t;

        hr = pSample_in->GetSampleTime(&t);

        if (SUCCEEDED(hr))
            os << "; t[sec]=" << (double(t) / 10000000);

        os << endl;
#endif

        return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }

#if 0
    {
        odbgstream os;
        os << "WebmMfVp8Dec::ProcessOutput: non-PREROLL";

        LONGLONG t;

        hr = pSample_in->GetSampleTime(&t);

        if (SUCCEEDED(hr))
            os << "; t[sec]=" << (double(t) / 10000000);

        os << endl;
    }
#endif

    GUID subtype;

    hr = m_pOutputMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
    assert(SUCCEEDED(hr));
    assert((subtype == MFVideoFormat_YV12) || (subtype == MFVideoFormat_IYUV));

    //TODO:
    //MFVideoFormat_I420

    const DWORD fcc = subtype.Data1;

    FrameSize frame_size;

    const DWORD cbFrameLen = GetOutputBufferSize(frame_size);
    assert(cbFrameLen > 0);

    //The sequence of querying for the output buffer is described on
    //the page "Uncompressed Video Buffers".
    //http://msdn.microsoft.com/en-us/library/aa473821%28v=VS.85%29.aspx

    IMF2DBuffer* buf2d_out;

    hr = buf_out->QueryInterface(&buf2d_out);

    if (SUCCEEDED(hr))
    {
        assert(buf2d_out);

        LONG stride_out;

        hr = buf2d_out->Lock2D(&ptr, &stride_out);
        assert(SUCCEEDED(hr));
        assert(ptr);
        assert(stride_out > 0);  //top-down DIBs are positive, right?

        hr = GetFrame(ptr, stride_out, subtype);
        assert(SUCCEEDED(hr));

        //TODO: set output buffer length?

        hr = buf2d_out->Unlock2D();
        assert(SUCCEEDED(hr));

        buf2d_out->Release();
        buf2d_out = 0;
    }
    else
    {
        DWORD cbMaxLen;

        hr = buf_out->Lock(&ptr, &cbMaxLen, 0);
        assert(SUCCEEDED(hr));
        assert(ptr);

        assert(cbMaxLen >= cbFrameLen);

        //TODO: verify stride of output buffer
        //The page "Uncompressed Video Buffers" here:
        //http://msdn.microsoft.com/en-us/library/aa473821%28v=VS.85%29.aspx
        //explains how to calculate the "minimum stride":
        //  MF_MT_DEFAULT_STRIDE
        //  or, MFGetStrideForBitmapInfoHeader
        //  or, calculate it yourself

        INT32 stride_out;

        hr = pSample_out->GetUINT32(
                MF_MT_DEFAULT_STRIDE,
                (UINT32*)&stride_out);

        if (SUCCEEDED(hr) && (stride_out != 0))
            assert(stride_out > 0);
        else
        {
            const DWORD w = frame_size.width;
            LONG stride_out_;

            hr = MFGetStrideForBitmapInfoHeader(fcc, w, &stride_out_);

            if (SUCCEEDED(hr) && (stride_out_ != 0))
            {
                assert(stride_out_ > 0);
                stride_out = stride_out_;
            }
            else
            {
                assert((w % 2) == 0);  //TODO
                stride_out = w;  //TODO: is this correct???
            }
        }

        hr = GetFrame(ptr, stride_out, subtype);
        assert(SUCCEEDED(hr));

        hr = buf_out->SetCurrentLength(cbFrameLen);
        assert(SUCCEEDED(hr));

        hr = buf_out->Unlock();
        assert(SUCCEEDED(hr));
    }

    LONGLONG t;

    hr = pSample_in->GetSampleTime(&t);

    if (SUCCEEDED(hr))
    {
        assert(t >= 0);

        hr = pSample_out->SetSampleTime(t);
        assert(SUCCEEDED(hr));
    }

    hr = pSample_in->GetSampleDuration(&t);

    if (SUCCEEDED(hr))
    {
        assert(t >= 0);  //TODO: move this into predicate?

        hr = pSample_out->SetSampleDuration(t);
        assert(SUCCEEDED(hr));
    }

    //pSample_in->Release();

#if 0 //def _DEBUG
    os << "WebmMfVp8Dec::ProcessOutput (end)" << endl;
#endif

    return S_OK;
}


HRESULT WebmMfVp8Dec::GetFrame(
    BYTE* pOutBuf,
    ULONG strideOut,
    const GUID& subtype)
{
    assert(pOutBuf);
    assert(strideOut);
    assert((strideOut % 2) == 0);  //TODO: resolve this issue

    vpx_codec_iter_t iter = 0;

    const vpx_image_t* f = vpx_codec_get_frame(&m_ctx, &iter);
    assert(f);  //TODO: this will fail if alt-ref frame ("invisible")

    //Y

    const BYTE* pInY = f->planes[PLANE_Y];
    assert(pInY);

    unsigned int wIn = f->d_w;
    unsigned int hIn = f->d_h;

    BYTE* pOut = pOutBuf;

    const int strideInY = f->stride[PLANE_Y];

    for (unsigned int y = 0; y < hIn; ++y)
    {
        memcpy(pOut, pInY, wIn);
        pInY += strideInY;
        pOut += strideOut;
    }

    strideOut /= 2;

    wIn = (wIn + 1) / 2;
    hIn = (hIn + 1) / 2;

    const BYTE* pInV = f->planes[PLANE_V];
    assert(pInV);

    const int strideInV = f->stride[PLANE_V];

    const BYTE* pInU = f->planes[PLANE_U];
    assert(pInU);

    const int strideInU = f->stride[PLANE_U];

    if (subtype == MFVideoFormat_YV12)
    {
        //V

        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInV, wIn);
            pInV += strideInV;
            pOut += strideOut;
        }

        //U

        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInU, wIn);
            pInU += strideInU;
            pOut += strideOut;
        }
    }
    else
    {
        //U

        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInU, wIn);
            pInU += strideInU;
            pOut += strideOut;
        }

        //V

        for (unsigned int y = 0; y < hIn; ++y)
        {
            memcpy(pOut, pInV, wIn);
            pInV += strideInV;
            pOut += strideOut;
        }
    }

    f = vpx_codec_get_frame(&m_ctx, &iter);
    assert(f == 0);

    return S_OK;
}


HRESULT WebmMfVp8Dec::SetRate(BOOL bThin, float rate)
{
    //odbgstream os;
    //os << "WebmMfVp8Dec::SetRate: bThin="
    //   << boolalpha << (bThin ? true : false)
    //   << " rate="
    //   << rate
    //   << endl;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //if (m_pEvents == 0)
    //    return MF_E_SHUTDOWN;

    //if (bThin)
    //    return MF_E_THINNING_UNSUPPORTED;  //TODO

    if (rate < 0)
        return MF_E_REVERSE_UNSUPPORTED;  //TODO

    m_rate = rate;
    m_bThin = bThin;

    return S_OK;
}


HRESULT WebmMfVp8Dec::GetRate(BOOL* pbThin, float* pRate)
{
    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //if (m_pEvents == 0)
    //    return MF_E_SHUTDOWN;

    if (pbThin)
        *pbThin = m_bThin;

    if (pRate)  //return error when pRate ptr is NULL?
        *pRate = m_rate;

    //odbgstream os;
    //os << "WebmMfVp8Dec::GetRate: bThin="
    //   << boolalpha << m_bThin
    //   << " rate="
    //   << m_rate
    //   << endl;

    return S_OK;
}


HRESULT WebmMfVp8Dec::GetSlowestRate(
    MFRATE_DIRECTION d,
    BOOL /* bThin */ ,
    float* pRate)
{
    //odbgstream os;
    //os << "WebmMfVp8Dec::GetSlowestRate" << endl;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //if (m_pEvents == 0)
    //    return MF_E_SHUTDOWN;

    if (d == MFRATE_REVERSE)
        return MF_E_REVERSE_UNSUPPORTED;  //TODO

    //if (bThin)
    //    return MF_E_THINNING_UNSUPPORTED;  //TODO

    if (pRate == 0)
        return E_POINTER;

    float& r = *pRate;
    r = 0;  //?

    return S_OK;
}


HRESULT WebmMfVp8Dec::GetFastestRate(
    MFRATE_DIRECTION d,
    BOOL /* bThin */ ,
    float* pRate)
{
    //odbgstream os;
    //os << "WebmMfSource::GetFastestRate" << endl;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //if (m_pEvents == 0)
    //    return MF_E_SHUTDOWN;

    if (d == MFRATE_REVERSE)
        return MF_E_REVERSE_UNSUPPORTED;  //TODO

    //if (bThin)
    //    return MF_E_THINNING_UNSUPPORTED;  //TODO

    if (pRate == 0)
        return E_POINTER;

    float& r = *pRate;
    r = 64;  //?

    return S_OK;
}


HRESULT WebmMfVp8Dec::IsRateSupported(
    BOOL /* bThin */ ,
    float rate,
    float* pNearestRate)
{
    //odbgstream os;
    //os << "WebmMfVp8Dec::IsRateSupported: rate=" << rate << endl;

    Lock lock;

    HRESULT hr = lock.Seize(this);

    if (FAILED(hr))
        return hr;

    //if (m_pEvents == 0)
    //    return MF_E_SHUTDOWN;

    //if (bThin)
    //    return MF_E_THINNING_UNSUPPORTED;  //TODO

    if (rate < 0)
        return MF_E_REVERSE_UNSUPPORTED;  //TODO

    //float int_part;
    //const float frac_part = modf(rate, &int_part);

    if (pNearestRate)
        *pNearestRate = rate;

    return S_OK;  //TODO
}


HRESULT WebmMfVp8Dec::GetService(
    REFGUID sid,
    REFIID iid,
    LPVOID* ppv)
{
    if (sid == MF_RATE_CONTROL_SERVICE)
        return WebmMfVp8Dec::QueryInterface(iid, ppv);

    if (ppv)
        *ppv = 0;

    return MF_E_UNSUPPORTED_SERVICE;
}


void WebmMfVp8Dec::Flush()
{
    while (!m_samples.empty())
    {
        IMFSample* const pSample = m_samples.front();
        assert(pSample);

        m_samples.pop_front();

        pSample->Release();
    }
}


}  //end namespace WebmMfVp8DecLib
