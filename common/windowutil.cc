#include <windows.h>
#include <windowsx.h>
#include <mfplay.h>
#include <mferror.h>

#include <cassert>
#include <string>

#include "debugutil.h"
#include "windowutil.h"

namespace WebmMfUtil
{

const wchar_t* const kWindowClass = L"WebM MF Window Class";
const wchar_t* const kWindowName = L"WebM MF Window";

WebmMfWindow::WebmMfWindow(WNDPROC ptrfn_window_proc) :
  instance_(NULL),
  hwnd_(NULL),
  ptrfn_window_proc_(ptrfn_window_proc)
{
    ::memset(&window_class_, 0, sizeof WNDCLASS);
    window_class_.lpfnWndProc   = ptrfn_window_proc;
    window_class_.hInstance     = GetModuleHandle(NULL);
    window_class_.hCursor       = LoadCursor(NULL, IDC_ARROW);
    window_class_.lpszClassName = kWindowClass;
}

WebmMfWindow::~WebmMfWindow()
{
    Destroy();
}

HRESULT WebmMfWindow::Create(HINSTANCE instance)
{
    assert(ptrfn_window_proc_);

    if (!ptrfn_window_proc_ || !RegisterClass(&window_class_))
    {
        return E_INVALIDARG;
    }
    instance_ = instance;
    hwnd_ = CreateWindow(window_class_.lpszClassName, kWindowName,
                         WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                         320, 240, NULL, NULL, instance_, NULL);
    assert(hwnd_);
    if (NULL == hwnd_)
    {
        return E_FAIL;
    }

    UpdateWindow(hwnd_);
    Show();
    return S_OK;
}

HRESULT WebmMfWindow::Destroy()
{
    HRESULT hr = S_FALSE;

    if (NULL != hwnd_)
    {
        DestroyWindow(hwnd_);
        hwnd_ = NULL;
        hr = S_OK;
    }

    return hr;
}

HWND WebmMfWindow::GetHwnd() const
{
    return hwnd_;
}

HRESULT WebmMfWindow::Show()
{
    assert(hwnd_);
    if (NULL == hwnd_)
    {
        return E_FAIL;
    }

    ShowWindow(hwnd_, SW_SHOW);

    return S_OK;
}

HRESULT WebmMfWindow::Hide()
{
    assert(hwnd_);
    if (NULL == hwnd_)
    {
        return E_FAIL;
    }

    ShowWindow(hwnd_, SW_HIDE);

    return S_OK;
}

HRESULT WebmMfWindow::SetUserData(LONG_PTR ptr_userdata)
{
    assert(ptr_userdata);
    if (NULL == ptr_userdata || NULL == hwnd_)
    {
        return E_INVALIDARG;
    }

    SetWindowLongPtr(hwnd_, GWLP_USERDATA, ptr_userdata);

    return S_OK;
}

} // WebmMfUtil namespace
