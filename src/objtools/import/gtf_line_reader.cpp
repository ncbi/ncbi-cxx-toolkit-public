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

#include <objtools/import/feat_import_error.hpp>
#include "gtf_line_reader.hpp"
#include "gtf_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CGtfLineReader::CGtfLineReader(
    CFeatMessageHandler& errorReporter):
//  ============================================================================
    CFeatLineReader(errorReporter),
    mColumnDelimiter(""),
    mSplitFlags(0)
{
}

//  ============================================================================
bool
CGtfLineReader::GetNextRecord(
    CFeatImportData& record)
//  ============================================================================
{
    xReportProgress();

    string nextLine = "";
    while (!mpLineReader->AtEOF()) {
        nextLine = *(++(*mpLineReader));
        if (xIgnoreLine(nextLine)) {
            continue;
        }
        vector<string> columns;
        xSplitLine(nextLine, columns);
        xInitializeRecord(columns, record);
        ++mRecordNumber;
        return true;
    }
    return false;
}

//  ============================================================================
void
CGtfLineReader::xSplitLine(
    const string& line,
    vector<string>& columns)
//  ============================================================================
{
    CFeatImportError errorInvalidColumnCount(
        CFeatImportError::CRITICAL, "Invalid column count");

    columns.clear();
    if (mColumnDelimiter.empty()) {
        mColumnDelimiter = "\t";
        mSplitFlags = 0;
        NStr::Split(line, mColumnDelimiter, columns, mSplitFlags);
        if (columns.size() == 9) {
            return;
        }
        columns.clear();
        mColumnDelimiter = " \t";
        mSplitFlags = NStr::fSplit_MergeDelimiters;
    }
    NStr::Split(line, mColumnDelimiter, columns, mSplitFlags);
    if (columns.size() == 9) {
        return;
    }
    if (mColumnDelimiter == " \t"  &&  columns.size() > 9) {
        vector<string> attributes(columns.begin() + 8, columns.end());
        columns[8] = NStr::Join(attributes, " ");
        columns.erase(columns.begin() + 9, columns.end());
        return;
    }
    throw errorInvalidColumnCount;
}

//  ============================================================================
void
CGtfLineReader::xInitializeRecord(
    const vector<string>& columns,
    CFeatImportData& record_)
//  ============================================================================
{
    assert(dynamic_cast<CGtfImportData*>(&record_));
    CGtfImportData& record = static_cast<CGtfImportData&>(record_);
    //record.InitializeFrom(columns, mLineNumber);

    string seqId;
    TSeqPos seqStart, seqStop;
    ENa_strand seqStrand;
    xInitializeLocation(columns, seqId, seqStart, seqStop, seqStrand);

    string source;
    xInitializeSource(columns, source);

    string featType;
    xInitializeType(columns, featType);

    bool scoreIsValid;
    double score;
    xInitializeScore(columns, scoreIsValid, score);

    bool frameIsValid;
    int frame;
    xInitializeFrame(columns, frameIsValid, frame);

    vector<pair<string, string>> attributes;
    xInitializeAttributes(columns, attributes);

    record.Initialize(seqId, source, featType, seqStart, seqStop, 
        scoreIsValid, score, seqStrand, frameIsValid, frame, attributes);

}

//  ============================================================================
void
CGtfLineReader::xInitializeAttributes(
    const vector<string>& columns,
    vector<pair<string, string>>& attributes)
//  ============================================================================
{
    CFeatImportError errorInvalidAttributeFormat(
        CFeatImportError::ERROR, "Invalid attribute formatting", 
        LineCount());

    string attributesStr = columns[8];
    string featType = columns[2];
    NStr::ToLower(featType);

    vector<string> attributesVec;
    xSplitAttributeStringBySemicolons(attributesStr, attributesVec);

    attributes.clear();
    for (auto singleAttr: attributesVec) {
        string key, value;
        if (!NStr::SplitInTwo(singleAttr, " ", key, value)) {

            //deal with AUGUSTUS convention for gene_ids and transcript_ids
            if (featType == "gene") {
                attributes.push_back(
                    pair<string, string>("gene_id", singleAttr));
                continue;
            }
            if (featType == "mrna"  ||  featType == "transcript") {
                string geneId, transcriptId;
                if (NStr::SplitInTwo(key, ".", geneId, transcriptId)) {
                    attributes.push_back(
                        pair<string, string>("gene_id", geneId));
                }
                attributes.push_back(
                    pair<string, string>("transcript_id", singleAttr));
                continue;
            }
            throw errorInvalidAttributeFormat;
        }
        NStr::TruncateSpacesInPlace(value);
        if (NStr::StartsWith(value, "\"")  &&  NStr::EndsWith(value, "\"")) {
            value = value.substr(1, value.size() -2);
        }
        attributes.push_back(pair<string, string>(key, value));
    }
}

