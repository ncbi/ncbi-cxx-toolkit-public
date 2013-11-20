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
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <boost/test/parameterized_test.hpp>
#include <util/util_exception.hpp>
#include <util/util_misc.hpp>
#include <util/random_gen.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);


#define NCBI_CHECK_THROW_SEQID(s) BOOST_CHECK_THROW(s, CSeqIdException)

NCBITEST_AUTO_INIT()
{
    // force use of built-in accession guide
    g_IgnoreDataFile("accguide.txt");
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
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("4[ip]")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromGIString)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("  1234 ")));
    BOOST_CHECK(id->IsGi());
    BOOST_CHECK(id->GetGi() == GI_FROM(TIntId, 1234));

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

    CSeq_id::EAccessionInfo ai = CSeq_id::IdentifyAccession("DAAA02000001");
    BOOST_CHECK_EQUAL(ai, CSeq_id::eAcc_gb_tpa_wgs_nuc);
    BOOST_CHECK_EQUAL(ai & CSeq_id::eAcc_division_mask, CSeq_id::eAcc_wgs);

    BOOST_CHECK_EQUAL(CSeq_id::IdentifyAccession("DAAA02000000"),
                      CSeq_id::eAcc_gb_tpa_wgsm_nuc);
    BOOST_CHECK_EQUAL(CSeq_id::IdentifyAccession("AACN010000000"),
                      CSeq_id::eAcc_gb_wgsm_nuc);
    BOOST_CHECK_EQUAL(CSeq_id::IdentifyAccession("AACN011000000"),
                      CSeq_id::eAcc_gb_wgs_nuc);

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("BABA01S00009")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("BABA0S1000093")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("BABA01SS000093")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("BABA01T000093")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("BABA010S00093")));
    BOOST_CHECK_EQUAL(CSeq_id::IdentifyAccession("BABA01S000093"),
                      CSeq_id::eAcc_ddbj_wgs_nuc);
    BOOST_CHECK_EQUAL(CSeq_id::IdentifyAccession("BABA01S0000000"),
                      CSeq_id::eAcc_ddbj_wgs_nuc);
    BOOST_CHECK_EQUAL(CSeq_id::IdentifyAccession("BABA01S00009300"),
                      CSeq_id::eAcc_ddbj_wgs_nuc);
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("BABA01S000093000")));

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("MAP_12345")));
    BOOST_CHECK_EQUAL(CSeq_id::IdentifyAccession("MAP_123456"),
                      CSeq_id::eAcc_gb_optical_map);
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("MAP_1234567")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromPRFAcc)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("50086A")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("550086A")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("650771AF")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("0806162C")));
    BOOST_CHECK(id->IsPrf());
    BOOST_CHECK(!id->GetPrf().IsSetAccession());
    BOOST_CHECK_EQUAL(id->GetPrf().GetName(), string("0806162C"));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("2015436HX")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("1309311A:PDB=1EMD,2CMD")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("650771ABC")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("080616C2")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("00806162C")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("0806162C3")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("2015436HIJ")));
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

    BOOST_CHECK_EQUAL(CSeq_id::IdentifyAccession("2004[dp]"),
                      CSeq_id::eAcc_unknown);
    BOOST_CHECK_EQUAL(CSeq_id::IdentifyAccession("2008;358:2545"),
                      CSeq_id::eAcc_unknown);
    BOOST_CHECK_EQUAL(CSeq_id::IdentifyAccession("2000:2010"),
                      CSeq_id::eAcc_unknown);
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

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("NZ_CH95931.1")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("NZ_CH959311.1")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("NZ_CH9593111.1")));

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("NZ_AABC0300051")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("NZ_AABC03000051")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("NZ_ABJB030000051")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("NZ_ABJB0300000510")));
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("NZ_ABJB03000005100")));
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

