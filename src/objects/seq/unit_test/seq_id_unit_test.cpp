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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   Unit test for CSeq_id and some closely related code
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/seqloc/Seq_id.hpp>

#include <objects/biblio/Id_pat.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
#ifndef BOOST_PARAM_TEST_CASE
#  include <boost/test/parameterized_test.hpp>
#endif
#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using boost::unit_test::test_suite;

// Use macros rather than inline functions to get accurate line number reports

// #define CHECK_NO_THROW(statement) BOOST_CHECK_NO_THROW(statement)
#define CHECK_NO_THROW(statement)                                       \
    try {                                                               \
        statement;                                                      \
        BOOST_CHECK_MESSAGE(true, "no exceptions were thrown by "#statement); \
    } catch (std::exception& e) {                                       \
        BOOST_ERROR("an exception was thrown by "#statement": " << e.what()); \
    } catch (...) {                                                     \
        BOOST_ERROR("a nonstandard exception was thrown by "#statement); \
    }

#define CHECK(expr)       CHECK_NO_THROW(BOOST_CHECK(expr))
#define CHECK_EQUAL(x, y) CHECK_NO_THROW(BOOST_CHECK_EQUAL(x, y))
#define CHECK_THROW(s, x) BOOST_CHECK_THROW(s, x)

#define CHECK_THROW_STD_(statement, xtype)                              \
    try {                                                               \
        statement;                                                      \
        BOOST_ERROR(#statement" failed to throw "#xtype);               \
    } catch (xtype& e) {                                                \
        BOOST_MESSAGE(#statement" properly threw "#xtype": " << e.what()); \
    }
#define CHECK_THROW_STD(s, x) CHECK_NO_THROW(CHECK_THROW_STD_(s, x))

#define CHECK_THROW_SEQID(s) CHECK_THROW_STD(s, CSeqIdException)

BOOST_AUTO_TEST_CASE(s_TestDefaultInit)
{
    CSeq_id id;
    CHECK_EQUAL(id.Which(), CSeq_id::e_not_set);
    CHECK_THROW(id.GetGi(), CInvalidChoiceSelection);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromJunk)
{
    CRef<CSeq_id> id;

    CHECK_THROW_SEQID(id.Reset(new CSeq_id(kEmptyStr)));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("JUNK")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("?!?!")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromGIString)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("  1234 ")));
    CHECK(id->IsGi());
    CHECK(id->GetGi() == 1234);

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("1234.5")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("-1234")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("0")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("9876543210")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromNAcc)
{
    CRef<CSeq_id> id;

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("N00001")));

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("N0068")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("N00068")));
    CHECK(id->IsDdbj());
    CHECK_EQUAL(id->GetDdbj().GetAccession(), string("N00068"));
    CHECK( !id->GetDdbj().IsSetName() );
    CHECK( !id->GetDdbj().IsSetVersion() );
    CHECK( !id->GetDdbj().IsSetRelease() );
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("N000068")));

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("N19999")));

    CHECK_NO_THROW(id.Reset(new CSeq_id("N20001.1")));
    CHECK(id->IsGenbank());
    CHECK_EQUAL(id->GetGenbank().GetAccession(), string("N20001"));
    CHECK( !id->GetGenbank().IsSetName() );
    CHECK_EQUAL(id->GetGenbank().GetVersion(), 1);
    CHECK( !id->GetGenbank().IsSetRelease() );

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("N20001.1.1")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("N20001.1a")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("N20001.-1")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("N20001.x")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("N20001.9876543210")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromStdAcc)
{
    CRef<CSeq_id> id;

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("BN00123")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("bn000123")));
    CHECK(id->IsTpe());
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("BN00012B")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("BN0000123")));

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("FAA0017")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("FAA00017")));
    CHECK(id->IsTpd());
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("FAA000017")));

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("ABCD1234567")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("CAAA01020304")));
    CHECK(id->IsEmbl());
    CHECK_NO_THROW(id.Reset(new CSeq_id("AACN011056789")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("ABCD1234567890")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("ABCD12345678901")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromPRFAcc)
{
    CRef<CSeq_id> id;

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("806162C")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("0806162C")));
    CHECK(id->IsPrf());
    CHECK(!id->GetPrf().IsSetAccession());
    CHECK_EQUAL(id->GetPrf().GetName(), string("0806162C"));
    CHECK_NO_THROW(id.Reset(new CSeq_id("080616AC")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("080616C2")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("00806162C")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("0806162C3")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("0806162CD")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromPDBAcc)
{
    CRef<CSeq_id> id;

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("1GA")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("1GAV")));
    CHECK(id->IsPdb());
    CHECK_EQUAL(id->GetPdb().GetMol().Get(), string("1GAV"));
    CHECK_EQUAL(id->GetPdb().GetChain(), ' ');
    // CHECK_THROW_SEQID(id.Reset(new CSeq_id("1GAV2")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("1GAV.2")));

    CHECK_NO_THROW(id.Reset(new CSeq_id("1GAVX")));
    CHECK(id->IsPdb());
    CHECK_EQUAL(id->GetPdb().GetMol().Get(), string("1GAV"));
    CHECK_EQUAL(id->GetPdb().GetChain(), 'X');

    CHECK_NO_THROW(id.Reset(new CSeq_id("1GAV|X")));
    CHECK(id->IsPdb());
    CHECK_EQUAL(id->GetPdb().GetMol().Get(), string("1GAV"));
    CHECK_EQUAL(id->GetPdb().GetChain(), 'X');

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("1GAV|XY")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("1GAV|XX")));
    CHECK_EQUAL(id->GetPdb().GetChain(), 'x');
    CHECK_NO_THROW(id.Reset(new CSeq_id("1GAV_!")));
    CHECK_EQUAL(id->GetPdb().GetChain(), '!');
    CHECK_NO_THROW(id.Reset(new CSeq_id("1GAV|VB")));
    CHECK_EQUAL(id->GetPdb().GetChain(), '|');
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("1GAV|AAA")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromSPAcc)
{
    CRef<CSeq_id> id;

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("Q7CQJ")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("q7cqj0")));
    CHECK(id->IsSwissprot());
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("Q7CQJO")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("Q7CQJ01")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("07CQJ0")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("A2ASS6.1")));
    CHECK(id->IsSwissprot());
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("A29SS6.1")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromRefSeqAcc)
{
    CRef<CSeq_id> id;

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("NM_00017")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("NM_000170.1")));
    CHECK(id->IsOther());
    CHECK_NO_THROW(id.Reset(new CSeq_id("NM_001000170")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("NM_0001000170")));

    CHECK_NO_THROW(id.Reset(new CSeq_id("ZP_00345678")));

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("NZ_AABC0300051")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("NZ_AABC03000051")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("NZ_AABC030000051")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromGpipeAcc)
{
    CRef<CSeq_id> id;

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("GPC_12345")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("GPC_123456.1")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("GPC_123456789.1")));
    CHECK(id->IsGpipe());
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("GPC_1234567890")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaLocal)
{
    CRef<CSeq_id> id;
    
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("asd|fgh|jkl")));

    CHECK_NO_THROW(id.Reset(new CSeq_id("lcl|123")));
    CHECK(id->IsLocal());
    CHECK(id->GetLocal().IsId());
    CHECK_EQUAL(id->GetLocal().GetId(), 123);

    CHECK_NO_THROW(id.Reset(new CSeq_id("lcl|asdf")));
    CHECK(id->IsLocal());
    CHECK(id->GetLocal().IsStr());
    CHECK_EQUAL(id->GetLocal().GetStr(), string("asdf"));

    CHECK_NO_THROW(id.Reset(new CSeq_id("lcl|NM_002020|")));
    CHECK(id->IsLocal());
    CHECK(id->GetLocal().IsStr());
    CHECK_EQUAL(id->GetLocal().GetStr(), string("NM_002020"));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("lcl|NM_002020|junk")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaObsolete)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("bbs|123")));
    CHECK(id->IsGibbsq());
    CHECK_EQUAL(id->GetGibbsq(), 123);
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("bbs|0")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("bbs|123.4")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("bbs|123Z")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("bbs|xyz")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("bbs|9876543210")));

    CHECK_NO_THROW(id.Reset(new CSeq_id("bbm|123")));
    CHECK(id->IsGibbmt());

    CHECK_NO_THROW(id.Reset(new CSeq_id("gim|123")));
    CHECK(id->IsGiim());
    CHECK_EQUAL(id->GetGiim().GetId(), 123);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaGenbank)
{
    CRef<CSeq_id> id;

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("gb|")))
    CHECK_NO_THROW(id.Reset(new CSeq_id("gb|U12345.1|AMU12345")));
    CHECK(id->IsGenbank());
    CHECK_EQUAL(id->GetGenbank().GetAccession(), string("U12345"));
    CHECK_EQUAL(id->GetGenbank().GetName(), string("AMU12345"));
    CHECK_EQUAL(id->GetGenbank().GetVersion(), 1);
    CHECK( !id->GetGenbank().IsSetRelease() );
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaEmbl)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("emb|AL123456|MTBH37RV")));
    CHECK(id->IsEmbl());
    CHECK_EQUAL(id->GetEmbl().GetAccession(), string("AL123456"));
    CHECK_EQUAL(id->GetEmbl().GetName(), string("MTBH37RV"));
    CHECK( !id->GetEmbl().IsSetVersion() );
    CHECK( !id->GetEmbl().IsSetRelease() );
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaPir)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("pir||S16356")));
    CHECK(id->IsPir());
    CHECK( !id->GetPir().IsSetAccession() );
    CHECK_EQUAL(id->GetPir().GetName(), string("S16356"));
    CHECK( !id->GetPir().IsSetVersion() );
    CHECK( !id->GetPir().IsSetRelease() );
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaSwissprot)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("sp|Q7CQJ0|RS22_SALTY")));
    CHECK(id->IsSwissprot());
    CHECK_EQUAL(id->GetSwissprot().GetAccession(), string("Q7CQJ0"));
    CHECK_EQUAL(id->GetSwissprot().GetName(), string("RS22_SALTY"));
    CHECK( !id->GetSwissprot().IsSetVersion() );
    CHECK_EQUAL(id->GetSwissprot().GetRelease(), string("reviewed"));

    CHECK_NO_THROW(id.Reset(new CSeq_id("tr|Q90RT2|Q90RT2_9HIV1")));
    CHECK_EQUAL(id->GetSwissprot().GetRelease(), string("unreviewed"));

    CHECK_NO_THROW(id.Reset(new CSeq_id("sp|Q7CQJ0.1")));
    CHECK(id->IsSwissprot());
    CHECK_EQUAL(id->GetSwissprot().GetAccession(), string("Q7CQJ0"));
    CHECK_EQUAL(id->GetSwissprot().GetVersion(), 1);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromPatent)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("pat|US|RE33188|1")));
    CHECK(id->IsPatent());
    CHECK_EQUAL(id->GetPatent().GetSeqid(), 1);
    CHECK_EQUAL(id->GetPatent().GetCit().GetCountry(), string("US"));
    CHECK(id->GetPatent().GetCit().GetId().IsNumber());
    CHECK_EQUAL(id->GetPatent().GetCit().GetId().GetNumber(),
                string("RE33188"));

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188|1.5")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188|1b")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188|9876543210")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188|-1")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188|Z")));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188")));

    CHECK_NO_THROW(id.Reset(new CSeq_id("pgp|EP|0238993|7")));
    CHECK(id->IsPatent());
    CHECK_EQUAL(id->GetPatent().GetSeqid(), 7);
    CHECK_EQUAL(id->GetPatent().GetCit().GetCountry(), string("EP"));
    CHECK(id->GetPatent().GetCit().GetId().IsApp_number());
    CHECK_EQUAL(id->GetPatent().GetCit().GetId().GetApp_number(),
                string("0238993"));

    CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Patent,
                                        "US", "RE33188", 1)));
    CHECK_EQUAL(id->GetPatent().GetCit().GetId().GetNumber(),
                string("RE33188"));

    CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Patent,
                                        "EP", "0238993", 7, "PGP")));
    CHECK_EQUAL(id->GetPatent().GetCit().GetId().GetApp_number(),
                string("0238993"));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaRefseq)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("ref|NM_000170.1")));
    CHECK(id->IsOther());
    CHECK_EQUAL(id->GetOther().GetAccession(), string("NM_000170"));
    // CHECK_EQUAL(id->GetOther().GetVersion(), 1);
    // Split up to avoid mysterious WorkShop 5.5 11381x-19 errors:
    int version;
    CHECK_NO_THROW(version = id->GetOther().GetVersion());
    CHECK_EQUAL(version, 1);
    // Don't try to do anything with the release field, which is no longer
    // supported.
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaGeneral)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW
        (id.Reset(new CSeq_id
                  ("gnl|dbSNP|rs31251_allelePos=201totallen=401|taxid=9606"
                   "|snpClass=1|alleles=?|mol=?|build=?")));
    CHECK(id->IsGeneral());
    CHECK_EQUAL(id->GetGeneral().GetDb(), string("dbSNP"));
    CHECK(id->GetGeneral().GetTag().IsStr());
    CHECK_EQUAL(id->GetGeneral().GetTag().GetStr(),
                string("rs31251_allelePos=201totallen=401|taxid=9606"
                       "|snpClass=1|alleles=?|mol=?|build=?"));

    CHECK_NO_THROW(id.Reset(new CSeq_id("gnl|taxon|9606")));
    CHECK(id->IsGeneral());
    CHECK_EQUAL(id->GetGeneral().GetDb(), string("taxon"));
    CHECK(id->GetGeneral().GetTag().IsId());
    CHECK_EQUAL(id->GetGeneral().GetTag().GetId(), 9606);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaGI)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("gi|1234")));
    CHECK(id->IsGi());
    CHECK_EQUAL(id->GetGi(), 1234);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaDdbj)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("dbj|N00068")));
    CHECK(id->IsDdbj());
    CHECK_EQUAL(id->GetDdbj().GetAccession(), string("N00068"));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaPrf)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("prf|0806162C")));
    CHECK(id->IsPrf());
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaPdb)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("pdb|1GAV")));
    CHECK(id->IsPdb());
    CHECK_EQUAL(id->GetPdb().GetMol().Get(), string("1GAV"));
    CHECK_EQUAL(id->GetPdb().GetChain(), ' ');

    CHECK_NO_THROW(id.Reset(new CSeq_id("pdb|1GAV|X")));
    CHECK(id->IsPdb());
    CHECK_EQUAL(id->GetPdb().GetMol().Get(), string("1GAV"));
    CHECK_EQUAL(id->GetPdb().GetChain(), 'X');

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("pdb|1GAV|XY")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("pdb|1GAV|XX")));
    CHECK_EQUAL(id->GetPdb().GetChain(), 'x');
    CHECK_NO_THROW(id.Reset(new CSeq_id("pdb|1GAV|!")));
    CHECK_EQUAL(id->GetPdb().GetChain(), '!');
    CHECK_NO_THROW(id.Reset(new CSeq_id("pdb|1GAV|VB")));
    CHECK_EQUAL(id->GetPdb().GetChain(), '|');
    CHECK_THROW_SEQID(id.Reset(new CSeq_id("pdb|1GAV|AAA")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaTpa)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("tpg|BK003456")));
    CHECK(id->IsTpg());
    CHECK_NO_THROW(id.Reset(new CSeq_id("tpe|BN000123")));
    CHECK(id->IsTpe());
    CHECK_NO_THROW(id.Reset(new CSeq_id("tpd|FAA00017")));
    CHECK(id->IsTpd());
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaGpipe)
{
    CRef<CSeq_id> id;
    CHECK_NO_THROW(id.Reset(new CSeq_id("gpp|GPC_123456789")));
    CHECK(id->IsGpipe());
}


