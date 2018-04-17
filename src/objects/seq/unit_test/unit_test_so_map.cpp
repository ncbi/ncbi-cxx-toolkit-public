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
* Author:  Frank Ludwig
*
* File Description:
*   Unit tests for mapping between SO terms and equivalent sefeat constructs.
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objects/seq/so_map.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  -----------------------------------------------------------------------------
//  Utility functions:
//  -----------------------------------------------------------------------------

bool PostProcessFile(
    const string& golden_name,
    const string& out_name,
    const string& keep_name,
    bool keep_diffs)
{
    CFile golden_file(golden_name);
    bool success = golden_file.CompareTextContents(out_name, CFile::eIgnoreWs);
    if (!success  &&  keep_diffs) {
        CFile(out_name).Copy(keep_name);
    }
    CDirEntry(out_name).Remove();
    return success;
}

//  ----------------------------------------------------------------------------
//  Defaults:
//  ----------------------------------------------------------------------------
const string default_golden_feats("data/unit_test_so_map.golden_feats");
const string default_output_feats("data/unit_test_so_map.output_feats");
const string default_golden_so("data/unit_test_so_map.golden_so");
const string default_output_so("data/unit_test_so_map.output_so");

//  ----------------------------------------------------------------------------
//  Data:
//  ----------------------------------------------------------------------------
string golden_feats;
string output_feats;
string golden_so;
string output_so;

bool keep_diffs = false;
bool regenerate = false;

vector<string> vec_so_supported_terms;
vector<CRef<CSeq_feat>> vec_generated_seqfeats;

//  ----------------------------------------------------------------------------
//  Boost test stuff:
//  ----------------------------------------------------------------------------

NCBITEST_INIT_CMDLINE(arg_descrs)
{
    arg_descrs->AddDefaultKey(
        "gsf", 
        "GOLDEN_SEQFEATS_FILE",
        "Override location of golden seqfeats file",
        CArgDescriptions::eString,
        default_golden_feats);
    arg_descrs->AddDefaultKey(
        "osf", 
        "SEQFEATS_OUTPUT_FILE",
        "Override location of seqfeats output file",
        CArgDescriptions::eString,
        default_output_feats);
    arg_descrs->AddDefaultKey(
        "gso", 
        "GOLDEN_SOTERMS_FILE",
        "Override location of golden so-terms file",
        CArgDescriptions::eString,
        default_golden_so);
    arg_descrs->AddDefaultKey(
        "oso", 
        "SEQFEATS_SOTERMS_FILE",
        "Override location of so-terms output file",
        CArgDescriptions::eString,
        default_output_so);
    arg_descrs->AddFlag(
        "R",
        "Retain output on error",
        true);
    arg_descrs->AddFlag(
        "N",
        "Regenerate golden files",
        true);
}

NCBITEST_AUTO_INIT()
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    golden_feats = CDirEntry::CreateAbsolutePath(args["gsf"].AsString());
    output_feats = CDirEntry::CreateAbsolutePath(args["osf"].AsString());
    golden_so = CDirEntry::CreateAbsolutePath(args["gso"].AsString());
    output_so = CDirEntry::CreateAbsolutePath(args["oso"].AsString());
    keep_diffs = args["R"].AsBoolean();
    regenerate = args["N"].AsBoolean();
}

NCBITEST_AUTO_FINI()
{
}

BOOST_AUTO_TEST_CASE(so_supported_terms)
{
    bool success = CSoMap::GetSupportedSoTerms(vec_so_supported_terms);
    vec_so_supported_terms.push_back("unsupported_type");
    BOOST_CHECK(success);
}

BOOST_AUTO_TEST_CASE(map_so_term_to_seqfeat)
{
    std::ofstream ostr;
    string temp_out = CDirEntry::GetTmpName();
    if (regenerate) {
        ostr.open(golden_feats);
    }
    else {
        ostr.open(temp_out);
    }

    CRef<CSeq_id> pId(new CSeq_id("1000"));
    CRef<CSeq_loc> pLocation(new CSeq_loc(*pId, 10, 20));
    for (auto term: vec_so_supported_terms) {
        CRef<CSeq_feat> pFeature(new CSeq_feat);
        pFeature->SetLocation(*pLocation);
        ostr << string(80, '-') << "\n";
        ostr << term << ":\n";
        ostr << string(80, '-') << "\n";
        bool success = CSoMap::SoTypeToFeature(term, *pFeature, true);
        BOOST_CHECK_MESSAGE(success, "CSoMap::SoTypeToFeature failure");
        if (!success) {
            ostr << "ERROR!\n\n";
            continue;
        }
        ostr << MSerial_Format_AsnText() << *pFeature << "\n";
        vec_generated_seqfeats.push_back(pFeature);
    }
    ostr.close();

    if (!regenerate) {
        BOOST_CHECK_MESSAGE(
            PostProcessFile(golden_feats, temp_out, output_feats, keep_diffs), 
            "Post processing diffs.");
    }
}

BOOST_AUTO_TEST_CASE(map_seqfeat_to_so_term)
{
    std::ofstream ostr;
    string temp_out = CDirEntry::GetTmpName();
    if (regenerate) {
        ostr.open(golden_so);
    }
    else {
        ostr.open(temp_out);
    }

    for (auto pFeature: vec_generated_seqfeats) {
        const CSeq_feat& feature = *pFeature;
        string so_type;
        ostr << string(80, '-') << "\n";
        ostr << MSerial_Format_AsnText() << feature;
        ostr << string(80, '-') << "\n";
        bool success = CSoMap::FeatureToSoType(feature, so_type);
        BOOST_CHECK_MESSAGE(success, "CSoMap::FeatureToSoType failure");
        if (!success) {
            ostr << "ERROR!\n\n";
            continue;
        }
        ostr << so_type << "\n\n";
    }
    ostr.close();

    if (!regenerate) {
        BOOST_CHECK_MESSAGE(
            PostProcessFile(golden_so, temp_out, output_so, keep_diffs), 
            "Post processing diffs.");
    }
}