BOOST_AUTO_TEST_CASE(s_TestInitFromNatAcc)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("AT_12345")));
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("AT_123456789.1")));
    BOOST_CHECK(id->IsNamed_annot_track());
    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("AT_1234567890")));
}

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaLocal)
{
    CRef<CSeq_id> id;

    NCBI_CHECK_THROW_SEQID(id.Reset(new CSeq_id("asd|fgh|jkl")));

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("lcl|0")));
    BOOST_CHECK(id->IsLocal());
    BOOST_CHECK(id->GetLocal().IsStr());
    BOOST_CHECK_EQUAL(id->GetLocal().GetStr(), "0");

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("lcl|123")));
    BOOST_CHECK(id->IsLocal());
    BOOST_CHECK(id->GetLocal().IsId());
    BOOST_CHECK_EQUAL(id->GetLocal().GetId(), 123);

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("lcl|0123")));
    BOOST_CHECK(id->IsLocal());
    BOOST_CHECK(id->GetLocal().IsStr());
    BOOST_CHECK_EQUAL(id->GetLocal().GetStr(), "0123");

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("lcl|-123")));
    BOOST_CHECK(id->IsLocal());
    BOOST_CHECK(id->GetLocal().IsStr());
    BOOST_CHECK_EQUAL(id->GetLocal().GetStr(), "-123");

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
    BOOST_CHECK_EQUAL(id->GetGi(), GI_FROM(TIntId, 1234));
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

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("prf||0806162C")));
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

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaNat)
{
    CRef<CSeq_id> id;
    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id("nat|AT_123456789")));
    BOOST_CHECK(id->IsNamed_annot_track());
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

    // No longer supported.
    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GenBank", "N20001.1")));

    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("EMBL", "AL123456.7")));
    BOOST_CHECK(id->IsEmbl());
    BOOST_CHECK_EQUAL(id->GetEmbl().GetAccession(), string("AL123456"));
    BOOST_CHECK( !id->GetEmbl().IsSetName() );
    BOOST_CHECK_EQUAL(id->GetEmbl().GetVersion(), 7);
    BOOST_CHECK( !id->GetEmbl().IsSetRelease() );

    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("EMBL", "AL123456.7.8")));
    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("EMBL", "AL123456.7b")));
    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("EMBL", "AL123456.-7")));
    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("EMBL", "AL123456.z")));

    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("DDBJ", "N00068")));
    BOOST_CHECK(id->IsDdbj());
    BOOST_CHECK_EQUAL(id->GetDdbj().GetAccession(), string("N00068"));

    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("GI", "12345X")));

    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("GI", "12345")));
    BOOST_CHECK(id->IsGi());
    BOOST_CHECK_EQUAL(id->GetGi(), GI_FROM(TIntId, 12345));

    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("GI", 12345)));
    BOOST_CHECK(id->IsGi());
    BOOST_CHECK_EQUAL(id->GetGi(), GI_FROM(TIntId, 12345));

    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("taxon", 9606)));
    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("taxon", 9606, true)));
    BOOST_CHECK_EQUAL(id->IdentifyAccession(), CSeq_id::eAcc_general);

    NCBI_CHECK_THROW_SEQID(id.Reset(s_NewDbtagId("TRACE_ASSM", "992")));
    BOOST_CHECK_NO_THROW(id.Reset(s_NewDbtagId("TRACE_ASSM", "992", true)));
    BOOST_CHECK_EQUAL(id->IdentifyAccession(), CSeq_id::eAcc_general_nuc);
}

