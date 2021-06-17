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
 * Author:  Greg Boratyn
 *
 * Test module for BLAST SRA input library
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/User_object.hpp>

#include <algo/blast/blast_sra_input/blast_sra_input.hpp>
#include <unordered_map>


using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;


BOOST_AUTO_TEST_SUITE(sra)

// Retrieve segment flags for short read sequences
static int s_GetSegmentFlags(const CBioseq& bioseq)
{
    int retval = 0;

    if (!bioseq.IsSetDescr()) {
        return 0;
    }

    for (auto desc : bioseq.GetDescr().Get()) {
        if (desc->Which() == CSeqdesc::e_User) {

            if (!desc->GetUser().IsSetType() ||
                !desc->GetUser().GetType().IsStr() ||
                desc->GetUser().GetType().GetStr() != "Mapping") {
                continue;
            }

            BOOST_REQUIRE(desc->GetUser().HasField("has_pair"));
            const CUser_field& field = desc->GetUser().GetField("has_pair");
            BOOST_REQUIRE(field.GetData().IsInt());

            retval = field.GetData().GetInt();
        }
    }

    return retval;
}


// Check that flags for paired reads are set correctly
BOOST_AUTO_TEST_CASE(FlagsForPairedReads)
{
    const bool kCheckForPairs = true;
    vector<string> accessions = {"SRR4423739"};

    CSraInputSource input_source(accessions, kCheckForPairs);
    CBlastInputOMF input(&input_source, 2);

    unordered_map<string, int> ref_flags = {
        {"gnl|SRA|SRR4423739.1.1", eFirstSegment},
        {"gnl|SRA|SRR4423739.1.2", eLastSegment}
    };

    CRef<CBioseq_set> queries(new CBioseq_set);
    input.GetNextSeqBatch(*queries);

    BOOST_REQUIRE_EQUAL(queries->GetSeq_set().size(), 2u);

    size_t count = 0;
    for (auto it : queries->GetSeq_set()) {
        string id = it->GetSeq().GetFirstId()->AsFastaString();
        int flags = s_GetSegmentFlags(it->GetSeq());
        int expected = ref_flags.at(id);

        BOOST_REQUIRE_MESSAGE(flags == expected, (string)"Segment flag for " +
                id + " is different from expected " +
                NStr::IntToString(flags) + " != " +
                NStr::IntToString(expected));
        count++;
    }

    BOOST_REQUIRE_EQUAL(ref_flags.size(), count);
}


BOOST_AUTO_TEST_CASE(FlagsForSingleReads)
{
    const bool kCheckForPairs = false;
    vector<string> accessions = {"SRR4423739"};
    CSraInputSource input_source(accessions, kCheckForPairs);
    CBlastInputOMF input(&input_source, 300);
    CRef<CBioseq_set> queries(new CBioseq_set);
    input.GetNextSeqBatch(*queries);

    BOOST_REQUIRE_EQUAL(queries->GetSeq_set().size(), 2u);

    size_t count = 0;
    for (auto it : queries->GetSeq_set()) {
        string id = it->GetSeq().GetFirstId()->AsFastaString();
        int flags = s_GetSegmentFlags(it->GetSeq());
        int expected = 0;

        BOOST_REQUIRE_MESSAGE(flags == expected, (string)"Segment flag for " +
                id + " is different from expected " +
                NStr::IntToString(flags) + " != " +
                NStr::IntToString(expected));
        count++;
    }

    BOOST_REQUIRE_EQUAL(2u, count);
}

// Test that all input SRA accessions are read
BOOST_AUTO_TEST_CASE(MultipleAccessions)
{
    const int kBatchSize = 30000000;
    vector<string> accessions = {"SRR3720856", "SRR5196091"};

    CSraInputSource input_source(accessions);
    CBlastInputOMF input(&input_source, kBatchSize);

    CRef<CBioseq_set> queries;
    queries.Reset(new CBioseq_set);
    input.GetNextSeqBatch(*queries);

    // the first query must be from the first SRA accession
    const CSeq_id* fid = queries->GetSeq_set().front()->GetSeq().GetFirstId();
    BOOST_REQUIRE(fid->GetSeqIdString().find(accessions.front()) != string::npos);

    // the last query must be from the last SRA accession
    const CSeq_id* lid = queries->GetSeq_set().back()->GetSeq().GetFirstId();
    BOOST_REQUIRE(lid->GetSeqIdString().find(accessions.back()) != string::npos);
}

BOOST_AUTO_TEST_CASE(MultipleAccessionsForceSingle)
{
    const int kBatchSize = 30000000;
    const bool kCheckForPairs = false;
    vector<string> accessions = {"SRR3720856", "SRR5196091"};

    CSraInputSource input_source(accessions, kCheckForPairs);
    CBlastInputOMF input(&input_source, kBatchSize);
    CRef<CBioseq_set> queries;
    queries.Reset(new CBioseq_set);
    input.GetNextSeqBatch(*queries);

    // the first query must be from the first SRA accession
    const CSeq_id* fid = queries->GetSeq_set().front()->GetSeq().GetFirstId();
    BOOST_REQUIRE(fid->GetSeqIdString().find(accessions.front()) != string::npos);

    // the last query must be from the last SRA accession
    const CSeq_id* lid = queries->GetSeq_set().back()->GetSeq().GetFirstId();
    BOOST_REQUIRE(lid->GetSeqIdString().find(accessions.back()) != string::npos);
}


BOOST_AUTO_TEST_SUITE_END()

