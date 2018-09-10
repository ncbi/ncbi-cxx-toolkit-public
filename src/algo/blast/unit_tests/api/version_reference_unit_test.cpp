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
* Author:  Christiam Camacho
*
* File Description:
*   Unit tests for blast::CVersion and blast::CReference classes
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <algo/blast/api/version.hpp>

#include <sstream>

using std::string;
using namespace ncbi;

BOOST_AUTO_TEST_SUITE(version_reference)

BOOST_AUTO_TEST_CASE(testVersion) {
    const int kMajor = 2;
    const int kMinor = 8;
    const int kPatch = 1;
    blast::CBlastVersion v;
    BOOST_REQUIRE_EQUAL(kMajor, v.GetMajor());
    BOOST_REQUIRE_EQUAL(kMinor, v.GetMinor());
    BOOST_REQUIRE_EQUAL(kPatch, v.GetPatchLevel());
    const string kVersionString =
        NStr::IntToString(kMajor) + "." +
        NStr::IntToString(kMinor) + "." +
        NStr::IntToString(kPatch) + "+";
    BOOST_REQUIRE_EQUAL(kVersionString, v.Print());
}

static
void s_DoTestReference(const string& kKeyword, const string& kPubmedId,
                       blast::CReference::EPublication kPublication) {
    using blast::CReference;

    string kReference(CReference::GetString(kPublication));
    BOOST_REQUIRE(kReference.find(kKeyword) != NPOS);

    string kUrl(CReference::GetPubmedUrl(kPublication));
    BOOST_REQUIRE(kUrl.find(kPubmedId) != NPOS);
}

BOOST_AUTO_TEST_CASE(testGappedBlastReference) {
    using blast::CReference;
    const string kKeyword("Gapped BLAST and PSI-BLAST");
    const string kPubmedId("9254694");
    const CReference::EPublication kPub = CReference::eGappedBlast;
    s_DoTestReference(kKeyword, kPubmedId, kPub);
}

BOOST_AUTO_TEST_CASE(testMegaBlastReference) {
    using blast::CReference;
    const string kKeyword("A greedy algorithm for aligning DNA");
    const string kPubmedId("10890397");
    const CReference::EPublication kPub = CReference::eMegaBlast;
    s_DoTestReference(kKeyword, kPubmedId, kPub);
}

BOOST_AUTO_TEST_CASE(testCompositionBasedStatisticsReference) {
    using blast::CReference;
    const string kKeyword("with composition-based statistics");
    const string kPubmedId("11452024");
    const CReference::EPublication kPub = CReference::eCompBasedStats;
    s_DoTestReference(kKeyword, kPubmedId, kPub);
}

BOOST_AUTO_TEST_CASE(testPhiBlastReference) {
    using blast::CReference;
    const string kKeyword("using patterns as seeds");
    const string kPubmedId("9705509");
    const CReference::EPublication kPub = CReference::ePhiBlast;
    s_DoTestReference(kKeyword, kPubmedId, kPub);
}

BOOST_AUTO_TEST_CASE(testInvalidReference) {
    using blast::CReference;

    string kRef(CReference::GetString(CReference::eMaxPublications));
    BOOST_REQUIRE(kRef.empty());

    string kUrl(CReference::GetPubmedUrl(CReference::eMaxPublications));
    BOOST_REQUIRE(kUrl.empty());
}

BOOST_AUTO_TEST_CASE(printAllReferencesHtml) {
    using blast::CReference;

    string text;
    const SIZE_TYPE kMaxNumPublications
        = static_cast<SIZE_TYPE>(CReference::eMaxPublications);

    for (SIZE_TYPE i(0); i < kMaxNumPublications; i++) {
        CReference::EPublication pub = 
            static_cast<CReference::EPublication>(i);
        text += CReference::GetString(pub) + "\n";
        text += "<a href=" + CReference::GetPubmedUrl(pub);
        text += ">Get abstract</a><br>\n";
    }

    const string kToken("www");
    SIZE_TYPE token_occurrences(0);
    for (SIZE_TYPE i(text.find(kToken)); 
         i != NPOS; 
         i = text.find(kToken, i)) {
        token_occurrences++;
        i++;
    }

    BOOST_REQUIRE_EQUAL(kMaxNumPublications, token_occurrences);
    //cout << text;
}

BOOST_AUTO_TEST_SUITE_END()
