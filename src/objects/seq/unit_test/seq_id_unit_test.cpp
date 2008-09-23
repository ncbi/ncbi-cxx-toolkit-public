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
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <boost/test/parameterized_test.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#define NCBI_CHECK_THROW_SEQID(s) BOOST_CHECK_THROW(s, CSeqIdException)

NCBITEST_AUTO_INIT()
{
    // force use of built-in accession guide
    CNcbiApplication::Instance()->GetConfig().Set("NCBI", "Data", kEmptyStr,
                                                  IRegistry::fPersistent);
}

BOOST_AUTO_TEST_CASE(s_TestDefaultInit)
{
    CSeq_id id;
    BOOST_CHECK_EQUAL(id.Which(), CSeq_id::e_not_set);
    BOOST_CHECK_THROW(id.GetGi(), CInvalidChoiceSelection);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromJunk)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id(kEmptyStr)));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("JUNK")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("?!?!")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromGIString)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("  1234 ")));
    BOOST_CHECK(id->IsGi());
    BOOST_CHECK(id->GetGi() == 1234);

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("1234.5")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("-1234")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("0")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("9876543210")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromNAcc)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("N00001")));

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("N0068")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("N00068")));
    BOOST_CHECK(id->IsDdbj());
    BOOST_CHECK_EQUAL(id->GetDdbj().GetAccession(), string("N00068"));
    BOOST_CHECK( !id->GetDdbj().IsSetName() );
    BOOST_CHECK( !id->GetDdbj().IsSetVersion() );
    BOOST_CHECK( !id->GetDdbj().IsSetRelease() );
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("N000068")));

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("N19999")));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("N20001.1")));
    BOOST_CHECK(id->IsGenbank());
    BOOST_CHECK_EQUAL(id->GetGenbank().GetAccession(), string("N20001"));
    BOOST_CHECK( !id->GetGenbank().IsSetName() );
    BOOST_CHECK_EQUAL(id->GetGenbank().GetVersion(), 1);
    BOOST_CHECK( !id->GetGenbank().IsSetRelease() );

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("N20001.1.1")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("N20001.1a")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("N20001.-1")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("N20001.x")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("N20001.9876543210")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromStdAcc)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("BN00123")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("bn000123")));
    BOOST_CHECK(id->IsTpe());
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("BN00012B")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("BN0000123")));

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("FAA0017")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("FAA00017")));
    BOOST_CHECK(id->IsTpd());
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("FAA000017")));

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("ABCD1234567")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("CAAA01020304")));
    BOOST_CHECK(id->IsEmbl());
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("AACN011056789")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("ABCD1234567890")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("ABCD12345678901")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromPRFAcc)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("806162C")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("0806162C")));
    BOOST_CHECK(id->IsPrf());
    BOOST_CHECK(!id->GetPrf().IsSetAccession());
    BOOST_CHECK_EQUAL(id->GetPrf().GetName(), string("0806162C"));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("080616AC")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("080616C2")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("00806162C")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("0806162C3")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("0806162CD")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromPDBAcc)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("1GA")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("1GAV")));
    BOOST_CHECK(id->IsPdb());
    BOOST_CHECK_EQUAL(id->GetPdb().GetMol().Get(), string("1GAV"));
    BOOST_CHECK_EQUAL(id->GetPdb().GetChain(), ' ');
    // NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("1GAV2")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("1GAV.2")));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("1GAVX")));
    BOOST_CHECK(id->IsPdb());
    BOOST_CHECK_EQUAL(id->GetPdb().GetMol().Get(), string("1GAV"));
    BOOST_CHECK_EQUAL(id->GetPdb().GetChain(), 'X');

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("1GAV|X")));
    BOOST_CHECK(id->IsPdb());
    BOOST_CHECK_EQUAL(id->GetPdb().GetMol().Get(), string("1GAV"));
    BOOST_CHECK_EQUAL(id->GetPdb().GetChain(), 'X');

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("1GAV|XY")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("1GAV|XX")));
    BOOST_CHECK_EQUAL(id->GetPdb().GetChain(), 'x');
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("1GAV_!")));
    BOOST_CHECK_EQUAL(id->GetPdb().GetChain(), '!');
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("1GAV|VB")));
    BOOST_CHECK_EQUAL(id->GetPdb().GetChain(), '|');
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("1GAV|AAA")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromSPAcc)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("Q7CQJ")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("q7cqj0")));
    BOOST_CHECK(id->IsSwissprot());
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("Q7CQJO")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("Q7CQJ01")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("07CQJ0")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("A2ASS6.1")));
    BOOST_CHECK(id->IsSwissprot());
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("A29SS6.1")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromRefSeqAcc)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("NM_00017")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("NM_000170.1")));
    BOOST_CHECK(id->IsOther());
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("NM_001000170")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("NM_0001000170")));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("ZP_00345678")));

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("NZ_AABC0300051")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("NZ_AABC03000051")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("NZ_AABC030000051")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromGpipeAcc)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("GPC_12345")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("GPC_123456.1")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("GPC_123456789.1")));
    BOOST_CHECK(id->IsGpipe());
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("GPC_1234567890")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaLocal)
{
    CRef<CSeq_id> id;
    
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("asd|fgh|jkl")));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("lcl|123")));
    BOOST_CHECK(id->IsLocal());
    BOOST_CHECK(id->GetLocal().IsId());
    BOOST_CHECK_EQUAL(id->GetLocal().GetId(), 123);

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("lcl|asdf")));
    BOOST_CHECK(id->IsLocal());
    BOOST_CHECK(id->GetLocal().IsStr());
    BOOST_CHECK_EQUAL(id->GetLocal().GetStr(), string("asdf"));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("lcl|NM_002020|")));
    BOOST_CHECK(id->IsLocal());
    BOOST_CHECK(id->GetLocal().IsStr());
    BOOST_CHECK_EQUAL(id->GetLocal().GetStr(), string("NM_002020"));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("lcl|NM_002020|junk")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaObsolete)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("bbs|123")));
    BOOST_CHECK(id->IsGibbsq());
    BOOST_CHECK_EQUAL(id->GetGibbsq(), 123);
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("bbs|0")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("bbs|123.4")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("bbs|123Z")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("bbs|xyz")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("bbs|9876543210")));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("bbm|123")));
    BOOST_CHECK(id->IsGibbmt());

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("gim|123")));
    BOOST_CHECK(id->IsGiim());
    BOOST_CHECK_EQUAL(id->GetGiim().GetId(), 123);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaGenbank)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("gb|")))
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("gb|U12345.1|AMU12345")));
    BOOST_CHECK(id->IsGenbank());
    BOOST_CHECK_EQUAL(id->GetGenbank().GetAccession(), string("U12345"));
    BOOST_CHECK_EQUAL(id->GetGenbank().GetName(), string("AMU12345"));
    BOOST_CHECK_EQUAL(id->GetGenbank().GetVersion(), 1);
    BOOST_CHECK( !id->GetGenbank().IsSetRelease() );
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaEmbl)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("emb|AL123456|MTBH37RV")));
    BOOST_CHECK(id->IsEmbl());
    BOOST_CHECK_EQUAL(id->GetEmbl().GetAccession(), string("AL123456"));
    BOOST_CHECK_EQUAL(id->GetEmbl().GetName(), string("MTBH37RV"));
    BOOST_CHECK( !id->GetEmbl().IsSetVersion() );
    BOOST_CHECK( !id->GetEmbl().IsSetRelease() );
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaPir)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("pir||S16356")));
    BOOST_CHECK(id->IsPir());
    BOOST_CHECK( !id->GetPir().IsSetAccession() );
    BOOST_CHECK_EQUAL(id->GetPir().GetName(), string("S16356"));
    BOOST_CHECK( !id->GetPir().IsSetVersion() );
    BOOST_CHECK( !id->GetPir().IsSetRelease() );
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaSwissprot)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("sp|Q7CQJ0|RS22_SALTY")));
    BOOST_CHECK(id->IsSwissprot());
    BOOST_CHECK_EQUAL(id->GetSwissprot().GetAccession(), string("Q7CQJ0"));
    BOOST_CHECK_EQUAL(id->GetSwissprot().GetName(), string("RS22_SALTY"));
    BOOST_CHECK( !id->GetSwissprot().IsSetVersion() );
    BOOST_CHECK_EQUAL(id->GetSwissprot().GetRelease(), string("reviewed"));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("tr|Q90RT2|Q90RT2_9HIV1")));
    BOOST_CHECK_EQUAL(id->GetSwissprot().GetRelease(), string("unreviewed"));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("sp|Q7CQJ0.1")));
    BOOST_CHECK(id->IsSwissprot());
    BOOST_CHECK_EQUAL(id->GetSwissprot().GetAccession(), string("Q7CQJ0"));
    BOOST_CHECK_EQUAL(id->GetSwissprot().GetVersion(), 1);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromPatent)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("pat|US|RE33188|1")));
    BOOST_CHECK(id->IsPatent());
    BOOST_CHECK_EQUAL(id->GetPatent().GetSeqid(), 1);
    BOOST_CHECK_EQUAL(id->GetPatent().GetCit().GetCountry(), string("US"));
    BOOST_CHECK(id->GetPatent().GetCit().GetId().IsNumber());
    BOOST_CHECK_EQUAL(id->GetPatent().GetCit().GetId().GetNumber(),
                string("RE33188"));

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188|1.5")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188|1b")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188|9876543210")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188|-1")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188|Z")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("pat|US|RE33188")));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("pgp|EP|0238993|7")));
    BOOST_CHECK(id->IsPatent());
    BOOST_CHECK_EQUAL(id->GetPatent().GetSeqid(), 7);
    BOOST_CHECK_EQUAL(id->GetPatent().GetCit().GetCountry(), string("EP"));
    BOOST_CHECK(id->GetPatent().GetCit().GetId().IsApp_number());
    BOOST_CHECK_EQUAL(id->GetPatent().GetCit().GetId().GetApp_number(),
                string("0238993"));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Patent,
                                        "US", "RE33188", 1)));
    BOOST_CHECK_EQUAL(id->GetPatent().GetCit().GetId().GetNumber(),
                string("RE33188"));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Patent,
                                        "EP", "0238993", 7, "PGP")));
    BOOST_CHECK_EQUAL(id->GetPatent().GetCit().GetId().GetApp_number(),
                string("0238993"));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaRefseq)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("ref|NM_000170.1")));
    BOOST_CHECK(id->IsOther());
    BOOST_CHECK_EQUAL(id->GetOther().GetAccession(), string("NM_000170"));
    // BOOST_CHECK_EQUAL(id->GetOther().GetVersion(), 1);
    // Split up to avoid mysterious WorkShop 5.5 11381x-19 errors:
    int version;
    BOOST_CHECK_NO_THROW(version = id->GetOther().GetVersion());
    BOOST_CHECK_EQUAL(version, 1);
    // Don't try to do anything with the release field, which is no longer
    // supported.
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaGeneral)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW
        (id.Reset(new CSeq_id
                  ("gnl|dbSNP|rs31251_allelePos=201totallen=401|taxid=9606"
                   "|snpClass=1|alleles=?|mol=?|build=?")));
    BOOST_CHECK(id->IsGeneral());
    BOOST_CHECK_EQUAL(id->GetGeneral().GetDb(), string("dbSNP"));
    BOOST_CHECK(id->GetGeneral().GetTag().IsStr());
    BOOST_CHECK_EQUAL(id->GetGeneral().GetTag().GetStr(),
                string("rs31251_allelePos=201totallen=401|taxid=9606"
                       "|snpClass=1|alleles=?|mol=?|build=?"));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("gnl|taxon|9606")));
    BOOST_CHECK(id->IsGeneral());
    BOOST_CHECK_EQUAL(id->GetGeneral().GetDb(), string("taxon"));
    BOOST_CHECK(id->GetGeneral().GetTag().IsId());
    BOOST_CHECK_EQUAL(id->GetGeneral().GetTag().GetId(), 9606);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaGI)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("gi|1234")));
    BOOST_CHECK(id->IsGi());
    BOOST_CHECK_EQUAL(id->GetGi(), 1234);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaDdbj)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("dbj|N00068")));
    BOOST_CHECK(id->IsDdbj());
    BOOST_CHECK_EQUAL(id->GetDdbj().GetAccession(), string("N00068"));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaPrf)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("prf|0806162C")));
    BOOST_CHECK(id->IsPrf());
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaPdb)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("pdb|1GAV")));
    BOOST_CHECK(id->IsPdb());
    BOOST_CHECK_EQUAL(id->GetPdb().GetMol().Get(), string("1GAV"));
    BOOST_CHECK_EQUAL(id->GetPdb().GetChain(), ' ');

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("pdb|1GAV|X")));
    BOOST_CHECK(id->IsPdb());
    BOOST_CHECK_EQUAL(id->GetPdb().GetMol().Get(), string("1GAV"));
    BOOST_CHECK_EQUAL(id->GetPdb().GetChain(), 'X');

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("pdb|1GAV|XY")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("pdb|1GAV|XX")));
    BOOST_CHECK_EQUAL(id->GetPdb().GetChain(), 'x');
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("pdb|1GAV|!")));
    BOOST_CHECK_EQUAL(id->GetPdb().GetChain(), '!');
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("pdb|1GAV|VB")));
    BOOST_CHECK_EQUAL(id->GetPdb().GetChain(), '|');
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("pdb|1GAV|AAA")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaTpa)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("tpg|BK003456")));
    BOOST_CHECK(id->IsTpg());
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("tpe|BN000123")));
    BOOST_CHECK(id->IsTpe());
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("tpd|FAA00017")));
    BOOST_CHECK(id->IsTpd());
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaGpipe)
{
    CRef<CSeq_id> id;
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("gpp|GPC_123456789")));
    BOOST_CHECK(id->IsGpipe());
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
    
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id(dbtag)));

    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("GenBank", "N20001.1")));
    BOOST_CHECK(id->IsGenbank());
    BOOST_CHECK_EQUAL(id->GetGenbank().GetAccession(), string("N20001"));
    BOOST_CHECK( !id->GetGenbank().IsSetName() );
    BOOST_CHECK_EQUAL(id->GetGenbank().GetVersion(), 1);
    BOOST_CHECK( !id->GetGenbank().IsSetRelease() );

    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GenBank", "N20001.1.1")));
    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GenBank", "N20001.1b")));
    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GenBank", "N20001.-1")));
    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GenBank", "N20001.z")));

    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("GenBank", "12345")));
    BOOST_CHECK(id->IsGi());
    BOOST_CHECK_EQUAL(id->GetGi(), 12345);

    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("GenBank", 12345)));
    BOOST_CHECK(id->IsGi());
    BOOST_CHECK_EQUAL(id->GetGi(), 12345);

    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("EMBL", "AL123456")));
    BOOST_CHECK(id->IsEmbl());
    BOOST_CHECK_EQUAL(id->GetEmbl().GetAccession(), string("AL123456"));

    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("DDBJ", "N00068")));
    BOOST_CHECK(id->IsDdbj());
    BOOST_CHECK_EQUAL(id->GetDdbj().GetAccession(), string("N00068"));

    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GI", "12345X")));

    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("GI", "12345")));
    BOOST_CHECK(id->IsGi());
    BOOST_CHECK_EQUAL(id->GetGi(), 12345);

    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("GI", 12345)));
    BOOST_CHECK(id->IsGi());
    BOOST_CHECK_EQUAL(id->GetGi(), 12345);

    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("taxon", 9606)));
    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("taxon", 9606, true)));
    BOOST_CHECK(id->IsGeneral());

    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("asdf", "jkl")));
    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("asdf", "jkl", true)));
    BOOST_CHECK(id->IsGeneral());
}

