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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.2  2004/09/07 14:09:45  grichenk
* Fixed assignment of default value to aliased types
*
* Revision 1.1  2003/10/21 13:45:23  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#include <serial/datatool/typestr.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/datatool/namespace.hpp>

BEGIN_NCBI_SCOPE

class CAliasTypeStrings : public CTypeStrings
{
    typedef CTypeStrings TParent;
public:
    CAliasTypeStrings(const string& externalName,
                      const string& className,
                      CTypeStrings& ref_type);
    ~CAliasTypeStrings(void);

    EKind GetKind(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    bool HaveSpecialRef(void) const;
    string GetRef(const CNamespace& ns) const;

    bool CanBeKey(void) const;
    bool CanBeCopied(void) const;
    // bool NeedSetFlag(void) const;

    string NewInstance(const string& init, const string& place) const;

    string GetInitializer(void) const;
    string GetDestructionCode(const string& expr) const;
    string GetIsSetCode(const string& var) const;
    string GetResetCode(const string& var) const;
    string GetDefaultCode(const string& var) const;

    void GenerateCode(CClassContext& ctx) const;
    void GenerateUserHPPCode(CNcbiOstream& out) const;
    void GenerateUserCPPCode(CNcbiOstream& out) const;

    void GenerateTypeCode(CClassContext& ctx) const;
    void GeneratePointerTypeCode(CClassContext& ctx) const;

    string GetClassName(void) const;
    string GetExternalName(void) const;

private:
    string m_ExternalName;
    string m_ClassName;
    AutoPtr<CTypeStrings> m_RefType;
};


class CAliasRefTypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    CAliasRefTypeStrings(const string& className,
                         const CNamespace& ns,
                         const string& fileName,
                         CTypeStrings& ref_type);

    EKind GetKind(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    bool HaveSpecialRef(void) const;
    string GetRef(const CNamespace& ns) const;

    bool CanBeKey(void) const;
    bool CanBeCopied(void) const;
    // bool NeedSetFlag(void) const;

    const CNamespace& GetNamespace(void) const;

    string GetInitializer(void) const;
    string GetDestructionCode(const string& expr) const;
    string GetIsSetCode(const string& var) const;
    string GetResetCode(const string& var) const;
    string GetDefaultCode(const string& var) const;

    virtual void GenerateCode(CClassContext& ctx) const;
    virtual void GenerateUserHPPCode(CNcbiOstream& out) const;
    virtual void GenerateUserCPPCode(CNcbiOstream& out) const;

    virtual void GenerateTypeCode(CClassContext& ctx) const;
    virtual void GeneratePointerTypeCode(CClassContext& ctx) const;

private:
    string m_ClassName;
    CNamespace m_Namespace;
    string m_FileName;
    AutoPtr<CTypeStrings> m_RefType;
    bool m_IsObject;
};


END_NCBI_SCOPE

#endif
