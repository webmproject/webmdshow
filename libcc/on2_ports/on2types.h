/*
//==========================================================================
//
//  Copyright (c) On2 Technologies Inc. All Rights Reserved.
//
//--------------------------------------------------------------------------
//
//  File:        $Workfile: on2types.h$
//               $Revision: 12$
//
//  Last Update: $DateUTC: 2007-05-25 19:34:06Z$
//
//--------------------------------------------------------------------------
*/
#ifndef __ON2TYPES_H__
#define __ON2TYPES_H__

#ifdef   HAVE_CONFIG_H
#include HAVE_CONFIG_H
#endif

//#include <sys/types.h>
#ifdef _MSC_VER
# include <basetsd.h>
 typedef SSIZE_T ssize_t;
#endif

#if defined(HAVE_STDINT_H) && HAVE_STDINT_H
  /* C99 types are preferred to on2 integer types */
# include <stdint.h>
#endif

/*!\defgroup basetypes Base Types
  @{*/
#if !defined(HAVE_STDINT_H) && !defined(INT_T_DEFINED)
# ifdef STRICTTYPES
   typedef signed char  int8_t;
   typedef signed short int16_t;
   typedef signed int   int32_t;
# else
   typedef char         int8_t;
   typedef short        int16_t;
   typedef int          int32_t;
# endif
 typedef unsigned char  uint8_t;
 typedef unsigned short uint16_t;
 typedef unsigned int   uint32_t;
#endif

typedef int8_t     on2s8;
typedef uint8_t    on2u8;
typedef int16_t    on2s16;
typedef uint16_t   on2u16;
typedef int32_t    on2s32;
typedef uint32_t   on2u32;
typedef int32_t    on2bool;

enum {on2false, on2true};

/*!\def OTC
   \brief a macro suitable for declaring a constant #on2tc*/
/*!\def ON2TC
   \brief printf format string suitable for printing an #on2tc*/
#ifdef UNICODE
# ifdef NO_WCHAR
#  error "no non-wchar support added yet"
# else
#  include <wchar.h>
   typedef wchar_t on2tc;
#  define OTC(str) L ## str
#  define ON2TC "ls"
# endif /*NO_WCHAR*/
#else
 typedef char on2tc;
# define OTC(str) (on2tc*)str
# define ON2TC "s"
#endif /*UNICODE*/
/*@} end - base types*/

/*!\addtogroup basetypes
  @{*/
/*!\def ON264
   \brief printf format string suitable for printing an #on2s64*/
#if defined(HAVE_STDINT_H)
# define ON264 PRId64
 typedef int64_t on2s64;
#elif defined(HASLONGLONG)
# undef  PRId64
# define PRId64 "lld"
# define ON264 PRId64
 typedef long long on2s64;
#elif defined(WIN32) || defined(_WIN32_WCE)
# undef  PRId64
# define PRId64 "I64d"
# define ON264 PRId64
 typedef __int64 on2s64;
 typedef unsigned __int64 on2u64;
#elif defined(__uClinux__) && defined(CHIP_DM642)
# include <lddk.h>
# undef  PRId64
# define PRId64 "lld"
# define ON264 PRId64
 typedef long on2s64;
#elif defined(__SYMBIAN32__)
# undef  PRId64
# define PRId64 "u"
# define ON264 PRId64
 typedef unsigned int on2s64;
#else
# error "64 bit integer type undefined for this platform!"
#endif
#if !defined(HAVE_STDINT_H) && !defined(INT_T_DEFINED)
 typedef on2s64 int64_t;
 typedef on2u64 uint64_t;
#endif
/*!@} end - base types*/

/*!\ingroup basetypes
   \brief Common return type*/
typedef enum {
    ON2_NOT_FOUND        = -404,
    ON2_BUFFER_EMPTY     = -202,
    ON2_BUFFER_FULL      = -201,

    ON2_CONNREFUSED      = -102,
    ON2_TIMEDOUT         = -101,
    ON2_WOULDBLOCK       = -100,

    ON2_NET_ERROR        = -9,
    ON2_INVALID_VERSION  = -8,
    ON2_INPROGRESS       = -7,
    ON2_NOT_SUPP         = -6,
    ON2_NO_MEM           = -3,
    ON2_INVALID_PARAMS   = -2,
    ON2_ERROR            = -1,
    ON2_OK               = 0,
    ON2_DONE             = 1
} on2sc;

#if defined(WIN32) || defined(_WIN32_WCE)
# define DLLIMPORT __declspec(dllimport)
# define DLLEXPORT __declspec(dllexport)
# define DLLLOCAL
#elif defined(LINUX)
# define DLLIMPORT
  /*visibility attribute support is available in 3.4 and later.
    see: http://gcc.gnu.org/wiki/Visibility for more info*/
# if defined(__GNUC__) && ((__GNUC__<<16|(__GNUC_MINOR__&0xff)) >= (3<<16|4))
#  define GCC_HASCLASSVISIBILITY
# endif /*defined(__GNUC__) && __GNUC_PREREQ(3,4)*/
# ifdef GCC_HASCLASSVISIBILITY
#  define DLLEXPORT   __attribute__ ((visibility("default")))
#  define DLLLOCAL __attribute__ ((visibility("hidden")))
# else
#  define DLLEXPORT
#  define DLLLOCAL
# endif /*GCC_HASCLASSVISIBILITY*/
#endif /*platform ifdefs*/

#endif /*__ON2TYPES_H__*/

#undef ON2API
/*!\def ON2API
   \brief library calling convention/storage class attributes.

   Specifies whether the function is imported through a dll
   or is from a static library.*/
#ifdef ON2DLL
# ifdef ON2DLLEXPORT
#  define ON2API DLLEXPORT
# else
#  define ON2API DLLIMPORT
# endif /*ON2DLLEXPORT*/
#else
# define ON2API
#endif /*ON2DLL*/
