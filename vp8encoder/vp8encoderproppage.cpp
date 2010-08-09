#include <strmif.h>
#include <string>
#include "vp8encoderidl.h"
#include "vp8encoderproppage.hpp"
#include "resource.h"
#include <new>
#include <cassert>
#include <sstream>
#include <windowsx.h>
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

extern const CLSID CLSID_PropPage = { //ED311102-5211-11DF-94AF-0026B977EEAA
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
#if 0 //def _DEBUG
    wodbgstream os;
    os << "activate" << endl;
#endif

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

    InitializeEndUsage(hWnd);
    InitializeKeyframeMode(hWnd);
    Initialize(hWnd);

    m_hWnd = hWnd;

    Move(prc);
    return Show(SW_SHOWNORMAL);
}


void PropPage::InitializeEndUsage(HWND hWnd)
{
    const HWND hCtrl = GetDlgItem(hWnd, IDC_END_USAGE);
    assert(hCtrl);

    int idx = ComboBox_AddString(hCtrl, L"");
    assert(idx == 0);

    idx = ComboBox_AddString(hCtrl, L"VBR");
    assert(idx == 1);

    idx = ComboBox_AddString(hCtrl, L"CBR");
    assert(idx == 2);
}


void PropPage::InitializeKeyframeMode(HWND hWnd)
{
    const HWND hCtrl = GetDlgItem(hWnd, IDC_KEYFRAME_MODE);
    assert(hCtrl);

    int idx = ComboBox_AddString(hCtrl, L"");
    assert(idx == 0);

    idx = ComboBox_AddString(hCtrl, L"Disabled");
    assert(idx == 1);

    idx = ComboBox_AddString(hCtrl, L"Auto");
    assert(idx == 2);
}


void PropPage::Initialize(HWND hWnd)
{
    GetDeadline(hWnd);
    GetThreadCount(hWnd);
    GetErrorResilient(hWnd);
    GetDropframeThreshold(hWnd);
    GetResizeAllowed(hWnd);
    GetResizeUpThreshold(hWnd);
    GetResizeDownThreshold(hWnd);
    GetEndUsage(hWnd);
    GetLagInFrames(hWnd);
    GetTokenPartitions(hWnd);
    GetTargetBitrate(hWnd);
    GetMinQuantizer(hWnd);
    GetMaxQuantizer(hWnd);
    GetUndershootPct(hWnd);
    GetOvershootPct(hWnd);
    GetDecoderBufferSize(hWnd);
    GetDecoderBufferInitialSize(hWnd);
    GetDecoderBufferOptimalSize(hWnd);
    GetKeyframeMode(hWnd);
    GetKeyframeMinInterval(hWnd);
    GetKeyframeMaxInterval(hWnd);
}



HRESULT PropPage::Deactivate()
{
#if 0
    wodbgstream os;
    os << "deactivate" << endl;
#endif

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
#if 0
    wodbgstream os;
    os << "getpageinfo" << endl;
#endif

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
#if 0
    wodbgstream os;
    os << "ispagedirty" << endl;
#endif

    return m_bDirty ? S_OK : S_FALSE;
}