BOOST_AUTO_TEST_CASE(s_TestInitFromInt)
{
    CRef<CSeq_id> id;

    BOOST_CHECK_NO_THROW(id.Reset(new CSeq_id(CSeq_id::e_Gi, 1234)));
    BOOST_CHECK(id->IsGi());
    BOOST_CHECK_EQUAL(id->GetGi(), GI_FROM(TIntId, 1234));

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
    "prf||0806162C",
    "pdb|1GAV| ",
    "pdb|1GAV|X",
    "pdb|1GAV|XX",
    "pdb|1GAV|!",
    "pdb|1GAV|VB",
    "tpg|BK003456|",
    "tpe|BN000123|",
    "tpd|FAA00017|",
    "gpp|GPC_123456789|",
    "nat|AT_123456789.1|",
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

BOOST_AUTO_PARAM_TEST_CASE(s_TestFastaRoundTrip, kTestFastaStrings + 0,
                           kTestFastaStrings + kNumFastaStrings);

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
        BOOST_CHECK_EQUAL(loc1->GetWhole().GetGi(), GI_FROM(TIntId, 1));

        BOOST_CHECK(loc2->IsEmpty());
        BOOST_CHECK(loc2->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc2->GetEmpty().GetGi(), GI_FROM(TIntId, 2));

        BOOST_CHECK(loc1->GetId());
        BOOST_CHECK(loc1->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc1->GetId()->GetGi(), GI_FROM(TIntId, 1));

        BOOST_CHECK(loc2->GetId());
        BOOST_CHECK(loc2->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc2->GetId()->GetGi(), GI_FROM(TIntId, 2));

        loc1->Assign(*loc2);

        id1.Reset();
        id2.Reset();

        BOOST_CHECK(loc1->IsEmpty());
        BOOST_CHECK(loc1->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc1->GetEmpty().GetGi(), GI_FROM(TIntId, 2));

        BOOST_CHECK(loc2->IsEmpty());
        BOOST_CHECK(loc2->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc2->GetEmpty().GetGi(), GI_FROM(TIntId, 2));

        BOOST_CHECK(loc1->GetId());
        BOOST_CHECK(loc1->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc1->GetId()->GetGi(), GI_FROM(TIntId, 2));

        BOOST_CHECK(loc2->GetId());
        BOOST_CHECK(loc2->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc2->GetId()->GetGi(), GI_FROM(TIntId, 2));
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
        BOOST_CHECK_EQUAL(loc1->GetWhole().GetGi(), GI_FROM(TIntId, 1));

        BOOST_CHECK(loc2->IsEmpty());
        BOOST_CHECK(loc2->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc2->GetEmpty().GetGi(), GI_FROM(TIntId, 2));

        BOOST_CHECK(loc1->GetId());
        BOOST_CHECK(loc1->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc1->GetId()->GetGi(), GI_FROM(TIntId, 1));

        BOOST_CHECK(loc2->GetId());
        BOOST_CHECK(loc2->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc2->GetId()->GetGi(), GI_FROM(TIntId, 2));

        feat1->Assign(*feat2);

        id1.Reset();
        id2.Reset();

        BOOST_CHECK_EQUAL(feat1->GetData().GetRegion(), string("2"));
        loc1 = &feat1->SetLocation();

        BOOST_CHECK(loc1->IsEmpty());
        BOOST_CHECK(loc1->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc1->GetEmpty().GetGi(), GI_FROM(TIntId, 2));

        BOOST_CHECK(loc2->IsEmpty());
        BOOST_CHECK(loc2->GetEmpty().IsGi());
        BOOST_CHECK_EQUAL(loc2->GetEmpty().GetGi(), GI_FROM(TIntId, 2));

        BOOST_CHECK(loc1->GetId());
        BOOST_CHECK(loc1->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc1->GetId()->GetGi(), GI_FROM(TIntId, 2));

        BOOST_CHECK(loc2->GetId());
        BOOST_CHECK(loc2->GetId()->IsGi());
        BOOST_CHECK_EQUAL(loc2->GetId()->GetGi(), GI_FROM(TIntId, 2));
    }
}



