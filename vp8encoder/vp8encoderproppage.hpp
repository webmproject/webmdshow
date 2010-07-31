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
    DWORD GetText(int, std::wstring&) const;

    HRESULT GetDeadline(HWND);
    HRESULT SetDeadline();

};

}  //end namespace VP8EncoderLib
