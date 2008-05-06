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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/version.hpp>
#include <corelib/ncbi_xstr.hpp>
#include <algorithm>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

#include <common/test_assert.h>  /* This header must go last */


// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;


#define OK  NcbiCout << " completed successfully!" << NcbiEndl
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
    double      d;

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
        return s == NStr::TruncateSpaces(str, NStr::eTrunc_Both);
    }
};

#define DF 0

//                str  flags  num   int   uint  Int8  Uint8  double
#define BAD(v)   {  v, DF, -1, kBad, kBad, kBad, kBad, kBad }
#define STR(v)   { #v, DF, NCBI_CONST_INT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), v##. }
#define STRI(v)  { #v, DF, -1, NCBI_CONST_INT8(v), kBad, NCBI_CONST_INT8(v), kBad, v##. }
#define STRU(v)  { #v, DF, -1, kBad, NCBI_CONST_UINT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), v##. }
#define STR8(v)  { #v, DF, -1, kBad, kBad, NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), v##. }
#define STRI8(v) { #v, DF, -1, kBad, kBad, NCBI_CONST_INT8(v), kBad, v##. }
#define STRU8(v) { #v, DF, -1, kBad, kBad, kBad, NCBI_CONST_UINT8(v), v##. }
#define STRD(v)  { #v, DF, -1, kBad, kBad, kBad, kBad, v##. }

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
    STRD(-9223372036854775809),
    { "-9223372036854775808", DF, -1, kBad, kBad, NCBI_CONST_INT8(-9223372036854775807)-1, kBad, -9223372036854775808. },
    STRI8(-9223372036854775807),
    STR8(9223372036854775806),
    STR8(9223372036854775807),
    STRU8(9223372036854775808),
    STRU8(18446744073709551614),
    STRU8(18446744073709551615),
    STRD(18446744073709551616),

    BAD(""),
    BAD("."),
    BAD(".."),
    BAD("abc"),
    { ".0",  DF, -1, kBad, kBad, kBad, kBad,  .0  },
    BAD(".0."),
    BAD("..0"),
    { ".01", DF, -1, kBad, kBad, kBad, kBad,  .01 },
    { "1.",  DF, -1, kBad, kBad, kBad, kBad,  1.  },
    { "1.1", DF, -1, kBad, kBad, kBad, kBad,  1.1 },
    BAD("1.1."),
    BAD("1.."),
    STRI(-123),
    BAD("--123"),
    { "+123", DF, 123, 123, 123, 123, 123, 123 },
    BAD("++123"),
    BAD("- 123"),
    BAD(" 123"),

    { " 123",     NStr::fAllowLeadingSpaces,  -1, 123,  123,  123,  123,  123. },
    { " 123",     NStr::fAllowLeadingSymbols, -1, 123,  123,  123,  123,  123. },
    BAD("123 "),
    { "123 ",     NStr::fAllowTrailingSpaces,  -1, 123,  123,  123,  123,  123. },
    { "123 ",     NStr::fAllowTrailingSymbols, -1, 123,  123,  123,  123,  123. },
    { "123(45) ", NStr::fAllowTrailingSymbols, -1, 123,  123,  123,  123,  123. },
    { " 123 ",    NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces, -1, 123,  123,  123,  123,  123. },
    
    { "1,234",    NStr::fAllowCommas, -1,    1234,    1234,    1234,    1234, kBad },
    { "1,234,567",NStr::fAllowCommas, -1, 1234567, 1234567, 1234567, 1234567, kBad },
    { "12,34",    NStr::fAllowCommas, -1,    kBad,    kBad,    kBad,    kBad, kBad },
    { ",123",     NStr::fAllowCommas, -1,    kBad,    kBad,    kBad,    kBad, kBad },
    { ",123",     NStr::fAllowCommas | NStr::fAllowLeadingSymbols, -1, 123, 123, 123, 123, 123 },

    { "+123",     0, 123, 123, 123, 123, 123, 123 },
    { "123",      NStr::fMandatorySign, 123, kBad, kBad, kBad, kBad, kBad },
    { "+123",     NStr::fMandatorySign, 123,  123,  123,  123,  123, 123 },
    { "-123",     NStr::fMandatorySign,  -1, -123, kBad, -123, kBad, -123 },
    { "+123",     NStr::fAllowLeadingSymbols, 123,  123,  123,  123,  123,  123 },
    { "-123",     NStr::fAllowLeadingSymbols,  -1, -123, kBad, -123, kBad, -123 }
};

