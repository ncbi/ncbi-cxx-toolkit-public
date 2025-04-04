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
 * File Description:
 *   TEST for:  NCBI C++ core string-related API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/version.hpp>
#include <corelib/ncbi_xstr.hpp>
#include <corelib/ncbifloat.h>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbiexec.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_test.hpp>
#include <algorithm>
#include <locale.h>
#include <math.h>

#define BOOST_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */


// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;

static const int kBad = 555;


//----------------------------------------------------------------------------
// NStr::StringTo*()
//----------------------------------------------------------------------------

struct SStringNumericValues
{
    const char* str;
    int         flags;
    Int8        num;
    Int8        i;
    Uint8       u;
    Int8        i8;
    Uint8       u8;
    float       f;
    double      d;
    double      delta;

    bool IsGoodInt(void) const {
        return i != kBad;
    }
    bool IsGoodUInt(void) const {
        return u != (Uint8)kBad;
    }
    bool IsGoodInt8(void) const {
        return i8 != kBad;
    }
    bool IsGoodUInt8(void) const {
        return u8 != (Uint8)kBad;
    }
    bool IsGoodFloat(void) const {
        return f != (float)kBad;
    }
    bool IsGoodDouble(void) const {
        return d != (double)kBad;
    }
    bool Same(const string& s) const {
        if ( str[0] == '+' && isdigit((unsigned char) str[1]) ) {
            if ( s[0] == '+' ) {
                string tmp(s, 1, NPOS);
                return tmp == str + 1;
            } 
            return s == str + 1;
        }
        return s == NStr::TruncateSpaces_Unsafe(str, NStr::eTrunc_Both);
    }
};


// Macro to silence GCC's __wur (warn unused result)
#define _no_warning(expr)  while ( expr ) break

// Test errno value, to check that 'errno' changes inside conversion methods.
static const int kTestErrno = 555;
#define CHECK_ERRNO  BOOST_CHECK(errno != kTestErrno)

// Default flags
#define DF 0

// define float constant to use in STR() macros.
// (we need extra 0 before 'f' for stricter compilers, plain constants don't need it)
#define NCBI_CONST_FLOAT(v) v##.0f

//                str  flags  num   int   uint  Int8  Uint8  float  double
#define BAD(v)   {  v, DF, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. }
#define STR(v)   { #v, DF, NCBI_CONST_INT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), NCBI_CONST_FLOAT(v), v##., 0. }
#define STRI(v)  { #v, DF, -1, NCBI_CONST_INT8(v), kBad, NCBI_CONST_INT8(v), kBad, NCBI_CONST_FLOAT(v), v##., 0.}
#define STRU(v)  { #v, DF, -1, kBad, NCBI_CONST_UINT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), NCBI_CONST_FLOAT(v), v##., 0.}
#define STR8(v)  { #v, DF, -1, kBad, kBad, NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), NCBI_CONST_FLOAT(v), v##., 0.}
#define STRI8(v) { #v, DF, -1, kBad, kBad, NCBI_CONST_INT8(v), kBad, NCBI_CONST_FLOAT(v), v##., 0.}
#define STRU8(v) { #v, DF, -1, kBad, kBad, kBad, NCBI_CONST_UINT8(v), NCBI_CONST_FLOAT(v), v##., 0. }
#define STRF(v)  { #v, DF, -1, kBad, kBad, kBad, kBad, NCBI_CONST_FLOAT(v), v##. }
#define STRD(v)  { #v, DF, -1, kBad, kBad, kBad, kBad, kBad, v##. }


static const SStringNumericValues s_Str2NumTests[] = {
    STR(0),
    STR(1),
    STRI(-1),
#if (SIZEOF_INT > 4)
    STRI(-2147483649),
#else
    STRI8(-2147483649),
#endif
    STRI(-2147483648),
    STRI(-2147483647),
    STR(2147483646),
    STR(2147483647),
#if (SIZEOF_INT > 4)
    STR(2147483648),
#else
    STRU(2147483648),
#endif
    STRU(4294967294),
    STRU(4294967295),
#if (SIZEOF_INT > 4)
    STR(4294967296),
#else
    STR8(4294967296),
#endif
    STR(10),
    STR(100),
    STR(1000),
    STR(10000),
    STR(100000),
    STR(1000000),
    STR(10000000),
    STR(100000000),
    STR(1000000000),
#if (SIZEOF_INT > 4)
    STR(10000000000),
    STR(100000000000),
    STR(1000000000000),
    STR(10000000000000),
    STR(100000000000000),
    STR(1000000000000000),
    STR(10000000000000000),
    STR(100000000000000000),
    STR(1000000000000000000),
#else
    STR8(10000000000),
    STR8(100000000000),
    STR8(1000000000000),
    STR8(10000000000000),
    STR8(100000000000000),
    STR8(1000000000000000),
    STR8(10000000000000000),
    STR8(100000000000000000),
    STR8(1000000000000000000),
#endif
    STRF(-9223372036854775809),
    { "-9223372036854775808", DF, -1, kBad, kBad, NCBI_CONST_INT8(-9223372036854775807)-1, kBad, -9223372036854775808.f, -9223372036854775808., 0. },
    STRI8(-9223372036854775807),
    STR8(9223372036854775806),
    STR8(9223372036854775807),
    STRU8(9223372036854775808),
    STRU8(18446744073709551614),
    STRU8(18446744073709551615),
    STRF(18446744073709551616),

    BAD(""),
    BAD("+"),
    BAD("-"),
    BAD("."),
    BAD(".."),
    BAD("abc"),
    { ".0",  DF, -1, kBad, kBad, kBad, kBad,  .0f, .0, 0.  },
    BAD(".0."),
    BAD("..0"),
    { ".01", DF, -1, kBad, kBad, kBad, kBad,  .01f, .01, 0. },
    { "1.",  DF, -1, kBad, kBad, kBad, kBad,  1.f , 1. , 0. },
    { "1.1", DF, -1, kBad, kBad, kBad, kBad,  1.1f, 1.1, 0. },
    BAD("1.1."),
    BAD("1.."),
    STRI(-123),
    BAD("--123"),
    { "+123", DF, 123, 123, 123, 123, 123, 123, 123, 0. },
    BAD("++123"),
    BAD("- 123"),
    BAD(" 123"),

    { " 123",     NStr::fAllowLeadingSpaces,  -1, 123,  123,  123,  123,  123.f, 123., 0.},
    { " 123",     NStr::fAllowLeadingSymbols, -1, 123,  123,  123,  123,  123.f, 123., 0. },
    BAD("123 "),
    { "123 ",     NStr::fAllowTrailingSpaces,  -1, 123,  123,  123,  123,  123.f, 123., 0. },
    { "123 ",     NStr::fAllowTrailingSymbols, -1, 123,  123,  123,  123,  123.f, 123., 0. },
    { "123(45) ", NStr::fAllowTrailingSymbols, -1, 123,  123,  123,  123,  123.f, 123., 0. },
    { " 123 ",    NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces, -1, 123,  123,  123,  123,  123.f, 123., 0. },
    
    { "1,234",    NStr::fAllowCommas, -1,    1234,    1234,    1234,    1234, kBad, kBad, 0. },
    { "1,234,567",NStr::fAllowCommas, -1, 1234567, 1234567, 1234567, 1234567, kBad, kBad, 0. },
    { "12,34",    NStr::fAllowCommas, -1,    kBad,    kBad,    kBad,    kBad, kBad, kBad, 0. },
    { ",123",     NStr::fAllowCommas, -1,    kBad,    kBad,    kBad,    kBad, kBad, kBad, 0. },
    { ",123",     NStr::fAllowCommas | NStr::fAllowLeadingSymbols, -1, 123, 123, 123, 123, 123, 123, 0. },

#if (SIZEOF_INT > 4)
    {  "4,294,967,294", NStr::fAllowCommas, -1, NCBI_CONST_UINT8(4294967294), NCBI_CONST_UINT8(4294967294), NCBI_CONST_UINT8(4294967294), NCBI_CONST_UINT8(4294967294), kBad, kBad, 0. },
    {  "4,294,967,295", NStr::fAllowCommas, -1, NCBI_CONST_UINT8(4294967295), NCBI_CONST_UINT8(4294967295), NCBI_CONST_UINT8(4294967295), NCBI_CONST_UINT8(4294967295), kBad, kBad, 0. },
    {  "4,294,967,296", NStr::fAllowCommas, -1, NCBI_CONST_UINT8(4294967296), NCBI_CONST_UINT8(4294967296), NCBI_CONST_UINT8(4294967296), NCBI_CONST_UINT8(4294967296), kBad, kBad, 0. },
    { "-4,294,967,294", NStr::fAllowCommas, -1, NCBI_CONST_INT8(-4294967294), kBad,                         NCBI_CONST_INT8(-4294967294), kBad,                         kBad, kBad, 0. },
    { "-4,294,967,295", NStr::fAllowCommas, -1, NCBI_CONST_INT8(-4294967295), kBad,                         NCBI_CONST_INT8(-4294967295), kBad,                         kBad, kBad, 0. },
    { "-4,294,967,296", NStr::fAllowCommas, -1, NCBI_CONST_INT8(-4294967296), kBad,                         NCBI_CONST_INT8(-4294967296), kBad,                         kBad, kBad, 0. },
#else
    {  "4,294,967,294", NStr::fAllowCommas, -1, kBad,                         NCBI_CONST_UINT8(4294967294), NCBI_CONST_UINT8(4294967294), NCBI_CONST_UINT8(4294967294), kBad, kBad, 0. },
    {  "4,294,967,295", NStr::fAllowCommas, -1, kBad,                         NCBI_CONST_UINT8(4294967295), NCBI_CONST_UINT8(4294967295), NCBI_CONST_UINT8(4294967295), kBad, kBad, 0. },
    {  "4,294,967,296", NStr::fAllowCommas, -1, kBad,                         kBad,                         NCBI_CONST_UINT8(4294967296), NCBI_CONST_UINT8(4294967296), kBad, kBad, 0. },
    { "-4,294,967,294", NStr::fAllowCommas, -1, kBad,                         kBad,                         NCBI_CONST_INT8(-4294967294), kBad,                         kBad, kBad, 0. },
    { "-4,294,967,295", NStr::fAllowCommas, -1, kBad,                         kBad,                         NCBI_CONST_INT8(-4294967295), kBad,                         kBad, kBad, 0. },
    { "-4,294,967,296", NStr::fAllowCommas, -1, kBad,                         kBad,                         NCBI_CONST_INT8(-4294967296), kBad,                         kBad, kBad, 0. },
#endif

    { "+123",     0, 123, 123, 123, 123, 123, 123, 123, 0. },
    { "123",      NStr::fMandatorySign, 123, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "+123",     NStr::fMandatorySign, 123,  123,  123,  123,  123, 123, 123, 0. },
    { "-123",     NStr::fMandatorySign,  -1, -123, kBad, -123, kBad, -123, -123, 0. },
    { "+123",     NStr::fAllowLeadingSymbols, 123,  123,  123,  123,  123,  123, 123, 0. },
#if 0
    { "7E-380",   DF, -1, kBad, kBad, kBad, kBad, kBad, kBad,   0. },
    { "7E-325",   DF, -1, kBad, kBad, kBad, kBad, kBad, kBad,   0. },
    { "7E-324",   DF, -1, kBad, kBad, kBad, kBad, kBad, 7E-324, 0. },
    { "7E-323",   DF, -1, kBad, kBad, kBad, kBad, kBad, 7E-323, 0. },
#endif
#if 0 && defined(NCBI_OS_LINUX) && (NCBI_PLATFORM_BITS == 32) && !defined(_DEBUG)
    { "7E-38",   DF, -1, kBad, kBad, kBad, kBad, 7E-38, 7E-38, 0.000000000000002e-38 },
#else
    { "7E-38",   DF, -1, kBad, kBad, kBad, kBad, 7E-38f, 7E-38, 0. },
#endif
    { "7E38",    DF, -1, kBad, kBad, kBad, kBad, kBad, 7E38, 0. },

    { "2.2e-308",DF, -1, kBad, kBad, kBad, kBad, 0, 0, DBL_MIN },
    { "-2.2e-310",DF, -1, kBad, kBad, kBad, kBad, 0, 0, DBL_MIN },
    { "7E-500",  DF, -1, kBad, kBad, kBad, kBad, 0, 0, 0. },
    { "7E-512",  DF, -1, kBad, kBad, kBad, kBad, 0, 0, 0. },
    { "7E500",   DF, -1, kBad, kBad, kBad, kBad, kBad, HUGE_VAL, 0. },
    { "7E512",   DF, -1, kBad, kBad, kBad, kBad, kBad, HUGE_VAL, 0. },
    { "7E768",   DF, -1, kBad, kBad, kBad, kBad, kBad, HUGE_VAL, 0. },
    { "7E4294967306", DF, -1, kBad, kBad, kBad, kBad, kBad, HUGE_VAL, 0. },
    { ".000000000000000000000000000001", DF, -1, kBad, kBad, kBad, kBad,
       .000000000000000000000000000001f, .000000000000000000000000000001, 1e-46 },
    { "-123",     NStr::fAllowLeadingSymbols,  -1, -123, kBad, -123, kBad, -123, -123, 0. }
};


BOOST_AUTO_TEST_CASE(s_StringToNum)
{
    const size_t count = sizeof(s_Str2NumTests) / sizeof(s_Str2NumTests[0]);
    {{
        // verify that compiler properly distinguishes overloads
        string str_value;
        NStr::NumericToString(str_value, (char)1); 
        TIntId id = 123;
        TGi gi(id);
        NStr::NumericToString(str_value, id);
        NStr::NumericToString(str_value, gi);
    }}

    for (size_t i = 0;  i < count;  ++i) {
      for (int extra = 0;  extra < 4;  ++extra) {
        const SStringNumericValues* test = &s_Str2NumTests[i];
        
        CTempString str;
        string extra_str;
        if ( !extra ) {
            str = test->str;
        }
        else {
            extra_str = test->str;
            extra_str += "  9x"[extra];
            str = CTempString(extra_str).substr(0, extra_str.size()-1);
        }
        NStr::TStringToNumFlags flags = test->flags;
        NStr::TNumToStringFlags str_flags = 0;
        if ( flags & NStr::fMandatorySign ) 
            str_flags |= NStr::fWithSign;
        if ( flags & NStr::fAllowCommas )
            str_flags |= NStr::fWithCommas;
        bool allow_same_test = (flags < NStr::fAllowLeadingSpaces);

        // num
        {{
            errno = kTestErrno;
            int value = NStr::StringToNonNegativeInt(str);
            CHECK_ERRNO;
            BOOST_CHECK_EQUAL(value, test->num);
        }}

        // int
        try {
            errno = kTestErrno;
            int value = NStr::StringToInt(str, flags), v2;
            CHECK_ERRNO;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<int>(str, flags));
            BOOST_CHECK(NStr::StringToNumeric(str, &v2, flags));
            BOOST_CHECK_EQUAL(v2, value);
            BOOST_CHECK(test->IsGoodInt());
            BOOST_CHECK_EQUAL(value, test->i);
            if (allow_same_test) {
                BOOST_CHECK(test->Same(NStr::IntToString(value, str_flags)));
                BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
            }
        }
        catch (CException&) {
            BOOST_CHECK(!test->IsGoodInt());
        }

        // int
        {
            errno = kTestErrno;
            int value = NStr::StringToInt(str, flags | NStr::fConvErr_NoThrow);
            CHECK_ERRNO;
            int err = errno;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<int>(str,
                              flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
                BOOST_CHECK(test->IsGoodInt());
                BOOST_CHECK_EQUAL(value, test->i);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::IntToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            }
            else {
                BOOST_CHECK( CNcbiError::GetLast() );
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodInt());
            }
        }

        // unsigned int
        try {
            errno = kTestErrno;
            unsigned int value = NStr::StringToUInt(str, flags), v2;
            CHECK_ERRNO;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<unsigned int>(str, flags));
            BOOST_CHECK(NStr::StringToNumeric(str, &v2, flags));
            BOOST_CHECK_EQUAL(v2, value);
            BOOST_CHECK(test->IsGoodUInt());
            BOOST_CHECK_EQUAL(value, test->u);
            if (allow_same_test) {
                BOOST_CHECK(test->Same(NStr::UIntToString(value, str_flags)));
                BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
            }
        }
        catch (CException&) {
            BOOST_CHECK(!test->IsGoodUInt());
        }

        // unsigned int
        {
            errno = kTestErrno;
            unsigned int value = NStr::StringToUInt(str, flags | NStr::fConvErr_NoThrow);
            CHECK_ERRNO;
            int err = errno;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<unsigned int>(str, 
                              flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
                BOOST_CHECK(test->IsGoodUInt());
                BOOST_CHECK_EQUAL(value, test->u);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::UIntToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            }
            else {
                BOOST_CHECK( CNcbiError::GetLast() );
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodUInt());
            }
        }

        // long
        try {
            errno = kTestErrno;
            long value = NStr::StringToLong(str, flags), v2;
            CHECK_ERRNO;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<long>(str, flags));
            BOOST_CHECK(NStr::StringToNumeric(str, &v2, flags));
            BOOST_CHECK_EQUAL(v2, value);

            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(test->IsGoodInt());
                BOOST_CHECK_EQUAL(value, test->i);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::LongToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #else
                BOOST_CHECK(test->IsGoodInt8());
                BOOST_CHECK_EQUAL(value, test->i8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::Int8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #endif
        }
        catch (CException&) {
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(!test->IsGoodInt());
            #else
                BOOST_CHECK(!test->IsGoodInt8());
            #endif
        }

        // long
        {
            errno = kTestErrno;
            long value = NStr::StringToLong(str, flags | NStr::fConvErr_NoThrow);
            CHECK_ERRNO;
            int err = errno;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<long>(str, 
                              flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(test->IsGoodInt());
                BOOST_CHECK_EQUAL(value, test->i);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::LongToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #else
                BOOST_CHECK(test->IsGoodInt8());
                BOOST_CHECK_EQUAL(value, test->i8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::Int8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #endif
            }
            else {
                BOOST_CHECK( CNcbiError::GetLast() );
                BOOST_CHECK(err);
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(!test->IsGoodInt());
            #else
                BOOST_CHECK(!test->IsGoodInt8());
            #endif
            }
        }

        // unsigned long
        try {
            errno = kTestErrno;
            unsigned long value = NStr::StringToULong(str, flags), v2;
            CHECK_ERRNO;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<unsigned long>(str, flags));
            BOOST_CHECK(NStr::StringToNumeric(str, &v2, flags));
            BOOST_CHECK_EQUAL(v2, value);
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(test->IsGoodUInt());
                BOOST_CHECK_EQUAL(value, test->u);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::ULongToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #else
                BOOST_CHECK(test->IsGoodUInt8());
                BOOST_CHECK_EQUAL(value, test->u8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::UInt8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #endif
        }
        catch (CException&) {
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(!test->IsGoodUInt());
            #else
                BOOST_CHECK(!test->IsGoodUInt8());
            #endif
        }

        // unsigned long
        {
            errno = kTestErrno;
            unsigned long value = NStr::StringToULong(str, flags | NStr::fConvErr_NoThrow);
            CHECK_ERRNO;
            int err = errno;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<unsigned long>(str,
                              flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(test->IsGoodUInt());
                BOOST_CHECK_EQUAL(value, test->u);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::ULongToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #else
                BOOST_CHECK(test->IsGoodUInt8());
                BOOST_CHECK_EQUAL(value, test->u8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::UInt8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #endif
            }
            else {
                BOOST_CHECK( CNcbiError::GetLast() );
                BOOST_CHECK(err);
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(!test->IsGoodUInt());
            #else
                BOOST_CHECK(!test->IsGoodUInt8());
            #endif
            }
        }

        // Int8
        try {
            errno = kTestErrno;
            Int8 value = NStr::StringToInt8(str, flags), v2;
            CHECK_ERRNO;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<Int8>(str, flags));
            BOOST_CHECK(NStr::StringToNumeric(str, &v2, flags));
            BOOST_CHECK_EQUAL(v2, value);
            BOOST_CHECK(test->IsGoodInt8());
            BOOST_CHECK_EQUAL(value, test->i8);
            if (allow_same_test) {
                BOOST_CHECK(test->Same(NStr::Int8ToString(value, str_flags)));
                BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
            }
        }
        catch (CException&) {
            BOOST_CHECK(!test->IsGoodInt8());
        }

        // Int8
        {
            errno = kTestErrno;
            Int8 value = NStr::StringToInt8(str, flags | NStr::fConvErr_NoThrow);
            int err = errno;
            BOOST_CHECK(err != kTestErrno);
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<Int8>(str, 
                flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
                BOOST_CHECK(test->IsGoodInt8());
                BOOST_CHECK_EQUAL(value, test->i8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::Int8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            }
            else {
                BOOST_CHECK( CNcbiError::GetLast() );
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodInt8());
            }
        }

        // Uint8
        try {
            errno = kTestErrno;
            Uint8 value = NStr::StringToUInt8(str, flags), v2;
            CHECK_ERRNO;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<Uint8>(str, flags));
            BOOST_CHECK(NStr::StringToNumeric(str, &v2, flags));
            BOOST_CHECK_EQUAL(v2, value);
            BOOST_CHECK(test->IsGoodUInt8());
            BOOST_CHECK_EQUAL(value, test->u8);
            if (allow_same_test) {
                BOOST_CHECK(test->Same(NStr::UInt8ToString(value, str_flags)));
                BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
            }
        }
        catch (CException&) {
            BOOST_CHECK(!test->IsGoodUInt8());
        }

        // Uint8
        {
            errno = kTestErrno;
            Uint8 value = NStr::StringToUInt8(str, flags | NStr::fConvErr_NoThrow);
            CHECK_ERRNO;
            int err = errno;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<Uint8>(str, 
                              flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
                BOOST_CHECK(test->IsGoodUInt8());
                BOOST_CHECK_EQUAL(value, test->u8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::UInt8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            }
            else {
                BOOST_CHECK( CNcbiError::GetLast() );
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodUInt8());
            }
        }

        // float -- StringToNumeric<float>
        try {
            errno = kTestErrno;
            double value = NStr::StringToDouble(str, flags);
            CHECK_ERRNO;
            errno = kTestErrno;
            double valueP = NStr::StringToDouble(str, flags | NStr::fDecimalPosix);
            CHECK_ERRNO;
            //BOOST_CHECK_EQUAL(value, valueP);
            // Note, we check conversion only, not rounding from double to float.
            BOOST_CHECK_EQUAL((float)value,  NStr::StringToNumeric<float>(str, flags));
            BOOST_CHECK_EQUAL((float)valueP, NStr::StringToNumeric<float>(str, flags | NStr::fDecimalPosix));
            BOOST_CHECK(test->IsGoodFloat());
        }
        catch (CException&) {
            BOOST_CHECK(!test->IsGoodFloat());
        }

        // double
        try {
            errno = kTestErrno;
            double value = NStr::StringToDouble(str, flags);
            CHECK_ERRNO;
            errno = kTestErrno;
            double valueP = NStr::StringToDouble(str, flags | NStr::fDecimalPosix);
            CHECK_ERRNO;
            //BOOST_CHECK_EQUAL(value, valueP);
            BOOST_CHECK_EQUAL(value,  NStr::StringToNumeric<double>(str, flags));
            BOOST_CHECK_EQUAL(valueP, NStr::StringToNumeric<double>(str, flags | NStr::fDecimalPosix));
            BOOST_CHECK(test->IsGoodDouble());
            BOOST_CHECK(valueP >= test->d-test->delta && valueP <= test->d+test->delta);
            BOOST_CHECK(value  >= test->d-test->delta && value  <= test->d+test->delta);
        }
        catch (CException&) {
            BOOST_CHECK(!test->IsGoodDouble());
        }

        // double
        {
            errno = kTestErrno;
            double value = NStr::StringToDouble(str, flags | NStr::fConvErr_NoThrow);
            CHECK_ERRNO;
            int err = errno;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<double>(str, 
                              flags | NStr::fConvErr_NoThrow));
            if ( value || (err == 0 || err == ERANGE)) {
                BOOST_CHECK(test->IsGoodDouble());
                BOOST_CHECK(value  >= test->d-test->delta && value  <= test->d+test->delta);
            }
            else {
                BOOST_CHECK( CNcbiError::GetLast() );
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodDouble());
            }
        }

        // double POSIX
        {
            errno = kTestErrno;
            double value = NStr::StringToDouble(str, flags | NStr::fDecimalPosix | NStr::fConvErr_NoThrow);
            CHECK_ERRNO;
            int err = errno;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<double>(str, 
                              flags | NStr::fDecimalPosix | NStr::fConvErr_NoThrow));
            if ( value || (err == 0 || err == ERANGE)) {
                BOOST_CHECK(test->IsGoodDouble());
                BOOST_CHECK(value  >= test->d-test->delta && value  <= test->d+test->delta);
            }
            else {
                BOOST_CHECK( CNcbiError::GetLast() );
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodDouble());
            }
        }
      }
    }

    // TGi
    {
        TIntId id = 123;
        TGi gi(id), gi2, gi3;
        string s = NStr::NumericToString(gi);
        gi2 = NStr::StringToNumeric<TGi>(s);
        BOOST_CHECK(NStr::StringToNumeric(s, &gi3));
        BOOST_CHECK(gi2 == gi);
        BOOST_CHECK(gi2 == gi3);
    }
    {
        unsigned long long n1 = 456, n2, n3;
        string s = NStr::NumericToString(n1);
        n2 = NStr::StringToNumeric<unsigned long long>(s);
        BOOST_CHECK(NStr::StringToNumeric(s, &n3));
        BOOST_CHECK(n2 == n1);
        BOOST_CHECK(n2 == n3);
    }
}


struct SStringDoublePosixTest
{
    const char* str;
    double result;
    double delta;
};

static const SStringDoublePosixTest s_StrToDoublePosix[] = {
    {"123",                 123.,                1e-13},
    {"123.",                123.,                1e-13},
    {"123.456",             123.456,             1e-13},
    {"-123.456",           -123.456,             1e-14},
    {"-12.45",             -12.45,               1e-15},
    {"0.01",                0.01,                1e-18},
    {"0.01456000",          0.01456,             1e-18},
    {"2147483649",          2147483649.,         0.},
    {"-2147483649",        -2147483649.,         0.},
    {"214748364913",        214748364913.,       0.},
    {"123456789123456789",  123456789123456789., 0.},
    {"123456789123.45",     123456789123.45,     0.},
    {"1234.5678912345",     1234.5678912345,     0.},
    {"1.23456789123456789", 1.23456789123456789, 0.},
    {".123456789",          .123456789,          0.},
    {".123456789123",       .123456789123,       0.},
    {"12e12",               12.e12,              0.},
    {"123.e2",              123.e2,              0.},
    {"123.456e+2",          123.456e+2,          0.},
    {"+123.456e-2",         123.456e-2,          0.},
    {"-123.456e+2",        -123.456e+2,          0.},
    {"-123.456e+12",       -123.456e+12,         0.},
    {"-123.456e+25",       -123.456e+25,         0.},
    {"-123.456e+78",       -123.456e+78,         0.},
    {"-123.456e-2",        -123.456e-2,          0.},
    {"-123.456e-12",       -123.456e-12,         0.},
    {"-123.456e-25",       -123.456e-25,         0.},
    {"-123.456e-78",       -123.456e-78,         0.00000000000002e-078},
    {"-9223372036854775809",      -9223372036854775809.,       0.},
    {"-922337.2036854775809",     -922337.2036854775809,       0.},
    {"-92233720368547.75809",     -92233720368547.75809,       0.},
    {"-9223372036854775808",      -9223372036854775808.,       0.},
    {"-9223372036854775807",      -9223372036854775807.,       0.},
    {"9223372036854775806",        9223372036854775806.,       0.},
    {"9223372036854775807",        9223372036854775807.,       0.},
    {"9223372036854775808",        9223372036854775808.,       0.},
    {"18446744073709551614",       18446744073709551614.,      0.},
    {"18446744073709551615",       18446744073709551615.,      0.},
    {"18446744073709551616",       18446744073709551616.,      0.},
    {"1844674407370955.1616",      1844674407370955.1616,      0.},
    {"1844674407370955.1616",      1844674407370955.1616,      0.},
    {"184467.44073709551616",      184467.44073709551616,      0.},
    {"1.8446744073709551616",      1.8446744073709551616,      0.},
    {"1.8446744073709551616e5",    1.8446744073709551616e5,    0.},
    {"1.8446744073709551616e25",   1.8446744073709551616e25,   0.},
    {"1.8446744073709551616e125",  1.8446744073709551616e125,  0.},
    {"184467.44073709551616e5",    184467.44073709551616e5,    0.},
    {"184467.44073709551616e-5",   184467.44073709551616e-5,   0.},
    {"184467.44073709551616e25",   184467.44073709551616e25,   0.},
    {"184467.44073709551616e-25",  184467.44073709551616e-25,  0.},

    {"1.7976931348623159e+308",  HUGE_VAL, 0.},
    {"1.7976931348623157e+308",  1.7976931348623157e+308, 0.},
    {"1.7976931348623155e+308",  1.7976931348623155e+308, 0.0000000000000003e+308},
    { "1.797693134862315e+307",  1.797693134862315e+307,  0.000000000000002e+307},
    { "1.797693134862315e+306",  1.797693134862315e+306,  0.},
    {"2.2250738585072014e-308",  2.2250738585072014e-308, 0.},
    {"2.2250738585072019e-308",  2.2250738585072019e-308, 0.},
    {"2.2250738585072024e-308",  2.2250738585072024e-308, 0.},
    { "2.225073858507202e-308",  2.225073858507202e-308,  0.},
    {"2.2250738585072016e-307",  2.2250738585072016e-307, 0.0000000000000004e-307},/* NCBI_FAKE_WARNING */
    {"2.2250738585072016e-306",  2.2250738585072016e-306, 0.0000000000000004e-306},/* NCBI_FAKE_WARNING */
    {"2.2250738585072016e-305",  2.2250738585072016e-305, 0.0000000000000004e-305},/* NCBI_FAKE_WARNING */
    {"-123.456e+4578",  -HUGE_VAL, 0.},
    {"-123.456e-4578",  0., 0.},

    { "7E0000000001", 70., 0.},
    { "7E512", HUGE_VAL, 0.},
    { "7E-500", 0., 0.},
    { "7E4294967306", HUGE_VAL, 0.},
    { "1.000000000000000000000000000001", 1., 0. },
    { "000.000000000000000000000000000001", 1e-30, 0.0000000000000001e-030 },
    { "0.", 0., 0. },
    { "-0", 0., 0. },
    {NULL,0,0}
};