BOOST_AUTO_TEST_CASE(s_TestInitFromInt)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Gi, 1234)));
    BOOST_CHECK(id->IsGi());
    BOOST_CHECK_EQUAL(id->GetGi(), 1234);

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id(CSeq_id::e_Gi, 0)));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id(CSeq_id::e_Pdb, 1234)));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Local, 1234)));
    BOOST_CHECK(id->IsLocal());
    BOOST_CHECK(id->GetLocal().IsId());
    BOOST_CHECK_EQUAL(id->GetLocal().GetId(), 1234);

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Gibbsq, 1234)));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Gibbmt, 1234)));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Giim, 1234)));
    BOOST_CHECK(id->IsGiim());
    BOOST_CHECK_EQUAL(id->GetGiim().GetId(), 1234);
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
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id(s)));
    BOOST_CHECK_EQUAL(id->AsFastaString(), s);
    for (SIZE_TYPE pos = strlen(s) - 1;
         pos != NPOS  &&  (s[pos] == '|'  ||  s[pos] == ' ');
         --pos) {
        CRef<CSeq_id> id2;
        string ss(s, pos);
        BOOST_MESSAGE("Testing equality with " << ss);
        BOOST_CHECK_NO_THROW(id2.Reset(new CSeq_id(ss)));
        BOOST_CHECK_EQUAL(id2->AsFastaString(), s);
        BOOST_CHECK(id->Match(*id2));
        BOOST_CHECK_EQUAL(id->Compare(*id2), CSeq_id::e_YES);
    }
}