BOOST_AUTO_TEST_CASE(s_StringToNum)
{
    NcbiCout << NcbiEndl << "NStr::StringTo*() tests...";

    const size_t count = sizeof(s_Str2NumTests) / sizeof(s_Str2NumTests[0]);

    for (size_t i = 0;  i < count;  ++i) {
        const SStringNumericValues* test = &s_Str2NumTests[i];
        const char*                 str  = test->str;
        NStr::TStringToNumFlags flags = test->flags;
        NStr::TNumToStringFlags str_flags = 0;
        if ( flags & NStr::fMandatorySign ) 
            str_flags |= NStr::fWithSign;
        if ( flags & NStr::fAllowCommas )
            str_flags |= NStr::fWithCommas;
        bool allow_same_test = (flags < NStr::fAllowLeadingSpaces);

        NcbiCout << "\n*** Checking string '" << str << "'***" << NcbiEndl;

        // num
        {{
            int value = NStr::StringToNumeric(str);
            NcbiCout << "numeric value: " << value << ", toString: '"
                     << NStr::IntToString(value) << "'" << NcbiEndl;
            BOOST_CHECK_EQUAL(value, test->num);
        }}

        // int
        try {
            int value = NStr::StringToInt(str, flags);
            NcbiCout << "int value: " << value << ", toString: '"
                     << NStr::IntToString(value, str_flags) << "'"
                     << NcbiEndl;
            BOOST_CHECK(test->IsGoodInt());
            BOOST_CHECK_EQUAL(value, test->i);
            if (allow_same_test)
                BOOST_CHECK(test->Same(NStr::IntToString(value, str_flags)));
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGoodInt() ) {
                ERR_POST("Cannot convert '" << str << "' to int");
            }
            BOOST_CHECK(!test->IsGoodInt());
        }

        // unsigned int
        try {
            unsigned int value = NStr::StringToUInt(str, flags);
            NcbiCout << "unsigned int value: " << value << ", toString: '"
                     << NStr::UIntToString(value, str_flags) << "'"
                     << NcbiEndl;
            BOOST_CHECK(test->IsGoodUInt());
            BOOST_CHECK_EQUAL(value, test->u);
            if (allow_same_test)
                BOOST_CHECK(test->Same(NStr::UIntToString(value, str_flags)));
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGoodUInt() ) {
                ERR_POST("Cannot convert '" << str << "' to unsigned int");
            }
            BOOST_CHECK(!test->IsGoodUInt());
        }

        // long
        try {
            long value = NStr::StringToLong(str, flags);
            NcbiCout << "long value: " << value << ", toString: '"
                     << NStr::IntToString(value, str_flags) << "', "
                     << "Int8ToString: '"
                     << NStr::Int8ToString(value, str_flags) << "'"
                     << NcbiEndl;
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(test->IsGoodInt());
                BOOST_CHECK_EQUAL(value, test->i);
                if (allow_same_test)
                    BOOST_CHECK(test->Same(NStr::IntToString(value, str_flags)));
            #else
                BOOST_CHECK(test->IsGoodInt8());
                BOOST_CHECK_EQUAL(value, test->i8);
                if (allow_same_test)
                    BOOST_CHECK(test->Same(NStr::Int8ToString(value, str_flags)));
            #endif
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            #if (SIZEOF_LONG == SIZEOF_INT)
                if ( test->IsGoodInt() ) {
                    ERR_POST("Cannot convert '" << str <<
                             "' to unsigned long");
                }
                BOOST_CHECK(!test->IsGoodInt());
            #else
                if ( test->IsGoodInt8() ) {
                    ERR_POST("Cannot convert '" << str <<
                             "' to unsigned long");
                }
                BOOST_CHECK(!test->IsGoodInt8());
            #endif
        }

        // unsigned long
        try {
            unsigned long value = NStr::StringToULong(str, flags);
            NcbiCout << "unsigned long value: " << value << ", toString: '"
                     << NStr::UIntToString(value, str_flags) << "', "
                     << "UInt8ToString: '"
                     << NStr::UInt8ToString(value, str_flags) << "'"
                     << NcbiEndl;
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(test->IsGoodUInt());
                BOOST_CHECK_EQUAL(value, test->u);
                if (allow_same_test)
                    BOOST_CHECK(test->Same(NStr::UIntToString(value, str_flags)));
            #else
                BOOST_CHECK(test->IsGoodUInt8());
                BOOST_CHECK_EQUAL(value, test->u8);
                if (allow_same_test)
                    BOOST_CHECK(test->Same(NStr::UInt8ToString(value, str_flags)));
            #endif
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            #if (SIZEOF_LONG == SIZEOF_INT)
                if ( test->IsGoodUInt() ) {
                    ERR_POST("Cannot convert '" << str <<
                             "' to unsigned long");
                }
                BOOST_CHECK(!test->IsGoodUInt());
            #else
                if ( test->IsGoodUInt8() ) {
                    ERR_POST("Cannot convert '" << str <<
                             "' to unsigned long");
                }
                BOOST_CHECK(!test->IsGoodUInt8());
            #endif
        }

        // Int8
        try {
            Int8 value = NStr::StringToInt8(str, flags);
            NcbiCout << "Int8 value: " << value << ", toString: '"
                     << NStr::Int8ToString(value, str_flags) << "'"
                     << NcbiEndl;
            BOOST_CHECK(test->IsGoodInt8());
            BOOST_CHECK_EQUAL(value, test->i8);
            if (allow_same_test)
                BOOST_CHECK(test->Same(NStr::Int8ToString(value, str_flags)));
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGoodInt8() ) {
                ERR_POST("Cannot convert '" << str << "' to Int8");
            }
            BOOST_CHECK(!test->IsGoodInt8());
        }

        // Uint8
        try {
            Uint8 value = NStr::StringToUInt8(str, flags);
            NcbiCout << "Uint8 value: " << value << ", toString: '"
                     << NStr::UInt8ToString(value, str_flags) << "'"
                     << NcbiEndl;
            BOOST_CHECK(test->IsGoodUInt8());
            BOOST_CHECK_EQUAL(value, test->u8);
            if (allow_same_test)
                BOOST_CHECK(test->Same(NStr::UInt8ToString(value, str_flags)));
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGoodUInt8() ) {
                ERR_POST("Cannot convert '" << str << "' to Uint8");
            }
            BOOST_CHECK(!test->IsGoodUInt8());
        }

        // double
        try {
            double value = NStr::StringToDouble(str, flags);
            NcbiCout << "double value: " << value << ", toString: '"
                     << NStr::DoubleToString(value, str_flags) << "'"
                     << NcbiEndl;
            BOOST_CHECK(test->IsGoodDouble());
            BOOST_CHECK_EQUAL(value, test->d);
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGoodDouble() ) {
                ERR_POST("Cannot convert '" << str << "' to double");
            }
            BOOST_CHECK(!test->IsGoodDouble());
        }
    }
    OK;
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

    bool Same(const string& s) const {
        if ( s.empty() ) {
            return true;
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

        return (NStr::strncmp(s1, s2, n) == 0);
    }
};