BOOST_AUTO_TEST_CASE(s_StringToDoublePosix)
{
    char* endptr;
    for (int i = 0; s_StrToDoublePosix[i].str; ++i) {
        const double& result  = s_StrToDoublePosix[i].result;
        double delta   = finite(result)? fabs(result)*2.22e-16: 0;
        const char* str = s_StrToDoublePosix[i].str;
        errno = kTestErrno;
        endptr = 0;
        double valuep = NStr::StringToDoublePosix(str, &endptr);
        if ( delta == 0 )
            BOOST_CHECK(valuep == result);
        double min = result-delta, max = result+delta;
        if ( finite(min) )
            BOOST_CHECK(valuep >= min);
        if ( finite(max) )
            BOOST_CHECK(valuep <= max);
    }
    
    string out;
    double value;

    value = NStr::StringToDoublePosix("nan", &endptr);
    BOOST_CHECK( isnan(value) );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "NaN") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "NaN") == 0 );

    value = NStr::StringToDoublePosix(out.c_str(), &endptr);
    BOOST_CHECK( isnan(value) );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "NaN") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "NaN") == 0 );

    value = NStr::StringToDoublePosix("inf", &endptr);
    BOOST_CHECK( !finite(value) && value>0.);
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix(out.c_str(), &endptr);
    BOOST_CHECK( endptr && !*endptr );
    BOOST_CHECK( !finite(value) && value>0.);
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("infinity", &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix(out.c_str(), &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("+inf", &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("+infinity", &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("-inf", &endptr);
    BOOST_CHECK( !finite(value) && value<0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );

    value = NStr::StringToDoublePosix("-infinity", &endptr);
    BOOST_CHECK( !finite(value) && value<0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );

    value = NStr::StringToDoublePosix("+Infinity", &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("Infinity", &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("-infinity", &endptr);
    BOOST_CHECK( !finite(value) && value<0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );

    value = NStr::StringToDoublePosix("-0", &endptr);
    BOOST_CHECK( value == 0. );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-0") == 0 );
}

static const SStringNumericValues s_Str2NumNonPosixTests[] = {
    { "",          DF,                         -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "",          NStr::fConvErr_NoThrow,     -1, kBad, kBad, kBad, kBad, 0.f,  0.,   0. },
    {  ",",        DF,                         -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    {  ",,",       DF,                         -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    {  ".,",       NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    {  ",.",       NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { ",0",        DF,                         -1, kBad, kBad, kBad, kBad, .0f , .0,   0. },
    { ",0.",       NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { ".0,",       NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { ",01",       DF,                         -1, kBad, kBad, kBad, kBad, .01f, .01,  0. },
    { "1,",        DF,                         -1, kBad, kBad, kBad, kBad, 1.f , 1.,   0. },
    { "1,1",       DF,                         -1, kBad, kBad, kBad, kBad, 1.1f, 1.1,  0. },
    { "1,1",       NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, 1.1f, 1.1,  0. },
    { "1,1",       NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "1.1",       NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, 1.1f, 1.1,  0. },
    { "1.1",       NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad, 1.1f, 1.1,  0. },
    { "1,1.",      NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "1,1,",      DF,                         -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "1.,",       NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "1.1,",      NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "1.1,",      NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "1.,",       NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "1.,",       NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "12,34",     DF,                         -1, kBad, kBad, kBad, kBad, 12.34f, 12.34, 0. },
    { "12,34",     NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, 12.34f, 12.34, 0. },
    { "12.34",     NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, 12.34f, 12.34, 0. },
    { "12.34",     NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad, 12.34f, 12.34, 0. },
    { "12,34e-2",  DF,                         -1, kBad, kBad, kBad, kBad, .1234f, .1234, 1e-17 },
    { "12,34e-2",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, .1234f, .1234, 1e-17 },
    { "12.34e-2",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, .1234f, .1234, 1e-17 },
    { "12.34e-2",  NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad, .1234f, .1234, 1e-17 },
    { "1234,",     NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, 1234.f, 1234., 0. },
    { "1234.",     NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad, 1234.f, 1234., 0. },
    { "1234",      NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, 1234.f, 1234., 0. },
    { "0,0",       NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, 0.f,  0.,   0. },
    { "0,000",     NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, 0.f,  0.,   0. },
    { "0.000",     NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad, 0.f,  0.,   0. },
    { ",,1234",    NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "1234,,",    NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. },
    { "12,,34",    NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, kBad, 0. }
};

BOOST_AUTO_TEST_CASE(s_StringToDouble)
{
    char* prevlocal = NcbiSysChar_strdup(setlocale(LC_NUMERIC,NULL));
    if (!setlocale(LC_NUMERIC,"deu")) {
        if (!setlocale(LC_NUMERIC,"de")) {
            if (!setlocale(LC_NUMERIC,"de_DE")) {
                if (!setlocale(LC_NUMERIC,"fr")) {
                    // cannot find suitable locale, skip the test
                    free(prevlocal);
                    return;
                }
            }
        }
    }
    const size_t count = sizeof(s_Str2NumNonPosixTests) / sizeof(s_Str2NumNonPosixTests[0]);

    for (size_t i = 0;  i < count;  ++i) {
        const SStringNumericValues* test = &s_Str2NumNonPosixTests[i];
        const char*                 str  = test->str;
        NStr::TStringToNumFlags     flags = test->flags;

        // double
        try {
            errno = kTestErrno;
            double value = NStr::StringToDouble(str, flags);
            CHECK_ERRNO;
            BOOST_CHECK(test->IsGoodDouble());
            BOOST_CHECK(value >= test->d-test->delta && value <= test->d+test->delta);
        }
        catch (CException&) {
            BOOST_CHECK(!test->IsGoodDouble());
        }
    }
    setlocale(LC_NUMERIC,prevlocal);
    free(prevlocal);
}


//----------------------------------------------------------------------------
// NStr::StringTo*() radix test
//----------------------------------------------------------------------------

// Writing separate tests for StringToUInt8 because we
// need to test for different radix values such as 2, 8, and 16.

struct SRadixTest {
    const char* str;      // String input
    int         base;     // Radix base 
    Uint8       value;    // Expected value

    bool Same(const string& s) const
    {
        if ( s.empty() ) {
            return false;
        }
        const char* s1 = str;
        const char* s2 = s.c_str();
        
        // Workaround for ICC 8.0. It have an optimizer bug.
        // c_str() can return non-null-terminated string.
        // So we will use strncmp() here instead of strcmp().
        
        size_t n = s.length();
        
        while (*s1 == '0') s1++;
        if ( *s1 == 'x' )  s1++;
        while (*s2 == '0') { s2++; n--; };

        // Use case-insensitive comparison for base 16
        if (base == 16) {
            return (NStr::strncasecmp(s1, s2, n) == 0);
        }
        return (NStr::strncmp(s1, s2, n) == 0);
    }
};

static const SRadixTest s_RadixTests[] = {
    { "0",         16, 0 },
    { "A",         16, 10 },
    { "a",         16, 10 },
    { "0xA",       16, 10 },
    { "0xa",       16, 10 },
    { "B9",        16, 185 },
    { "b9",        16, 185 },
    { "C5D",       16, 3165 },
    { "FFFF",      16, 65535 },
    { "ffff",      16, 65535 },
    { "17ABCDEF",  16, 397135343 },
    { "BADBADBA",  16, 3134959034U },
    { "badbadba",  16, 3134959034U },
    { "0",          8, 0 },
    { "0",          8, 0 },
    { "7",          8, 7 },
    { "17",         8, 15 },
    { "177",        8, 127 },
    { "0123",       8, 83 },
    { "01234567",   8, 342391 },
    { "0",          2, 0 },
    { "1",          2, 1 },
    { "10",         2, 2 },
    { "11",         2, 3 },
    { "100",        2, 4 },
    { "101",        2, 5 }, 
    { "110",        2, 6 },
    { "111",        2, 7 },

    // Autodetect radix base
    { "0xC5D",      0, 3165 },    // base 16
    { "0123",       0, 83   },    // base 8
    { "123",        0, 123  },    // base 10
    { "111",        0, 111  },    // base 10

    // Invalid values come next
    { "10ABCDEFGH",16, kBad },
    { "12345A",    10, kBad },
    { "012345678",  8, kBad },
    { "012",        2, kBad }
};

BOOST_AUTO_TEST_CASE(s_StringToNum_Radix)
{
    const size_t count = sizeof(s_RadixTests)/sizeof(s_RadixTests[0]);

    for (size_t i = 0;  i < count;  ++i)
    {
        const SRadixTest* test = &s_RadixTests[i];

        // Int
        try {
            if ( test->value <= (Uint8)kMax_Int ) {
                errno = kTestErrno;
                int val = NStr::StringToInt(test->str, 0, test->base);
                BOOST_CHECK(errno != kTestErrno);
                string str;
                if ( test->base ) {
                    errno = kTestErrno;
                    NStr::IntToString(str, val, 0, test->base);
                    BOOST_CHECK(errno != kTestErrno);
                }
                BOOST_CHECK_EQUAL((Uint8)val, test->value);
                if ( test->base ) {
                    BOOST_CHECK(test->Same(str));
                    NStr::NumericToString(str, val, 0, test->base);
                    BOOST_CHECK(test->Same(str));
                }
            }
        }
        catch (CException&) {
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }

        // UInt
        try {
            if ( test->value <= kMax_UInt ) {
                errno = kTestErrno;
                unsigned int val = NStr::StringToUInt(test->str, 0, test->base);
                BOOST_CHECK(errno != kTestErrno);
                string str;
                if ( test->base ) {
                    errno = kTestErrno;
                    NStr::UIntToString(str, val, 0, test->base);
                    BOOST_CHECK(errno != kTestErrno);
                }
                BOOST_CHECK_EQUAL(val, test->value);
                if ( test->base ) {
                    BOOST_CHECK(test->Same(str));
                    NStr::NumericToString(str, val, 0, test->base);
                    BOOST_CHECK(test->Same(str));
                }
            }
        }
        catch (CException&) {
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }

        // Int8
        try {
            if ( test->value <= (Uint8)kMax_I8 ) {
                errno = kTestErrno;
                Int8 val = NStr::StringToInt8(test->str, 0, test->base);
                BOOST_CHECK(errno != kTestErrno);
                string str;
                if ( test->base ) {
                    errno = kTestErrno;
                    NStr::Int8ToString(str, val, 0, test->base);
                    BOOST_CHECK(errno != kTestErrno);
                }
                BOOST_CHECK_EQUAL((Uint8)val, test->value);
                if ( test->base ) {
                    BOOST_CHECK(test->Same(str));
                    NStr::NumericToString(str, val, 0, test->base);
                    BOOST_CHECK(test->Same(str));
                }
            }
        }
        catch (CException&) {
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }

        // Uint8
        try {
            errno = kTestErrno;
            Uint8 val = NStr::StringToUInt8(test->str, 0, test->base);
            BOOST_CHECK(errno != kTestErrno);
            string str;
            if ( test->base ) {
                errno = kTestErrno;
                NStr::UInt8ToString(str, val, 0, test->base);
                BOOST_CHECK(errno != kTestErrno);
            }
            BOOST_CHECK_EQUAL(val, test->value);
            if ( test->base ) {
                BOOST_CHECK(test->Same(str));
                NStr::NumericToString(str, val, 0, test->base);
                BOOST_CHECK(test->Same(str));
            }
        }
        catch (CException&) {
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }
    } 

    // Some additional tests

    string str;

    NStr::IntToString(str, kMax_Int, 0, 2);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "1111111111111111111111111111111");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "111111111111111111111111111111111111111111111111111111111111111");
#endif
    NStr::NumericToString(str, kMax_Int, 0, 2);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "1111111111111111111111111111111");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "111111111111111111111111111111111111111111111111111111111111111");
#endif

    NStr::LongToString(str, kMax_Long, 0, 2);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "1111111111111111111111111111111");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "111111111111111111111111111111111111111111111111111111111111111");
#endif
    NStr::NumericToString(str, kMax_Long, 0, 2);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "1111111111111111111111111111111");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "111111111111111111111111111111111111111111111111111111111111111");
#endif

    NStr::UIntToString(str, kMax_UInt, 0, 2);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "11111111111111111111111111111111");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "1111111111111111111111111111111111111111111111111111111111111111");
#endif
    NStr::NumericToString(str, kMax_UInt, 0, 2);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "11111111111111111111111111111111");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "1111111111111111111111111111111111111111111111111111111111111111");
#endif

    NStr::ULongToString(str, kMax_ULong, 0, 2);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "11111111111111111111111111111111");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "1111111111111111111111111111111111111111111111111111111111111111");
#endif
    NStr::NumericToString(str, kMax_ULong, 0, 2);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "11111111111111111111111111111111");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "1111111111111111111111111111111111111111111111111111111111111111");
#endif

    NStr::IntToString(str, -1, 0, 8);
#if (SIZEOF_INT == 4) 
    BOOST_CHECK(str == "37777777777");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "1777777777777777777777");
#endif
    NStr::NumericToString(str, -1, 0, 8);
#if (SIZEOF_INT == 4) 
    BOOST_CHECK(str == "37777777777");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "1777777777777777777777");
#endif

    NStr::LongToString(str, -1, 0, 8);
#if (SIZEOF_LONG == 4) 
    BOOST_CHECK(str == "37777777777");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "1777777777777777777777");
#endif
    NStr::NumericToString(str, (long)-1, 0, 8);
#if (SIZEOF_LONG == 4) 
    BOOST_CHECK(str == "37777777777");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "1777777777777777777777");
#endif

    NStr::IntToString(str, -1, 0, 16);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "FFFFFFFF");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "FFFFFFFFFFFFFFFF");
#endif
    NStr::NumericToString(str, -1, 0, 16);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "FFFFFFFF");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "FFFFFFFFFFFFFFFF");
#endif

    NStr::LongToString(str, -1, 0, 16);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "FFFFFFFF");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "FFFFFFFFFFFFFFFF");
#endif
    NStr::NumericToString(str, (long)-1, 0, 16);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "FFFFFFFF");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "FFFFFFFFFFFFFFFF");
#endif

    NStr::UInt8ToString(str, NCBI_CONST_UINT8(12345678901234567), 0, 16);
    BOOST_CHECK(str == "2BDC545D6B4B87");
    NStr::NumericToString(str, NCBI_CONST_UINT8(12345678901234567), 0, 16);
    BOOST_CHECK(str == "2BDC545D6B4B87");
}


//----------------------------------------------------------------------------
// NStr::StringTo*_DataSize()
//----------------------------------------------------------------------------

struct SStringDataSizeValues
{
    const char*             str;
    NStr::TStringToNumFlags flags;
    Uint8                   expected;

    bool IsGood(void) const {
        return expected != (Uint8)kBad;
    }
};

