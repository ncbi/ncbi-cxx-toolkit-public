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

CFlatParseReport::ErrMessageLookup CFlatParseReport::mMessageTemplates = {
    { ErrCode(ERR_QUALIFIER_UnbalancedQuotes), 
        "Qualifier /%s value has unbalanced quotes. The entire qualifier has been ignored."},
};


void CFlatParseReport::ReportUnbalancedQuotes(
    const string& pQualKey)
{
    CFlatParseReport::ReportProblemParms1(
        SEV_ERROR,
        ERR_QUALIFIER_UnbalancedQuotes,
        pQualKey);
}

void CFlatParseReport::ReportProblemParms1(
    ErrSev severity,
    int majorCode,
    int minorCode,
    const char* pQualKey)
{
    auto pMessageTemplate = mMessageTemplates.find(ErrCode(majorCode, minorCode))->second;
    ErrPostEx(
        severity,
        majorCode, minorCode,
        pMessageTemplate,
        pQualKey);
}

void CFlatParseReport::ReportProblemParms1(
    ErrSev severity,
    int majorCode,
    int minorCode,
    const string& qualKey)
{
    ReportProblemParms1(
        severity,
        majorCode, minorCode,
        qualKey.c_str());
}

END_NCBI_SCOPE
