// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

namespace VP8EncoderLib
{

class PropPage : public IPropertyPage
{
    friend HRESULT CreatePropPage(
            IClassFactory*,
            IUnknown*,
            const IID&,
            void**);

    explicit PropPage(IClassFactory*);
    virtual ~PropPage();

    PropPage(const PropPage&);
    PropPage& operator=(const PropPage&);

public:

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    //IPropertyPage interface:

    HRESULT STDMETHODCALLTYPE SetPageSite(IPropertyPageSite*);
    HRESULT STDMETHODCALLTYPE Activate(HWND, LPCRECT, BOOL);
    HRESULT STDMETHODCALLTYPE Deactivate();
    HRESULT STDMETHODCALLTYPE GetPageInfo(PROPPAGEINFO*);
    HRESULT STDMETHODCALLTYPE SetObjects(ULONG, IUnknown**);
    HRESULT STDMETHODCALLTYPE Show(UINT);
    HRESULT STDMETHODCALLTYPE Move(LPCRECT);
    HRESULT STDMETHODCALLTYPE IsPageDirty();
    HRESULT STDMETHODCALLTYPE Apply();
    HRESULT STDMETHODCALLTYPE Help(LPCOLESTR);
    HRESULT STDMETHODCALLTYPE TranslateAccelerator(MSG*);

private:

    IClassFactory* const m_pClassFactory;
    LONG m_cRef;
    IPropertyPageSite* m_pSite;
    bool m_bDirty;
    IVP8Encoder* m_pVP8;
    HWND m_hWnd;

    static INT_PTR CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM);
    INT_PTR OnCommand(WPARAM, LPARAM);

    static DWORD SetText(HWND, int, const std::wstring&);
    static DWORD SetText(HWND, int);
    DWORD GetText(int, std::wstring&) const;

    typedef HRESULT (STDMETHODCALLTYPE IVP8Encoder::* pfnGetValue)(int*);

    HRESULT GetIntValue(
                HWND,
                pfnGetValue,
                int code,
                const wchar_t*);

    typedef HRESULT (STDMETHODCALLTYPE IVP8Encoder::* pfnSetValue)(int);

    HRESULT SetIntValue(
                pfnSetValue,
                int code,
                const wchar_t*);

    HRESULT GetDeadline(HWND);
    HRESULT SetDeadline();

    HRESULT GetThreadCount(HWND);
    HRESULT SetThreadCount();

    HRESULT GetErrorResilient(HWND);
    HRESULT SetErrorResilient();

    HRESULT GetDropframeThreshold(HWND);
    HRESULT SetDropframeThreshold();

    HRESULT GetResizeAllowed(HWND);
    HRESULT SetResizeAllowed();

    HRESULT GetResizeUpThreshold(HWND);
    HRESULT SetResizeUpThreshold();

    HRESULT GetResizeDownThreshold(HWND);
    HRESULT SetResizeDownThreshold();

    HRESULT GetEndUsage(HWND);
    HRESULT SetEndUsage();

    HRESULT GetLagInFrames(HWND);
    HRESULT SetLagInFrames();

    HRESULT GetTokenPartitions(HWND);
    HRESULT SetTokenPartitions();

    HRESULT GetTargetBitrate(HWND);
    HRESULT SetTargetBitrate();

    HRESULT GetMinQuantizer(HWND);
    HRESULT SetMinQuantizer();

    HRESULT GetMaxQuantizer(HWND);
    HRESULT SetMaxQuantizer();

    HRESULT GetUndershootPct(HWND);
    HRESULT SetUndershootPct();

    HRESULT GetOvershootPct(HWND);
    HRESULT SetOvershootPct();

    HRESULT GetDecoderBufferSize(HWND);
    HRESULT SetDecoderBufferSize();

    HRESULT GetDecoderBufferInitialSize(HWND);
    HRESULT SetDecoderBufferInitialSize();

    HRESULT GetDecoderBufferOptimalSize(HWND);
    HRESULT SetDecoderBufferOptimalSize();

    HRESULT GetKeyframeMode(HWND);
    HRESULT SetKeyframeMode();

    HRESULT GetKeyframeMinInterval(HWND);
    HRESULT SetKeyframeMinInterval();

    HRESULT GetKeyframeMaxInterval(HWND);
    HRESULT SetKeyframeMaxInterval();

    void Initialize(HWND);
    void InitializeEndUsage(HWND);
    void InitializeKeyframeMode(HWND);

    HRESULT Clear();
    HRESULT Reload();
    HRESULT Reset();

};

}  //end namespace VP8EncoderLib