static CSeq_id* s_NewDbtagId(const string& db, const string& tag,
                             bool set_as_general = false)
{
    CDbtag dbtag;
    dbtag.SetDb(db);
    dbtag.SetTag().SetStr(tag);
    return new CSeq_id(dbtag, set_as_general);
}

static CSeq_id* s_NewDbtagId(const string& db, int tag,
                             bool set_as_general = false)
{
    CDbtag dbtag;
    dbtag.SetDb(db);
    dbtag.SetTag().SetId(tag);
    return new CSeq_id(dbtag, set_as_general);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromDbtag)
{
    CRef<CSeq_id> id;
    CDbtag        dbtag;
    
    CHECK_THROW_SEQID(id.Reset(new CSeq_id(dbtag)));

    CHECK_NO_THROW(id.Reset(s_NewDbtagId("GenBank", "N20001.1")));
    CHECK(id->IsGenbank());
    CHECK_EQUAL(id->GetGenbank().GetAccession(), string("N20001"));
    CHECK( !id->GetGenbank().IsSetName() );
    CHECK_EQUAL(id->GetGenbank().GetVersion(), 1);
    CHECK( !id->GetGenbank().IsSetRelease() );

    CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GenBank", "N20001.1.1")));
    CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GenBank", "N20001.1b")));
    CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GenBank", "N20001.-1")));
    CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GenBank", "N20001.z")));

    CHECK_NO_THROW(id.Reset(s_NewDbtagId("GenBank", "12345")));
    CHECK(id->IsGi());
    CHECK_EQUAL(id->GetGi(), 12345);

    CHECK_NO_THROW(id.Reset(s_NewDbtagId("GenBank", 12345)));
    CHECK(id->IsGi());
    CHECK_EQUAL(id->GetGi(), 12345);

    CHECK_NO_THROW(id.Reset(s_NewDbtagId("EMBL", "AL123456")));
    CHECK(id->IsEmbl());
    CHECK_EQUAL(id->GetEmbl().GetAccession(), string("AL123456"));

    CHECK_NO_THROW(id.Reset(s_NewDbtagId("DDBJ", "N00068")));
    CHECK(id->IsDdbj());
    CHECK_EQUAL(id->GetDdbj().GetAccession(), string("N00068"));

    CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GI", "12345X")));

    CHECK_NO_THROW(id.Reset(s_NewDbtagId("GI", "12345")));
    CHECK(id->IsGi());
    CHECK_EQUAL(id->GetGi(), 12345);

    CHECK_NO_THROW(id.Reset(s_NewDbtagId("GI", 12345)));
    CHECK(id->IsGi());
    CHECK_EQUAL(id->GetGi(), 12345);

    CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("taxon", 9606)));
    CHECK_NO_THROW(id.Reset(s_NewDbtagId("taxon", 9606, true)));
    CHECK(id->IsGeneral());

    CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("asdf", "jkl")));
    CHECK_NO_THROW(id.Reset(s_NewDbtagId("asdf", "jkl", true)));
    CHECK(id->IsGeneral());
}