static const NStr::TStringToNumFlags kNoThrow = NStr::fConvErr_NoThrow;
static const SStringDataSizeValues s_Str2DataSizeTests[] = {
    // str  flags     num
    { "10",    0,      10 },
    { "10b",   0,      10 },
    { "10k",   0, 10*1000 },
    { "10K",   0, 10*1000 },
    { "10KB",  0, 10*1000 },
    { "10KiB", 0, 10*1024 },
    { "10KIB", 0, 10*1024 },
    { "10K",   NStr::fDS_ForceBinary, 10*1024 },
    { "10KB",  NStr::fDS_ForceBinary, 10*1024 },
    { "10M",   0, 10*1000*1000 },
    { "10MB",  0, 10*1000*1000 },
    { "10MiB", 0, 10*1024*1024 },
    { "10M",   NStr::fDS_ForceBinary, 10*1024*1024 },
    { "10MB",  NStr::fDS_ForceBinary, 10*1024*1024 },
    { "10G",   0, Uint8(10)*1000*1000*1000 },
    { "10GB",  0, Uint8(10)*1000*1000*1000 },
    { "10GiB", 0, Uint8(10)*1024*1024*1024 },
    { "10G",   NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024 },
    { "10GB",  NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024 },
    { "10T",   0, Uint8(10)*1000*1000*1000*1000 },
    { "10TB",  0, Uint8(10)*1000*1000*1000*1000 },
    { "10TiB", 0, Uint8(10)*1024*1024*1024*1024 },
    { "10T",   NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024 },
    { "10TB",  NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024 },
    { "10P",   0, Uint8(10)*1000*1000*1000*1000*1000 },
    { "10PB",  0, Uint8(10)*1000*1000*1000*1000*1000 },
    { "10PiB", 0, Uint8(10)*1024*1024*1024*1024*1024 },
    { "10P",   NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024*1024 },
    { "10PB",  NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024*1024 },
    { "10E",   0, Uint8(10)*1000*1000*1000*1000*1000*1000 },
    { "10EB",  0, Uint8(10)*1000*1000*1000*1000*1000*1000 },
    { "10EiB", 0, Uint8(10)*1024*1024*1024*1024*1024*1024 },
    { "10E",   NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024*1024*1024 },
    { "10EB",  NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024*1024*1024 },
    { "+10K",  0, 10*1000 },
    { "-10K",  0, kBad },
    { "1GBx",  0, kBad },
    { "1Gi",   0, kBad },
    { "10000000000000GB", 0, kBad },
    { " 10K",  0, kBad },
    { " 10K",  NStr::fAllowLeadingSpaces, 10*1000 },
    { "10K ",  0, kBad },
    { "10K ",  NStr::fAllowTrailingSpaces, 10*1000 },
    { "10 K",  0, 10*1000 },
    { "10 K",  NStr::fDS_ProhibitSpaceBeforeSuffix, kBad },
    { "10K",   NStr::fMandatorySign, kBad },
    { "+10K",  NStr::fMandatorySign, 10*1000 },
    { "-10K",  NStr::fMandatorySign, kBad },
    { "1,000K", 0, kBad },
    { "1,000K", NStr::fAllowCommas, 1000*1000 },
    { "K10K",  0, kBad },
    { "K10K",  NStr::fAllowLeadingSymbols, 10*1000 },
    { "K10K",  NStr::fAllowLeadingSymbols, 10*1000 },
    { "10KG",  NStr::fAllowTrailingSymbols, 10*1000 },
    { "0.123",  0, 0 },
    { "0.567",  0, 1 },
    { "0.123456 K",  0, 123 },
    { "0.123567 K",  0, 124 },
    { "0.123456 KiB",  0, 126 },
    { "0.123567 KiB",  0, 127 },
    { "0.123 MB",  0, 123000 },
    { "0.123 MiB", 0, 128975 },
    { "0.123456 GiB",  0, 132559871 },
    { "123abc",  NStr::fAllowTrailingSymbols, 123 },
    { "123klm",  NStr::fAllowTrailingSymbols, 123000 },
    { "123klm",  NStr::fAllowTrailingSymbols + NStr::fDS_ForceBinary, 125952 },
    { "123kbc",  NStr::fAllowTrailingSymbols, 123000 },
    { "123kic",  NStr::fAllowTrailingSymbols, 123000 },
    { "123kibc", NStr::fAllowTrailingSymbols, 125952 },
    { "123.abc", NStr::fAllowTrailingSymbols, 123 },
    { "123.kic", NStr::fAllowTrailingSymbols, 123 },
    { "123.001 kib", 0, 125953 },
    { "12.3456 pib", 0, NCBI_CONST_UINT8(13899909889916299) },
    { "abc.123.abc",  NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols, 123 },
    { "abc.+123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols, 123 },
    { "abc+.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols, 123 },
    { "abc-.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols, 123 },
    { "abc.-123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols, 123 },
    { "abc.123.abc",  NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign, kBad },
    { "abc.+123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign, 123 },
    { "abc+.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign, kBad },
    { "abc-.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign, kBad },
    { "abc.-123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign, kBad },
    { "abc.123.abc",  NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fDS_ProhibitFractions, 123 },
    { "abc.+123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fDS_ProhibitFractions, 123 },
    { "abc+.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fDS_ProhibitFractions, 123 },
    { "abc-.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fDS_ProhibitFractions, 123 },
    { "abc.-123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fDS_ProhibitFractions, 123 },
    { "abc.123.abc",  NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign + NStr::fDS_ProhibitFractions, kBad },
    { "abc.+123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign + NStr::fDS_ProhibitFractions, 123 },
    { "abc+.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign + NStr::fDS_ProhibitFractions, kBad },
    { "abc-.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign + NStr::fDS_ProhibitFractions, kBad },
    { "abc.-123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign + NStr::fDS_ProhibitFractions, kBad },
    { "123.456", 0, 123 },
    { "123.567", 0, 124 },
    { "123.567",   NStr::fAllowTrailingSymbols + NStr::fDS_ProhibitFractions, 123 },
    { "123.567MB", NStr::fAllowTrailingSymbols + NStr::fDS_ProhibitFractions, 123 },
    { "123.567MB", NStr::fDS_ProhibitFractions, kBad },
    { ".34KB",   0, kBad },
    { ".34KB",   NStr::fAllowLeadingSymbols, 34000 },
    { "k.5.6m",  NStr::fAllowLeadingSymbols, 5600000 },
    { "k.5.6m",  NStr::fAllowLeadingSymbols + NStr::fDS_ProhibitFractions, kBad },
    { "k.5.6m",  NStr::fAllowLeadingSymbols + NStr::fDS_ProhibitFractions
                 + NStr::fAllowTrailingSymbols, 5 },
    { "123,4,56789,0", NStr::fAllowCommas, 1234567890 },
    { "123,4,", NStr::fAllowCommas, kBad },
    { "123,4,", NStr::fAllowCommas + NStr::fAllowTrailingSymbols, 1234 },
    { "123,4,,56", NStr::fAllowCommas, kBad },
    { "123,4,,56", NStr::fAllowCommas + NStr::fAllowTrailingSymbols, 1234 },
    { ",123,4", NStr::fAllowCommas, kBad },
    { ",123,4", NStr::fAllowCommas + NStr::fAllowLeadingSymbols, 1234 },
    { "10", NStr::fDecimalPosix + kNoThrow, kBad },
    { "10", NStr::fDecimalPosixOrLocal + kNoThrow, kBad },
    { "10", NStr::fWithSign + kNoThrow, kBad },
    { "10", NStr::fWithCommas + kNoThrow, kBad },
    { "10", NStr::fDoubleFixed + kNoThrow, kBad },
    { "10", NStr::fDoubleScientific + kNoThrow, kBad },
    { "10", NStr::fDoublePosix + kNoThrow, kBad },
    { "10", NStr::fDS_Binary + kNoThrow, kBad },
    { "10", NStr::fDS_NoDecimalPoint + kNoThrow, kBad },
    { "10", NStr::fDS_PutSpaceBeforeSuffix + kNoThrow, kBad },
    { "10", NStr::fDS_ShortSuffix + kNoThrow, kBad },
    { "10", NStr::fDS_PutBSuffixToo + kNoThrow, kBad }
};

BOOST_AUTO_TEST_CASE(s_StringToNum_DataSize)
{
    const size_t count = sizeof(s_Str2DataSizeTests) /
                         sizeof(s_Str2DataSizeTests[0]);

    for (size_t i = 0;  i < count;  ++i)
    {
        const SStringDataSizeValues* test  = &s_Str2DataSizeTests[i];
        const char*                  str   = test->str;
        NStr::TStringToNumFlags      flags = test->flags;

        try {
            errno = kTestErrno;
            Uint8 value = NStr::StringToUInt8_DataSize(str, flags);
            CHECK_ERRNO;
            BOOST_CHECK(test->IsGood());
            BOOST_CHECK_EQUAL(value, test->expected);
        }
        catch (CException&) {
            BOOST_CHECK(!test->IsGood());
        }
    }
}


//----------------------------------------------------------------------------
// NStr::Uint8ToString_DataSize()
//----------------------------------------------------------------------------

struct SUint8DataSizeValues
{
    Uint8                   num;
    NStr::TNumToStringFlags flags;
    unsigned int            max_digits;
    const char*             expected;

    bool IsGood(void) const {
        return expected != NULL;
    }
};

static const SUint8DataSizeValues s_Num2StrDataSizeTests[] = {
    // num  flags    str
    { 10, 0, 3, "10" },
    { 10, NStr::fDS_PutBSuffixToo, 3, "10B" },
    { 10, NStr::fDS_PutBSuffixToo + NStr::fDS_PutSpaceBeforeSuffix, 3, "10 B" },
    { 10*1000, 0, 3, "10.0KB" },
    { 10*1000, NStr::fDS_ShortSuffix, 3, "10.0K" },
    { 10*1024, NStr::fDS_Binary, 3, "10.0KiB" },
    { 10*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0K" },
    { 10*1000*1000, 0, 3, "10.0MB" },
    { 10*1000*1000, NStr::fDS_ShortSuffix, 3, "10.0M" },
    { 10*1024*1024, NStr::fDS_Binary, 3, "10.0MiB" },
    { 10*1024*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0M" },
    { Uint8(10)*1000*1000*1000, 0, 3, "10.0GB" },
    { Uint8(10)*1000*1000*1000, NStr::fDS_ShortSuffix, 3, "10.0G" },
    { Uint8(10)*1024*1024*1024, NStr::fDS_Binary, 3, "10.0GiB" },
    { Uint8(10)*1024*1024*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0G" },
    { Uint8(10)*1000*1000*1000*1000, 0, 3, "10.0TB" },
    { Uint8(10)*1000*1000*1000*1000, NStr::fDS_ShortSuffix, 3, "10.0T" },
    { Uint8(10)*1024*1024*1024*1024, NStr::fDS_Binary, 3, "10.0TiB" },
    { Uint8(10)*1024*1024*1024*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0T" },
    { Uint8(10)*1000*1000*1000*1000*1000, 0, 3, "10.0PB" },
    { Uint8(10)*1000*1000*1000*1000*1000, NStr::fDS_ShortSuffix, 3, "10.0P" },
    { Uint8(10)*1024*1024*1024*1024*1024, NStr::fDS_Binary, 3, "10.0PiB" },
    { Uint8(10)*1024*1024*1024*1024*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0P" },
    { Uint8(10)*1000*1000*1000*1000*1000*1000, 0, 3, "10.0EB" },
    { Uint8(10)*1000*1000*1000*1000*1000*1000, NStr::fDS_ShortSuffix, 3, "10.0E" },
    { Uint8(10)*1024*1024*1024*1024*1024*1024, NStr::fDS_Binary, 3, "10.0EiB" },
    { Uint8(10)*1024*1024*1024*1024*1024*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0E" },
    { 10*1000, NStr::fWithSign, 3, "+10.0KB" },
    { 10*1000, NStr::fDS_NoDecimalPoint, 3, "10KB" },
    { 10*1000, NStr::fDS_PutSpaceBeforeSuffix, 3, "10.0 KB" },
    { 1000, NStr::fDS_NoDecimalPoint, 4, "1000" },
    { 1000, NStr::fDS_NoDecimalPoint + NStr::fWithCommas, 4, "1,000" },
    { 123456789, 0, 6, "123.457MB" },
    { 3456789, 0, 6, "3.45679MB" },
    { 456789, 0, 6, "456.789KB" },
    { 123456789, 0, 4, "123.5MB" },
    { 123456789, NStr::fDS_NoDecimalPoint, 6, "123457KB" },
    { 3456789, NStr::fDS_NoDecimalPoint, 6, "3457KB" },
    { 456789, NStr::fDS_NoDecimalPoint, 6, "456789" },
    { 123456789, NStr::fDS_NoDecimalPoint, 4, "123MB" },
    { 23456789, NStr::fDS_NoDecimalPoint, 4, "23MB" },
    { 3456789, NStr::fDS_NoDecimalPoint, 4, "3457KB" },
    { 123456789, 0, 1, "123MB" },
    { 123456789, 0, 2, "123MB" },
    { 12345, 0, 1, "12.3KB" },
    { NCBI_CONST_UINT8(13899853594920957), NStr::fDS_Binary, 6, "12.3456PiB" },
    { 1000, NStr::fDS_Binary, 3, "0.98KiB" },
    { 1000, NStr::fDS_Binary, 4, "1000" },
    { 1000, NStr::fDS_Binary, 5, "1000" },
    { 1000, NStr::fDS_Binary, 6, "1000" },
    { 1023, NStr::fDS_Binary, 3, "1.00KiB" },
    { 1023, NStr::fDS_Binary, 4, "1023" },
    { 1023, NStr::fDS_Binary, 5, "1023" },
    { 1023, NStr::fDS_Binary, 6, "1023" },
    { 1024, NStr::fDS_Binary, 3, "1.00KiB" },
    { 1024, NStr::fDS_Binary, 4, "1.000KiB" },
    { 1024, NStr::fDS_Binary, 5, "1.000KiB" },
    { 1024, NStr::fDS_Binary, 6, "1.000KiB" },
    { 99999, NStr::fDS_Binary, 3, "97.7KiB" },
    { 99999, NStr::fDS_Binary, 4, "97.66KiB" },
    { 99999, NStr::fDS_Binary, 5, "97.655KiB" },
    { 99999, NStr::fDS_Binary, 6, "97.655KiB" },
    { 999930, NStr::fDS_Binary, 3, "976KiB" },
    { 999930, NStr::fDS_Binary, 4, "976.5KiB" },
    { 999930, NStr::fDS_Binary, 5, "976.49KiB" },
    { 999930, NStr::fDS_Binary, 6, "976.494KiB" },
    { 1000000, NStr::fDS_Binary, 3, "977KiB" },
    { 1000000, NStr::fDS_Binary, 4, "976.6KiB" },
    { 1000000, NStr::fDS_Binary, 5, "976.56KiB" },
    { 1000000, NStr::fDS_Binary, 6, "976.563KiB" },
    { 1023993, NStr::fDS_Binary, 3, "0.98MiB" },
    { 1023993, NStr::fDS_Binary, 4, "1000KiB" },
    { 1023993, NStr::fDS_Binary, 5, "999.99KiB" },
    { 1023993, NStr::fDS_Binary, 6, "999.993KiB" },
    { 1047552, NStr::fDS_Binary, 3, "1.00MiB" },
    { 1047552, NStr::fDS_Binary, 4, "1023KiB" },
    { 1047552, NStr::fDS_Binary, 5, "1023.0KiB" },
    { 1047552, NStr::fDS_Binary, 6, "1023.00KiB" },
    { 1048064, NStr::fDS_Binary, 3, "1.00MiB" },
    { 1048064, NStr::fDS_Binary, 4, "1.000MiB" },
    { 1048064, NStr::fDS_Binary, 5, "1023.5KiB" },
    { 1048064, NStr::fDS_Binary, 6, "1023.50KiB" },
    { 1048576, NStr::fDS_Binary, 3, "1.00MiB" },
    { 1048576, NStr::fDS_Binary, 4, "1.000MiB" },
    { 1048576, NStr::fDS_Binary, 5, "1.0000MiB" },
    { 1048576, NStr::fDS_Binary, 6, "1.00000MiB" },
    { 1572864, NStr::fDS_Binary, 3, "1.50MiB" },
    { 1572864, NStr::fDS_Binary, 4, "1.500MiB" },
    { 1572864, NStr::fDS_Binary, 5, "1.5000MiB" },
    { 1572864, NStr::fDS_Binary, 6, "1.50000MiB" },
    { 1000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1KiB" },
    { 1000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1000" },
    { 1000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1000" },
    { 1000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1000" },
    { 1023, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1KiB" },
    { 1023, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1023" },
    { 1023, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1023" },
    { 1023, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1023" },
    { 1024, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1KiB" },
    { 1024, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1024" },
    { 1024, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1024" },
    { 1024, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1024" },
    { 99999, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "98KiB" },
    { 99999, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "98KiB" },
    { 99999, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "99999" },
    { 99999, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "99999" },
    { 999930, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "976KiB" },
    { 999930, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "976KiB" },
    { 999930, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "976KiB" },
    { 999930, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "999930" },
    { 1000000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "977KiB" },
    { 1000000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "977KiB" },
    { 1000000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "977KiB" },
    { 1000000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "977KiB" },
    { 1023993, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1MiB" },
    { 1023993, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1000KiB" },
    { 1023993, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1000KiB" },
    { 1023993, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1000KiB" },
    { 1047552, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1MiB" },
    { 1047552, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1023KiB" },
    { 1047552, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1023KiB" },
    { 1047552, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1023KiB" },
    { 1048064, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1MiB" },
    { 1048064, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1024KiB" },
    { 1048064, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1024KiB" },
    { 1048064, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1024KiB" },
    { 1048576, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1MiB" },
    { 1048576, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1024KiB" },
    { 1048576, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1024KiB" },
    { 1048576, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1024KiB" },
    { 1572864, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "2MiB" },
    { 1572864, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1536KiB" },
    { 1572864, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1536KiB" },
    { 1572864, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1536KiB" },
    { 1000, 0, 3, "1.00KB" },
    { 1000, 0, 4, "1.000KB" },
    { 1000, 0, 5, "1.000KB" },
    { 1000, 0, 6, "1.000KB" },
    { 1023, 0, 3, "1.02KB" },
    { 1023, 0, 4, "1.023KB" },
    { 1023, 0, 5, "1.023KB" },
    { 1023, 0, 6, "1.023KB" },
    { 1024, 0, 3, "1.02KB" },
    { 1024, 0, 4, "1.024KB" },
    { 1024, 0, 5, "1.024KB" },
    { 1024, 0, 6, "1.024KB" },
    { 99999, 0, 3, "100KB" },
    { 99999, 0, 4, "100.0KB" },
    { 99999, 0, 5, "99.999KB" },
    { 99999, 0, 6, "99.999KB" },
    { 999930, 0, 3, "1.00MB" },
    { 999930, 0, 4, "999.9KB" },
    { 999930, 0, 5, "999.93KB" },
    { 999930, 0, 6, "999.930KB" },
    { 1000000, 0, 3, "1.00MB" },
    { 1000000, 0, 4, "1.000MB" },
    { 1000000, 0, 5, "1.0000MB" },
    { 1000000, 0, 6, "1.00000MB" },
    { 1023993, 0, 3, "1.02MB" },
    { 1023993, 0, 4, "1.024MB" },
    { 1023993, 0, 5, "1.0240MB" },
    { 1023993, 0, 6, "1.02399MB" },
    { 1047552, 0, 3, "1.05MB" },
    { 1047552, 0, 4, "1.048MB" },
    { 1047552, 0, 5, "1.0476MB" },
    { 1047552, 0, 6, "1.04755MB" },
    { 1048064, 0, 3, "1.05MB" },
    { 1048064, 0, 4, "1.048MB" },
    { 1048064, 0, 5, "1.0481MB" },
    { 1048064, 0, 6, "1.04806MB" },
    { 1048576, 0, 3, "1.05MB" },
    { 1048576, 0, 4, "1.049MB" },
    { 1048576, 0, 5, "1.0486MB" },
    { 1048576, 0, 6, "1.04858MB" },
    { 1572864, 0, 3, "1.57MB" },
    { 1572864, 0, 4, "1.573MB" },
    { 1572864, 0, 5, "1.5729MB" },
    { 1572864, 0, 6, "1.57286MB" },
    { 1000, NStr::fDS_NoDecimalPoint, 3, "1KB" },
    { 1000, NStr::fDS_NoDecimalPoint, 4, "1000" },
    { 1000, NStr::fDS_NoDecimalPoint, 5, "1000" },
    { 1000, NStr::fDS_NoDecimalPoint, 6, "1000" },
    { 1023, NStr::fDS_NoDecimalPoint, 3, "1KB" },
    { 1023, NStr::fDS_NoDecimalPoint, 4, "1023" },
    { 1023, NStr::fDS_NoDecimalPoint, 5, "1023" },
    { 1023, NStr::fDS_NoDecimalPoint, 6, "1023" },
    { 1024, NStr::fDS_NoDecimalPoint, 3, "1KB" },
    { 1024, NStr::fDS_NoDecimalPoint, 4, "1024" },
    { 1024, NStr::fDS_NoDecimalPoint, 5, "1024" },
    { 1024, NStr::fDS_NoDecimalPoint, 6, "1024" },
    { 99999, NStr::fDS_NoDecimalPoint, 3, "100KB" },
    { 99999, NStr::fDS_NoDecimalPoint, 4, "100KB" },
    { 99999, NStr::fDS_NoDecimalPoint, 5, "99999" },
    { 99999, NStr::fDS_NoDecimalPoint, 6, "99999" },
    { 999930, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 999930, NStr::fDS_NoDecimalPoint, 4, "1000KB" },
    { 999930, NStr::fDS_NoDecimalPoint, 5, "1000KB" },
    { 999930, NStr::fDS_NoDecimalPoint, 6, "999930" },
    { 1000000, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 1000000, NStr::fDS_NoDecimalPoint, 4, "1000KB" },
    { 1000000, NStr::fDS_NoDecimalPoint, 5, "1000KB" },
    { 1000000, NStr::fDS_NoDecimalPoint, 6, "1000KB" },
    { 1023993, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 1023993, NStr::fDS_NoDecimalPoint, 4, "1024KB" },
    { 1023993, NStr::fDS_NoDecimalPoint, 5, "1024KB" },
    { 1023993, NStr::fDS_NoDecimalPoint, 6, "1024KB" },
    { 1047552, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 1047552, NStr::fDS_NoDecimalPoint, 4, "1048KB" },
    { 1047552, NStr::fDS_NoDecimalPoint, 5, "1048KB" },
    { 1047552, NStr::fDS_NoDecimalPoint, 6, "1048KB" },
    { 1048064, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 1048064, NStr::fDS_NoDecimalPoint, 4, "1048KB" },
    { 1048064, NStr::fDS_NoDecimalPoint, 5, "1048KB" },
    { 1048064, NStr::fDS_NoDecimalPoint, 6, "1048KB" },
    { 1048576, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 1048576, NStr::fDS_NoDecimalPoint, 4, "1049KB" },
    { 1048576, NStr::fDS_NoDecimalPoint, 5, "1049KB" },
    { 1048576, NStr::fDS_NoDecimalPoint, 6, "1049KB" },
    { 1572864, NStr::fDS_NoDecimalPoint, 3, "2MB" },
    { 1572864, NStr::fDS_NoDecimalPoint, 4, "1573KB" },
    { 1572864, NStr::fDS_NoDecimalPoint, 5, "1573KB" },
    { 1572864, NStr::fDS_NoDecimalPoint, 6, "1573KB" },
    { 2000, NStr::fDS_Binary, 3, "1.95KiB" },
    { 2047, NStr::fDS_Binary, 3, "2.00KiB" },
    { 2000, NStr::fDS_Binary, 6, "1.953KiB" },
    { 2047, NStr::fDS_Binary, 6, "1.999KiB" },
    { 1048575, NStr::fDS_Binary, 6, "1.00000MiB" },
    { 2000000, NStr::fDS_Binary, 6, "1.90735MiB" },
    { 2097146, NStr::fDS_Binary, 6, "1.99999MiB" },
    { 10, NStr::fConvErr_NoThrow, 3, NULL },
    { 10, NStr::fMandatorySign, 3, NULL },
    { 10, NStr::fAllowCommas, 3, NULL },
    { 10, NStr::fAllowLeadingSpaces, 3, NULL },
    { 10, NStr::fAllowLeadingSymbols, 3, NULL },
    { 10, NStr::fAllowTrailingSpaces, 3, NULL },
    { 10, NStr::fAllowTrailingSymbols, 3, NULL },
    { 10, NStr::fDecimalPosix, 3, NULL },
    { 10, NStr::fDecimalPosixOrLocal, 3, NULL },
    { 10, NStr::fDS_ForceBinary, 3, NULL },
    { 10, NStr::fDS_ProhibitFractions, 3, NULL },
    { 10, NStr::fDS_ProhibitSpaceBeforeSuffix, 3, NULL }
};

BOOST_AUTO_TEST_CASE(s_NumToString_DataSize)
{
    const size_t count = sizeof(s_Num2StrDataSizeTests) /
                         sizeof(s_Num2StrDataSizeTests[0]);

    for (size_t i = 0;  i < count;  ++i)
    {
        const SUint8DataSizeValues* test = &s_Num2StrDataSizeTests[i];
        Uint8 num = test->num;
        NStr::TNumToStringFlags flags = test->flags;
        unsigned int max_digits = test->max_digits;

        try {
            errno = kTestErrno;
            string value = NStr::UInt8ToString_DataSize(num, flags, max_digits);
            CHECK_ERRNO;
            BOOST_CHECK(test->IsGood());
            BOOST_CHECK_EQUAL(value, test->expected);
        }
        catch (CException&) {
            BOOST_CHECK(!test->IsGood());
        }
    }
}


//----------------------------------------------------------------------------
// NStr::BoolToString()
//----------------------------------------------------------------------------

struct SStringBoolValues
{
    const char* str;        // String input
    bool        is_good;    // String input is correct
    bool        expected;   // Expected value 
};

static const SStringBoolValues s_StrToBoolTests[] = {
    { "true",   true,   true  },
    { "false",  true,   false },
    { "t",      true,   true  },
    { "f",      true,   false },
    { "yes",    true,   true  },
    { "no",     true,   false },
    { "y",      true,   true  },
    { "n",      true,   false },
    { "1",      true,   true  },
    { "0",      true,   false },
    { "truee",  false,  false },
    { "10",     false,  false },
    { "00",     false,  false }
};

BOOST_AUTO_TEST_CASE(s_BoolToString)
{
    // BoolToString()

    BOOST_CHECK_EQUAL(NStr::BoolToString(true), string("true"));
    BOOST_CHECK_EQUAL(NStr::BoolToString(false), string("false"));

    // StringToBool()

    const size_t count = sizeof(s_StrToBoolTests) / 
                         sizeof(s_StrToBoolTests[0]);
    for (size_t i = 0;  i < count;  ++i)
    {
        const SStringBoolValues* test = &s_StrToBoolTests[i];
        try {
            errno = kTestErrno;
            bool value = NStr::StringToBool(test->str);
            BOOST_CHECK(errno != kTestErrno);
            BOOST_CHECK(test->is_good);
            BOOST_CHECK_EQUAL(value, test->expected);
        }
        catch (CException&) {
            BOOST_CHECK(!test->is_good);
        }
    }
}


//----------------------------------------------------------------------------
// NStr::PtrToString()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_PtrToString)
{
    string s;
    {{
        errno = kTestErrno;
        NStr::PtrToString(s, &s);
        BOOST_CHECK(!s.empty());
        BOOST_CHECK(errno != kTestErrno);

        errno = kTestErrno;
        const void* ptr = NStr::StringToPtr(s);
        BOOST_CHECK(errno != kTestErrno);
        BOOST_CHECK_EQUAL(ptr, &s);
    }}
    {{
        #if SIZEOF_VOIDP == 8
            const void* ptr_val = (void*)0x01234d00002fe008;
            #if defined(NCBI_OS_MSWIN)
                const char* ptr_str = "01234D00002FE008";
            #elif defined(NCBI_OS_SOLARIS)
                const char* ptr_str = "1234d00002fe008";
            #else
                const char* ptr_str = "0x1234d00002fe008";
            #endif
        #else
            const void* ptr_val = (void*)0xD02fe008;
            #if defined(NCBI_OS_MSWIN)
                const char* ptr_str = "D02FE008";
            #elif defined(NCBI_OS_SOLARIS)
                const char* ptr_str = "d02fe008";
            #else
                const char* ptr_str = "0xd02fe008";
            #endif
        #endif

        errno = kTestErrno;
        s = NStr::PtrToString(ptr_val);
        BOOST_CHECK(errno != kTestErrno);
        BOOST_CHECK_EQUAL(s, string(ptr_str));

        errno = kTestErrno;
        const void* ptr1 = NStr::StringToPtr(s);
        BOOST_CHECK(errno != kTestErrno);
        BOOST_CHECK_EQUAL(ptr1, ptr_val);

        errno = kTestErrno;
        const void* ptr2 = NStr::StringToPtr(CTempString(ptr_str));
        BOOST_CHECK(errno != kTestErrno);
        BOOST_CHECK_EQUAL(ptr2, ptr_val);
    }}
    {{
        const void* ptr;

        errno = kTestErrno;
        ptr = NStr::StringToPtr("0");
        BOOST_CHECK(errno == 0);
        BOOST_CHECK_EQUAL(ptr, (void*)0);

        errno = kTestErrno;
        ptr = NStr::StringToPtr("q");
        BOOST_CHECK(errno != kTestErrno  &&  errno > 0);
        BOOST_CHECK_EQUAL(ptr, (void*)0);
    }}
}


//----------------------------------------------------------------------------
// CommonPrefixSize / CommonSuffixSize / CommonOverlapSize
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_CommonSize)
{
    BOOST_CHECK_EQUAL( NStr::CommonPrefixSize("", ""),            0U );
    BOOST_CHECK_EQUAL( NStr::CommonPrefixSize("123", ""),         0U );
    BOOST_CHECK_EQUAL( NStr::CommonPrefixSize("123", "123"),      3U );
    BOOST_CHECK_EQUAL( NStr::CommonPrefixSize("123", "12345"),    3U );
    BOOST_CHECK_EQUAL( NStr::CommonPrefixSize("12367", "12345"),  3U );
    BOOST_CHECK_EQUAL( NStr::CommonPrefixSize("123", "456"),      0U );

    BOOST_CHECK_EQUAL( NStr::CommonSuffixSize("", ""),            0U );
    BOOST_CHECK_EQUAL( NStr::CommonSuffixSize("123", ""),         0U );
    BOOST_CHECK_EQUAL( NStr::CommonSuffixSize("123", "123"),      3U );
    BOOST_CHECK_EQUAL( NStr::CommonSuffixSize("123", "12345"),    0U );
    BOOST_CHECK_EQUAL( NStr::CommonSuffixSize("12345", "345"),    3U );
    BOOST_CHECK_EQUAL( NStr::CommonSuffixSize("12345", "145"),    2U );
    BOOST_CHECK_EQUAL( NStr::CommonPrefixSize("123", "456"),      0U );

    BOOST_CHECK_EQUAL( NStr::CommonOverlapSize("", ""),           0U );
    BOOST_CHECK_EQUAL( NStr::CommonOverlapSize("", "123"),        0U );
    BOOST_CHECK_EQUAL( NStr::CommonOverlapSize("123", ""),        0U );
    BOOST_CHECK_EQUAL( NStr::CommonOverlapSize("123", "123"),     3U );
    BOOST_CHECK_EQUAL( NStr::CommonOverlapSize("123", "456"),     0U );
    BOOST_CHECK_EQUAL( NStr::CommonOverlapSize("123", "1234"),    3U );
    BOOST_CHECK_EQUAL( NStr::CommonOverlapSize("1234", "123"),    0U );
    BOOST_CHECK_EQUAL( NStr::CommonOverlapSize("1234", "234561"), 3U );
    BOOST_CHECK_EQUAL( NStr::CommonOverlapSize("234561", "1234"), 1U );
}


//----------------------------------------------------------------------------
// NStr::Replace()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_Replace)
{
    {{
        string src("aaabbbaaccczzcccXX");
        string dst;

        // Replace()

        string search("ccc");
        string replace("RrR");
        NStr::Replace(src, search, replace, dst);
        BOOST_CHECK_EQUAL(dst, string("aaabbbaaRrRzzRrRXX"));

        search  = "a";
        replace = "W";
        NStr::Replace(src, search, replace, dst, 6, 1);
        BOOST_CHECK_EQUAL(dst, string("aaabbbWaccczzcccXX"));

        search  = "bbb";
        replace = "BBB";
        NStr::Replace(src, search, replace, dst, 50);
        BOOST_CHECK_EQUAL(dst, string("aaabbbaaccczzcccXX"));

        search  = "ggg";
        replace = "no";
        dst = NStr::Replace(src, search, replace);
        BOOST_CHECK_EQUAL(dst, string("aaabbbaaccczzcccXX"));

        search  = "a";
        replace = "A";
        dst = NStr::Replace(src, search, replace);
        BOOST_CHECK_EQUAL(dst, string("AAAbbbAAccczzcccXX"));

        search  = "X";
        replace = "x";
        dst = NStr::Replace(src, search, replace, src.size() - 1);
        BOOST_CHECK_EQUAL(dst, string("aaabbbaaccczzcccXx"));

        search  = "a";
        replace = "ab";
        dst = NStr::Replace(src, search, replace);
        BOOST_CHECK_EQUAL(dst, string("abababbbbababccczzcccXX"));

        search  = "ab";
        replace = "a";
        dst = NStr::Replace(src, search, replace);
        BOOST_CHECK_EQUAL(dst, string("aaabbaaccczzcccXX"));


        // ReplaceInPlace()

        search  = "a";
        replace = "W";
        NStr::ReplaceInPlace(src, search, replace);
        BOOST_CHECK_EQUAL(src, string("WWWbbbWWccczzcccXX"));

        search = "W";
        replace = "a";
        NStr::ReplaceInPlace(src, search, replace, 2, 2);
        BOOST_CHECK_EQUAL(src, string("WWabbbaWccczzcccXX"));

        search = "a";
        replace = "bb";
        NStr::ReplaceInPlace(src, search, replace);
        BOOST_CHECK_EQUAL(src, string("WWbbbbbbbWccczzcccXX"));

        search = "bb";
        replace = "c";
        NStr::ReplaceInPlace(src, search, replace);
        BOOST_CHECK_EQUAL(src, string("WWcccbWccczzcccXX"));

        search = "c";
        replace = "cb";
        NStr::ReplaceInPlace(src, search, replace);
        BOOST_CHECK_EQUAL(src, string("WWcbcbcbbWcbcbcbzzcbcbcbXX"));

        search = "cb";
        replace = "c";
        NStr::ReplaceInPlace(src, search, replace);
        BOOST_CHECK_EQUAL(src, string("WWcccbWccczzcccXX"));
    }}

    // "number of replacements occurred" test
    {{
        string src("aaabbbaaccczzcccXX");
        size_t n;
        string tmp;

        NStr::Replace(src, "a", "bb", tmp, 0, 0, &n);
        BOOST_CHECK(n == 5);
        NStr::Replace(src, "aa", "bb", tmp, 0, 0, &n);
        BOOST_CHECK(n == 2);
        NStr::Replace(src, "aaa", "b", tmp, 0, 0, &n);
        BOOST_CHECK(n == 1);

        tmp = src;
        NStr::ReplaceInPlace(tmp, "a", "bb", 0, 0, &n);
        BOOST_CHECK(n == 5);
        tmp = src;
        NStr::ReplaceInPlace(tmp, "aa", "bb", 0, 0, &n);
        BOOST_CHECK(n == 2);
        tmp = src;
        NStr::ReplaceInPlace(tmp, "aaa", "b", 0, 0, &n);
        BOOST_CHECK(n == 1);
    }}

    // Replace() for big strings (CXX-3871)
    {{
        const unsigned int kStringSize = 50*1024;
        AutoArray<char> buf(kStringSize);
        char* src_buf = buf.get();
        BOOST_CHECK(src_buf);
        assert(src_buf);

        CNcbiTest::SetRandomSeed("Replace");
        for (size_t i = 0; i < kStringSize; i++) {
            src_buf[i] = (rand() % 2 == 0) ? 'A' : 'B';
        }
        string src_str(src_buf, kStringSize);
        string dst_str;
        string cmp_str;
        size_t n1, n2;

        NStr::Replace(src_str, "A", "CCC", dst_str, 0, 0, &n1);
        NStr::Replace(dst_str, "CCC", "A", cmp_str, 0, 0, &n2);
        BOOST_CHECK_EQUAL(src_str, cmp_str);
        BOOST_CHECK_EQUAL(n1, n2);

        NStr::Replace(src_str, "A", "DD", dst_str, 0, 1, &n1);
        NStr::Replace(dst_str, "DD", "A", cmp_str, 0, 0, &n2);
        BOOST_CHECK_EQUAL(src_str, cmp_str);
        BOOST_CHECK(n1 == 1  &&  n2 == 1);

        NStr::Replace(src_str, "A", "EEEE", dst_str, 1000, 5, &n1);
        NStr::Replace(dst_str, "EEEE", "A", cmp_str, 0, 0, &n2);
        BOOST_CHECK_EQUAL(src_str, cmp_str);
        BOOST_CHECK(n1 <= 5);
        BOOST_CHECK_EQUAL(n1, n2);

        NStr::Replace(src_str, "A", "FFFFF", dst_str, 1000, kStringSize, &n1);
        NStr::Replace(dst_str, "FFFFF", "A", cmp_str, 0, 0, &n2);
        BOOST_CHECK_EQUAL(src_str, cmp_str);
        BOOST_CHECK_EQUAL(n1, n2);
    }}
}


//----------------------------------------------------------------------------
// NStr::PrintableString/ParseEscapes()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_PrintableString)
{
    // NStr::PrintableString()

    BOOST_CHECK(NStr::PrintableString(kEmptyStr).empty());
    BOOST_CHECK(NStr::PrintableString
                ("?AA\?\?AA?AA\?\?\?AA\?\?AA?").compare
                ("\?AA\\?\\?AA\?AA\\?\\?\\?AA\\?\\?AA\?") == 0);
    BOOST_CHECK(NStr::PrintableString
                ("AB\\CD\nAB\rCD\vAB?\tCD\'AB\"").compare
                ("AB\\\\CD\\nAB\\rCD\\vAB\?\\tCD\\\'AB\\\"") == 0);
    BOOST_CHECK(NStr::PrintableString
                ("A\x01\r\177\x000F\0205B" + string(1,'\0') + "CD").compare
                ("A\\1\\r\\177\\17\\0205B\\0CD") == 0);
    BOOST_CHECK(NStr::PrintableString
                ("A\x01\r\xC1\xF7\x07\x3A\252\336\202\x000F\0205B" + string(1,'\0') + "CD",
                 NStr::fNonAscii_Quote | NStr::fPrintable_Full).compare
                ("A\\001\\r\\301\\367\\a:\\252\\336\\202\\017\\0205B\\000CD") == 0);
    BOOST_CHECK(NStr::PrintableString
                ("A\nB\\\nC").compare
                ("A\\nB\\\\\\nC") == 0);
    BOOST_CHECK(NStr::PrintableString
                ("A\nB\\\nC", NStr::fNewLine_Passthru).compare
                ("A\\n\\\nB\\\\\\n\\\nC") == 0);

    // NStr::ParseEscapes

    BOOST_CHECK(NStr::ParseEscapes(kEmptyStr).empty());
    BOOST_CHECK(NStr::ParseEscapes
                ("AB\\\\CD\\nAB\\rCD\\vAB\?\\tCD\\\'AB\\\"").compare
                ("AB\\CD\nAB\rCD\vAB?\tCD\'AB\"") == 0);
    BOOST_CHECK(NStr::ParseEscapes
                ("A\\1\\r2\\17\\0205B\\x00CD\\x000F").compare
                ("A\x01\r2\x0F\x10""5B\xCD\x0F") == 0);
    BOOST_CHECK(NStr::ParseEscapes
                ("A\\\nB\\nC\\\\\\n\\\nD").compare
                ("AB\nC\\\nD") == 0);

    // NStr::ParseEscapes() with out or range sequences
    {{
        string s;
        // eEscSeqRange_LastByte
        BOOST_CHECK_EQUAL(NStr::ParseEscapes("\\x4547", NStr::eEscSeqRange_Standard), "\x47");
    
        // eEscSeqRange_FirstByte
        BOOST_CHECK_EQUAL(NStr::ParseEscapes("\\x4547", NStr::eEscSeqRange_FirstByte), "E47");
    
        // eEscSeqRange_Errno
        errno = kTestErrno;
        s = NStr::ParseEscapes("\\x45", NStr::eEscSeqRange_Errno);
        BOOST_CHECK(errno == 0);
        BOOST_CHECK_EQUAL(s, "\x45");
        errno = kTestErrno;
        s = NStr::ParseEscapes("\\x4547", NStr::eEscSeqRange_Errno);
        BOOST_CHECK(errno == ERANGE);
        BOOST_CHECK(s.empty());

        // eEscSeqRange_Throw
        try {
            s = NStr::ParseEscapes("\\x4547", NStr::eEscSeqRange_Throw);
            _TROUBLE;
        }
        catch (CStringException&) {}

        // eEscSeqRange_User
        BOOST_CHECK_EQUAL(NStr::ParseEscapes("\\x4547", NStr::eEscSeqRange_User, '?'), "?");
    }}


    // NStr::PrintableString() vs. Printable()
    CNcbiTest::SetRandomSeed("ParseEscapes");
    size_t size = (size_t)(rand() & 0xFFFF);
    string data;
    for (size_t i = 0;  i < size;  i++) {
        data += char(rand() & 0xFF);
    }
    BOOST_CHECK(data.size() == size);

    // NB: embedded '\0's are also being checked
    CNcbiOstrstream os1;
    os1 << Printable(data);
    const string& s1 = NStr::PrintableString(data);
    BOOST_CHECK(s1.compare(CNcbiOstrstreamToString(os1)) == 0);

    // Just to make sure it still parses okay
    BOOST_CHECK(NStr::ParseEscapes(s1).compare(data) == 0);

    // NB: checks C string (no '\0's)
    const char* str = data.c_str();
    CNcbiOstrstream os2;
    os2 << Printable(str);
    const string& s2 = NStr::PrintableString(str);
    BOOST_CHECK(s2.compare(CNcbiOstrstreamToString(os2)) == 0);
}


//----------------------------------------------------------------------------
// NStr::Escape()
//----------------------------------------------------------------------------

struct SEscapeTest {
    const char*  str;
    const char*  meta;
    char         esc;
    const char*  result;
};

static const SEscapeTest s_EscapeTests[] = {
    { "",           "",    '\\',  "" },
    { "ABC",        "#",   '\\',  "ABC" },
    { "#",          "#",   '\\',  "\\#" },
    { "###",        "#",   '\\',  "\\#\\#\\#" },
    { "##A",        "#",   '\\',  "\\#\\#A" },
    { "A##",        "#",   '\\',  "A\\#\\#" },
    { "A#B##C###D", "#",   '\\',  "A\\#B\\#\\#C\\#\\#\\#D" },
    { "A#B##C###D", "#",   ' ',   "A #B # #C # # #D" },
    { "A*B=#C#=*D", "#*=", '\\',  "A\\*B\\=\\#C\\#\\=\\*D" },
    { "*",          "#",   '*',   "**" },
    { "*A**",       "#",   '*',   "**A****" },
    { "**A***B*",   "#",   '*',   "****A******B**" },
    { "********",   "#",   '*',   "****************" }
};

BOOST_AUTO_TEST_CASE(s_Escape)
{
    const size_t count = sizeof(s_EscapeTests) / sizeof(s_EscapeTests[0]);
    for (size_t i = 0;  i < count;  ++i)
    {
        const SEscapeTest* test = &s_EscapeTests[i];
        string s = NStr::Escape(test->str, test->meta, test->esc);
        BOOST_CHECK(s.compare(test->result) == 0);
        BOOST_CHECK(NStr::Unescape(s, test->esc).compare(test->str) == 0);
    }
}


//----------------------------------------------------------------------------
// NStr::Quote()
//----------------------------------------------------------------------------

struct SQuoteTest {
    const char* str;
    char        quote;
    char        esc;  
    const char* result;
};

static const SQuoteTest s_QuoteTests[] = {

    // single quoting

    { "ABC",        '"',   '\\',   "\"ABC\""          },
    { "'ABC'",      '"',   '\\',   "\"'ABC'\""        },
    { "\"ABC\"",    '"',   '\\',   "\"\\\"ABC\\\"\""  },
    { "-ABC-",      '"',   '\\',   "\"-ABC-\""        },
    { "A'B'C",      '"',   '\\',   "\"A'B'C\""        },
    { "A\"B\"C",    '"',   '\\',   "\"A\\\"B\\\"C\""  },
    { "A-B-C",      '"',   '\\',   "\"A-B-C\""        },
    { "A\\B\\C",    '"',   '\\',   "\"A\\\\B\\\\C\""  },

    { "ABC",        '\'',  '\\',   "'ABC'"            },
    { "'ABC'",      '\'',  '\\',   "'\\'ABC\\''"      },
    { "\"ABC\"",    '\'',  '\\',   "'\"ABC\"'"        },
    { "-ABC-",      '\'',  '\\',   "'-ABC-'"          },
    { "A'B'C",      '\'',  '\\',   "'A\\'B\\'C'"      },
    { "A\"B\"C",    '\'',  '\\',   "'A\"B\"C'"        },
    { "A-B-C",      '\'',  '\\',   "'A-B-C'"          },
    { "A\\B\\C",    '\'',  '\\',   "'A\\\\B\\\\C'"    },

    { "ABC",        '-',   '\\',   "-ABC-"            },
    { "'ABC'",      '-',   '\\',   "-'ABC'-"          },
    { "\"ABC\"",    '-',   '\\',   "-\"ABC\"-"        },
    { "-ABC-",      '-',   '\\',   "-\\-ABC\\--"      },
    { "A'B'C",      '-',   '\\',   "-A'B'C-"          },
    { "A\"B\"C",    '-',   '\\',   "-A\"B\"C-"        },
    { "A-B-C",      '-',   '\\',   "-A\\-B\\-C-"      },
    { "A\\B\\C",    '-',   '\\',   "-A\\\\B\\\\C-"    },

    // double quoting

    { "ABC",        '"',   '"',    "\"ABC\""          },
    { "'ABC'",      '"',   '"',    "\"'ABC'\""        },
    { "\"ABC\"",    '"',   '"',    "\"\"\"ABC\"\"\""  },
    { "-ABC-",      '"',   '"',    "\"-ABC-\""        },
    { "A'B'C",      '"',   '"',    "\"A'B'C\""        },
    { "A\"B\"C",    '"',   '"',    "\"A\"\"B\"\"C\""  },
    { "A-B-C",      '"',   '"',    "\"A-B-C\""        },
    { "A\\B\\C",    '"',   '"',    "\"A\\B\\C\""      },

    { "ABC",        '\'',  '\'',   "'ABC'"            },
    { "'ABC'",      '\'',  '\'',   "'''ABC'''"        },
    { "\"ABC\"",    '\'',  '\'',   "'\"ABC\"'"        },
    { "-ABC-",      '\'',  '\'',   "'-ABC-'"          },
    { "A'B'C",      '\'',  '\'',   "'A''B''C'"        },
    { "A\"B\"C",    '\'',  '\'',   "'A\"B\"C'"        },
    { "A-B-C",      '\'',  '\'',   "'A-B-C'"          },
    { "A\\B\\C",    '\'',  '\'',   "'A\\B\\C'"        },

    { "ABC",        '-',   '-',    "-ABC-"            },
    { "'ABC'",      '-',   '-',    "-'ABC'-"          },
    { "\"ABC\"",    '-',   '-',    "-\"ABC\"-"        },
    { "-ABC-",      '-',   '-',    "---ABC---"        },
    { "A'B'C",      '-',   '-',    "-A'B'C-"          },
    { "A\"B\"C",    '-',   '-',    "-A\"B\"C-"        },
    { "A-B-C",      '-',   '-',    "-A--B--C-"        },
    { "A\\B\\C",    '-',   '-',   "-A\\B\\C-"         }
};

BOOST_AUTO_TEST_CASE(s_Quote)
{
    const size_t count = sizeof(s_QuoteTests) / sizeof(s_QuoteTests[0]);
    for (size_t i = 0; i < count; ++i)
    {
        const SQuoteTest* test = &s_QuoteTests[i];
        BOOST_CHECK(NStr::Quote(test->str, test->quote, test->esc).compare(test->result) == 0);
        BOOST_CHECK(NStr::Unquote(test->result, test->esc).compare(test->str) == 0);
    }
    // Matrix test
    {
        for (unsigned i1 = 1; i1 < 256; i1++) {
            for (unsigned i2 = 0; i2 < 256; i2++) {
                char s[3];
                s[0] = (char)i1;
                s[1] = (char)i2;
                s[2] = '\0';
                string sq = NStr::Quote(s);
                string su = NStr::Unquote(sq);
                BOOST_CHECK_EQUAL(su, CTempString(s));
            }
        }
    }
}


//----------------------------------------------------------------------------
// NStr::Sanitize()
//----------------------------------------------------------------------------

struct SSanitizeTest {
    const char*     str;
    NStr::TSS_Flags flags;
    const char*     allow; 
    const char*     reject;
    char            replacement;
    // Results for simplified version, that don't use (allow + reject + replacement) fields
    const char*     res_allowed;       // flags
    const char*     res_rejected;      // flags + fSS_Reject
    // Results for extended version
    const char*     res_ex_allowed;    // flags
    const char*     res_ex_rejected;   // flags + fSS_Reject
};

// Abbreviations to shorten tests description
const NStr::TSS_Flags A   = NStr::fSS_alpha;
const NStr::TSS_Flags D   = NStr::fSS_digit;
const NStr::TSS_Flags AD  = NStr::fSS_alnum;
const NStr::TSS_Flags PR  = NStr::fSS_print;
const NStr::TSS_Flags C   = NStr::fSS_cntrl;
const NStr::TSS_Flags I   = NStr::fSS_punct;
const NStr::TSS_Flags RM  = NStr::fSS_Remove;
const NStr::TSS_Flags NM  = NStr::fSS_NoMerge;
const NStr::TSS_Flags NT  = NStr::fSS_NoTruncate;
const NStr::TSS_Flags NTB = NStr::fSS_NoTruncate_Begin;
const NStr::TSS_Flags NTE = NStr::fSS_NoTruncate_End;

static const SSanitizeTest s_SanitizeTests[] = {

    // Default flags

    { "",                 0,   "",  "",  '?',  ""         , ""               , ""            , ""               }, // 0
    { "   ",              0,   "",  "",  '?',  ""         , ""               , ""            , "?"              },
    { "ABC",              0,   "",  "",  '?',  "ABC"      , ""               , "ABC"         , "?"              },
    { "  ABC",            0,   "",  "",  '?',  "ABC"      , ""               , "ABC"         , "?"              },
    { "ABC  ",            0,   "",  "",  '?',  "ABC"      , ""               , "ABC"         , "?"              },
    { " ABC ",            0,   "",  "",  '?',  "ABC"      , ""               , "ABC"         , "?"              },
    { "   ABC   ",        0,   "",  "",  '?',  "ABC"      , ""               , "ABC"         , "?"              },
    { "   A B C   ",      0,   "",  "",  '?',  "A B C"    , ""               , "A B C"       , "?"              },
    { "   A  B  C   ",    0,   "",  "",  '?',  "A B C"    , ""               , "A B C"       , "?"              },
    { "   AB CD EF   ",   0,   "",  "",  '?',  "AB CD EF" , ""               , "AB CD EF"    , "?"              },
    { "   AB  CD  EF   ", 0,   "",  "",  '?',  "AB CD EF" , ""               , "AB CD EF"    , "?"              }, // 10
    { "\nA\tB""\x01 ""C", 0,   "",  "",  '?',  "A B C"    , "\n \t \x01"     , "?A?B? C"     , "\n?\t?\x01?"    },
    { "\n \nA B\nC\n \n", 0,   "",  "",  '?',  "A B C"    , "\n \n \n \n \n" , "? ?A B?C? ?" , "\n?\n?\n?\n?\n" },
    { "\n\nA B\nC\n\n",   0,   "",  "",  '?',  "A B C"    , "\n\n \n \n\n"   , "?A B?C?"     , "\n\n?\n?\n\n"   },
    { "  \nA B\nC\n  ",   0,   "",  "",  '?',  "A B C"    , "\n \n \n"       , "?A B?C?"     , "?\n?\n?\n?"     },

    // Default flags (use custom filters only, so no character classes)
    // Affects extended Sanitize() only, simplified results are same as before.

    { "",                 0,   "A\t",  "BE",  '?',  ""         , ""               , ""                , ""      }, // 15
    { "   ",              0,   "A\t",  "BE",  '?',  ""         , ""               , ""                , "?"     },
    { "   ",              0,   "",     " ",   '?',  ""         , ""               , "?"               , "?"     },
    { "   ",              0,   " ",    "",    '?',  ""         , ""               , ""                , ""      },
    { "   ",              0,   " ",    " ",   '?',  ""         , ""               , "?"               , "?"     },
    { "ABC",              0,   "A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"             , "A?"    }, // 20
    { "  ABC",            0,   "A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"             , "?A?"   },
    { "ABC  ",            0,   "A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"             , "A?"    },
    { " ABC ",            0,   "A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"             , "?A?"   },
    { "   ABC   ",        0,   "A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"             , "?A?"   },
    { "   A B C   ",      0,   "A\t",  "BE",  '?',  "A B C"    , ""               , "A ? C"           , "?A?"   },
    { "   A  B  C   ",    0,   "A\t",  "BE",  '?',  "A B C"    , ""               , "A ? C"           , "?A?"   },
    { "   AB CD EF   ",   0,   "A\t",  "BE",  '?',  "AB CD EF" , ""               , "A? CD ?F"        , "?A?"   },
    { "   AB  CD  EF   ", 0,   "A\t",  "BE",  '?',  "AB CD EF" , ""               , "A? CD ?F"        , "?A?"   },
    { "\nA\tB""\x01 ""C", 0,   "A\t",  "BE",  '?',  "A B C"    , "\n \t \x01"     , "\nA\t?\x01 C"    , "?A\t?" },
    { "\n \nA B\nC\n \n", 0,   "A\t",  "BE",  '?',  "A B C"    , "\n \n \n \n \n" , "\n \nA ?\nC\n \n", "?A?"   }, // 30
    { "\n\nA B\nC\n\n",   0,   "A\t",  "BE",  '?',  "A B C"    , "\n\n \n \n\n"   , "\n\nA ?\nC\n\n"  , "?A?"   },
    { "  \nA B\nC\n  ",   0,   "A\t",  "BE",  '?',  "A B C"    , "\n \n \n"       , "\nA ?\nC\n"      , "?A?"   },

    // Default flags (use custom filters + fSS_Print)
    // Affects extended Sanitize() only, simplified results are same as before.

    { "",                 PR,   "A\t",  "BE",  '?',  ""         , ""               , ""           , ""                }, // 33
    { "   ",              PR,   "A\t",  "BE",  '?',  ""         , ""               , ""           , "?"               },
    { "   ",              PR,   " ",    "",    '?',  ""         , ""               , ""           , ""                },
    { "   ",              PR,   "",     " ",   '?',  ""         , ""               , "?"          , "?"               },
    { "   ",              PR,   " ",    " ",   '?',  ""         , ""               , "?"          , "?"               },
    { "ABC",              PR,   "A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"        , "A?"              },
    { "  ABC",            PR,   "A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"        , "?A?"             },
    { "ABC  ",            PR,   "A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"        , "A?"              }, // 40
    { " ABC ",            PR,   "A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"        , "?A?"             },
    { "   ABC   ",        PR,   "A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"        , "?A?"             },
    { "   A B C   ",      PR,   "A\t",  "BE",  '?',  "A B C"    , ""               , "A ? C"      , "?A?"             },
    { "   A  B  C   ",    PR,   "A\t",  "BE",  '?',  "A B C"    , ""               , "A ? C"      , "?A?"             },
    { "   AB CD EF   ",   PR,   "A\t",  "BE",  '?',  "AB CD EF" , ""               , "A? CD ?F"   , "?A?"             },
    { "   AB  CD  EF   ", PR,   "A\t",  "BE",  '?',  "AB CD EF" , ""               , "A? CD ?F"   , "?A?"             },
    { "\nA\tB""\x01 ""C", PR,   "A\t",  "BE",  '?',  "A B C"    , "\n \t \x01"     , "?A\t? C"    , "\nA\t?\x01?"     },
    { "\n \nA B\nC\n \n", PR,   "A\t",  "BE",  '?',  "A B C"    , "\n \n \n \n \n" , "? ?A ?C? ?" , "\n?\nA?\n?\n?\n" },
    { "\n\nA B\nC\n\n",   PR,   "A\t",  "BE",  '?',  "A B C"    , "\n\n \n \n\n"   , "?A ?C?"     , "\n\nA?\n?\n\n"   },
    { "  \nA B\nC\n  ",   PR,   "A\t",  "BE",  '?',  "A B C"    , "\n \n \n"       , "?A ?C?"     , "?\nA?\n?\n?"     }, // 50

    // same as before + allowed space

    { "",                 PR,   " A\t",  "BE",  '?',  ""         , ""               , ""           , ""                 }, // 51
    { "   ",              PR,   " A\t",  "BE",  '?',  ""         , ""               , ""           , ""                 },
    { "   ",              PR,   " ",     "",    '?',  ""         , ""               , ""           , ""                 },
    { "   ",              PR,   "",      " ",   '?',  ""         , ""               , "?"          , "?"                },
    { "   ",              PR,   " ",     " ",   '?',  ""         , ""               , "?"          , "?"                },
    { "ABC",              PR,   " A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"        , "A?"               },
    { "  ABC",            PR,   " A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"        , "A?"               },
    { "ABC  ",            PR,   " A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"        , "A?"               },
    { " ABC ",            PR,   " A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"        , "A?"               },
    { "   ABC   ",        PR,   " A\t",  "BE",  '?',  "ABC"      , ""               , "A?C"        , "A?"               }, // 60
    { "   A B C   ",      PR,   " A\t",  "BE",  '?',  "A B C"    , ""               , "A ? C"      , "A ? ?"            },
    { "   A  B  C   ",    PR,   " A\t",  "BE",  '?',  "A B C"    , ""               , "A ? C"      , "A ? ?"            },
    { "   AB CD EF   ",   PR,   " A\t",  "BE",  '?',  "AB CD EF" , ""               , "A? CD ?F"   , "A? ? ?"           },
    { "   AB  CD  EF   ", PR,   " A\t",  "BE",  '?',  "AB CD EF" , ""               , "A? CD ?F"   , "A? ? ?"           },
    { "\nA\tB""\x01 ""C", PR,   " A\t",  "BE",  '?',  "A B C"    , "\n \t \x01"     , "?A\t? C"    , "\nA\t?\x01 ?"     },
    { "\n \nA B\nC\n \n", PR,   " A\t",  "BE",  '?',  "A B C"    , "\n \n \n \n \n" , "? ?A ?C? ?" , "\n \nA ?\n?\n \n" },
    { "\n\nA B\nC\n\n",   PR,   " A\t",  "BE",  '?',  "A B C"    , "\n\n \n \n\n"   , "?A ?C?"     , "\n\nA ?\n?\n\n"   },
    { "  \nA B\nC\n  ",   PR,   " A\t",  "BE",  '?',  "A B C"    , "\n \n \n"       , "?A ?C?"     , "\nA ?\n?\n"       },

    // Character classes filters

    { "A123,BC45\nD6.",   0,    "A\t",   "BE",  '?',  "A123,BC45 D6."  , "\n"            , "A123,?C45\nD6." , "A?"            },
    { "A123,BC45\nD6.",   PR,   "A\t",   "BE",  '?',  "A123,BC45 D6."  , "\n"            , "A123,?C45?D6."  , "A?\n?"         }, // 70
    { "A123,BC45\nD6.",   A,    "A\t",   "BE",  '?',  "A BC D"         , "123, 45\n 6."  , "A?C?D?"         , "A123,?45\n?6." },
    { "A123,BC45\nD6.",   D,    "A\t",   "BE",  '?',  "123 45 6"       , "A ,BC \nD ."   , "A123?45?6?"     , "A?,?C?\nD?."   },
    { "A123,BC45\nD6.",   AD,   "A\t",   "BE",  '?',  "A123 BC45 D6"   , ", \n ."        , "A123?C45?D6?"   , "A?,?\n?."      },
    { "A123,BC45\nD6.",   A+D,  "A\t",   "BE",  '?',  "A123 BC45 D6"   , ", \n ."        , "A123?C45?D6?"   , "A?,?\n?."      },
    { "A123,BC45\nD6.",   I,    "A\t",   "BE",  '?',  ", ."            , "A123 BC45\nD6" , "A?,?."          , "A123?C45\nD6?" },
    { "A123,BC45\nD6.",   C,    "A\t",   "BE",  '?',  "\n"             , "A123,BC45 D6." , "A?\n?"          , "A123,?C45?D6." },
    { "A123,BC45\nD6.",   I+C,  "A\t",   "BE",  '?',   ", \n ."        , "A123 BC45 D6"  , "A?,?\n?."       , "A123?C45?D6?"  },


    // fSS_NoMerge
 
    { "",                 NM,     " A\t",  "BE",  '?',  ""              , ""                 , ""                 , ""               },
    { "   ",              NM,     " A\t",  "BE",  '?',  ""              , ""                 , ""                 , ""               },
    { "   ",              NM,     " ",     "",    '?',  ""              , ""                 , ""                 , ""               }, // 80
    { "   ",              NM,     "",      " ",   '?',  ""              , ""                 , "???"              , "???"            },
    { "   ",              NM,     " ",     " ",   '?',  ""              , ""                 , "???"              , "???"            },
    { "ABC",              NM,     " A\t",  "BE",  '?',  "ABC"           , ""                 , "A?C"              , "A??"            },
    { "  ABC",            NM,     " A\t",  "BE",  '?',  "ABC"           , ""                 , "A?C"              , "A??"            },
    { "ABC  ",            NM,     " A\t",  "BE",  '?',  "ABC"           , ""                 , "A?C"              , "A??"            },
    { " ABC ",            NM,     " A\t",  "BE",  '?',  "ABC"           , ""                 , "A?C"              , "A??"            },
    { "   ABC   ",        NM,     " A\t",  "BE",  '?',  "ABC"           , ""                 , "A?C"              , "A??"            },
    { "   A B C   ",      NM,     " A\t",  "BE",  '?',  "A B C"         , ""                 , "A ? C"            , "A ? ?"          },
    { "   A  B  C   ",    NM,     " A\t",  "BE",  '?',  "A  B  C"       , ""                 , "A  ?  C"          , "A  ?  ?"        },
    { "   AB CD EF   ",   NM,     " A\t",  "BE",  '?',  "AB CD EF"      , ""                 , "A? CD ?F"         , "A? ?? ??"       }, // 90
    { "   AB  CD  EF   ", NM,     " A\t",  "BE",  '?',  "AB  CD  EF"    , ""                 , "A?  CD  ?F"       , "A?  ??  ??"     },
    { "\nA\tB""\x01 ""C", NM,     " A\t",  "BE",  '?',  "A B  C"        , "\n \t \x01"       , "\nA\t?\x01 C"     , "?A\t?? ?"       },
    { "\n \nA B\nC\n \n", NM,     " A\t",  "BE",  '?',  "A B C"         , "\n \n   \n \n \n" , "\n \nA ?\nC\n \n" , "? ?A ???? ?"    },
    { "\n\nA B\nC\n\n",   NM,     " A\t",  "BE",  '?',  "A B C"         , "\n\n   \n \n\n"   , "\n\nA ?\nC\n\n"   , "??A ?????"      },
    { "  \nA B\nC\n  ",   NM,     " A\t",  "BE",  '?',  "A B C"         , "\n   \n \n"       , "\nA ?\nC\n"       , "?A ????"        },
    { "A123,BC45\nD6.",   NM,     " A\t",  "BE",  '?',  "A123,BC45 D6." , "\n"               , "A123,?C45\nD6."   , "A????????????"  },
    { "A123,BC45\nD6.",   NM+PR,  " A\t",  "BE",  '?',  "A123,BC45 D6." , "\n"               , "A123,?C45?D6."    , "A????????\n???" },
    { "A123,BC45\nD6.",   NM+A,   " A\t",  "BE",  '?',  "A    BC   D"   , "123,  45\n 6."    , "A?????C???D??"    , "A123,??45\n?6." },
    { "A123,BC45\nD6.",   NM+D,   " A\t",  "BE",  '?',  "123   45  6"   , "A   ,BC  \nD ."   , "A123???45??6?"    , "A???,?C??\nD?." },
    { "A123,BC45\nD6.",   NM+AD,  " A\t",  "BE",  '?',  "A123 BC45 D6"  , ",    \n  ."       , "A123??C45?D6?"    , "A???,????\n??." }, // 100
    { "A123,BC45\nD6.",   NM+A+D, " A\t",  "BE",  '?',  "A123 BC45 D6"  , ",    \n  ."       , "A123??C45?D6?"    , "A???,????\n??." },
    { "A123,BC45\nD6.",   NM+I,   " A\t",  "BE",  '?',  ",       ."     , "A123 BC45\nD6"    , "A???,???????."    , "A123??C45\nD6?" },
    { "A123,BC45\nD6.",   NM+C,   " A\t",  "BE",  '?',  "\n"            , "A123,BC45 D6."    , "A????????\n???"   , "A123,?C45?D6."  },
    { "A123,BC45\nD6.",   NM+I+C, " A\t",  "BE",  '?',  ",    \n  ."    , "A123 BC45 D6"     , "A???,????\n??."   , "A123??C45?D6?"  },

    // fSS_Remove (remove not allowed + merge by default)

    { "",                 RM,     " A\t",  "BE",  '?',  ""             , ""             , ""                , ""             }, // 105
    { "   ",              RM,     " A\t",  "BE",  '?',  ""             , ""             , ""                , ""             },
    { "   ",              RM,     " ",     "",    '?',  ""             , ""             , ""                , ""             },
    { "   ",              RM,     "",      " ",   '?',  ""             , ""             , ""                , ""             },
    { "   ",              RM,     " ",     " ",   '?',  ""             , ""             , ""                , ""             },
    { "ABC",              RM,     " A\t",  "BE",  '?',  "ABC"          , ""             , "AC"              , "A"            }, // 110
    { "  ABC",            RM,     " A\t",  "BE",  '?',  "ABC"          , ""             , "AC"              , "A"            },
    { "ABC  ",            RM,     " A\t",  "BE",  '?',  "ABC"          , ""             , "AC"              , "A"            },
    { " ABC ",            RM,     " A\t",  "BE",  '?',  "ABC"          , ""             , "AC"              , "A"            },
    { "   ABC   ",        RM,     " A\t",  "BE",  '?',  "ABC"          , ""             , "AC"              , "A"            },
    { "   A B C   ",      RM,     " A\t",  "BE",  '?',  "A B C"        , ""             , "A C"             , "A"            },
    { "   A  B  C   ",    RM,     " A\t",  "BE",  '?',  "A B C"        , ""             , "A C"             , "A"            },
    { "   AB CD EF   ",   RM,     " A\t",  "BE",  '?',  "AB CD EF"     , ""             , "A CD F"          , "A"            },
    { "   AB  CD  EF   ", RM,     " A\t",  "BE",  '?',  "AB CD EF"     , ""             , "A CD F"          , "A"            },
    { "\nA\tB""\x01 ""C", RM,     " A\t",  "BE",  '?',  "AB C"         , "\n\t\x01"     , "\nA\t\x01 C"     , "A\t"          },
    { "\n \nA B\nC\n \n", RM,     " A\t",  "BE",  '?',  "A BC"         , "\n\n\n\n\n"   , "\n \nA \nC\n \n" , "A"            }, // 120
    { "\n\nA B\nC\n\n",   RM,     " A\t",  "BE",  '?',  "A BC"         , "\n\n\n\n\n"   , "\n\nA \nC\n\n"   , "A"            },
    { "  \nA B\nC\n  ",   RM,     " A\t",  "BE",  '?',  "A BC"         , "\n\n\n"       , "\nA \nC\n"       , "A"            },
    { "A123,BC45\nD6.",   RM,     " A\t",  "BE",  '?',  "A123,BC45D6." , "\n"           , "A123,C45\nD6."   , "A"            },
    { "A123,BC45\nD6.",   RM+PR,  " A\t",  "BE",  '?',  "A123,BC45D6." , "\n"           , "A123,C45D6."     , "A\n"          },
    { "A123,BC45\nD6.",   RM+A,   " A\t",  "BE",  '?',  "ABCD"         , "123,45\n6."   , "ACD"             , "A123,45\n6."  },
    { "A123,BC45\nD6.",   RM+D,   " A\t",  "BE",  '?',  "123456"       , "A,BC\nD."     , "A123456"         , "A,C\nD."      },
    { "A123,BC45\nD6.",   RM+AD,  " A\t",  "BE",  '?',  "A123BC45D6"   , ",\n."         , "A123C45D6"       , "A,\n."        },
    { "A123,BC45\nD6.",   RM+A+D, " A\t",  "BE",  '?',  "A123BC45D6"   , ",\n."         , "A123C45D6"       , "A,\n."        },
    { "A123,BC45\nD6.",   RM+I,   " A\t",  "BE",  '?',  ",."           , "A123BC45\nD6" , "A,."             , "A123C45\nD6"  },
    { "A123,BC45\nD6.",   RM+C,   " A\t",  "BE",  '?',  "\n"           , "A123,BC45D6." , "A\n"             , "A123,C45D6."  }, //130
    { "A123,BC45\nD6.",   RM+I+C, " A\t",  "BE",  '?',  ",\n."         , "A123BC45D6"   , "A,\n."           , "A123C45D6"    },

    // fSS_Remove + fSS_NoMerge

    { "",                 NM+RM,     " A\t",  "BE",  '?',  ""             , ""             , ""                 , ""            }, // 132
    { "   ",              NM+RM,     " A\t",  "BE",  '?',  ""             , ""             , ""                 , ""            },
    { "   ",              NM+RM,     " ",     "",    '?',  ""             , ""             , ""                 , ""            },
    { "   ",              NM+RM,     "",      " ",   '?',  ""             , ""             , ""                 , ""            },
    { "   ",              NM+RM,     " ",     " ",   '?',  ""             , ""             , ""                 , ""            },
    { "ABC",              NM+RM,     " A\t",  "BE",  '?',  "ABC"          , ""             , "AC"               , "A"           },
    { "  ABC",            NM+RM,     " A\t",  "BE",  '?',  "ABC"          , ""             , "AC"               , "A"           },
    { "ABC  ",            NM+RM,     " A\t",  "BE",  '?',  "ABC"          , ""             , "AC"               , "A"           },
    { " ABC ",            NM+RM,     " A\t",  "BE",  '?',  "ABC"          , ""             , "AC"               , "A"           }, // 140
    { "   ABC   ",        NM+RM,     " A\t",  "BE",  '?',  "ABC"          , ""             , "AC"               , "A"           },
    { "   A B C   ",      NM+RM,     " A\t",  "BE",  '?',  "A B C"        , ""             , "A  C"             , "A"           },
    { "   A  B  C   ",    NM+RM,     " A\t",  "BE",  '?',  "A  B  C"      , ""             , "A    C"           , "A"           },
    { "   AB CD EF   ",   NM+RM,     " A\t",  "BE",  '?',  "AB CD EF"     , ""             , "A CD F"           , "A"           },
    { "   AB  CD  EF   ", NM+RM,     " A\t",  "BE",  '?',  "AB  CD  EF"   , ""             , "A  CD  F"         , "A"           },
    { "\nA\tB""\x01 ""C", NM+RM,     " A\t",  "BE",  '?',  "AB C"         , "\n\t\x01"     , "\nA\t\x01 C"      , "A\t"         },
    { "\n \nA B\nC\n \n", NM+RM,     " A\t",  "BE",  '?',  "A BC"         , "\n\n\n\n\n"   , "\n \nA \nC\n \n"  , "A"           },
    { "\n\nA B\nC\n\n",   NM+RM,     " A\t",  "BE",  '?',  "A BC"         , "\n\n\n\n\n"   , "\n\nA \nC\n\n"    , "A"           },
    { "  \nA B\nC\n  ",   NM+RM,     " A\t",  "BE",  '?',  "A BC"         , "\n\n\n"       , "\nA \nC\n"        , "A"           },
    { "A123,BC45\nD6.",   NM+RM,     " A\t",  "BE",  '?',  "A123,BC45D6." , "\n"           , "A123,C45\nD6."    , "A"           }, // 150
    { "A123,BC45\nD6.",   NM+RM+PR,  " A\t",  "BE",  '?',  "A123,BC45D6." , "\n"           , "A123,C45D6."      , "A\n"         },
    { "A123,BC45\nD6.",   NM+RM+A,   " A\t",  "BE",  '?',  "ABCD"         , "123,45\n6."   , "ACD"              , "A123,45\n6." },
    { "A123,BC45\nD6.",   NM+RM+D,   " A\t",  "BE",  '?',  "123456"       , "A,BC\nD."     , "A123456"          , "A,C\nD."     },
    { "A123,BC45\nD6.",   NM+RM+AD,  " A\t",  "BE",  '?',  "A123BC45D6"   , ",\n."         , "A123C45D6"        , "A,\n."       },
    { "A123,BC45\nD6.",   NM+RM+A+D, " A\t",  "BE",  '?',  "A123BC45D6"   , ",\n."         , "A123C45D6"        , "A,\n."       },
    { "A123,BC45\nD6.",   NM+RM+I,   " A\t",  "BE",  '?',  ",."           , "A123BC45\nD6" , "A,."              , "A123C45\nD6" },
    { "A123,BC45\nD6.",   NM+RM+C,   " A\t",  "BE",  '?',  "\n"           , "A123,BC45D6." , "A\n"              , "A123,C45D6." },
    { "A123,BC45\nD6.",   NM+RM+I+C, " A\t",  "BE",  '?',  ",\n."         , "A123BC45D6"   , "A,\n."            , "A123C45D6"   },

    // fSS_NoTruncate_* (+ merge by default)

    { "",                 NTB,  " A\t",  "BE",  '?',  ""           , ""                , ""                 , ""          },
    { "   ",              NTB,  " A\t",  "BE",  '?',  ""           , ""                , ""                 , ""          }, // 160
    { "   ",              NTB,  " ",     "",    '?',  ""           , ""                , ""                 , ""          },
    { "   ",              NTB,  "",      " ",   '?',  ""           , ""                , "?"                , "?"         },
    { "   ",              NTB,  " ",     " ",   '?',  ""           , ""                , "?"                , "?"         },
    { "ABC",              NTB,  " A\t",  "BE",  '?',  "ABC"        , ""                , "A?C"              , "A?"        },
    { "  ABC",            NTB,  " A\t",  "BE",  '?',  " ABC"       , ""                , " A?C"             , " A?"       },
    { "ABC  ",            NTB,  " A\t",  "BE",  '?',  "ABC"        , ""                , "A?C"              , "A?"        },
    { " ABC ",            NTB,  " A\t",  "BE",  '?',  " ABC"       , ""                , " A?C"             , " A?"       },
    { "   ABC   ",        NTB,  " A\t",  "BE",  '?',  " ABC"       , ""                , " A?C"             , " A?"       },
    { "   A B C   ",      NTB,  " A\t",  "BE",  '?',  " A B C"     , ""                , " A ? C"           , " A ? ?"    },
    { "   A  B  C   ",    NTB,  " A\t",  "BE",  '?',  " A B C"     , ""                , " A ? C"           , " A ? ?"    }, // 170
    { "   AB CD EF   ",   NTB,  " A\t",  "BE",  '?',  " AB CD EF"  , ""                , " A? CD ?F"        , " A? ? ?"   },
    { "   AB  CD  EF   ", NTB,  " A\t",  "BE",  '?',  " AB CD EF"  , ""                , " A? CD ?F"        , " A? ? ?"   },
    { "\n\nA  B\nC\n\n",  NTB,  " A\t",  "BE",  '?',  " A B C"     , "\n\n \n \n\n"    , "\n\nA ?\nC\n\n"   , "?A ?"      },
    { "\n \nA B\nC\n \n", NTB,  " A\t",  "BE",  '?',  " A B C"     , "\n \n \n \n \n"  , "\n \nA ?\nC\n \n" , "? ?A ? ?"  },
    { " \nA  B\nC\n ",    NTB,  " A\t",  "BE",  '?',  " A B C"     , " \n \n \n"       , " \nA ?\nC\n"      , " ?A ?"     },
    { "  \nA B\nC\n  ",   NTB,  " A\t",  "BE",  '?',  " A B C"     , " \n \n \n"       , " \nA ?\nC\n"      , " ?A ?"     },

    { "",                 NTE,  " A\t",  "BE",  '?',  ""           , ""                , ""                 , ""          },
    { "   ",              NTE,  " A\t",  "BE",  '?',  ""           , ""                , ""                 , ""          },
    { "   ",              NTE,  " ",     "",    '?',  ""           , ""                , ""                 , ""          },
    { "   ",              NTE,  "",      " ",   '?',  ""           , ""                , "?"                , "?"         }, // 180
    { "   ",              NTE,  " ",     " ",   '?',  ""           , ""                , "?"                , "?"         },
    { "ABC",              NTE,  " A\t",  "BE",  '?',  "ABC"        , ""                , "A?C"              , "A?"        },
    { "  ABC",            NTE,  " A\t",  "BE",  '?',  "ABC"        , ""                , "A?C"              , "A?"        },
    { "ABC  ",            NTE,  " A\t",  "BE",  '?',  "ABC "       , ""                , "A?C "             , "A? "       },
    { " ABC ",            NTE,  " A\t",  "BE",  '?',  "ABC "       , ""                , "A?C "             , "A? "       },
    { "   ABC   ",        NTE,  " A\t",  "BE",  '?',  "ABC "       , ""                , "A?C "             , "A? "       },
    { "   A B C   ",      NTE,  " A\t",  "BE",  '?',  "A B C "     , ""                , "A ? C "           , "A ? ? "    },
    { "   A  B  C   ",    NTE,  " A\t",  "BE",  '?',  "A B C "     , ""                , "A ? C "           , "A ? ? "    },
    { "   AB CD EF   ",   NTE,  " A\t",  "BE",  '?',  "AB CD EF "  , ""                , "A? CD ?F "        , "A? ? ? "   },
    { "   AB  CD  EF   ", NTE,  " A\t",  "BE",  '?',  "AB CD EF "  , ""                , "A? CD ?F "        , "A? ? ? "   }, // 190
    { "\n\nA  B\nC\n\n",  NTE,  " A\t",  "BE",  '?',  "A B C "     , "\n\n \n \n\n"    , "\n\nA ?\nC\n\n"   , "?A ?"      },
    { "\n \nA B\nC\n \n", NTE,  " A\t",  "BE",  '?',  "A B C "     , "\n \n \n \n \n"  , "\n \nA ?\nC\n \n" , "? ?A ? ?"  },
    { " \nA  B\nC\n ",    NTE,  " A\t",  "BE",  '?',  "A B C "     ,  "\n \n \n "      , "\nA ?\nC\n "      , "?A ? "     },
    { "  \nA B\nC\n  ",   NTE,  " A\t",  "BE",  '?',  "A B C "     , "\n \n \n "       , "\nA ?\nC\n "      , "?A ? "     },

    { "",                 NT,   " A\t",  "BE",  '?',  ""           , ""                , ""                 , ""          },
    { "   ",              NT,   " A\t",  "BE",  '?',  " "          , " "               , " "                , " "         },
    { "   ",              NT,   " ",     "",    '?',  " "          , " "               , " "                , " "         },
    { "   ",              NT,   "",      " ",   '?',  " "          , " "               , "?"                , "?"         },
    { "   ",              NT,   " ",     " ",   '?',  " "          , " "               , "?"                , "?"         },
    { "ABC",              NT,   " A\t",  "BE",  '?',  "ABC"        , " "               , "A?C"              , "A?"        }, // 200
    { "  ABC",            NT,   " A\t",  "BE",  '?',  " ABC"       , " "               , " A?C"             , " A?"       },
    { "ABC  ",            NT,   " A\t",  "BE",  '?',  "ABC "       , " "               , "A?C "             , "A? "       },
    { " ABC ",            NT,   " A\t",  "BE",  '?',  " ABC "      , " "               , " A?C "            , " A? "      },
    { "   ABC   ",        NT,   " A\t",  "BE",  '?',  " ABC "      , " "               , " A?C "            , " A? "      },
    { "   A B C   ",      NT,   " A\t",  "BE",  '?',  " A B C "    , " "               , " A ? C "          , " A ? ? "   },
    { "   A  B  C   ",    NT,   " A\t",  "BE",  '?',  " A B C "    , " "               , " A ? C "          , " A ? ? "   },
    { "   AB CD EF   ",   NT,   " A\t",  "BE",  '?',  " AB CD EF " , " "               , " A? CD ?F "       , " A? ? ? "  },
    { "   AB  CD  EF   ", NT,   " A\t",  "BE",  '?',  " AB CD EF " , " "               , " A? CD ?F "       , " A? ? ? "  },
    { "\n\nA  B\nC\n\n",  NT,   " A\t",  "BE",  '?',  " A B C "    , "\n\n \n \n\n"    , "\n\nA ?\nC\n\n"   , "?A ?"      },
    { "\n \nA B\nC\n \n", NT,   " A\t",  "BE",  '?',  " A B C "    , "\n \n \n \n \n"  , "\n \nA ?\nC\n \n" , "? ?A ? ?"  }, // 210
    { " \nA  B\nC\n ",    NT,   " A\t",  "BE",  '?',  " A B C "    , " \n \n \n "      , " \nA ?\nC\n "     , " ?A ? "    },
    { "  \nA B\nC\n  ",   NT,   " A\t",  "BE",  '?',  " A B C "    , " \n \n \n "      , " \nA ?\nC\n "     , " ?A ? "    },

    // fSS_NoTruncate_* + fSS_NoMerge

    { "",                 NTB+NM,  " A\t",  "BE",  '?',  ""              ,  ""                  , ""                 , ""               },
    { "   ",              NTB+NM,  " A\t",  "BE",  '?',  ""              ,  ""                  , ""                 , ""               },
    { "   ",              NTB+NM,  " ",     "",    '?',  ""              ,  ""                  , ""                 , ""               },
    { "   ",              NTB+NM,  "",      " ",   '?',  ""              ,  ""                  , "???"              , "???"            },
    { "   ",              NTB+NM,  " ",     " ",   '?',  ""              ,  ""                  , "???"              , "???"            },
    { "ABC",              NTB+NM,  " A\t",  "BE",  '?',  "ABC"           ,  ""                  , "A?C"              , "A??"            },
    { "  ABC",            NTB+NM,  " A\t",  "BE",  '?',  "  ABC"         ,  ""                  , "  A?C"            , "  A??"          },
    { "ABC  ",            NTB+NM,  " A\t",  "BE",  '?',  "ABC"           ,  ""                  , "A?C"              , "A??"            }, // 220
    { " ABC ",            NTB+NM,  " A\t",  "BE",  '?',  " ABC"          ,  ""                  , " A?C"             , " A??"           },
    { "   ABC   ",        NTB+NM,  " A\t",  "BE",  '?',  "   ABC"        ,  ""                  , "   A?C"           , "   A??"         },
    { "   A B C   ",      NTB+NM,  " A\t",  "BE",  '?',  "   A B C"      ,  ""                  , "   A ? C"         , "   A ? ?"       },
    { "   A  B  C   ",    NTB+NM,  " A\t",  "BE",  '?',  "   A  B  C"    ,  ""                  , "   A  ?  C"       , "   A  ?  ?"     },
    { "   AB CD EF   ",   NTB+NM,  " A\t",  "BE",  '?',  "   AB CD EF"   ,  ""                  , "   A? CD ?F"      , "   A? ?? ??"    },
    { "   AB  CD  EF   ", NTB+NM,  " A\t",  "BE",  '?',  "   AB  CD  EF" ,  ""                  , "   A?  CD  ?F"    , "   A?  ??  ??"  },
    { "\n\nA  B\nC\n\n",  NTB+NM,  " A\t",  "BE",  '?',  "  A  B C"      ,  "\n\n    \n \n\n"   , "\n\nA  ?\nC\n\n"  , "??A  ?????"     },
    { "\n \nA B\nC\n \n", NTB+NM,  " A\t",  "BE",  '?',  "   A B C"      ,  "\n \n   \n \n \n"  , "\n \nA ?\nC\n \n" , "? ?A ???? ?"    },
    { " \nA  B\nC\n ",    NTB+NM,  " A\t",  "BE",  '?',  "  A  B C"      ,  " \n    \n \n"      , " \nA  ?\nC\n"     , " ?A  ????"      },
    { "  \nA B\nC\n  ",   NTB+NM,  " A\t",  "BE",  '?',  "   A B C"      ,  "  \n   \n \n"      , "  \nA ?\nC\n"     , "  ?A ????"      }, // 230

    { "",                 NTE+NM,  " A\t",  "BE",  '?',  ""              ,  ""                  , ""                 , ""               },
    { "   ",              NTE+NM,  " A\t",  "BE",  '?',  ""              ,  ""                  , ""                 , ""               },
    { "   ",              NTE+NM,  " ",     "",    '?',  ""              ,  ""                  , ""                 , ""               },
    { "   ",              NTE+NM,  "",      " ",   '?',  ""              ,  ""                  , "???"              , "???"            },
    { "   ",              NTE+NM,  " ",     " ",   '?',  ""              ,  ""                  , "???"              , "???"            },
    { "ABC",              NTE+NM,  " A\t",  "BE",  '?',  "ABC"           ,  ""                  , "A?C"              , "A??"            },
    { "  ABC",            NTE+NM,  " A\t",  "BE",  '?',  "ABC"           ,  ""                  , "A?C"              , "A??"            },
    { "ABC  ",            NTE+NM,  " A\t",  "BE",  '?',  "ABC  "         ,  ""                  , "A?C  "            , "A??  "          },
    { " ABC ",            NTE+NM,  " A\t",  "BE",  '?',  "ABC "          ,  ""                  , "A?C "             , "A?? "           },
    { "   ABC   ",        NTE+NM,  " A\t",  "BE",  '?',  "ABC   "        ,  ""                  , "A?C   "           , "A??   "         }, // 240
    { "   A B C   ",      NTE+NM,  " A\t",  "BE",  '?',  "A B C   "      ,  ""                  , "A ? C   "         , "A ? ?   "       },
    { "   A  B  C   ",    NTE+NM,  " A\t",  "BE",  '?',  "A  B  C   "    ,  ""                  , "A  ?  C   "       , "A  ?  ?   "     },
    { "   AB CD EF   ",   NTE+NM,  " A\t",  "BE",  '?',  "AB CD EF   "   ,  ""                  , "A? CD ?F   "      , "A? ?? ??   "    },
    { "   AB  CD  EF   ", NTE+NM,  " A\t",  "BE",  '?',  "AB  CD  EF   " ,  ""                  , "A?  CD  ?F   "    , "A?  ??  ??   "  },
    { "\n\nA  B\nC\n\n",  NTE+NM,  " A\t",  "BE",  '?',  "A  B C  "      ,  "\n\n    \n \n\n"   , "\n\nA  ?\nC\n\n"  , "??A  ?????"     },
    { "\n \nA B\nC\n \n", NTE+NM,  " A\t",  "BE",  '?',  "A B C   "      ,  "\n \n   \n \n \n"  , "\n \nA ?\nC\n \n" , "? ?A ???? ?"    },
    { " \nA  B\nC\n ",    NTE+NM,  " A\t",  "BE",  '?',  "A  B C  "      ,  "\n    \n \n "      , "\nA  ?\nC\n "     , "?A  ???? "      },
    { "  \nA B\nC\n  ",   NTE+NM,  " A\t",  "BE",  '?',  "A B C   "      ,  "\n   \n \n  "      , "\nA ?\nC\n  "     , "?A ????  "      },

    { "",                 NT+NM,   " A\t",  "BE",  '?',  ""                 ,  ""                  , ""                 , ""                 },
    { "   ",              NT+NM,   " A\t",  "BE",  '?',  "   "              ,  "   "               , "   "              , "   "              }, // 250
    { "   ",              NT+NM,   " ",     "",    '?',  "   "              ,  "   "               , "   "              , "   "              },
    { "   ",              NT+NM,   "",      " ",   '?',  "   "              ,  "   "               , "???"              , "???"              },
    { "   ",              NT+NM,   " ",     " ",   '?',  "   "              ,  "   "               , "???"              , "???"              },
    { "ABC",              NT+NM,   " A\t",  "BE",  '?',  "ABC"              ,  "   "               , "A?C"              , "A??"              },
    { "  ABC",            NT+NM,   " A\t",  "BE",  '?',  "  ABC"            ,  "     "             , "  A?C"            , "  A??"            },
    { "ABC  ",            NT+NM,   " A\t",  "BE",  '?',  "ABC  "            ,  "     "             , "A?C  "            , "A??  "            },
    { " ABC ",            NT+NM,   " A\t",  "BE",  '?',  " ABC "            ,  "     "             , " A?C "            , " A?? "            },
    { "   ABC   ",        NT+NM,   " A\t",  "BE",  '?',  "   ABC   "        ,  "         "         , "   A?C   "        , "   A??   "        },
    { "   A B C   ",      NT+NM,   " A\t",  "BE",  '?',  "   A B C   "      ,  "           "       , "   A ? C   "      , "   A ? ?   "      },
    { "   A  B  C   ",    NT+NM,   " A\t",  "BE",  '?',  "   A  B  C   "    ,  "             "     , "   A  ?  C   "    , "   A  ?  ?   "    }, // 260
    { "   AB CD EF   ",   NT+NM,   " A\t",  "BE",  '?',  "   AB CD EF   "   ,  "              "    , "   A? CD ?F   "   , "   A? ?? ??   "   },
    { "   AB  CD  EF   ", NT+NM,   " A\t",  "BE",  '?',  "   AB  CD  EF   " ,  "                "  , "   A?  CD  ?F   " , "   A?  ??  ??   " },
    { "\n\nA  B\nC\n\n",  NT+NM,   " A\t",  "BE",  '?',  "  A  B C  "       ,  "\n\n    \n \n\n"   , "\n\nA  ?\nC\n\n"  , "??A  ?????"       },
    { "\n \nA B\nC\n \n", NT+NM,   " A\t",  "BE",  '?',  "   A B C   "      ,  "\n \n   \n \n \n"  , "\n \nA ?\nC\n \n" , "? ?A ???? ?"      },
    { " \nA  B\nC\n ",    NT+NM,   " A\t",  "BE",  '?',  "  A  B C  "       ,  " \n    \n \n "     , " \nA  ?\nC\n "    , " ?A  ???? "       },
    { "  \nA B\nC\n  ",   NT+NM,   " A\t",  "BE",  '?',  "   A B C   "      ,  "  \n   \n \n  "    , "  \nA ?\nC\n  "   , "  ?A ????  "      },

    // fSS_NoTruncate_* + fSS_Remove

    { "",                 NTB+RM,  " A\t",  "BE",  '?',  ""          ,  ""            , ""                 , ""     },
    { "   ",              NTB+RM,  " A\t",  "BE",  '?',  ""          ,  ""            , ""                 , ""     },
    { "   ",              NTB+RM,  " ",     "",    '?',  ""          ,  ""            , ""                 , ""     },
    { "   ",              NTB+RM,  "",      " ",   '?',  ""          ,  ""            , ""                 , ""     }, // 270
    { "   ",              NTB+RM,  " ",     " ",   '?',  ""          ,  ""            , ""                 , ""     },
    { "ABC",              NTB+RM,  " A\t",  "BE",  '?',  "ABC"       ,  ""            , "AC"               , "A"    },
    { "  ABC",            NTB+RM,  " A\t",  "BE",  '?',  " ABC"      ,  ""            , " AC"              , " A"   },
    { "ABC  ",            NTB+RM,  " A\t",  "BE",  '?',  "ABC"       ,  ""            , "AC"               , "A"    },
    { " ABC ",            NTB+RM,  " A\t",  "BE",  '?',  " ABC"      ,  ""            , " AC"              , " A"   },
    { "   ABC   ",        NTB+RM,  " A\t",  "BE",  '?',  " ABC"      ,  ""            , " AC"              , " A"   },
    { "   A B C   ",      NTB+RM,  " A\t",  "BE",  '?',  " A B C"    ,  ""            , " A C"             , " A"   },
    { "   A  B  C   ",    NTB+RM,  " A\t",  "BE",  '?',  " A B C"    ,  ""            , " A C"             , " A"   },
    { "   AB CD EF   ",   NTB+RM,  " A\t",  "BE",  '?',  " AB CD EF" ,  ""            , " A CD F"          , " A"   },
    { "   AB  CD  EF   ", NTB+RM,  " A\t",  "BE",  '?',  " AB CD EF" ,  ""            , " A CD F"          , " A"   }, // 280
    { "\n\nA  B\nC\n\n",  NTB+RM,  " A\t",  "BE",  '?',  "A BC"      ,  "\n\n\n\n\n"  , "\n\nA \nC\n\n"    , "A"    },
    { "\n \nA B\nC\n \n", NTB+RM,  " A\t",  "BE",  '?',  " A BC"     ,  "\n\n\n\n\n"  , "\n \nA \nC\n \n"  , " A"   },
    { " \nA  B\nC\n ",    NTB+RM,  " A\t",  "BE",  '?',  " A BC"     ,  "\n\n\n"      , " \nA \nC\n"       , " A"   },
    { "  \nA B\nC\n  ",   NTB+RM,  " A\t",  "BE",  '?',  " A BC"     ,  "\n\n\n"      , " \nA \nC\n"       , " A"   },

    { "",                 NTE+RM,  " A\t",  "BE",  '?',  ""          ,  ""            , ""                 , ""     },
    { "   ",              NTE+RM,  " A\t",  "BE",  '?',  ""          ,  ""            , ""                 , ""     },
    { "   ",              NTE+RM,  " ",     "",    '?',  ""          ,  ""            , ""                 , ""     },
    { "   ",              NTE+RM,  "",      " ",   '?',  ""          ,  ""            , ""                 , ""     },
    { "   ",              NTE+RM,  " ",     " ",   '?',  ""          ,  ""            , ""                 , ""     },
    { "ABC",              NTE+RM,  " A\t",  "BE",  '?',  "ABC"       ,  ""            , "AC"               , "A"    }, // 290
    { "  ABC",            NTE+RM,  " A\t",  "BE",  '?',  "ABC"       ,  ""            , "AC"               , "A"    },
    { "ABC  ",            NTE+RM,  " A\t",  "BE",  '?',  "ABC "      ,  ""            , "AC "              , "A "   },
    { " ABC ",            NTE+RM,  " A\t",  "BE",  '?',  "ABC "      ,  ""            , "AC "              , "A "   },
    { "   ABC   ",        NTE+RM,  " A\t",  "BE",  '?',  "ABC "      ,  ""            , "AC "              , "A "   },
    { "   A B C   ",      NTE+RM,  " A\t",  "BE",  '?',  "A B C "    ,  ""            , "A C "             , "A "   },
    { "   A  B  C   ",    NTE+RM,  " A\t",  "BE",  '?',  "A B C "    ,  ""            , "A C "             , "A "   },
    { "   AB CD EF   ",   NTE+RM,  " A\t",  "BE",  '?',  "AB CD EF " ,  ""            , "A CD F "          , "A "   },
    { "   AB  CD  EF   ", NTE+RM,  " A\t",  "BE",  '?',  "AB CD EF " ,  ""            , "A CD F "          , "A "   },
    { "\n\nA  B\nC\n\n",  NTE+RM,  " A\t",  "BE",  '?',  "A BC"      ,  "\n\n\n\n\n"  , "\n\nA \nC\n\n"    , "A "   },
    { "\n \nA B\nC\n \n", NTE+RM,  " A\t",  "BE",  '?',  "A BC "     ,  "\n\n\n\n\n"  , "\n \nA \nC\n \n"  , "A "   }, // 300
    { " \nA  B\nC\n ",    NTE+RM,  " A\t",  "BE",  '?',  "A BC "     ,  "\n\n\n"      , "\nA \nC\n "       , "A "   },
    { "  \nA B\nC\n  ",   NTE+RM,  " A\t",  "BE",  '?',  "A BC "     ,  "\n\n\n"      , "\nA \nC\n "       , "A "   },

    { "",                 NT+RM,   " A\t",  "BE",  '?',  ""           ,  ""            , ""                , ""     },
    { "   ",              NT+RM,   " A\t",  "BE",  '?',  " "          ,  ""            , " "               , " "    },
    { "   ",              NT+RM,   " ",     "",    '?',  " "          ,  ""            , " "               , " "    },
    { "   ",              NT+RM,   "",      " ",   '?',  " "          ,  ""            , ""                , ""     },
    { "   ",              NT+RM,   " ",     " ",   '?',  " "          ,  ""            , ""                , ""     },
    { "ABC",              NT+RM,   " A\t",  "BE",  '?',  "ABC"        ,  ""            , "AC"              , "A"    },
    { "  ABC",            NT+RM,   " A\t",  "BE",  '?',  " ABC"       ,  ""            , " AC"             , " A"   },
    { "ABC  ",            NT+RM,   " A\t",  "BE",  '?',  "ABC "       ,  ""            , "AC "             , "A "   }, // 310
    { " ABC ",            NT+RM,   " A\t",  "BE",  '?',  " ABC "      ,  ""            , " AC "            , " A "  },
    { "   ABC   ",        NT+RM,   " A\t",  "BE",  '?',  " ABC "      ,  ""            , " AC "            , " A "  },
    { "   A B C   ",      NT+RM,   " A\t",  "BE",  '?',  " A B C "    ,  ""            , " A C "           , " A "  },
    { "   A  B  C   ",    NT+RM,   " A\t",  "BE",  '?',  " A B C "    ,  ""            , " A C "           , " A "  },
    { "   AB CD EF   ",   NT+RM,   " A\t",  "BE",  '?',  " AB CD EF " ,  ""            , " A CD F "        , " A "  },
    { "   AB  CD  EF   ", NT+RM,   " A\t",  "BE",  '?',  " AB CD EF " ,  ""            , " A CD F "        , " A "  },
    { "\n\nA  B\nC\n\n" , NT+RM,   " A\t",  "BE",  '?',  "A BC"       ,  "\n\n\n\n\n"  , "\n\nA \nC\n\n"   , "A "   },
    { "\n \nA B\nC\n \n", NT+RM,   " A\t",  "BE",  '?',  " A BC "     ,  "\n\n\n\n\n"  , "\n \nA \nC\n \n" , " A "  },
    { " \nA  B\nC\n ",    NT+RM,   " A\t",  "BE",  '?',  " A BC "     ,  "\n\n\n"      , " \nA \nC\n "     , " A "  },
    { "  \nA B\nC\n  ",   NT+RM,   " A\t",  "BE",  '?',  " A BC "     ,  "\n\n\n"      , " \nA \nC\n "     , " A "  }, // 320

    // fSS_NoTruncate_* + fSS_NoMerge + fSS_Remove

    { "",                 NTB+NM+RM,  " A\t",  "BE",  '?',  ""              ,  ""            , ""                , ""      },
    { "   ",              NTB+NM+RM,  " A\t",  "BE",  '?',  ""              ,  ""            , ""                , ""      },
    { "   ",              NTB+NM+RM,  " ",     "",    '?',  ""              ,  ""            , ""                , ""      },
    { "   ",              NTB+NM+RM,  "",      " ",   '?',  ""              ,  ""            , ""                , ""      },
    { "   ",              NTB+NM+RM,  " ",     " ",   '?',  ""              ,  ""            , ""                , ""      },
    { "ABC",              NTB+NM+RM,  " A\t",  "BE",  '?',  "ABC"           ,  ""            , "AC"              , "A"     },
    { "  ABC",            NTB+NM+RM,  " A\t",  "BE",  '?',  "  ABC"         ,  ""            , "  AC"            , "  A"   },
    { "ABC  ",            NTB+NM+RM,  " A\t",  "BE",  '?',  "ABC"           ,  ""            , "AC"              , "A"     },
    { " ABC ",            NTB+NM+RM,  " A\t",  "BE",  '?',  " ABC"          ,  ""            , " AC"             , " A"    },
    { "   ABC   ",        NTB+NM+RM,  " A\t",  "BE",  '?',  "   ABC"        ,  ""            , "   AC"           , "   A"  }, // 330
    { "   A B C   ",      NTB+NM+RM,  " A\t",  "BE",  '?',  "   A B C"      ,  ""            , "   A  C"         , "   A"  },
    { "   A  B  C   ",    NTB+NM+RM,  " A\t",  "BE",  '?',  "   A  B  C"    ,  ""            , "   A    C"       , "   A"  },
    { "   AB CD EF   ",   NTB+NM+RM,  " A\t",  "BE",  '?',  "   AB CD EF"   ,  ""            , "   A CD F"       , "   A"  },
    { "   AB  CD  EF   ", NTB+NM+RM,  " A\t",  "BE",  '?',  "   AB  CD  EF" ,  ""            , "   A  CD  F"     , "   A"  },
    { "\n\nA  B\nC\n\n",  NTB+NM+RM,  " A\t",  "BE",  '?',  "A  BC"         ,  "\n\n\n\n\n"  , "\n\nA  \nC\n\n"  , "A"     },
    { "\n \nA B\nC\n \n", NTB+NM+RM,  " A\t",  "BE",  '?',  " A BC"         ,  "\n\n\n\n\n"  , "\n \nA \nC\n \n" , " A"    },
    { " \nA  B\nC\n ",    NTB+NM+RM,  " A\t",  "BE",  '?',  " A  BC"        ,  "\n\n\n"      , " \nA  \nC\n"     , " A"    },
    { "  \nA B\nC\n  ",   NTB+NM+RM,  " A\t",  "BE",  '?',  "  A BC"        ,  "\n\n\n"      , "  \nA \nC\n"     , "  A"   },

    { "",                 NTE+NM+RM,  " A\t",  "BE",  '?',  ""              ,  ""            , ""                , ""          },
    { "   ",              NTE+NM+RM,  " A\t",  "BE",  '?',  ""              ,  ""            , ""                , ""          }, // 340
    { "   ",              NTE+NM+RM,  " ",     "",    '?',  ""              ,  ""            , ""                , ""          },
    { "   ",              NTE+NM+RM,  "",      " ",   '?',  ""              ,  ""            , ""                , ""          },
    { "   ",              NTE+NM+RM,  " ",     " ",   '?',  ""              ,  ""            , ""                , ""          },
    { "ABC",              NTE+NM+RM,  " A\t",  "BE",  '?',  "ABC"           ,  ""            , "AC"              , "A"         },
    { "  ABC",            NTE+NM+RM,  " A\t",  "BE",  '?',  "ABC"           ,  ""            , "AC"              , "A"         },
    { "ABC  ",            NTE+NM+RM,  " A\t",  "BE",  '?',  "ABC  "         ,  ""            , "AC  "            , "A  "       },
    { " ABC ",            NTE+NM+RM,  " A\t",  "BE",  '?',  "ABC "          ,  ""            , "AC "             , "A "        },
    { "   ABC   ",        NTE+NM+RM,  " A\t",  "BE",  '?',  "ABC   "        ,  ""            , "AC   "           , "A   "      },
    { "   A B C   ",      NTE+NM+RM,  " A\t",  "BE",  '?',  "A B C   "      ,  ""            , "A  C   "         , "A     "    },
    { "   A  B  C   ",    NTE+NM+RM,  " A\t",  "BE",  '?',  "A  B  C   "    ,  ""            , "A    C   "       , "A       "  }, // 350
    { "   AB CD EF   ",   NTE+NM+RM,  " A\t",  "BE",  '?',  "AB CD EF   "   ,  ""            , "A CD F   "       , "A     "    },
    { "   AB  CD  EF   ", NTE+NM+RM,  " A\t",  "BE",  '?',  "AB  CD  EF   " ,  ""            , "A  CD  F   "     , "A       "  },
    { "\n\nA  B\nC\n\n",  NTE+NM+RM,  " A\t",  "BE",  '?',  "A  BC"         ,  "\n\n\n\n\n"  , "\n\nA  \nC\n\n"  , "A  "       },
    { "\n \nA B\nC\n \n", NTE+NM+RM,  " A\t",  "BE",  '?',  "A BC "         ,  "\n\n\n\n\n"  , "\n \nA \nC\n \n" , "A  "       },
    { " \nA  B\nC\n ",    NTE+NM+RM,  " A\t",  "BE",  '?',  "A  BC "        ,  "\n\n\n"      , "\nA  \nC\n "     , "A   "      },
    { "  \nA B\nC\n  ",   NTE+NM+RM,  " A\t",  "BE",  '?',  "A BC  "        ,  "\n\n\n"      , "\nA \nC\n  "     , "A   "      },

    { "",                 NT+NM+RM,   " A\t",  "BE",  '?',  ""                 ,  ""            , ""                , ""             },
    { "   ",              NT+NM+RM,   " A\t",  "BE",  '?',  "   "              ,  ""            , "   "             , "   "          },
    { "   ",              NT+NM+RM,   " ",     "",    '?',  "   "              ,  ""            , "   "             , "   "          },
    { "   ",              NT+NM+RM,   "",      " ",   '?',  "   "              ,  ""            , ""                , ""             }, // 360
    { "   ",              NT+NM+RM,   " ",     " ",   '?',  "   "              ,  ""            , ""                , ""             },
    { "ABC",              NT+NM+RM,   " A\t",  "BE",  '?',  "ABC"              ,  ""            , "AC"              , "A"            },
    { "  ABC",            NT+NM+RM,   " A\t",  "BE",  '?',  "  ABC"            ,  ""            , "  AC"            , "  A"          },
    { "ABC  ",            NT+NM+RM,   " A\t",  "BE",  '?',  "ABC  "            ,  ""            , "AC  "            , "A  "          },
    { " ABC ",            NT+NM+RM,   " A\t",  "BE",  '?',  " ABC "            ,  ""            , " AC "            , " A "          },
    { "   ABC   ",        NT+NM+RM,   " A\t",  "BE",  '?',  "   ABC   "        ,  ""            , "   AC   "        , "   A   "      },
    { "   A B C   ",      NT+NM+RM,   " A\t",  "BE",  '?',  "   A B C   "      ,  ""            , "   A  C   "      , "   A     "    },
    { "   A  B  C   ",    NT+NM+RM,   " A\t",  "BE",  '?',  "   A  B  C   "    ,  ""            , "   A    C   "    , "   A       "  },
    { "   AB CD EF   ",   NT+NM+RM,   " A\t",  "BE",  '?',  "   AB CD EF   "   ,  ""            , "   A CD F   "    , "   A     "    },
    { "   AB  CD  EF   ", NT+NM+RM,   " A\t",  "BE",  '?',  "   AB  CD  EF   " ,  ""            , "   A  CD  F   "  , "   A       "  }, // 370
    { "\n\nA  B\nC\n\n",  NT+NM+RM,   " A\t",  "BE",  '?',  "A  BC"            ,  "\n\n\n\n\n"  , "\n\nA  \nC\n\n"  , "A  "          },
    { "\n \nA B\nC\n \n", NT+NM+RM,   " A\t",  "BE",  '?',  " A BC "           ,  "\n\n\n\n\n"  , "\n \nA \nC\n \n" , " A  "         },
    { " \nA  B\nC\n ",    NT+NM+RM,   " A\t",  "BE",  '?',  " A  BC "          ,  "\n\n\n"      , " \nA  \nC\n "    , " A   "        },
    { "  \nA B\nC\n  ",   NT+NM+RM,   " A\t",  "BE",  '?',  "  A BC  "         ,  "\n\n\n"      , "  \nA \nC\n  "   , "  A   "       }
};

BOOST_AUTO_TEST_CASE(s_Sanitize)
{
    const size_t count = sizeof(s_SanitizeTests) / sizeof(s_SanitizeTests[0]);
    for (size_t i = 0;  i < count;  ++i)
    {
        const SSanitizeTest* test = &s_SanitizeTests[i];
        string out;

        out = NStr::Sanitize(test->str, test->flags);
        BOOST_CHECK_EQUAL(out.compare(test->res_allowed), 0);
        if (out.compare(test->res_allowed) != 0) {
            cout << i << ". A    : " 
                 << "str = '" << NStr::PrintableString(test->str) << "', "
                 << "res = '" << NStr::PrintableString(out) << "', "
                 << "expected = '" << NStr::PrintableString(test->res_allowed) << "'" << endl;
        }
        out = NStr::Sanitize(test->str, test->flags | NStr::fSS_Reject);
        BOOST_CHECK_EQUAL(out.compare(test->res_rejected), 0);
        if (out.compare(test->res_rejected) != 0) {
            cout << i << ". R    : "
                 << "str = '" << NStr::PrintableString(test->str) << "', "
                 << "res = '" << NStr::PrintableString(out) << "', "
                 << "expected = '" << NStr::PrintableString(test->res_rejected) << "'" << endl;
        }
        out = NStr::Sanitize(test->str, test->allow, test->reject, test->replacement, test->flags);
        BOOST_CHECK_EQUAL(out.compare(test->res_ex_allowed), 0);
        if (out.compare(test->res_ex_allowed) != 0) {
            cout << i << ". A ex : "
                 << "str = '" << NStr::PrintableString(test->str) << "', "
                 << "res = '" << NStr::PrintableString(out) << "', "
                 << "expected = '" << NStr::PrintableString(test->res_ex_allowed) << "'" << endl;
        }
        out = NStr::Sanitize(test->str, test->allow, test->reject, test->replacement, test->flags | NStr::fSS_Reject);
        BOOST_CHECK_EQUAL(out.compare(test->res_ex_rejected), 0);
        if (out.compare(test->res_ex_rejected) != 0) {
            cout << i << ". R ex : "
                 << "str = '" << NStr::PrintableString(test->str) << "', "
                 << "res = '" << NStr::PrintableString(out) << "', "
                 << "expected = '" << NStr::PrintableString(test->res_ex_rejected) << "'" << endl;
        }
    }
}


struct SDedent {
    const char*        str;
    const char*        expected;
    NStr::TDedentFlags flags;
};

static const SDedent s_DedentTest[] =
{
    // Default flags

    { "A"             , "A"       , 0 },
    { "A\nB"          , "A\nB"    , 0 },
    { " A\n  B"       , "A\n B"   , 0 },
    { ""              , ""        , 0 },
    { "\n"            , "\n"      , 0 },
    { "\n  "          , "\n"      , 0 },
    { "  "            , ""        , 0 },
    { "  \n"          , "\n"      , 0 },
    { "  \n  "        , "\n"      , 0 },
    { "  A"           , "A"       , 0 },
    { "  A\n"         , "A\n"     , 0 },
    { "  A\n  "       , "A\n"     , 0 },
    { "  A  "         , "A  "     , 0 },
    { "\n  A"         , "\nA"     , 0 },
    { "\nA"           , "\nA"     , 0 },
    { "\nA\n"         , "\nA\n"   , 0 },
    { "\nA\n  "       , "\nA\n  " , 0 },
    { "\t \n\tA\n\tB" , " \nA\nB" , 0 },

    // fDedent_NormalizeEmptyLines

    { "A"             , "A"       , NStr::fDedent_NormalizeEmptyLines },
    { "A\nB"          , "A\nB"    , NStr::fDedent_NormalizeEmptyLines },
    { " A\n  B"       , "A\n B"   , NStr::fDedent_NormalizeEmptyLines },
    { ""              , ""        , NStr::fDedent_NormalizeEmptyLines },
    { "\n"            , "\n"      , NStr::fDedent_NormalizeEmptyLines },
    { "\n  "          , "\n"      , NStr::fDedent_NormalizeEmptyLines },
    { "  "            , ""        , NStr::fDedent_NormalizeEmptyLines },
    { "  \n"          , "\n"      , NStr::fDedent_NormalizeEmptyLines },
    { "  \n  "        , "\n"      , NStr::fDedent_NormalizeEmptyLines },
    { "  A"           , "A"       , NStr::fDedent_NormalizeEmptyLines },
    { "  A\n"         , "A\n"     , NStr::fDedent_NormalizeEmptyLines },
    { "  A\n  "       , "A\n"     , NStr::fDedent_NormalizeEmptyLines },
    { "  A  "         , "A  "     , NStr::fDedent_NormalizeEmptyLines },
    { "\n  A"         , "\nA"     , NStr::fDedent_NormalizeEmptyLines },
    { "\nA"           , "\nA"     , NStr::fDedent_NormalizeEmptyLines },
    { "\nA\n"         , "\nA\n"   , NStr::fDedent_NormalizeEmptyLines },
    { "\nA\n  "       , "\nA\n"   , NStr::fDedent_NormalizeEmptyLines },
    { "\t \n\tA\n\tB" , "\nA\nB"  , NStr::fDedent_NormalizeEmptyLines },

    // fDedent_SkipFirstLine

    { "A"             ,  ""       , NStr::fDedent_SkipFirstLine },
    { "A\nB"          ,  "B"      , NStr::fDedent_SkipFirstLine },
    { " A\n  B"       ,  "B"      , NStr::fDedent_SkipFirstLine },
    { ""              ,  ""       , NStr::fDedent_SkipFirstLine },
    { "\n"            ,  ""       , NStr::fDedent_SkipFirstLine },
    { "\n  "          ,  ""       , NStr::fDedent_SkipFirstLine },
    { "  "            ,  ""       , NStr::fDedent_SkipFirstLine },
    { "  \n"          ,  ""       , NStr::fDedent_SkipFirstLine },
    { "  \n  "        ,  ""       , NStr::fDedent_SkipFirstLine },
    { "  A"           ,  ""       , NStr::fDedent_SkipFirstLine },
    { "  A\n"         ,  ""       , NStr::fDedent_SkipFirstLine },
    { "  A\n  "       ,  ""       , NStr::fDedent_SkipFirstLine },
    { "  A  "         ,  ""       , NStr::fDedent_SkipFirstLine },
    { "\n  A"         ,  "A"      , NStr::fDedent_SkipFirstLine },
    { "\nA"           ,  "A"      , NStr::fDedent_SkipFirstLine },
    { "\nA\n"         ,  "A\n"    , NStr::fDedent_SkipFirstLine },
    { "\nA\n  "       ,  "A\n  "  , NStr::fDedent_SkipFirstLine },
    { "\t \n\tA\n\tB" ,  "A\nB"   , NStr::fDedent_SkipFirstLine },

    // DedentR -- fDedent_SkipEmptyFirstLine

    { "A"             , "A"       , NStr::fDedent_SkipEmptyFirstLine },
    { "A\nB"          , "A\nB"    , NStr::fDedent_SkipEmptyFirstLine },
    { " A\n  B"       , "A\n B"   , NStr::fDedent_SkipEmptyFirstLine },
    { ""              , ""        , NStr::fDedent_SkipEmptyFirstLine },
    { "\n"            , ""        , NStr::fDedent_SkipEmptyFirstLine },
    { "\n  "          , ""        , NStr::fDedent_SkipEmptyFirstLine },
    { "  "            , ""        , NStr::fDedent_SkipEmptyFirstLine },
    { "  \n"          , "\n"      , NStr::fDedent_SkipEmptyFirstLine },
    { "  \n  "        , "\n"      , NStr::fDedent_SkipEmptyFirstLine },
    { "  A"           , "A"       , NStr::fDedent_SkipEmptyFirstLine },
    { "  A\n"         , "A\n"     , NStr::fDedent_SkipEmptyFirstLine },
    { "  A\n  "       , "A\n"     , NStr::fDedent_SkipEmptyFirstLine },
    { "  A  "         , "A  "     , NStr::fDedent_SkipEmptyFirstLine },
    { "\n  A"         , "A"       , NStr::fDedent_SkipEmptyFirstLine },
    { "\nA"           , "A"       , NStr::fDedent_SkipEmptyFirstLine },
    { "\nA\n"         , "A\n"     , NStr::fDedent_SkipEmptyFirstLine },
    { "\nA\n  "       , "A\n  "   , NStr::fDedent_SkipEmptyFirstLine },
    { "\t \n\tA\n\tB" , " \nA\nB" , NStr::fDedent_SkipEmptyFirstLine }
};

BOOST_AUTO_TEST_CASE(s_Dedent)
{
    size_t count = (sizeof(s_DedentTest) / sizeof(s_DedentTest[0]));

    for (size_t i = 0; i < count; ++i) 
    {
        const SDedent& test = s_DedentTest[i];
        CTempString_Storage storage;
        string result = NStr::Dedent(test.str, test.flags);
        if (result != test.expected) {
            cout << i << ". " 
                 << "str = '" << NStr::PrintableString(test.str) << "', "
                 << "res = '" << NStr::PrintableString(result) << "', "
                 << "expected = '" << NStr::PrintableString(test.expected) << "'" << endl;
        }
        BOOST_CHECK_EQUAL(result, test.expected);
    }
}


//----------------------------------------------------------------------------
// NStr::CEncode|CParse()
//----------------------------------------------------------------------------

struct SCEncodeTest {
    const char* str;                // String input
    const char* expected_nonquoted; // C encoded string with eNotQuoted flag
    const char* expected_quoted;    // C encoded string with eQuoted flag
};

static const SCEncodeTest s_CEncodeTests[] = {
    { "ABC",        "ABC",          "\"ABC\""           },
    { "ABC",        "ABC",          "\"ABC\""           },
    { "\"ABC\"",    "\\\"ABC\\\"",  "\"\\\"ABC\\\"\""   },
    { "\t\n\x44",   "\\t\\nD",      "\"\\t\\nD\""       },
    { "\x81""f",    "\\201f",       "\"\\201f\""        },
    { "\\x81f",     "\\""\\x81f",   "\"\\\\x81f\""      },
    { "\\x81""f",   "\\""\\x81f",   "\"\\\\x81f\""      },
    { "\\x81""f",   "\\\\x81f",     "\"\\\\x81f\""      }
};

BOOST_AUTO_TEST_CASE(s_CEncode)
{
    const size_t count = sizeof(s_CEncodeTests) / sizeof(s_CEncodeTests[0]);
    for (size_t i = 0;  i < count;  ++i)
    {
        const SCEncodeTest* test = &s_CEncodeTests[i];
        string ce, cp;

        // eNotQuoted
        try {
            ce = NStr::CEncode(test->str, NStr::eNotQuoted);
            BOOST_CHECK_EQUAL(ce, test->expected_nonquoted);
            cp = NStr::CParse(ce, NStr::eNotQuoted);
            BOOST_CHECK_EQUAL(cp, test->str);
        }
        catch (CStringException&) {
            _TROUBLE;
        }

        // eQuoted (by default)
        try {
            ce = NStr::CEncode(test->str, NStr::eQuoted);
            BOOST_CHECK_EQUAL(ce, test->expected_quoted);
            cp = NStr::CParse(ce, NStr::eQuoted);
            BOOST_CHECK_EQUAL(cp, test->str);
        }
        catch (CStringException&) {
            _TROUBLE;
        }
    }

    // Special cases for CParse(str, eQuoted)
    {
        string s;
        // Unterminated escaped string
        try {
            s = NStr::CParse("\"", NStr::eQuoted);
            _TROUBLE;
        }
        catch (CStringException&) {}

        // Must start with a double quote
        try {
            s = NStr::CParse(" \"A\"", NStr::eQuoted);
            _TROUBLE;
        }
        catch (CStringException&) {}

        // Must finish with a double quote
        try {
            s = NStr::CParse("\"A\" ", NStr::eQuoted);
            _TROUBLE;
        }
        catch (CStringException&) {}

        // Must finish with a double quote
        try {
            s = NStr::CParse("\"A\\t", NStr::eQuoted);
            _TROUBLE;
        }
        catch (CStringException&) {}

        // Escaped string format error
        try {
            s = NStr::CParse("\"A\\\"", NStr::eQuoted);
            _TROUBLE;
        }
        catch (CStringException&) {}

        // No anything between adjacent strings ("A""B").
        try {
            s = NStr::CParse("\"A\"?\"B\"", NStr::eQuoted);
            _TROUBLE;
        }
        catch (CStringException&) {}

        BOOST_CHECK_EQUAL( NStr::CParse("\"A\"\"B\"",          NStr::eQuoted),    "AB" );
        BOOST_CHECK_EQUAL( NStr::CParse("\"A\"\"B\"",          NStr::eNotQuoted), "\"A\"\"B\"" );
        BOOST_CHECK_EQUAL( NStr::CParse("A\"\"B",              NStr::eNotQuoted), "A\"\"B" );

        BOOST_CHECK_EQUAL( NStr::CParse("\"bar\\x44zoo\"",     NStr::eQuoted),    "barDzoo" );
        BOOST_CHECK_EQUAL( NStr::CParse("\"bar\\x44zoo\"",     NStr::eNotQuoted), "\"barDzoo\"" );

        BOOST_CHECK_EQUAL( NStr::CParse("\"\\x44\"\"f\"",      NStr::eQuoted),    "Df" );
        BOOST_CHECK_EQUAL( NStr::CParse("\\x44\"\"f",          NStr::eNotQuoted), "D\"\"f" );

        BOOST_CHECK_EQUAL( NStr::CParse("\"\x4\"\"4\"",        NStr::eQuoted),    "\x4""4" );
        BOOST_CHECK_EQUAL( NStr::CParse("\"\x4\"\"4\"",        NStr::eNotQuoted), "\"\x4\"\"4\"" );

        BOOST_CHECK_EQUAL( NStr::CParse("\"bar\\x44foo\"",     NStr::eQuoted),    "barOoo" );
        BOOST_CHECK_EQUAL( NStr::CParse("\"bar\\x44foo\"",     NStr::eNotQuoted), "\"barOoo\"" );
        BOOST_CHECK_EQUAL( NStr::CParse("bar\\x44foo",         NStr::eNotQuoted), "barOoo" );

        BOOST_CHECK_EQUAL( NStr::CParse("\"bar\\x44\"\"foo\"", NStr::eQuoted),    "barDfoo" );
        BOOST_CHECK_EQUAL( NStr::CParse("\"bar\\x44\"\"foo\"", NStr::eNotQuoted), "\"barD\"\"foo\"" );
        BOOST_CHECK_EQUAL( NStr::CParse("bar\\x44\"\"foo",     NStr::eNotQuoted), "barD\"\"foo" );
    }

    // Matrix test
    {
        for (unsigned i1 = 1;  i1 < 256;  i1++) {
            for (unsigned i2 = 0;  i2 < 256;  i2++) {
                char s[3];
                s[0] = (char) i1;
                s[1] = (char) i2;
                s[2] = '\0';

                string ce = NStr::CEncode     (s,  NStr::eQuoted);
                string cp = NStr::CParse      (ce, NStr::eQuoted);
                string pq = NStr::ParseQuoted (ce);
                BOOST_CHECK_EQUAL(s, cp);
                BOOST_CHECK_EQUAL(s, pq);

                string cenq  = NStr::CEncode (s,    NStr::eNotQuoted);
                string cpnq  = NStr::CParse  (cenq, NStr::eNotQuoted);
                BOOST_CHECK_EQUAL(s, cpnq);
                BOOST_CHECK_EQUAL("\"" + cenq + "\"", ce);
            }
        }
    }
    return;
}


//----------------------------------------------------------------------------
// NStr::Compare()
//----------------------------------------------------------------------------

static void s_CompareStr(int expr_res, int valid_res)
{
    int res = expr_res > 0 ? 1 :
        expr_res == 0 ? 0 : -1;
    BOOST_CHECK_EQUAL(res, valid_res);
}

struct SStrCompare
{
    const char* s1;
    const char* s2;

    int case_res;      /* -1, 0, 1 */
    int nocase_res;    /* -1, 0, 1 */

    SIZE_TYPE n; 
    int n_case_res;    /* -1, 0, 1 */
    int n_nocase_res;  /* -1, 0, 1 */
};

static const SStrCompare s_StrCompare[] = {
    { "", "",  0, 0,  0,     0, 0 },
    { "", "",  0, 0,  NPOS,  0, 0 },
    { "", "",  0, 0,  10,    0, 0 },
    { "", "",  0, 0,  1,     0, 0 },

    { "a", "",  1, 1,  0,     0, 0 },
    { "a", "",  1, 1,  1,     1, 1 },
    { "a", "",  1, 1,  2,     1, 1 },
    { "a", "",  1, 1,  NPOS,  1, 1 },

    { "", "bb",  -1, -1,  0,     -1, -1 },
    { "", "bb",  -1, -1,  1,     -1, -1 },
    { "", "bb",  -1, -1,  2,     -1, -1 },
    { "", "bb",  -1, -1,  3,     -1, -1 },
    { "", "bb",  -1, -1,  NPOS,  -1, -1 },

    { "ba", "bb",  -1, -1,  0,     -1, -1 },
    { "ba", "bb",  -1, -1,  1,     -1, -1 },
    { "ba", "b",    1,  1,  1,      0,  0 },
    { "ba", "bb",  -1, -1,  2,     -1, -1 },
    { "ba", "bb",  -1, -1,  3,     -1, -1 },
    { "ba", "bb",  -1, -1,  NPOS,  -1, -1 },

    { "a", "A",  1, 0,  0,    -1, -1 },
    { "a", "A",  1, 0,  1,     1,  0 },
    { "a", "A",  1, 0,  2,     1,  0 },
    { "a", "A",  1, 0,  NPOS,  1,  0 },

    { "A", "a",  -1, 0,  0,     -1, -1 },
    { "A", "a",  -1, 0,  1,     -1,  0 },
    { "A", "a",  -1, 0,  2,     -1,  0 },
    { "A", "a",  -1, 0,  NPOS,  -1,  0 },

    { "ba", "ba1",  -1, -1,  0,     -1, -1 },
    { "ba", "ba1",  -1, -1,  1,     -1, -1 },
    { "ba", "ba1",  -1, -1,  2,     -1, -1 },
    { "bA", "ba",   -1,  0,  2,     -1,  0 },
    { "ba", "ba1",  -1, -1,  3,     -1, -1 },
    { "ba", "ba1",  -1, -1,  NPOS,  -1, -1 },

    { "ba1", "ba",  1, 1,  0,    -1, -1 },
    { "ba1", "ba",  1, 1,  1,    -1, -1 },
    { "ba1", "ba",  1, 1,  2,     0,  0 },
    { "ba",  "bA",  1, 0,  2,     1,  0 },
    { "ba1", "ba",  1, 1,  3,     1,  1 },
    { "ba1", "ba",  1, 1,  NPOS,  1,  1 },
    { "ba1", "ba",  1, 1,  NPOS,  1,  1 }
};

BOOST_AUTO_TEST_CASE(s_Compare)
{
    const size_t count = sizeof(s_StrCompare) / sizeof(s_StrCompare[0]);

    for (size_t i = 0;  i < count;  i++) 
    {
        const SStrCompare* rec = &s_StrCompare[i];

        string s1 = rec->s1;
        s_CompareStr(NStr::Compare(s1, rec->s2, NStr::eCase), rec->case_res);
        s_CompareStr(NStr::Compare(s1, rec->s2, NStr::eNocase), rec->nocase_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, rec->s2, NStr::eCase), rec->n_case_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, rec->s2, NStr::eNocase), rec->n_nocase_res);

        string s2 = rec->s2;
        s_CompareStr(NStr::Compare(s1, s2, NStr::eCase), rec->case_res);
        s_CompareStr(NStr::Compare(s1, s2, NStr::eNocase), rec->nocase_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, s2, NStr::eCase), rec->n_case_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, s2, NStr::eNocase), rec->n_nocase_res);
    }

    BOOST_CHECK(NStr::Compare("0123", 0, 2, "12") <  0);
    BOOST_CHECK(NStr::Compare("0123", 1, 2, "12") == 0);
    BOOST_CHECK(NStr::Compare("0123", 2, 2, "12") >  0);
    BOOST_CHECK(NStr::Compare("0123", 3, 2,  "3") == 0);

    // std::string/CTempString with zero symbols inside

    string zs1{ 't', 'e', 0, 's', 't', 0 };
    string zs2("te");

    BOOST_CHECK( zs1.compare(zs2)                         >  0 );
    BOOST_CHECK( NStr::Compare(zs1, zs2)                  >  0 );
    BOOST_CHECK( NStr::CompareCase(zs1, zs2)              >  0 );
    BOOST_CHECK( NStr::CompareNocase(zs1, zs2)            >  0 );
    BOOST_CHECK( NStr::Compare(zs2, zs1)                  <  0 );
    BOOST_CHECK( NStr::Compare(zs1, 0, zs1.length(), zs2) >  0 );
    BOOST_CHECK( NStr::Compare(zs1, 0, zs2.length(), zs2) == 0 );
    BOOST_CHECK( NStr::Compare(zs1.data(), zs2.data())    == 0 );  // char*
}

BOOST_AUTO_TEST_CASE(s_XCompare)
{
    const size_t count = sizeof(s_StrCompare) / sizeof(s_StrCompare[0]);

    for (size_t i = 0;  i < count;  i++)
    {
        const SStrCompare* rec = &s_StrCompare[i];

        string s1 = rec->s1;
        s_CompareStr(XStr::Compare(s1, rec->s2, XStr::eCase), rec->case_res);
        s_CompareStr(XStr::Compare(s1, rec->s2, XStr::eNocase), rec->nocase_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, rec->s2, XStr::eCase), rec->n_case_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, rec->s2, XStr::eNocase), rec->n_nocase_res);

        string s2 = rec->s2;
        s_CompareStr(XStr::Compare(s1, s2, XStr::eCase), rec->case_res);
        s_CompareStr(XStr::Compare(s1, s2, XStr::eNocase), rec->nocase_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, s2, XStr::eCase), rec->n_case_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, s2, XStr::eNocase), rec->n_nocase_res);
    }

    BOOST_CHECK(XStr::Compare("0123", 0, 2, "12") <  0);
    BOOST_CHECK(XStr::Compare("0123", 1, 2, "12") == 0);
    BOOST_CHECK(XStr::Compare("0123", 2, 2, "12") >  0);
    BOOST_CHECK(XStr::Compare("0123", 3, 2,  "3") == 0);
}


//----------------------------------------------------------------------------
// NStr::Split() -- delimiters test
//----------------------------------------------------------------------------

static const char* s_SplitStr[] = {
    "abc",
    "---",
    "ab+cd+ef",
    "aaAAabBbbb",
    "-abc-def--ghijk---",
    "a12c3ba45acb678bc",
    "nodelim",
    "",
    "emptydelim",
    ";"
};

static const char* s_SplitDelim[] = {
    "def", "-", "+", "AB", "-", "abc", "*", "*", "", ";"
};

static const char* s_SplitRes[] =
{
    // merge delimiters -- NStr::fSplit_MergeDelimiters
    "abc",
    "", "",
    "ab", "cd", "ef",
    "aa", "ab", "bbb",
    "", "abc", "def", "ghijk", "",
    "", "12", "3", "45", "678", "", 
    "nodelim",
    "emptydelim",
    "", "",

    // no merge delimiters -- default
    "abc",
    "", "", "", "",
    "ab", "cd", "ef",
    "aa", "", "ab", "bbb",
    "", "abc", "def", "", "ghijk", "", "", "",
    "", "12", "3", "", "45", "", "", "678", "", "",
    "nodelim",
    "emptydelim", 
    "", ""
};

BOOST_AUTO_TEST_CASE(s_Split)
{
    list<string> split;
    size_t count = sizeof(s_SplitStr) / sizeof(s_SplitStr[0]);

    for (size_t i = 0; i < count; i++) {
        NStr::Split(s_SplitStr[i], s_SplitDelim[i], split, NStr::fSplit_MergeDelimiters);
    }
    for (size_t i = 0; i < count; i++) {
        NStr::Split(s_SplitStr[i], s_SplitDelim[i], split, 0); // default
    }
    size_t j = 0;
    ITERATE(list<string>, it, split) {
        BOOST_REQUIRE(j < sizeof(s_SplitRes) / sizeof(s_SplitRes[0]));
        BOOST_CHECK(NStr::Compare(*it, s_SplitRes[j++]) == 0);
    }
}


//----------------------------------------------------------------------------
// NStr::Split()
//----------------------------------------------------------------------------

struct SSplit
{
    NStr::TSplitFlags flags;
    const char*       str;
    const char*       delim;
    const char*       expected;
};

static const SSplit s_SplitTest[] = 
{
    { 0,
            "one, two ", ", ",
            "0: one, 4: , 5: two, 9: " },
    { NStr::fSplit_MergeDelimiters,
            "one, two ", ", ",
            "0: one, 5: two, 9: " },
    { NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate,
            "one, two ", ", ",
            "0: one, 5: two" },

    { NStr::fSplit_Truncate_Begin,
            "---a----b-c---", "-",
            "3: a, 5: , 6: , 7: , 8: b, 10: c, 12: , 13: , 14: " },
    { NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate_Begin,
            "---a----b-c---", "-",
            "3: a, 8: b, 10: c, 12: " },
    { NStr::fSplit_Truncate_End,
            "---a----b-c---", "-",
            "0: , 1: , 2: , 3: a, 5: , 6: , 7: , 8: b, 10: c" },
    { NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate_End,
            "---a----b-c---", "-",
            "0: , 3: a, 8: b, 10: c" },
    { NStr::fSplit_Truncate,
            "---a----b-c---", "-",
            "3: a, 5: , 6: , 7: , 8: b, 10: c" },
    { NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate,
            "---a----b-c---", "-",
            "3: a, 8: b, 10: c" },

    { NStr::fSplit_CanEscape,       
            "asdf jkl\\", " ", "" }, // throws
    { NStr::fSplit_CanEscape | NStr::fSplit_ByPattern,
            "\\x;y z\\; a'bc\\\\; de\"f;\\ ghi;", 
            "; ",
            "0: x;y z; a'bc\\, 17: de\"f; ghi;" },
    { NStr::fSplit_CanSingleQuote | NStr::fSplit_MergeDelimiters,
            "'abc'' def' g\\hi, jk\"l",
            ", ",
            "0: abc' def, 12: g\\hi, 18: jk\"l" },
    { NStr::fSplit_CanSingleQuote | NStr::fSplit_ByPattern,
            "It's a trap!", ", ", "" }, // throws
    { NStr::fSplit_CanSingleQuote | NStr::fSplit_CanEscape,
            "It\\'s... 'Monty Python''s' \"Flying\" Circus\\!",
            " ",
            "0: It's..., 9: Monty Python's, 27: \"Flying\", 36: Circus!" },
    { NStr::fSplit_CanSingleQuote | NStr::fSplit_CanEscape  | NStr::fSplit_ByPattern | NStr::fSplit_MergeDelimiters,
            "<><>That\\'s<>s<'> n<'>t<><all>,<><>'f<>lks'", 
            "<>",
            "0: , 4: That's, 13: s<> n<>t, 25: <all>,, 35: f<>lks" },
    { NStr::fSplit_CanSingleQuote | NStr::fSplit_CanEscape  | NStr::fSplit_ByPattern /* + no merge delimiters */,
            "<><>That\\'s<>s<'> n<'>t<><all>,<><>'f<>lks'", 
            "<>",
            "0: , 2: , 4: That's, 13: s<> n<>t, 25: <all>,, 33: , 35: f<>lks" },
    { NStr::fSplit_CanDoubleQuote, "\"Forget something?", " ", "" }, // throws
    { NStr::fSplit_CanDoubleQuote | NStr::fSplit_ByPattern,
            "I said\\, \"\"\"Time's up, everyone!\"\"\"",
            ", ",
            "0: I said\\, 9: \"Time's up, everyone!\"" },
    { NStr::fSplit_CanDoubleQuote | NStr::fSplit_CanSingleQuote,
            " \"ne'st\" '\\eg\"gs'",
            " ",
            "0: , 1: ne'st, 9: \\eg\"gs" },
    { NStr::fSplit_CanDoubleQuote | NStr::fSplit_CanSingleQuote /* + no merge delimiters */,
            " \"ne'st\" '\\eg\"gs'", 
            " ",
            "0: , 1: ne'st, 9: \\eg\"gs" },
    { NStr::fSplit_CanQuote | NStr::fSplit_CanEscape | NStr::fSplit_ByPattern | NStr::fSplit_MergeDelimiters,
            "abc\\, def, \"gh'i\"\", j\\\"kl\", 'm\"no, p''qr\\'s', ", 
            ", ",
            "0: abc, def, 11: gh'i\", j\"kl, 28: m\"no, p'qr's, 46: " }
};

BOOST_AUTO_TEST_CASE(s_Split_Flags)
{
    vector<CTempStringEx> v;
    vector<SIZE_TYPE>     token_pos;
    string                s;

    size_t count = (sizeof(s_SplitTest) / sizeof(s_SplitTest[0]));

    for (size_t i = 0; i < count; i++) 
    {
        const SSplit& data = s_SplitTest[i];

        CTempString_Storage storage;
        v.clear();
        token_pos.clear();
        try {
            NStr::Split(data.str, data.delim, v, data.flags, &token_pos, &storage);
            BOOST_REQUIRE_EQUAL(v.size(), token_pos.size());
            CNcbiOstrstream oss;
            const char* sep = "";
            for (size_t j = 0;  j < v.size();  ++j) {
                oss << sep << token_pos[j] << ": " << v[j];
                sep = ", ";
            }
            s = CNcbiOstrstreamToString(oss);
        } catch (CStringException&) {
            s.clear();
        }
        BOOST_CHECK_EQUAL(s, data.expected);
    }
}


//----------------------------------------------------------------------------
// NStr::SplitInTwo()
//----------------------------------------------------------------------------

struct SSplitInTwo {
    const char*       str;
    const char*       delim;
    NStr::TSplitFlags flags;
    const char*       expected_str1;
    const char*       expected_str2;
    bool              expected_ret;
};

static const SSplitInTwo s_SplitInTwoTest[] =
{
    { "ab+cd+ef",    "+",      0,                            "ab", "cd+ef",     true  },
    { "ab+cd+ef",    "+",      NStr::fSplit_MergeDelimiters, "ab", "cd+ef",     true  },
    { "ab+++cd+ef",  "+",      NStr::fSplit_MergeDelimiters, "ab", "cd+ef",     true  },
    { "+++ab+cd",    "+",      0,                            "",   "++ab+cd",   true  },
    { "+++ab+cd",    "+",      NStr::fSplit_MergeDelimiters, "",   "ab+cd",     true  },
    { "+++ab+cd",    "+",      NStr::fSplit_Truncate_Begin,  "ab", "cd",        true  },
    { "+++ab+cd",    "+",      NStr::fSplit_Truncate_End,    "",   "++ab+cd",   true  }, // no effect
    { "+++ab+cd",    "+",      NStr::fSplit_Truncate,        "ab", "cd",        true  },
    { "ab+++",       "+",      0,                            "ab", "++",        true  },
    { "ab+++",       "+",      NStr::fSplit_MergeDelimiters, "ab", "",          true  },
    { "ab+++",       "+",      NStr::fSplit_Truncate_End,    "ab", "++",        true  }, // no effect
    { "ab+++",       "+",      NStr::fSplit_Truncate,        "ab", "++",        true  }, // no effect
    { "ab+++",       "+",      NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate,
                                                             "ab", "",          true  }, // no effect
    { "ab+cd++",     "+",      0,                            "ab", "cd++",      true  },
    { "ab+cd++",     "+",      NStr::fSplit_MergeDelimiters, "ab", "cd++",      true  }, // no effect
    { "ab+cd++",     "+",      NStr::fSplit_Truncate_End,    "ab", "cd++",      true  }, // no effect
    { "ab+cd++",     "+",      NStr::fSplit_Truncate,        "ab", "cd++",      true  }, // no effect
    { "ab+cd++",     "+",      NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate,
                                                             "ab", "cd++",      true  }, // no effect
    { "aaAAabBbbb",  "AB",     0,                            "aa", "AabBbbb",   true  },
    { "aaABAabBbbb", "AB",     NStr::fSplit_MergeDelimiters, "aa", "abBbbb",    true  },
    { "aaCAabBCbbb", "ABC",    0,                            "aa", "AabBCbbb",  true  },
    { "aaCAabBCbbb", "ABC",    NStr::fSplit_MergeDelimiters, "aa", "abBCbbb",   true  },
    { "-beg-delim-", "-",      0,                            "",   "beg-delim-",true  },
    { "-beg-delim-", "-",      NStr::fSplit_MergeDelimiters, "",   "beg-delim-",true  }, // no effect
    { "-beg-delim-", "-",      NStr::fSplit_Truncate_End,    "",   "beg-delim-",true  }, // no effect
    { "-beg-delim-", "-",      NStr::fSplit_Truncate_Begin,  "beg","delim-",    true  },
    { "-beg-delim-", "-",      NStr::fSplit_Truncate,        "beg","delim-",    true  },
    { "end-delim:",  ":",      0,                            "end-delim",  "",  true  },
    { "end-delim:",  ":",      NStr::fSplit_Truncate_End,    "end-delim",  "",  true  }, // no effect
    { "end-delim:",  ":",      NStr::fSplit_Truncate,        "end-delim",  "",  true  }, // no effect
    { "nodelim",     ".,:;-+", 0,                            "nodelim",    "",  false },
    { "emptydelim",  "",       0,                            "emptydelim", "",  false },
    { "", "emtpystring",       0,                            "", "",            false },
    { "", "",                  0,                            "", "",            false },
    { "a b:c: d",    ": ",     NStr::fSplit_MergeDelimiters, "a",     "b:c: d", true  },
    { "a b:c: d",    ": ",     NStr::fSplit_ByPattern,       "a b:c", "d",      true  },
    { "abc\\\\:",    ":",      NStr::fSplit_CanEscape,       "abc\\", "",       true  },
    { "abc\\:",      ":",      NStr::fSplit_CanEscape,       "abc:", "",        false },
    { "abc\\\\: ",   ": ",     NStr::fSplit_CanEscape | NStr::fSplit_ByPattern,
                                                             "abc\\", "",       true  },
    { "abc\\: ",     ": ",     NStr::fSplit_CanEscape | NStr::fSplit_ByPattern,
                                                             "abc: ", "",       false },
    { "'abc':",      ":",      NStr::fSplit_CanSingleQuote,  "abc",  "",        true  },
    { "'abc:'",      ":",      NStr::fSplit_CanSingleQuote,  "abc:", "",        false }
};

BOOST_AUTO_TEST_CASE(s_SplitInTwo)
{
    CTempStringEx str1, str2;
    size_t count = (sizeof(s_SplitInTwoTest) / sizeof(s_SplitInTwoTest[0]));

    for (size_t i = 0; i < count; i++) 
    {
        const SSplitInTwo& data = s_SplitInTwoTest[i];
        CTempString_Storage storage;
        bool result = NStr::SplitInTwo(data.str, data.delim, str1, str2,
                                       data.flags, &storage);
        BOOST_CHECK_EQUAL(data.expected_ret, result);
        BOOST_CHECK_EQUAL(data.expected_str1, str1);
        BOOST_CHECK_EQUAL(data.expected_str2, str2);
    }
}


//----------------------------------------------------------------------------
// NStr::SplitByPattern()
//----------------------------------------------------------------------------

static const char* s_SplitPatternStr[] = {
    "<tag>",
    "<tag><tag><tag>",
    "begin<tag>",
    "begin<tag><tag>",
    "<tag><tag>end",
    "<tag>text<tag>",
    "<tag>begin<tag>text<tag><tag><tag>end<tag>"
};

static const char* s_SplitPatternRes[] =
{
    // merge delimiters -- NStr::fSplit_MergeDelimiters
    "", "",                         "#",
    "", "",                         "#",
    "begin", "",                    "#",
    "begin", "",                    "#",
    "", "end",                      "#",
    "", "text", "",                 "#",
    "", "begin", "text", "end", "", "#",

    // no merge delimiters -- default
    "", "",                         "#",
    "", "", "", "",                 "#",
    "begin", "",                    "#",
    "begin", "", "",                "#",
    "", "", "end",                  "#",
    "", "text", "",                 "#",
    "", "begin", "text", "", "", "end", "", "#"
};

BOOST_AUTO_TEST_CASE(s_SplitByPattern)
{
    const char* pattern = "<tag>";
    const char* stopper = "#";  // to correctly compare separate strings splitting

    vector<string> split;
    size_t count = sizeof(s_SplitPatternStr) / sizeof(s_SplitPatternStr[0]);

    for (size_t i = 0; i < count; i++) {
        NStr::SplitByPattern(s_SplitPatternStr[i], pattern, split, NStr::fSplit_MergeDelimiters);
        split.push_back(stopper);
    }
    for (size_t i = 0; i < count; i++) {
        NStr::SplitByPattern(s_SplitPatternStr[i], pattern, split); // default
        split.push_back(stopper);
    }
    size_t j = 0;
    ITERATE(vector<string>, it, split) {
        BOOST_REQUIRE(j < sizeof(s_SplitPatternRes) / sizeof(s_SplitPatternRes[0]));
        BOOST_CHECK(NStr::Compare(*it, s_SplitPatternRes[j++]) == 0);
    }
}


//----------------------------------------------------------------------------
// NStr::ToLower/ToUpper()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_Case)
{
    static const struct {
        const char* orig;
        const char* x_lower;
        const char* x_upper;
    } s_Tri[] = {
        { "", "", "" },
        { "a", "a", "A" },
        { "4", "4", "4" },
        { "B5a", "b5a", "B5A" },
        { "baObaB", "baobab", "BAOBAB" },
        { "B", "b", "B" },
        { "B", "b", "B" }
    };
    static const char s_Indiff[] =
        "#@+_)(*&^%/?\"':;~`'\\!\v|=-0123456789.,><{}[]\t\n\r";

    {{
        BOOST_CHECK( NStr::IsLower(""));
        BOOST_CHECK( NStr::IsUpper(""));
        BOOST_CHECK( NStr::IsLower("123 .,-"));
        BOOST_CHECK( NStr::IsUpper("123 .,-"));
        BOOST_CHECK( NStr::IsLower("123 a.,-"));
        BOOST_CHECK(!NStr::IsUpper("123 a.,-"));
        BOOST_CHECK(!NStr::IsLower("123 A.,-"));
        BOOST_CHECK( NStr::IsUpper("123 A.,-"));
     }}

    {{
        char indiff[sizeof(s_Indiff) + 1];
        ::strcpy(indiff, s_Indiff);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, indiff), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
        BOOST_CHECK(NStr::IsLower(indiff));
        ::strcpy(indiff, s_Indiff);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToUpper(indiff)), 0);
        BOOST_CHECK(NStr::IsUpper(indiff));
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
        BOOST_CHECK(NStr::IsLower(indiff));
    }}
    {{
        string indiff;
        indiff = s_Indiff;
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, indiff), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
        BOOST_CHECK(NStr::IsLower(indiff));
        indiff = s_Indiff;
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToUpper(indiff)), 0);
        BOOST_CHECK(NStr::IsUpper(indiff));
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
        BOOST_CHECK(NStr::IsLower(indiff));
    }}

    for (size_t i = 0;  i < sizeof(s_Tri) / sizeof(s_Tri[0]);  i++) {
        BOOST_CHECK_EQUAL(NStr::Compare(s_Tri[i].orig, s_Tri[i].x_lower, NStr::eNocase),0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Tri[i].orig, s_Tri[i].x_upper, NStr::eNocase),0);
        string orig = s_Tri[i].orig;
        BOOST_CHECK_EQUAL(NStr::Compare(orig, s_Tri[i].x_lower, NStr::eNocase), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(orig, s_Tri[i].x_upper, NStr::eNocase), 0);
        string x_lower = s_Tri[i].x_lower;
        {{
            char x_str[16];
            ::strcpy(x_str, s_Tri[i].orig);
            BOOST_CHECK(::strlen(x_str) < sizeof(x_str));
            BOOST_CHECK_EQUAL(NStr::Compare(NStr::ToLower(x_str), x_lower), 0);
            BOOST_CHECK(NStr::IsLower(x_str));
            ::strcpy(x_str, s_Tri[i].orig);
            BOOST_CHECK_EQUAL(NStr::Compare(NStr::ToUpper(x_str), s_Tri[i].x_upper),0);
            BOOST_CHECK(NStr::IsUpper(x_str));
            BOOST_CHECK_EQUAL(NStr::Compare(x_lower, NStr::ToLower(x_str)), 0);
            BOOST_CHECK(NStr::IsLower(x_str));
        }}
        {{
            string x_str;
            x_lower = s_Tri[i].x_lower;
            x_str = s_Tri[i].orig;
            BOOST_CHECK_EQUAL(NStr::Compare(NStr::ToLower(x_str), x_lower), 0);
            BOOST_CHECK(NStr::IsLower(x_str));
            x_str = s_Tri[i].orig;
            BOOST_CHECK_EQUAL(NStr::Compare(NStr::ToUpper(x_str), s_Tri[i].x_upper),0);
            BOOST_CHECK(NStr::IsUpper(x_str));
            BOOST_CHECK_EQUAL(NStr::Compare(x_lower, NStr::ToLower(x_str)), 0);
            BOOST_CHECK(NStr::IsLower(x_str));
        }}
    }
}


//----------------------------------------------------------------------------
// NStr::str[n]casecmp()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_strcasecmp)
{
    BOOST_CHECK_EQUAL(NStr::strncasecmp("ab", "a", 1), 0);
    BOOST_CHECK_EQUAL(NStr::strncasecmp("Ab", "a", 1), 0);
    BOOST_CHECK_EQUAL(NStr::strncasecmp("a", "Ab", 1), 0);
    BOOST_CHECK_EQUAL(NStr::strncasecmp("a", "ab", 1), 0);

    BOOST_CHECK_EQUAL(NStr::strcasecmp("a",  "A"), 0);
    BOOST_CHECK_EQUAL(NStr::strcasecmp("a",  "a"), 0);
    BOOST_CHECK(NStr::strcasecmp("ab", "a") != 0);
    BOOST_CHECK(NStr::strcasecmp("a", "ab") != 0);
    BOOST_CHECK(NStr::strcasecmp("a",   "") != 0);
    BOOST_CHECK(NStr::strcasecmp("",   "a") != 0);
    BOOST_CHECK_EQUAL(NStr::strcasecmp("",    ""), 0);
}


//----------------------------------------------------------------------------
// NStr::AStrEquiv()  &   NStr::Equal*()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_Equal)
{
    string as1("abcdefg ");
    string as2("abcdefg ");
    string as3("aBcdEfg ");
    string as4("lsekfu");

    BOOST_CHECK_EQUAL( AStrEquiv(as1, as2, PNocase()), true );
    BOOST_CHECK_EQUAL( AStrEquiv(as1, as3, PNocase()), true );
    BOOST_CHECK_EQUAL( AStrEquiv(as3, as4, PNocase()), false );
    BOOST_CHECK_EQUAL( AStrEquiv(as1, as2, PCase()),   true );
    BOOST_CHECK_EQUAL( AStrEquiv(as1, as3, PCase()),   false );
    BOOST_CHECK_EQUAL( AStrEquiv(as2, as4, PCase()),   false );

    BOOST_CHECK_EQUAL( NStr::EqualNocase(as1, as2),    true );
    BOOST_CHECK_EQUAL( NStr::EqualNocase(as1, as3),    true );
    BOOST_CHECK_EQUAL( NStr::EqualNocase(as3, as4),    false );
    BOOST_CHECK_EQUAL( NStr::EqualCase(as1, as2),      true );
    BOOST_CHECK_EQUAL( NStr::EqualCase(as1, as3),      false );
    BOOST_CHECK_EQUAL( NStr::EqualCase(as2, as4),      false );


    // std::string/CTempString with zero symbols inside

    string zs1{ 't', 'e', 0, 's', 't', 0 };
    string zs2("te");

    BOOST_CHECK_EQUAL( zs1 == zs2,                  false );
    BOOST_CHECK_EQUAL( NStr::Equal(zs1, zs2),       false );
    BOOST_CHECK_EQUAL( NStr::EqualCase(zs1, zs2),   false );
    BOOST_CHECK_EQUAL( NStr::EqualNocase(zs1, zs2), false );
    BOOST_CHECK_EQUAL( NStr::Equal(zs1, 0, zs1.length(), zs2), false );
    BOOST_CHECK_EQUAL( NStr::Equal(zs1, 0, zs2.length(), zs2), true  );
}


//----------------------------------------------------------------------------
// NStr::MatchesMask()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_MatchesMask)
{
    BOOST_CHECK(   NStr::MatchesMask( "aaa", "*a"                            ) );
    BOOST_CHECK( ! NStr::MatchesMask( "bbb", "*a"                            ) );
    BOOST_CHECK(   NStr::MatchesMask( "bba", "*a"                            ) );
    BOOST_CHECK( ! NStr::MatchesMask( "aab", "*a"                            ) );
    BOOST_CHECK(   NStr::MatchesMask( "aaa", "*a*"                           ) );
    BOOST_CHECK(   NStr::MatchesMask( "AAA", "*a",             NStr::eNocase ) );
    BOOST_CHECK(   NStr::MatchesMask( "aaa", "*"                             ) );
    BOOST_CHECK(   NStr::MatchesMask( "aaa", "[a-z][a]a"                     ) );
    BOOST_CHECK(   NStr::MatchesMask( "aaa", "[!b][!b-c]a"                   ) );
    BOOST_CHECK( ! NStr::MatchesMask( "aaa", "a[]"                           ) );
    BOOST_CHECK( ! NStr::MatchesMask( "aaa", "a[a-a][a-"                     ) );
    BOOST_CHECK(   NStr::MatchesMask( "aaa", "a[a-a][a-]"                    ) );
    BOOST_CHECK( ! NStr::MatchesMask( "a\\", "a\\"                           ) );
    BOOST_CHECK(   NStr::MatchesMask( "a\\", "a\\\\"                         ) );
    BOOST_CHECK(   NStr::MatchesMask( "a\\", "a[\\]"                         ) );
    BOOST_CHECK(   NStr::MatchesMask( "a\\", "a[\\\\]"                       ) );
    BOOST_CHECK( ! NStr::MatchesMask( "aaa", "[a-b][a-b][a-b"                ) );
    BOOST_CHECK(   NStr::MatchesMask( "a*a", "a\\*a"                         ) );
    BOOST_CHECK(   NStr::MatchesMask( "a[]", "[a-z][[][]]"                   ) );
    BOOST_CHECK( ! NStr::MatchesMask( "a[]", "[a-z][[][[\\]]"                ) );
    BOOST_CHECK( ! NStr::MatchesMask( "a!b", "[a-z][!][A-Z]",  NStr::eNocase ) );
    BOOST_CHECK(   NStr::MatchesMask( "a!b", "[a-z][!A-Z]b",   NStr::eNocase ) );
    BOOST_CHECK(   NStr::MatchesMask( "a!b", "[a-z][!][A-Z]b", NStr::eNocase ) );
    BOOST_CHECK(   NStr::MatchesMask( "a-b", "[a-z][0-][A-Z]", NStr::eNocase ) );
    BOOST_CHECK(   NStr::MatchesMask( "a-b", "[a-z][-9][A-Z]", NStr::eNocase ) );
}


//----------------------------------------------------------------------------
// Reference counting
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_ReferenceCounting)
{
    string s1(10, '1');
    string s2(s1);
    if ( s1.data() != s2.data() ) {
        LOG_POST("BAD: string reference counting is OFF");
    }
    else {
        LOG_POST("GOOD: string reference counting is ON");
        for (size_t i = 0; i < 4; i++) {
            LOG_POST("Restoring reference counting");
            s2 = s1;
            if ( s1.data() != s2.data() ) {
                LOG_POST("BAD: cannot restore string reference counting");
                continue;
            }
            LOG_POST("GOOD: reference counting is ON");

            const char* type = i&1? "str.begin()": "str.c_str()";
            LOG_POST("Calling " << type);
            if ( i&1 ) {
                std::ignore = s2.begin();
            }
            else {
                std::ignore = s1.c_str();
            }
            if ( s1.data() == s2.data() ) {
                LOG_POST("GOOD: " << type << " doesn't affect reference counting");
                continue;
            }
            LOG_POST("OK: "<< type << " turns reference counting OFF");

            LOG_POST("Restoring reference counting");
            s2 = s1;
            if ( s1.data() != s2.data() ) {
                LOG_POST("BAD: " << type << " turns reference counting OFF completely");
                continue;
            }
            LOG_POST("GOOD: reference counting is ON");

            if ( i&1 ) continue;

            LOG_POST("Calling " << type << " on source");
            std::ignore = s1.c_str();
            if ( s1.data() != s2.data() ) {
                LOG_POST("BAD: " << type << " on source turns reference counting OFF");
            }

            LOG_POST("Calling "<< type <<" on destination");
            std::ignore = s2.c_str();
            if ( s1.data() != s2.data() ) {
                LOG_POST("BAD: " << type << " on destination turns reference counting OFF");
            }
        }
    }
}


//----------------------------------------------------------------------------
// NStr::Find*()
//----------------------------------------------------------------------------

struct SFindStr {
    const char*  str;
    const char*  pattern;
    NStr::ECase  use_case;
    NStr::EDirection direction;
    SIZE_TYPE    occurence;
    SIZE_TYPE    result;
};

// Abbreviations to shorten tests description
#define f_CF  NStr::eCase,   NStr::eForwardSearch
#define f_CR  NStr::eCase,   NStr::eReverseSearch
#define f_NF  NStr::eNocase, NStr::eForwardSearch
#define f_NR  NStr::eNocase, NStr::eReverseSearch

static const SFindStr s_FindStrTest[] =
{
    // eCase + eForwardSearch
    { "bcbab",          "abc", f_CF, 0,  NPOS },
    { "abc",            "abc", f_CF, 0,  0    },
    { "abc++",          "abc", f_CF, 0,  0    },
    { "++abc",          "abc", f_CF, 0,  2    },
    { "ab",             "abc", f_CF, 0,  NPOS },
    { "+++abc++",       "abc", f_CF, 0,  3    },
    { "+abc+abc++abc+", "abc", f_CF, 0,  1    },
    { "+abc+abc++abc+", "abc", f_CF, 1,  5    },
    { "+abc+abc++abc+", "abc", f_CF, 2,  10   },
    { "+abc+abc++abc+", "abc", f_CF, 3,  NPOS },
    { "cabc+abc++abca", "abc", f_CF, 3,  NPOS },

    // eCase + eReverseSearch
    { "bcbab",          "abc", f_CR, 0,  NPOS },
    { "abc",            "abc", f_CR, 0,  0    },
    { "abc++",          "abc", f_CR, 0,  0    },
    { "++abc",          "abc", f_CR, 0,  2    },
    { "ab",             "abc", f_CR, 0,  NPOS },
    { "+++abc++",       "abc", f_CR, 0,  3    },
    { "+abc+abc++abc+", "abc", f_CR, 0,  10   },
    { "+abc+abc++abc+", "abc", f_CR, 1,  5    },
    { "+abc+abc++abc+", "abc", f_CR, 2,  1    },
    { "+abc+abc++abc+", "abc", f_CR, 3,  NPOS },
    { "cabc+abc++abca", "abc", f_CR, 3,  NPOS },

    // eNocase + eForwardSearch
    { "bcbab",          "ABC", f_NF, 0,  NPOS },
    { "abc",            "ABC", f_NF, 0,  0    },
    { "abc++",          "ABC", f_NF, 0,  0    },
    { "++abc",          "ABC", f_NF, 0,  2    },
    { "ab",             "ABC", f_NF, 0,  NPOS },
    { "+++abc++",       "ABC", f_NF, 0,  3    },
    { "+abc+abc++abc+", "ABC", f_NF, 0,  1    },
    { "+abc+abc++abc+", "ABC", f_NF, 1,  5    },
    { "+abc+abc++abc+", "ABC", f_NF, 2,  10   },
    { "+abc+abc++abc+", "ABC", f_NF, 3,  NPOS },
    { "cabc+abc++abca", "ABC", f_NF, 3,  NPOS },

    // eNocase + eReverseSearch
    { "bcbab",          "ABC", f_NR, 0,  NPOS },
    { "abc",            "ABC", f_NR, 0,  0    },
    { "abc++",          "ABC", f_NR, 0,  0    },
    { "++abc",          "ABC", f_NR, 0,  2    },
    { "ab",             "ABC", f_NR, 0,  NPOS },
    { "+++abc++",       "ABC", f_NR, 0,  3    },
    { "+abc+abc++abc+", "ABC", f_NR, 0,  10   },
    { "+abc+abc++abc+", "ABC", f_NR, 1,  5    },
    { "+abc+abc++abc+", "ABC", f_NR, 2,  1    },
    { "+abc+abc++abc+", "ABC", f_NR, 3,  NPOS },
    { "cabc+abc++abca", "ABC", f_NR, 3,  NPOS }
};


#ifdef NCBI_COARSE_DEPRECATION_WARNING_GRANULARITY
NCBI_SUSPEND_DEPRECATION_WARNINGS
#endif
BOOST_AUTO_TEST_CASE(s_Find)
{
    {
        const size_t count = sizeof(s_FindStrTest) / sizeof(s_FindStrTest[0]);
        for (size_t i = 0; i < count; i++) {
            const SFindStr& t = s_FindStrTest[i];
            BOOST_CHECK_EQUAL(NStr::Find(t.str, t.pattern, t.use_case, t.direction, t.occurence), t.result);
            /*
            if (NStr::Find(t.str, t.pattern, t.use_case, t.direction, t.occurence) != t.result) {
                cout << "s_FindStrTest [ " << i  << " ]"<< endl;
            }
            */
        }
    }

    // Backward compatibility test
    // @deprecated
    // TODO: change to Find() later.
    
#ifndef NCBI_COARSE_DEPRECATION_WARNING_GRANULARITY
NCBI_SUSPEND_DEPRECATION_WARNINGS
#endif
    BOOST_CHECK_EQUAL(NStr::FindCase  ("abcd", "xyz"),                           NPOS);
    BOOST_CHECK_EQUAL(NStr::FindCase  ("abcd", "xyz", 0, NPOS, NStr::eLast),     NPOS);
    BOOST_CHECK_EQUAL(NStr::FindNoCase("abcd", "xyz"),                           NPOS);
    BOOST_CHECK_EQUAL(NStr::FindNoCase("abcd", "xyz", 0, NPOS, NStr::eLast),     NPOS);
    BOOST_CHECK_EQUAL(NStr::FindCase  ("abcd", "aBc", 0, NPOS, NStr::eLast),     NPOS);
    BOOST_CHECK_EQUAL(NStr::FindNoCase("abcd", "aBc", 0, NPOS, NStr::eLast),     0U);
    BOOST_CHECK_EQUAL(NStr::Find("abc abc abc", "bc", 2, 8, NStr::eFirst, NStr::eCase), 5U);
    BOOST_CHECK_EQUAL(NStr::FindCase  ("abc abc abc", "bc", 2, 8, NStr::eFirst), 5U);
    BOOST_CHECK_EQUAL(NStr::FindCase  ("abc abc abc", "bc", 2, 8, NStr::eLast),  5U);
}
NCBI_RESUME_DEPRECATION_WARNINGS


BOOST_AUTO_TEST_CASE(s_FindWord)
{
    // NStr::eForwardSearch
    BOOST_CHECK_EQUAL(NStr::FindWord("abcd",    "xyz"), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("abcd",    "abc"), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("xabc",    "abc"), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("abc d",   "abc"), 0U  );
    BOOST_CHECK_EQUAL(NStr::FindWord("xabc d",  "abc"), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("x,abc:d", "abc"), 2U  );
    BOOST_CHECK_EQUAL(NStr::FindWord("x,abc",   "abc"), 2U  );
    BOOST_CHECK_EQUAL(NStr::FindWord("xabcx abc\ny abc,z", "abc"), 6U);

    // NStr::eReverseSearch
    BOOST_CHECK_EQUAL(NStr::FindWord("abcd",    "xyz", f_CR), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("abcd",    "abc", f_CR), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("xabc",    "abc", f_CR), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("ab abc",  "abc", f_CR), 3U);
    BOOST_CHECK_EQUAL(NStr::FindWord("x abcd",  "abc", f_CR), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("x,abc:d", "abc", f_CR), 2U);
    BOOST_CHECK_EQUAL(NStr::FindWord("x,abc",   "abc", f_CR), 2U);
    BOOST_CHECK_EQUAL(NStr::FindWord("xabcx abc\ny abc,z", "abc", f_CR), 12U);

    // NStr::eNocase
    BOOST_CHECK_EQUAL(NStr::FindWord("abcd",    "xyz", f_NF), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("abcd",    "ABC", f_NF), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("xabc",    "ABC", f_NF), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("abc d",   "ABC", f_NF), 0U  );
    BOOST_CHECK_EQUAL(NStr::FindWord("xabc d",  "ABC", f_NF), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("x,abc:d", "ABC", f_NF), 2U  );
    BOOST_CHECK_EQUAL(NStr::FindWord("x,abc",   "ABC", f_NF), 2U  );
    BOOST_CHECK_EQUAL(NStr::FindWord("xabcx abc\ny abc,z", "ABC", f_NF), 6U);

    BOOST_CHECK_EQUAL(NStr::FindWord("abcd",    "xyz", f_NR), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("abcd",    "ABC", f_NR), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("xabc",    "ABC", f_NR), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("ab abc",  "ABC", f_NR), 3U  );
    BOOST_CHECK_EQUAL(NStr::FindWord("x abcd",  "ABC", f_NR), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("x,abc:d", "ABC", f_NR), 2U  );
    BOOST_CHECK_EQUAL(NStr::FindWord("x,abc",   "ABC", f_NR), 2U  );
    BOOST_CHECK_EQUAL(NStr::FindWord("xabcx abc\ny abc,z", "ABC", f_NR), 12U);

    // "Word" with non-word characters
    BOOST_CHECK_EQUAL(NStr::FindWord("a b c",         "a b c"), 0U);
    BOOST_CHECK_EQUAL(NStr::FindWord(" a b c ",       "a b c"), 1U);
    BOOST_CHECK_EQUAL(NStr::FindWord("  a b c  ",     "a b c"), 2U);
    BOOST_CHECK_EQUAL(NStr::FindWord("x a b c y",     "a b c"), 2U);
    BOOST_CHECK_EQUAL(NStr::FindWord("a b a a b c d", "a b c"), 6U);
    BOOST_CHECK_EQUAL(NStr::FindWord("a b 1a b c d",  "a b c"), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("x abc x",       " abc "), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("x abc abc x",   " abc "), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("x abc  abc x",  " abc "), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("x abc  abc ",   " abc "), 6U);
    BOOST_CHECK_EQUAL(NStr::FindWord("x abc  abc  x", " abc "), 6U);
    BOOST_CHECK_EQUAL(NStr::FindWord("x  abc abc  x", " abc "), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindWord("x  abc+ abc +", " abc "), 7U);
}


//----------------------------------------------------------------------------
// CVersionInfo:: parse from str
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_VersionInfo)
{
    {{
        CVersionInfo ver("1.2.3");
        BOOST_CHECK_EQUAL(ver.GetMajor(), 1);
        BOOST_CHECK_EQUAL(ver.GetMinor(), 2);
        BOOST_CHECK_EQUAL(ver.GetPatchLevel(), 3);

        ver.FromStr("12.35");
        BOOST_CHECK_EQUAL(ver.GetMajor(), 12);
        BOOST_CHECK_EQUAL(ver.GetMinor(), 35);
        BOOST_CHECK_EQUAL(ver.GetPatchLevel(), 0);

        BOOST_CHECK_THROW( ver.FromStr("12.35a"), exception);
    }}

    // ParseVersionString tests...

    static const struct {
        const char* str;
        const char* name;
        int         ver_major;
        int         ver_minor;
        int         patch_level;
    } s_VerInfo[] = {
        { "1.3.2",                      "",                  1, 3, 2 },
        { "My_Program21p32c 1.3.3",     "My_Program21p32c",  1, 3, 3 },
        { "2.3.4 ( program)",           "program",           2, 3, 4 },
        { "version 50.1.0",             "",                 50, 1, 0 },
        { "MyProgram version 50.2.1",   "MyProgram",        50, 2, 1 },
        { "MyProgram ver. 50.3.1",      "MyProgram",        50, 3, 1 },
        { "MyOtherProgram2 ver 51.3.1", "MyOtherProgram2",  51, 3, 1 },
        { "Program_ v. 1.3.1",          "Program_",          1, 3, 1 }
    };

    CVersionInfo ver("1.2.3");
    string       name;

    for (size_t i = 0;  i < sizeof(s_VerInfo) / sizeof(s_VerInfo[0]);  i++) {
        ParseVersionString(s_VerInfo[i].str, &name, &ver);
        BOOST_CHECK_EQUAL(name,                s_VerInfo[i].name);
        BOOST_CHECK_EQUAL(ver.GetMajor(),      s_VerInfo[i].ver_major);
        BOOST_CHECK_EQUAL(ver.GetMinor(),      s_VerInfo[i].ver_minor);
        BOOST_CHECK_EQUAL(ver.GetPatchLevel(), s_VerInfo[i].patch_level);
    }
    ParseVersionString("MyProgram ", &name, &ver);
    BOOST_CHECK_EQUAL(name, string("MyProgram"));
    BOOST_CHECK(ver.IsAny());
}

    const unsigned char s_ExtAscii[] = {
        0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff,0x00};
    const unsigned char s_Converted[] = {
0xc2,0x80,0xc2,0x81,0xc2,0x82,0xc2,0x83,0xc2,0x84,0xc2,0x85,0xc2,0x86,0xc2,0x87,
0xc2,0x88,0xc2,0x89,0xc2,0x8a,0xc2,0x8b,0xc2,0x8c,0xc2,0x8d,0xc2,0x8e,0xc2,0x8f,
0xc2,0x90,0xc2,0x91,0xc2,0x92,0xc2,0x93,0xc2,0x94,0xc2,0x95,0xc2,0x96,0xc2,0x97,
0xc2,0x98,0xc2,0x99,0xc2,0x9a,0xc2,0x9b,0xc2,0x9c,0xc2,0x9d,0xc2,0x9e,0xc2,0x9f,
0xc2,0xa0,0xc2,0xa1,0xc2,0xa2,0xc2,0xa3,0xc2,0xa4,0xc2,0xa5,0xc2,0xa6,0xc2,0xa7,
0xc2,0xa8,0xc2,0xa9,0xc2,0xaa,0xc2,0xab,0xc2,0xac,0xc2,0xad,0xc2,0xae,0xc2,0xaf,
0xc2,0xb0,0xc2,0xb1,0xc2,0xb2,0xc2,0xb3,0xc2,0xb4,0xc2,0xb5,0xc2,0xb6,0xc2,0xb7,
0xc2,0xb8,0xc2,0xb9,0xc2,0xba,0xc2,0xbb,0xc2,0xbc,0xc2,0xbd,0xc2,0xbe,0xc2,0xbf,
0xc3,0x80,0xc3,0x81,0xc3,0x82,0xc3,0x83,0xc3,0x84,0xc3,0x85,0xc3,0x86,0xc3,0x87,
0xc3,0x88,0xc3,0x89,0xc3,0x8a,0xc3,0x8b,0xc3,0x8c,0xc3,0x8d,0xc3,0x8e,0xc3,0x8f,
0xc3,0x90,0xc3,0x91,0xc3,0x92,0xc3,0x93,0xc3,0x94,0xc3,0x95,0xc3,0x96,0xc3,0x97,
0xc3,0x98,0xc3,0x99,0xc3,0x9a,0xc3,0x9b,0xc3,0x9c,0xc3,0x9d,0xc3,0x9e,0xc3,0x9f,
0xc3,0xa0,0xc3,0xa1,0xc3,0xa2,0xc3,0xa3,0xc3,0xa4,0xc3,0xa5,0xc3,0xa6,0xc3,0xa7,
0xc3,0xa8,0xc3,0xa9,0xc3,0xaa,0xc3,0xab,0xc3,0xac,0xc3,0xad,0xc3,0xae,0xc3,0xaf,
0xc3,0xb0,0xc3,0xb1,0xc3,0xb2,0xc3,0xb3,0xc3,0xb4,0xc3,0xb5,0xc3,0xb6,0xc3,0xb7,
0xc3,0xb8,0xc3,0xb9,0xc3,0xba,0xc3,0xbb,0xc3,0xbc,0xc3,0xbd,0xc3,0xbe,0xc3,0xbf};


BOOST_AUTO_TEST_CASE(s_CUtf8)
{
    const char *src, *res, *conv;
    src = (char*)s_ExtAscii;
    conv= (char*)s_Converted;
    // extended ASCII does not match any encoding
    {
        CStringUTF8 u1( CUtf8::AsUTF8(src,eEncoding_ISO8859_1,CUtf8::eNoValidate) );
        CStringUTF8 u2 = CUtf8::AsUTF8(src,eEncoding_ISO8859_1,CUtf8::eNoValidate);
    }

    CStringUTF8 u8str( CUtf8::AsUTF8(src,eEncoding_ISO8859_1,CUtf8::eNoValidate) );
    res = u8str.c_str();

    BOOST_CHECK_EQUAL( strncmp(src,res,126), 0);
    BOOST_CHECK( strlen(res+127) == 256 );
    res += 127;
    BOOST_CHECK_EQUAL( strncmp(conv,res,256), 0);

    string sample("micro=\265 Agrave=\300 atilde=\343 ccedil=\347");
    string u8sample("micro=\302\265 Agrave=\303\200 atilde=\303\243 ccedil=\303\247");

    u8str = CUtf8::AsUTF8( sample, eEncoding_ISO8859_1);
    u8str = CUtf8::AsUTF8( sample, eEncoding_ISO8859_1, CUtf8::eValidate);

#if defined(HAVE_WSTRING)
    wstring wss, wss2;
#else
    TStringUnicode wss, wss2;
#endif
    string ssab("ab");
    wss.append(1,0x61).append(1,0x62).append(1,0x0).append(1,0x63);
    wss2.append(1,0x61).append(1,0x62);
    {
// string to string
        string u8test( CUtf8::AsUTF8(sample, eEncoding_ISO8859_1));
        BOOST_CHECK_EQUAL(u8test,u8sample);

        string u8test2( CUtf8::AsUTF8( sample.c_str(),eEncoding_ISO8859_1));
        BOOST_CHECK_EQUAL(u8test2,u8sample);

        string u8test3( CUtf8::AsUTF8( CTempString(sample.c_str(),sample.size()),eEncoding_ISO8859_1));
        BOOST_CHECK_EQUAL(u8test3,u8sample);

// should not compile!
//        u8test = CUtf8::AsUTF8( sample.c_str(), 1);
//        u8test = CUtf8::AsUTF8( sample);
//        u8test = CUtf8::AsBasicString<char>(sample);

        
        u8test.clear();
        CUtf8::AppendAsUTF8(u8test,ssab, eEncoding_ISO8859_1);
        CUtf8::AppendAsUTF8(u8test, CTempString(ssab.data(), ssab.size()), eEncoding_ISO8859_1);
        CUtf8::AppendAsUTF8(u8test,ssab[0], eEncoding_ISO8859_1);
        CUtf8::AppendAsUTF8(u8test, 'a', eEncoding_ISO8859_1);
        CUtf8::AppendAsUTF8(u8test, TUnicodeSymbol(0x62));
        BOOST_CHECK_EQUAL(u8test,"ababaab");

// should not compile!
//        CUtf8::AppendAsUTF8(u8test, 'a');
//        CUtf8::AppendAsUTF8(u8test,ssab);
//        CUtf8::AppendAsUTF8(u8test,ssab.data(), ssab.size());
//        CUtf8::AppendAsUTF8(u8test,ssab[0]);
    }
    {
// wide string to string
        string w8test(CUtf8::AsUTF8(wss));
        BOOST_CHECK_EQUAL(w8test.size(),4U);
        w8test = CUtf8::AsUTF8(wss);
        BOOST_CHECK_EQUAL(w8test.size(),4U);
        w8test = CUtf8::AsUTF8(wss.c_str());
        BOOST_CHECK_EQUAL(w8test.size(),2U);

        string w8test2( CUtf8::AsUTF8(wss.c_str()));
        BOOST_CHECK_EQUAL(w8test2,ssab);
        BOOST_CHECK_EQUAL(w8test2.size(),2U);

        string w8test3( CUtf8::AsUTF8( wss.c_str(), wss.size()));
        BOOST_CHECK_EQUAL(w8test3.size(),wss.size());

        string ss = CUtf8::AsSingleByteString(w8test2,eEncoding_UTF8);
        ss = CUtf8::AsSingleByteString(w8test2,eEncoding_Ascii);
        BOOST_CHECK_EQUAL(ss,ssab);

        w8test.clear();
        CUtf8::AppendAsUTF8(w8test,wss2);
        CUtf8::AppendAsUTF8(w8test, wss2.data(), wss2.size());
        CUtf8::AppendAsUTF8(w8test,wss2[0]);
        BOOST_CHECK_EQUAL(w8test,"ababa");
    }
    {
// string or wide string to CStringUTF8
        CStringUTF8 w8test;
        w8test = CUtf8::AsUTF8( sample, eEncoding_ISO8859_1);
        w8test = CUtf8::AsUTF8( sample.c_str(), eEncoding_ISO8859_1);

        w8test = CUtf8::AsUTF8(wss);
        BOOST_CHECK_EQUAL(w8test.size(),4U);
        w8test = CUtf8::AsUTF8(wss.c_str());
        BOOST_CHECK_EQUAL(w8test.size(),2U);
        w8test = CUtf8::AsUTF8(wss.c_str(), wss.size());
        BOOST_CHECK_EQUAL(w8test.size(),4U);
        w8test = CUtf8::AsUTF8(wss.c_str());
        BOOST_CHECK_EQUAL(w8test.size(),2U);
        w8test += CUtf8::AsUTF8(wss.c_str());
        BOOST_CHECK_EQUAL(w8test.size(),4U);
    }

    BOOST_CHECK_EQUAL(u8str,u8sample);
    BOOST_CHECK_EQUAL( CUtf8::AsSingleByteString( u8str, eEncoding_ISO8859_1), sample);

    BOOST_CHECK_EQUAL(128u, CUtf8::GetValidSymbolCount(CTempString((const char*)s_Converted, sizeof(s_Converted))));
    BOOST_CHECK_EQUAL(127u, CUtf8::GetValidSymbolCount((const char*)s_ExtAscii));
    BOOST_CHECK_EQUAL(34u,  CUtf8::GetValidSymbolCount(CTempString(u8str.data(), u8str.length())));
    BOOST_CHECK_EQUAL(34u,  CUtf8::GetValidSymbolCount(u8str));

    BOOST_CHECK_EQUAL(256u, CUtf8::GetValidBytesCount(CTempString((const char*)s_Converted, sizeof(s_Converted))));
    BOOST_CHECK_EQUAL(127u, CUtf8::GetValidBytesCount((const char*)s_ExtAscii));
    BOOST_CHECK_EQUAL(38u,  CUtf8::GetValidBytesCount(CTempString(u8str.data(), u8str.length())));
    BOOST_CHECK_EQUAL(38u,  CUtf8::GetValidBytesCount(u8str));

    BOOST_CHECK_EQUAL( (int)CUtf8::StringToEncoding("UtF-8"),           (int)eEncoding_UTF8);
    BOOST_CHECK_EQUAL( (int)CUtf8::StringToEncoding("Windows-1252"),    (int)eEncoding_Windows_1252);
    BOOST_CHECK_EQUAL( (int)CUtf8::StringToEncoding("cp367"),           (int)eEncoding_Ascii);
    BOOST_CHECK_EQUAL( (int)CUtf8::StringToEncoding("csISOLatin1"),     (int)eEncoding_ISO8859_1);
    BOOST_CHECK_EQUAL( (int)CUtf8::StringToEncoding("ISO-2022-CN-EXT"), (int)eEncoding_Unknown);

    TStringUnicode uus = CUtf8::AsBasicString<TUnicodeSymbol>(u8sample);
    TStringUCS4    u4s = CUtf8::AsBasicString<TCharUCS4>(u8sample);
    TStringUCS2    u2s = CUtf8::AsBasicString<TCharUCS2>(u8sample);
#if defined(HAVE_WSTRING)
    wstring        uws = CUtf8::AsBasicString<wchar_t>(u8sample);
#endif

// other types
#if defined(HAVE_WSTRING)
    {
        typedef wchar_t xxxMywchar;
        basic_string<xxxMywchar> xxxwss;
        string w8test( CUtf8::AsUTF8(xxxwss));
        xxxwss = CUtf8::AsBasicString<xxxMywchar>(w8test);
        xxxwss = CUtf8::AsBasicString<xxxMywchar>(w8test, 0);
        xxxwss = CUtf8::AsBasicString<xxxMywchar>(w8test, 0, CUtf8::eNoValidate);

        xxxwss.append(1,1000);
        CUtf8::AppendAsUTF8(w8test,xxxwss);
        CUtf8::AppendAsUTF8(w8test,xxxwss.data(), xxxwss.size());
        CUtf8::AppendAsUTF8(w8test,xxxwss[0]);
    }
#endif
    {
        typedef char16_t xxxMywchar;
        basic_string<xxxMywchar> xxxwss;
        string w8test( CUtf8::AsUTF8(xxxwss));
        xxxwss = CUtf8::AsBasicString<xxxMywchar>(w8test);

        xxxwss.append(1,1000);
        CUtf8::AppendAsUTF8(w8test,xxxwss);
        CUtf8::AppendAsUTF8(w8test,xxxwss.data(), xxxwss.size());
        CUtf8::AppendAsUTF8(w8test,xxxwss[0]);
    }
    {
        typedef char32_t xxxMywchar;
        basic_string<xxxMywchar> xxxwss;
        string w8test( CUtf8::AsUTF8(xxxwss));
        xxxwss = CUtf8::AsBasicString<xxxMywchar>(w8test);

        xxxwss.append(1,1000);
        CUtf8::AppendAsUTF8(w8test,xxxwss);
        CUtf8::AppendAsUTF8(w8test,xxxwss.data(), xxxwss.size());
        CUtf8::AppendAsUTF8(w8test,xxxwss[0]);
    }
    // iteration
    {
        wss.erase().append(1,0x1000).append(1,0x1100).append(1,0x1110).append(1,0x1111);
        wss2.erase();
        string s(CUtf8::AsUTF8(wss));
        string s2;
        for (string::const_iterator i = s.begin(); i != s.end(); ++i) {
            TUnicodeSymbol sym = CUtf8::Decode(i);
            wss2.append(1,sym);
            s2 += CUtf8::AsUTF8(&sym,1);
        }
        BOOST_CHECK(s == s2);
        BOOST_CHECK(wss == wss2);
    }

    // wrong Utf8
    {
        string u8tmp("micro=\302 Agrave=\303\200 atilde=\303\243 ccedil=\303\247");
        string expected("micro=\\302 A");
        for (int i = 0; i < 3; ++i) {
            bool gotit = false;
            string msg;
            try {
                switch (i) {
                case 0: {
                    CUtf8::AsBasicString<TCharUCS2>(u8tmp, NULL, CUtf8::eValidate);
                } break;
                case 1: {
                    CUtf8::GetSymbolCount(u8tmp);
                } break;
                case 2: {
                    CUtf8::AsSingleByteString(u8tmp,eEncoding_Ascii, NULL, CUtf8::eValidate);
                } break;
                }
            } catch (CStringException& e) {
                gotit = true;
                msg = e.GetMsg();
            }
            BOOST_CHECK( gotit );
            BOOST_CHECK( NStr::FindCase(msg, expected) != NPOS);
        }
    }

// locales
#if defined(HAVE_WSTRING) && defined(NCBI_OS_MSWIN)
    {
        typedef char16_t xxxMywchar;
        basic_string<xxxMywchar> xxxwss, wstest;
        string u8, u8x, res;
        const char* lcl_name;
        {
            lcl_name = "ru-RU";
            try {
                locale lcl(lcl_name);
                string t("�������");
                wstest = {1050, 1086, 1084, 1072, 1085, 1076, 1099};
                u8 = CUtf8::AsUTF8(t, lcl);
                xxxwss = CUtf8::AsBasicString<xxxMywchar>(u8);
                u8x = CUtf8::AsUTF8(xxxwss);
                res = CUtf8::AsSingleByteString(u8, lcl);
                BOOST_CHECK(xxxwss == wstest);
                BOOST_CHECK(u8 == u8x);
                BOOST_CHECK(t == res);
                res = CUtf8::AsSingleByteString(u8, locale("pl"), "");
                BOOST_CHECK(res.empty());
            } catch(const exception& e) {
                cout << lcl_name << ": " << e.what() << endl;
            }
        }
        lcl_name = "pl-PL";
        {
            try {
                locale lcl(lcl_name);
                string t("Mo�esz u�ywa� wy��czy�");
                wstest = {77,111,380,101,115,122,32,117,380,121,119,97,263,32,119,121,322,261,99,122,121,263};
                u8 = CUtf8::AsUTF8(t, lcl);
                xxxwss = CUtf8::AsBasicString<xxxMywchar>(u8);
                u8x = CUtf8::AsUTF8(xxxwss);
                res = CUtf8::AsSingleByteString(u8, lcl);
                BOOST_CHECK(xxxwss == wstest);
                BOOST_CHECK(u8 == u8x);
                BOOST_CHECK(t == res);
            } catch(const exception& e) {
                cout << lcl_name << ": " << e.what() << endl;
            }
        }
        lcl_name = "ar-EG";
        {
            try {
                locale lcl(lcl_name);
                string t("������� ������");
                wstest = {1573,1593,1583,1575,1583,1575,1578,32,1573,1590,1575,1601,1610,1577};
                u8 = CUtf8::AsUTF8(t, lcl);
                xxxwss = CUtf8::AsBasicString<xxxMywchar>(u8);
                u8x = CUtf8::AsUTF8(xxxwss);
                res = CUtf8::AsSingleByteString(u8, lcl);
                BOOST_CHECK(xxxwss == wstest);
                BOOST_CHECK(u8 == u8x);
                BOOST_CHECK(t == res);
            } catch(const exception& e) {
                cout << lcl_name << ": " << e.what() << endl;
            }
        }
    }
#endif
    // CESU-8
    {
        string t1("be \xED\xA0\x81\xED\xB0\x80 and \xed\xa0\xb5\xed\xb1\x93 and \xed\xa0\xb5\xed\xb2\x93");
        BOOST_CHECK(CUtf8::GuessEncoding(t1) == eEncoding_CESU8);
        BOOST_CHECK(CUtf8::MatchEncoding(t1, eEncoding_UTF8));
        string u1 = CUtf8::AsUTF8(t1, eEncoding_CESU8);
        BOOST_CHECK(CUtf8::GuessEncoding(u1) == eEncoding_UTF8);
        BOOST_CHECK(!CUtf8::MatchEncoding(u1, eEncoding_CESU8));

#if defined(HAVE_WSTRING)
        wstring        tws = CUtf8::AsBasicString<wchar_t>(t1);
        wstring        uws = CUtf8::AsBasicString<wchar_t>(u1);
        BOOST_CHECK( tws == uws);
        string         uuw = CUtf8::AsUTF8(uws);
        BOOST_CHECK( uuw == u1);
#endif
        TStringUCS2    t2s = CUtf8::AsBasicString<TCharUCS2>(t1);
        TStringUCS2    u2s = CUtf8::AsBasicString<TCharUCS2>(u1);
        BOOST_CHECK( t2s == u2s);
        string         uu2 = CUtf8::AsUTF8(u2s);
        BOOST_CHECK( uu2 == u1);

        TStringUCS4    t4s = CUtf8::AsBasicString<TCharUCS4>(t1);
        TStringUCS4    u4s = CUtf8::AsBasicString<TCharUCS4>(u1);
        BOOST_CHECK( t4s == u4s);
        string         uu4 = CUtf8::AsUTF8(u4s);
        BOOST_CHECK( uu4 == u1);
    }

}


//----------------------------------------------------------------------------
// NStr::TruncateSpaves()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_TruncateSpaces)
{
    const char* szEmpty     = "";
    const char* szSpaces    = "  \t\n  \t\n  \t\n";
    const char* szTrunc     = "some long\tmultiline\nstring";
    const char* szBegSpace  = "  \t\nsome long\tmultiline\nstring";
    const char* szEndSpace  = "some long\tmultiline\nstring  \t\n";
    const char* szBothSpace = "  \t\nsome long\tmultiline\nstring  \t\n";

    const string sEmpty     = szEmpty;
    const string sSpaces    = szSpaces;
    const string sTrunc     = szTrunc;
    const string sBegSpace  = szBegSpace;
    const string sEndSpace  = szEndSpace;
    const string sBothSpace = szBothSpace;

    const CTempString tsEmpty    (szEmpty    );
    const CTempString tsSpaces   (szSpaces   );
    const CTempString tsTrunc    (szTrunc    );
    const CTempString tsBegSpace (szBegSpace );
    const CTempString tsEndSpace (szEndSpace );
    const CTempString tsBothSpace(szBothSpace);

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szEmpty,     NStr::eTrunc_Begin), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szEmpty,     NStr::eTrunc_End  ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szEmpty,     NStr::eTrunc_Both ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsEmpty,     NStr::eTrunc_Begin), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsEmpty,     NStr::eTrunc_End  ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsEmpty,     NStr::eTrunc_Both ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEmpty,             NStr::eTrunc_Begin), sEmpty     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEmpty,             NStr::eTrunc_End  ), sEmpty     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEmpty,             NStr::eTrunc_Both ), sEmpty     );

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szSpaces,    NStr::eTrunc_Begin), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szSpaces,    NStr::eTrunc_End  ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szSpaces,    NStr::eTrunc_Both ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsSpaces,    NStr::eTrunc_Begin), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsSpaces,    NStr::eTrunc_End  ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsSpaces,    NStr::eTrunc_Both ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sSpaces,            NStr::eTrunc_Begin), sEmpty     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sSpaces,            NStr::eTrunc_End  ), sEmpty     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sSpaces,            NStr::eTrunc_Both ), sEmpty     );

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szTrunc,     NStr::eTrunc_Begin), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szTrunc,     NStr::eTrunc_End  ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szTrunc,     NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsTrunc,     NStr::eTrunc_Begin), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsTrunc,     NStr::eTrunc_End  ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsTrunc,     NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sTrunc,             NStr::eTrunc_Begin), sTrunc     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sTrunc,             NStr::eTrunc_End  ), sTrunc     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sTrunc,             NStr::eTrunc_Both ), sTrunc     );

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szBegSpace,  NStr::eTrunc_Begin), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szBegSpace,  NStr::eTrunc_End  ), tsBegSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szBegSpace,  NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsBegSpace,  NStr::eTrunc_Begin), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsBegSpace,  NStr::eTrunc_End  ), tsBegSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsBegSpace,  NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBegSpace,          NStr::eTrunc_Begin), sTrunc     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBegSpace,          NStr::eTrunc_End  ), sBegSpace  );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBegSpace,          NStr::eTrunc_Both ), sTrunc     );

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szEndSpace,  NStr::eTrunc_Begin), tsEndSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szEndSpace,  NStr::eTrunc_End  ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szEndSpace,  NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsEndSpace,  NStr::eTrunc_Begin), tsEndSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsEndSpace,  NStr::eTrunc_End  ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsEndSpace,  NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEndSpace,          NStr::eTrunc_Begin), sEndSpace  );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEndSpace,          NStr::eTrunc_End  ), sTrunc     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEndSpace,          NStr::eTrunc_Both ), sTrunc     );

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szBothSpace, NStr::eTrunc_Begin), tsEndSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szBothSpace, NStr::eTrunc_End  ), tsBegSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(szBothSpace, NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsBothSpace, NStr::eTrunc_Begin), tsEndSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsBothSpace, NStr::eTrunc_End  ), tsBegSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces_Unsafe(tsBothSpace, NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBothSpace,         NStr::eTrunc_Begin), sEndSpace  );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBothSpace,         NStr::eTrunc_End  ), sBegSpace  );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBothSpace,         NStr::eTrunc_Both ), sTrunc     );

