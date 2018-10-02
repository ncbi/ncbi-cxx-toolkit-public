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

#include <objtools/import/feat_import_error.hpp>
#include "gtf_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CGtfImportData::CGtfImportData(
    const CIdResolver& idResolver,
    CFeatMessageHandler& errorReporter):
//  ============================================================================
    CFeatImportData(idResolver, errorReporter),
    mpScore(nullptr),
    mpFrame(nullptr)
{
    mLocation.SetNull();
}

//  ============================================================================
CGtfImportData::CGtfImportData(
    const CGtfImportData& rhs):
//  ============================================================================
    CFeatImportData(rhs),
    mSource(rhs.mSource),
    mType(rhs.mType),
    mpScore(nullptr),
    mpFrame(nullptr),
    mGeneId(rhs.mGeneId),
    mTranscriptId(rhs.mTranscriptId)
{
    mLocation.Assign(rhs.mLocation);
}

//  ============================================================================
void
CGtfImportData::InitializeFrom(
    const vector<string>& columns,
    unsigned int originatingLineNumber)
    //  ============================================================================
{
    CFeatImportData::InitializeFrom(columns, originatingLineNumber);

    xSetLocation(columns[0], columns[3], columns[4], columns[6]);
    xSetSource(columns[1]);
    xSetType(columns[2]);
    xSetScore(columns[5]);
    xSetFrame(columns[7]);
    xSetAttributes(columns[8]);
}

//  ============================================================================
void
CGtfImportData::Serialize(
    CNcbiOstream& out)
//  ============================================================================
{
    assert(mLocation.IsInt());
    auto seqId = mLocation.GetInt().GetId().GetSeqIdString();
    auto seqStart = mLocation.GetInt().GetFrom();
    auto seqStop = mLocation.GetInt().GetTo();
    auto seqStrand = (
        mLocation.GetInt().GetStrand() == eNa_strand_minus ? "minus" : "plus");
    auto score = (mpScore ? NStr::DoubleToString(*mpScore) : "(not set)");
    auto frame = (mpFrame ? NStr::IntToString(*mpFrame) : "(not set)");
    out << "CGtfImportData:\n";
    out << "  SeqId = \"" << seqId << "\"\n";
    out << "  Source = \"" << mSource << "\"\n";
    out << "  Type = \"" << mType << "\"\n";
    out << "  SeqStart = " << seqStart << "\n";
    out << "  SeqStop = " << seqStop << "\n";
    out << "  Score = " << score << "\n";
    out << "  SeqStrand = " << seqStrand << "\n";
    out << "  Frame = " << frame << "\n";
    out << "  gene_id = \"" << mGeneId << "\"\n";
    out << "  transcript_id = \"" << mTranscriptId << "\"\n";

    out << "\n";
}

//  ============================================================================
string 
CGtfImportData::AttributeValueOf(
    const string& key) const
//  ============================================================================
{
    auto it = mAttributes.find(key);
    if (it == mAttributes.end()) {
        return "";
    }
    return it->second.front();
}
    
//  ============================================================================
void
CGtfImportData::xSetLocation(
    const string& seqIdAsStr,
    const string& seqStartAsStr,
    const string& seqStopAsStr,
    const string& seqStrandAsStr)
//  ============================================================================
{
    CFeatImportError errorInvalidSeqStartValue(
        CFeatImportError::ERROR, "Invalid seqStart value",
        mOriginatingLineNumber);
    CFeatImportError errorInvalidSeqStopValue(
        CFeatImportError::ERROR, "Invalid seqStop value",
        mOriginatingLineNumber);
    CFeatImportError errorInvalidSeqStrandValue(
        CFeatImportError::ERROR, "Invalid seqStrand value",
        mOriginatingLineNumber);

    TSeqPos seqStart(0), seqStop(0);
    ENa_strand seqStrand(eNa_strand_plus);

    try {
        seqStart = NStr::StringToInt(seqStartAsStr)-1;
    }
    catch(std::exception&) {
        throw errorInvalidSeqStartValue;
    }
    try {
        seqStop = NStr::StringToInt(seqStopAsStr)-1;
    }
    catch(std::exception&) {
        throw errorInvalidSeqStopValue;
    }

    vector<string> strandLegals = {".", "+", "-"};
    if (find(strandLegals.begin(), strandLegals.end(), seqStrandAsStr) ==
            strandLegals.end()) {
        throw errorInvalidSeqStrandValue;
    }
    seqStrand = ((seqStrandAsStr == "-") ? eNa_strand_minus : eNa_strand_plus);

    auto& featInt = mLocation.SetInt();
    featInt.SetId(*mIdResolver(seqIdAsStr));
    featInt.SetFrom(seqStart);
    featInt.SetTo(seqStop);
    featInt.SetStrand(seqStrand);
}

//  ============================================================================
void
CGtfImportData::xSetSource(
    const string& source)
//  ============================================================================
{
    mSource = source;
}

//  ============================================================================
void
CGtfImportData::xSetType(
    const string& type)
