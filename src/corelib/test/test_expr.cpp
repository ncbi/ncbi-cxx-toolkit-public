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
 * Author: Sergey Sikorskiy, Mikhail Zakharov
 *
 * File Description: 
 *      Unit tests for expresiion parsing and evaluation.
 *
 * ===========================================================================
 */


#include <ncbi_pch.hpp>

#include <corelib/ncbifile.hpp>
#include <corelib/expr.hpp>
#include <corelib/test_boost.hpp>

//#include <corelib/ncbifile.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(ParseInt)
{
    CExprParser parser;

    // Int ...
    parser.Parse("1");
    BOOST_CHECK_EQUAL(CExprValue::eINT, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(1, parser.GetResult().GetInt());

    parser.Parse("1 + 1");
    BOOST_CHECK_EQUAL(CExprValue::eINT, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(2, parser.GetResult().GetInt());

    parser.Parse("abc + def");

    parser.Parse("1 + 2 * 3");
    BOOST_CHECK_EQUAL(CExprValue::eINT, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(7, parser.GetResult().GetInt());
    
    parser.Parse("(1 + 2) * 3");
    BOOST_CHECK_EQUAL(CExprValue::eINT, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(9, parser.GetResult().GetInt());
}


////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(ParseDouble)
{
    CExprParser parser;

    // Double ...
    parser.Parse("1.0");
    BOOST_CHECK_EQUAL(CExprValue::eFLOAT, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(1.0, parser.GetResult().GetDouble());

    parser.Parse("pi");
    BOOST_CHECK_EQUAL(CExprValue::eFLOAT, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(3.1415926535897932385E0, parser.GetResult().GetDouble());

    parser.Parse("e");
    BOOST_CHECK_EQUAL(CExprValue::eFLOAT, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(2.7182818284590452354E0, parser.GetResult().GetDouble());
    
    parser.Parse("abs(-1)");
    BOOST_CHECK_EQUAL(CExprValue::eFLOAT, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(1.0, parser.GetResult().GetDouble());
}


////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(ParseBool)
{
    CExprParser parser;

    // Boolean ...
    parser.Parse("true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("true && true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("true && false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("false && true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("false && false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("true || true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("true || false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("false || true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("false || false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("true || true && true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("false || true && true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("(false || true) && true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("true || false && true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("(true || false) && true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("true || true && false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("true && true || true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("false && (true || true)");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("false && true || true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("true && false || true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("true && true || false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("false && true || false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("false && (true || false)");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("2 > 1");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("1 > 2");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("1 != 2");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("true != false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("1 == 1");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("false == false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("true == true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("true ^ true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("false ^ false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("true ^ false");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("false ^ true");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(AddSymbol)
{
    CExprParser parser;

    parser.AddSymbol("1", Int8(1));
    parser.AddSymbol("2", double(2.0));
    parser.AddSymbol("3", false);

    // request for member 'invalid_data_type' in '* value', which is of
    // non-class type 'const char'
    // parser.AddSymbol("4", (const char*)4);
    // Call is ambiguous
    // parser.AddSymbol("5", 0);
    // Call is ambiguous
    // parser.AddSymbol("5", 1);
    // 'void*' is not a pointer-to-object type
    // parser.AddSymbol("4", (void*)0);
}

////////////////////////////////////////////////////////////////////////////////
/// Test string 2 bool function: checks if given directory exists
bool DirectoryExists(const string& DirectoryName)
{
    CDir Directory(DirectoryName);

    return Directory.Exists();
}

/// Boost test case checks parsing of string literals
BOOST_AUTO_TEST_CASE(ParseString)
{
    CExprParser parser;

    // String literal
    parser.Parse("\"abc\"");
    BOOST_CHECK_EQUAL(CExprValue::eSTRING, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL("abc", parser.GetResult().GetString());

    // String literal concatenation by "+"
    parser.Parse("\"abc\"+\"def\"");
    BOOST_CHECK_EQUAL(CExprValue::eSTRING, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL("abcdef", parser.GetResult().GetString());

    // All other operations on strings throw
    BOOST_CHECK_THROW(parser.Parse("\"abc\"-\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"*\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"/\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"\\\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"&&\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"||\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"^^\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("!\"abc\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("-\"abc\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("~\"abc\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"%\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"<\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\">\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"&\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"|\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"^\"def\""), CExprParserException);
    BOOST_CHECK_THROW(parser.Parse("\"abc\"**\"def\""), CExprParserException);

    // String to Bool function call
    parser.AddSymbol("dir", DirectoryExists);

#ifndef NCBI_OS_MSWIN 
    parser.Parse("dir(\"/var/log\")");
#else
    parser.Parse("dir(\"c:\\\")");
#endif
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("dir(\"/var/NotExistingDirectory\")");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    // String ==/!= check
    string Dummy("SomeDummyString");

    parser.AddSymbol("var1", Dummy);
    parser.Parse("var1 == \"SomeDammyString\"");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());

    parser.Parse("var1 == \"SomeDummyString\"");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("var1 != \"SomeDammyString\"");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    parser.Parse("var1 != \"SomeDummyString\"");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());}

/// Test special no-division mode
BOOST_AUTO_TEST_CASE(NoDivisionMode)
{
    // Switch NoDivision mode
    CExprParser parser(CExprParser::fLogicalOnly);

    // Int
    parser.AddSymbol("/var/log", 2);
    parser.Parse("/var/log");
    BOOST_CHECK_EQUAL(CExprValue::eINT, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(2, parser.GetResult().GetInt());

    // Boolean
    parser.AddSymbol("/var/log1", true);
    parser.Parse("/var/log1");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    // Check that dot is actually disabled and allowed as variable name
    parser.AddSymbol("0.1.0/main/Group1/Test1/0.0.2", true);
    parser.Parse("0.1.0/main/Group1/Test1/0.0.2");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(true, parser.GetResult().GetBool());

    // Check that dot is actually disabled and allowed as variable name
    parser.AddSymbol("0.1.0/main/Group1/Test1/0.0.2", true);
    parser.Parse("false || 0.1.0/main/Group2/Test1/0.0.2");
    BOOST_CHECK_EQUAL(CExprValue::eBOOL, parser.GetResult().GetType());
    BOOST_CHECK_EQUAL(false, parser.GetResult().GetBool());
}

