#ifndef CORELIB___NCBI_LIMITS__H
#define CORELIB___NCBI_LIMITS__H

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Denis Vakatov
 *
 *
 */

/**
 * @file ncbitype.h
 *
 * Defines Limits for the types used in NCBI C/C++ toolkit.
 *
 *   Limits for the NCBI C/C++ fixed-size types:
 *      Char, Uchar
 *      Int1, Uint1  --  kMin_I1,       kMax_I1,       kMax_UI1
 *      Int2, Uint2  --  kMin_I2,       kMax_I2,       kMax_UI2
 *      Int4, Uint4  --  kMin_I4,       kMax_I4,       kMax_UI4
 *      Int8, Uint8  --  kMin_I8,       kMax_I8,       kMax_UI8
 *
 *   Limits for the built-in integer types:
 *      "char"       --  kMin_Char,     kMax_Char,     kMax_UChar
 *      "short"      --  kMin_Short,    kMax_Short,    kMax_UShort
 *      "int"        --  kMin_Int,      kMax_Int,      kMax_UInt
 *      "long"       --  kMin_Long,     kMax_Long,     kMax_ULong
 *      "long long"  --  kMin_LongLong, kMax_LongLong, kMax_ULongLong
 *      "__int64"    --  kMin_Int64,    kMax_Int64,    kMax_UInt64
 *
 *   Limits for the built-in floating-point types:
 *      "float"      --  kMin_Float,    kMax_Float
 *      "double"     --  kMin_Double,   kMax_Double
 *
 */

#include <corelib/ncbitype.h>
#include <limits.h>
#include <float.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif


/** @addtogroup Portability
 *
 * @{
 */


/* Int8, Uint8
 *   NOTE:  "NCBI_MIN/MAX_***8" are temporary preprocessor definitions, so
 *          do not use them... always use "kMax_*" and "kMin_*" instead!
 */
#if   (SIZEOF_LONG == 8)
#  define NCBI_MIN_I8  LONG_MIN
#  define NCBI_MAX_I8  LONG_MAX
#  define NCBI_MAX_UI8 ULONG_MAX
#elif (SIZEOF_LONG_LONG == 8)
#  define NCBI_MIN_I8  0x8000000000000000LL
#  define NCBI_MAX_I8  0x7FFFFFFFFFFFFFFFLL
#  define NCBI_MAX_UI8 0xFFFFFFFFFFFFFFFFULL
#elif defined(NCBI_INT8_IS_INT64)
#  define NCBI_MIN_I8  0x8000000000000000i64
#  define NCBI_MAX_I8  0x7FFFFFFFFFFFFFFFi64
#  define NCBI_MAX_UI8 0xFFFFFFFFFFFFFFFFui64
#endif


/*  Limits:  C++ and C interfaces
 */

#ifdef __cplusplus
/* (BEGIN C++ interface) */

/* [C++]  built-in integer types */
const          char   kMin_Char   = CHAR_MIN;
const          char   kMax_Char   = CHAR_MAX;
const signed   char   kMin_SChar  = SCHAR_MIN;
const signed   char   kMax_SChar  = SCHAR_MAX;
const unsigned char   kMax_UChar  = UCHAR_MAX;

#if defined(HAVE_WCHAR_H)  &&  defined(WCHAR_MIN)
const wchar_t kMin_WChar = WCHAR_MIN;
const wchar_t kMax_WChar = WCHAR_MAX;
#endif

const signed   short  kMin_Short  = SHRT_MIN;
const signed   short  kMax_Short  = SHRT_MAX;
const unsigned short  kMax_UShort = USHRT_MAX;

const signed   int    kMin_Int    = INT_MIN;
const signed   int    kMax_Int    = INT_MAX;
const unsigned int    kMax_UInt   = UINT_MAX;

const signed   long   kMin_Long   = LONG_MIN;
const signed   long   kMax_Long   = LONG_MAX;
const unsigned long   kMax_ULong  = ULONG_MAX;

#  if (SIZEOF_LONG_LONG == 8)
const signed   long long  kMin_LongLong   = 0x8000000000000000LL;
const signed   long long  kMax_LongLong   = 0x7FFFFFFFFFFFFFFFLL;
const unsigned long long  kMax_ULongLong  = 0xFFFFFFFFFFFFFFFFULL;
#  elif (SIZEOF_LONG_LONG == 4)
const signed   long long  kMin_LongLong   = 0x80000000LL;
const signed   long long  kMax_LongLong   = 0x7FFFFFFFLL;
const unsigned long long  kMax_ULongLong  = 0xFFFFFFFFULL;
#  endif

