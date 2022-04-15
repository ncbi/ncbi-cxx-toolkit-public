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
//#include "ftaerr.hpp"
//# include <objtools/flatfile/flat2err.h>

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
CKeywordParser::CKeywordParser(
    Parser::EFormat format) :
//  ----------------------------------------------------------------------------
    mFormat(format),
    mDataDone(false),
    mDataClean(false)
{};

//  ----------------------------------------------------------------------------
CKeywordParser::~CKeywordParser()
//  ----------------------------------------------------------------------------
{};

//  ----------------------------------------------------------------------------
const list<string> 
CKeywordParser::KeywordList() const
//  ----------------------------------------------------------------------------
{
    return mKeywords;
}

//  ----------------------------------------------------------------------------
void 
CKeywordParser::AddDataLine(
    const string& line)
//  ----------------------------------------------------------------------------
{
    if (mDataDone) {
        // throw
    }
    string data(line);
    switch (mFormat) {
    default:
        break;
    case Parser::EFormat::EMBL:
        data = NStr::TruncateSpaces(data.substr(2));
        break;
    }
    if (!mPending.empty() && !NStr::EndsWith(mPending, ";")) {
        mPending += ' ';
    }
    mPending += data;
    if (NStr::EndsWith(mPending, '.')) {
        xFinalize();
        return;
    }
    if (!NStr::EndsWith(mPending, ";")) {
        mPending += ' ';
        return;
    }
}

//  ----------------------------------------------------------------------------
void 
CKeywordParser::xFinalize()
//  ----------------------------------------------------------------------------
{
    list<string> words;
    NStr::TrimSuffixInPlace(mPending, ".");
    NStr::Split(mPending, ";", words);
    for (auto word : words) {
        mKeywords.push_back(NStr::TruncateSpaces(word));
    }
    mDataDone = true;
}

//  ----------------------------------------------------------------------------
void 
CKeywordParser::Cleanup()
//  ----------------------------------------------------------------------------
{
}

END_NCBI_SCOPE
