#include <windows.h>
#include <windowsx.h>
#include <mfplay.h>
#include <mferror.h>

#include <cassert>
#include <string>

#include "debugutil.hpp"
#include "windowutil.hpp"

namespace WebmMfUtil
{

const wchar_t* const kWindowClass = L"WebM MF Window Class";
const wchar_t* const kWindowName = L"WebM MF Window";

WebmMfWindow::WebmMfWindow(WNDPROC ptrfn_window_proc) :
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

HRESULT WebmMfWindow::Create()
{
    assert(ptrfn_window_proc_);

    if (!ptrfn_window_proc_ || !RegisterClass(&window_class_))
    {
        return E_INVALIDARG;
    }

    hwnd_ = CreateWindow(window_class_.lpszClassName, kWindowName,
                         WS_POPUPWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                         CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL,
                         GetModuleHandle(NULL), NULL);

    assert(hwnd_);
    if (NULL == hwnd_)
    {
        return E_FAIL;
    }

    const LONG_PTR ourdata = reinterpret_cast<LONG_PTR>(this);

    if (!SetWindowLongPtr(hwnd_, GWL_USERDATA, ourdata))
    {
        return E_FAIL;
    }

    return Show();
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

HRESULT WebmMfWindow::Show()
{
    HRESULT hr = E_FAIL;
    assert(hwnd_);
    if (NULL == hwnd_)
    {
        return hr;
    }

    if (ShowWindow(hwnd_, SW_SHOW))
    {
        hr = S_OK;
    }

    return hr;
}

HRESULT WebmMfWindow::Hide()
{
    HRESULT hr = E_FAIL;
    assert(hwnd_);
    if (NULL == hwnd_)
    {
        return hr;
    }

    if (ShowWindow(hwnd_, SW_HIDE))
    {
        hr = S_OK;
    }

    return hr;
}

HRESULT WebmMfWindow::SetUserData(LONG_PTR ptr_userdata)
{
    assert(ptr_userdata);
    if (NULL == ptr_userdata || NULL == hwnd_)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = E_FAIL;

    if (SetWindowLongPtr(hwnd_, GWL_USERDATA, ptr_userdata))
    {
        hr = S_OK;
    }

    return hr;
}

#if 0
static LRESULT CALLBACK WebmMfWindow::WindowProc(HWND hwnd, UINT msg,
                                                 WPARAM wparam, LPARAM lparam)
{
    LONG_PTR ourdata = GetWindowLongPtr(hwnd, GWL_USERDATA);
    assert(ourdata);
    WebmMfWindow* ptr_mfwindow = reinterpret_cast<WebmMfWindow*>(ourdata);

    switch(msg)
    {
        HANDLE_MSG(hwnd, WM_CLOSE, ptr_mfwindow->OnClose_);
        HANDLE_MSG(hwnd, WM_PAINT, ptr_mfwindow->OnPaint_);
        HANDLE_MSG(hwnd, WM_SIZE, ptr_mfwindow->OnSize_);

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    };
}
#endif

} // WebmMfUtil namespace
