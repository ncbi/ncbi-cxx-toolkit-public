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
 * File Name: keyword_parse.cpp
 *
 * Author: Frank Ludwig
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "flatparse_report.hpp"
#include "keyword_parse.hpp"
// #include "ftaerr.hpp"
// #include <objtools/flatfile/flat2err.h>

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
CKeywordParser::CKeywordParser(
    Parser::EFormat format) :
    mFormat(format)
{
    xInitialize();
}

//  ----------------------------------------------------------------------------
CKeywordParser::~CKeywordParser()
{
}

//  ----------------------------------------------------------------------------
void CKeywordParser::xInitialize()
//  ----------------------------------------------------------------------------
{
    mDataFinal = mDataClean = false;
    mPending.clear();
    mKeywords.clear();
}

//  ----------------------------------------------------------------------------
const list<string>
CKeywordParser::KeywordList() const
//  ----------------------------------------------------------------------------
{
    return mKeywords;
}

//  ----------------------------------------------------------------------------
void CKeywordParser::AddDataLine(
    const string& line)
//  ----------------------------------------------------------------------------
{
    if (mDataFinal) {
        // this can actually happen!
        //  the cause would be an input file that contains multiple entries back
        //  to back.
        xInitialize();
    }
    string data(line);
    switch (mFormat) {
    default:
        break;
    case Parser::EFormat::EMBL:
        data = NStr::TruncateSpaces(data.substr(2));
        break;
    }
    if (! mPending.empty() && ! mPending.ends_with(';')) {
        mPending += ' ';
    }
    mPending += data;
    if (mPending.ends_with('.')) {
        xFinalize();
        return;
    }
    if (! mPending.ends_with(';')) {
        mPending += ' ';
        return;
    }
}

//  ----------------------------------------------------------------------------
void CKeywordParser::xFinalize()
//  ----------------------------------------------------------------------------
{
    list<string> words;
    NStr::TrimSuffixInPlace(mPending, ".");
    NStr::Split(mPending, ";", words);
    for (auto word : words) {
        mKeywords.push_back(NStr::TruncateSpaces(word));
    }
    mDataFinal = true;
}

//  ----------------------------------------------------------------------------
void CKeywordParser::Cleanup()
//  ----------------------------------------------------------------------------
{
    if (mDataClean) {
        return;
    }
    if (! mDataFinal) {
        // throw
    }
    if (mFormat == Parser::EFormat::SPROT) {
        xCleanupStripEco();
    }
    xCleanupFixWgsThirdPartyData();
    mDataClean = true;
}

//  ----------------------------------------------------------------------------
void CKeywordParser::xCleanupStripEco()
//  ----------------------------------------------------------------------------
{
    for (auto keyword : mKeywords) {
        auto ecoStart = keyword.find("{ECO:");
        while (ecoStart != string::npos) {
            auto ecoStop = keyword.find("}", ecoStart);
            if (ecoStop == string::npos) {
                continue; // hmmm
            }
            string test1 = keyword.substr(ecoStop);
            while (keyword[ecoStop + 1] == ' ') {
                ++ecoStop;
            }
            keyword  = keyword.substr(0, ecoStart) + keyword.substr(ecoStop + 1);
            ecoStart = keyword.find("{ECO:", ecoStart);
        }
    }
}

//  ----------------------------------------------------------------------------
void CKeywordParser::xCleanupFixWgsThirdPartyData()
//  ----------------------------------------------------------------------------
{
    const string problematic("WGS Third Party Data");

    auto keywordIt = mKeywords.begin();
    while (keywordIt != mKeywords.end()) {
        string& keyword = *keywordIt;
        if (keyword == problematic) {
            keyword = "WGS";
            mKeywords.insert(keywordIt, "Third Party Data");
            ++keywordIt;
        }
        ++keywordIt;
    }
}

END_NCBI_SCOPE