// http://unicode.org/charts/uca/chart_Whitespace.html
// http://en.wikipedia.org/wiki/Whitespace_character
    TUnicodeSymbol ws[] = {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x20, 0x85, 0xA0, 0x1680, 0x180E,
    0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200A,
    0x2028, 0x2029, 0x202F, 0x205F, 0x3000, 0 };
    int i;
    cout << "Testing iswspace" << endl;
    for (i=0; ws[i]; ++i) {
        if (iswspace(ws[i]) == 0) {
            cout << "WARNING: wide char " << hex << static_cast<int>(ws[i])
                 << " is not whitespace" << endl;
        }
    }
    cout << "Testing CStringUTF8::IsWhiteSpace" << endl;
    for (i=0; ws[i]; ++i) {
        BOOST_CHECK( CUtf8::IsWhiteSpace(ws[i]) );
    }
    for (TUnicodeSymbol t=1; t<=0x3000; ++t) {
        if (CUtf8::IsWhiteSpace(t)) {
            bool found=false;
            for (i=0; !found && ws[i]; ++i) {
                found = ws[i] == t;
            }
            BOOST_CHECK(found);
        }
    }

#if defined(HAVE_WSTRING)
    wstring wss;
#else
    TStringUnicode wss;
#endif

// CUtf8
    wss.erase().append(1, 0x2003).append(1, 0x2004).append(1, 0x2028).append(1, 0x205F);
    CStringUTF8 u8 = CUtf8::AsUTF8(wss);
    BOOST_CHECK( CUtf8::TruncateSpacesInPlace(u8,NStr::eTrunc_Begin).empty() );
    u8 = CUtf8::AsUTF8(wss);
    BOOST_CHECK( CUtf8::TruncateSpacesInPlace(u8,NStr::eTrunc_End).empty() );
    u8 = CUtf8::AsUTF8(wss);
    BOOST_CHECK( CUtf8::TruncateSpacesInPlace(u8).empty() );

    wss.append(1,0x61).append(1,0x62);
    u8 = CUtf8::AsUTF8(wss);
    BOOST_CHECK_EQUAL( CUtf8::TruncateSpacesInPlace(u8), "ab" );

    wss.append(1, 0x2028).append(1, 0x205F).append(1,0x0D);
    u8 = CUtf8::AsUTF8(wss);
    BOOST_CHECK_EQUAL( CUtf8::TruncateSpacesInPlace(u8), "ab" );
    u8 = CUtf8::AsUTF8(wss);
    BOOST_CHECK_EQUAL( CUtf8::TruncateSpaces(u8), "ab" );
}


