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

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CGtfImportData::CGtfImportData(
    const CIdResolver& idResolver):
//  ============================================================================
    mIdResolver(idResolver),
    mpScore(nullptr),
    mpFrame(nullptr)
{
}

//  ============================================================================
CGtfImportData::CGtfImportData(
    const CGtfImportData& rhs):
//  ============================================================================
    mIdResolver(rhs.mIdResolver),
    mSeqId(rhs.mSeqId),
    mSource(rhs.mSource),
    mType(rhs.mType),
    mSeqStart(rhs.mSeqStart),
    mSeqStop(rhs.mSeqStop),
    mpScore(nullptr),
    mSeqStrand(rhs.mSeqStrand),
    mpFrame(nullptr),
    mGeneId(rhs.mGeneId),
    mTranscriptId(rhs.mTranscriptId)
{
}

//  ============================================================================
void
CGtfImportData::InitializeFrom(
    const vector<string>& columns)
    //  ============================================================================
{
    xSetSeqId(columns[0]);
    xSetSource(columns[1]);
    xSetType(columns[2]);
    xSetSeqStart(columns[3]);
    xSetSeqStop(columns[4]);
    xSetScore(columns[5]);
    xSetSeqStrand(columns[6]);
    xSetFrame(columns[7]);
    xSetAttributes(columns[8]);
}

//  ============================================================================
void
CGtfImportData::Serialize(
    CNcbiOstream& out)
//  ============================================================================
{
    auto seqStrand = (mSeqStrand == eNa_strand_minus ? "minus" : "plus");
    auto score = (mpScore ? NStr::DoubleToString(*mpScore) : "(not set)");
    auto frame = (mpFrame ? NStr::IntToString(*mpFrame) : "(not set)");
    out << "CGtfImportData:\n";
    out << "  SeqId = \"" << mSeqId << "\"\n";
    out << "  Source = \"" << mSource << "\"\n";
    out << "  Type = \"" << mType << "\"\n";
    out << "  SeqStart = " << mSeqStart << "\n";
    out << "  SeqStop = " << mSeqStop << "\n";
    out << "  Score = " << score << "\n";
    out << "  SeqStrand = " << seqStrand << "\n";
    out << "  Frame = " << frame << "\n";
    out << "  gene_id = \"" << mGeneId << "\"\n";
    out << "  transcript_id = \"" << mTranscriptId << "\"\n";

    out << "\n";
}

//  ============================================================================
CRef<CSeq_id>
CGtfImportData::SeqIdRef() const
//  ============================================================================
{
    return mIdResolver(SeqId());
}

//  ============================================================================
CRef<CSeq_loc>
CGtfImportData::LocationRef() const
//  ============================================================================
{
    CRef<CSeq_loc> pLocation(new CSeq_loc);
    auto& featInt = pLocation->SetInt();

    featInt.SetId(*SeqIdRef());
    featInt.SetFrom(SeqStart());
    featInt.SetTo(SeqStop());
    featInt.SetStrand(SeqStrand());
    return pLocation;
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
CGtfImportData::xSetSeqId(
    const string& seqId)
//  ============================================================================
{
    mSeqId = seqId;
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
    CFeatureImportError errorIllegalFeatureType(
        CFeatureImportError::ERROR, "Illegal feature type");

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

    if (find(validTypes.begin(), validTypes.end(), normalized) 
            == validTypes.end()) {
        throw errorIllegalFeatureType;
    }
    mType = normalized;
}

//  ============================================================================
void
CGtfImportData::xSetSeqStart(
    const string& seqStart)
//  ============================================================================
{
    CFeatureImportError errorInvalidSeqStartValue(
        CFeatureImportError::ERROR, "Invalid seqStart value");

    try {
        mSeqStart = NStr::StringToInt(seqStart)-1;
    }
    catch(std::exception&) {
        throw errorInvalidSeqStartValue;
    }
}

//  ============================================================================
void
CGtfImportData::xSetSeqStop(
    const string& seqStop)
//  ============================================================================
{
    CFeatureImportError errorInvalidSeqStopValue(
        CFeatureImportError::ERROR, "Invalid seqStop value");

    try {
        mSeqStop = NStr::StringToInt(seqStop)-1;
    }
    catch(std::exception&) {
        throw errorInvalidSeqStopValue;
    }
}

//  ============================================================================
void
CGtfImportData::xSetSeqStrand(
    const string& seqStrand)
//  ============================================================================
{
    CFeatureImportError errorInvalidSeqStrandValue(
        CFeatureImportError::ERROR, "Invalid seqStrand value");

    vector<string> validSettings = {".", "+", "-"};
    if (find(validSettings.begin(), validSettings.end(), seqStrand) ==
            validSettings.end()) {
        throw errorInvalidSeqStrandValue;
    }
    if (seqStrand == "-") {
        mSeqStrand = eNa_strand_minus;
    }
    else {
        mSeqStrand = eNa_strand_plus;
    }
}

//  ============================================================================
void
CGtfImportData::xSetScore(
    const string& score)
//  ============================================================================
{
    CFeatureImportError errorInvalidScoreValue(
        CFeatureImportError::ERROR, "Invalid score value");

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
    CFeatureImportError errorInvalidFrameValue(
        CFeatureImportError::ERROR, "Invalid frame value");

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
    CFeatureImportError errorMissingGeneId(
        CFeatureImportError::ERROR, "Feature misses mandatory gene_id");

    CFeatureImportError errorMissingTranscriptId(
        CFeatureImportError::ERROR, "Feature misses mandatory gene_id");

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
    CFeatureImportError errorAssigningAttrsBeforeFeatureType(
        CFeatureImportError::CRITICAL, 
        "Attempt to assign attributes before feature type");

    CFeatureImportError errorInvalidAttributeFormat(
        CFeatureImportError::ERROR, "Invalid attribute formatting");

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

