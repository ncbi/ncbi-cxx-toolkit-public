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
* Revision 1.6  1999/09/14 18:54:17  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.5  1999/09/01 17:38:12  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.4  1999/08/31 17:50:08  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.3  1999/08/13 15:53:50  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.2  1999/07/01 17:55:28  vasilche
* Implemented ASN.1 binary write.
*
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
    m_Optional = true;
    return this;
}

CMemberInfo* CMemberInfo::SetDefault(TConstObjectPtr def)
{
    SetOptional();
    m_Default = def;
    return this;
}

TConstObjectPtr CMemberInfo::GetDefault(void) const
{
    return m_Default;
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
        TTypeInfo contTypeInfo = m_ContainerType.Get();
        CTypeInfo::TMemberIndex index = contTypeInfo->FindMember(m_MemberName);
        if ( index < 0 )
            THROW1_TRACE(runtime_error, "member not found: " + m_MemberName);
        m_BaseMember = baseMember = contTypeInfo->GetMemberInfo(index);
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