HRESULT PropPage::Apply()
{
#if 0
    wodbgstream os;
    os << "apply" << endl;
#endif

    if (m_pVP8 == 0)
        return E_UNEXPECTED;

    if (m_pSite == 0)
        return E_UNEXPECTED;

    if (!m_bDirty)
        return S_OK;

    SetDeadline();
    SetThreadCount();
    SetErrorResilient();
    SetDropframeThreshold();
    SetResizeAllowed();
    SetResizeUpThreshold();
    SetResizeDownThreshold();
    SetEndUsage();
    SetLagInFrames();
    SetTokenPartitions();
    SetTargetBitrate();
    SetMinQuantizer();
    SetMaxQuantizer();
    SetUndershootPct();
    SetOvershootPct();
    SetDecoderBufferSize();
    SetDecoderBufferInitialSize();
    SetDecoderBufferOptimalSize();
    SetKeyframeMode();
    SetKeyframeMinInterval();
    SetKeyframeMaxInterval();

    m_bDirty = false;

    const HRESULT hr = m_pVP8->ApplySettings();

    if (FAILED(hr))
    {
        MessageBox(m_hWnd, L"ApplySettings failed.", L"Error", MB_OK);
        return S_OK;  //?
    }

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

#if 0
    wodbgstream os;
    os << "OnCommand: code=0x" << hex << code << dec << " id=" << id << endl;
#endif

    //#define CBN_ERRSPACE        (-1)
    //#define CBN_SELCHANGE       1
    //#define CBN_DBLCLK          2
    //#define CBN_SETFOCUS        3
    //#define CBN_KILLFOCUS       4
    //#define CBN_EDITCHANGE      5
    //#define CBN_EDITUPDATE      6
    //#define CBN_DROPDOWN        7
    //#define CBN_CLOSEUP         8
    //#define CBN_SELENDOK        9
    //#define CBN_SELENDCANCEL    10

    //if (code == EN_CHANGE)
    //    os << "EN_CHANGE" << endl;

    //#define BN_CLICKED          0
    //#define BN_PAINT            1
    //#define BN_HILITE           2
    //#define BN_UNHILITE         3
    //#define BN_DISABLE          4
    //#define BN_DOUBLECLICKED    5
    //#if(WINVER >= 0x0400)
    //#define BN_PUSHED           BN_HILITE
    //#define BN_UNPUSHED         BN_UNHILITE
    //#define BN_DBLCLK           BN_DOUBLECLICKED
    //#define BN_SETFOCUS         6
    //#define BN_KILLFOCUS        7

    switch (code)
    {
        case CBN_SELCHANGE:
        case EN_CHANGE:
#if 0 //def _DEBUG
            os << "OnCommand: code=0x" << hex << code << dec
               << " id=" << id
               << " CHANGE"
               << endl;
#endif

            m_bDirty = true;

            if (m_pSite)
                m_pSite->OnStatusChange(PROPPAGESTATUS_DIRTY);

            return TRUE;

        case BN_CLICKED:
            if (id == IDC_CLEAR)
            {
                Clear();
                return TRUE;
            }

            if (id == IDC_RELOAD)
            {
                Reload();
                return TRUE;
            }

            if (id == IDC_RESET)
            {
                Reset();
                return TRUE;
            }

            return FALSE;

        default:
            return FALSE;
    }
}


HRESULT PropPage::Clear()
{
    const HWND hWnd = m_hWnd;
    m_hWnd = 0;

    SetText(hWnd, IDC_DEADLINE);
    SetText(hWnd, IDC_THREADCOUNT);
    SetText(hWnd, IDC_ERROR_RESILIENT);
    SetText(hWnd, IDC_DROPFRAME_THRESHOLD);
    ComboBox_SetCurSel(GetDlgItem(hWnd, IDC_END_USAGE), 0);
    SetText(hWnd, IDC_LAG_IN_FRAMES);
    SetText(hWnd, IDC_TOKEN_PARTITIONS);
    SetText(hWnd, IDC_TARGET_BITRATE);
    SetText(hWnd, IDC_MIN_QUANTIZER);
    SetText(hWnd, IDC_MAX_QUANTIZER);
    SetText(hWnd, IDC_UNDERSHOOT_PCT);
    SetText(hWnd, IDC_OVERSHOOT_PCT);
    SetText(hWnd, IDC_RESIZE_ALLOWED);
    SetText(hWnd, IDC_RESIZE_UP_THRESHOLD);
    SetText(hWnd, IDC_RESIZE_DOWN_THRESHOLD);
    SetText(hWnd, IDC_DECODER_BUFFER_SIZE);
    SetText(hWnd, IDC_DECODER_BUFFER_INITIAL_SIZE);
    SetText(hWnd, IDC_DECODER_BUFFER_OPTIMAL_SIZE);
    ComboBox_SetCurSel(GetDlgItem(hWnd, IDC_KEYFRAME_MODE), 0);
    SetText(hWnd, IDC_KEYFRAME_MIN_INTERVAL);
    SetText(hWnd, IDC_KEYFRAME_MAX_INTERVAL);

    m_hWnd = hWnd;
    m_bDirty = false;

    if (m_pSite)
        m_pSite->OnStatusChange(PROPPAGESTATUS_CLEAN);

    return S_OK;
}