BOOST_AUTO_TU_REGISTRAR(s_FastaRoundTrip)
(BOOST_PARAM_TEST_CASE(s_TestFastaRoundTrip, kTestFastaStrings + 0,
                       kTestFastaStrings + kNumFastaStrings));

BOOST_AUTO_TEST_CASE(s_TestNoStrays)
{
    CSeq_id id;
    BOOST_CHECK_NO_THROW(id.SetGiim().SetDb("foo"));
    BOOST_CHECK_NO_THROW(id.SetGiim().SetRelease("2.0"));
    BOOST_CHECK(id.IsGiim());
    BOOST_CHECK(id.GetGiim().IsSetDb());
    BOOST_CHECK(id.GetGiim().IsSetRelease());
    BOOST_CHECK_NO_THROW(id.Set("gim|123"));
    BOOST_CHECK(id.IsGiim());
    BOOST_CHECK( !id.GetGiim().IsSetDb() );
    BOOST_CHECK( !id.GetGiim().IsSetRelease() );

    BOOST_CHECK_NO_THROW(id.SetGenbank().SetRelease("135"));
    BOOST_CHECK(id.IsGenbank());
    BOOST_CHECK(id.GetGenbank().IsSetRelease());
    BOOST_CHECK_NO_THROW(id.Set("gb|U12345.1|AMU12345"));
    BOOST_CHECK(id.IsGenbank());
    BOOST_CHECK( !id.GetGenbank().IsSetRelease() );

    BOOST_CHECK_NO_THROW(id.SetPatent().SetCit().SetDoc_type("app"));
    BOOST_CHECK(id.IsPatent());
    BOOST_CHECK(id.GetPatent().GetCit().IsSetDoc_type());
    BOOST_CHECK_NO_THROW(id.Set("pat|US|RE33188|1"));
    BOOST_CHECK(id.IsPatent());
    BOOST_CHECK( !id.GetPatent().GetCit().IsSetDoc_type() );

    BOOST_CHECK_NO_THROW(id.SetPdb().SetRel().SetToTime(GetFastLocalTime()));
    BOOST_CHECK(id.IsPdb());
    BOOST_CHECK(id.GetPdb().IsSetRel());
    BOOST_CHECK_NO_THROW(id.Set("pdb|1GAV|X"));
    BOOST_CHECK(id.IsPdb());
    BOOST_CHECK( !id.GetPdb().IsSetRel() );
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
    BOOST_CHECK_EQUAL(CSeq_id::ParseFastaIds(ids, merged, true),
                      kNumFastaStrings);
    BOOST_CHECK_EQUAL(ids.size(), kNumFastaStrings);
    BOOST_CHECK_EQUAL(CSeq_id::GetStringDescr(bs, CSeq_id::eFormat_FastA),
                      string("gi|1234|ref|NM_000170.1|"));
    BOOST_CHECK_EQUAL(CSeq_id::GetStringDescr(bs, CSeq_id::eFormat_ForceGI),
                      string("gi|1234"));
    BOOST_CHECK_EQUAL
        (CSeq_id::GetStringDescr(bs, CSeq_id::eFormat_BestWithVersion),
         string("ref|NM_000170.1"));
    BOOST_CHECK_EQUAL
        (CSeq_id::GetStringDescr(bs, CSeq_id::eFormat_BestWithoutVersion),
         string("ref|NM_000170"));
    BOOST_CHECK_EQUAL
        (CSeq_id::ParseFastaIds(ids, "gi|1234|junk|pdb|1GAV", true),
         size_t(2));
    NCBI_CHECK_THROW_SEQID
        (CSeq_id::ParseFastaIds(ids, "gi|1234|junk|pdb|1GAV"));
}