//  ============================================================================
void
CGtfLineReader::xInitializeFrame(
    const vector<string>& columns,
    bool& frameIsValid,
    int& frame)
//  ============================================================================
{
    CFeatImportError errorInvalidFrameValue(
        CFeatImportError::ERROR, "Invalid frame value", LineCount());

    vector<string> validSettings = {".", "0", "1", "2"};
    if (find(validSettings.begin(), validSettings.end(), columns[7]) ==
        validSettings.end()) {
        throw errorInvalidFrameValue;
    }
    if (columns[7] == ".") {
        frameIsValid = false;
        return;
    }
    frame = NStr::StringToInt(columns[7]);
    frameIsValid = true;
}

//  ============================================================================
void
CGtfLineReader::xInitializeScore(
    const vector<string>& columns,
    bool& scoreIsValid,
    double& score)
//  ============================================================================
{
    CFeatImportError errorInvalidScoreValue(
        CFeatImportError::ERROR, "Invalid score value",
        LineCount());

    if (columns[5] == ".") {
        scoreIsValid = false;
        return;
    }

    try {
        score = NStr::StringToDouble(columns[5]);
    }
    catch(std::exception&) {
        throw errorInvalidScoreValue;
    }
    scoreIsValid = true;
}

//  ============================================================================
void
CGtfLineReader::xInitializeType(
    const vector<string>& columns,
    string& featType)
//  ============================================================================
{
    CFeatImportError errorIllegalFeatureType(
        CFeatImportError::ERROR, "Illegal feature type", LineCount());

    static const vector<string> validTypes = {
        "cds", "exon", "gene", "initial", "internal", "intron", "mrna", 
        "start_codon", "stop_codon",
    };

    // normalization:
    string normalized(columns[2]);
    NStr::ToLower(normalized);
    if (normalized == "transcript") {
        normalized = "mrna";
    }

    if (find(validTypes.begin(), validTypes.end(), normalized) ==
            validTypes.end()) {
        throw errorIllegalFeatureType;
    }
    featType = normalized;
}

//  ============================================================================
void
CGtfLineReader::xInitializeSource(
    const vector<string>& columns,
    string& source)
    //  ============================================================================
{
    source = columns[1];
}

//  ============================================================================
void
CGtfLineReader::xInitializeLocation(
    const vector<string>& columns,
    string& seqId,
    TSeqPos& seqStart,
    TSeqPos& seqStop,
    ENa_strand& seqStrand)
//  ============================================================================
{
    CFeatImportError errorInvalidSeqStartValue(
        CFeatImportError::ERROR, "Invalid seqStart value",
        LineCount());
    CFeatImportError errorInvalidSeqStopValue(
        CFeatImportError::ERROR, "Invalid seqStop value",
        LineCount());
    CFeatImportError errorInvalidSeqStrandValue(
        CFeatImportError::ERROR, "Invalid seqStrand value",
        LineCount());

    seqId = columns[0];

    try {
        seqStart = NStr::StringToInt(columns[3])-1;
    }
    catch(std::exception&) {
        throw errorInvalidSeqStartValue;
    }
    try {
        seqStop = NStr::StringToInt(columns[4])-1;
    }
    catch(std::exception&) {
        throw errorInvalidSeqStopValue;
    }

    vector<string> strandLegals = {".", "+", "-"};
    if (find(strandLegals.begin(), strandLegals.end(), columns[6]) ==
            strandLegals.end()) {
        throw errorInvalidSeqStrandValue;
    }
    seqStrand = ((columns[6] == "-") ? eNa_strand_minus : eNa_strand_plus);
}


//  ============================================================================
void
CGtfLineReader::xSplitAttributeStringBySemicolons(
    const std::string& attrStr,
    std::vector<std::string>& attrVec)
//  ============================================================================
{
    string strCurrAttrib;
    bool inQuotes = false;

    for (auto curChar: attrStr) {
        if (inQuotes) {
            if (curChar == '\"') {
                inQuotes = false;
            }  
            strCurrAttrib += curChar;
        } else { // not in quotes
            if (curChar == ';') {
                NStr::TruncateSpacesInPlace( strCurrAttrib );
                if(!strCurrAttrib.empty())
                    attrVec.push_back(strCurrAttrib);
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
        attrVec.push_back(strCurrAttrib);
    }
}

