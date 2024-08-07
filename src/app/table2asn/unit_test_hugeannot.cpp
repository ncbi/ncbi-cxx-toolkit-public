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
* Authors: Sergiy Gotvyanskyy
*
* File Description:
*   Unit test for indexed five column feature table reader
*
*/
#include <ncbi_pch.hpp>
#include "annot_match.hpp"
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <corelib/test_boost.hpp>

using namespace ::ncbi;
using namespace ::ncbi::objects;

struct CTestData
{
    std::string_view fasta_search;
    std::string_view fasta_expected;
    //std::string_view prefix;
};

static constexpr std::string_view s_five_column_data =
R"annot(>Feature lcl|scaffold1
1   100 CDS
>Feature SeqID1
1   100 CDS
>Feature lcl|SeqID2
1   100 CDS
>Feature gnl|xyz|SeqID2
1   100 CDS
>Feature gb|SeqID2
1   100 CDS
>Feature SeqID3
1   100 CDS
>Feature gnl|xyz|SeqID4
1   100 CDS
>Feature GL758201
1   100 CDS
>Feature GL758201.1
1   100 CDS
>Feature GL758201.2
1   100 CDS
>Feature gb|GL758202.1
1   100 CDS
>Feature gb|GL758202.2
1   100 CDS
>Feature gnl|xyz|GL758203
1   100 CDS
>Feature GL758204.1
1   100 CDS
>Feature GL758205
1   100 CDS
)annot";

static constexpr CTestData test_cases [] =
{
    {"lcl|SeqID1", "lcl|SeqID1"},
    {"gnl|xyz|SeqID2", "lcl|SeqID2;gnl|xyz|SeqID2"},
    {"gb|SeqID2", "gb|SeqID2|"},
    {"gb|GL758201.2", "gb|GL758201.2|"},
    {"gb|GL758201.1", "gb|GL758201.1|"},
    {"gb|GL758201.3", "gb|GL758201|;gb|GL758201.2|;gb|GL758201.1|"},
    //{"lcl|SeqID4", ""},
    {"gb|GL758204", "gb|GL758204.1|"},
    {"gb|GL758205.1", "gb|GL758205|"},
};


BOOST_AUTO_TEST_CASE(RunTests)
{
    CFast5colReader reader;
    reader.Init("xyz", 0);
    reader.Open(s_five_column_data);
    BOOST_TEST(reader.size() == 15);
    BOOST_TEST(reader.unused_annots() == 15);
    BOOST_TEST(reader.index_size() == 34);

    for (auto& test: test_cases)
    {
        CBioseq::TId ids;
        CSeq_id::ParseIDs(ids, test.fasta_search, CSeq_id::fParse_RawText | CSeq_id::fParse_ValidLocal | CSeq_id::fParse_PartialOK);
        for (auto& id: ids) {
            auto found = reader.FindAnnots(id);
            string fasta;
            for (auto& found_id: found) {
                auto f = found_id->AsFastaString();
                if (!fasta.empty())
                    fasta.append(";");
                fasta.append(f);
                std::cout << test.fasta_search << ":" << f << "\n";
            }
            BOOST_TEST(fasta == test.fasta_expected);
        }
    }
    BOOST_TEST(reader.unused_annots() == 15);
}
