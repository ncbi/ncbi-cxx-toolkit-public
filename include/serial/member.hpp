#ifndef MEMBER__HPP
#define MEMBER__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  1999/08/31 17:50:03  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.3  1999/08/13 15:53:42  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.2  1999/06/30 16:04:21  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.1  1999/06/24 14:44:38  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeref.hpp>

BEGIN_NCBI_SCOPE

class CMemberInfo {
public:
    CMemberInfo(void);
    virtual ~CMemberInfo(void);

    bool Implicit(void) const;
    CMemberInfo* SetImplicit(void);
    bool Optional(void) const;
    CMemberInfo* SetOptional(void);
    TConstObjectPtr GetDefault(void) const;
    CMemberInfo* SetDefault(TConstObjectPtr def);

    virtual size_t GetOffset(void) const = 0;

    virtual TTypeInfo GetTypeInfo(void) const = 0;

    size_t GetSize(void) const;

    TObjectPtr GetMember(TObjectPtr object) const;
    TConstObjectPtr GetMember(TConstObjectPtr object) const;
    TObjectPtr GetContainer(TObjectPtr object) const;
    TConstObjectPtr GetContainer(TConstObjectPtr object) const;

    size_t GetEndOffset(void) const;

private:
    bool m_Implicit;
    bool m_Optional;
    // default value
    TConstObjectPtr m_Default;
};

class CRealMemberInfo : public CMemberInfo {
public:
    // common member
    CRealMemberInfo(size_t offset, const CTypeRef& type);

    virtual size_t GetOffset(void) const;
    virtual TTypeInfo GetTypeInfo(void) const;

private:
    // physical description
    size_t m_Offset;
    CTypeRef m_Type;
};

class CMemberAliasInfo : public CMemberInfo {
public:
    // common member
    CMemberAliasInfo(const CTypeRef& containerType,
                     const string& memberName);

    virtual size_t GetOffset(void) const;
    virtual TTypeInfo GetTypeInfo(void) const;

protected:
    const CMemberInfo* GetBaseMember(void) const;

private:
    // physical description
    mutable const CMemberInfo* m_BaseMember;
    CTypeRef m_ContainerType;
    string m_MemberName;
};

class CTypedMemberAliasInfo : public CMemberAliasInfo {
public:
    // common member
    CTypedMemberAliasInfo(const CTypeRef& type,
                          const CTypeRef& containerType,
                          const string& memberName);

    virtual TTypeInfo GetTypeInfo(void) const;

private:
    // physical description
    CTypeRef m_Type;
};

#include <serial/member.inl>

END_NCBI_SCOPE

#endif  /* MEMBER__HPP */