BOOST_AUTO_TEST_CASE(s_TestInitFromInt)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Gi, 1234)));
    CHECK(id->IsGi());
    CHECK_EQUAL(id->GetGi(), 1234);

    CHECK_THROW_SEQID(id.Reset(new CSeq_id(CSeq_id::e_Gi, 0)));
    CHECK_THROW_SEQID(id.Reset(new CSeq_id(CSeq_id::e_Pdb, 1234)));

    CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Local, 1234)));
    CHECK(id->IsLocal());
    CHECK(id->GetLocal().IsId());
    CHECK_EQUAL(id->GetLocal().GetId(), 1234);

    CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Gibbsq, 1234)));
    CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Gibbmt, 1234)));

    CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Giim, 1234)));
    CHECK(id->IsGiim());
    CHECK_EQUAL(id->GetGiim().GetId(), 1234);
}

static const char* kTestFastaStrings[] = {
    "lcl|123",
    "lcl|asdf",
    "bbs|123",
    "bbm|123",
    "gim|123",
    "gb|U12345.1|AMU12345",
    "emb|AL123456|MTBH37RV",
    "pir||S16356",
    "sp|Q7CQJ0|RS22_SALTY",
    "tr|Q90RT2|Q90RT2_9HIV1",
    "sp|Q7CQJ0.1|",
    "pat|US|RE33188|1",
    "pgp|EP|0238993|7",
    "ref|NM_000170.1|",
    "gnl|EcoSeq|EcoAce",
    "gnl|taxon|9606",
    "gi|1234",
    "dbj|N00068|",
    "prf|0806162C|",
    "pdb|1GAV| ",
    "pdb|1GAV|X",
    "pdb|1GAV|XX",
    "pdb|1GAV|!",
    "pdb|1GAV|VB",
    "tpg|BK003456|",
    "tpe|BN000123|",
    "tpd|FAA00017|",
    "gpp|GPC_123456789|",
    /* Must be last due to special-cased greedy parsing */
    "gnl|dbSNP|rs31251_allelePos=201totallen=401|taxid=9606"
    "|snpClass=1|alleles=?|mol=?|build=?"
};
static const size_t kNumFastaStrings
= sizeof(kTestFastaStrings)/sizeof(*kTestFastaStrings);