static const SRadixTest s_RadixTests[] = {
    { "A",         16, 10 },
    { "0xA",       16, 10 },
    { "B9",        16, 185 },
    { "C5D",       16, 3165 },
    { "FFFF",      16, 65535 },
    { "17ABCDEF",  16, 397135343 },
    { "BADBADBA",  16, 3134959034U },
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


BOOST_AUTO_TEST_CASE(s_StringToNumRadix)
{
    NcbiCout << NcbiEndl << "NStr::StringTo*() radix tests...";

    const size_t count = sizeof(s_RadixTests)/sizeof(s_RadixTests[0]);
    for (size_t i = 0;  i < count;  ++i) 
    {
        const SRadixTest* test = &s_RadixTests[i];
        NcbiCout << NcbiEndl 
                 << "Checking numeric string: '" << test->str 
                 << "': with base " << test->base << NcbiEndl;

        // Int
        try {
            if ( test->value <= (Uint8)kMax_Int ) {
                int val = NStr::StringToInt(test->str, 0, test->base);
                string str;
                if ( test->base ) {
                    NStr::IntToString(str, val, 0, test->base);
                }
                NcbiCout << "Int value: " << val 
                        << ", toString: '" << str << "'" 
                        << NcbiEndl;
                BOOST_CHECK_EQUAL((Uint8)val, test->value);
                BOOST_CHECK(test->Same(str));
            }
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }


        // UInt
        try {
            if ( test->value <= kMax_UInt ) {
                unsigned int val = 
                    NStr::StringToUInt(test->str, 0, test->base);
                string str;
                if ( test->base ) {
                    NStr::UIntToString(str, val, 0, test->base);
                }
                NcbiCout << "UInt value: " << val 
                        << ", toString: '" << str << "'" 
                        << NcbiEndl;
                BOOST_CHECK_EQUAL(val, test->value);
                BOOST_CHECK(test->Same(str));
            }
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }

        // Int8
        try {
            if ( test->value <= (Uint8)kMax_I8 ) {
                Int8 val = NStr::StringToInt8(test->str, 0, test->base);
                string str;
                if ( test->base ) {
                    NStr::Int8ToString(str, val, 0, test->base);
                }
                NcbiCout << "Int8 value: " << val 
                        << ", toString: '" << str << "'" 
                        << NcbiEndl;
                BOOST_CHECK_EQUAL((Uint8)val, test->value);
                BOOST_CHECK(test->Same(str));
            }
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }

        // Uint8
        try {
            Uint8 val = NStr::StringToUInt8(test->str, 0, test->base);
            string str;
            if ( test->base ) {
                NStr::UInt8ToString(str, val, 0, test->base);
            }
            NcbiCout << "Uint8 value: " << (unsigned)val 
                     << ", toString: '" << str << "'" 
                     << NcbiEndl;
            BOOST_CHECK_EQUAL(val, test->value);
            BOOST_CHECK(test->Same(str));
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }
    } 

    // Some additional tests

    string str;

    NStr::IntToString(str, kMax_Long, 0, 2);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "1111111111111111111111111111111");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "111111111111111111111111111111111111111111111111111111111111111");
