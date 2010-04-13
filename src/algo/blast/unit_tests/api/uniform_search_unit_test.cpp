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
 * Authors: Christiam Camacho
 *
 */

/** @file uniform_search_unit_test.cpp
 * Unit tests for the uniform search API
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/local_search.hpp>
#include <algo/blast/api/remote_search.hpp>

#include <algo/blast/api/objmgr_query_data.hpp>

// needed for objmgr dependent tests of query data interface
#include "test_objmgr.hpp"
#include "blast_test_util.hpp"
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>

// Object utils
#include <objects/scoremat/PssmWithParameters.hpp>

#include <util/random_gen.hpp>

// SeqAlign comparison includes
#include "seqalign_cmp.hpp"
#include "seqalign_set_convert.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

struct TQueryMessagesComparator : 
        public binary_function< CRef<CSearchMessage>, 
                                CRef<CSearchMessage>, 
                                bool>
{ 
        result_type operator() (const first_argument_type& a,
                                const second_argument_type& b) const {
            return *a < *b;
        }
};
    

    static CSearchResultSet
    RunMultipleProteinSearch(ISearchFactory& factory, const string& impl) 
    {
        // Obtain the search components from the factory
        CRef<ISeqSearch> uniform_search = factory.GetSeqSearch();
        CRef<CBlastOptionsHandle> options = factory.GetOptions(eBlastp);
        CConstRef<CSearchDatabase> subject
            (new CSearchDatabase("ecoli.aa", 
                                 CSearchDatabase::eBlastDbIsProtein));

        // Set up the queries
        TSeqLocVector queries;
        CSeq_id query_id0(CSeq_id::e_Gi, 129295);
        auto_ptr<SSeqLoc> sl0(CTestObjMgr::Instance().CreateSSeqLoc(query_id0));
        queries.push_back(*sl0);
        CSeq_id query_id1(CSeq_id::e_Gi, 129296);
        auto_ptr<SSeqLoc> sl1(CTestObjMgr::Instance().CreateSSeqLoc(query_id1));
        queries.push_back(*sl1);
        CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(queries));

        options->SetEvalueThreshold(1.0);
        options->SetHitlistSize(25);

        // Configure and run the uniform search object
        uniform_search->SetQueryFactory(query_factory);
        uniform_search->SetSubject(subject);
        uniform_search->SetOptions(options);
        CSearchResultSet retval = *uniform_search->Run();
        return retval;
    }

BOOST_AUTO_TEST_SUITE(uniform_search);

BOOST_AUTO_TEST_CASE(SortSearchMessages_DifferentSeverity) {
        TQueryMessages messages;
        messages.push_back(CRef<CSearchMessage>
                           (new CSearchMessage(eBlastSevFatal, 1, "test")));
        messages.push_back(CRef<CSearchMessage>
                           (new CSearchMessage(eBlastSevInfo, 1, "test")));
        messages.push_back(CRef<CSearchMessage>
                           (new CSearchMessage(eBlastSevError, 1, "test")));
        messages.push_back(CRef<CSearchMessage>
                           (new CSearchMessage(eBlastSevWarning, 1, "test")));

        sort(messages.begin(), messages.end(), TQueryMessagesComparator());

        for (size_t i = 0; i < messages.size() - 1; i++) {
            BOOST_REQUIRE(messages[i]->GetSeverity() <
                           messages[i+1]->GetSeverity());
        }
}

BOOST_AUTO_TEST_CASE(SortSearchMessages_DifferentErrorId) {
        CRandom random_gen;
        TQueryMessages messages;
        for (int i = 0; i < 10; i++) {
            messages.push_back(CRef<CSearchMessage>
                               (new CSearchMessage(eBlastSevInfo,
                                                   random_gen.GetRand(),
                                                   "test")));
        }

        sort(messages.begin(), messages.end(), TQueryMessagesComparator());

        for (size_t i = 0; i < messages.size() - 1; i++) {
            BOOST_REQUIRE(messages[i]->GetErrorId() <
                           messages[i+1]->GetErrorId());
        }
}

BOOST_AUTO_TEST_CASE(SortSearchMessages_DifferentMessage) {
        CRandom random_gen;
        TQueryMessages messages;

        for (int i = 0; i < 10; i++) {
            string msg("test");
            msg += NStr::IntToString(random_gen.GetRand());
            messages.push_back(CRef<CSearchMessage>
                               (new CSearchMessage(eBlastSevInfo, 2, msg)));
        }

        sort(messages.begin(), messages.end(), TQueryMessagesComparator());

        for (size_t i = 0; i < messages.size() - 1; i++) {
            BOOST_REQUIRE(messages[i]->GetMessage() <
                           messages[i+1]->GetMessage());
        }
}

BOOST_AUTO_TEST_CASE(PartialOrderSearchMessages) {
        const EBlastSeverity kSev = eBlastSevWarning;
        const int kErrorId = 2;
        const string kMsg("hello");

        CSearchMessage m1(kSev, kErrorId, kMsg);
        CSearchMessage m1_copy(kSev, kErrorId, kMsg);
        CSearchMessage m2(kSev, kErrorId+2, kMsg);

        BOOST_REQUIRE(!(m1 < m1_copy));
        BOOST_REQUIRE(m1 < m2);

        CSearchMessage m3(eBlastSevFatal, kErrorId, kMsg);
        BOOST_REQUIRE(m1 < m3);

        CSearchMessage m4(kSev, kErrorId, string(kMsg + " world"));
        BOOST_REQUIRE(m1 < m4);
}

BOOST_AUTO_TEST_CASE(EmptyAlignmentInCSearchResultSet) {
        const string kFname("data/empty_result_set.asn");
        const size_t kNumQueries = 3;
        const int gis[kNumQueries] = { 555, 115988564, 3090 };

        CSearchResultSet::TQueryIdVector queries(kNumQueries);
        TSeqAlignVector alignments(kNumQueries);
        TSearchMessages messages;
        messages.resize(kNumQueries);

        ifstream input(kFname.c_str());
        if ( !input ) {
            throw runtime_error("Failed to read " + kFname);
        }

        for (size_t i = 0; i < kNumQueries; i++) {
            alignments[i].Reset(new CSeq_align_set);
            input >> MSerial_AsnText >> *alignments[i];
            queries[i].Reset(new CSeq_id(CSeq_id::e_Gi, gis[i]));
        }

        CSearchResultSet results(queries, alignments, messages);
        BOOST_REQUIRE_EQUAL(kNumQueries, results.GetNumResults());

        BOOST_REQUIRE(results[0].HasAlignments());
        BOOST_REQUIRE(!results[1].HasAlignments());
        BOOST_REQUIRE(results[2].HasAlignments());
}

BOOST_AUTO_TEST_CASE(EqualitySearchMessages) {
        const EBlastSeverity kSev = eBlastSevWarning;
        const int kErrorId = 2;
        const string kMsg("hello");
        CSearchMessage m1(kSev, kErrorId, kMsg);
        CSearchMessage m2(kSev, kErrorId, kMsg);

        BOOST_REQUIRE(m1 == m2);

        CSearchMessage m3(kSev, kErrorId+1, kMsg);
        BOOST_REQUIRE(m1 != m3);
}

BOOST_AUTO_TEST_CASE(MultipleProteinSearch) {
        CLocalSearchFactory local_factory;
        CSearchResultSet local_results =
            RunMultipleProteinSearch(local_factory, "Local");
        BOOST_REQUIRE(local_results.GetNumResults() > 0);

        CRemoteSearchFactory remote_factory;
        CSearchResultSet remote_results =
            RunMultipleProteinSearch(remote_factory, "Remote");
        BOOST_REQUIRE(remote_results.GetNumResults() > 0);
}

BOOST_AUTO_TEST_CASE(SearchDatabase_RestrictionGiList)
{
    CSeqDBGiList gis;
    gis.AddGi(1);
    gis.AddGi(5);
    CSearchDatabase db("junk", CSearchDatabase::eBlastDbIsProtein);
    db.SetGiList(&gis);
    BOOST_REQUIRE_THROW(db.SetNegativeGiList(&gis), CBlastException);
}

BOOST_AUTO_TEST_CASE(SearchDatabase_Restriction)
{
    CSeqDBGiList gis;
    gis.AddGi(1);
    gis.AddGi(5);
    CSearchDatabase db("junk", CSearchDatabase::eBlastDbIsProtein);
    db.SetNegativeGiList(&gis);
    BOOST_REQUIRE_THROW(db.SetGiList(&gis), CBlastException);
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