static void s_TestFastaRoundTrip(const char* s)
{
    CRef<CSeq_id> id;
    BOOST_MESSAGE("Testing round trip for " << s);
    CHECK_NO_THROW(id.Reset(new CSeq_id(s)));
    CHECK_EQUAL(id->AsFastaString(), s);
    for (SIZE_TYPE pos = strlen(s) - 1;
         pos != NPOS  &&  (s[pos] == '|'  ||  s[pos] == ' ');
         --pos) {
        CRef<CSeq_id> id2;
        string ss(s, pos);
        BOOST_MESSAGE("Testing equality with " << ss);
        CHECK_NO_THROW(id2.Reset(new CSeq_id(ss)));
        CHECK_EQUAL(id2->AsFastaString(), s);
        CHECK(id->Match(*id2));
        CHECK_EQUAL(id->Compare(*id2), CSeq_id::e_YES);
    }
}

#ifdef BOOST_AUTO_TC_REGISTRAR
BOOST_AUTO_TC_REGISTRAR(s_FastaRoundTrip)
#else
static boost::unit_test::ut_detail::auto_unit_test_registrar
s_FastaRoundTripRegistrar
#endif
(BOOST_PARAM_TEST_CASE(s_TestFastaRoundTrip, kTestFastaStrings + 0,
                       kTestFastaStrings + kNumFastaStrings));

