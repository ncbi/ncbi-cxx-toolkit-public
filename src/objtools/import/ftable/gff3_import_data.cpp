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
 * Author: Frank Ludwig
 *
 * File Description:  Iterate through file names matching a given glob pattern
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/so_map.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>

#include "feat_util.hpp"
#include "gff_util.hpp"
#include <objtools/import/import_error.hpp>
#include "gff3_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CGff3ImportData::CGff3ImportData(
    const CIdResolver& idResolver,
    CImportMessageHandler& errorReporter):
//  ============================================================================
    CFeatImportData(idResolver, errorReporter)
{
}

//  ============================================================================
CGff3ImportData::CGff3ImportData(
    const CGff3ImportData& rhs):
//  ============================================================================
    CFeatImportData(rhs)
{
}

//  ============================================================================
void
CGff3ImportData::Initialize(
    const std::string& seqId,
    const std::string& source,
    const std::string& featureType,
    TSeqPos seqStart,
    TSeqPos seqStop,
    bool scoreIsValid, double score,
    ENa_strand seqStrand,
    const string& phase,
    const vector<pair<string, string>>& attributes)
//  ============================================================================
{

    mpFeat.Reset(new CSeq_feat);
    CSoMap::SoTypeToFeature(featureType, *mpFeat, true);

    CRef<CSeq_id> pId = mIdResolver(seqId);
    mpFeat->SetLocation().SetInt().Assign(
        CSeq_interval(*pId, seqStart, seqStop, seqStrand));
 
    mSource = source;
    if (scoreIsValid) {
        mpScore.reset(new double(score));
    }

    if (mpFeat->GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion) {
        if (phase != ".") {
            mpFeat->SetData().SetCdregion().SetFrame(
                GffUtil::PhaseToFrame(phase));
        }
    }

    xInitializeAttributes(attributes);
}


//  ============================================================================
void
CGff3ImportData::xInitializeAttributes(
    const vector<pair<string, string>>& attributes)
//  ============================================================================
{
    vector<string> alwaysIgnored = {
        "gbkey"
    };

    // typically attributes that have meaning for some feature types only and
    // should not be present for others:
    vector<string> sometimesIgnored = {
        "locus_tag"
    };

    for (auto keyValuePair: attributes) {
        auto key = keyValuePair.first;
        auto value = keyValuePair.second;

        auto itAI = find(alwaysIgnored.begin(), alwaysIgnored.end(), key);
        if (itAI != alwaysIgnored.end()) {
            continue;
        }

        if (key == "ID") {
            mId = value;
            //continue;
        }
        if (key == "Parent") {
            mParent = value;
            //continue;
        }
        if (xInitializeDbxref(key, value)) {
            continue;
        }
        if (xInitializeComment(key, value)) {
            continue;
        }
        if (xInitializeDataGene(key, value)) {
            continue;
        }
        if (xInitializeDataRna(key, value)) {
            continue;
        }
        if (xInitializeDataCds(key, value)) {
            continue;
        }
        if (xInitializeMultiValue(key, value)) {
            continue;
        }

        auto itCI = find(sometimesIgnored.begin(), sometimesIgnored.end(), key);
        if (itCI != sometimesIgnored.end()) {
            continue;
        }
        mpFeat->AddQualifier(key, NStr::URLDecode(value));
    }
}


//  ============================================================================
bool
CGff3ImportData::xInitializeDbxref(
    const std::string& key,
    const std::string& value)
//  ============================================================================
{
    if (key != "Dbxref") {
        return false;
    }
    vector<string> dbxRefs;
    NStr::Split(value, ",", dbxRefs);
    CRef<CDbtag> pDbtag;
    for (auto dbxRef: dbxRefs) {
        pDbtag.Reset(new CDbtag);
        string db, tag;
        NStr::SplitInTwo(dbxRef, ":", pDbtag->SetDb(), pDbtag->SetTag().SetStr());
        mpFeat->SetDbxref().push_back(pDbtag);
    }
    return true;
}


//  ============================================================================
bool
CGff3ImportData::xInitializeComment(
    const std::string& key,
    const std::string& value)
//  ============================================================================
{
    if (key != "Note") {
        return false;
    }
    auto normalizedValue = NStr::URLDecode(value);
    mpFeat->SetComment() = normalizedValue;
    return true;
}


//  ============================================================================
bool
CGff3ImportData::xInitializeDataGene(
    const std::string& key,
    const std::string& value)
//  ============================================================================
{
    auto& data = mpFeat->SetData();
    if (!data.IsGene()) {
        return false;
    }

    auto& geneRef = data.SetGene();
    if (key == "gene") {
        geneRef.SetLocus(value);
        return true;
    }
    if (key == "locus_tag") {
        geneRef.SetLocus_tag(value);
        return true;
    }
    if (key == "gene_synonym") {
        vector<string> synonyms;
        NStr::Split(value, ",", synonyms);
        for (auto synonym: synonyms) {
            geneRef.SetSyn().push_back(synonym);
        }
        return true;
    }
    return false;
}

//  ============================================================================
bool
CGff3ImportData::xInitializeDataCds(
    const std::string& key,
    const std::string& value)
//  ============================================================================
{
    auto& data = mpFeat->SetData();
    if (!data.IsCdregion()) {
        return false;
    }
    auto& cdsRef = data.SetCdregion();

    if (key == "transl_except") {
        vector<string> codeBreaks;
        NStr::Split(value, ",", codeBreaks);
        for (auto codeBreakStr: codeBreaks) {
            CRef<CCode_break> pCodeBreak = FeatUtil::MakeCodeBreak(
                *mpFeat->GetLocation().GetId(), 
                NStr::URLDecode(codeBreakStr));
            if (pCodeBreak) {
                cdsRef.SetCode_break().push_back(pCodeBreak);
            }
        }
        return true;
    }

    if (key == "transl_table") {
        CRef<CGenetic_code::C_E> pCe(new CGenetic_code::C_E);
        pCe->SetId(NStr::StringToInt(value));
        cdsRef.SetCode().Set().push_back(pCe);
        return true;
    }
    return false;
}

//  ============================================================================
bool
CGff3ImportData::xInitializeDataRna(
    const std::string& key,
    const std::string& value)
//  ============================================================================
{
    auto& data = mpFeat->SetData();
    if (!data.IsRna()) {
        return false;
    }
    auto& rnaRef = data.SetRna();

    if (key == "ncrna_class") {
        rnaRef.SetExt().SetGen().SetClass(value);
        mpFeat->AddOrReplaceQualifier("ncRNA_class", value);
        return true;
    }
    return false;
}

//  ============================================================================
bool
CGff3ImportData::xInitializeMultiValue(
    const std::string& key,
    const std::string& value)
//  ============================================================================
{
    vector<string> multiValueAttrs = {
        "ec_number", "function", "go_process", "inference"
    };
    auto attrIt = find(multiValueAttrs.begin(), multiValueAttrs.end(), key);
    if (attrIt == multiValueAttrs.end()) {
        return false;
    }
    vector<string> allValues;
    NStr::Split(value, ",", allValues);
    for (auto singleValue: allValues) {
        mpFeat->AddQualifier(key, NStr::URLDecode(singleValue));
    }
    return true;
}

//  ============================================================================
void
CGff3ImportData::Serialize(
    CNcbiOstream& out)
//  ============================================================================
{
    out << "CGff3ImportData:\n";
    out << "\n";
}


//  ============================================================================
CRef<CSeq_feat>
CGff3ImportData::GetData() const
//  ============================================================================
{
    return mpFeat;
}

