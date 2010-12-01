// mfplay.cpp : Defines the entry point for the console application.
//

#define WINVER _WIN32_WINNT_WIN7

#include <new>
#include <windows.h>
#include <windowsx.h>
#include <mfplay.h>
#include <mferror.h>
#include <shobjidl.h>   // defines IFileOpenDialog
#include <tchar.h>
#include <strsafe.h>

// strsafe deprecates several functions used in gtest; disable the warnings
#pragma warning(push)
#pragma warning(disable:4995)
#include "gtest/gtest.h"
#pragma warning(pop)

int _tmain(int argc, _TCHAR* argv[])
{
    (void)argc, (void)argv;
    return 0;
}

