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
 * Authors:  Christiam Camacho
 *
 */

/** @file linkoutdb_unit_test.cpp
 * Unit tests for the CLinkoutDB class */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <objtools/blast/seqdb_reader/linkoutdb.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objects/blastdb/defline_extra.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_SUITE(linkoutdb)

BOOST_AUTO_TEST_CASE(NonExistantLinkoutDB)
{
    BOOST_REQUIRE_THROW(CLinkoutDB foo("bar"), CSeqDBException);
}

BOOST_AUTO_TEST_CASE(TestSeqIds)
{
    CLinkoutDB linkoutdb("data/linkouts1");
    typedef vector< CRef<CSeq_id> > TSeqIdVector;
    TSeqIdVector ids;
    ids.reserve(10);
    ids.push_back(CRef<CSeq_id>(new CSeq_id("lcl|hmm24")));
    ids.push_back(CRef<CSeq_id>(new CSeq_id("lcl|hmm2024")));
    ids.push_back(CRef<CSeq_id>(new CSeq_id("lcl|hmm4024")));
    ids.push_back(CRef<CSeq_id>(new CSeq_id("lcl|hmm6024")));
    ids.push_back(CRef<CSeq_id>(new CSeq_id("lcl|hmm8024")));
    ids.push_back(CRef<CSeq_id>(new CSeq_id("lcl|hmm10024")));
    ids.push_back(CRef<CSeq_id>(new CSeq_id("lcl|hmm12024")));
    ids.push_back(CRef<CSeq_id>(new CSeq_id("lcl|hmm14024")));
    ids.push_back(CRef<CSeq_id>(new CSeq_id("lcl|hmm16024")));
    ids.push_back(CRef<CSeq_id>(new CSeq_id("lcl|hmm18024")));

    ITERATE(TSeqIdVector, id, ids) {
        int l = linkoutdb.GetLinkout(**id);
        BOOST_REQUIRE_EQUAL(l, eGenomicSeq);
    }
}

BOOST_AUTO_TEST_CASE(TestGIs)
{
    CLinkoutDB linkoutdb("data/linkouts2");
    typedef vector< pair<int, int> > TGiLinkoutVector;
    TGiLinkoutVector reference_data;
    //1
    reference_data.push_back(make_pair(1786182,
       eLocuslink|eUnigene|eStructure|eGeo|eGene));
    //2
    reference_data.push_back(make_pair(1786183,
       eLocuslink|eUnigene|eStructure|eGeo|eGene));
    //3
    reference_data.push_back(make_pair(1786184,
       eLocuslink|eUnigene|eGeo|eGene|eBioAssay));
    //4
    reference_data.push_back(make_pair(1786185,
       eLocuslink|eUnigene|eGene|eGenomicSeq|eBioAssay));
    //5
    reference_data.push_back(make_pair(1786186,
       eLocuslink|eUnigene|eGene|eGenomicSeq|eBioAssay));
    //6
    reference_data.push_back(make_pair(1786187,
       eLocuslink|eUnigene|eGene|eAnnotatedInMapviewer|eGenomicSeq|eBioAssay));
    //7
    reference_data.push_back(make_pair(1786188,
       eLocuslink|eUnigene|eGene|eAnnotatedInMapviewer|eGenomicSeq|eBioAssay));
    //8
    reference_data.push_back(make_pair(1786189,
       eLocuslink|eUnigene|eHitInMapviewer|eAnnotatedInMapviewer|
       eGenomicSeq|eBioAssay));
    //9
    reference_data.push_back(make_pair(1786191,
       eLocuslink|eHitInMapviewer|eAnnotatedInMapviewer|
       eGenomicSeq|eBioAssay));
    //10
    reference_data.push_back(make_pair(1786191,
       eLocuslink|eHitInMapviewer|eAnnotatedInMapviewer|
       eGenomicSeq|eBioAssay));

    ITERATE(TGiLinkoutVector, itr, reference_data) {
        int linkout = linkoutdb.GetLinkout(itr->first);
        BOOST_REQUIRE_EQUAL(itr->second, linkout);
    }
}

BOOST_AUTO_TEST_SUITE_END()
#endif /* SKIP_DOXYGEN_PROCESSING */

