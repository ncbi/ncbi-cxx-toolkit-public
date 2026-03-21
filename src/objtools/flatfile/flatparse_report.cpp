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
 * File Name: flatparse_report.cpp
 *
 * Author: Frank Ludwig
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/flatfile/flatdefn.h>

#include "flatparse_report.hpp"
#include "ftaerr.hpp"

BEGIN_NCBI_SCOPE


//  ----------------------------------------------------------------------------
void CFlatParseReport::ContainsEmbeddedQualifier(
    const string& featKey,
    const string& featLocation,
    const string& qualKey,
    const string& firstEmbedded,
    bool          inNote)
//  ----------------------------------------------------------------------------
{
    FtaErrPost(
        inNote ? SEV_INFO : SEV_WARNING,
        ERR_QUALIFIER_EmbeddedQual,
        "Qualifier /{} contains embedded qualifier /{}. " 
        "Feature \"{}\", location \"{}\".",
        qualKey,
        firstEmbedded,
        featKey.empty() ? "Unknown" : featKey,
        featLocation.empty() ? "Empty" : featLocation);
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::NoTextAfterEqualSign(
    const string& featKey,
    const string& featLocation,
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    FtaErrPost(
        SEV_INFO,
        ERR_QUALIFIER_NoTextAfterEqualSign,
       "Qualifier /{} has not text after the equal sign. Interpreted as empty value. "
        "Feature \"{}\", location \"{}\".",
        qualKey,
        featKey.empty() ? "Unknown" : featKey,
        featLocation.empty() ? "Empty" : featLocation);
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::UnbalancedQuotes(
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    FtaErrPost(
        SEV_ERROR,
        ERR_QUALIFIER_UnbalancedQuotes,
        "Qualifier /{} value has unbalanced quotes. Qualifier has been dropped.",
        qualKey);
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::QualShouldHaveValue(
    const string& featKey,
    const string& featLocation,
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    FtaErrPost(
        SEV_ERROR,
        ERR_QUALIFIER_EmptyQual,
        "Qualifier /{} should have a data value. Qualifier has been dropped. "
        "Feature \"{}\", location \"{}\"." ,
        qualKey,
        featKey.empty() ? "Unknown" : featKey,
        featLocation.empty() ? "Empty" : featLocation);
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::UnknownQualifierKey(
    const string& featKey,
    const string& featLocation,
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    FtaErrPost(
        SEV_ERROR,
        ERR_QUALIFIER_UnknownQualifier,
        "Qualifier key /{} is not recognized. Qualifier has been dropped. "
        "Feature \"{}\", location \"{}\".",
        qualKey,
        featKey.empty() ? "Unknown" : featKey,
        featLocation.empty() ? "Empty" : featLocation);
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::QualShouldNotHaveValue(
    const string& featKey,
    const string& featLocation,
    const string& qualKey)
//  ----------------------------------------------------------------------------
{
    FtaErrPost(
        SEV_WARNING,
        ERR_QUALIFIER_ShouldNotHaveValue,
        "Qualifier /{} should not have data value. Qualifier value has been dropped. "
        "Feature \"{}\", location \"{}\".",
        qualKey,
        featKey.empty() ? "Unknown" : featKey,
        featLocation.empty() ? "Empty" : featLocation);
}


//  ----------------------------------------------------------------------------
void CFlatParseReport::UnexpectedData(
    const string& featKey,
    const string& featLocation)
//  ----------------------------------------------------------------------------
{
    FtaErrPost(
        SEV_ERROR,
        ERR_FORMAT_UnexpectedData,
       "Encountered unexpected data while looking for qualifier key. "
       "Data has been dropped.");
}


END_NCBI_SCOPE