#endif

    NStr::UIntToString(str, kMax_ULong, 0, 2);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "11111111111111111111111111111111");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "1111111111111111111111111111111111111111111111111111111111111111");
#endif

    NStr::IntToString(str, -1, 0, 8);
#if (SIZEOF_LONG == 4) 
    BOOST_CHECK(str == "37777777777");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "1777777777777777777777");
#endif

    NStr::IntToString(str, -1, 0, 16);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "FFFFFFFF");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "FFFFFFFFFFFFFFFF");
#endif

    NStr::UInt8ToString(str, NCBI_CONST_UINT8(12345678901234567), 0, 16);
    BOOST_CHECK(str == "2BDC545D6B4B87");

    OK;
}


//----------------------------------------------------------------------------
// NStr::StringTo*_DataSize()
//----------------------------------------------------------------------------

struct SStringDataSizeValues
{
    const char*             str;
    NStr::TStringToNumFlags flags;
    int                     base;
    Uint8                   expected;

    bool IsGood(void) const {
        return expected != (Uint8)kBad;
    }
};

static const SStringDataSizeValues s_Str2DataSizeTests[] = {
    // str  flags  base  num
    { "10",    0, 10,      10 },
    { "10k",   0, 10, 10*1024 },
    { "10K",   0, 10, 10*1024 },
    { "10KB",  0, 10, 10*1024 },
    { "10M",   0, 10, 10*1024*1024 },
    { "10MB",  0, 10, 10*1024*1024 },
    { "10K",   0,  2,  2*1024 },
    { "10K",   0,  8,  8*1024 },
    { "AK",    0, 16, 10*1024 },
    { "10K",   0,  0, 10*1024 },
    { "+10K",  0,  0, 10*1024 },
    { "-10K",  0,  0, kBad },
    { "1GBx",  0, 10, kBad },
    { "10000000000000GB", 0, 10, kBad },
    { " 10K",  0, 10, kBad },
    { " 10K",  (1<<12)/*NStr::fAllowLeadingSpaces*/,  10, 10*1024 },
    { "10K ",  0, 10, kBad },
    { "10K ",  (1<<14)/*NStr::fAllowTrailingSpaces*/,  10, 10*1024 },
    { "10K",   (1<<10)/*NStr::fMandatorySign*/, 10, kBad },
    { "+10K",  0, 10, 10*1024 },
    { "+10K",  (1<<10)/*NStr::fMandatorySign*/, 10, 10*1024 },
    { "-10K",  0, 10, kBad },
    { "-10K",  (1<<10)/*NStr::fMandatorySign*/, 10, kBad },
    { "1,000K", 0, 10, kBad },
    { "1,000K",(1<<11)/*NStr::fAllowCommas*/,   10, 1000*1024 },
    { "K10K",  0, 10, kBad },
    { "K10K",  (1<<12)|(1<<13)/*NStr::fAllowLeadingSymbols*/, 10, 10*1024 },
    { "K10K",  (1<<12)|(1<<13)/*NStr::fAllowLeadingSymbols*/, 10, 10*1024 },
    { "10KG",  (1<<14)|(1<<15)/*NStr::fAllowTrailingSymbols*/,10, 10*1024 }
};

BOOST_AUTO_TEST_CASE(s_StringToNumDataSize)
{
    NcbiCout << NcbiEndl << "NStr::StringToUInt8_DataSize() tests...";

    const size_t count = sizeof(s_Str2DataSizeTests) /
                         sizeof(s_Str2DataSizeTests[0]);

    for (size_t i = 0;  i < count;  ++i)
    {
        const SStringDataSizeValues* test  = &s_Str2DataSizeTests[i];
        const char*                  str   = test->str;
        NStr::TStringToNumFlags      flags = test->flags;
        NcbiCout << "\n*** Checking string '" << str << "'***" << NcbiEndl;

        try {
            Uint8 value = NStr::StringToUInt8_DataSize(str, flags,test->base);
            NcbiCout << "value: " << value << NcbiEndl;
            BOOST_CHECK(test->IsGood());
            BOOST_CHECK_EQUAL(value, test->expected);
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGood() ) {
                ERR_POST("Cannot convert '" << str << "' to data size");
            } else {
                NcbiCout << "value: bad" << NcbiEndl;
            }
            BOOST_CHECK(!test->IsGood());
        }
    }
    OK;
}


