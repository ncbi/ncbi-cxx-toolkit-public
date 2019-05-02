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

#include <objtools/import/import_error.hpp>
#include "gff_util.hpp"
#include "gtf_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CGtfImportData::CGtfImportData(
    const CIdResolver& idResolver,
    CImportMessageHandler& errorReporter):
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
    mAttributes.insert(rhs.mAttributes.begin(), rhs.mAttributes.end());
    if (rhs.mpFrame) {
        mpFrame = new CCdregion::TFrame(*rhs.mpFrame);
    }
    if (rhs.mpScore) {
        mpScore = new double(*rhs.mpScore);
    }
}

//  ============================================================================
void
CGtfImportData::Initialize(
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
    CRef<CSeq_id> pId = mIdResolver(seqId);
    mLocation.SetInt().Assign(
        CSeq_interval(*pId, seqStart, seqStop, seqStrand));   
    mSource = source; 
    mType = featureType;
    mpScore = (scoreIsValid ? new double(score) : nullptr);
    mpFrame = nullptr;
    if (phase != ".") { 
        mpFrame = new CCdregion::TFrame(GffUtil::PhaseToFrame(phase)); 
    }
    xInitializeAttributes(attributes);
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
CGtfImportData::xInitializeAttributes(
    const vector<pair<string, string>>& attributes)
//  ============================================================================
{
    CImportError errorMissingGeneId(
        CImportError::ERROR, "Feature misses mandatory gene_id");

    CImportError errorMissingTranscriptId(
        CImportError::ERROR, "Feature misses mandatory transcript_id");

    mAttributes.clear();
    bool foundGeneId(false), foundTranscriptId(false);
    for (auto keyValuePair: attributes) {
        auto key = keyValuePair.first;
        auto value = keyValuePair.second;

        if (key == "gene_id") {
            mGeneId = value;
            foundGeneId = true;
            continue;
        }
        if (key == "transcript_id") {
            mTranscriptId = value;
            foundTranscriptId = true;
            continue;
        }
        auto appendTo = mAttributes.find(key);
        if (appendTo == mAttributes.end()) {
            appendTo = mAttributes.insert(
                pair<string, vector<string> >(key, vector<string>())).first;
        }
        appendTo->second.push_back(value);
    }

    if (!foundGeneId) {
        throw errorMissingGeneId;
    }
    if (mType != "gene"  &&  !foundTranscriptId) {
        throw errorMissingTranscriptId;
    }
}

