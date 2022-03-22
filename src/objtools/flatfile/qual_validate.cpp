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
* File Name:  qual_validate.cpp
*
* Author: Frank Ludwig
*
*/
#include <ncbi_pch.hpp>

#include "qual_validate.hpp"
#include "ftaerr.hpp"

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
bool CQualCleanup::CleanAndValidate(
    const string& featKey,
    string& qualKey,
    string& qualVal)
    //  ----------------------------------------------------------------------------
{
    using VALIDATOR = bool (*)(const string&, string&, string&);
    static map<string, VALIDATOR> validators = {
        {"specific_host", &CQualCleanup::xCleanAndValidateSpecificHost},
        {"translation", &CQualCleanup::xCleanAndValidateTranslation},
    };

    auto validatorIt = validators.find(qualKey);
    if (validatorIt == validators.end()) {
        return CQualCleanup::xCleanAndValidateGeneric(featKey, qualKey, qualVal);
    }
    return (validatorIt->second)(featKey, qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool ::CQualCleanup::xCleanAndValidateGeneric(
    const string& featKey,
    string& qualKey,
    string& qualVal)
//  ----------------------------------------------------------------------------
{
    return true;
}


//  ----------------------------------------------------------------------------
bool ::CQualCleanup::xCleanAndValidateSpecificHost(
    const string& featKey,
    string& qualKey,
    string& qualVal)
    //  ----------------------------------------------------------------------------
{
    if (qualKey == "specific_host") {
        qualKey = "host";
    }
    return xCleanAndValidateGeneric(featKey, qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool ::CQualCleanup::xCleanAndValidateTranslation(
    const string& featKey,
    string& qualKey,
    string& qualVal)
    //  ----------------------------------------------------------------------------
{
    return CQualCleanup::xCleanStripBlanks(featKey, qualKey, qualVal) &&
        xCleanAndValidateGeneric(featKey, qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool ::CQualCleanup::xCleanStripBlanks(
    const string& featKey,
    string& qualKey,
    string& qualVal)
    //  ----------------------------------------------------------------------------
{
    NStr::ReplaceInPlace(qualVal, " ", "");
    return true;
}


END_NCBI_SCOPE
