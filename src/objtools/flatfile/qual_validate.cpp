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
#include <corelib/ncbistd.hpp>

#include "qual_validate.hpp"
#include "ftaerr.hpp"

# include <objtools/flatfile/flat2err.h>

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
CQualCleanup::CQualCleanup(
    const string& featKey,
    const string& featLocation):
//  ---------------------------------------------------------------------------- 
    mpFeatKey(featKey.c_str()),
    mpFeatLocation(featLocation.c_str())
{
}


//  ----------------------------------------------------------------------------
bool CQualCleanup::CleanAndValidate(
    string& qualKey,
    string& qualVal)
//  ----------------------------------------------------------------------------
{
    using VALIDATOR = bool (CQualCleanup::*)(string&, string&);
    static map<string, VALIDATOR> validators = {
        {"artifial_location", &CQualCleanup::xCleanAndValidateArtificialLocation},
        {"specific_host", &CQualCleanup::xCleanAndValidateSpecificHost},
        {"replace", &CQualCleanup::xCleanAndValidateReplace},
        {"rpt_unit", &CQualCleanup::xCleanAndValidateRptUnit},
        {"translation", &CQualCleanup::xCleanAndValidateTranslation},
    };

    auto validatorIt = validators.find(qualKey);
    if (validatorIt == validators.end()) {
        return CQualCleanup::xCleanAndValidateGeneric(qualKey, qualVal);
    }
    VALIDATOR val = validatorIt->second;
    return (*this.*val)(qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool CQualCleanup::xCleanAndValidateGeneric(
    string& qualKey,
    string& qualVal)
//  ----------------------------------------------------------------------------
{
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualCleanup::xCleanAndValidateArtificialLocation(
    string& qualKey,
    string& qualVal)
//  ----------------------------------------------------------------------------
{
return true; // not sure of the exact rules yet
    if (!qualVal.empty()) {
        ErrPostEx(SEV_WARNING, ERR_QUALIFIER_ShouldNotHaveValue,
            "Qualifier /%s should not have data value. Qualifier value has been ignored. "
            "Feature \"%s\", location \"%s\".",
            qualKey.c_str(), (mpFeatKey) ? "Unknown" : mpFeatKey,
            (mpFeatLocation) ? "Empty" : mpFeatLocation);
        qualVal.clear(); 
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualCleanup::xCleanAndValidateConsSplice(
    string& qualKey,
    string& qualVal)
//  ----------------------------------------------------------------------------
{
    return xCleanFollowCommasWithBlanks(qualVal)  &&
        xCleanAndValidateGeneric(qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool CQualCleanup::xCleanAndValidateSpecificHost(
    string& qualKey,
    string& qualVal)
//  ----------------------------------------------------------------------------
{
    if (qualKey == "specific_host") {
        qualKey = "host";
    }
    return xCleanAndValidateGeneric(qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool CQualCleanup::xCleanAndValidateReplace(
    string& qualKey,
    string& qualVal)
//  ----------------------------------------------------------------------------
{
    return xCleanStripBlanks(qualVal) &&
        xCleanAndValidateGeneric(qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool CQualCleanup::xCleanAndValidateRptUnit(
    string& qualKey,
    string& qualVal)
//  ----------------------------------------------------------------------------
{
    return xCleanToLower(qualVal) &&
        xCleanAndValidateGeneric(qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool CQualCleanup::xCleanAndValidateTranslation(
    string& qualKey,
    string& qualVal)
//  ----------------------------------------------------------------------------
{
    return xCleanStripBlanks(qualVal) &&
        xCleanAndValidateGeneric(qualKey, qualVal);
}


//  ----------------------------------------------------------------------------
bool CQualCleanup::xCleanStripBlanks(
    string& val)
//  ----------------------------------------------------------------------------
{
    NStr::ReplaceInPlace(val, " ", "");
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualCleanup::xCleanToLower(
    string& val)
//  ----------------------------------------------------------------------------
{
    NStr::ToLower(val);
    return true;
}


//  ----------------------------------------------------------------------------
bool CQualCleanup::xCleanFollowCommasWithBlanks(
    string& val)
//  ----------------------------------------------------------------------------
{
    auto origSize = val.size();
    string newVal(1, val[0]);
    newVal.reserve(2*origSize);
    for (auto i=1; i < origSize-1; ++i) {
        newVal += val[i];
        if (val[i] == ','  &&  val[i+1] != ' ') {
            newVal += ' ';
        }
    }
    if (newVal.size() > origSize) {
        val = newVal;
    }
    return true;
}


END_NCBI_SCOPE