#  if defined(NCBI_INT8_IS_INT64)
const signed   __int64 kMin_Int64  = NCBI_MIN_I8;
const signed   __int64 kMax_Int64  = NCBI_MAX_I8;
const unsigned __int64 kMax_UInt64 = NCBI_MAX_UI8;
#  endif

/* [C++]  built-in floating-point types */
const float kMin_Float = FLT_MIN;
const float kMax_Float = FLT_MAX;

const double kMin_Double = DBL_MIN;
const double kMax_Double = DBL_MAX;

/* [C++]  NCBI fixed-size types */
const Int1  kMin_I1  = SCHAR_MIN;
const Int1  kMax_I1  = SCHAR_MAX;
const Uint1 kMax_UI1 = UCHAR_MAX;

const Int2  kMin_I2  = SHRT_MIN;
const Int2  kMax_I2  = SHRT_MAX;
const Uint2 kMax_UI2 = USHRT_MAX;

const Int4  kMin_I4  = INT_MIN;
const Int4  kMax_I4  = INT_MAX;
const Uint4 kMax_UI4 = UINT_MAX;

const Int8  kMin_I8  = NCBI_MIN_I8;
const Int8  kMax_I8  = NCBI_MAX_I8;
const Uint8 kMax_UI8 = NCBI_MAX_UI8;
#  undef NCBI_MIN_I8
#  undef NCBI_MAX_I8
#  undef NCBI_MAX_UI8


/* (END of C++ interface) */
#else
/* (BEGIN C interface) */


/* [ C ]  built-in integer types */
#  define kMin_Char   CHAR_MIN
#  define kMax_Char   CHAR_MAX
#  define kMin_SChar  SCHAR_MIN
#  define kMax_SChar  SCHAR_MAX
#  define kMax_UChar  UCHAR_MAX

#  define kMin_Short  SHRT_MIN
#  define kMax_Short  SHRT_MAX
#  define kMax_UShort USHRT_MAX

#  define kMin_Int    INT_MIN
#  define kMax_Int    INT_MAX
#  define kMax_UInt   UINT_MAX

#  if (SIZEOF_LONG_LONG == 8)
#    define kMin_LongLong   0x8000000000000000LL
#    define kMax_LongLong   0x7FFFFFFFFFFFFFFFLL
#    define kMax_ULongLong  0xFFFFFFFFFFFFFFFFULL
#  elif (SIZEOF_LONG_LONG == 4)
#    define kMin_LongLong   0x80000000LL
#    define kMax_LongLong   0x7FFFFFFFLL
#    define kMax_ULongLong  0xFFFFFFFFULL
#  endif

#  if (SIZEOF___INT64 == 8)
#    define kMin_Int64  NCBI_CONST_INT8(0x8000000000000000)
#    define kMax_Int64  NCBI_CONST_INT8(0x7FFFFFFFFFFFFFFF)
#    define kMax_UInt64 NCBI_CONST_UINT8(0xFFFFFFFFFFFFFFFF)
#  endif

/* [ C ]  built-in floating-point types */
#  define kMin_Float  FLT_MIN;
#  define kMax_Float  FLT_MAX;

#  define kMin_Double DBL_MIN;
#  define kMax_Double DBL_MAX;

/* [ C ]  NCBI fixed-size types */
#  define kMin_I1   SCHAR_MIN
#  define kMax_I1   SCHAR_MAX
#  define kMax_UI1  UCHAR_MAX
#  define kMin_I2   SHRT_MIN
#  define kMax_I2   SHRT_MAX
#  define kMax_UI2  USHRT_MAX
#  define kMin_I4   INT_MIN
#  define kMax_I4   INT_MAX
#  define kMax_UI4  UINT_MAX
#  define kMin_I8   NCBI_MIN_I8
#  define kMax_I8   NCBI_MAX_I8
#  define kMax_UI8  NCBI_MAX_UI8


/* (END of C interface) */
#endif  /* __cplusplus */


#endif /* CORELIB___NCBI_LIMITS__H */


/* @} */
