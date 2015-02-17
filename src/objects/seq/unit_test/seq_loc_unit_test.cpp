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
 *   Unit test for CSeq_loc and some closely related code
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <objects/seqloc/seqloc__.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <boost/test/parameterized_test.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);


CRef<CSeq_loc> MakeLoc(const char* text)
{
    string input = text;
    if ( input.find("::=") == NPOS ) {
        input = "Seq-loc::="+input;
    }
    CNcbiIstrstream str(input.c_str());
    CRef<CSeq_loc> loc(new CSeq_loc);
    str >> MSerial_AsnText >> *loc;
    return loc;
}


template<class Obj>
string MakeASN(const Obj& loc)
{
    CNcbiOstrstream str;
    str << MSerial_AsnText << loc;
    return CNcbiOstrstreamToString(str);
}


BOOST_AUTO_TEST_CASE(CSeq_loc_CI_Test1)
{
    CRef<CSeq_loc> loc =
        MakeLoc("int { from 10, to 20, id gi 2 }");
    
    CSeq_loc_CI it(*loc);
    BOOST_CHECK(!it.HasEquivParts());

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(2));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(10, 20));
    BOOST_CHECK(!it.IsSetStrand());
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(!it.IsPoint());
    BOOST_CHECK(!it.IsInEquivPart());
    ++it;

    BOOST_CHECK(!it);
}


BOOST_AUTO_TEST_CASE(CSeq_loc_CI_Test2)
{
    CRef<CSeq_loc> loc =
        MakeLoc("mix {"
                " int { from 10, to 20, strand plus, id gi 2 },"
                " pnt { point 30, strand minus, id gi 3}"
                "}");
    
    CSeq_loc_CI it(*loc);
    BOOST_CHECK(!it.HasEquivParts());

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(2));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(10, 20));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_plus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(!it.IsPoint());
    BOOST_CHECK(!it.IsInEquivPart());
    ++it;

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(3));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(30, 30));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_minus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(it.IsPoint());
    BOOST_CHECK(!it.IsInEquivPart());
    ++it;

    BOOST_CHECK(!it);
}


BOOST_AUTO_TEST_CASE(CSeq_loc_CI_Test3)
{
    CRef<CSeq_loc> loc =
        MakeLoc("mix {"
                " int { from 10, to 20, strand plus, id gi 2 },"
                " equiv {"
                "  int { from 25, to 27, strand plus, id gi 2 },"
                "  mix {"
                "   int { from 25, to 26, strand plus, id gi 2 },"
                "   int { from 27, to 27, strand minus, id gi 2 }"
                "  }"
                " },"
                " pnt { point 30, strand minus, id gi 3}"
                "}");
    
    CSeq_loc_CI it(*loc);
    BOOST_CHECK(it.HasEquivParts());

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(2));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(10, 20));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_plus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(!it.IsPoint());
    BOOST_CHECK(!it.IsInEquivPart());
    ++it;

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(2));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(25, 27));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_plus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(!it.IsPoint());
    BOOST_CHECK(it.IsInEquivPart());
    ++it;

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(2));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(25, 26));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_plus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(!it.IsPoint());
    BOOST_CHECK(it.IsInEquivPart());
    ++it;

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(2));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(27, 27));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_minus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(it.IsPoint());
    BOOST_CHECK(it.IsInEquivPart());
    ++it;

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(3));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(30, 30));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_minus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(it.IsPoint());
    BOOST_CHECK(!it.IsInEquivPart());
    ++it;

    BOOST_CHECK(!it);
}


