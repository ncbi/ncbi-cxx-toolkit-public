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
* Author:  Ilya Dondoshansky
*
* File Description:
*   Unit test module for different implementations of the IBlastSeqInfoSrc 
*   interface, providing access to sequence identifiers and lengths. 
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <algo/blast/api/seqinfosrc_seqvec.hpp>
#include <algo/blast/api/seqinfosrc_seqdb.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include "test_objmgr.hpp"

using namespace std;
USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

BOOST_AUTO_TEST_SUITE(seqinfosrc)

BOOST_AUTO_TEST_CASE(testSeqVecSeqInfoSrc)
{
    const int kNumSeqs = 20;
    const int kGiList[kNumSeqs] = 
       { 15641222, 14520853, 43933406, 8347191, 42882855, 17384706, 
         43512473, 43792134, 21928942, 2780777, 7444295, 42867691,
         21229103, 30064920, 6648639, 44512031, 23024816, 37679720,
         27694999, 22984137 };
    const string kIdPrefix("gi|");
    const Uint4 kOid = 15;
    const int kGoodLength = 304;
    const char* kGoodIdStr = "44512031";

    int index;
    TSeqLocVector seqv;

    for (index = 0; index < kNumSeqs; ++index) {
        CSeq_id seqid(kIdPrefix + NStr::IntToString(kGiList[index]));
        auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(seqid));
        seqv.push_back(*sl);
    }
    CSeqVecSeqInfoSrc seqinfo_src(seqv);
    Uint4 length = seqinfo_src.GetLength(kOid);
    BOOST_REQUIRE_EQUAL(kGoodLength, (int)length);
    CRef<CSeq_id> seqid(seqinfo_src.GetId(kOid).front());
    BOOST_REQUIRE(!strcmp(kGoodIdStr, seqid->GetSeqIdString().c_str()));
}

BOOST_AUTO_TEST_CASE(testEmptySeqVec)
{
    TSeqLocVector seqv;
    BOOST_REQUIRE_THROW(CSeqVecSeqInfoSrc seqinfo_src(seqv), CBlastException);
}

BOOST_AUTO_TEST_CASE(testSeqVecIndexOutOfRange)
{
    const int kIndex = 2;
    TSeqLocVector seqv;
    CSeq_id seqid("gi|555");
    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(seqid));
    seqv.push_back(*sl);
    CSeqVecSeqInfoSrc seqinfo_src(seqv);
    Uint4 length;
    BOOST_REQUIRE_THROW(length = seqinfo_src.GetLength(kIndex),
                        CBlastException);
}

BOOST_AUTO_TEST_CASE(testSeqDbSeqInfoSrc)
{
    const int kOid = 1000;
    const int kGoodLength = 323;
    const char* kGoodIdStr = "46075105";

    CSeqDbSeqInfoSrc seqinfo_src("data/seqn", false);
    Uint4 length = seqinfo_src.GetLength(kOid);
    BOOST_REQUIRE_EQUAL(kGoodLength, (int)length);
    CRef<CSeq_id> seqid(seqinfo_src.GetId(kOid).front());
    BOOST_REQUIRE(!strcmp(kGoodIdStr, seqid->GetSeqIdString().c_str()));
}

BOOST_AUTO_TEST_CASE(testBadDatabase)
{
    BOOST_REQUIRE_THROW(CSeqDbSeqInfoSrc seqinfo_src("data/seqn", true),
                        CSeqDBException);
}

BOOST_AUTO_TEST_CASE(testSeqDbOidOutOfRange)
{
    const Uint4 kOid = 2005;
    Uint4 length;
    CSeqDbSeqInfoSrc seqinfo_src("data/seqp", true);
    BOOST_REQUIRE_THROW(length = seqinfo_src.GetLength(kOid), CSeqDBException);
}

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: seqinfosrc-cppunit.cpp,v $
* Revision 1.7  2008/10/27 17:00:12  camacho
* Fix include paths to deprecated headers
*
* Revision 1.6  2008/07/09 14:29:44  camacho
* Fix error message
*
* Revision 1.5  2008/02/20 16:23:27  bealer
* - Fix overly specific error message matching in unit tests.
*
* Revision 1.4  2005/03/04 17:20:45  bealer
* - Command line option support.
*
* Revision 1.3  2005/01/10 15:43:22  camacho
* Updates for changes in data/seqp
*
* Revision 1.2  2004/12/02 22:46:35  dondosha
* Renamed constants according to C++ toolkit convention
*
* Revision 1.1  2004/10/06 15:03:36  dondosha
* Test the IBlastSeqInfoSrc interface for sequence vector and database
*
*
* ===========================================================================
*/