HRESULT PropPage::Reload()
{
    const HWND hWnd = m_hWnd;
    m_hWnd = 0;

    Initialize(hWnd);

    m_hWnd = hWnd;
    m_bDirty = false;

    if (m_pSite)
        m_pSite->OnStatusChange(PROPPAGESTATUS_CLEAN);

    return S_OK;
}


HRESULT PropPage::Reset()
{
    assert(m_pVP8);

    HRESULT hr = m_pVP8->ResetSettings();

    if (FAILED(hr))
    {
        MessageBox(
            m_hWnd,
            L"Unable to reset settings.",
            L"Error",
            MB_OK);

        return hr;
    }

    const HWND hWnd = m_hWnd;
    m_hWnd = 0;

    Initialize(hWnd);

    m_hWnd = hWnd;
    m_bDirty = false;

    if (m_pSite)
        m_pSite->OnStatusChange(PROPPAGESTATUS_CLEAN);

    return S_OK;
}


DWORD PropPage::SetText(HWND hWnd, int id)
{
    const BOOL b = ::SetDlgItemText(hWnd, id, 0);

    if (b)
        return 0;  //SUCCESS

    return GetLastError();
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




HRESULT PropPage::SetIntValue(
    pfnSetValue SetValue,
    int code,
    const wchar_t* name)
{
    wstring text;

    const DWORD e = GetText(code, text);

    if (e)
    {
        wstring msg = L"Unable to get ";
        msg += name;
        msg += L" value from edit control.";

        MessageBox(m_hWnd, msg.c_str(), L"Error", MB_OK);
        return S_FALSE;
    }

    //TODO: we could interpret empty text to mean "set to default value",
    //which is exact what it means when the dialog box is first initialized.
    if (text.empty())
        return S_OK;

    wistringstream is(text);

    int val;

    if (!(is >> val) || !is.eof())
    {
        wstring msg = L"Bad ";
        msg += name;
        msg += L" value.";

        MessageBox(
            m_hWnd,
            msg.c_str(),
            L"Error",
            MB_OK | MB_ICONEXCLAMATION);

        return S_FALSE;
    }

    //As above, we could interpret this to mean "set to default".
    if (val < 0)  //treat as nonce values
        return S_OK;

    assert(m_pVP8);

    HRESULT hr = (m_pVP8->*SetValue)(val);

    if (FAILED(hr))
    {
        wstring msg = L"Unable to set ";
        msg += name;
        msg += L" value on filter.";

        MessageBox(m_hWnd, msg.c_str(), L"Error", MB_OK);
        return S_FALSE;
    }

    return S_OK;
}


HRESULT PropPage::GetIntValue(
    HWND hWnd,
    pfnGetValue GetValue,
    int code,
    const wchar_t* name)
{
    int val;

    assert(m_pVP8);

    HRESULT hr = (m_pVP8->*GetValue)(&val);

    if (FAILED(hr))
    {
        wstring text = L"Unable to get ";
        text += name;
        text += L" value from filter.";

        MessageBox(
            m_hWnd,
            text.c_str(),
            L"Error",
            MB_OK);

        return hr;
    }

    wostringstream os;

    if (val >= 0)
        os << val;

    const DWORD e = SetText(hWnd, code, os.str());

    if (e == 0)
        return S_OK;

    wstring text = L"Unable to set value for ";
    text += name;
    text += L" edit control.";

    MessageBox(
        hWnd,
        text.c_str(),
        L"Error",
        MB_OK);

    return E_FAIL;
}


HRESULT PropPage::GetDeadline(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetDeadline,
            IDC_DEADLINE,
            L"deadline");
}


