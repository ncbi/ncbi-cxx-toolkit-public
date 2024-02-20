/* $Id$
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
 * File Name: qual_parse.cpp
 *
 * Author: Frank Ludwig
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqfeat/Seq_feat.hpp>

#include "flatparse_report.hpp"
#include "qual_parse.hpp"
#include "ftaerr.hpp"

#include <objtools/flatfile/flat2err.h>

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
CQualParser::CQualParser(
    Parser::EFormat       fmt,
    const string&         featKey,
    const string&         featLocation,
    const vector<string>& qualLines) :
    //  ----------------------------------------------------------------------------
    mFlatFormat(fmt),
    mFeatKey(featKey),
    mFeatLocation(featLocation),
    mCleanerUpper(featKey, featLocation),
    mData(qualLines),
    mMaxChunkSize(fmt == Parser::EFormat::EMBL ? 59 : 58)
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
    // Note: In general, if a qualifier spans multiple lines then it's
    //  surrounded by quotes. The one exception I am aware of is anticodon.
    // If there are more, or the situation gets more complicated in general,
    //  then the code needs to be rewritten to extract values according to
    //  qualifier dependent rules. So key based handler lookup and stuff like
    //  that.
    // Let's hope we don't have to go there.

    // if (mCurrent->find("/protein_id=\"BAA00994") != string::npos) {
    //     cerr << "";
    // }

    qualKey.clear();
    qualVal.clear();
    bool thereIsMore = false;
    if (! xParseQualifierHead(qualKey, qualVal, thereIsMore)) {
        return false;
    }
    if (! xParseQualifierTail(qualKey, qualVal, thereIsMore)) {
        return false;
    }

    if (! xValidateSyntax(qualKey, qualVal)) {
        // error should have been reported at lower level, fail
        return false;
    }
    return mCleanerUpper.CleanAndValidate(qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool CQualParser::xParseQualifierHead(
    string& qualKey,
    string& qualVal,
    bool&   thereIsMore)
//  ----------------------------------------------------------------------------
{
    while (mCurrent != mData.end()) {
        if (xParseQualifierStart(false, qualKey, qualVal, thereIsMore)) {
            return true;
        }
        ++mCurrent;
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool CQualParser::xParseQualifierStart(
    bool    silent,
    string& qualKey,
    string& qualVal,
    bool&   thereIsMore)
//  ----------------------------------------------------------------------------
{
    if (! mPendingKey.empty()) {
        qualKey = mPendingKey, mPendingKey.clear();
        qualVal = mPendingVal, mPendingVal.clear();
        return true;
    }

    auto cleaned = NStr::TruncateSpaces(*mCurrent);
    if (! NStr::StartsWith(cleaned, '/') || NStr::StartsWith(cleaned, "/ ")) {
        if (! silent) {
            CFlatParseReport::UnexpectedData(mFeatKey, mFeatLocation);
        }
        return false;
    }
    auto idxEqual     = cleaned.find('=', 1);
    auto maybeQualKey = cleaned.substr(1, idxEqual);
    if (idxEqual != string::npos) {
        maybeQualKey.pop_back();
    }
    if (! sIsLegalQual(maybeQualKey)) {
        if (! silent) {
            CFlatParseReport::UnknownQualifierKey(mFeatKey, mFeatLocation, maybeQualKey);
        }
        return false;
    }
    qualKey = maybeQualKey;
    ++mCurrent; // found what we are looking for, flush data

    if (idxEqual == string::npos) {
        qualVal = "";
        return true;
    }

    auto tail            = cleaned.substr(idxEqual + 1, string::npos);
    mLastKeyForDataChunk = qualKey,
    mLastDataChunkForKey = cleaned;

    if (tail.empty()) {
        // we can't be harsh here because the legacy flatfile parser regarded
        //  /xxx, /xxx=, and /xxx="" as the same thing.
        CFlatParseReport::NoTextAfterEqualSign(mFeatKey, mFeatLocation, qualKey);
        qualVal     = "";
        thereIsMore = false;
        return true;
    }
    if (! NStr::StartsWith(tail, '\"')) {
        // sanity check tail?
        qualVal     = tail;
        thereIsMore = (qualKey == "anticodon");
        return true;
    }
    // established: tail starts with quote
    if (NStr::EndsWith(tail, '\"')) {
        qualVal = tail.substr(1, tail.size() - 2);
        NStr::TruncateSpacesInPlace(qualVal);
        thereIsMore = false;
        return true;
    }
    // partial qualifier value
    qualVal     = tail.substr(1, string::npos);
    thereIsMore = true;
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualParser::xParseQualifierTail(
    const string& qualKey,
    string&       qualVal,
    bool&         thereIsMore)
//  ----------------------------------------------------------------------------
{
    while (thereIsMore) {
        if (mCurrent == mData.end()) {
            thereIsMore = false;
            if (qualKey != "anticodon") {
                CFlatParseReport::UnbalancedQuotes(qualKey);
                return false;
            }
            return true;
        }
        if (! xParseQualifierCont(qualKey, qualVal, thereIsMore)) {
            if (qualKey != "anticodon") {
                return false;
            }
        }
    }
    NStr::TruncateSpacesInPlace(qualVal);
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualParser::xParseQualifierCont(
    const string& qualKey,
    string&       qualVal,
    bool&         thereIsMore)
//  ----------------------------------------------------------------------------
{
    if (! mPendingKey.empty()) {
        // report error
        return false;
    }
    bool thereShouldBeMore = thereIsMore;
    if (xParseQualifierStart(true, mPendingKey, mPendingVal, thereIsMore)) {
        if (thereShouldBeMore && qualKey != "anticodon") {
            CFlatParseReport::UnbalancedQuotes(qualKey);
        }
        return false;
    }
    auto cleaned = NStr::TruncateSpaces(*mCurrent);
    ++mCurrent; // we are going to accept it, so flush it out

    thereIsMore = true;
    if (NStr::EndsWith(cleaned, '\"')) {
        cleaned     = cleaned.substr(0, cleaned.size() - 1);
        thereIsMore = false;
    }
    xQualValAppendLine(qualKey, cleaned, qualVal);
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualParser::xValidateSyntax(
    const string& qualKey,
    const string& qualVal)
// -----------------------------------------------------------------------------
{
    // sIsLegalQual should be here, but for efficieny it was done before even
    //  extracting the qualifier value.

    if (! sHasBalancedQuotes(qualVal)) {
        CFlatParseReport::UnbalancedQuotes(qualKey);
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
void CQualParser::xQualValAppendLine(
    const string& qualKey,
    const string& line,
    string&       qualData)
//  ----------------------------------------------------------------------------
{
    // Consult notes for RW-1600 for documentation on the below.
    // Or don't --- most of the below does not derive from written specs (no such
    //  thing) but from observing what's happening out in the wild.
    // As a primer: the main problem is to determine whether a line break is on a
    //  word  boundary or not. With lots of run-on chemical compound names
    //  poluting in particular the /note qualifiers that's a non-trivial problem.
    //  It's particularly tricky if a word is too long for a line by just one or
    //  two characters. or just fits into a line and the next token is just one
    //  or two characters.
    //

    // if (qualKey == "note" &&
    //         qualData.find(
    //         "Catalytic activity: GTP cyclohydrolases II convert") != string::npos) {
    //     cerr << ""; // breakpoint
    // }
    //
    string lastDataChunkSeen;
    if (qualKey == mLastKeyForDataChunk) {
        lastDataChunkSeen = mLastDataChunkForKey;
    } else {
        mLastKeyForDataChunk = qualKey;
    }
    mLastDataChunkForKey = line;

    if (qualData.empty() || line.empty()) {
        qualData += line;
        return;
    }
    if (qualKey == "anticodon") { //? no documentation, only observation
        qualData += line;
        return;
    }

    auto sizeAlready = qualData.size();
    if (sizeAlready < 2) {
        qualData += ' ';
        qualData += line;
        return;
    }
    if (qualData[sizeAlready - 1] == '-' && qualData[sizeAlready - 2] != ' ') {
        qualData += line;
        return;
    }
    if (NStr::EndsWith(qualData, "(EC") && '0' <= line[0] && line[0] <= '9') {
        qualData += ' ';
        qualData += line;
        return;
    }
    if (qualData[sizeAlready - 1] == ',' && line[0] == '(') {
        qualData += ' ';
        qualData += line;
        return;
    }

    auto lastSeenSize = lastDataChunkSeen.size();
    auto recentBlank  = lastDataChunkSeen.find(' ');
    if (lastSeenSize == mMaxChunkSize - 1 || lastSeenSize == mMaxChunkSize) {
        if (recentBlank != string::npos) {
            qualData += ' ';
        } else {
            auto pendingBlank = line.find(' ');
            if (lastSeenSize == mMaxChunkSize - 1) {
                if (pendingBlank > 2) {
                    qualData += ' ';
                } else if (pendingBlank == 1) {
                    auto singleCharAllowed = string("+-").find(line[0]);
                    if (singleCharAllowed != string::npos) {
                        qualData += ' ';
                    }
                }
            }
        }
        qualData += line;
        return;
    }
    qualData += ' ';
    qualData += line;
    return;
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
    if (qualKey == "geo_loc_name") {
        return true;
    }
    auto type = objects::CSeqFeatData::GetQualifierType(qualKey);
    return (type != objects::CSeqFeatData::eQual_bad);
}


//  ----------------------------------------------------------------------------
bool CQualParser::sHasBalancedQuotes(
    const string& qualVal)
//  ----------------------------------------------------------------------------
{
    int  countEscapedQuotes(0);
    auto nextEscapedQuote = qualVal.find("\"\"");
    while (nextEscapedQuote != string::npos) {
        ++countEscapedQuotes;
        nextEscapedQuote = qualVal.find("\"\"", nextEscapedQuote + 2);
    }
    int  countAnyQuotes(0);
    auto nextAnyQuote = qualVal.find("\"");
    while (nextAnyQuote != string::npos) {
        ++countAnyQuotes;
        nextAnyQuote = qualVal.find("\"", nextAnyQuote + 1);
    }
    return (0 == countEscapedQuotes % 4 &&
            countAnyQuotes == 2 * countEscapedQuotes);
}


END_NCBI_SCOPE
