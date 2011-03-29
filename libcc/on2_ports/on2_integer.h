#ifndef ON2_INTEGER_H
#define ON2_INTEGER_H

/* get ptrdiff_t, size_t, wchar_t, NULL */ 
#include <stddef.h>

#if defined(HAVE_STDINT_H) && HAVE_STDINT_H
#include <stdint.h>
#else
    typedef signed char  int8_t;
    typedef signed short int16_t;
    typedef signed int   int32_t;

    typedef unsigned char  uint8_t;
    typedef unsigned short uint16_t;
    typedef unsigned int   uint32_t;

#if defined(_MSC_VER)
    typedef signed __int64   int64_t;
    typedef unsigned __int64 uint64_t;
#endif
	
#ifdef HAVE_ARMV6
    typedef unsigned int int_fast16_t;
#else
    typedef signed short int_fast16_t;
#endif
    typedef signed char int_fast8_t;
    typedef unsigned char uint_fast8_t;

#ifndef _UINTPTR_T_DEFINED
    typedef unsigned int   uintptr_t;
#endif

#endif

#endif