BOOST_AUTO_TEST_CASE(s_TestSeq_id_GetLabel)
{
    static const char* sc_SeqIdLabels[] = {
        // order is important!
        // - raw id in ASN.1
        // - type
        // - content
        // - both
        // - fasta (CSeq_id::AsFastaString())
        // - seq-id string, +version
        // - seq-id string, -version
        "Seq-id ::= gi 1234",
        "gi", "1234", "gi|1234",
        "gi|1234", "1234", "1234",

        "Seq-id ::= other { accession \"NM_123456\", version 1}",
        "ref", "NM_123456.1", "ref|NM_123456.1",
        "ref|NM_123456.1|", "NM_123456.1", "NM_123456",

        "Seq-id ::= general { db \"ti\", tag id 1}",
        "gnl", "ti:1", "gnl|ti:1",
        "gnl|ti|1", "ti:1", "ti:1",

        "Seq-id ::= general { db \"NCBI_GENOMES\", tag id 1}",
        "gnl", "NCBI_GENOMES:1", "gnl|NCBI_GENOMES:1",
        "gnl|NCBI_GENOMES|1", "NCBI_GENOMES:1", "NCBI_GENOMES:1",

        "Seq-id ::= pir { name \"S34010\" }",
        "pir", "S34010", "pir|S34010",
        "pir||S34010", "S34010", "S34010",

        "Seq-id ::= patent { seqid 257, cit { country \"JP\", id number \"2003530853\" } }",
        "pat", "JP2003530853_257", "pat|JP2003530853_257",
        "pat|JP|2003530853|257", "JP2003530853_257", "JP2003530853_257",

        "Seq-id ::= pdb { mol \"1GAV\", chain 120 }",
        "pdb", "1GAV_XX", "pdb|1GAV_XX",
        "pdb|1GAV|XX", "1GAV_XX", "1GAV_XX",

        NULL, NULL, NULL, NULL, NULL, NULL
    };


    const char** p = sc_SeqIdLabels;
    for ( ;  p  &&  *p;  p += 7) {
        const char* src_id    = *(p + 0);
        const char* type      = *(p + 1);
        const char* content   = *(p + 2);
        const char* both      = *(p + 3);
        const char* fasta_str = *(p + 4);
        const char* seqid_str1 = *(p + 5);
        const char* seqid_str2 = *(p + 6);

        LOG_POST(Info << "checking ID: " << src_id);
        CSeq_id id;
        {{
             CNcbiIstrstream istr(src_id);
             istr >> MSerial_AsnText >> id;
         }}

        string s;

        s.erase();
        id.GetLabel(&s, CSeq_id::eType);
        LOG_POST(Info << "  type label: " << s);
        BOOST_CHECK_EQUAL(s, type);

        s.erase();
        id.GetLabel(&s, CSeq_id::eContent);
        LOG_POST(Info << "  content label: " << s);
        BOOST_CHECK_EQUAL(s, content);

        s.erase();
        id.GetLabel(&s, CSeq_id::eBoth);
        LOG_POST(Info << "  type + content label: " << s);
        BOOST_CHECK_EQUAL(s, both);

        LOG_POST(Info << "  fasta string: " << id.AsFastaString());
        BOOST_CHECK_EQUAL(id.AsFastaString(), fasta_str);
        LOG_POST(Info << "  id.GetSeqIdString(true): "
                 << id.GetSeqIdString(true));
        BOOST_CHECK_EQUAL(id.GetSeqIdString(true), seqid_str1);
        LOG_POST(Info << "  id.GetSeqIdString(false): "
                 << id.GetSeqIdString(false));
        BOOST_CHECK_EQUAL(id.GetSeqIdString(false), seqid_str2);
    }
}


#if 0
/// NB: disabled, as certain of these tests are guaranteed to fail
BOOST_AUTO_TEST_CASE(s_TestSeq_id_GetLabel_FastaString)
{
    static const char* sc_Ids = "\
Seq-id ::= pir {\
  name \"S34010\"\
}\
Seq-id ::= patent {\
  seqid 257,\
  cit {\
    country \"JP\",\
    id number \"2003530853\"\
  }\
}\
";

    CNcbiIstrstream istr(sc_Ids);
    while (istr) {
        CSeq_id id;
        try {
            istr >> MSerial_AsnText >> id;
        }
        catch (CEofException&) {
            break;
        }

        string fasta_seqid = id.AsFastaString();
        string label;
        id.GetLabel(&label, CSeq_id::eBoth);
        BOOST_CHECK_EQUAL(label, fasta_seqid);

        CSeq_id other(label);
        BOOST_CHECK(other.Equals(id));
    }
}
#endif