//----------------------------------------------------------------------------
// NStr::Trim[Pre|Suf]fix*()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_TrimPrefixSuffix)
{
    const char*       cStr = "some long string";
    const string      sStr = cStr;
    const CTempString tStr(cStr);

    string str, s;
    CTempString tstr, tres;

    // NStr::Case

    str = cStr;
    {{
        tres = NStr::TrimPrefix_Unsafe(cStr, "Some");
        NStr::TrimPrefixInPlace(str, "Some");
        BOOST_CHECK_EQUAL(str, string(cStr));
        BOOST_CHECK_EQUAL(str, tres);

        tres = NStr::TrimPrefix_Unsafe(cStr, "SOME");
        NStr::TrimPrefixInPlace(str, "SOME");
        BOOST_CHECK_EQUAL(str, string(cStr));
        BOOST_CHECK_EQUAL(str, tres);

        tres = NStr::TrimSuffix_Unsafe(cStr, "STRING");
        NStr::TrimSuffixInPlace(str, "STRING");
        BOOST_CHECK_EQUAL(str, string(cStr));
        BOOST_CHECK_EQUAL(str, tres);
    }}
    {{
        s = str;
        tres = NStr::TrimPrefix_Unsafe(s, "some ");
        NStr::TrimPrefixInPlace(str, "some ");
        BOOST_CHECK_EQUAL(str, string("long string"));
        BOOST_CHECK_EQUAL(str, tres);

        s = str;
        tres = NStr::TrimSuffix_Unsafe(s, " string");
        NStr::TrimSuffixInPlace(str, " string");
        BOOST_CHECK_EQUAL(str, string("long"));
        BOOST_CHECK_EQUAL(str, tres);
    }}

    tstr = cStr;
    {{
        s = tstr;
        tres = NStr::TrimPrefix_Unsafe(s, "Some");
        NStr::TrimPrefixInPlace(tstr, "Some");
        BOOST_CHECK_EQUAL(tstr, string(cStr));
        BOOST_CHECK_EQUAL(tstr, tres);

        s = tstr;
        tres = NStr::TrimPrefix_Unsafe(s, "SOME");
        NStr::TrimPrefixInPlace(tstr, "SOME");
        BOOST_CHECK_EQUAL(tstr, string(cStr));
        BOOST_CHECK_EQUAL(tstr, tres);

        s = tstr;
        tres = NStr::TrimSuffix_Unsafe(s, "STRING");
        NStr::TrimSuffixInPlace(str, "STRING");
        BOOST_CHECK_EQUAL(tstr, string(cStr));
        BOOST_CHECK_EQUAL(tstr, tres);
    }}
    {{
        s = tstr;
        tres = NStr::TrimPrefix_Unsafe(s, "some ");
        NStr::TrimPrefixInPlace(tstr, "some ");
        BOOST_CHECK_EQUAL(tstr, string("long string"));
        BOOST_CHECK_EQUAL(tstr, tres);

        s = tstr;
        tres = NStr::TrimSuffix_Unsafe(s, " string");
        NStr::TrimSuffixInPlace(tstr, " string");
        BOOST_CHECK_EQUAL(tstr, string("long"));
        BOOST_CHECK_EQUAL(tstr, tres);
    }}

    // NStr::eNocase

    str = cStr;
    {{
        s = str;
        tres = NStr::TrimPrefix_Unsafe(s, "Some ", NStr::eNocase);
        NStr::TrimPrefixInPlace(str, "Some ", NStr::eNocase);
        BOOST_CHECK_EQUAL(str, string("long string"));
        BOOST_CHECK_EQUAL(str, tres);

        s = str;
        tres = NStr::TrimSuffix_Unsafe(s, " STRING", NStr::eNocase);
        NStr::TrimSuffixInPlace(str, " STRING", NStr::eNocase);
        BOOST_CHECK_EQUAL(str, string("long"));
        BOOST_CHECK_EQUAL(str, tres);
    }}

    tstr = cStr;
    {{
        s = tstr;
        tres = NStr::TrimPrefix_Unsafe(s, "Some ", NStr::eNocase);
        NStr::TrimPrefixInPlace(tstr, "Some ", NStr::eNocase);
        BOOST_CHECK_EQUAL(tstr, string("long string"));
        BOOST_CHECK_EQUAL(tstr, tres);

        s = tstr;
        tres = NStr::TrimSuffix_Unsafe(s, " STRING", NStr::eNocase);
        NStr::TrimSuffixInPlace(tstr, " STRING", NStr::eNocase);
        BOOST_CHECK_EQUAL(tstr, string("long"));
        BOOST_CHECK_EQUAL(tstr, tres);
    }}
}


