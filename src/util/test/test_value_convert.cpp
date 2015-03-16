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
 * Author: Sergey Sikorskiy
 *
 * File Description: 
 *      Unit-test for functions Convert() and ConvertSafe().
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <util/value_convert.hpp>
#include <corelib/test_boost.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <common/test_assert.h>  /* This header must go last */

// Tolerances in percentage units, for use with BOOST_CHECK_CLOSE.
static const float       kFloatTolerance      = 1e-3F;
static const double      kDoubleTolerance     = 1e-5;
static const long double kLongDoubleTolerance = 1e-7L;

USING_NCBI_SCOPE;

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(ValueConvertSafe)
{
    const Uint1 value_Uint1 = 255;
    const Int1 value_Int1 = -127;
    const Uint2 value_Uint2 = 64000;
    const Int2 value_Int2 = -32768;
    const Uint4 value_Uint4 = 4000000000U;
    const Int4 value_Int4 = -2147483520;
    const Uint8 value_Uint8 = 9223372036854775808ULL;
    const Int8 value_Int8 = -9223372036854775807LL;
    const float value_float = float(21.4);
    const double value_double = 42.8;
    const bool value_bool = true;
    const string value_string = "test string 0987654321";
    const char* value_char = "test char* 1234567890";
    // const char* value_binary = "test binary 1234567890 binary";
    const CTime value_CTime( CTime::eCurrent );
    const string str_value_char(value_char);
    const string str_Uint1("255");
    const string str_Int1("-127");
    const string str_Uint2("64000");
    const string str_Int2("-32768");
    const string str_Uint4("4000000000");
    const string str_Int4("-2147483520");
    const string str_Uint8("9223372036854775808");
    const string str_Int8("-9223372036854775807");
    const string str_bool("true");
    const string str_float("21.4");
    const string str_double("42.8");

    try {
        // Int8
        {
            Int8 value;

            value = ConvertSafe(value_Int8);
            BOOST_CHECK_EQUAL(value, value_Int8);

            value = ConvertSafe(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Doesn't compile ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = ConvertSafe(str_Int8);
            BOOST_CHECK_EQUAL(value, value_Int8);

            value = ConvertSafe(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            BOOST_CHECK_THROW(value = ConvertSafe(str_bool), CException);

            value = ConvertSafe(str_float);
            BOOST_CHECK_EQUAL(value, Int8(value_float));

            value = ConvertSafe(str_double);
            BOOST_CHECK_EQUAL(value, Int8(value_double));
        }

        // Uint8
        {
            Uint8 value;

            value = ConvertSafe(value_Uint8);
            BOOST_CHECK_EQUAL(value, value_Uint8);

            value = ConvertSafe(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Doesn't compile ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = ConvertSafe(str_Uint8);
            BOOST_CHECK_EQUAL(value, value_Uint8);

            value = ConvertSafe(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't convert at run-time ...
            BOOST_CHECK_THROW(value = ConvertSafe(str_bool), CException);

            value = ConvertSafe(str_float);
            BOOST_CHECK_EQUAL(value, Uint8(value_float));

            value = ConvertSafe(str_double);
            BOOST_CHECK_EQUAL(value, Uint8(value_double));
        }

        // Int4
        {
            Int4 value;

            // Doesn't compile ...
            // value = ConvertSafe(value_Uint8);

            value = ConvertSafe(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Doesn't compile ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot convert at run-time ...
            BOOST_CHECK_THROW(value = ConvertSafe(str_Int8), CException);

            value = ConvertSafe(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            BOOST_CHECK_THROW(value = ConvertSafe(str_bool), CException);

            value = ConvertSafe(str_float);
            BOOST_CHECK_EQUAL(value, Int4(value_float));

            value = ConvertSafe(str_double);
            BOOST_CHECK_EQUAL(value, Int4(value_double));
        }

        // Uint4
        {
            Uint4 value;

            value = ConvertSafe(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Doesn't compile ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot convert at run-time ...
            BOOST_CHECK_THROW(value = ConvertSafe(str_Int8), CException);

            value = ConvertSafe(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't convert at run-time ...
            BOOST_CHECK_THROW(value = ConvertSafe(str_bool), CException);

            value = ConvertSafe(str_float);
            BOOST_CHECK_EQUAL(value, Uint4(value_float));

            value = ConvertSafe(str_double);
            BOOST_CHECK_EQUAL(value, Uint4(value_double));
        }

        // Int2
        {
            Int2 value;

            value = ConvertSafe(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Doesn't compile ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int4);
            // BOOST_CHECK_EQUAL(value, value_Int4);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int2);
            // BOOST_CHECK_EQUAL(value, value_Int2);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int1);
            // BOOST_CHECK_EQUAL(value, value_Int1);

            // Doesn't compile ...
            // value = ConvertSafe(str_Uint1);
            // BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Doesn't compile ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Uint2
        {
            Uint2 value;

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Doesn't compile ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // Doesn't compile ...
            // value = ConvertSafe(str_Uint4);
            // BOOST_CHECK_EQUAL(value, value_Uint4);

            // Doesn't compile ...
            // value = ConvertSafe(str_Uint2);
            // BOOST_CHECK_EQUAL(value, value_Uint2);

            // Doesn't compile ...
            // value = ConvertSafe(str_Uint1);
            // BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Doesn't compile ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Int1
        {
            Int1 value;

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Doesn't compile ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int4);
            // BOOST_CHECK_EQUAL(value, value_Int4);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int2);
            // BOOST_CHECK_EQUAL(value, value_Int2);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int1);
            // BOOST_CHECK_EQUAL(value, value_Int1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Doesn't compile ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Uint1
        {
            Uint1 value;

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Doesn't compile ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Doesn't compile ...
            // CSafeConvPolicy doesn't allow this conversion.
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // CSafeConvPolicy doesn't allow this conversion.
            // because of return type of NStr::StringToUInt.
            // value = ConvertSafe(str_Uint4);
            // BOOST_CHECK_EQUAL(value, value_Uint4);

            // CSafeConvPolicy doesn't allow this conversion.
            // because of return type of NStr::StringToUInt.
            // value = ConvertSafe(str_Uint2);
            // BOOST_CHECK_EQUAL(value, value_Uint2);

            // CSafeConvPolicy doesn't allow this conversion.
            // because of return type of NStr::StringToUInt.
            // value = ConvertSafe(str_Uint1);
            // BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Doesn't compile ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // bool
        {
            bool value;

            value = ConvertSafe(value_Int8);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Int4);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Uint4);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Int2);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            value = ConvertSafe(value_CTime);
            BOOST_CHECK_EQUAL(value, !value_CTime.IsEmpty());

            // Won't convert at run-time ...
            // String cannot be converted to bool.
            BOOST_CHECK_THROW(value = ConvertSafe(str_Int8), CException);
            BOOST_CHECK_THROW(value = ConvertSafe(str_Uint8), CException);

            // Won't convert at run-time ...
            // String cannot be converted to bool.
            BOOST_CHECK_THROW(value = ConvertSafe(str_Int4), CException);
            BOOST_CHECK_THROW(value = ConvertSafe(str_Uint4), CException);

            // Won't convert at run-time ...
            // String cannot be converted to bool.
            BOOST_CHECK_THROW(value = ConvertSafe(str_Int2), CException);
            BOOST_CHECK_THROW(value = ConvertSafe(str_Uint2), CException);

            // Won't convert at run-time ...
            // String cannot be converted to bool.
            BOOST_CHECK_THROW(value = ConvertSafe(str_Int1), CException);
            BOOST_CHECK_THROW(value = ConvertSafe(str_Uint1), CException);

            value = ConvertSafe(str_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Won't convert at run-time ...
            // String cannot be converted to bool.
            BOOST_CHECK_THROW(value = ConvertSafe(str_float), CException);

            // Won't convert at run-time ...
            // String cannot be converted to bool.
            BOOST_CHECK_THROW(value = ConvertSafe(str_double), CException);
        } 

        // Boolean expressions ...
        {
            const string yes("yes");
            const string no("no");

            if (true && ConvertSafe(yes)) {
                BOOST_CHECK(true);
            } else {
                BOOST_CHECK(false);
            }

            if (false || ConvertSafe(no)) {
                BOOST_CHECK(false);
            } else {
                BOOST_CHECK(true);
            }

            if (ConvertSafe(yes) && ConvertSafe(yes)) {
                BOOST_CHECK(true);
            } else {
                BOOST_CHECK(false);
            }

            if (ConvertSafe(no) || ConvertSafe(no)) {
                BOOST_CHECK(false);
            } else {
                BOOST_CHECK(true);
            }

            if (!ConvertSafe(no)) {
                BOOST_CHECK(true);
            } else {
                BOOST_CHECK(false);
            }
        }

        // string
        {
            string value;

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_Int8), string);
                BOOST_CHECK_EQUAL(value, str_Int8);
            }

            {
                string value;
                value += ConvertSafe(value_Int8);
                BOOST_CHECK_EQUAL(value, str_Int8);
            }

            {
                string value = "Test 1." + ConvertSafe(value_Int8);
                value = ConvertSafe(value_Int8) + "Test 2.";
                value = value + ConvertSafe(value_Int8);
                value = ConvertSafe(value_Int8) + value;
            }

            value = ConvertSafe(value_Int8).operator string();
            BOOST_CHECK_EQUAL(value, str_Int8);

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_Uint8), string);
                BOOST_CHECK_EQUAL(value, str_Uint8);
            }

            {
                string value;
                value += ConvertSafe(value_Uint8);
                BOOST_CHECK_EQUAL(value, str_Uint8);
            }

            {
                string value = "Test 1." + ConvertSafe(value_Uint8);
                value = ConvertSafe(value_Uint8) + "Test 2.";
                value = value + ConvertSafe(value_Uint8);
                value = ConvertSafe(value_Uint8) + value;
            }

            value = ConvertSafe(value_Uint8).operator string();
            BOOST_CHECK_EQUAL(value, str_Uint8);

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_Int4), string);
                BOOST_CHECK_EQUAL(value, str_Int4);
            }

            {
                string value;
                value += ConvertSafe(value_Int4);
                BOOST_CHECK_EQUAL(value, str_Int4);
            }

            {
                string value = "Test 1." + ConvertSafe(value_Int4);
                value = ConvertSafe(value_Int4) + "Test 2.";
                value = value + ConvertSafe(value_Int4);
                value = ConvertSafe(value_Int4) + value;
            }

            value = ConvertSafe(value_Int4).operator string();
            BOOST_CHECK_EQUAL(value, str_Int4);

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_Uint4), string);
                BOOST_CHECK_EQUAL(value, str_Uint4);
            }

            {
                string value;
                value += ConvertSafe(value_Uint4);
                BOOST_CHECK_EQUAL(value, str_Uint4);
            }

            {
                string value = "Test 1." + ConvertSafe(value_Uint4);
                value = ConvertSafe(value_Uint4) + "Test 2.";
                value = value + ConvertSafe(value_Uint4);
                value = ConvertSafe(value_Uint4) + value;
            }

            value = ConvertSafe(value_Uint4).operator string();
            BOOST_CHECK_EQUAL(value, str_Uint4);

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_Int2), string);
                BOOST_CHECK_EQUAL(value, str_Int2);
            }

            {
                string value;
                value += ConvertSafe(value_Int2);
                BOOST_CHECK_EQUAL(value, str_Int2);
            }

            {
                string value = "Test 1." + ConvertSafe(value_Int2);
                value = ConvertSafe(value_Int2) + "Test 2.";
                value = value + ConvertSafe(value_Int2);
                value = ConvertSafe(value_Int2) + value;
            }

            value = ConvertSafe(value_Int2).operator string();
            BOOST_CHECK_EQUAL(value, str_Int2);

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_Uint2), string);
                BOOST_CHECK_EQUAL(value, str_Uint2);
            }

            {
                string value;
                value += ConvertSafe(value_Uint2);
                BOOST_CHECK_EQUAL(value, str_Uint2);
            }

            {
                string value = "Test 1." + ConvertSafe(value_Uint2);
                value = ConvertSafe(value_Uint2) + "Test 2.";
                value = value + ConvertSafe(value_Uint2);
                value = ConvertSafe(value_Uint2) + value;
            }

            value = ConvertSafe(value_Uint2).operator string();
            BOOST_CHECK_EQUAL(value, str_Uint2);

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_Int1), string);
                BOOST_CHECK_EQUAL(value, str_Int1);
            }

            {
                string value;
                value += ConvertSafe(value_Int1);
                BOOST_CHECK_EQUAL(value, str_Int1);
            }

            {
                string value = "Test 1." + ConvertSafe(value_Int1);
                value = ConvertSafe(value_Int1) + "Test 2.";
                value = value + ConvertSafe(value_Int1);
                value = ConvertSafe(value_Int1) + value;
            }

            value = ConvertSafe(value_Int1).operator string();
            BOOST_CHECK_EQUAL(value, str_Int1);

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_Uint1), string);
                BOOST_CHECK_EQUAL(value, str_Uint1);
            }

            {
                string value;
                value += ConvertSafe(value_Uint1);
                BOOST_CHECK_EQUAL(value, str_Uint1);
            }

            {
                string value = "Test 1." + ConvertSafe(value_Uint1);
                value = ConvertSafe(value_Uint1) + "Test 2.";
                value = value + ConvertSafe(value_Uint1);
                value = ConvertSafe(value_Uint1) + value;
            }

            value = ConvertSafe(value_Uint1).operator string();
            BOOST_CHECK_EQUAL(value, str_Uint1);

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_bool), string);
                BOOST_CHECK_EQUAL(value, str_bool);
            }

            {
                string value;
                value += ConvertSafe(value_bool);
                BOOST_CHECK_EQUAL(value, str_bool);
            }

            {
                string value = "Test 1." + ConvertSafe(value_bool);
                value = ConvertSafe(value_bool) + "Test 2.";
                value = value + ConvertSafe(value_bool);
                value = ConvertSafe(value_bool) + value;
            }

            value = ConvertSafe(value_bool).operator string();
            BOOST_CHECK_EQUAL(value, str_bool);

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_float), string);
                BOOST_CHECK_EQUAL(value, str_float);
            }

            {
                string value;
                value += ConvertSafe(value_float);
                BOOST_CHECK_EQUAL(value, str_float);
            }

            {
                string value = "Test 1." + ConvertSafe(value_float);
                value = ConvertSafe(value_float) + "Test 2.";
                value = value + ConvertSafe(value_float);
                value = ConvertSafe(value_float) + value;
            }

            value = ConvertSafe(value_float).operator string();
            BOOST_CHECK_EQUAL(value, str_float);

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_double), string);
                BOOST_CHECK_EQUAL(value, str_double);
            }

            {
                string value;
                value += ConvertSafe(value_double);
                BOOST_CHECK_EQUAL(value, str_double);
            }

            {
                string value = "Test 1." + ConvertSafe(value_double);
                value = ConvertSafe(value_double) + "Test 2.";
                value = value + ConvertSafe(value_double);
                value = ConvertSafe(value_double) + value;
            }

            value = ConvertSafe(value_double).operator string();
            BOOST_CHECK_EQUAL(value, str_double);

            //////////
            {
                string value = NCBI_CONVERT_TO(ConvertSafe(value_CTime), string);
                BOOST_CHECK_EQUAL(value, value_CTime.AsString());
            }

            {
                string value;
                value += ConvertSafe(value_CTime);
                BOOST_CHECK_EQUAL(value, value_CTime.AsString());
            }

            {
                string value = "Test 1." + ConvertSafe(value_CTime);
                value = ConvertSafe(value_CTime) + "Test 2.";
                value = value + ConvertSafe(value_CTime);
                value = ConvertSafe(value_CTime) + value;
            }

            value = ConvertSafe(value_CTime).operator string();;
            BOOST_CHECK_EQUAL(value, value_CTime.AsString());
        }

        // float
        {
            float value;

            value = ConvertSafe(value_Int8);
            BOOST_CHECK_CLOSE(value, float(value_Int8), kFloatTolerance);

            value = ConvertSafe(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            value = ConvertSafe(value_float);
            BOOST_CHECK_EQUAL(value, value_float);

            // Won't compile.
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't compile.
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Won't compile.
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // Won't compile.
            // value = ConvertSafe(str_Int4);
            // BOOST_CHECK_EQUAL(value, value_Int4);

            // Won't compile.
            // value = ConvertSafe(str_Uint4);
            // BOOST_CHECK_EQUAL(value, value_Uint4);

            // Won't compile.
            // value = ConvertSafe(str_Int2);
            // BOOST_CHECK_EQUAL(value, value_Int2);

            // Won't compile.
            // value = ConvertSafe(str_Uint2);
            // BOOST_CHECK_EQUAL(value, value_Uint2);

            // Won't compile.
            // value = ConvertSafe(str_Int1);
            // BOOST_CHECK_EQUAL(value, value_Int1);

            // Won't compile.
            // value = ConvertSafe(str_Uint1);
            // BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't compile.
            // value = ConvertSafe(str_bool);

            // Won't compile.
            // value = ConvertSafe(str_float);

            // Won't compile.
            // value = ConvertSafe(str_double);
        }

        // double
        {
            double value;

            value = ConvertSafe(value_Int8);
            BOOST_CHECK_CLOSE(value, double(value_Int8), kDoubleTolerance);

            value = ConvertSafe(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            value = ConvertSafe(value_float);
            BOOST_CHECK_EQUAL(value, value_float);

            value = ConvertSafe(value_double);
            BOOST_CHECK_EQUAL(value, value_double);

            // Won't compile.
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = ConvertSafe(str_Int8);
            BOOST_CHECK_CLOSE(value, double(value_Int8), kDoubleTolerance);

            value = ConvertSafe(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't compile.
            // value = ConvertSafe(str_bool);

            value = ConvertSafe(str_float);
            BOOST_CHECK_CLOSE(float(value), value_float, kFloatTolerance);

            value = ConvertSafe(str_double);
            BOOST_CHECK_EQUAL(value, value_double);
        }

        // long double
        {
            long double value;

            value = ConvertSafe(value_Int8);
            BOOST_CHECK_CLOSE(value, (long double)value_Int8,
                              kLongDoubleTolerance);

            value = ConvertSafe(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            value = ConvertSafe(value_float);
            BOOST_CHECK_EQUAL(value, value_float);

            value = ConvertSafe(value_double);
            BOOST_CHECK_EQUAL(value, value_double);

            // Won't compile.
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = ConvertSafe(str_Int8);
            BOOST_CHECK_CLOSE(value, (long double)value_Int8,
                              kLongDoubleTolerance);

            value = ConvertSafe(str_Uint8);
            BOOST_CHECK_CLOSE(value, (long double)value_Uint8,
                              kLongDoubleTolerance);

            value = ConvertSafe(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't compile.
            // value = ConvertSafe(str_bool);

            value = ConvertSafe(str_float);
            BOOST_CHECK_CLOSE(float(value), value_float, kFloatTolerance);

            value = ConvertSafe(str_double);
            BOOST_CHECK_CLOSE(double(value), value_double, kDoubleTolerance);
        }
    }
    catch(const CException& ex) {
        BOOST_FAIL(ex.what());
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(ValueConvertRuntime)
{
    const Uint1 value_Uint1 = 255;
    const Int1 value_Int1 = -127;
    const Uint2 value_Uint2 = 64000;
    const Int2 value_Int2 = -32768;
    const Uint4 value_Uint4 = 4000000000U;
    const Int4 value_Int4 = -2147483520;
    const Uint8 value_Uint8 = 9223372036854775809ULL;
    const Int8 value_Int8 = -9223372036854775807LL;
    const float value_float = float(21.4);
    const double value_double = 42.8;
    const bool value_bool = true;
    const string value_string = "test string 0987654321";
    const char* value_char = "test char* 1234567890";
    // const char* value_binary = "test binary 1234567890 binary";
    const CTime value_CTime( CTime::eCurrent );
    const string str_value_char(value_char);
    const string str_Uint1("255");
    const string str_Int1("-127");
    const string str_Uint2("64000");
    const string str_Int2("-32768");
    const string str_Uint4("4000000000");
    const string str_Int4("-2147483520");
    const string str_Uint8("9223372036854775809");
    const string str_Int8("-9223372036854775807");
    const string str_bool("true");
    const string str_float("21.4");
    const string str_double("42.8");

    try {
        // Int8
        {
            Int8 value;

            value = Convert(value_Int8);
            BOOST_CHECK_EQUAL(value, value_Int8);
            
            BOOST_CHECK_THROW(value = Convert(value_Uint8), CInvalidConversionException);

            value = Convert(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = Convert(str_Int8);
            BOOST_CHECK_EQUAL(value, value_Int8);

            value = Convert(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_bool), CException);

            value = Convert(str_float);
            BOOST_CHECK_EQUAL(value, Int8(value_float));

            value = Convert(str_double);
            BOOST_CHECK_EQUAL(value, Int8(value_double));
        }

        // Uint8
        {
            Uint8 value;

            value = Convert(value_Uint8);
            BOOST_CHECK_EQUAL(value, value_Uint8);
            BOOST_CHECK_THROW(value = Convert(value_Int8), CInvalidConversionException);

            value = Convert(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);
            BOOST_CHECK_THROW(value = Convert(value_Int4), CInvalidConversionException);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);
            BOOST_CHECK_THROW(value = Convert(value_Int2), CInvalidConversionException);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);
            BOOST_CHECK_THROW(value = Convert(value_Int1), CInvalidConversionException);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = Convert(str_Uint8);
            BOOST_CHECK_EQUAL(value, value_Uint8);

            value = Convert(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_bool), CException);

            value = Convert(str_float);
            BOOST_CHECK_EQUAL(value, Uint8(value_float));

            value = Convert(str_double);
            BOOST_CHECK_EQUAL(value, Uint8(value_double));
        }

        // Int4
        {
            Int4 value;

            BOOST_CHECK_THROW(value = Convert(value_Uint8), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int8), CInvalidConversionException);

            value = Convert(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);
            BOOST_CHECK_THROW(value = Convert(value_Uint4), CInvalidConversionException);

            value = Convert(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int8), CException);
            BOOST_CHECK_THROW(value = Convert(str_Uint8), CException);

            value = Convert(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_bool), CException);

            value = Convert(str_float);
            BOOST_CHECK_EQUAL(value, Int4(value_float));

            value = Convert(str_double);
            BOOST_CHECK_EQUAL(value, Int4(value_double));
        }

        // Uint4
        {
            Uint4 value;

            BOOST_CHECK_THROW(value = Convert(value_Uint8), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int8), CInvalidConversionException);

            value = Convert(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);
            BOOST_CHECK_THROW(value = Convert(value_Int4), CInvalidConversionException);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);
            BOOST_CHECK_THROW(value = Convert(value_Int2), CInvalidConversionException);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);
            BOOST_CHECK_THROW(value = Convert(value_Int1), CInvalidConversionException);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int8), CException);
            BOOST_CHECK_THROW(value = Convert(str_Uint8), CException);

            value = Convert(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_bool), CException);

            value = Convert(str_float);
            BOOST_CHECK_EQUAL(value, Uint4(value_float));

            value = Convert(str_double);
            BOOST_CHECK_EQUAL(value, Uint4(value_double));
        }

        // Int2
        {
            Int2 value;

            BOOST_CHECK_THROW(value = Convert(value_Uint8), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int8), CInvalidConversionException);

            BOOST_CHECK_THROW(value = Convert(value_Uint4), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int4), CInvalidConversionException);

            value = Convert(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);
            BOOST_CHECK_THROW(value = Convert(value_Uint2), CInvalidConversionException);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int8), CException);

            // Cannot Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int4), CInvalidConversionException);

            value = Convert(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_bool), CException);

            value = Convert(str_float);
            BOOST_CHECK_EQUAL(value, Int2(value_float));

            value = Convert(str_double);
            BOOST_CHECK_EQUAL(value, Int2(value_double));
        }

        // Uint2
        {
            Uint2 value;

            BOOST_CHECK_THROW(value = Convert(value_Uint8), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int8), CInvalidConversionException);

            BOOST_CHECK_THROW(value = Convert(value_Uint4), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int4), CInvalidConversionException);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);
            BOOST_CHECK_THROW(value = Convert(value_Int2), CInvalidConversionException);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);
            BOOST_CHECK_THROW(value = Convert(value_Int1), CInvalidConversionException);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Uint4), CInvalidConversionException);

            // Cannot Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Uint4), CInvalidConversionException);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_bool), CException);

            value = Convert(str_float);
            BOOST_CHECK_EQUAL(value, Uint2(value_float));

            value = Convert(str_double);
            BOOST_CHECK_EQUAL(value, Uint2(value_double));
        }

        // Int1
        {
            Int1 value;

            BOOST_CHECK_THROW(value = Convert(value_Uint8), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int8), CInvalidConversionException);

            BOOST_CHECK_THROW(value = Convert(value_Uint4), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int4), CInvalidConversionException);

            BOOST_CHECK_THROW(value = Convert(value_Uint2), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int2), CInvalidConversionException);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);
            BOOST_CHECK_THROW(value = Convert(value_Uint1), CInvalidConversionException);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int8), CException);

            // Cannot Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int4), CInvalidConversionException);

            // Cannot Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int2), CInvalidConversionException);

            value = Convert(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_bool), CException);

            value = Convert(str_float);
            BOOST_CHECK_EQUAL(value, Int1(value_float));

            value = Convert(str_double);
            BOOST_CHECK_EQUAL(value, Int1(value_double));
        }

        // Uint1
        {
            Uint1 value;

            BOOST_CHECK_THROW(value = Convert(value_Uint8), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int8), CInvalidConversionException);

            BOOST_CHECK_THROW(value = Convert(value_Uint4), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int4), CInvalidConversionException);

            BOOST_CHECK_THROW(value = Convert(value_Uint2), CInvalidConversionException);
            BOOST_CHECK_THROW(value = Convert(value_Int2), CInvalidConversionException);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);
            BOOST_CHECK_THROW(value = Convert(value_Int1), CInvalidConversionException);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            // value = Convert(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Uint8), CException);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Uint4), CInvalidConversionException);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Uint2), CInvalidConversionException);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int1), CException);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_bool), CException);

            value = Convert(str_float);
            BOOST_CHECK_EQUAL(value, Uint1(value_float));

            value = Convert(str_double);
            BOOST_CHECK_EQUAL(value, Uint1(value_double));
        }

        // bool
        {
            bool value;

            value = Convert(value_Int8);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Uint8);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Int4);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Uint4);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Int2);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            value = Convert(value_CTime);
            BOOST_CHECK_EQUAL(value, !value_CTime.IsEmpty());

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int8), CException);
            BOOST_CHECK_THROW(value = Convert(str_Uint8), CException);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int4), CException);
            BOOST_CHECK_THROW(value = Convert(str_Uint4), CException);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int2), CException);
            BOOST_CHECK_THROW(value = Convert(str_Uint2), CException);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_Int1), CException);
            BOOST_CHECK_THROW(value = Convert(str_Uint1), CException);

            value = Convert(str_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_float), CException);

            // Won't Convert at run-time ...
            BOOST_CHECK_THROW(value = Convert(str_double), CException);
        }

        // string
        {
            string value;

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_Int8), string);
                BOOST_CHECK_EQUAL(value, str_Int8);
            }

            {
                string value;
                value += Convert(value_Int8);
                BOOST_CHECK_EQUAL(value, str_Int8);
            }

            value = Convert(value_Int8).operator string();
            BOOST_CHECK_EQUAL(value, str_Int8);

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_Uint8), string);
                BOOST_CHECK_EQUAL(value, str_Uint8);
            }

            {
                string value;
                value += Convert(value_Uint8);
                BOOST_CHECK_EQUAL(value, str_Uint8);
            }

            value = Convert(value_Uint8).operator string();
            BOOST_CHECK_EQUAL(value, str_Uint8);

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_Int4), string);
                BOOST_CHECK_EQUAL(value, str_Int4);
            }

            {
                string value;
                value += Convert(value_Int4);
                BOOST_CHECK_EQUAL(value, str_Int4);
            }

            value = Convert(value_Int4).operator string();
            BOOST_CHECK_EQUAL(value, str_Int4);

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_Uint4), string);
                BOOST_CHECK_EQUAL(value, str_Uint4);
            }

            {
                string value;
                value += Convert(value_Uint4);
                BOOST_CHECK_EQUAL(value, str_Uint4);
            }

            value = Convert(value_Uint4).operator string();
            BOOST_CHECK_EQUAL(value, str_Uint4);

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_Int2), string);
                BOOST_CHECK_EQUAL(value, str_Int2);
            }

            {
                string value;
                value += Convert(value_Int2);
                BOOST_CHECK_EQUAL(value, str_Int2);
            }

            value = Convert(value_Int2).operator string();
            BOOST_CHECK_EQUAL(value, str_Int2);

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_Uint2), string);
                BOOST_CHECK_EQUAL(value, str_Uint2);
            }

            {
                string value;
                value += Convert(value_Uint2);
                BOOST_CHECK_EQUAL(value, str_Uint2);
            }

            value = Convert(value_Uint2).operator string();
            BOOST_CHECK_EQUAL(value, str_Uint2);

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_Int1), string);
                BOOST_CHECK_EQUAL(value, str_Int1);
            }

            {
                string value;
                value += Convert(value_Int1);
                BOOST_CHECK_EQUAL(value, str_Int1);
            }

            value = Convert(value_Int1).operator string();
            BOOST_CHECK_EQUAL(value, str_Int1);

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_Uint1), string);
                BOOST_CHECK_EQUAL(value, str_Uint1);
            }

            {
                string value;
                value += Convert(value_Uint1);
                BOOST_CHECK_EQUAL(value, str_Uint1);
            }

            value = Convert(value_Uint1).operator string();
            BOOST_CHECK_EQUAL(value, str_Uint1);

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_bool), string);
                BOOST_CHECK_EQUAL(value, str_bool);
            }

            {
                string value;
                value += Convert(value_bool);
                BOOST_CHECK_EQUAL(value, str_bool);
            }

            value = Convert(value_bool).operator string();
            BOOST_CHECK_EQUAL(value, str_bool);

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_float), string);
                BOOST_CHECK_EQUAL(value, str_float);
            }

            {
                string value;
                value += Convert(value_float);
                BOOST_CHECK_EQUAL(value, str_float);
            }

            value = Convert(value_float).operator string();
            BOOST_CHECK_EQUAL(value, str_float);

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_double), string);
                BOOST_CHECK_EQUAL(value, str_double);
            }

            {
                string value;
                value += Convert(value_double);
                BOOST_CHECK_EQUAL(value, str_double);
            }

            value = Convert(value_double).operator string();
            BOOST_CHECK_EQUAL(value, str_double);

            //////////
            {
                string value = NCBI_CONVERT_TO(Convert(value_CTime), string);
                BOOST_CHECK_EQUAL(value, value_CTime.AsString());
            }

            {
                string value;
                value += Convert(value_CTime);
                BOOST_CHECK_EQUAL(value, value_CTime.AsString());
            }

            value = Convert(value_CTime).operator string();;
            BOOST_CHECK_EQUAL(value, value_CTime.AsString());
        }

        // float
        {
            float value;

            value = Convert(value_Int8);
            BOOST_CHECK_CLOSE(value, float(value_Int8), kFloatTolerance);
            
            value = Convert(value_Uint8);
            BOOST_CHECK_CLOSE(value, float(value_Uint8), kFloatTolerance);

            value = Convert(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            value = Convert(value_float);
            BOOST_CHECK_EQUAL(value, value_float);

            value = Convert(value_double);
            BOOST_CHECK_CLOSE(value, value_double, kFloatTolerance);

            // Won't compile ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = Convert(str_Int8);
            BOOST_CHECK_CLOSE(value, float(value_Int8), kFloatTolerance);

            value = Convert(str_Uint8);
            BOOST_CHECK_CLOSE(value, float(value_Uint8), kFloatTolerance);

            value = Convert(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't Convert at run-time ...
            // value = Convert(str_bool);

            value = Convert(str_float);
            BOOST_CHECK_EQUAL(value, value_float);

            value = Convert(str_double);
            BOOST_CHECK_CLOSE(value, float(value_double), kFloatTolerance);
        }

        // double
        {
            double value;

            value = Convert(value_Int8);
            BOOST_CHECK_CLOSE(value, double(value_Int8), kDoubleTolerance);
            
            value = Convert(value_Uint8);
            BOOST_CHECK_CLOSE(value, double(value_Uint8), kDoubleTolerance);

            value = Convert(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            value = Convert(value_float);
            BOOST_CHECK_EQUAL(value, value_float);

            value = Convert(value_double);
            BOOST_CHECK_EQUAL(value, value_double);

            // Won't compile ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = Convert(str_Int8);
            BOOST_CHECK_CLOSE(value, double(value_Int8), kDoubleTolerance);

            value = Convert(str_Uint8);
            BOOST_CHECK_CLOSE(value, double(value_Uint8), kDoubleTolerance);

            value = Convert(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't Convert at run-time ...
            // value = Convert(str_bool);

            value = Convert(str_float);
            BOOST_CHECK_CLOSE(float(value), value_float, kFloatTolerance);

            value = Convert(str_double);
            BOOST_CHECK_EQUAL(value, value_double);
        }

        // long double
        {
            long double value;

            value = Convert(value_Int8);
            BOOST_CHECK_CLOSE(value, (long double)value_Int8,
                              kLongDoubleTolerance);
            
            value = Convert(value_Uint8);
            BOOST_CHECK_EQUAL(value, value_Uint8);

            value = Convert(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value != 0, value_bool);

            value = Convert(value_float);
            BOOST_CHECK_EQUAL(value, value_float);

            value = Convert(value_double);
            BOOST_CHECK_EQUAL(value, value_double);

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = Convert(str_Int8);
            BOOST_CHECK_CLOSE(value, (long double)value_Int8,
                              kLongDoubleTolerance);

            value = Convert(str_Uint8);
            BOOST_CHECK_CLOSE(value, (long double)value_Uint8,
                              kLongDoubleTolerance);

            value = Convert(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Won't Convert at run-time ...
            // value = Convert(str_bool);

            value = Convert(str_float);
            BOOST_CHECK_CLOSE(float(value), value_float, kFloatTolerance);

            value = Convert(str_double);
            BOOST_CHECK_EQUAL(value, value_double);
        }
    }
    catch(const CException& ex) {
        BOOST_FAIL(ex.what());
    }
}


////////////////////////////////////////////////////////////////////////////////
typedef long CS_LONG;

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(TypeProxy)
{
    try {
        {
            typedef Int4 MyInt4;

            MyInt4 value = Convert(string("1"));
            BOOST_CHECK_EQUAL(value, 1);
        }

        {
            typedef int CS_INT;

            CS_INT value = Convert(string("1"));
            BOOST_CHECK_EQUAL(value, 1);
        }

        {
            CS_LONG value = Convert(string("1"));
            BOOST_CHECK_EQUAL(value, 1);
        }

    }
    catch(const CException& ex) {
        BOOST_FAIL(ex.what());
    }
}

