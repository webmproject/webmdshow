#ifndef __WEBMDSHOW_COMMON_DEBUGUTIL_HPP__
#define __WEBMDSHOW_COMMON_DEBUGUTIL_HPP__

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
#endif

#ifdef _DEBUG
#include "odbgstream.hpp"
#include "iidstr.hpp"

// keep the compiler quiet about do/while(0)'s used in log macros
#pragma warning(disable:4127)

#define DBGLOG(X) \
do { \
    wodbgstream wos; \
    wos << "["__FUNCTION__"] " << X << std::endl; \
} while(0)

#define REFTIMETOSECONDS(X) (double(X) / 10000000.0f)

#else
#define DBGLOG(X)
#define REFTIMETOSECONDS(X)
#endif

#endif // __WEBMDSHOW_COMMON_DEBUGUTIL_HPP__