//----------------------------------------------------------------------------
// NStr::Replace()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_Replace)
{
    NcbiCout << NcbiEndl << "NStr::Replace() tests...";

    string src("aaabbbaaccczzcccXX");
    string dst;

    string search("ccc");
    string replace("RrR");
    NStr::Replace(src, search, replace, dst);
    BOOST_CHECK_EQUAL(dst, string("aaabbbaaRrRzzRrRXX"));

    search = "a";
    replace = "W";
    NStr::Replace(src, search, replace, dst, 6, 1);
    BOOST_CHECK_EQUAL(dst, string("aaabbbWaccczzcccXX"));
    
    search = "bbb";
    replace = "BBB";
    NStr::Replace(src, search, replace, dst, 50);
    BOOST_CHECK_EQUAL(dst, string("aaabbbaaccczzcccXX"));

    search = "ggg";
    replace = "no";
    dst = NStr::Replace(src, search, replace);
    BOOST_CHECK_EQUAL(dst, string("aaabbbaaccczzcccXX"));

    search = "a";
    replace = "A";
    dst = NStr::Replace(src, search, replace);
    BOOST_CHECK_EQUAL(dst, string("AAAbbbAAccczzcccXX"));

    search = "X";
    replace = "x";
    dst = NStr::Replace(src, search, replace, src.size() - 1);
    BOOST_CHECK_EQUAL(dst, string("aaabbbaaccczzcccXx"));

    // ReplaceInPlace

    search = "a";
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

    OK;
}


//----------------------------------------------------------------------------
// NStr::PrintableString/ParseEscapes()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_PrintableString)
{
    NcbiCout << NcbiEndl << "NStr::PrintableString() tests...";

    // NStr::PrintableString()
    BOOST_CHECK(NStr::PrintableString(kEmptyStr).empty());
    BOOST_CHECK(NStr::PrintableString
           ("AB\\CD\nAB\rCD\vAB?\tCD\'AB\"").compare
           ("AB\\\\CD\\nAB\\rCD\\vAB\?\\tCD\\\'AB\\\"") == 0);
    BOOST_CHECK(NStr::PrintableString
           ("A\x01\r\202\x000F\0205B" + string(1,'\0') + "CD").compare
           ("A\\1\\r\\202\\17\\0205B\\0CD") == 0);
    BOOST_CHECK(NStr::PrintableString
           ("A\x01\r\202\x000F\0205B" + string(1,'\0') + "CD",
            NStr::fPrintable_Full).compare
           ("A\\001\\r\\202\\017\\0205B\\000CD") == 0);

    // NStr::ParseEscapes
    BOOST_CHECK(NStr::ParseEscapes(kEmptyStr).empty());
    BOOST_CHECK(NStr::ParseEscapes
           ("AB\\\\CD\\nAB\\rCD\\vAB\?\\tCD\\\'AB\\\"").compare
           ("AB\\CD\nAB\rCD\vAB?\tCD\'AB\"") == 0);
    BOOST_CHECK(NStr::ParseEscapes
           ("A\\1\\r2\\17\\0205B\\x00CD\\x000F").compare
           ("A\x01\r2\x0F\x10""5B\xCD\x0F") == 0);

    OK;
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
    NcbiCout << NcbiEndl << "NStr::Compare() tests...";

    const SStrCompare* rec;
    const size_t count = sizeof(s_StrCompare) / sizeof(s_StrCompare[0]);

    for (size_t i = 0;  i < count;  i++) {
        rec = &s_StrCompare[i];

        string s1 = rec->s1;
        s_CompareStr(NStr::Compare(s1, rec->s2, NStr::eCase), rec->case_res);
        s_CompareStr(NStr::Compare(s1, rec->s2, NStr::eNocase),
                     rec->nocase_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, rec->s2, NStr::eCase),
                     rec->n_case_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, rec->s2, NStr::eNocase),
                     rec->n_nocase_res);

        string s2 = rec->s2;
        s_CompareStr(NStr::Compare(s1, s2, NStr::eCase), rec->case_res);
        s_CompareStr(NStr::Compare(s1, s2, NStr::eNocase), rec->nocase_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, s2, NStr::eCase),
                     rec->n_case_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, s2, NStr::eNocase),
                     rec->n_nocase_res);
    }

    BOOST_CHECK(NStr::Compare("0123", 0, 2, "12") <  0);
    BOOST_CHECK(NStr::Compare("0123", 1, 2, "12") == 0);
    BOOST_CHECK(NStr::Compare("0123", 2, 2, "12") >  0);
    BOOST_CHECK(NStr::Compare("0123", 3, 2,  "3") == 0);

    OK;
}

BOOST_AUTO_TEST_CASE(s_XCompare)
{
    NcbiCout << NcbiEndl << "XStr::Compare() tests...";

    const SStrCompare* rec;
    const size_t count = sizeof(s_StrCompare) / sizeof(s_StrCompare[0]);

    for (size_t i = 0;  i < count;  i++) {
        rec = &s_StrCompare[i];

        string s1 = rec->s1;
        s_CompareStr(XStr::Compare(s1, rec->s2, XStr::eCase),   rec->case_res);
        s_CompareStr(XStr::Compare(s1, rec->s2, XStr::eNocase),
                     rec->nocase_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, rec->s2, XStr::eCase),
                     rec->n_case_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, rec->s2, XStr::eNocase),
                     rec->n_nocase_res);

        string s2 = rec->s2;
        s_CompareStr(XStr::Compare(s1, s2, XStr::eCase), rec->case_res);
        s_CompareStr(XStr::Compare(s1, s2, XStr::eNocase), rec->nocase_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, s2, XStr::eCase),
                     rec->n_case_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, s2, XStr::eNocase),
                     rec->n_nocase_res);
    }

    BOOST_CHECK(XStr::Compare("0123", 0, 2, "12") <  0);
    BOOST_CHECK(XStr::Compare("0123", 1, 2, "12") == 0);
    BOOST_CHECK(XStr::Compare("0123", 2, 2, "12") >  0);
    BOOST_CHECK(XStr::Compare("0123", 3, 2,  "3") == 0);

    OK;
}


