#include <strmif.h>
#include <string>
#include "vp8encoderidl.h"
#include "vp8encoderproppage.hpp"
#include "resource.h"
#include <new>
#include <cassert>
#include <sstream>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
using std::hex;
using std::dec;
#endif

using std::wstring;
using std::wistringstream;
using std::wostringstream;

extern HMODULE g_hModule;

namespace VP8EncoderLib
{

extern const CLSID CLSID_PropPage = { /* ED311102-5211-11DF-94AF-0026B977EEAA */
    0xED311102,
    0x5211,
    0x11DF,
    {0x94, 0xAF, 0x00, 0x26, 0xB9, 0x77, 0xEE, 0xAA}
};


HRESULT CreatePropPage(
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

    PropPage* const p = new (std::nothrow) PropPage(pClassFactory);

    if (p == 0)
        return E_OUTOFMEMORY;

    assert(p->m_cRef == 1);

    IPropertyPage* const pUnk = static_cast<IPropertyPage*>(p);

    const HRESULT hr = pUnk->QueryInterface(iid, ppv);

    const ULONG n = pUnk->Release();
    n;

    return hr;
}


PropPage::PropPage(IClassFactory* pClassFactory) :
    m_pClassFactory(pClassFactory),
    m_cRef(1),  //see PropPageCreateInstance
    m_pSite(0),
    m_pVP8(0),
    m_hWnd(0),
    m_bDirty(false)
{
    m_pClassFactory->LockServer(TRUE);

#ifdef _DEBUG
    wodbgstream os;
    os << "PropPage::ctor" << endl;
#endif
}


PropPage::~PropPage()
{
#ifdef _DEBUG
    wodbgstream os;
    os << "PropPage::dtor" << endl;
#endif

    assert(m_pSite == 0);
    assert(m_pVP8 == 0);
    assert(m_hWnd == 0);

    m_pClassFactory->LockServer(FALSE);
}


HRESULT PropPage::QueryInterface(
    const IID& iid,
    void** ppv)
{
    if (ppv == 0)
        return E_POINTER;

    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);

    if (iid == __uuidof(IUnknown))
    {
        pUnk = static_cast<IPropertyPage*>(this);  //must be non-delegating
    }
    else if (iid == __uuidof(IPropertyPage))
    {
        pUnk = static_cast<IPropertyPage*>(this);
    }
    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }

    pUnk->AddRef();
    return S_OK;
}


ULONG PropPage::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}


ULONG PropPage::Release()
{
    if (LONG n = InterlockedDecrement(&m_cRef))
        return n;

    SetPageSite(0);
    SetObjects(0, 0);

    delete this;
    return 0;
}


HRESULT PropPage::SetPageSite(IPropertyPageSite* pSite)
{
    if (pSite)
    {
        if (m_pSite)
            return E_UNEXPECTED;

        m_pSite = pSite;
        m_pSite->AddRef();
    }
    else if (m_pSite)
    {
        m_pSite->Release();
        m_pSite = 0;
    }

    return S_OK;
}


HRESULT PropPage::Activate(HWND hWndParent, LPCRECT prc, BOOL /* fModal */ )
{
    wodbgstream os;
    os << "activate" << endl;

    if (m_pVP8 == 0)  //SetObjects hasn't been called yet
        return E_UNEXPECTED;

    if (m_hWnd)
        return E_UNEXPECTED;

    const HWND hWnd = CreateDialogParam(
                        g_hModule,
                        MAKEINTRESOURCE(IDD_PROPPAGE_VP8ENCODER),
                        hWndParent,
                        &PropPage::DialogProc,
                        reinterpret_cast<LPARAM>(this));

    if (hWnd == 0)
    {
        const DWORD e = GetLastError();
        return HRESULT_FROM_WIN32(e);
    }

    GetDeadline(hWnd);

    m_hWnd = hWnd;

    Move(prc);
    return Show(SW_SHOWNORMAL);
}


HRESULT PropPage::Deactivate()
{
    wodbgstream os;
    os << "deactivate" << endl;

    if (m_hWnd == 0)
        return E_UNEXPECTED;

    //Set CONTROLPARENT back to false before destroying this dlg.
    LONG dwStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
    dwStyle &= ~WS_EX_CONTROLPARENT;

    const HWND hWnd = m_hWnd;
    m_hWnd = 0;
    SetWindowLong(hWnd, GWL_EXSTYLE, dwStyle);
    m_hWnd = hWnd;

    //OnDeactivate

    BOOL b = DestroyWindow(m_hWnd);
    b;

    m_hWnd = 0;

    return S_OK;
}


