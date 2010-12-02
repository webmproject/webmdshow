#ifndef __WEBMDSHOW_COMMON_WINDOWUTIL_HPP__
#define __WEBMDSHOW_COMMON_WINDOWUTIL_HPP__

namespace WebmMfUtil
{

class WebmMfWindow
{
public:
    explicit WebmMfWindow(WNDPROC ptrfn_window_proc);
    ~WebmMfWindow();

    HRESULT Create();
    HRESULT Destroy();
    HRESULT Show();
    HRESULT Hide();

    HRESULT SetUserData(LONG_PTR ptr_userdata);
private:

    HRESULT OnClose_();
    HRESULT OnPaint_();
    HRESULT OnCommand_(int id, HWND hwndCtl, UINT codeNotify);
    HRESULT OnSize_(UINT state, int cx, int cy);
    HRESULT OnKeyDown_(UINT vk, BOOL fDown, int cRepeat, UINT flags);

    HWND hwnd_;
    WNDCLASS window_class_;
    const WNDPROC ptrfn_window_proc_;

    DISALLOW_COPY_AND_ASSIGN(WebmMfWindow);
};

} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_WINDOWUTIL_HPP__
