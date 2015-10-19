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
 * Author:  Eugene Vasilchenko, NCBI
 *
 * File Description:
 *   Unit test for CUser_object and CUser_field
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <objects/general/general__.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <util/util_exception.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static string ToASNString(const CSerialObject& obj)
{
    CNcbiOstrstream out;
    out << MSerial_AsnText << obj;
    return CNcbiOstrstreamToString(out);
}


static string ToASNString(const CUser_object& obj, ESerialDataFormat format)
{
    CNcbiStrstream str;
    str << MSerial_Format(format) << obj;
    CUser_object obj2;
    str >> MSerial_Format(format) >> obj2;
    return ToASNString(obj2);
}


void TestAllFormats(const CUser_object& obj, const char* asn_text)
{
    BOOST_CHECK_EQUAL(ToASNString(obj), asn_text);
    BOOST_CHECK_EQUAL(ToASNString(obj, eSerial_AsnText), asn_text);
    BOOST_CHECK_EQUAL(ToASNString(obj, eSerial_AsnBinary), asn_text);
    BOOST_CHECK_EQUAL(ToASNString(obj, eSerial_Xml), asn_text);
}


BOOST_AUTO_TEST_CASE(s_TestInt8_1)
{
    CRef<CUser_object> obj(new CUser_object);
    obj->SetType().SetId(1);

    obj->ResetData();
    obj->AddField("string", "123456789", CUser_object::eParse_Number);
    TestAllFormats(*obj,
                   "User-object ::= {\n"
                   "  type id 1,\n"
                   "  data {\n"
                   "    {\n"
                   "      label str \"string\",\n"
                   "      data int 123456789\n"
                   "    }\n"
                   "  }\n"
                   "}\n");

    obj->ResetData();
    obj->AddField("string", "-123456789", CUser_object::eParse_Number);
    TestAllFormats(*obj,
                   "User-object ::= {\n"
                   "  type id 1,\n"
                   "  data {\n"
                   "    {\n"
                   "      label str \"string\",\n"
                   "      data int -123456789\n"
                   "    }\n"
                   "  }\n"
                   "}\n");

    obj->ResetData();
    obj->AddField("double", 123456789.);
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"double\",\n"
                      "      data real { 123456789, 10, 0 }\n"
                      "    }\n"
                      "  }\n"
                      "}\n");

    obj->ResetData();
    obj->AddField("double", -123456789.);
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"double\",\n"
                      "      data real { -123456789, 10, 0 }\n"
                      "    }\n"
                      "  }\n"
                      "}\n");

    obj->ResetData();
    obj->AddField("Int8", Int8(123456789));
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"Int8\",\n"
                      "      data int 123456789\n"
                      "    }\n"
                      "  }\n"
                      "}\n");

    obj->ResetData();
    obj->AddField("Int8", Int8(-123456789));
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"Int8\",\n"
                      "      data int -123456789\n"
                      "    }\n"
                      "  }\n"
                      "}\n");
}


BOOST_AUTO_TEST_CASE(s_TestInt8_2)
{
    CRef<CUser_object> obj(new CUser_object);
    obj->SetType().SetId(1);

    obj->ResetData();
    obj->AddField("string", "12345678901234", CUser_object::eParse_Number);
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"string\",\n"
                      "      data real { 12345678901234, 10, 0 }\n"
                      "    }\n"
                      "  }\n"
                      "}\n");

    obj->ResetData();
    obj->AddField("string", "-12345678901234", CUser_object::eParse_Number);
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"string\",\n"
                      "      data real { -12345678901234, 10, 0 }\n"
                      "    }\n"
                      "  }\n"
                      "}\n");

    obj->ResetData();
    obj->AddField("double", 12345678901234.);
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"double\",\n"
                      "      data real { 12345678901234, 10, 0 }\n"
                      "    }\n"
                      "  }\n"
                      "}\n");

    obj->ResetData();
    obj->AddField("double", -12345678901234.);
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"double\",\n"
                      "      data real { -12345678901234, 10, 0 }\n"
                      "    }\n"
                      "  }\n"
                      "}\n");

    obj->ResetData();
    obj->AddField("Int8", Int8(12345678901234));
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"Int8\",\n"
                      "      data real { 12345678901234, 10, 0 }\n"
                      "    }\n"
                      "  }\n"
                      "}\n");

    obj->ResetData();
    obj->AddField("Int8", Int8(-12345678901234));
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"Int8\",\n"
                      "      data real { -12345678901234, 10, 0 }\n"
                      "    }\n"
                      "  }\n"
                      "}\n");
}


