#include <new>
#include <windows.h>
#include <windowsx.h>
#include <mfplay.h>
#include <mferror.h>
#include <tchar.h>
#include <strsafe.h>

// strsafe deprecates several functions used in gtest; disable the warnings
#pragma warning(push)
#pragma warning(disable:4995)
#include "gtest/gtest.h"
#pragma warning(pop)

#include "debugutil.hpp"
#include "mfplayer.hpp"

void ShowErrorMessage(PCWSTR format, HRESULT hrErr)
{
    HRESULT hr = S_OK;
    WCHAR msg[MAX_PATH];

    hr = StringCbPrintf(msg, sizeof(msg), L"%s (hr=0x%X)", format, hrErr);

    if (SUCCEEDED(hr))
    {
        MessageBox(NULL, msg, L"Error", MB_ICONERROR);
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (argc < 2)
    {
        ShowErrorMessage(L"No file specified", E_FAIL);
        return EXIT_FAILURE;
    }

    const DWORD co_init_flags =
        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE;

    HRESULT hr = CoInitializeEx(NULL, co_init_flags);
    if (FAILED(hr))
    {
        ShowErrorMessage(L"COM init failed", E_FAIL);
        return EXIT_FAILURE;
    }

    std::wstring url_str = argv[1];

    WebMMfTests::MfPlayer player;

    hr = player.Open(url_str);
    assert(SUCCEEDED(hr));

    if (SUCCEEDED(hr))
    {
        hr = player.Play();
        assert(SUCCEEDED(hr));
    }

    CoUninitialize();

    return EXIT_SUCCESS;
}

