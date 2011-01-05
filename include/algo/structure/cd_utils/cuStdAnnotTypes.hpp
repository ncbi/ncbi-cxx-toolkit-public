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
* Authors:  Chris Lanczycki
*
* File Description:
*           API for defining and/or accessing pre-defined annotation types
*           and descriptions.
*
* ===========================================================================
*/

#ifndef CU_STDANNOTTYPES__HPP
#define CU_STDANNOTTYPES__HPP 

#include <map>
#include <vector>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

typedef vector<string> TTypeNames;
typedef pair<string, TTypeNames > TTypeNamesPair;
typedef map<int, TTypeNamesPair> TStandardTypesData;


class NCBI_CDUTILS_EXPORT CStdAnnotTypes
{

public:

    static const int m_invalidType;

    CStdAnnotTypes() {};

    //  Load type definition and type -> predefined names from a file
    //  that conforms to format of a .ini file (see CMemoryRegistry class
    //  and Toolkit documentation pertaining to registry file format).
    //  Return true if 'iniFile' was successfully parsed; return false on error.
    static bool LoadTypes(const string& iniFile);

    //  Remove all previously loaded type mappings.
    static void Reset() { m_stdAnnotTypeData.clear(); }


    //  Return true if 'type' is a key in m_stdAnnotTypeData.
    static bool IsValidType(int type);

    //  Return true if 'typeStr' corresponds to a key in m_stdAnnotTypeData.
    //  By default, 'typeStr' is searched for in a case-sensivitive fashion.
    static bool IsValidTypeStr(const string& typeStr, bool useCase = true);

    //  Return true if 'type' is a valid key and a non-empty typeStr exists.
    static bool GetTypeAsString(int type, string& typeStr);

    //  Return corresponding integral type for a valid 'typeStr', or m_invalidType if it is invalid.
    //  By default, 'typeStr' is searched for in a case-sensivitive fashion.
    static int GetTypeAsInt(const string& typeStr, bool useCase = true);

    //  Return true if 'type' is a valid key and at least one name exists.
    static bool GetTypeNames(int type, TTypeNames& typeNames);

    //  Return true if 'typeStr' is recognized and at least one name exists.
    //  'useCase' defines whether 'typeStr' is searched for case-sensitively.
    static bool GetTypeNames(const string& typeStr, TTypeNames& typeNames, bool useCase = true);

    //  Return true if 'type' is a valid key and typeNamesPair can be populated.
    static bool GetTypeNamesPair(int type, TTypeNamesPair& typeNamesPair);

    //  Return the number of predefined names for 'type' (0 if 'type' is not valid).
    static unsigned int NumPredefinedDescrs(int type);

    //  Return true only when 'descr' is among the predefined names for 'type'.
    //  By default, 'descr' is searched for in a case-sensivitive fashion.
    static bool IsPredefinedDescrForType(int type, const string& descr, bool useCase = true);

    //  Return true if 'descr' is a predefined name for some type; return false otherwise.
    //  When found, 'type' is set to be the type code, and 'typeIndex' is its position
    //  in the corresponding vector of names.  When not found, 'type' and 'typeIndex' are
    //  set to m_invalidType.
    //  By default, 'descr' is searched for in a case-sensivitive fashion.
    static bool IsPredefinedDescr(const string& descr, int& type, int& typeIndex, bool useCase = true);

private:

    //  Never allow a key = m_invalidType.
    //  Note: each TTypeNames vector is alphabetically sorted.
    static TStandardTypesData m_stdAnnotTypeData;

};


END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif // CU_STDANNOTTYPES__HPP