//----------------------------------------------------------------------------
// NStr::Split()
//----------------------------------------------------------------------------

static const string s_SplitStr[] = {
    "ab+cd+ef",
    "aaAAabBbbb",
    "-abc-def--ghijk---",
    "a12c3ba45acb678bc",
    "nodelim",
    "emptydelim",
    ";",
    ""
};

static const string s_SplitDelim[] = {
    "+", "AB", "-", "abc", "*", "", ";", "*"
};

static const string split_result[] = {
    "ab", "cd", "ef",
    "aa", "ab", "bbb",
    "abc", "def", "ghijk",
    "12", "3", "45", "678",
    "nodelim",
    "emptydelim"
};

BOOST_AUTO_TEST_CASE(s_Split)
{
    NcbiCout << NcbiEndl << "NStr::Split() tests...";

    list<string> split;

    for (size_t i = 0; i < sizeof(s_SplitStr) / sizeof(s_SplitStr[0]); i++) {
        NStr::Split(s_SplitStr[i], s_SplitDelim[i], split);
    }
    size_t i = 0;
    ITERATE(list<string>, it, split) {
        BOOST_CHECK(i < sizeof(split_result) / sizeof(split_result[0]));
        BOOST_CHECK(NStr::Compare(*it, split_result[i++]) == 0);
    }
    OK;
}


//----------------------------------------------------------------------------
// NStr::Tokenize()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_Tokenize)
{
    NcbiCout << NcbiEndl << "NStr::Tokenize() tests...";

    static const string s_TokStr[] = {
        "ab+cd+ef",
        "123;45,78",
        "1;",
        ";1",
        "emptydelim"
    };
    static const string s_TokDelim[] = {
        "+", ";,", ";", ";", ""
    };
    static const string tok_result[] = {
        "ab", "cd", "ef",
        "123", "45", "78",
        "1", "", 
        "", "1",
        "emptydelim"
    };

    vector<string> tok;

    for (size_t i = 0; i < sizeof(s_TokStr) / sizeof(s_TokStr[0]); ++i) {
        NStr::Tokenize(s_TokStr[i], s_TokDelim[i], tok);               
    }
    {{
        size_t i = 0;
        ITERATE(vector<string>, it, tok) {
            BOOST_CHECK_EQUAL(NStr::Compare(*it, tok_result[i++]), 0);
        }
    }}
    
    tok.clear();

    for (size_t i = 0; i < sizeof(s_SplitStr) / sizeof(s_SplitStr[0]); i++) {
        NStr::Tokenize(s_SplitStr[i], s_SplitDelim[i], tok,
                       NStr::eMergeDelims);
    }
    {{
        size_t i = 0;
        ITERATE(vector<string>, it, tok) {
            BOOST_CHECK_EQUAL(NStr::Compare(*it, split_result[i++]), 0);
        }
    }}

    OK;
}


//----------------------------------------------------------------------------
// NStr::SplitInTo()
//----------------------------------------------------------------------------

struct SSplitInTwo {
    const char* str;
    const char* delim;
    const char* expected_str1;
    const char* expected_str2;
    bool        expected_ret;
};
    
static const SSplitInTwo s_SplitInTwoTest[] = {
    { "ab+cd+ef", "+", "ab", "cd+ef", true },
    { "aaAAabBbbb", "AB", "aa", "AabBbbb", true },
    { "aaCAabBCbbb", "ABC", "aa", "AabBCbbb", true },
    { "-beg-delim-", "-", "", "beg-delim-", true },
    { "end-delim:", ":", "end-delim", "", true },
    { "nodelim", ".,:;-+", "nodelim", "", false },
    { "emptydelim", "", "emptydelim", "", false },
    { "", "emtpystring", "", "", false },
    { "", "", "", "", false }
};

BOOST_AUTO_TEST_CASE(s_SplitInTwo)
{
    NcbiCout << NcbiEndl << "NStr::SplitInTwo() tests...";

    string string1, string2;
    bool   result;
    const size_t count = sizeof(s_SplitInTwoTest) /
                         sizeof(s_SplitInTwoTest[0]);
    for (size_t i = 0; i < count; i++) {
        result = NStr::SplitInTwo(s_SplitInTwoTest[i].str,
                                  s_SplitInTwoTest[i].delim,
                                  string1, string2);
        BOOST_CHECK_EQUAL(s_SplitInTwoTest[i].expected_ret, result);
        BOOST_CHECK_EQUAL(s_SplitInTwoTest[i].expected_str1, string1);
        BOOST_CHECK_EQUAL(s_SplitInTwoTest[i].expected_str2, string2);
    }
    OK;
}