BOOST_AUTO_TEST_CASE(s_TestSeq_id_Compare)
{
    // The array sc_Ids is sorted to match CompareOrdered().
    // Some elements may compare equal.
    static const char* const sc_Ids[] = {
        "lcl|12",
        "lcl|12",
        "lcl|13",
        "lcl|13",
        "lcl|123",
        "lcl|123",
        "lcl|124",
        "lcl|124",
        "lcl|0012",
        "lcl|00123",
        "lcl|00124",
        "lcl|0013",
        "lcl|012",
        "lcl|0123",
        "lcl|0124",
        "lcl|013",
        "NC_000001",
        "ref|NC_000001|chr1_build35",
        "ref|NC_000001|chr1_build36",
        "NC_000001.8",
        "nc_000001.8",
        "NC_000001.9",
        "Nc_000001.9",
        "ref|NC_000001.9|chr1_build36",
        "gnl|ti|-12312",
        "gnl|ti|0",
        "gnl|ti|12312",
        "gnl|ti|3231212",
        "gnl|ti|42312324",
        "gnl|TI|52312123124",
        "gnl|ti|623121231214",
        "gnl|ti|22312-234",
        "gnl|TI|str",
        "gnl|TRACE|-12312",
        "gnl|TRACE|0",
        "gnl|TRACE|12312",
        "gnl|trace|3231212",
        "gnl|TRACE|42312324",
        "gnl|TRACE|52312123124",
        "gnl|trace|623121231214",
        "gnl|TRACE|22312-234",
        "gnl|trace|str",
    };

    typedef CRef<CSeq_id> TRef;
    vector<TRef> ids;
    for ( size_t i = 0; i < ArraySize(sc_Ids); ++i ) {
        ids.push_back(TRef(new CSeq_id(sc_Ids[i])));
        if ( i > 0 && ids[i]->IsLocal() && ids[i]->Equals(*ids[i-1]) ) {
            int id = ids[i]->GetLocal().GetId();
            ids[i]->SetLocal().SetStr(NStr::NumericToString(id));
        }
    }
    CRandom rnd(1);
    for ( size_t i = 0; i < ids.size(); ++i ) {
        swap(ids[i], ids[rnd.GetRand(i, ids.size()-1)]);
    }
    vector<TRef> sorted_ids = ids;
    sort(sorted_ids.begin(), sorted_ids.end(), PPtrLess<TRef>());
    for ( size_t i = 0; i < sorted_ids.size(); ++i ) {
        BOOST_CHECK_EQUAL(sorted_ids[i]->CompareOrdered(*sorted_ids[i]), 0);
        for ( size_t j = 0; j < i; ++j ) {
            BOOST_CHECK(sorted_ids[j]->CompareOrdered(*sorted_ids[i]) <= 0);
            BOOST_CHECK(sorted_ids[i]->CompareOrdered(*sorted_ids[j]) >= 0);
        }
        CSeq_id expected_id(sc_Ids[i]);
        if ( expected_id.CompareOrdered(*sorted_ids[i]) != 0 ) {
            BOOST_CHECK_EQUAL(sorted_ids[i]->AsFastaString(),
                              expected_id.AsFastaString());
            BOOST_CHECK_EQUAL(sorted_ids[i]->AsFastaString(), "");
        }
    }
    typedef set<TRef, PPtrLess<TRef> > TSet;
    TSet ids_set(ids.begin(), ids.end());
    BOOST_CHECK(ids_set.size() < sorted_ids.size());
    ITERATE ( TSet, it, ids_set ) {
        //NcbiCout << (*it)->AsFastaString() << NcbiEndl;
        BOOST_CHECK_EQUAL((*it)->CompareOrdered(**it), 0);
        ITERATE ( TSet, it2, ids_set ) {
            if ( it2 == it ) {
                break;
            }
            BOOST_CHECK((*it2)->CompareOrdered(**it) < 0);
            BOOST_CHECK((*it)->CompareOrdered(**it2) > 0);
        }
    }
}

BEGIN_LOCAL_NAMESPACE;

static const char* const sc_Ids[] = {
    "gnl|ti|12312",
    "gi|3231212",
    "NC_000001"
};

CRef<CSeq_loc> GetRandomSegment(CRandom& rnd)
{
    CRef<CSeq_loc> loc(new CSeq_loc);
    if ( rnd.GetRand(0, 10) == 0 ) {
        loc->SetNull();
    }
    else {
        CRef<CSeq_id> id(new CSeq_id(sc_Ids[rnd.GetRand(0, 2)]));
        TSeqPos from = rnd.GetRand(0, 10);
        TSeqPos to = rnd.GetRand(0, 10);
        if ( from == to && rnd.GetRand(0, 1) ) {
            loc->SetPnt().SetId(*id);
            loc->SetPnt().SetPoint(from);
            if ( rnd.GetRand(0, 1) ) {
                loc->SetPnt().SetStrand(eNa_strand_minus);
            }
        }
        else {
            loc->SetInt().SetId(*id);
            if ( from > to || (from == to && rnd.GetRand(0, 1)) ) {
                swap(from, to);
                loc->SetInt().SetStrand(eNa_strand_minus);
            }
            loc->SetInt().SetFrom(from);
            loc->SetInt().SetTo(to);
        }
    }
    return loc;
}

struct PSeq_locLess {
    bool operator()(const CSeq_loc& a, const CSeq_loc& b) const {
        if ( 0 && (a.IsNull() || b.IsNull()) ) {
            cout << "a: "<<MSerial_AsnText<<a;
            cout << "b: "<<MSerial_AsnText<<b<<endl;
        }
        int diff = a.Compare(b);
        if ( 0 && (a.IsNull() || b.IsNull()) ) {
            cout << " = " << diff << endl;
        }
        //a.GetId();
        //b.GetId();
        return diff < 0;
    }
    bool operator()(const CRef<CSeq_loc>& a, const CRef<CSeq_loc>& b) const {
        return (*this)(*a, *b);
    }
};

