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
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objtools/import/import_error.hpp>
#include "5col_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
C5ColImportData::C5ColImportData(
    const CIdResolver& idResolver,
    CImportMessageHandler& errorReporter):
//  ============================================================================
    CFeatImportData(idResolver, errorReporter)
{
}

//  ============================================================================
C5ColImportData::C5ColImportData(
    const C5ColImportData& rhs):
//  ============================================================================
    CFeatImportData(rhs)
{
}

//  ============================================================================
void 
C5ColImportData::Initialize(
    const string& seqId,
    const string& featType,
    const vector<pair<int, int>>& vecIntervals,
    const vector<pair<bool, bool>>& vecPartials,
    const vector<pair<string, string>>& vecAttributes)
//  ============================================================================
{
    mpFeature.Reset(new CSeq_feat);

    // ftype
    xFeatureSetType(featType);
    
    // location
    mpFeature->SetLocation().SetNull();
    CRef<CSeq_id> pId = mIdResolver(seqId);
    for (int i=0; i < vecIntervals.size(); ++i) {
        TSeqPos intervalFrom = vecIntervals[i].first;
        TSeqPos intervalTo = vecIntervals[i].second;
        bool partialFrom = vecPartials[i].first;
        bool partialTo = vecPartials[i].second;
        ENa_strand intervalStrand = eNa_strand_plus;
        if (intervalFrom > intervalTo) {
            swap(intervalFrom, intervalTo);
            swap(partialFrom, partialTo);
            intervalStrand = eNa_strand_minus;
        }
        CSeq_loc addition;
        addition.SetInt().Assign(
            CSeq_interval(*pId, intervalFrom, intervalTo, intervalStrand));
        if (mpFeature->GetLocation().IsNull()) {
            mpFeature->SetLocation().Assign(addition);
        }
        else {
            mpFeature->SetLocation().Assign(
                *mpFeature->GetLocation().Add(addition, 0, nullptr));
        }
    }
    // attributes
    for (auto attribute: vecAttributes) {
        mpFeature->AddQualifier(attribute.first, attribute.second);
    }
}

//  ============================================================================
void
C5ColImportData::Serialize(
    CNcbiOstream& out)
//  ============================================================================
{
    auto featSubtype = mpFeature->GetData().GetSubtype();
    auto typeStr = CSeqFeatData::SubtypeValueToName(featSubtype);

    vector<string> vecAttrs;
    for (auto pQual: mpFeature->GetQual()) {
        vecAttrs.push_back(pQual->GetQual() + ":" + pQual->GetVal());
    }

    vector<string> vecLoc;
    const CSeq_loc& loc = mpFeature->GetLocation();
    auto locationStr = NStr::IntToString(loc.GetStart(eExtreme_Positional));
    locationStr += "..";
    locationStr += NStr::IntToString(loc.GetStop(eExtreme_Positional));
    
    out << "C5ColImportData:\n";
    out << "  Type = " << typeStr << "\n";
    out << "  Range = " << locationStr << "\n";
    out << "  Attributes = " << NStr::Join(vecAttrs, ", ") << "\n";
    out << "\n";
}

//  ============================================================================
void
C5ColImportData::xFeatureSetType(
    const string& type_)
//  ============================================================================
{
    CImportError errorBadFeatureType(
        CImportError::ERROR, 
        "Feature type not recognized");

    vector<string> recognizedTypes {
        "gene", "mrna", "cds", "cdregion", "rrna", "trna"
    };

    auto type(type_);
    NStr::ToLower(type);
    if (find(recognizedTypes.begin(), recognizedTypes.end(), type) == 
            recognizedTypes.end()) {
        errorBadFeatureType.AmendMessage(type);
        throw errorBadFeatureType;
    }

    if (type == "gene") {
        mpFeature->SetData().SetGene();
        return;
    }
    if (type == "mrna") {
        mpFeature->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
        return;
    }
    if (type == "rrna") {
        mpFeature->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
        return;
    }
    if (type == "trna") {
        mpFeature->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
        return;
    }
    if (type == "cds"  ||  type == "cdregion") {
        mpFeature->SetData().SetCdregion();
        return;
    }
    return;
}

