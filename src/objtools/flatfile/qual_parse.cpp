/*
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
* File Name:  qual_parse.cpp
*
* Author: Frank Ludwig
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqfeat/Seq_feat.hpp>

#include "qual_parse.hpp"
#include "ftaerr.hpp"

# include <objtools/flatfile/flat2err.h>

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
CQualParser::CQualParser(
    const string& featKey,
    const string& featLocation,
    const vector<string>& qualLines): 
//  ----------------------------------------------------------------------------
    mFeatKey(featKey), 
    mFeatLocation(featLocation),
    mCleanerUpper(featKey, featLocation),
    mData(qualLines)
{
    mCurrent = mData.begin();
}


//  ----------------------------------------------------------------------------
CQualParser::~CQualParser()
//  ----------------------------------------------------------------------------
{
}


//  ----------------------------------------------------------------------------
bool CQualParser::GetNextQualifier(
    string& qualKey,
    string& qualVal)
//  ----------------------------------------------------------------------------
{
    qualKey.clear();
    qualVal.clear();
    bool thereIsMore = false;
    while (true) {
        if (mCurrent == mData.end()) {
            return false;
        }
        if (xParseQualifierStart(qualKey, qualVal, thereIsMore)) {
            // report error, throw out, and try again
            break;
        }
        // report error, throw out, and try again
        ++mCurrent;
    }

    while (thereIsMore) {
        if (mCurrent == mData.end()) {
            // report data truncation, fail
            return false;
        }
        if (!xParseQualifierCont(qualVal, thereIsMore)) {
            // error should have been reported at lower level, fail
            return false;
        }
    }
    if (!sHasBalancedQuotes(qualVal)) {
        CQualReport::ReportUnbalancedQuotes(qualKey);
        return false;
    }
    return mCleanerUpper.CleanAndValidate(qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool CQualParser::xParseQualifierStart(
    string& qualKey,
    string& qualVal,
    bool& thereIsMore)
//  ----------------------------------------------------------------------------
{
    auto cleaned = NStr::TruncateSpaces(*mCurrent);
    if (!NStr::StartsWith(cleaned, '/')) {
        return false;
    }
    auto idxEqual = cleaned.find('=', 1);
    qualKey = cleaned.substr(1, idxEqual);
    if (idxEqual != string::npos) {
        qualKey.pop_back();
    }
    if (!sIsLegalQual(qualKey)) {
        //report bad key
        return false;
    }
    ++ mCurrent; // found what we are looking for, flush data

    if (idxEqual == string::npos) {
        qualVal = "";
        return true;
    }
    if (qualKey == "note") {
        cerr << "";
    }
    auto tail = cleaned.substr(idxEqual + 1, string::npos);
    if (tail.empty()) {
        // report missing value
        qualVal = "";
        thereIsMore = false;
        return false;
    }
    if (!NStr::StartsWith(tail, '\"')) {
        // sanity check tail
        qualVal = tail;
        thereIsMore = false;
        return true;
    }
    // established: tail starts with quote
    if (NStr::EndsWith(tail, '\"')) {
        qualVal = tail.substr(1, tail.size() - 2);
        thereIsMore = false;
        return true;
    }
    // partial qualifier value
    qualVal = tail.substr(1, string::npos);
    thereIsMore = true;
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualParser::xParseQualifierCont(
    string& qualVal,
    bool& thereIsMore)
//  ----------------------------------------------------------------------------
{
    string dummy1, dummy2;
    if (xParseQualifierStart(dummy1, dummy2, thereIsMore)) {
        // report error
        --mCurrent;
        return false;
    }
    auto cleaned = NStr::TruncateSpaces(*mCurrent);
    ++mCurrent; //we are going to accept it, so flush it out

    thereIsMore = true;
    if (NStr::EndsWith(cleaned, '\"')) {
        cleaned = cleaned.substr(0, cleaned.size() - 1);
        thereIsMore = false;
    }
    qualVal += ' ';
    qualVal += cleaned;
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualParser::Done() const
//  ----------------------------------------------------------------------------
{
    return (mCurrent == mData.end());
}


//  ----------------------------------------------------------------------------
bool CQualParser::sIsLegalQual(
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    auto type = objects::CSeqFeatData::GetQualifierType(qualKey);
    return (type != objects::CSeqFeatData::eQual_bad);
}


//  ----------------------------------------------------------------------------
bool CQualParser::sHasBalancedQuotes(
    const string& qualVal)
//  ----------------------------------------------------------------------------
{
    int countEscapedQuotes(0);
    auto nextEscapedQuote = qualVal.find("\"\"");
    while (nextEscapedQuote != string::npos) {
        ++countEscapedQuotes;
        nextEscapedQuote = qualVal.find("\"\"", nextEscapedQuote+2);
    }
    int countAnyQuotes(0);
    auto nextAnyQuote = qualVal.find("\"");
    while (nextAnyQuote != string::npos) {
        ++countAnyQuotes;
        nextAnyQuote = qualVal.find("\"", nextAnyQuote+1);
    }
    return (0 == countEscapedQuotes % 4  &&  
        countAnyQuotes == 2*countEscapedQuotes);
}


END_NCBI_SCOPE
