/* $Id $
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
 * File Name:
 *
 * Author: Frank Ludwig
 *
 * File Description:
 *
 */

#ifndef FLATFILE__QUAL_PARSE__HPP
#define FLATFILE__QUAL_PARSE__HPP

#include <objtools/flatfile/flatfile_parse_info.hpp>

#include "qual_validate.hpp"

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
class CQualParser
//  ----------------------------------------------------------------------------
{
public:
    using DATA = vector<string>;

    CQualParser(
        Parser::EFormat fmt,
        const string& featKey,
        const string& featLocation,
        const vector<string>& qualLines);
    virtual ~CQualParser();

    virtual bool GetNextQualifier(
        string& qualKey,
        string& qualVal);

    bool Done() const;

private:
    bool xParseQualifierHead(
        string& qualKey,
        string& qualVal,
        bool& thereIsMore);

    bool xParseQualifierTail(
        const string& qualKey,
        string& qualVal,
        bool& thereIsMore);

    bool xParseQualifierStart(
        bool silent,
        string& qualKey,
        string& qualVal,
        bool& thereIsMore);

    bool xParseQualifierCont(
        const string& qualKey,
        string& qualVal,
        bool& thereIsMore);

    bool xValidateSyntax(               //rule: 
        const string& qualKey,          // check syntax rules in the parser
        const string& qualVal);         // leave semantic rules for the validator

    static bool sIsLegalQual(
        const string& qualKey);
    static bool sHasBalancedQuotes(
        const string& qualVal);
    
    void xQualValAppendLine(
        const string& qualKey,
        const string& line,
        string& qualData);

    Parser::EFormat mFlatFormat;
    const string& mFeatKey;
    const string& mFeatLocation;
    CQualCleanup mCleanerUpper;
    const DATA& mData;
    DATA::const_iterator mCurrent;
    string mPendingKey;
    string mPendingVal;

    string mLastKeyForDataChunk;
    string mLastDataChunkForKey;
    const string::size_type mMaxChunkSize;
};

END_NCBI_SCOPE

#endif //FLATFILE__QUAL_PARSE__HPP
