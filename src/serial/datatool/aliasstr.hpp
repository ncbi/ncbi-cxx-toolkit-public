#ifndef ALIASSTR_HPP
#define ALIASSTR_HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   C++ aliased type info: includes, used classes, C++ code etc.
*
*/

#include "typestr.hpp"
#include <corelib/ncbiutil.hpp>
#include "namespace.hpp"

BEGIN_NCBI_SCOPE

class CAliasTypeStrings : public CTypeStrings
{
    typedef CTypeStrings TParent;
public:
    CAliasTypeStrings(const string& externalName,
                      const string& className,
                      CTypeStrings& ref_type,
                      const CComments& comments);
    ~CAliasTypeStrings(void);

    virtual EKind GetKind(void) const override;

    virtual string GetCType(const CNamespace& ns) const override;
    virtual string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const override;
    virtual bool HaveSpecialRef(void) const override;
    virtual string GetRef(const CNamespace& ns) const override;

    virtual bool CanBeKey(void) const override;
    virtual bool CanBeCopied(void) const override;
    // bool NeedSetFlag(void) const;

    virtual string NewInstance(const string& init, const string& place) const override;

    virtual string GetInitializer(void) const override;
    virtual string GetDestructionCode(const string& expr) const override;
    virtual string GetIsSetCode(const string& var) const override;
    virtual string GetResetCode(const string& var) const override;
    virtual string GetDefaultCode(const string& var) const override;

    virtual void GenerateCode(CClassContext& ctx) const override;
    virtual void GenerateUserHPPCode(CNcbiOstream& out) const override;
    virtual void GenerateUserCPPCode(CNcbiOstream& out) const override;

    virtual void GenerateTypeCode(CClassContext& ctx) const override;
    virtual void GeneratePointerTypeCode(CClassContext& ctx) const override;

    string GetClassName(void) const;
    string GetExternalName(void) const;
    
    void SetFullAlias(bool set = true) {
        m_FullAlias = set;
    }
    bool IsFullAlias(void) const {
        return m_FullAlias;
    }
private:
    string m_ExternalName;
    string m_ClassName;
    AutoPtr<CTypeStrings> m_RefType;
    bool m_FullAlias;
    mutable bool m_Nested;
};


class CAliasRefTypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    CAliasRefTypeStrings(const string& className,
                         const CNamespace& ns,
                         const string& fileName,
                         CTypeStrings& ref_type,
                         const CComments& comments);

    virtual EKind GetKind(void) const override;

    virtual string GetCType(const CNamespace& ns) const override;
    virtual string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const override;
    virtual bool HaveSpecialRef(void) const override;
    virtual string GetRef(const CNamespace& ns) const override;

    virtual bool CanBeKey(void) const override;
    virtual bool CanBeCopied(void) const override;
    // bool NeedSetFlag(void) const;

    virtual const CNamespace& GetNamespace(void) const override;

    virtual string GetInitializer(void) const override;
    virtual string GetDestructionCode(const string& expr) const override;
    virtual string GetIsSetCode(const string& var) const override;
    virtual string GetResetCode(const string& var) const override;
    virtual string GetDefaultCode(const string& var) const override;

    virtual void GenerateCode(CClassContext& ctx) const override;
    virtual void GenerateUserHPPCode(CNcbiOstream& out) const override;
    virtual void GenerateUserCPPCode(CNcbiOstream& out) const override;

    virtual void GenerateTypeCode(CClassContext& ctx) const override;
    virtual void GeneratePointerTypeCode(CClassContext& ctx) const override;

private:
    string m_ClassName;
    CNamespace m_Namespace;
    string m_FileName;
    AutoPtr<CTypeStrings> m_RefType;
    bool m_IsObject;
};


END_NCBI_SCOPE

#endif