HRESULT PropPage::SetDeadline()
{
    return SetIntValue(
            &IVP8Encoder::SetDeadline,
            IDC_DEADLINE,
            L"deadline");
}


HRESULT PropPage::GetThreadCount(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetThreadCount,
            IDC_THREADCOUNT,
            L"thread count");
}


HRESULT PropPage::SetThreadCount()
{
    return SetIntValue(
            &IVP8Encoder::SetThreadCount,
            IDC_THREADCOUNT,
            L"thread count");
}


HRESULT PropPage::SetErrorResilient()
{
    return SetIntValue(
            &IVP8Encoder::SetErrorResilient,
            IDC_ERROR_RESILIENT,
            L"error resilient");
}


HRESULT PropPage::GetErrorResilient(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetErrorResilient,
            IDC_ERROR_RESILIENT,
            L"error resilient");
}


HRESULT PropPage::SetDropframeThreshold()
{
    return SetIntValue(
            &IVP8Encoder::SetDropframeThreshold,
            IDC_DROPFRAME_THRESHOLD,
            L"dropframe threshold");
}


HRESULT PropPage::GetDropframeThreshold(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetDropframeThreshold,
            IDC_DROPFRAME_THRESHOLD,
            L"dropframe threshold");
}


HRESULT PropPage::SetResizeAllowed()
{
    return SetIntValue(
            &IVP8Encoder::SetResizeAllowed,
            IDC_RESIZE_ALLOWED,
            L"resize allowed");
}


HRESULT PropPage::GetResizeAllowed(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetResizeAllowed,
            IDC_RESIZE_ALLOWED,
            L"resize allowed");
}


HRESULT PropPage::SetResizeUpThreshold()
{
    return SetIntValue(
            &IVP8Encoder::SetResizeUpThreshold,
            IDC_RESIZE_UP_THRESHOLD,
            L"resize up threshold");
}


HRESULT PropPage::GetResizeUpThreshold(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetResizeUpThreshold,
            IDC_RESIZE_UP_THRESHOLD,
            L"resize up threshold");
}


HRESULT PropPage::SetResizeDownThreshold()
{
    return SetIntValue(
            &IVP8Encoder::SetResizeDownThreshold,
            IDC_RESIZE_DOWN_THRESHOLD,
            L"resize down threshold");
}


HRESULT PropPage::GetResizeDownThreshold(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetResizeDownThreshold,
            IDC_RESIZE_DOWN_THRESHOLD,
            L"resize down threshold");
}


HRESULT PropPage::GetEndUsage(HWND hWnd)
{
    VP8EndUsage val;

    assert(m_pVP8);
    HRESULT hr = m_pVP8->GetEndUsage(&val);

    if (FAILED(hr))
    {
        MessageBox(
            m_hWnd,
            L"Unable to get end usage value from filter.",
            L"Error",
            MB_OK);

        return hr;
    }

    int idx;

    switch (val)
    {
        case kEndUsageDefault:
        default:
            idx = 0;
            break;

        case kEndUsageVBR:
            idx = 1;
            break;

        case kEndUsageCBR:
            idx = 2;
            break;
    }

    const HWND hCtrl = GetDlgItem(hWnd, IDC_END_USAGE);
    assert(hCtrl);

    const int result = ComboBox_SetCurSel(hCtrl, idx);

    if (result >= 0)
        return S_OK;

    MessageBox(
        hWnd,
        L"Unable to set value for end usage combo box.",
        L"Error",
        MB_OK);

    return S_OK;
}


