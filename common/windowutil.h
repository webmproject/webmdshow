#ifndef __WEBMDSHOW_COMMON_WINDOWUTIL_HPP__
#define __WEBMDSHOW_COMMON_WINDOWUTIL_HPP__

namespace WebmMfUtil
{

class WebmMfWindow
{
public:
    explicit WebmMfWindow(WNDPROC ptrfn_window_proc);
    ~WebmMfWindow();

    HRESULT Create(HINSTANCE hinstance);
    HRESULT Destroy();
    HWND GetHwnd() const;
    HRESULT Show();
    HRESULT Hide();

    HRESULT SetUserData(LONG_PTR ptr_userdata);
private:

    HINSTANCE instance_;
    HWND hwnd_;
    WNDCLASS window_class_;
    const WNDPROC ptrfn_window_proc_;

    DISALLOW_COPY_AND_ASSIGN(WebmMfWindow);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_WINDOWUTIL_HPP__