//----------------------------------------------------------------------------
// NStr::TokenizePattern()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_TokenizePattern)
{
    NcbiCout << NcbiEndl << "NStr::TokenizePattern() tests...";

    static const string tok_pattern_str =
        "<tag>begin<tag>text<tag><tag><tag>end<tag>";
    static const string tok_pattern_delim = "<tag>";
    static const string tok_pattern_result_m[] = {
        "begin", "text", "end"
    };
    static const string tok_pattern_result_nm[] = {
        "", "begin", "text",  "", "", "end", ""
    };

    vector<string> tok;

    NStr::TokenizePattern(tok_pattern_str, tok_pattern_delim,
                          tok, NStr::eMergeDelims);
    {{
        BOOST_CHECK_EQUAL(sizeof(tok_pattern_result_m)/sizeof(tok_pattern_result_m[0]),
               tok.size());
        size_t i = 0;
        ITERATE(vector<string>, it, tok) {
            BOOST_CHECK_EQUAL(NStr::Compare(*it, tok_pattern_result_m[i++]), 0);
        }
    }}
    tok.clear();

    NStr::TokenizePattern(tok_pattern_str, tok_pattern_delim,
                          tok, NStr::eNoMergeDelims);
    {{
        BOOST_CHECK_EQUAL(sizeof(tok_pattern_result_nm)/sizeof(tok_pattern_result_nm[0]),
               tok.size());
        size_t i = 0;
        ITERATE(vector<string>, it, tok) {
            BOOST_CHECK_EQUAL(NStr::Compare(*it, tok_pattern_result_nm[i++]), 0);
        }
    }}
    OK;
}


//----------------------------------------------------------------------------
// NStr::ToLower/ToUpper()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_Case)
{
    NcbiCout << NcbiEndl << "NStr::ToLower/ToUpper() tests...";

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
        char indiff[sizeof(s_Indiff) + 1];
        ::strcpy(indiff, s_Indiff);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, indiff), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
        ::strcpy(indiff, s_Indiff);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToUpper(indiff)), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
    }}
    {{
        string indiff;
        indiff = s_Indiff;
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, indiff), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
        indiff = s_Indiff;
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToUpper(indiff)), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
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
            ::strcpy(x_str, s_Tri[i].orig);
            BOOST_CHECK_EQUAL(NStr::Compare(NStr::ToUpper(x_str), s_Tri[i].x_upper),0);
            BOOST_CHECK_EQUAL(NStr::Compare(x_lower, NStr::ToLower(x_str)), 0);
        }}
        {{
            string x_str;
            x_lower = s_Tri[i].x_lower;
            x_str = s_Tri[i].orig;
            BOOST_CHECK_EQUAL(NStr::Compare(NStr::ToLower(x_str), x_lower), 0);
            x_str = s_Tri[i].orig;
            BOOST_CHECK_EQUAL(NStr::Compare(NStr::ToUpper(x_str), s_Tri[i].x_upper),0);
            BOOST_CHECK_EQUAL(NStr::Compare(x_lower, NStr::ToLower(x_str)), 0);
        }}
    }
    OK;
}


//----------------------------------------------------------------------------
// NStr::str[n]casecmp()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_strcasecmp)
{
    NcbiCout << NcbiEndl << "NStr::str[n]casecmp() tests...";

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

    OK;
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

    NcbiCout << NcbiEndl << "AStrEquiv tests...";

    BOOST_CHECK_EQUAL( AStrEquiv(as1, as2, PNocase()), true );
    BOOST_CHECK_EQUAL( AStrEquiv(as1, as3, PNocase()), true );
    BOOST_CHECK_EQUAL( AStrEquiv(as3, as4, PNocase()), false );
    BOOST_CHECK_EQUAL( AStrEquiv(as1, as2, PCase()),   true );
    BOOST_CHECK_EQUAL( AStrEquiv(as1, as3, PCase()),   false );
    BOOST_CHECK_EQUAL( AStrEquiv(as2, as4, PCase()),   false );
    OK;

    NcbiCout << NcbiEndl << "Equal{Case,Nocase} tests...";

    BOOST_CHECK_EQUAL( NStr::EqualNocase(as1, as2), true );
    BOOST_CHECK_EQUAL( NStr::EqualNocase(as1, as3), true );
    BOOST_CHECK_EQUAL( NStr::EqualNocase(as3, as4), false );
    BOOST_CHECK_EQUAL( NStr::EqualCase(as1, as2),   true );
    BOOST_CHECK_EQUAL( NStr::EqualCase(as1, as3),   false );
    BOOST_CHECK_EQUAL( NStr::EqualCase(as2, as4),   false );
    OK;
}