END_LOCAL_NAMESPACE;

BOOST_AUTO_TEST_CASE(s_TestSeq_id_Compare_Seq_loc)
{
    CRandom rnd(1);
    for ( int t = 0; t < 1000; ++t ) {
        vector< CRef<CSeq_loc> > locs;
        for ( int i = 0; i < 10; ++i ) {
            size_t segs = rnd.GetRand(1, 10);
            CRef<CSeq_loc> loc(new CSeq_loc);
            if ( segs == 1 && rnd.GetRand(0, 1) ) {
                loc = GetRandomSegment(rnd);
            }
            else {
                for ( size_t j = 0; j < segs; ++j ) {
                    loc->SetMix().Set().push_back(GetRandomSegment(rnd));
                }
            }
            locs.push_back(loc);
        }
        sort(locs.begin(), locs.end(), PSeq_locLess());
        for ( size_t i = 0; i < locs.size(); ++i ) {
            //cout << i << ": " << MSerial_AsnText << *locs[i] << endl;
            BOOST_CHECK_EQUAL(locs[i]->Compare(*locs[i]), 0);
            if ( locs[i]->Compare(*locs[i]) != 0 ) {
                cout << i << ": " << MSerial_AsnText << *locs[i];
                cout << " = " << locs[i]->Compare(*locs[i]) << endl;
            }
            for ( size_t j = 0; j < i; ++j ) {
                BOOST_CHECK(locs[j]->Compare(*locs[i]) <= 0);
                BOOST_CHECK(locs[i]->Compare(*locs[j]) >= 0);
                if ( locs[j]->Compare(*locs[i]) > 0 ||
                     locs[i]->Compare(*locs[j]) < 0 ) {
                    cout << j << ": " << MSerial_AsnText << *locs[j];
                    cout << i << ": " << MSerial_AsnText << *locs[i];
                    cout << " = " << locs[j]->Compare(*locs[i]) << endl;
                    cout << i << ": " << MSerial_AsnText << *locs[i];
                    cout << j << ": " << MSerial_AsnText << *locs[j];
                    cout << " = " << locs[i]->Compare(*locs[j]) << endl;
                }
            }
        }
    }
}


ostream& operator<<(ostream& out, const CSeq_id_Handle::TMatches& ids)
{
    ITERATE ( CSeq_id_Handle::TMatches, it, ids ) {
        if ( it != ids.begin() ) {
            out << ',';
        }
        out << *it;
    }
    return out;
}