HRESULT PropPage::SetEndUsage()
{
    const HWND hCtrl = GetDlgItem(m_hWnd, IDC_END_USAGE);
    assert(hCtrl);

    const int idx = ComboBox_GetCurSel(hCtrl);
    assert(idx >= 0);
    assert(idx <= 2);

    VP8EndUsage val;

    switch (idx)
    {
        case 0:
        default:
            //val = kEndUsageDefault;
            //break;
            return S_OK;

        case 1:
            val = kEndUsageVBR;
            break;

        case 2:
            val = kEndUsageCBR;
            break;
    }

    assert(m_pVP8);

    HRESULT hr = m_pVP8->SetEndUsage(val);

    if (FAILED(hr))
    {
        MessageBox(
            m_hWnd,
            L"Unable to set end usage value on filter.",
            L"Error",
            MB_OK);

        return S_FALSE;
    }

    return S_OK;
}


HRESULT PropPage::GetLagInFrames(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetLagInFrames,
            IDC_LAG_IN_FRAMES,
            L"lag in frames");
}


HRESULT PropPage::SetLagInFrames()
{
    return SetIntValue(
            &IVP8Encoder::SetLagInFrames,
            IDC_LAG_IN_FRAMES,
            L"lag in frames");
}


HRESULT PropPage::GetTokenPartitions(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetTokenPartitions,
            IDC_TOKEN_PARTITIONS,
            L"token partitions");
}


HRESULT PropPage::SetTokenPartitions()
{
    return SetIntValue(
            &IVP8Encoder::SetTokenPartitions,
            IDC_TOKEN_PARTITIONS,
            L"token partitions");
}


HRESULT PropPage::GetTargetBitrate(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetTargetBitrate,
            IDC_TARGET_BITRATE,
            L"target bitrate");
}


HRESULT PropPage::SetTargetBitrate()
{
    return SetIntValue(
            &IVP8Encoder::SetTargetBitrate,
            IDC_TARGET_BITRATE,
            L"target bitrate");
}


HRESULT PropPage::GetMinQuantizer(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetMinQuantizer,
            IDC_MIN_QUANTIZER,
            L"min quantizer");
}


HRESULT PropPage::SetMinQuantizer()
{
    return SetIntValue(
            &IVP8Encoder::SetMinQuantizer,
            IDC_MIN_QUANTIZER,
            L"min quantizer");
}


HRESULT PropPage::GetMaxQuantizer(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetMaxQuantizer,
            IDC_MAX_QUANTIZER,
            L"min quantizer");
}


HRESULT PropPage::SetMaxQuantizer()
{
    return SetIntValue(
            &IVP8Encoder::SetMaxQuantizer,
            IDC_MAX_QUANTIZER,
            L"max quantizer");
}


HRESULT PropPage::GetUndershootPct(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetUndershootPct,
            IDC_UNDERSHOOT_PCT,
            L"undershoot pct");
}


HRESULT PropPage::SetUndershootPct()
{
    return SetIntValue(
            &IVP8Encoder::SetUndershootPct,
            IDC_UNDERSHOOT_PCT,
            L"undershoot pct");
}


HRESULT PropPage::GetOvershootPct(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetOvershootPct,
            IDC_OVERSHOOT_PCT,
            L"overshoot pct");
}


HRESULT PropPage::SetOvershootPct()
{
    return SetIntValue(
            &IVP8Encoder::SetOvershootPct,
            IDC_OVERSHOOT_PCT,
            L"overshoot pct");
}


HRESULT PropPage::GetDecoderBufferSize(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetDecoderBufferSize,
            IDC_DECODER_BUFFER_SIZE,
            L"decoder buffer size");
}


HRESULT PropPage::SetDecoderBufferSize()
{
    return SetIntValue(
            &IVP8Encoder::SetDecoderBufferSize,
            IDC_DECODER_BUFFER_SIZE,
            L"decoder buffer size");
}


HRESULT PropPage::GetDecoderBufferInitialSize(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetDecoderBufferInitialSize,
            IDC_DECODER_BUFFER_INITIAL_SIZE,
            L"decoder buffer initial size");
}