//----------------------------------------------------------------------------
// NStr::GetField()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_GetField)
{
    BOOST_CHECK_EQUAL( NStr::GetField(NULL, 17, "not important"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 0, ":"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 10, ":"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 1, ":"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 1, "-.:;"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 1, "-.:"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 1, "-.:;"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 176, "-.:;"), string() );

    BOOST_CHECK_EQUAL( NStr::GetField(NULL, 17, "not important", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 0, ":", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 10, ":", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::: two", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:::two:", 1, ":", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::-:two::", 1, "-.:;", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:two", 1, "-.:", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one.two.", 1, "-.:;", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 176, "-.:;", NStr::eMergeDelims), string() );
}

BOOST_AUTO_TEST_CASE(s_GetField_SingleDilimiter)
{
    BOOST_CHECK_EQUAL( NStr::GetField(NULL, 17, 'n'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 0, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 10, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 1, ':'), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 1, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 176, ':'), string() );

    BOOST_CHECK_EQUAL( NStr::GetField(NULL, 17, 'n', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 0, ':', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 10, ':', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::: two", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:::two:", 1, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::-:two::", 2, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:two", 1, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one.two.", 1, '.', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 176, ':', NStr::eMergeDelims), string() );
}

BOOST_AUTO_TEST_CASE(s_GetField_Unsafe)
{
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe(NULL, 17, "not important"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 0, ":"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 10, ":"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one: two", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one: two", 1, ":"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one: two", 1, "-.:;"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one: two", 1, "-.:"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 1, "-.:;"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 176, "-.:;"), string() );

    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe(NULL, 17, "not important", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 0, ":", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 10, ":", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::: two", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:::two:", 1, ":", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::-:two::", 1, "-.:;", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:two", 1, "-.:", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one.two.", 1, "-.:;", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 176, "-.:;", NStr::eMergeDelims), string() );
}

BOOST_AUTO_TEST_CASE(s_GetField_SingleDilimiter_Unsafe)
{
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe(NULL, 17, 'n'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 0, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 10, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one: two", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:two", 1, ':'), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:two:", 1, ':'), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 1, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 176, ':'), string() );

    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe(NULL, 17, 'n', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 0, ':', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 10, ':', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::: two", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:::two:", 1, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::-:two::", 2, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:two", 1, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one.two.", 1, '.', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 176, ':', NStr::eMergeDelims), string() );
}


//----------------------------------------------------------------------------
// NStr::SQLEncode()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_SQLEncode)
{
    BOOST_CHECK_EQUAL( NStr::SQLEncode(
        CUtf8::AsUTF8("should not be touched",eEncoding_ISO8859_1),
        NStr::eSqlEnc_TagNonASCII),
        CUtf8::AsUTF8("'should not be touched'",eEncoding_ISO8859_1) );
    BOOST_CHECK_EQUAL(NStr::SQLEncode(CUtf8::AsUTF8("", eEncoding_ISO8859_1),
                                      NStr::eSqlEnc_TagNonASCII),
                      CUtf8::AsUTF8("''",eEncoding_ISO8859_1));
    BOOST_CHECK_EQUAL(NStr::SQLEncode(CUtf8::AsUTF8("'", eEncoding_ISO8859_1),
                                      NStr::eSqlEnc_TagNonASCII),
                      CUtf8::AsUTF8("''''",eEncoding_ISO8859_1));
    BOOST_CHECK_EQUAL(NStr::SQLEncode(CUtf8::AsUTF8("\\'",eEncoding_ISO8859_1),
                                      NStr::eSqlEnc_TagNonASCII),
                      CUtf8::AsUTF8("'\\'''",eEncoding_ISO8859_1));
    BOOST_CHECK_EQUAL(NStr::SQLEncode(CUtf8::AsUTF8("'a", eEncoding_ISO8859_1),
                                      NStr::eSqlEnc_TagNonASCII),
                      CUtf8::AsUTF8("'''a'",eEncoding_ISO8859_1));
    BOOST_CHECK_EQUAL(NStr::SQLEncode(CUtf8::AsUTF8("a'", eEncoding_ISO8859_1),
                                      NStr::eSqlEnc_TagNonASCII),
                      CUtf8::AsUTF8("'a'''",eEncoding_ISO8859_1));
    BOOST_CHECK_EQUAL( NStr::SQLEncode(
                           CUtf8::AsUTF8("`1234567890-=~!@#$%^&*()_+qwertyuiop[]\\asdfghjkl;zxcvbnm,./QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?",
                                         eEncoding_ISO8859_1),
                           NStr::eSqlEnc_TagNonASCII),
        CUtf8::AsUTF8("'`1234567890-=~!@#$%^&*()_+qwertyuiop[]\\asdfghjkl;zxcvbnm,./QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?'",eEncoding_ISO8859_1) );

    const unsigned char s_UpperHalf[] = {
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
    };

    const unsigned char s_Expected[] = {
  '\'', 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, '\''
    };

    CStringUTF8      upperHalf( CUtf8::AsUTF8(CTempString((char*)s_UpperHalf, 128),eEncoding_ISO8859_1 ));
    CStringUTF8      expected( CUtf8::AsUTF8( CTempString((char*)s_Expected, 130),eEncoding_ISO8859_1 ));

    BOOST_CHECK_EQUAL(NStr::SQLEncode(upperHalf, NStr::eSqlEnc_Plain), expected);
    BOOST_CHECK_EQUAL(NStr::SQLEncode(upperHalf, NStr::eSqlEnc_TagNonASCII), 'N' + expected);
}


//----------------------------------------------------------------------------
// NStr::StrigToNum() speed test
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_StringToInt_Speed)
{
    const int COUNT = 10000000;
    const int TESTS = 6;
    const string ss[TESTS] = { "", "0", "1", "12345", "1234567890", "TRACE" };
    const int ssr[TESTS] = { -1, 0, 1, 12345, 1234567890, -1 };

    for ( int t = 0; t < TESTS; ++t ) 
    {
        int v = NStr::StringToNumeric<int>(ss[t], NStr::fConvErr_NoThrow);
        if ( !v && errno ) v = -1;
        if ( v != ssr[t] ) Abort();

        errno = 0;
        Uint8 v8 = NStr::StringToUInt8(ss[t], NStr::fConvErr_NoThrow);
        v = (int)v8;
        if ( !v8 && errno ) v = -1;
        if ( v != ssr[t] ) Abort();
        
        try {
            v = (int)NStr::StringToNumeric<Uint8>(ss[t]);
        }
        catch ( exception& ) {
            v = -1;
        }
        if ( v != ssr[t] ) Abort();
        try {
            v = (int)NStr::StringToUInt8(ss[t]);
        }
        catch ( exception& ) {
            v = -1;
        }
        if ( v != ssr[t] ) Abort();
    }
    for ( int t = 0; t < TESTS; ++t ) {
        CTempString s = ss[t];
        CStopWatch sw;
        double time;

        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                NStr::StringToNumeric<int>(ss[t], NStr::fConvErr_NoThrow);
            }
            time = sw.Elapsed();
            LOG_POST("StringToNumeric<int>("<<ss[t]<<") time: " << time);
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                NStr::StringToUInt8(s, NStr::fConvErr_NoThrow);
            }
            time = sw.Elapsed();
            LOG_POST("StringToInt8("<<ss[t]<<") time: " << time);
        }
        if ( 0 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                int v;
                try {
                    v = (int)NStr::StringToUInt8(s);
                }
                catch ( exception& ) {
                    v = -1;
                }
                if ( v != ssr[t] ) Abort();
            }
            time = sw.Elapsed();
            LOG_POST("StringToInt8("<<ss[t]<<") time: " << time);
        }
    }
}


