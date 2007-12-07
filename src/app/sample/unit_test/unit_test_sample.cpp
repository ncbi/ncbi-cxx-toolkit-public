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
*   Sample unit test (simplified version of CSeq_id's)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Seq_id.hpp>

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

BOOST_AUTO_TEST_CASE(s_TestInitFromStdAcc)
{
    CRef<CSeq_id> id;

    CHECK_THROW_SEQID(id.Reset(new CSeq_id("BN00123")));
    CHECK_NO_THROW(id.Reset(new CSeq_id("BN000123")));
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

BOOST_AUTO_TEST_CASE(s_TestInitFromFastaGI)
{
    CRef<CSeq_id> id;

    CHECK_NO_THROW(id.Reset(new CSeq_id("gi|1234")));
    CHECK(id->IsGi());
    CHECK_EQUAL(id->GetGi(), 1234);
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