HRESULT PropPage::SetDecoderBufferInitialSize()
{
    return SetIntValue(
            &IVP8Encoder::SetDecoderBufferInitialSize,
            IDC_DECODER_BUFFER_INITIAL_SIZE,
            L"decoder buffer initial size");
}


HRESULT PropPage::GetDecoderBufferOptimalSize(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetDecoderBufferOptimalSize,
            IDC_DECODER_BUFFER_OPTIMAL_SIZE,
            L"decoder buffer optimal size");
}


HRESULT PropPage::SetDecoderBufferOptimalSize()
{
    return SetIntValue(
            &IVP8Encoder::SetDecoderBufferOptimalSize,
            IDC_DECODER_BUFFER_OPTIMAL_SIZE,
            L"decoder buffer optimal size");
}


HRESULT PropPage::GetKeyframeMode(HWND hWnd)
{
    VP8KeyframeMode val;

    assert(m_pVP8);
    HRESULT hr = m_pVP8->GetKeyframeMode(&val);

    if (FAILED(hr))
    {
        MessageBox(
            m_hWnd,
            L"Unable to get keyframe mode value from filter.",
            L"Error",
            MB_OK);

        return hr;
    }

    int idx;

    switch (val)
    {
        case kKeyframeModeDefault:
        default:
            idx = 0;
            break;

        case kKeyframeModeDisabled:
            idx = 1;
            break;

        case kKeyframeModeAuto:
            idx = 2;
            break;
    }

    const HWND hCtrl = GetDlgItem(hWnd, IDC_KEYFRAME_MODE);
    assert(hCtrl);

    const int result = ComboBox_SetCurSel(hCtrl, idx);

    if (result >= 0)
        return S_OK;

    MessageBox(
        hWnd,
        L"Unable to set value for keyframe mode combo box.",
        L"Error",
        MB_OK);

    return S_OK;
}


HRESULT PropPage::SetKeyframeMode()
{
    const HWND hCtrl = GetDlgItem(m_hWnd, IDC_KEYFRAME_MODE);
    assert(hCtrl);

    const int idx = ComboBox_GetCurSel(hCtrl);
    assert(idx >= 0);
    assert(idx <= 2);

    VP8KeyframeMode val;

    switch (idx)
    {
        case 0:
        default:
            //val = kKeyframeModeDefault;
            //break;
            return S_OK;

        case 1:
            val = kKeyframeModeDisabled;
            break;

        case 2:
            val = kKeyframeModeAuto;
            break;
    }

    assert(m_pVP8);
    HRESULT hr = m_pVP8->SetKeyframeMode(val);

    if (FAILED(hr))
    {
        MessageBox(
            m_hWnd,
            L"Unable to set keyframe mode on filter.",
            L"Error",
            MB_OK);

        return S_FALSE;
    }

    return S_OK;
}


HRESULT PropPage::GetKeyframeMinInterval(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetKeyframeMinInterval,
            IDC_KEYFRAME_MIN_INTERVAL,
            L"keyframe min interval");
}


HRESULT PropPage::SetKeyframeMinInterval()
{
    return SetIntValue(
            &IVP8Encoder::SetKeyframeMinInterval,
            IDC_KEYFRAME_MIN_INTERVAL,
            L"keyframe min interval");
}


HRESULT PropPage::GetKeyframeMaxInterval(HWND hWnd)
{
    return GetIntValue(
            hWnd,
            &IVP8Encoder::GetKeyframeMaxInterval,
            IDC_KEYFRAME_MAX_INTERVAL,
            L"keyframe max interval");
}


HRESULT PropPage::SetKeyframeMaxInterval()
{
    return SetIntValue(
            &IVP8Encoder::SetKeyframeMaxInterval,
            IDC_KEYFRAME_MAX_INTERVAL,
            L"keyframe max interval");
}


}  //end namespace VP8EncoderLib
