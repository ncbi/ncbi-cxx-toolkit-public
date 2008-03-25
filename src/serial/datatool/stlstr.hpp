#ifndef STLSTR_HPP
#define STLSTR_HPP

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
#include <corelib/ncbiutil.hpp>
#include "classctx.hpp"

BEGIN_NCBI_SCOPE

class CNamespace;

class CTemplate1TypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    CTemplate1TypeStrings(const string& templateName,
                          CTypeStrings* type,
                          const string& namespaceName);
    CTemplate1TypeStrings(const string& templateName,
                          AutoPtr<CTypeStrings> type,
                          const string& namespaceName);
    ~CTemplate1TypeStrings(void);

    EKind GetKind(void) const;

    const string& GetTemplateName(void) const
        {
            return m_TemplateName;
        }
    const CTypeStrings* GetArg1Type(void) const
        {
            return m_Arg1Type.get();
        }

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    string GetIsSetCode(const string& var) const;

    void GenerateTypeCode(CClassContext& ctx) const;

protected:
    void AddTemplateInclude(CClassContext::TIncludes& hpp) const;

    virtual string GetRefTemplate(void) const;
    virtual const CNamespace& GetTemplateNamespace(void) const;

private:
    string m_TemplateName;
    AutoPtr<CTypeStrings> m_Arg1Type;
};

class CSetTypeStrings : public CTemplate1TypeStrings
{
    typedef CTemplate1TypeStrings CParent;
public:
    CSetTypeStrings(const string& templateName,
                    AutoPtr<CTypeStrings> type,
                    const string& namespaceName);
    ~CSetTypeStrings(void);

    string GetDestructionCode(const string& expr) const;
    string GetResetCode(const string& var) const;
};

class CListTypeStrings : public CTemplate1TypeStrings
{
    typedef CTemplate1TypeStrings CParent;
public:
    CListTypeStrings(const string& templateName,
                     AutoPtr<CTypeStrings> type,
                     const string& namespaceName,
                     bool externalSet = false);
    ~CListTypeStrings(void);

    string GetDestructionCode(const string& expr) const;
    string GetResetCode(const string& var) const;

protected:
    string GetRefTemplate(void) const;

private:
    bool m_ExternalSet;
};

class CTemplate2TypeStrings : public CTemplate1TypeStrings
{
    typedef CTemplate1TypeStrings CParent;
public:
    CTemplate2TypeStrings(const string& templateName,
                          AutoPtr<CTypeStrings> type1,
                          AutoPtr<CTypeStrings> type2,
                          const string& namespaceName);
    ~CTemplate2TypeStrings(void);

    const CTypeStrings* GetArg2Type(void) const
        {
            return m_Arg2Type.get();
        }

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    void GenerateTypeCode(CClassContext& ctx) const;

private:
    AutoPtr<CTypeStrings> m_Arg2Type;
};

class CMapTypeStrings : public CTemplate2TypeStrings
{
    typedef CTemplate2TypeStrings CParent;
public:
    CMapTypeStrings(const string& templateName,
                    AutoPtr<CTypeStrings> keyType,
                    AutoPtr<CTypeStrings> valueType,
                    const string& namespaceName);
    ~CMapTypeStrings(void);

    string GetDestructionCode(const string& expr) const;
    string GetResetCode(const string& var) const;
};

class CVectorTypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    CVectorTypeStrings(const string& charType,
                       const string& namespaceName,
                       const CComments& comments);
    ~CVectorTypeStrings(void);

    EKind GetKind(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    void GenerateTypeCode(CClassContext& ctx) const;

    string GetResetCode(const string& var) const;

private:
    string m_CharType;
};

END_NCBI_SCOPE

#endif