BOOST_AUTO_TEST_CASE(s_TestNoStrays)
{
    CSeq_id id;
    CHECK_NO_THROW(id.SetGiim().SetDb("foo"));
    CHECK_NO_THROW(id.SetGiim().SetRelease("2.0"));
    CHECK(id.IsGiim());
    CHECK(id.GetGiim().IsSetDb());
    CHECK(id.GetGiim().IsSetRelease());
    CHECK_NO_THROW(id.Set("gim|123"));
    CHECK(id.IsGiim());
    CHECK( !id.GetGiim().IsSetDb() );
    CHECK( !id.GetGiim().IsSetRelease() );

    CHECK_NO_THROW(id.SetGenbank().SetRelease("135"));
    CHECK(id.IsGenbank());
    CHECK(id.GetGenbank().IsSetRelease());
    CHECK_NO_THROW(id.Set("gb|U12345.1|AMU12345"));
    CHECK(id.IsGenbank());
    CHECK( !id.GetGenbank().IsSetRelease() );

    CHECK_NO_THROW(id.SetPatent().SetCit().SetDoc_type("app"));
    CHECK(id.IsPatent());
    CHECK(id.GetPatent().GetCit().IsSetDoc_type());
    CHECK_NO_THROW(id.Set("pat|US|RE33188|1"));
    CHECK(id.IsPatent());
    CHECK( !id.GetPatent().GetCit().IsSetDoc_type() );

    CHECK_NO_THROW(id.SetPdb().SetRel().SetToTime(GetFastLocalTime()));
    CHECK(id.IsPdb());
    CHECK(id.GetPdb().IsSetRel());
    CHECK_NO_THROW(id.Set("pdb|1GAV|X"));
    CHECK(id.IsPdb());
    CHECK( !id.GetPdb().IsSetRel() );
}

