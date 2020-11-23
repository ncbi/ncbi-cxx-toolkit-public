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
* Author:  Justin Foley
*
* File Description:
*   Unit tests for CLinkage_evidence class
*
*/

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objects/seq/Linkage_evidence.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(GetLinkageEvidence) 
{
    vector<string> evidence_strings {"paired-ends", 
                                     "align_genus", 
                                     "align_xgenus", 
                                     "align_trnscpt", 
                                     "within_clone",
                                     "clone_contig",
                                     "map",
                                     "strobe",
                                     "unspecified",
                                     "pcr",
                                     "proximity_ligation"};

    CLinkage_evidence::TLinkage_evidence output_vector;
    auto success = CLinkage_evidence::GetLinkageEvidence(output_vector, evidence_strings);

    BOOST_CHECK(success);
    BOOST_CHECK_EQUAL(output_vector.size(), evidence_strings.size()); 
}


BOOST_AUTO_TEST_CASE(VecToString)
{
    vector<string> initial_strings  {"paired-ends", 
                                     "align_genus", 
                                     "align_xgenus", 
                                     "align_trnscpt", 
                                     "within_clone",
                                     "clone_contig",
                                     "map",
                                     "strobe",
                                     "unspecified",
                                     "pcr",
                                     "proximity_ligation"};

    CLinkage_evidence::TLinkage_evidence evidence_vector;
    CLinkage_evidence::GetLinkageEvidence(evidence_vector, initial_strings);

    string output;
    auto success = CLinkage_evidence::VecToString(output, evidence_vector);
    BOOST_CHECK(success); 
    string expected = "paired-ends;align_genus;align_xgenus;"
                      "align_trnscpt;within_clone;clone_contig;"
                      "map;strobe;unspecified;pcr;proximity_ligation";
    BOOST_CHECK_EQUAL(output, expected);
} 


BOOST_AUTO_TEST_CASE(VecToStringUnknown)
{
   vector<CLinkage_evidence::EType> type_vector { 
        CLinkage_evidence::eType_paired_ends,
        CLinkage_evidence::eType_other,
        CLinkage_evidence::eType_unspecified };


    CLinkage_evidence::TLinkage_evidence evidence_vector;
    for (auto linkage_type : type_vector) {
        auto evidence = Ref(new CLinkage_evidence());
        evidence->SetType(linkage_type);
        evidence_vector.push_back(move(evidence));
    }

    string output;
    auto success = CLinkage_evidence::VecToString(output, evidence_vector);
    BOOST_CHECK(!success); 
    string expected = "paired-ends;UNKNOWN;unspecified";
    BOOST_CHECK_EQUAL(output, expected);
} 

