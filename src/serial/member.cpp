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
* Revision 1.1  1999/06/30 16:04:50  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/member.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE

CMemberInfo::~CMemberInfo(void)
{
}

size_t CMemberInfo::GetSize(void) const
{
    return GetTypeInfo()->GetSize();
}

CMemberInfo* CMemberInfo::SetOptional(void)
{
    return SetDefault(GetTypeInfo()->GetDefault());
}

CMemberInfo* CMemberInfo::SetDefault(TConstObjectPtr def)
{
    m_Default = def;
    return this;
}

size_t CRealMemberInfo::GetOffset(void) const
{
    return m_Offset;
}

TTypeInfo CRealMemberInfo::GetTypeInfo(void) const
{
    return m_Type.Get();
}

const CMemberInfo* CMemberAliasInfo::GetBaseMember(void) const
{
    const CMemberInfo* baseMember = m_BaseMember;
    if ( !baseMember ) {
        m_BaseMember = baseMember =
            m_ContainerType.Get()->FindMember(m_MemberName);
        if ( !baseMember )
            THROW1_TRACE(runtime_error, "member not found: " + m_MemberName);
    }
    return baseMember;
}

size_t CMemberAliasInfo::GetOffset(void) const
{
    return GetBaseMember()->GetOffset();
}

TTypeInfo CMemberAliasInfo::GetTypeInfo(void) const
{
    return GetBaseMember()->GetTypeInfo();
}

TTypeInfo CTypedMemberAliasInfo::GetTypeInfo(void) const
{
    return m_Type.Get();
}

END_NCBI_SCOPE