BOOST_AUTO_TEST_CASE(s_TestSeq_locAssign)
{
    {
        CRef<CSeq_id> id1(new CSeq_id("gi|1"));
        CRef<CSeq_loc> loc1(new CSeq_loc);
        loc1->SetWhole(*id1);
        CRef<CSeq_loc> mix1(new CSeq_loc);
        mix1->SetMix().Set().push_back(loc1);

        CRef<CSeq_id> id2(new CSeq_id("gi|2"));
        CRef<CSeq_loc> loc2(new CSeq_loc);
        loc2->SetEmpty(*id2);
        CRef<CSeq_loc> mix2(new CSeq_loc);
        mix2->SetMix().Set().push_back(loc2);

        BOOST_CHECK(loc1->IsWhole());
        BOOST_CHECK(loc1->GetWhole().IsGi());
        BOOST_CHECK_EQUAL(loc1->GetWhole().GetGi(), 1);

        BOOST_CHECK(loc2->IsEmpty());
        BOOST_CHECK(loc2->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc2->GetEmpty().GetGi(), 2);

        BOOST_CHECK(loc1->GetId());
        BOOST_CHECK(loc1->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc1->GetId()->GetGi(), 1);

        BOOST_CHECK(loc2->GetId());
        BOOST_CHECK(loc2->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc2->GetId()->GetGi(), 2);

        loc1->Assign(*loc2);

        id1.Reset();
        id2.Reset();

        BOOST_CHECK(loc1->IsEmpty());
        BOOST_CHECK(loc1->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc1->GetEmpty().GetGi(), 2);

        BOOST_CHECK(loc2->IsEmpty());
        BOOST_CHECK(loc2->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc2->GetEmpty().GetGi(), 2);
    
        BOOST_CHECK(loc1->GetId());
        BOOST_CHECK(loc1->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc1->GetId()->GetGi(), 2);

        BOOST_CHECK(loc2->GetId());
        BOOST_CHECK(loc2->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc2->GetId()->GetGi(), 2);
    }
    {
        CRef<CSeq_id> id1(new CSeq_id("gi|1"));
        CRef<CSeq_loc> loc1(new CSeq_loc);
        loc1->SetWhole(*id1);
        CRef<CSeq_feat> feat1(new CSeq_feat);
        feat1->SetData().SetRegion("1");
        feat1->SetLocation(*loc1);

        CRef<CSeq_id> id2(new CSeq_id("gi|2"));
        CRef<CSeq_loc> loc2(new CSeq_loc);
        loc2->SetEmpty(*id2);
        CRef<CSeq_feat> feat2(new CSeq_feat);
        feat2->SetData().SetRegion("2");
        feat2->SetLocation(*loc2);

        BOOST_CHECK(loc1->IsWhole());
        BOOST_CHECK(loc1->GetWhole().IsGi());
        BOOST_CHECK_EQUAL(loc1->GetWhole().GetGi(), 1);

        BOOST_CHECK(loc2->IsEmpty());
        BOOST_CHECK(loc2->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc2->GetEmpty().GetGi(), 2);

        BOOST_CHECK(loc1->GetId());
        BOOST_CHECK(loc1->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc1->GetId()->GetGi(), 1);

        BOOST_CHECK(loc2->GetId());
        BOOST_CHECK(loc2->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc2->GetId()->GetGi(), 2);

        feat1->Assign(*feat2);

        id1.Reset();
        id2.Reset();

        BOOST_CHECK_EQUAL(feat1->GetData().GetRegion(), string("2"));
        loc1 = &feat1->SetLocation();

        BOOST_CHECK(loc1->IsEmpty());
        BOOST_CHECK(loc1->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc1->GetEmpty().GetGi(), 2);

        BOOST_CHECK(loc2->IsEmpty());
        BOOST_CHECK(loc2->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc2->GetEmpty().GetGi(), 2);
    
        BOOST_CHECK(loc1->GetId());
        BOOST_CHECK(loc1->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc1->GetId()->GetGi(), 2);

        BOOST_CHECK(loc2->GetId());
        BOOST_CHECK(loc2->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc2->GetId()->GetGi(), 2);
    }
}