//  ============================================================================
{
    CFeatImportError errorIllegalFeatureType(
        CFeatImportError::ERROR, "Illegal feature type",
        mOriginatingLineNumber);

    static const vector<string> validTypes = {
        "cds", "exon", "gene", "initial", "internal", "intron", "mrna", 
        "start_codon", "stop_codon",
    };

    // normalization:
    string normalized(type);
    NStr::ToLower(normalized);
    if (normalized == "transcript") {
        normalized = "mrna";
    }

    if (find(validTypes.begin(), validTypes.end(), normalized) ==
            validTypes.end()) {
        throw errorIllegalFeatureType;
    }
    mType = normalized;
}

//  ============================================================================
void
CGtfImportData::xSetScore(
    const string& score)
//  ============================================================================
{
    CFeatImportError errorInvalidScoreValue(
        CFeatImportError::ERROR, "Invalid score value",
        mOriginatingLineNumber);

    if (score == ".") {
        return;
    }
    if (!mpScore) {
        mpScore = new double;
    }
    try {
        *mpScore = NStr::StringToDouble(score);
    }
    catch(std::exception&) {
        throw errorInvalidScoreValue;
    }
}

//  ============================================================================
void
CGtfImportData::xSetFrame(
    const string& frame)
//  ============================================================================
{
    CFeatImportError errorInvalidFrameValue(
        CFeatImportError::ERROR, "Invalid frame value",
        mOriginatingLineNumber);

    vector<string> validSettings = {".", "0", "1", "2"};
    if (find(validSettings.begin(), validSettings.end(), frame) ==
            validSettings.end()) {
        throw errorInvalidFrameValue;
    }
    if (frame == ".") {
        return;
    }
    if (!mpFrame) {
        mpFrame = new int;
    }
    *mpFrame = NStr::StringToInt(frame);
}

//  ============================================================================
void
CGtfImportData::xSetAttributes(
    const string& attributes)
//  ============================================================================
{
    CFeatImportError errorMissingGeneId(
        CFeatImportError::ERROR, "Feature misses mandatory gene_id",
        mOriginatingLineNumber);

    CFeatImportError errorMissingTranscriptId(
        CFeatImportError::ERROR, "Feature misses mandatory transcript_id",
        mOriginatingLineNumber);

    mAttributes.clear();
    vector<string> splitAttributes;
    xSplitAttributeString(attributes, splitAttributes);
    for (auto splitAttr: splitAttributes) {
        xImportSplitAttribute(splitAttr);
    }
    if (mGeneId.empty()) {
        throw errorMissingGeneId;
    }
    if (mType != "gene"  && mTranscriptId.empty()) {
        throw errorMissingTranscriptId;
    }
}

//  ============================================================================
void
CGtfImportData::xSplitAttributeString(
    const string& attrString,
    vector<string>& splitAttributes)
//  ============================================================================
{
    string strCurrAttrib;
    bool inQuotes = false;

    for (auto curChar: attrString) {
        if (inQuotes) {
            if (curChar == '\"') {
                inQuotes = false;
            }  
            strCurrAttrib += curChar;
        } else { // not in quotes
            if (curChar == ';') {
                NStr::TruncateSpacesInPlace( strCurrAttrib );
                if(!strCurrAttrib.empty())
                    splitAttributes.push_back(strCurrAttrib);
                strCurrAttrib.clear();
            } else {
                if(curChar == '\"') {
                    inQuotes = true;
                }
                strCurrAttrib += curChar;
            }
        }
    }

    NStr::TruncateSpacesInPlace( strCurrAttrib );
    if (!strCurrAttrib.empty()) {
        splitAttributes.push_back(strCurrAttrib);
    }
}

//  ============================================================================
void
CGtfImportData::xImportSplitAttribute(
    const string& splitAttr)
//  ============================================================================
{
    CFeatImportError errorAssigningAttrsBeforeFeatureType(
        CFeatImportError::CRITICAL, 
        "Attempt to assign attributes before feature type",
        mOriginatingLineNumber);
    CFeatImportError errorInvalidAttributeFormat(
        CFeatImportError::ERROR, "Invalid attribute formatting",
        mOriginatingLineNumber);

    if (mType.empty()) {
        throw errorAssigningAttrsBeforeFeatureType;
    }
    string key, value;
    if (!NStr::SplitInTwo(splitAttr, " ", key, value)) {
        //deal with AUGUSTUS convention for gene_ids and transcript_ids
        if (mType == "gene") {
            mGeneId = key;
            return;
        }
        if (mType == "mrna"  ||  mType == "transcript") {
            string geneId, transcriptTail;
            if (NStr::SplitInTwo(key, ".", geneId, transcriptTail)) {
                mGeneId = geneId;
            }
            mTranscriptId = key;
            return;
        }
        throw errorInvalidAttributeFormat;
    }
    NStr::TruncateSpacesInPlace(value);
    if (NStr::StartsWith(value, "\"")  &&  NStr::EndsWith(value, "\"")) {
        value = value.substr(1, value.size() -2);
    }
    if (key == "gene_id") {
        mGeneId = value;
        return;
    }
    if (key == "transcript_id") {
        mTranscriptId = value;
        return;
    }
    auto appendTo = mAttributes.find(key);
    if (appendTo == mAttributes.end()) {
        appendTo = mAttributes.insert(
            pair<string, vector<string> >(key, vector<string>())).first;
    }
    appendTo->second.push_back(value);
}

