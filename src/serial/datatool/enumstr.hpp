#ifndef ENUMSTR_HPP
#define ENUMSTR_HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   C++ class info: includes, used classes, C++ code etc.
*
*/

#include "typestr.hpp"
#include "namespace.hpp"
#include <memory>
#include <list>

BEGIN_NCBI_SCOPE

class CEnumDataTypeValue;

class CEnumTypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    typedef list<CEnumDataTypeValue> TValues;
    CEnumTypeStrings(const string& externalName, const string& enumName,
                     const string& packedType,
                     const string& cType, bool isInteger,
                     const TValues& values, const string& valuesPrefix,
                     const string& namespaceName, const CDataType* dataType,
                     const CComments& comments);

    const string& GetExternalName(void) const
        {
            return m_ExternalName;
        }

    void SetEnumNamespace(const CNamespace& ns);

    EKind GetKind(void) const;
    const string& GetEnumName(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;
    string GetInitializer(void) const;

    void GenerateTypeCode(CClassContext& ctx) const;

private:
    string m_ExternalName;
    string m_EnumName;
    string m_PackedType;
    string m_CType;
    bool m_IsInteger;
    const TValues& m_Values;
    string m_ValuesPrefix;
};

class CEnumRefTypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    CEnumRefTypeStrings(const string& enumName,
                        const string& cName,
                        const CNamespace& ns,
                        const string& fileName,
                        const CComments& comments);

    EKind GetKind(void) const;
    const CNamespace& GetNamespace(void) const;
    const string& GetEnumName(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;
    string GetInitializer(void) const;

    void GenerateTypeCode(CClassContext& ctx) const;

private:
    string m_EnumName;
    string m_CType;
    CNamespace m_Namespace;
    string m_FileName;
};

END_NCBI_SCOPE

#endif
