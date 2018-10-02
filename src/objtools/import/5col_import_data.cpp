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
#include <objtools/import/feat_import_error.hpp>
#include "5col_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
C5ColImportData::C5ColImportData(
    const CIdResolver& idResolver,
    CFeatMessageHandler& errorReporter):
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
C5ColImportData::InitializeFrom(
    const vector<string>& lines)
//  ============================================================================
{
    vector<string> columns;

    assert(lines.size() >= 2  &&  !lines[0].empty());
    auto seqId = lines[0];
    auto offset = lines[1];
    xFeatureInit(seqId, offset);

    NStr::Split(lines[2], "\t", columns);
    assert(columns.size() == 3);
    xFeatureSetInterval(columns[0], columns[1]);
    xFeatureSetType(columns[2]);
    
    auto i = 3; 
    for (/**/; !NStr::StartsWith(lines[i], "\t"); ++i) {
        columns.clear();
        NStr::Split(lines[i], "\t", columns);
        assert(columns.size() == 2);
        xFeatureAddInterval(columns[0], columns[1]);
    }
    for (/**/; i < lines.size(); ++i) {
        columns.clear();
        NStr::Split(lines[i], "\t", columns);
        assert(columns.size() == 5);
        xFeatureAddAttribute(columns[3], columns[4]);
    }
    //Serialize(cerr);
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
C5ColImportData::xFeatureInit(
    const string& seqId,
    const string& offset)
//  ============================================================================
{
    mpFeature.Reset(new CSeq_feat);
    mOffset = NStr::StringToInt(offset);
    mpId = mIdResolver(seqId);
}

//  ============================================================================
void
C5ColImportData::xFeatureSetType(
    const string& type_)
//  ============================================================================
{
    CFeatImportError errorBadFeatureType(
        CFeatImportError::ERROR, 
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

//  ============================================================================
void
C5ColImportData::xFeatureSetInterval(
    const string& fromStr,
    const string& toStr)
//  ============================================================================
{
    xParseInterval(fromStr, toStr, mpFeature->SetLocation().SetInt());
}

//  ============================================================================
void
C5ColImportData::xFeatureAddInterval(
    const string& toStr,
    const string& fromStr)
//  ============================================================================
{
    CSeq_loc addition;
    xParseInterval(fromStr, toStr, addition.SetInt());
    CRef<CSeq_loc> pUpdatedLocation = mpFeature->GetLocation().Add(
        addition, CSeq_loc::fSortAndMerge_All, nullptr);
    mpFeature->SetLocation().Assign(*pUpdatedLocation);
}
  
//  =============================================================================
void
C5ColImportData::xFeatureAddAttribute(
    const string& key,
    const string& value)
//  =============================================================================
{
    mpFeature->AddQualifier(key, value);
}

//  =============================================================================
void
C5ColImportData::xParseInterval(
    const string& fromStr_,
    const string& toStr_,
    CSeq_interval& parseInt)
//  =============================================================================
{
    CFeatImportError errorBadIntervalBoundaries(
        CFeatImportError::ERROR, 
        "Bad interval boundaries");

    string fromStr(fromStr_);
    string toStr(toStr_);
    bool fromPartial(false);
    bool toPartial(false);
    TSeqPos from = 0;
    TSeqPos to = 0;

    if (fromStr[0] == '>'  ||  fromStr[0] == '<') {
        fromStr = fromStr.substr(1, string::npos);
        fromPartial = true;
    }
    if (toStr[0] == '>'  ||  toStr[0] == '<') {
        toStr = toStr.substr(1, string::npos);
        toPartial = true;
    }

    try {
        from = NStr::StringToInt(fromStr);
        to = NStr::StringToInt(toStr);
    }
    catch (CException&) {
        throw errorBadIntervalBoundaries;
    }

    ENa_strand strand = eNa_strand_plus;
    if (from > to) {
        swap(from , to);
        swap(fromPartial, toPartial);
        strand = eNa_strand_minus;
    }
    parseInt.Assign(CSeq_interval(*mpId, mOffset + from, mOffset + to, strand));
    if (fromPartial) {
        parseInt.SetPartialStart(true, eExtreme_Positional);
    }
    if (toPartial) {
        parseInt.SetPartialStop(true, eExtreme_Positional);
    }
}