//----------------------------------------------------------------------------
// Reference counting
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_ReferenceCounting)
{
    NcbiCout << NcbiEndl 
             << "Testing string reference counting properties:"
             << NcbiEndl;

    string s1(10, '1');
    string s2(s1);
    if ( s1.data() != s2.data() ) {
        NcbiCout << "BAD: string reference counting is OFF" << NcbiEndl;
    }
    else {
        NcbiCout << "GOOD: string reference counting is ON" << NcbiEndl;
        for (size_t i = 0; i < 4; i++) {
            NcbiCout << "Restoring reference counting"<<NcbiEndl;
            s2 = s1;
            if ( s1.data() != s2.data() ) {
                NcbiCout << "BAD: cannot restore string reference " \
                    "counting" << NcbiEndl;
                continue;
            }
            NcbiCout << "GOOD: reference counting is ON" << NcbiEndl;

            const char* type = i&1? "str.begin()": "str.c_str()";
            NcbiCout << "Calling "<<type<<NcbiEndl;
            if ( i&1 ) {
                s2.begin();
            }
            else {
                s1.c_str();
            }
            if ( s1.data() == s2.data() ) {
                NcbiCout << "GOOD: "<< type 
                            <<" doesn't affect reference counting"<< NcbiEndl;
                continue;
            }
            NcbiCout << "OK: "<< type 
                        << " turns reference counting OFF" << NcbiEndl;

            NcbiCout << "Restoring reference counting"<<NcbiEndl;
            s2 = s1;
            if ( s1.data() != s2.data() ) {
                NcbiCout << "BAD: " << type 
                            <<" turns reference counting OFF completely"
                            << NcbiEndl;
                continue;
            }
            NcbiCout << "GOOD: reference counting is ON" << NcbiEndl;

            if ( i&1 ) continue;

            NcbiCout << "Calling " << type << " on source" << NcbiEndl;
            s1.c_str();
            if ( s1.data() != s2.data() ) {
                NcbiCout << "BAD: "<< type
                            << " on source turns reference counting OFF"
                            << NcbiEndl;
            }

            NcbiCout << "Calling "<< type <<" on destination" << NcbiEndl;
            s2.c_str();
            if ( s1.data() != s2.data() ) {
                NcbiCout << "BAD: " << type
                         <<" on destination turns reference counting OFF"
                         << NcbiEndl;
            }
        }
    }
    OK;
}


//----------------------------------------------------------------------------
// NStr::FindNoCase()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_FindNoCase)
{
    NcbiCout << NcbiEndl << "NStr::FindNoCase() tests...";
    BOOST_CHECK_EQUAL(NStr::FindNoCase(" abcd", " xyz"), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindNoCase(" abcd", " xyz", 0, NPOS, NStr::eLast), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindNoCase(" abcd", " aBc", 0, NPOS, NStr::eLast), 0);

    OK;
}


//----------------------------------------------------------------------------
// CVersionInfo:: parse from str
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_VersionInfo)
{
    NcbiCout << NcbiEndl << "CVersionInfo::FromStr tests...";
    {{
        CVersionInfo ver("1.2.3");
        BOOST_CHECK_EQUAL(ver.GetMajor(), 1);
        BOOST_CHECK_EQUAL(ver.GetMinor(), 2);
        BOOST_CHECK_EQUAL(ver.GetPatchLevel(), 3);

        ver.FromStr("12.35");
        BOOST_CHECK_EQUAL(ver.GetMajor(), 12);
        BOOST_CHECK_EQUAL(ver.GetMinor(), 35);
        BOOST_CHECK_EQUAL(ver.GetPatchLevel(), 0);
        {{
            bool err_catch = false;
            try {
                ver.FromStr("12.35a");
            }
            catch (exception&) {
                err_catch = true;
            }
            BOOST_CHECK(err_catch);
        }}
    }}
    OK;

    NcbiCout << NcbiEndl << "ParseVersionString tests...";

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

    OK;
}

BOOST_AUTO_TEST_CASE(s_StringUTF8)
{
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

    NcbiCout << NcbiEndl << "CStringUTF8 tests...";

    const char *src, *res, *conv;
    src = (char*)s_ExtAscii;
    conv= (char*)s_Converted;
    CStringUTF8 str(src);
    res = str.c_str();

    BOOST_CHECK_EQUAL( strncmp(src,res,126), 0);
    BOOST_CHECK_EQUAL( strlen(res+127), 256);
    res += 127;
    BOOST_CHECK_EQUAL( strncmp(conv,res,256), 0);

    string sample("micro=µ Agrave=À atilde=ã ccedil=ç");
    str = sample;
    BOOST_CHECK_EQUAL(strcmp(str.c_str(),"micro=Âµ Agrave=Ã€ atilde=Ã£ ccedil=Ã§"), 0);
    BOOST_CHECK_EQUAL( str.AsLatin1(), sample);
    OK;
}