BOOST_AUTO_TEST_CASE(s_TestInt8_3)
{
    CRef<CUser_object> obj(new CUser_object);
    obj->SetType().SetId(1);

    obj->ResetData();
    obj->AddField("string", "123456789012345678", CUser_object::eParse_Number);
    TestAllFormats(*obj,
                   "User-object ::= {\n"
                   "  type id 1,\n"
                   "  data {\n"
                   "    {\n"
                   "      label str \"string\",\n"
                   "      data real { 123456789012346, 10, 3 }\n"
                   "    }\n"
                   "  }\n"
                   "}\n");

    obj->ResetData();
    obj->AddField("string", "-123456789012345678", CUser_object::eParse_Number);
    TestAllFormats(*obj,
                   "User-object ::= {\n"
                   "  type id 1,\n"
                   "  data {\n"
                   "    {\n"
                   "      label str \"string\",\n"
                   "      data real { -123456789012346, 10, 3 }\n"
                   "    }\n"
                   "  }\n"
                   "}\n");

    obj->ResetData();
    obj->AddField("double", 123456789012345678.);
    TestAllFormats(*obj,
                   "User-object ::= {\n"
                   "  type id 1,\n"
                   "  data {\n"
                   "    {\n"
                   "      label str \"double\",\n"
                   "      data real { 123456789012346, 10, 3 }\n"
                   "    }\n"
                   "  }\n"
                   "}\n");

    obj->ResetData();
    obj->AddField("double", -123456789012345678.);
    TestAllFormats(*obj,
                   "User-object ::= {\n"
                   "  type id 1,\n"
                   "  data {\n"
                   "    {\n"
                   "      label str \"double\",\n"
                   "      data real { -123456789012346, 10, 3 }\n"
                   "    }\n"
                   "  }\n"
                   "}\n");

    obj->ResetData();
    obj->AddField("Int8", Int8(123456789012345678));
    TestAllFormats(*obj,
                   "User-object ::= {\n"
                   "  type id 1,\n"
                   "  data {\n"
                   "    {\n"
                   "      label str \"Int8\",\n"
                   "      data str \"123456789012345678\"\n"
                   "    }\n"
                   "  }\n"
                   "}\n");

    obj->ResetData();
    obj->AddField("Int8", Int8(-123456789012345678));
    TestAllFormats(*obj,
                   "User-object ::= {\n"
                   "  type id 1,\n"
                   "  data {\n"
                   "    {\n"
                   "      label str \"Int8\",\n"
                   "      data str \"-123456789012345678\"\n"
                   "    }\n"
                   "  }\n"
                   "}\n");
}


BOOST_AUTO_TEST_CASE(s_TestGi_1)
{
    CRef<CUser_object> obj(new CUser_object);
    obj->SetType().SetId(1);

    obj->ResetData();
    obj->AddField("GI", GI_CONST(123456789));
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"GI\",\n"
                      "      data int 123456789\n"
                      "    }\n"
                      "  }\n"
                      "}\n");

    obj->ResetData();
    obj->AddField("GI", GI_CONST(-123456789));
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"GI\",\n"
                      "      data int -123456789\n"
                      "    }\n"
                      "  }\n"
                      "}\n");
}


#ifdef NCBI_INT8_GI
BOOST_AUTO_TEST_CASE(s_TestGi_2)
{
    CRef<CUser_object> obj(new CUser_object);
    obj->SetType().SetId(1);

    obj->ResetData();
    obj->AddField("GI", GI_CONST(12345678901234));
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"GI\",\n"
                      "      data real { 12345678901234, 10, 0 }\n"
                      "    }\n"
                      "  }\n"
                      "}\n");

    obj->ResetData();
    obj->AddField("GI", GI_CONST(-12345678901234));
    BOOST_CHECK_EQUAL(ToASNString(*obj),
                      "User-object ::= {\n"
                      "  type id 1,\n"
                      "  data {\n"
                      "    {\n"
                      "      label str \"GI\",\n"
                      "      data real { -12345678901234, 10, 0 }\n"
                      "    }\n"
                      "  }\n"
                      "}\n");
}


BOOST_AUTO_TEST_CASE(s_TestGi_3)
{
    CRef<CUser_object> obj(new CUser_object);
    obj->SetType().SetId(1);

    obj->ResetData();
    obj->AddField("GI", GI_CONST(123456789012345678));
    TestAllFormats(*obj,
                   "User-object ::= {\n"
                   "  type id 1,\n"
                   "  data {\n"
                   "    {\n"
                   "      label str \"GI\",\n"
                   "      data str \"123456789012345678\"\n"
                   "    }\n"
                   "  }\n"
                   "}\n");

    obj->ResetData();
    obj->AddField("GI", GI_CONST(-123456789012345678));
    TestAllFormats(*obj,
                   "User-object ::= {\n"
                   "  type id 1,\n"
                   "  data {\n"
                   "    {\n"
                   "      label str \"GI\",\n"
                   "      data str \"-123456789012345678\"\n"
                   "    }\n"
                   "  }\n"
                   "}\n");
}
#endif