BOOST_AUTO_TEST_CASE(s_TestListOps)
{
    string merged;
    for (size_t i = 0;  i < kNumFastaStrings;  ++i) {
        if (i > 0) {
            merged += '|';
        }
        merged += kTestFastaStrings[i];
    }
    CBioseq bs;
    bs.SetInst().SetRepr(CSeq_inst::eRepr_virtual);
    bs.SetInst().SetMol(CSeq_inst::eMol_other);
    CBioseq::TId& ids = bs.SetId();
    CHECK_EQUAL(CSeq_id::ParseFastaIds(ids, merged, true), kNumFastaStrings);
    CHECK_EQUAL(ids.size(), kNumFastaStrings);
    CHECK_EQUAL(CSeq_id::GetStringDescr(bs, CSeq_id::eFormat_FastA),
                string("gi|1234|ref|NM_000170.1|"));
    CHECK_EQUAL(CSeq_id::GetStringDescr(bs, CSeq_id::eFormat_ForceGI),
                string("gi|1234"));
    CHECK_EQUAL(CSeq_id::GetStringDescr(bs, CSeq_id::eFormat_BestWithVersion),
                string("ref|NM_000170.1"));
    CHECK_EQUAL(CSeq_id::GetStringDescr(bs,
                                        CSeq_id::eFormat_BestWithoutVersion),
                string("ref|NM_000170"));
    CHECK_EQUAL(CSeq_id::ParseFastaIds(ids, "gi|1234|junk|pdb|1GAV", true),
                size_t(2));
    CHECK_THROW_SEQID(CSeq_id::ParseFastaIds(ids, "gi|1234|junk|pdb|1GAV"));
}
