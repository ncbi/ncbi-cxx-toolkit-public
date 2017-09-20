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

    virtual EKind GetKind(void) const override;
    virtual const string& GetEnumName(void) const override;

    virtual string GetCType(const CNamespace& ns) const override;
    virtual string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const override;
    virtual string GetRef(const CNamespace& ns) const override;
    virtual string GetInitializer(void) const override;

    virtual void GenerateTypeCode(CClassContext& ctx) const override;

    void SetBitset(bool bitset) {
        m_IsBitset = bitset;
    }
    bool IsBitset(void) const {
        return m_IsBitset;
    }
private:
    string m_ExternalName;
    string m_EnumName;
    string m_PackedType;
    string m_CType;
    bool m_IsInteger;
    bool m_IsBitset;
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

    virtual EKind GetKind(void) const override;
    virtual const CNamespace& GetNamespace(void) const override;
    virtual const string& GetEnumName(void) const override;

    virtual string GetCType(const CNamespace& ns) const override;
    virtual string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const override;
    virtual string GetRef(const CNamespace& ns) const override;
    virtual string GetInitializer(void) const override;

    virtual void GenerateTypeCode(CClassContext& ctx) const override;

private:
    string m_EnumName;
    string m_CType;
    CNamespace m_Namespace;
    string m_FileName;
};

END_NCBI_SCOPE

#endif