BOOST_AUTO_TEST_CASE(s_StringToDouble_Speed)
{
    const int COUNT = 10000000;
    const string ss[] = {
        "", "0", "1", "12", "123", "123456789", "1234567890", "TRACE",
        "0e9", "1e9",
        "1.234567890123456789e300", "-1.234567890123456789e-300",
        "1.234567890123456789e200", "-1.234567890123456789e-200" 
    };
    const double ssr[] = {
        -1, 0, 1, 12, 123, 123456789, 1234567890, -1,
        0, 1e9,
        1.234567890123456789e300, -1.234567890123456789e-300,
        1.234567890123456789e200, -1.234567890123456789e-200
    };
    double ssr_min[sizeof(ssr)/sizeof(ssr[0])];
    double ssr_max[sizeof(ssr)/sizeof(ssr[0])];
    const size_t TESTS = ArraySize(ss);

    int flags = NStr::fConvErr_NoThrow|NStr::fAllowLeadingSpaces;
    double v;
    for ( size_t t = 0; t < TESTS; ++t ) {
        if ( 1 ) {
            double r_min = ssr[t], r_max = r_min;
            if ( r_min < 0 ) {
                r_min *= 1+1e-15;
                r_max *= 1-1e-15;
            }
            else {
                r_min *= 1-1e-15;
                r_max *= 1+1e-15;
            }
            ssr_min[t] = r_min;
            ssr_max[t] = r_max;
        }
        if ( 1 ) {
            errno = 0;
            v = NStr::StringToDouble(ss[t], flags|NStr::fDecimalPosix);
            if ( errno ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_FATAL(v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            v = NStr::StringToDouble(ss[t], flags);
            if ( errno ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_FATAL(v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            char* errptr;
            v = NStr::StringToDoublePosix(ss[t].c_str(), &errptr);
            if ( errno || (errptr&&(*errptr||errptr==ss[t].c_str())) ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_FATAL(v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            char* errptr;
            v = strtod(ss[t].c_str(), &errptr);
            if ( errno || (errptr&&(*errptr||errptr==ss[t].c_str())) ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_FATAL(v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }
    }
    for ( size_t t = 0; t < TESTS; ++t ) {
        string s1 = ss[t];
        CTempStringEx s = ss[t];
        const char* s2 = ss[t].c_str();
        CStopWatch sw;
        double time;

        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                NStr::StringToDouble(s, flags|NStr::fDecimalPosix);
            }
            time = sw.Elapsed();
            LOG_POST("StringToDouble("<<ss[t]<<", Posix) time: " << time);
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                NStr::StringToDouble(s, flags);
            }
            time = sw.Elapsed();
            LOG_POST("StringToDouble("<<ss[t]<<") time: " << time);
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                char* errptr;
                NStr::StringToDoublePosix(s2, &errptr);
            }
            time = sw.Elapsed();
            LOG_POST("StringToDoublePosix("<<ss[t]<<") time: " << time);
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                char* errptr;
                _no_warning(strtod(s2, &errptr));
            }
            time = sw.Elapsed();
            LOG_POST("strtod("<<ss[t]<<") time: " << time);
        }
    }
}


static const string s_ShellStr[] = {
    "abc",            // normal string, no encoding
    "ab\acd",         // non-printable chars, need BASH encoding
    "ab\ncd",         // EOL in the string
    "ab cd\tef",      // spaces - need quotes
    "ab!{}cd?*",      // more special chars which need quotes
    "ab'cd'ef",       // single quote - use double quotes around the string
    "ab ' cd ' ef",   // the same with extra-spaces
    "ab'$cd",         // additional chars ($, \) - can not use double quotes
    "\"ab cd ef\"",   // double quotes - use single quotes around the string
    "ab\\cd",         // backslash
    "ab\\cd'ef",      // backslash with single quote
    "ab\\cd\"ef",     // backslash with double quote
    "ab'\\cd\"$ef",   // ', ", \, $
    "",               // empty string
    "''",             // empty single quotes
    "\"\""            // empty double quotes
};


#ifdef NCBI_OS_UNIX
BOOST_AUTO_TEST_CASE(s_ShellEncode)
{
    string echo_file = CFile::GetTmpName(CFile::eTmpFileCreate);
    string cmd_file = "./echo.sh";

    {{
        CNcbiOfstream out(cmd_file.c_str());
        out << "#! /usr/bin/env bash" << endl;

        for (size_t i = 0;  i < sizeof(s_ShellStr) / sizeof(s_ShellStr[0]);  i++) {
            string cmd = "echo -E ";
            cmd += NStr::ShellEncode(s_ShellStr[i]);
            cmd += " >> ";
            cmd += echo_file;
           out << cmd << endl;
        }
    }}
    CFile(cmd_file).SetMode(CFile::fDefaultUser | CFile::fExecute);
    BOOST_CHECK_EQUAL(CExec::System(cmd_file.c_str()), 0);

    CNcbiIfstream in(echo_file.c_str());
    string s, line;
    for (size_t i = 0;  i < sizeof(s_ShellStr) / sizeof(s_ShellStr[0]);  i++) {
        s.clear();
        size_t eol_pos = 0;
        do {
            getline(in, line);
            s.append(line);
            eol_pos = s_ShellStr[i].find('\n', eol_pos);
            if (eol_pos != NPOS) {
                eol_pos++;
                s.append("\n");
            }
        } while (eol_pos != NPOS);
        BOOST_CHECK_EQUAL(s_ShellStr[i], s);
    }
    CFile(echo_file).Remove();
    CFile(cmd_file).Remove();
}
#endif

BOOST_AUTO_TEST_CASE(s_StringJoin)
{
    string result("one,two,three"), resultN("1,2,3");
    stringstream iss("one two three");
    istream_iterator<string> it(iss);
    BOOST_CHECK_EQUAL(result, NStr::Join(it, istream_iterator<string>(), ","));

    stringstream iss1("1 2 3");
    BOOST_CHECK_EQUAL(resultN, NStr::JoinNumeric(istream_iterator<int>(iss1), istream_iterator<int>(), ","));

    list<string> x = {"one", "two", "three"};
    BOOST_CHECK_EQUAL(result, NStr::Join(x, ","));
    BOOST_CHECK_EQUAL(result, NStr::Join(x.begin(), x.end(), ","));

    initializer_list<string> y = {"one", "two", "three"};
    BOOST_CHECK_EQUAL(result, NStr::Join(begin(y), end(y), ","));
    BOOST_CHECK_EQUAL(result, NStr::Join(y, ","));
    BOOST_CHECK_EQUAL(result, NStr::Join(initializer_list<string>({"one", "two", "three"}), ","));
    BOOST_CHECK_EQUAL(result, NStr::Join({"one", "two", "three"}, ","));

    initializer_list<const char*> y2 = {"one", "two", "three"};
    BOOST_CHECK_EQUAL(result, NStr::Join(begin(y2), end(y2), ","));
    BOOST_CHECK_EQUAL(result, NStr::Join(y2, ","));

    string z[3] = {"one", "two", "three"};
    BOOST_CHECK_EQUAL(result, NStr::Join(begin(z), end(z), ","));
    BOOST_CHECK_EQUAL(result, NStr::Join(z, ","));

    const char* z2[3] = {"one", "two", "three"};
    BOOST_CHECK_EQUAL(result, NStr::Join(begin(z2), end(z2), ","));
    BOOST_CHECK_EQUAL(result, NStr::Join(z2, ","));

    map<string, string> m;
    m["one"] = "uno";
    m["two"] = "dos";
    BOOST_CHECK_EQUAL("one:uno",NStr::Join({m.begin()->first, m.begin()->second}, ":"));
#if 0
    string jjj = NStr::JoinNumeric( begin(m), end(m), ",");
#endif

    BOOST_CHECK_EQUAL("one:uno,two:dos", NStr::TransformJoin( m.begin(), m.end(), ",", 
                      [](const map<string, string>::value_type& i){ return NStr::Join( {i.first, i.second}, ":");}));
// using auto in lambdas, requires C++14?
// that is, this might fail to compile, otherwise, it is correct
//    BOOST_CHECK_EQUAL("one:uno,two:dos", NStr::TransformJoin( m.begin(), m.end(), ",", [](const auto& i){ return NStr::Join( {i.first, i.second}, ":");}));

    list<int> mi = {1,2,3};
//    BOOST_CHECK_EQUAL(resultN, NStr::TransformJoin( mi.begin(), mi.end(), ",", [](const auto& i){ return NStr::NumericToString(i);}));
    BOOST_CHECK_EQUAL(resultN, NStr::TransformJoin( mi.begin(), mi.end(), ",", [](const int& i){ return NStr::NumericToString(i);}));
    BOOST_CHECK_EQUAL(resultN, NStr::JoinNumeric( mi.begin(), mi.end(), ","));

    initializer_list<int> mi2 = {1,2,3};
//    BOOST_CHECK_EQUAL(resultN, NStr::TransformJoin( mi2.begin(), mi2.end(), ",", [](const auto& i){ return NStr::NumericToString(i);}));
    BOOST_CHECK_EQUAL(resultN, NStr::TransformJoin( mi2.begin(), mi2.end(), ",", [](const int i){ return NStr::NumericToString(i);}));
    BOOST_CHECK_EQUAL(resultN, NStr::JoinNumeric( mi2.begin(), mi2.end(), ","));

    int z3[3] = {1,2,3};
#if 0
    string jjj = NStr::Join(begin(z3), end(z3), ",");
    jjj = NStr::Join(z3, ",");
#endif
    BOOST_CHECK_EQUAL(resultN, NStr::TransformJoin( begin(z3), end(z3), ",", [](const int& i){ return NStr::NumericToString(i);}));
    BOOST_CHECK_EQUAL(resultN, NStr::JoinNumeric( begin(z3), end(z3), ","));

    list<CTime> t1;
    t1.push_back( CTime(CTime::eCurrent, CTime::eLocal));
    t1.push_back( CTime(CTime::eCurrent, CTime::eUTC));
    t1.push_back( CTime(CTime::eCurrent, CTime::eGmt));
    string j = NStr::Join(t1, ",");
//    j = NStr::TransformJoin( t1.begin(), t1.end(), ",", [](const auto& i){ return i.AsString();});
    j = NStr::TransformJoin( t1.begin(), t1.end(), ",", [](const CTime& i){ return i.AsString();});
    j = NStr::Join({CTime(CTime::eCurrent, CTime::eLocal), CTime(CTime::eCurrent, CTime::eUTC)}, ",");

    CTime arr[2];
    arr[0].SetCurrent();
    arr[1].SetCurrent();
    j = NStr::Join(arr, ",");
    j = NStr::Join(begin(arr), end(arr), ",");
}

BOOST_AUTO_TEST_CASE(s_HtmlEncode)
{
    {
        TStringUCS2 wstest = {1050, 1086, 1084, 1072, 1085, 1076, 1099};
        string u8 = CUtf8::AsUTF8(wstest);
        string u8enc = NStr::HtmlEncode(u8);
        string u8dec = NStr::HtmlDecode(u8enc);
        TStringUCS2 wsout = CUtf8::AsBasicString<TCharUCS2>(u8dec);
        bool eq = wstest == wsout;
        BOOST_CHECK(eq);
    }
    {
        string stest = "&Aacute;&scaron;&Gamma;&thinsp;&rang;&#x960;&#x1225;";
        string sdec = NStr::HtmlDecode(stest);
        string senc = NStr::HtmlEncode(sdec);
        BOOST_CHECK_EQUAL(senc, "&#xC1;&#x161;&#x393;&#x2009;&#x232A;&#x960;&#x1225;");
        BOOST_CHECK( CUtf8::MatchEncoding(sdec, eEncoding_UTF8));
        TStringUCS2    u2s = CUtf8::AsBasicString<TCharUCS2>(sdec);
    }
    {
        string stest = "amp = &, preencoded = &Aacute;&#x960;, &;";
        string senc1 = NStr::HtmlEncode(stest, NStr::fHtmlEnc_SkipEntities);
        string sdec1 = NStr::HtmlDecode(senc1);
        BOOST_CHECK_EQUAL(senc1, "amp = &amp;, preencoded = &Aacute;&amp;#x960;, &amp;;");
        BOOST_CHECK_EQUAL(sdec1, "amp = &, preencoded = Á&#x960;, &;");
        string senc2 = NStr::HtmlEncode(stest, NStr::fHtmlEnc_EncodeAll);
        string sdec2 = NStr::HtmlDecode(senc2);
        BOOST_CHECK_EQUAL(senc2, "amp = &amp;, preencoded = &amp;Aacute;&amp;#x960;, &amp;;");
        BOOST_CHECK_EQUAL(sdec2, stest);
    }
#if 0
    {
    string input = "this is &#32; but it's not &#33; a &#35; not &#38; a &#43; sign is different from a &#45; you can use &lt; but not &gt; &#63; &Agrave; &Aacute; &Aring; &Eacute; &Eumi; &Ntilde; &Uacute;";
    string output = NStr::HtmlDecode(input);
    cout << output << endl;
    string inagain = NStr::HtmlEncode(output);
    cout << inagain << endl;
    }
#endif
}

BOOST_AUTO_TEST_CASE(s_JsonEncode)
{
    vector<const char*> plain = {
        "\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0",
        "\xef\xee\xed\xec\xeb\xea\xe9\xe8\xe7\xe6\xe5\xe4\xe3\xe2\xe1\xe0",
        "\xdf\xde\xdd\xdc\xdb\xda\xd9\xd8\xd7\xd6\xd5\xd4\xd3\xd2\xd1\xd0",
        "\xcf\xce\xcd\xcc\xcb\xca\xc9\xc8\xc7\xc6\xc5\xc4\xc3\xc2\xc1\xc0",
        "\xbf\xbe\xbd\xbc\xbb\xba\xb9\xb8\xb7\xb6\xb5\xb4\xb3\xb2\xb1\xb0",
        "\xaf\xae\xad\xac\xab\xaa\xa9\xa8\xa7\xa6\xa5\xa4\xa3\xa2\xa1\xa0",
        "\x9f\x9e\x9d\x9c\x9b\x9a\x99\x98\x97\x96\x95\x94\x93\x92\x91\x90",
        "\x8f\x8e\x8d\x8c\x8b\x8a\x89\x88\x87\x86\x85\x84\x83\x82\x81\x80",
        "\x7f\x7e\x7d\x7c\x7b\x7a\x79\x78\x77\x76\x75\x74\x73\x72\x71\x70",
        "\x6f\x6e\x6d\x6c\x6b\x6a\x69\x68\x67\x66\x65\x64\x63\x62\x61\x60",
        "\x5f\x5e\x5d\x5c\x5b\x5a\x59\x58\x57\x56\x55\x54\x53\x52\x51\x50",
        "\x4f\x4e\x4d\x4c\x4b\x4a\x49\x48\x47\x46\x45\x44\x43\x42\x41\x40",
        "\x3f\x3e\x3d\x3c\x3b\x3a\x39\x38\x37\x36\x35\x34\x33\x32\x31\x30",
        "\x2f\x2e\x2d\x2c\x2b\x2a\x29\x28\x27\x26\x25\x24\x23\x22\x21\x20",
        "\x1f\x1e\x1d\x1c\x1b\x1a\x19\x18\x17\x16\x15\x14\x13\x12\x11\x10",
        "\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01",
    };
    vector<pair<NStr::EJsonEncode, vector<const char*>>> encoded = {
        {
            NStr::eJsonEnc_UTF8,
            {
                "\\u00ff\\u00fe\\u00fd\\u00fc\\u00fb\\u00fa\\u00f9\\u00f8\\u00f7\\u00f6\\u00f5\\u00f4\\u00f3\\u00f2\\u00f1\\u00f0",
                "\\u00ef\\u00ee\\u00ed\\u00ec\\u00eb\\u00ea\\u00e9\\u00e8\\u00e7\\u00e6\\u00e5\\u00e4\\u00e3\\u00e2\\u00e1\\u00e0",
                "\\u00df\\u00de\\u00dd\\u00dc\\u00db\\u00da\\u00d9\\u00d8\\u00d7\\u00d6\\u00d5\\u00d4\\u00d3\\u00d2\\u00d1\\u00d0",
                "\\u00cf\\u00ce\\u00cd\\u00cc\\u00cb\\u00ca\\u00c9\\u00c8\\u00c7\\u00c6\\u00c5\\u00c4\\u00c3\\u00c2\\u00c1\\u00c0",
                "\\u00bf\\u00be\\u00bd\\u00bc\\u00bb\\u00ba\\u00b9\\u00b8\\u00b7\\u00b6\\u00b5\\u00b4\\u00b3\\u00b2\\u00b1\\u00b0",
                "\\u00af\\u00ae\\u00ad\\u00ac\\u00ab\\u00aa\\u00a9\\u00a8\\u00a7\\u00a6\\u00a5\\u00a4\\u00a3\\u00a2\\u00a1\\u00a0",
                "\\u009f\\u009e\\u009d\\u009c\\u009b\\u009a\\u0099\\u0098\\u0097\\u0096\\u0095\\u0094\\u0093\\u0092\\u0091\\u0090",
                "\\u008f\\u008e\\u008d\\u008c\\u008b\\u008a\\u0089\\u0088\\u0087\\u0086\\u0085\\u0084\\u0083\\u0082\\u0081\\u0080",
                "\x7f\x7e\x7d\x7c\x7b\x7a\x79\x78\x77\x76\x75\x74\x73\x72\x71\x70",
                "\x6f\x6e\x6d\x6c\x6b\x6a\x69\x68\x67\x66\x65\x64\x63\x62\x61\x60",
                "\x5f\x5e\x5d\\\x5c\x5b\x5a\x59\x58\x57\x56\x55\x54\x53\x52\x51\x50",
                "\x4f\x4e\x4d\x4c\x4b\x4a\x49\x48\x47\x46\x45\x44\x43\x42\x41\x40",
                "\x3f\x3e\x3d\x3c\x3b\x3a\x39\x38\x37\x36\x35\x34\x33\x32\x31\x30",
                "\x2f\x2e\x2d\x2c\x2b\x2a\x29\x28\x27\x26\x25\x24\x23\\\x22\x21\x20",
                "\\u001f\\u001e\\u001d\\u001c\\u001b\\u001a\\u0019\\u0018\\u0017\\u0016\\u0015\\u0014\\u0013\\u0012\\u0011\\u0010",
                "\\u000f\\u000e\\u000d\\u000c\\u000b\\u000a\\u0009\\u0008\\u0007\\u0006\\u0005\\u0004\\u0003\\u0002\\u0001",
            },
        },
        {
            NStr::eJsonEnc_Quoted,
            {
                "\"\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0\"",
                "\"\xef\xee\xed\xec\xeb\xea\xe9\xe8\xe7\xe6\xe5\xe4\xe3\xe2\xe1\xe0\"",
                "\"\xdf\xde\xdd\xdc\xdb\xda\xd9\xd8\xd7\xd6\xd5\xd4\xd3\xd2\xd1\xd0\"",
                "\"\xcf\xce\xcd\xcc\xcb\xca\xc9\xc8\xc7\xc6\xc5\xc4\xc3\xc2\xc1\xc0\"",
                "\"\xbf\xbe\xbd\xbc\xbb\xba\xb9\xb8\xb7\xb6\xb5\xb4\xb3\xb2\xb1\xb0\"",
                "\"\xaf\xae\xad\xac\xab\xaa\xa9\xa8\xa7\xa6\xa5\xa4\xa3\xa2\xa1\xa0\"",
                "\"\x9f\x9e\x9d\x9c\x9b\x9a\x99\x98\x97\x96\x95\x94\x93\x92\x91\x90\"",
                "\"\x8f\x8e\x8d\x8c\x8b\x8a\x89\x88\x87\x86\x85\x84\x83\x82\x81\x80\"",
                "\"\x7f\x7e\x7d\x7c\x7b\x7a\x79\x78\x77\x76\x75\x74\x73\x72\x71\x70\"",
                "\"\x6f\x6e\x6d\x6c\x6b\x6a\x69\x68\x67\x66\x65\x64\x63\x62\x61\x60\"",
                "\"\x5f\x5e\x5d\\\x5c\x5b\x5a\x59\x58\x57\x56\x55\x54\x53\x52\x51\x50\"",
                "\"\x4f\x4e\x4d\x4c\x4b\x4a\x49\x48\x47\x46\x45\x44\x43\x42\x41\x40\"",
                "\"\x3f\x3e\x3d\x3c\x3b\x3a\x39\x38\x37\x36\x35\x34\x33\x32\x31\x30\"",
                "\"\x2f\x2e\x2d\x2c\x2b\x2a\x29\x28\x27\x26\x25\x24\x23\\\x22\x21\x20\"",
                "\"\\u001f\\u001e\\u001d\\u001c\\u001b\\u001a\\u0019\\u0018\\u0017\\u0016\\u0015\\u0014\\u0013\\u0012\\u0011\\u0010\"",
                "\"\\u000f\\u000e\\u000d\\u000c\\u000b\\u000a\\u0009\\u0008\\u0007\\u0006\\u0005\\u0004\\u0003\\u0002\\u0001\"",
            },
        },
    };

    for (const auto& current : encoded) {
        assert(current.second.size() == plain.size());

        for (size_t i = 0; i < plain.size(); ++i) {
            auto result = NStr::JsonEncode(plain[i], current.first);
            BOOST_CHECK_EQUAL(result, current.second[i]);
        }
    }
}

BOOST_AUTO_TEST_CASE(s_JsonDecode)
{
    vector<const char*> plain = {
        "\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0",
        "\xef\xee\xed\xec\xeb\xea\xe9\xe8\xe7\xe6\xe5\xe4\xe3\xe2\xe1\xe0",
        "\xdf\xde\xdd\xdc\xdb\xda\xd9\xd8\xd7\xd6\xd5\xd4\xd3\xd2\xd1\xd0",
        "\xcf\xce\xcd\xcc\xcb\xca\xc9\xc8\xc7\xc6\xc5\xc4\xc3\xc2\xc1\xc0",
        "\xbf\xbe\xbd\xbc\xbb\xba\xb9\xb8\xb7\xb6\xb5\xb4\xb3\xb2\xb1\xb0",
        "\xaf\xae\xad\xac\xab\xaa\xa9\xa8\xa7\xa6\xa5\xa4\xa3\xa2\xa1\xa0",
        "\x9f\x9e\x9d\x9c\x9b\x9a\x99\x98\x97\x96\x95\x94\x93\x92\x91\x90",
        "\x8f\x8e\x8d\x8c\x8b\x8a\x89\x88\x87\x86\x85\x84\x83\x82\x81\x80",
        "\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0",
        "\xef\xee\xed\xec\xeb\xea\xe9\xe8\xe7\xe6\xe5\xe4\xe3\xe2\xe1\xe0",
        "\xdf\xde\xdd\xdc\xdb\xda\xd9\xd8\xd7\xd6\xd5\xd4\xd3\xd2\xd1\xd0",
        "\xcf\xce\xcd\xcc\xcb\xca\xc9\xc8\xc7\xc6\xc5\xc4\xc3\xc2\xc1\xc0",
        "\xbf\xbe\xbd\xbc\xbb\xba\xb9\xb8\xb7\xb6\xb5\xb4\xb3\xb2\xb1\xb0",
        "\xaf\xae\xad\xac\xab\xaa\xa9\xa8\xa7\xa6\xa5\xa4\xa3\xa2\xa1\xa0",
        "\x9f\x9e\x9d\x9c\x9b\x9a\x99\x98\x97\x96\x95\x94\x93\x92\x91\x90",
        "\x8f\x8e\x8d\x8c\x8b\x8a\x89\x88\x87\x86\x85\x84\x83\x82\x81\x80",
        "\x7f\x7e\x7d\x7c\x7b\x7a\x79\x78\x77\x76\x75\x74\x73\x72\x71\x70",
        "\x6f\x6e\x6d\x6c\x6b\x6a\x69\x68\x67\x66\x65\x64\x63\x62\x61\x60",
        "\x5f\x5e\x5d\x5c\x5b\x5a\x59\x58\x57\x56\x55\x54\x53\x52\x51\x50",
        "\x4f\x4e\x4d\x4c\x4b\x4a\x49\x48\x47\x46\x45\x44\x43\x42\x41\x40",
        "\x3f\x3e\x3d\x3c\x3b\x3a\x39\x38\x37\x36\x35\x34\x33\x32\x31\x30",
        "\x2f\x2e\x2d\x2c\x2b\x2a\x29\x28\x27\x26\x25\x24\x23\x22\x21\x20",
        "\x1f\x1e\x1d\x1c\x1b\x1a\x19\x18\x17\x16\x15\x14\x13\x12\x11\x10",
        "\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01",
    };
    vector<const char*> encoded = {
        "\"\\u00ff\\u00fe\\u00fd\\u00fc\\u00fb\\u00fa\\u00f9\\u00f8\\u00f7\\u00f6\\u00f5\\u00f4\\u00f3\\u00f2\\u00f1\\u00f0\"",
        "\"\\u00ef\\u00ee\\u00ed\\u00ec\\u00eb\\u00ea\\u00e9\\u00e8\\u00e7\\u00e6\\u00e5\\u00e4\\u00e3\\u00e2\\u00e1\\u00e0\"",
        "\"\\u00df\\u00de\\u00dd\\u00dc\\u00db\\u00da\\u00d9\\u00d8\\u00d7\\u00d6\\u00d5\\u00d4\\u00d3\\u00d2\\u00d1\\u00d0\"",
        "\"\\u00cf\\u00ce\\u00cd\\u00cc\\u00cb\\u00ca\\u00c9\\u00c8\\u00c7\\u00c6\\u00c5\\u00c4\\u00c3\\u00c2\\u00c1\\u00c0\"",
        "\"\\u00bf\\u00be\\u00bd\\u00bc\\u00bb\\u00ba\\u00b9\\u00b8\\u00b7\\u00b6\\u00b5\\u00b4\\u00b3\\u00b2\\u00b1\\u00b0\"",
        "\"\\u00af\\u00ae\\u00ad\\u00ac\\u00ab\\u00aa\\u00a9\\u00a8\\u00a7\\u00a6\\u00a5\\u00a4\\u00a3\\u00a2\\u00a1\\u00a0\"",
        "\"\\u009f\\u009e\\u009d\\u009c\\u009b\\u009a\\u0099\\u0098\\u0097\\u0096\\u0095\\u0094\\u0093\\u0092\\u0091\\u0090\"",
        "\"\\u008f\\u008e\\u008d\\u008c\\u008b\\u008a\\u0089\\u0088\\u0087\\u0086\\u0085\\u0084\\u0083\\u0082\\u0081\\u0080\"",
        "\"\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0\"",
        "\"\xef\xee\xed\xec\xeb\xea\xe9\xe8\xe7\xe6\xe5\xe4\xe3\xe2\xe1\xe0\"",
        "\"\xdf\xde\xdd\xdc\xdb\xda\xd9\xd8\xd7\xd6\xd5\xd4\xd3\xd2\xd1\xd0\"",
        "\"\xcf\xce\xcd\xcc\xcb\xca\xc9\xc8\xc7\xc6\xc5\xc4\xc3\xc2\xc1\xc0\"",
        "\"\xbf\xbe\xbd\xbc\xbb\xba\xb9\xb8\xb7\xb6\xb5\xb4\xb3\xb2\xb1\xb0\"",
        "\"\xaf\xae\xad\xac\xab\xaa\xa9\xa8\xa7\xa6\xa5\xa4\xa3\xa2\xa1\xa0\"",
        "\"\x9f\x9e\x9d\x9c\x9b\x9a\x99\x98\x97\x96\x95\x94\x93\x92\x91\x90\"",
        "\"\x8f\x8e\x8d\x8c\x8b\x8a\x89\x88\x87\x86\x85\x84\x83\x82\x81\x80\"",
        "\"\x7f\x7e\x7d\x7c\x7b\x7a\x79\x78\x77\x76\x75\x74\x73\x72\x71\x70\"",
        "\"\x6f\x6e\x6d\x6c\x6b\x6a\x69\x68\x67\x66\x65\x64\x63\x62\x61\x60\"",
        "\"\x5f\x5e\x5d\\\x5c\x5b\x5a\x59\x58\x57\x56\x55\x54\x53\x52\x51\x50\"",
        "\"\x4f\x4e\x4d\x4c\x4b\x4a\x49\x48\x47\x46\x45\x44\x43\x42\x41\x40\"",
        "\"\x3f\x3e\x3d\x3c\x3b\x3a\x39\x38\x37\x36\x35\x34\x33\x32\x31\x30\"",
        "\"\x2f\x2e\x2d\x2c\x2b\x2a\x29\x28\x27\x26\x25\x24\x23\\\x22\x21\x20\"",
        "\"\\u001f\\u001e\\u001d\\u001c\\u001b\\u001a\\u0019\\u0018\\u0017\\u0016\\u0015\\u0014\\u0013\\u0012\\u0011\\u0010\"",
        "\"\\u000f\\u000e\\u000d\\u000c\\u000b\\u000a\\u0009\\u0008\\u0007\\u0006\\u0005\\u0004\\u0003\\u0002\\u0001\"",
    };

    assert(encoded.size() == plain.size());

    for (size_t i = 0; i < plain.size(); ++i) {
        const auto& original = encoded[i];

        size_t n_read;
        auto result = NStr::JsonDecode(original, &n_read);
        BOOST_CHECK_EQUAL(n_read, strlen(original));
        BOOST_CHECK_EQUAL(result, plain[i]);
    }
}


NCBITEST_INIT_TREE()
{
    NCBITEST_DISABLE(s_StringToInt_Speed);
    NCBITEST_DISABLE(s_StringToDouble_Speed);
}