HRESULT PropPage::GetPageInfo(PROPPAGEINFO* pInfo)
{
    wodbgstream os;
    os << "getpageinfo" << endl;

    assert(m_hWnd == 0);

    if (pInfo == 0)
        return E_POINTER;

    PROPPAGEINFO& info = *pInfo;

    info.cb = sizeof info;

    const wchar_t title[] = L"VP8 Encoder Properties";
    const size_t len = 1 + wcslen(title);
    const size_t cb = len * sizeof(wchar_t);

    info.pszTitle = (wchar_t*)CoTaskMemAlloc(cb);

    if (info.pszTitle)
    {
        const errno_t e = wcscpy_s(info.pszTitle, len, title);
        e;
        assert(e == 0);
    }

    info.pszDocString = 0;
    info.pszHelpFile = 0;
    info.dwHelpContext = 0;

    const HWND hWnd = CreateDialogParam(
                        g_hModule,
                        MAKEINTRESOURCE(IDD_PROPPAGE_VP8ENCODER),
                        GetDesktopWindow(),
                        &PropPage::DialogProc,
                        0);

    if (hWnd == 0)
    {
        const DWORD e = GetLastError();
        return HRESULT_FROM_WIN32(e);
    }

    RECT rc;

    BOOL b = GetWindowRect(hWnd, &rc);

    if (!b)
    {
        const DWORD e = GetLastError();
        return HRESULT_FROM_WIN32(e);
    }

    SIZE& size = info.size;

    size.cx = rc.right - rc.left;
    size.cy = rc.bottom - rc.top;

    b = DestroyWindow(hWnd);

    return S_OK;
}


HRESULT PropPage::SetObjects(ULONG n, IUnknown** ppUnk)
{
    if (n == 0)
    {
        if (m_pVP8)
        {
            m_pVP8->Release();
            m_pVP8 = 0;
        }

        return S_OK;
    }

    if (m_pVP8)
        return E_UNEXPECTED;

    if (ppUnk == 0)
        return E_INVALIDARG;

    IUnknown** const ppUnk_end = ppUnk + n;

    while (ppUnk != ppUnk_end)
    {
        IUnknown* const pUnk = *ppUnk++;

        if (pUnk == 0)
            return E_INVALIDARG;

        const HRESULT hr = pUnk->QueryInterface(&m_pVP8);

        if (SUCCEEDED(hr))
        {
            assert(m_pVP8);

            //TODO: some init here will probably be req'd
            return S_OK;
        }
    }

    return E_NOINTERFACE;
}


HRESULT PropPage::Show(UINT nCmdShow)
{
    if (m_hWnd == 0)
        return E_UNEXPECTED;

    switch (nCmdShow)
    {
        case SW_SHOW:
        case SW_SHOWNORMAL:
        case SW_HIDE:
            break;

        default:
            return E_INVALIDARG;
    }

    BOOL b = ShowWindow(m_hWnd, nCmdShow);
    b = InvalidateRect(m_hWnd, 0, TRUE);

    return S_OK;
}


HRESULT PropPage::Move(LPCRECT prc)
{
    if (m_hWnd == 0)
        return E_UNEXPECTED;

    if (prc == 0)
        return E_INVALIDARG;

    const RECT& rc = *prc;

    const LONG x = rc.left;
    const LONG y = rc.top;
    const LONG w = rc.right - rc.left;
    const LONG h = rc.bottom - rc.top;

    const BOOL b = MoveWindow(m_hWnd, x, y, w, h, TRUE);
    b;

    return S_OK;
}

HRESULT PropPage::IsPageDirty()
{
    wodbgstream os;
    os << "ispagedirty" << endl;

    return m_bDirty ? S_OK : S_FALSE;
}


HRESULT PropPage::Apply()
{
    wodbgstream os;
    os << "apply" << endl;

    if (m_pVP8 == 0)
        return E_UNEXPECTED;

    if (m_pSite == 0)
        return E_UNEXPECTED;

    if (!m_bDirty)
        return S_OK;

    HRESULT hr = SetDeadline();

    if (hr != S_OK)
        return hr;

    m_bDirty = false;

    return S_OK;
}


HRESULT PropPage::Help(LPCOLESTR)
{
    return E_NOTIMPL;
}


HRESULT PropPage::TranslateAccelerator(MSG*)
{
    return E_NOTIMPL;
}


INT_PTR PropPage::DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    //wodbgstream os;
    //os << "DlgProc: msg=0x" << hex << msg << dec << endl;

    if (msg == WM_INITDIALOG)
    {
        SetLastError(0);
        SetWindowLongPtr(hWnd, DWLP_USER, lParam);

        const DWORD e = GetLastError();
        e;
        assert(e == 0);

        return TRUE;
    }

    SetLastError(0);
    const LONG_PTR ptr = GetWindowLongPtr(hWnd, DWLP_USER);

    const DWORD e = GetLastError();
    e;
    assert(e == 0);

    if (ptr == 0)
        return FALSE;

    PropPage* const pPage = reinterpret_cast<PropPage*>(ptr);

    if (pPage->m_hWnd == 0)
        return FALSE;

    assert(pPage->m_hWnd == hWnd);

    switch (msg)
    {
        case WM_STYLECHANGING:
        {
            if (wParam == GWL_EXSTYLE)
            {
                STYLESTRUCT* const p = reinterpret_cast<STYLESTRUCT*>(lParam);
                assert(p);

                p->styleNew |= WS_EX_CONTROLPARENT;
            }

            return FALSE;
        }
        case WM_COMMAND:
            return pPage->OnCommand(wParam, lParam);

        default:
            return FALSE;
    }
}


