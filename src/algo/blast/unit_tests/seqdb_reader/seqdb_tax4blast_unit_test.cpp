/*
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
 * Authors:  Christiam Camacho
 *
 * File Description:
 *   Unit tests for CTaxonomy4BlastSQLite
 *
 */
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objtools/blast/seqdb_reader/tax4blastsqlite.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
BOOST_AUTO_TEST_SUITE(seqdb_tax4blast)

BOOST_AUTO_TEST_CASE(InvalidTaxonomyId)
{
    unique_ptr<ITaxonomy4Blast> tb;
    BOOST_REQUIRE_NO_THROW(tb.reset((new CTaxonomy4BlastSQLite("data/t4b.sqlite3"))));
    const vector<int> kInvalidTaxIDs = { -1, 0, -3458 };
    vector<int> desc;
    for (const auto& taxid: kInvalidTaxIDs) {
        BOOST_REQUIRE_NO_THROW(tb->GetLeafNodeTaxids(taxid, desc));
        BOOST_REQUIRE(desc.empty());
    }
}

BOOST_AUTO_TEST_CASE(TaxonomyIdNotFound)
{
    unique_ptr<ITaxonomy4Blast> tb;
    BOOST_REQUIRE_NO_THROW(tb.reset((new CTaxonomy4BlastSQLite("data/t4b.sqlite3"))));
    const int kTaxID(2);
    vector<int> desc;
    BOOST_REQUIRE_NO_THROW(tb->GetLeafNodeTaxids(kTaxID, desc));
    BOOST_REQUIRE(desc.empty());
}

BOOST_AUTO_TEST_CASE(RetrieveHumanTaxids)
{
    unique_ptr<ITaxonomy4Blast> tb;
    BOOST_REQUIRE_NO_THROW(tb.reset((new CTaxonomy4BlastSQLite("data/t4b.sqlite3"))));
    const int kTaxID(9606);
    vector<int> desc;
    BOOST_REQUIRE_NO_THROW(tb->GetLeafNodeTaxids(kTaxID, desc));

    BOOST_REQUIRE_EQUAL(desc.size(), 2U);
    const vector<int> kExpectedResults = {63221,741158};
    for (const auto& expected: kExpectedResults) {
        auto it = std::find(desc.begin(), desc.end(), expected);
        CNcbiOstrstream oss;
        oss << "Failed to find " << expected << " descendant taxid from " << kTaxID;
        BOOST_REQUIRE_MESSAGE(it != desc.end(), CNcbiOstrstreamToString(oss));
    }
}

BOOST_AUTO_TEST_CASE(RetrieveLeafNodeTaxid_Neanderthal)
{
    unique_ptr<ITaxonomy4Blast> tb;
    BOOST_REQUIRE_NO_THROW(tb.reset((new CTaxonomy4BlastSQLite("data/t4b.sqlite3"))));
    // Right now this is a leaf node taxid, therefore it has no descendant taxIDs
    const int kTaxID(63221);
    vector<int> desc;
    BOOST_REQUIRE_NO_THROW(tb->GetLeafNodeTaxids(kTaxID, desc));
    BOOST_REQUIRE(desc.empty());
}

BOOST_AUTO_TEST_CASE(DatabaseNotFound)
{
    BOOST_REQUIRE_THROW(
        CTaxonomy4BlastSQLite non_existent("dummy-data-does-not-exist"),
        CSeqDBException
    );
}

BOOST_AUTO_TEST_CASE(InvalidSQLiteDatabase)
{
    // File is an SQLite database, but it doesn't have the necessary table and index
    BOOST_REQUIRE_THROW(
        CTaxonomy4BlastSQLite non_existent("data/incorrect-db.sqlite3"),
        CSeqDBException
    );
}

BOOST_AUTO_TEST_SUITE_END()
#endif /* SKIP_DOXYGEN_PROCESSING */