void s_Match_id(size_t num_ids,
                const char* const fasta_ids[],
                const char* const match_to_ids[],
                const char* const weak_match_to_ids[])
{
    vector<CSeq_id_Handle> ids;
    map<CSeq_id_Handle, CSeq_id_Handle::TMatches> match_to_map;
    map<CSeq_id_Handle, CSeq_id_Handle::TMatches> weak_match_to_map;
    map<CSeq_id_Handle, CSeq_id_Handle::TMatches> matching_map;
    map<CSeq_id_Handle, CSeq_id_Handle::TMatches> weak_matching_map;
    for ( size_t i = 0; i < num_ids; ++i ) {
        CSeq_id_Handle id = CSeq_id_Handle::GetHandle(fasta_ids[i]);
        ids.push_back(id);
        vector<string> ids;
        NStr::Tokenize(match_to_ids[i], ",", ids);
        ITERATE ( vector<string>, it, ids ) {
            CSeq_id_Handle match_to_id = CSeq_id_Handle::GetHandle(*it);
            match_to_map[id].insert(match_to_id);
            matching_map[match_to_id].insert(id);
        }
        ids.clear();
        NStr::Tokenize(weak_match_to_ids[i], ",", ids);
        ITERATE ( vector<string>, it, ids ) {
            CSeq_id_Handle match_to_id = CSeq_id_Handle::GetHandle(*it);
            weak_match_to_map[id].insert(match_to_id);
            weak_matching_map[match_to_id].insert(id);
        }
    }
    for ( size_t i = 0; i < num_ids; ++i ) {
        CSeq_id_Handle::TMatches matches;
        ids[i].GetMatchingHandles(matches);
        CSeq_id_Handle::TMatches exp_matches = matching_map[ids[i]];
        exp_matches.insert(ids[i]);
        if ( matches != exp_matches ) {
            NcbiCerr << "Bad matches for " << ids[i] << NcbiEndl;
            NcbiCerr << "got: " << matches << NcbiEndl;
            NcbiCerr << "exp: " << exp_matches << NcbiEndl;
        }
        BOOST_CHECK(matches == exp_matches);
        ITERATE ( CSeq_id_Handle::TMatches, it, matches ) {
            BOOST_CHECK(ids[i].MatchesTo(*it));
        }
    }
    for ( size_t i = 0; i < num_ids; ++i ) {
        CSeq_id_Handle::TMatches matches;
        ids[i].GetReverseMatchingHandles(matches);
        CSeq_id_Handle::TMatches exp_matches = match_to_map[ids[i]];
        exp_matches.insert(ids[i]);
        const CSeq_id_Handle::TMatches& more_matches = matching_map[ids[i]];
        exp_matches.insert(more_matches.begin(), more_matches.end());
        if ( matches != exp_matches ) {
            NcbiCerr << "Bad rev matches for " << ids[i] << NcbiEndl;
            NcbiCerr << "got: " << matches << NcbiEndl;
            NcbiCerr << "exp: " << exp_matches << NcbiEndl;
        }
        BOOST_CHECK(matches == exp_matches);
        ITERATE ( CSeq_id_Handle::TMatches, it, matches ) {
            BOOST_CHECK(ids[i].MatchesTo(*it)||it->MatchesTo(ids[i]));
        }
    }
    for ( size_t i = 0; i < num_ids; ++i ) {
        CSeq_id_Handle::TMatches matches;
        ids[i].GetMatchingHandles(matches, eAllowWeakMatch);
        CSeq_id_Handle::TMatches exp_matches = weak_matching_map[ids[i]];
        exp_matches.insert(ids[i]);
        if ( matches != exp_matches ) {
            NcbiCerr << "Bad weak matches for " << ids[i] << NcbiEndl;
            NcbiCerr << "got: " << matches << NcbiEndl;
            NcbiCerr << "exp: " << exp_matches << NcbiEndl;
        }
        BOOST_CHECK(matches == exp_matches);
        ITERATE ( CSeq_id_Handle::TMatches, it, matches ) {
            CSeq_id_Handle id2 = *it;
            if ( ids[i].Which() != id2.Which() ) {
                CSeq_id id;
                id.Select(ids[i].Which());
                const_cast<CTextseq_id&>(*id.GetTextseq_Id())
                    .Assign(*id2.GetSeqId()->GetTextseq_Id());
                id2 = CSeq_id_Handle::GetHandle(id);
            }
            BOOST_CHECK(ids[i].MatchesTo(id2));
        }
    }
    for ( size_t i = 0; i < num_ids; ++i ) {
        CSeq_id_Handle::TMatches matches;
        ids[i].GetReverseMatchingHandles(matches, eAllowWeakMatch);
        CSeq_id_Handle::TMatches exp_matches = weak_match_to_map[ids[i]];
        exp_matches.insert(ids[i]);
        const CSeq_id_Handle::TMatches& more_matches = weak_matching_map[ids[i]];
        exp_matches.insert(more_matches.begin(), more_matches.end());
        if ( matches != exp_matches ) {
            NcbiCerr << "Bad weak rev matches for " << ids[i] << NcbiEndl;
            NcbiCerr << "got: " << matches << NcbiEndl;
            NcbiCerr << "exp: " << exp_matches << NcbiEndl;
        }
        BOOST_CHECK(matches == exp_matches);
        ITERATE ( CSeq_id_Handle::TMatches, it, matches ) {
            CSeq_id_Handle id2 = *it;
            if ( ids[i].Which() != id2.Which() ) {
                CSeq_id id;
                id.Select(ids[i].Which());
                const_cast<CTextseq_id&>(*id.GetTextseq_Id())
                    .Assign(*id2.GetSeqId()->GetTextseq_Id());
                id2 = CSeq_id_Handle::GetHandle(id);
            }
            BOOST_CHECK(ids[i].MatchesTo(id2)||id2.MatchesTo(ids[i]));
        }
    }
}