BOOST_AUTO_TEST_CASE(CSeq_loc_I_Test1)
{
    CRef<CSeq_loc> loc =
        MakeLoc("mix {"
                " int { from 10, to 20, strand plus, id gi 2 },"
                " pnt { point 30, strand minus, id gi 3}"
                "}");
    
    CSeq_loc_I it(*loc);
    BOOST_CHECK(!it.HasEquivParts());

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(2));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(10, 20));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_plus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(!it.IsPoint());
    BOOST_CHECK(!it.IsInEquivPart());
    it.SetTo(10);
    ++it;

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(3));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(30, 30));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_minus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(it.IsPoint());
    BOOST_CHECK(!it.IsInEquivPart());
    it.SetSeq_id_Handle(CSeq_id_Handle::GetGiHandle(2));
    it.SetStrand(eNa_strand_plus);
    ++it;

    BOOST_CHECK(!it);

    string loc2 = MakeASN(*it.MakeSeq_loc());
    BOOST_CHECK_EQUAL(loc2,
                      "Seq-loc ::= packed-pnt {\n"
                      "  strand plus,\n"
                      "  id gi 2,\n"
                      "  points {\n"
                      "    10,\n"
                      "    30\n"
                      "  }\n"
                      "}\n");
}


BOOST_AUTO_TEST_CASE(CSeq_loc_I_Test2)
{
    CRef<CSeq_loc> loc =
        MakeLoc("mix {"
                " int { from 10, to 20, strand plus, id gi 2 },"
                " pnt { point 30, strand minus, id gi 3}"
                "}");
    
    CSeq_loc_I it(*loc);
    BOOST_CHECK(!it.HasEquivParts());

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(2));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(10, 20));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_plus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(!it.IsPoint());
    BOOST_CHECK(!it.IsInEquivPart());
    it.SetTo(10);
    ++it;

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(3));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(30, 30));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_minus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(it.IsPoint());
    BOOST_CHECK(!it.IsInEquivPart());
    it.SetSeq_id_Handle(CSeq_id_Handle::GetGiHandle(2));
    ++it;

    BOOST_CHECK(!it);

    string loc2 = MakeASN(*it.MakeSeq_loc());
    BOOST_CHECK_EQUAL(loc2,
                      "Seq-loc ::= packed-int {\n"
                      "  {\n"
                      "    from 10,\n"
                      "    to 10,\n"
                      "    strand plus,\n"
                      "    id gi 2\n"
                      "  },\n"
                      "  {\n"
                      "    from 30,\n"
                      "    to 30,\n"
                      "    strand minus,\n"
                      "    id gi 2\n"
                      "  }\n"
                      "}\n");
}


BOOST_AUTO_TEST_CASE(CSeq_loc_I_Test3)
{
    CRef<CSeq_loc> loc =
        MakeLoc("mix {"
                " int { from 10, to 20, strand plus, id gi 2 },"
                " pnt { point 30, strand minus, id gi 3}"
                "}");
    
    CSeq_loc_I it(*loc);
    BOOST_CHECK(!it.HasEquivParts());

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(2));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(10, 20));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_plus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(!it.IsPoint());
    BOOST_CHECK(!it.IsInEquivPart());
    it.SetTo(10);
    ++it;

    BOOST_REQUIRE(it);
    BOOST_CHECK_EQUAL(it.GetSeq_id_Handle(), CSeq_id_Handle::GetGiHandle(3));
    BOOST_CHECK(it.GetSeq_id().IsGi());
    BOOST_CHECK_EQUAL(it.GetRange(), CRange<TSeqPos>(30, 30));
    BOOST_CHECK(it.IsSetStrand());
    BOOST_CHECK_EQUAL(int(it.GetStrand()), int(eNa_strand_minus));
    BOOST_CHECK(!it.IsWhole());
    BOOST_CHECK(!it.IsEmpty());
    BOOST_CHECK(it.IsPoint());
    BOOST_CHECK(!it.IsInEquivPart());
    it.SetSeq_id_Handle(CSeq_id_Handle::GetGiHandle(2));
    ++it;

    BOOST_CHECK(!it);

    string loc2 = MakeASN(*it.MakeSeq_loc(it.eMake_PreserveType));
    BOOST_CHECK_EQUAL(loc2,
                      "Seq-loc ::= mix {\n"
                      "  int {\n"
                      "    from 10,\n"
                      "    to 10,\n"
                      "    strand plus,\n"
                      "    id gi 2\n"
                      "  },\n"
                      "  pnt {\n"
                      "    point 30,\n"
                      "    strand minus,\n"
                      "    id gi 2\n"
                      "  }\n"
                      "}\n");
}