INT_PTR PropPage::OnCommand(WPARAM wParam, LPARAM)
{
    const WORD code = HIWORD(wParam);
    const WORD id = LOWORD(wParam);

    //#define EN_SETFOCUS         0x0100
    //#define EN_KILLFOCUS        0x0200
    //#define EN_CHANGE           0x0300
    //#define EN_UPDATE           0x0400
    //#define EN_ERRSPACE         0x0500
    //#define EN_MAXTEXT          0x0501
    //#define EN_HSCROLL          0x0601
    //#define EN_VSCROLL          0x0602

    wodbgstream os;

    //os << "OnCommand: code=0x" << hex << code << dec << " id=" << id << endl;
    //
    //if (code == EN_CHANGE)
    //    os << "EN_CHANGE" << endl;

    if (code == EN_CHANGE)
    {
        os << "OnCommand: code=EN_CHANGE; id=" << id << endl;

        m_bDirty = true;

        if (m_pSite)
            m_pSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
    }

    return FALSE;
}


DWORD PropPage::SetText(HWND hWnd, int id, const std::wstring& str)
{
    const BOOL b = ::SetDlgItemText(hWnd, id, str.c_str());

    if (b)
        return 0;  //SUCCESS

    return GetLastError();
}


DWORD PropPage::GetText(int id, std::wstring& str) const
{
    int count = 64;

    for (;;)
    {
        const size_t size = count * sizeof(wchar_t);
        wchar_t* buf = (wchar_t*)_malloca(size);

        const UINT result = ::GetDlgItemText(m_hWnd, id, buf, count);

        if (result == 0)
        {
            if (DWORD e = GetLastError())
                return e;
        }

        if ((result + 1) >= UINT(count))
        {
            count *= 2;
            continue;
        }

        for (;;)  //strip leading whitespace
        {
            if (*buf == L'\0')
            {
                str.clear();
                return 0;
            }

            if (*buf == L' ')
            {
                ++buf;
                continue;
            }

            if (iswcntrl(*buf))
            {
                ++buf;
                continue;
            }

            str = buf;
            break;
        }

        for (;;)  //strip trailing whitespace
        {
            const wstring::size_type len = str.length();
            assert(len > 0);

            const wstring::size_type off = len - 1;
            const wchar_t c = str[off];

            if ((c == L' ') || iswcntrl(c))
            {
                str.erase(off, 1);
                continue;
            }

            return 0;
        }
    }
}


HRESULT PropPage::GetDeadline(HWND hWnd)
{
    int val;

    assert(m_pVP8);

    HRESULT hr = m_pVP8->GetDeadline(&val);

    if (FAILED(hr))
    {
        MessageBox(
            m_hWnd,
            L"Unable to get deadline value from filter.",
            L"Error",
            MB_OK);

        return hr;
    }

    wostringstream os;

    if (val < 0)
        os << kDeadlineGoodQuality;
    else
        os << val;

    const DWORD e = SetText(hWnd, IDC_DEADLINE, os.str());

    if (e == 0)
        return S_OK;

    MessageBox(
        hWnd,
        L"Unable to set value for deadline edit control.",
        L"Error",
        MB_OK);

    return E_FAIL;
}


HRESULT PropPage::SetDeadline()
{
    wstring text;

    const DWORD e = GetText(IDC_DEADLINE, text);

    if (e)
    {
        MessageBox(m_hWnd, L"Unable to get deadline value.", L"Error", MB_OK);
        return S_FALSE;
    }

    if (text.empty())
        return S_OK;

    //TODO: must handle case "default", or "good", etc

    wistringstream is(text);

    int val;

    if (!(is >> val) || !is.eof())
    {
        MessageBox(
            m_hWnd,
            L"Bad deadline value.",
            L"Error",
            //MB_RETRYCANCEL | MB_ICONEXCLAMATION | MB_DEFBUTTON1);
            MB_OK | MB_ICONEXCLAMATION);

        return S_FALSE;
    }

    if (val < 0)  //treat as nonce values
        return S_OK;

    assert(m_pVP8);

    HRESULT hr = m_pVP8->SetDeadline(val);

    if (FAILED(hr))
    {
        MessageBox(m_hWnd, L"Unable to set deadline value.", L"Error", MB_OK);
        return S_FALSE;
    }

    return S_OK;
}


}  //end namespace VP8EncoderLib
