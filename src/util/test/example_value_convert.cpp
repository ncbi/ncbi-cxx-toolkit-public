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
 *      Example application for functions Convert() and ConvertSafe().
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <util/value_convert.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;

////////////////////////////////////////////////////////////////////////////////
namespace t1
{
	void ConsumeData(Int8 value)
	{
		value += value;
	}
}

namespace t2
{
	void ConsumeData(Uint8 value)
	{
		value += value;
	}
}

namespace t3
{
	void ConsumeData(Int2 value)
	{
		value += value;
	}
}

namespace t4
{
	// Overloaded functions ...
	
	void ConsumeData(Int8 value)
	{
		value += value;
	}
	void ConsumeData(Uint8 value)
	{
		value += value;
	}
}

int main (void)
{
    const Uint4 value_Uint4 = 4000000000U;
    const Uint8 value_Uint8 = 9223372036854775800ULL;

    const string str_Uint4("4000000000");
    const string str_Uint8("9223372036854775800");




	//////////////////////////////
	// Old-fashioned way ...
	//////////////////////////////
	


	/////////////////////////////
	// Numeric data types ...
	t1::ConsumeData(static_cast<Int8>(value_Uint8)); // Not safe ...
	t2::ConsumeData(value_Uint8); // Safe, not portable ...
	
	
	////////////////////////////
	// String data ...
	
	t1::ConsumeData(NStr::StringToInt8(str_Uint8));
	t2::ConsumeData(NStr::StringToUInt8(str_Uint8));

	t1::ConsumeData(NStr::StringToInt8(str_Uint4));
	t2::ConsumeData(NStr::StringToUInt8(str_Uint4));

	// Not safe, not readable ...
	// t3::ConsumeData(static_cast<Int2>(NStr::StringToInt(str_Uint4))); // Will throw an exception.
	// Invalid conversion.
	t3::ConsumeData(static_cast<Int2>(NStr::StringToUInt(str_Uint4)));
	// Invalid conversion.
	t3::ConsumeData(static_cast<Int2>(NStr::StringToInt8(str_Uint4)));
	
	// It is also possible to check value range manually.
	Int8 tmp_value = NStr::StringToInt8(str_Uint4);
	if (tmp_value < numeric_limits<Int2>::min() || tmp_value > numeric_limits<Int2>::max()) 
	{
		// throw string("Invalid conversion");
	}
	t3::ConsumeData(static_cast<Int2>(tmp_value));
	

	
	//////////////////////////////
	// New way ...
	//////////////////////////////
	
	

	//////////////////////////////
	// Numeric data types ...
	
	// t1::ConsumeData(ConvertSafe(value_Uint8));
	// Conversion is is ambiguous. numeric_limits<Uint8>::max() > numeric_limits<Int8>::max()
	
	t2::ConsumeData(ConvertSafe(value_Uint8));

	t1::ConsumeData(ConvertSafe(value_Uint4));
	t2::ConsumeData(ConvertSafe(value_Uint4));

	// If you still need to convert a bigger data type to a smaller one,
	// compile-time check can be replaced with run-time one.
	try {
		t1::ConsumeData(Convert(value_Uint8));

		t2::ConsumeData(Convert(value_Uint8));
		t2::ConsumeData(Convert(value_Uint4));
	} catch (const CException&) {
		// Oops ...
	}

	// The same with variables.
	//
	Int8 result_Int8 = 0;
	Int4 result_Int4 = 0;

	// result_Int8 = ConvertSafe(value_Uint8); // ERROR.
	result_Int8 = ConvertSafe(value_Uint4); 
        _ASSERT(result_Int8 == value_Uint4);
	// result_Int4 = ConvertSafe(value_Uint8); // ERROR.
	// result_Int4 = ConvertSafe(value_Uint4); // ERROR.
	
	try {
		result_Int8 = Convert(value_Uint8);
		result_Int8 = Convert(value_Uint4); 
		result_Int4 = Convert(value_Uint8);
		result_Int4 = Convert(value_Uint4);
	} catch (const CException&) {
		// Oops ...
	}

	
	////////////////////////////
	// String data ...
	
	t1::ConsumeData(ConvertSafe(str_Uint8));
	t2::ConsumeData(ConvertSafe(str_Uint8));

	t1::ConsumeData(ConvertSafe(str_Uint4));
	t2::ConsumeData(ConvertSafe(str_Uint4));

	// !!! ATTENTION !!!
	// t3::ConsumeData(ConvertSafe(str_Uint4)); // !!! Compilation error. !!!
	// int StringToInt(const CTempString& str) returns "int".
	// It is not safe to convert "int" to "Int2". Use run-time conversion
	// function instead.

	try {
		t3::ConsumeData(Convert(str_Uint4)); 
	} catch (const CException&) {
		// Oops ...
	}

	// The same with variables.
	//
	result_Int8 = ConvertSafe(str_Uint8); // May throw an exception at run-time.
	result_Int8 = ConvertSafe(str_Uint4); 
	// result_Int4 = ConvertSafe(str_Uint8); // May throw an exception at run-time.
	// result_Int4 = ConvertSafe(str_Uint4); // May throw an exception at run-time.

	try {
		result_Int8 = Convert(str_Uint8);
		result_Int8 = Convert(str_Uint4); 
		result_Int4 = Convert(str_Uint8);
		result_Int4 = Convert(str_Uint4);
                _ASSERT(result_Int4 == static_cast<Int4>(value_Uint4));
	} catch (const CException&) {
		// Oops ...
	}

	////////////////////////////
	// Overloaded functions ...
	// 1) Use static_cast ...
	t4::ConsumeData(static_cast<Uint8>(ConvertSafe(str_Uint8)));
	// 2) Explicitly call conversion operator ...
	t4::ConsumeData(ConvertSafe(str_Uint8).operator Int8());
	
	return 0;
}
