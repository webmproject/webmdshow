#ifndef __WEBMDSHOW_COMMON_DEBUGUTIL_HPP__
#define __WEBMDSHOW_COMMON_DEBUGUTIL_HPP__

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
#endif

#ifdef _DEBUG
#include "odbgstream.h"
#include "hrtext.h"
#include "iidstr.h"

// Simple trace logging macro that expands to nothing in release mode builds.
// Output is sent to the vs console.
#define DBGLOG(X) \
do { \
    wodbgstream wos; \
    wos << "["__FUNCTION__"] " << X << std::endl; \
} while(0)

// Extract error from the HRESULT, and output its hex and decimal values.
#define \
    HRLOG(X) L" {" << #X << L"=" << X << L"/" << std::hex << X << std::dec \
    << L" (" << hrtext(X) << L")}"

// Convert 100ns units to seconds
#define REFTIMETOSECONDS(X) (double(X) / 10000000.0f)

#else
#define DBGLOG(X)
#define REFTIMETOSECONDS(X)
#endif

// Keep the compiler quiet about do/while(0)'s (constant conditional) used in
// log macros.
#pragma warning(disable:4127)

// Check the HRESULT for failure (<0), and log it if we're in debug mode, and
// format the failure text so that it is clickable in vs output window.
#define CHK(X, Y) \
do { \
    if (FAILED(X=(Y))) \
    { \
        DBGLOG("\n" << __FILE__ << "(" << __LINE__ << ") : " << #Y << HRLOG(X)); \
    } \
} while (0)

#endif // __WEBMDSHOW_COMMON_DEBUGUTIL_HPP__