BOOST_AUTO_TEST_CASE(Match_id1)
{
    const char* const fasta_ids[] = {
        "gb|A000001",
        "gb|A000001.2",
        "gb|A000001.3",
        "tpg|A000001",
        "tpg|A000001.2",
        "tpg|A000001.3",
    };
    const char* const match_to_ids[] = {
        "",
        "gb|A000001",
        "gb|A000001",
        "",
        "tpg|A000001",
        "tpg|A000001",
    };
    const char* const weak_match_to_ids[] = {
        "tpg|A000001",
        "tpg|A000001,tpg|A000001.2,gb|A000001",
        "tpg|A000001,tpg|A000001.3,gb|A000001",
        "gb|A000001",
        "gb|A000001,gb|A000001.2,tpg|A000001",
        "gb|A000001,gb|A000001.3,tpg|A000001",
    };
    s_Match_id(ArraySize(fasta_ids),
               fasta_ids, match_to_ids, weak_match_to_ids);
}


BOOST_AUTO_TEST_CASE(Match_id2)
{
    const char* const fasta_ids[] = {
        "gb|A000001",
        "gb|A000001.2",
        "gb|A000001.3",
        "tpg|A000001",
        "tpg|A000001.2",
        "tpg|A000001.3",
        "gb|AAAAAAA",
        "gb|AAAAAAA.2",
        "gb|AAAAAAA.4",
        "tpg|AAAAAAA",
        "tpg|AAAAAAA.2",
        "tpg|AAAAAAA.5",
    };
    const char* const match_to_ids[] = {
        "",
        "gb|A000001",
        "gb|A000001",
        "",
        "tpg|A000001",
        "tpg|A000001",
        "",
        "gb|AAAAAAA",
        "gb|AAAAAAA",
        "",
        "tpg|AAAAAAA",
        "tpg|AAAAAAA",
    };
    const char* const weak_match_to_ids[] = {
        "tpg|A000001",
        "tpg|A000001,tpg|A000001.2,gb|A000001",
        "tpg|A000001,tpg|A000001.3,gb|A000001",
        "gb|A000001",
        "gb|A000001,gb|A000001.2,tpg|A000001",
        "gb|A000001,gb|A000001.3,tpg|A000001",
        "tpg|AAAAAAA",
        "tpg|AAAAAAA,tpg|AAAAAAA.2,gb|AAAAAAA",
        "tpg|AAAAAAA,gb|AAAAAAA",
        "gb|AAAAAAA",
        "gb|AAAAAAA,gb|AAAAAAA.2,tpg|AAAAAAA",
        "gb|AAAAAAA,tpg|AAAAAAA",
    };
    s_Match_id(ArraySize(fasta_ids),
               fasta_ids, match_to_ids, weak_match_to_ids);
}


BOOST_AUTO_TEST_CASE(Match_id3)
{
    const char* const fasta_ids[] = {
        "gb|A000001",
        "gb|A000001.2",
        "gb|A000001.3",
        "tpg|A000001",
        "tpg|A000001.2",
        "tpg|A000001.3",
        "gb|A000002",
        "gb|A000002.2",
        "gb|A000002.4",
        "tpg|A000002",
        "tpg|A000002.2",
        "tpg|A000002.5",
    };
    const char* const match_to_ids[] = {
        "",
        "gb|A000001",
        "gb|A000001",
        "",
        "tpg|A000001",
        "tpg|A000001",
        "",
        "gb|A000002",
        "gb|A000002",
        "",
        "tpg|A000002",
        "tpg|A000002",
    };
    const char* const weak_match_to_ids[] = {
        "tpg|A000001",
        "tpg|A000001,tpg|A000001.2,gb|A000001",
        "tpg|A000001,tpg|A000001.3,gb|A000001",
        "gb|A000001",
        "gb|A000001,gb|A000001.2,tpg|A000001",
        "gb|A000001,gb|A000001.3,tpg|A000001",
        "tpg|A000002",
        "tpg|A000002,tpg|A000002.2,gb|A000002",
        "tpg|A000002,gb|A000002",
        "gb|A000002",
        "gb|A000002,gb|A000002.2,tpg|A000002",
        "gb|A000002,tpg|A000002",
    };
    s_Match_id(ArraySize(fasta_ids),
               fasta_ids, match_to_ids, weak_match_to_ids);
}


NCBITEST_INIT_TREE()
{
    NCBITEST_DISABLE(Match_id3);
}
