/*  $Id$
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
* Author:  Chris Lanczycki
*
* File Description:
*           API for defining and/or accessing pre-defined annotation types
*           and descriptions.
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <algo/structure/cd_utils/cuStdAnnotTypes.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const int CStdAnnotTypes::m_invalidType = -1;
TStandardTypesData CStdAnnotTypes::m_stdAnnotTypeData;


bool CStdAnnotTypes::LoadTypes(const string& iniFile)
{
    static const string ID("id");

    bool result = false;
    int id;
    int accessFlags = (IRegistry::fCaseFlags | IRegistry::fInternalSpaces);
    int readFlags = (IRegistry::fCaseFlags | IRegistry::fInternalSpaces);
    string value;
    list<string> categories, names;

    //  Use CMemoryRegistry simply to leverage registry file format parser.
    CMemoryRegistry dummyRegistry;
    auto_ptr<CNcbiIfstream> iniIn(new CNcbiIfstream(iniFile.c_str(), IOS_BASE::in | IOS_BASE::binary));
    if (*iniIn) {

        result = true;
        ERR_POST(ncbi::Trace << "loading predefined annotation types " << iniFile);

        dummyRegistry.Read(*iniIn, readFlags);
        dummyRegistry.EnumerateSections(&categories, accessFlags);

        //  Loop over all type categories
        ITERATE (list<string>, cit, categories) {
            if (*cit == kEmptyStr) continue;

            //  reject 'id' if it equals m_invalidType; otherwise allow negative values
            id = dummyRegistry.GetInt(*cit, ID, m_invalidType, accessFlags, CMemoryRegistry::eReturn);
            if (id != m_invalidType) {
                TTypeNamesPair& typeNamesPair = m_stdAnnotTypeData[id];
                TTypeNames& values = typeNamesPair.second;
                typeNamesPair.first = *cit;

                //  Get all named fields for this section.
                dummyRegistry.EnumerateEntries(*cit, &names, accessFlags);

                //  Add value for each non-id named field; sort alphabetically on completion.
                ITERATE (list<string>, eit, names) {
                    if (*eit == ID || *eit == kEmptyStr) continue;
                    value = dummyRegistry.GetString(*cit, *eit, kEmptyStr, accessFlags);
                    NStr::TruncateSpacesInPlace(value);
                    if (value != kEmptyStr) {
                        values.push_back(value);
                    }
                }

                sort(values.begin(), values.end());

                cout << "\nTesting sort for " << *cit << endl;
                ITERATE (vector<string>, vit, values) {
                    cout << *vit << endl;
                }

            }
        }
    }

    return result;
}


bool CStdAnnotTypes::IsValidType(int type)
{
    TStandardTypesData::const_iterator cit = m_stdAnnotTypeData.find(type);
    return ((type != m_invalidType) && (cit != m_stdAnnotTypeData.end()));
}

bool CStdAnnotTypes::IsValidTypeStr(const string& typeStr, bool useCase)
{
    bool result = false;
    NStr::ECase caseSensitiveEqual = (useCase) ? NStr::eCase : NStr::eNocase;
    TStandardTypesData::const_iterator satdCit = m_stdAnnotTypeData.begin();
    TStandardTypesData::const_iterator satdEnd = m_stdAnnotTypeData.end();

    for (; (satdCit != satdEnd) && !result; ++satdCit) {
        if (NStr::Equal(satdCit->second.first, typeStr.c_str(), caseSensitiveEqual)) {
            result = true;
        }
    }
    return result;
}

bool CStdAnnotTypes::GetTypeAsString(int type, string& typeStr)
{
    bool result = IsValidType(type);
    if (result) {
        TStandardTypesData::const_iterator cit = m_stdAnnotTypeData.find(type);
        typeStr = cit->second.first;
        if (typeStr.length() == 0) result = false;
    }
    return result;
}

int CStdAnnotTypes::GetTypeAsInt(const string& typeStr, bool useCase)
{
    int result = m_invalidType;
    NStr::ECase caseSensitiveEqual = (useCase) ? NStr::eCase : NStr::eNocase;
    TStandardTypesData::const_iterator satdCit = m_stdAnnotTypeData.begin();
    TStandardTypesData::const_iterator satdEnd = m_stdAnnotTypeData.end();

    for (; (satdCit != satdEnd); ++satdCit) {
        if (NStr::Equal(satdCit->second.first, typeStr.c_str(), caseSensitiveEqual)) {
            result = satdCit->first;
            break;
        }
    }
    return result;
}

bool CStdAnnotTypes::GetTypeNames(int type, TTypeNames& typeNames)
{
    bool result = IsValidType(type);
    if (result) {
        TStandardTypesData::const_iterator cit = m_stdAnnotTypeData.find(type);
        typeNames = cit->second.second;
        if (typeNames.size() == 0) result = false;
    }
    return result;
}

bool CStdAnnotTypes::GetTypeNames(const string& typeStr, TTypeNames& typeNames, bool useCase)
{
    int type = GetTypeAsInt(typeStr, useCase);
    bool result = (type != m_invalidType);
    if (result) {
        TStandardTypesData::const_iterator cit = m_stdAnnotTypeData.find(type);
        typeNames = cit->second.second;
        if (typeNames.size() == 0) result = false;
    }
    return result;
}

bool CStdAnnotTypes::GetTypeNamesPair(int type, TTypeNamesPair& typeNamesPair)
{
    bool result = IsValidType(type);
    if (result) {
        TStandardTypesData::const_iterator cit = m_stdAnnotTypeData.find(type);
        typeNamesPair.first = cit->second.first;
        typeNamesPair.second = cit->second.second;
    }
    return result;
}

unsigned int CStdAnnotTypes::NumPredefinedDescrs(int type)
{
    unsigned int n = 0;
    TStandardTypesData::const_iterator cit = m_stdAnnotTypeData.find(type);
    if ((type != m_invalidType) && (cit != m_stdAnnotTypeData.end())) {
        n = cit->second.second.size();
    }
    return n;
}

bool CStdAnnotTypes::IsPredefinedDescrForType(int type, const string& descr, bool useCase)
{
    bool result = false;
    NStr::ECase caseSensitiveEqual = (useCase) ? NStr::eCase : NStr::eNocase;
    TStandardTypesData::const_iterator cit = m_stdAnnotTypeData.find(type);
    if ((type != m_invalidType) && (cit != m_stdAnnotTypeData.end())) {
        const TTypeNames& names = cit->second.second;
        for (TTypeNames::const_iterator nCit = names.begin(); nCit != names.end(); ++nCit) {
            if (NStr::Equal(nCit->c_str(), descr.c_str(), caseSensitiveEqual)) {
                result = true;
                break;
            }
        }
    }
    return result;
}

bool CStdAnnotTypes::IsPredefinedDescr(const string& descr, int& type, int& typeIndex, bool useCase)
{
    bool result = false;
    int index;
    NStr::ECase caseSensitiveEqual = (useCase) ? NStr::eCase : NStr::eNocase;
    TStandardTypesData::const_iterator satdCit = m_stdAnnotTypeData.begin();
    TStandardTypesData::const_iterator satdEnd = m_stdAnnotTypeData.end();

    type = m_invalidType;
    typeIndex = m_invalidType;
    for (; (satdCit != satdEnd) && !result; ++satdCit) {
        index = 0;
        const TTypeNames& names = satdCit->second.second;
        for (TTypeNames::const_iterator nCit = names.begin(); nCit != names.end(); ++nCit, ++index) {
            if (NStr::Equal(nCit->c_str(), descr.c_str(), caseSensitiveEqual)) {
                result = true;
                type = satdCit->first;
                typeIndex = index;
                break;
            }
        }
    }
    return result;
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE
