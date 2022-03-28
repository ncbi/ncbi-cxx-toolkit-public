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

#include "flatparse_report.hpp"
#include "ftaerr.hpp"

# include <objtools/flatfile/flat2err.h>

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
CFlatParseReport::ErrMessageLookup CFlatParseReport::mMessageTemplates = {
//  ----------------------------------------------------------------------------
    { ErrCode(ERR_QUALIFIER_EmbeddedQual),
        "Qualifier /%s contains embedded qualifier /%s. "
        "Feature \"%s\", location \"%s\"." },
    { ErrCode(ERR_QUALIFIER_EmptyQual),
        "Qualifier /%s should have a data value. Qualifier has been dropped. "
        "Feature \"%s\", location \"%s\"." },
    { ErrCode(ERR_QUALIFIER_NoTextAfterEqualSign),
        "Qualifier /%s has not text after the equal sign. Interpreted as empty value."},
    { ErrCode(ERR_QUALIFIER_ShouldNotHaveValue),
        "Qualifier /%s should not have data value. Qualifier value has been dropped. "
        "Feature \"%s\", location \"%s\"." },
    { ErrCode(ERR_QUALIFIER_UnbalancedQuotes),
        "Qualifier /%s value has unbalanced quotes. Qualifier has been dropped."},
    { ErrCode(ERR_FORMAT_UnexpectedData),
        "Encountered unexpected data while looking for qualifier key. Data has been dropped."},
    { ErrCode(ERR_QUALIFIER_UnknownKey),
        "Qualifier key /%s is not recognized. Qualifier has been dropped. "
        "Feature \"%s\", location \"%s\"." },
};


//  ----------------------------------------------------------------------------
void CFlatParseReport::ContainsEmbeddedQualifier(
    const string& featKey,
    const string& featLocation,
    const string& qualKey,
    const string& firstEmbedded,
    bool inNote)
//  ----------------------------------------------------------------------------
{
    ErrPostEx(
        inNote ? SEV_INFO : SEV_WARNING,
        ERR_QUALIFIER_EmbeddedQual,
        sMessageTemplateFor(ERR_QUALIFIER_EmbeddedQual),
        qualKey.c_str(),
        firstEmbedded.c_str(),
        featKey.empty() ? "Unknown" : featKey.c_str(),
        featLocation.empty() ? "Empty" : featLocation.c_str());
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::NoTextAfterEqualSign(
    const string& featKey,
    const string& featLocation,
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    ErrPostEx(
        SEV_INFO,
        ERR_QUALIFIER_NoTextAfterEqualSign,
        sMessageTemplateFor(ERR_QUALIFIER_NoTextAfterEqualSign),
        qualKey.c_str(),
        featKey.empty() ? "Unknown" : featKey.c_str(),
        featLocation.empty() ? "Empty" : featLocation.c_str());
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::UnbalancedQuotes(
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    ErrPostEx(
        SEV_ERROR,
        ERR_QUALIFIER_UnbalancedQuotes,
        sMessageTemplateFor(ERR_QUALIFIER_UnbalancedQuotes),
        qualKey.c_str());
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::QualShouldHaveValue(
    const string& featKey,
    const string& featLocation,
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    ErrPostEx(
        SEV_ERROR,
        ERR_QUALIFIER_EmptyQual,
        sMessageTemplateFor(ERR_QUALIFIER_EmptyQual),
        qualKey.c_str(),
        featKey.empty() ? "Unknown" : featKey.c_str(),
        featLocation.empty() ? "Empty" : featLocation.c_str());
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::UnknownQualifierKey(
    const string& featKey,
    const string& featLocation,
    const string& qualKey)
    //  ----------------------------------------------------------------------------
{
    ErrPostEx(
        SEV_ERROR,
        ERR_QUALIFIER_UnknownKey,
        sMessageTemplateFor(ERR_QUALIFIER_UnknownKey),
        qualKey.c_str(),
        featKey.empty() ? "Unknown" : featKey.c_str(),
        featLocation.empty() ? "Empty" : featLocation.c_str());
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::QualShouldNotHaveValue(
    const string& featKey,
    const string& featLocation,
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    ErrPostEx(
        SEV_WARNING,
        ERR_QUALIFIER_ShouldNotHaveValue,
        sMessageTemplateFor(ERR_QUALIFIER_ShouldNotHaveValue),
        qualKey.c_str(),
        featKey.empty() ? "Unknown" : featKey.c_str(),
        featLocation.empty() ? "Empty" : featLocation.c_str());
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::UnexpectedData(
    const string & featKey,
    const string & featLocation)
//  ----------------------------------------------------------------------------
{
    ErrPostEx(
        SEV_ERROR,
        ERR_FORMAT_UnexpectedData,
        sMessageTemplateFor(ERR_FORMAT_UnexpectedData));
}


//  ----------------------------------------------------------------------------
const char* CFlatParseReport::sMessageTemplateFor(
    int major,
    int minor)
//  ----------------------------------------------------------------------------
{
    auto messageIt = mMessageTemplates.find(ErrCode(major, minor));
    _ASSERT(messageIt != mMessageTemplates.end()); // update message list!
    return messageIt->second;
}

END_NCBI_SCOPE
