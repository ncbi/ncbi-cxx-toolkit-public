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
 * Author:  Michael Kornbluh, NCBI
 *
 * File Description:
 *   Unit test for CSeqFeatData and some closely related code
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <objects/seqfeat/SeqFeatData.hpp>
#include <corelib/ncbimisc.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <boost/test/parameterized_test.hpp>
#include <util/util_exception.hpp>
#include <util/util_misc.hpp>
#include <util/random_gen.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {
    bool s_TestSubtype(CSeqFeatData::ESubtype eSubtype) {
        const string & sNameOfSubtype = 
            CSeqFeatData::SubtypeValueToName(eSubtype);
        if( sNameOfSubtype.empty() ) {
            return false;
        }
        // make sure it goes in the other direction, too
        const CSeqFeatData::ESubtype eReverseSubtype =
            CSeqFeatData::SubtypeNameToValue(sNameOfSubtype);
        BOOST_CHECK( eReverseSubtype != CSeqFeatData::eSubtype_bad );
        BOOST_CHECK_EQUAL(eSubtype, eReverseSubtype);
        return true;
    }
}

BOOST_AUTO_TEST_CASE(s_TestSubtypeMaps)
{
    typedef set<CSeqFeatData::ESubtype> TSubtypeSet;
    // this set holds the subtypes where we expect it to fail
    // when we try to convert to string
    TSubtypeSet subtypesExpectedToFail;

#ifdef ESUBTYPE_SHOULD_FAIL
#  error ESUBTYPE_SHOULD_FAIL already defined
#endif

#define ESUBTYPE_SHOULD_FAIL(name) \
    subtypesExpectedToFail.insert(CSeqFeatData::eSubtype_##name);

    // this list seems kind of long.  Maybe some of these
    // should be added to the lookup maps so they
    // do become valid.
    ESUBTYPE_SHOULD_FAIL(bad);
    ESUBTYPE_SHOULD_FAIL(EC_number);
    ESUBTYPE_SHOULD_FAIL(Imp_CDS);
    ESUBTYPE_SHOULD_FAIL(allele);
    ESUBTYPE_SHOULD_FAIL(imp);
    ESUBTYPE_SHOULD_FAIL(misc_RNA);
    ESUBTYPE_SHOULD_FAIL(mutation);
    ESUBTYPE_SHOULD_FAIL(org);
    ESUBTYPE_SHOULD_FAIL(precursor_RNA);
    ESUBTYPE_SHOULD_FAIL(source);
    ESUBTYPE_SHOULD_FAIL(max);

#undef ESUBTYPE_SHOULD_FAIL

    ITERATE_0_IDX(iSubtypeAsInteger, CSeqFeatData::eSubtype_max) 
    {
        CSeqFeatData::ESubtype eSubtype = 
            static_cast<CSeqFeatData::ESubtype>(iSubtypeAsInteger);

        // subtypesExpectedToFail tells us which ones are
        // expected to fail.  Others are expected to succeed
        if( subtypesExpectedToFail.find(eSubtype) == 
            subtypesExpectedToFail.end() ) 
        {
            NCBITEST_CHECK(s_TestSubtype(eSubtype));
        } else {
            NCBITEST_CHECK( ! s_TestSubtype(eSubtype) );
        }
    }

    // a couple values we haven't covered that should
    // also be tested.
    NCBITEST_CHECK( ! s_TestSubtype(CSeqFeatData::eSubtype_max) );
    NCBITEST_CHECK( ! s_TestSubtype(CSeqFeatData::eSubtype_any) );
}
