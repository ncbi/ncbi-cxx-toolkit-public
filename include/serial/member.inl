#if defined(MEMBER__HPP)  &&  !defined(MEMBER__INL)
#define MEMBER__INL

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
* Revision 1.5  1999/09/01 17:38:01  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.4  1999/08/31 17:50:04  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.3  1999/08/13 15:53:42  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.2  1999/06/30 16:04:22  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.1  1999/06/24 14:44:39  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/

inline
CMemberInfo::CMemberInfo(void)
    : m_Optional(false), m_Default(0)
{
}

inline
bool CMemberInfo::Optional(void) const
{
    return m_Optional;
}

inline
TObjectPtr CMemberInfo::GetMember(TObjectPtr object) const
{
    return Add(object, GetOffset());
}

inline
TConstObjectPtr CMemberInfo::GetMember(TConstObjectPtr object) const
{
    return Add(object, GetOffset());
}

inline
TObjectPtr CMemberInfo::GetContainer(TObjectPtr object) const
{
    return Add(object, -GetOffset());
}

inline
TConstObjectPtr CMemberInfo::GetContainer(TConstObjectPtr object) const
{
    return Add(object, -GetOffset());
}

inline
size_t CMemberInfo::GetEndOffset(void) const
{
    return GetOffset() + GetSize();
}

inline
CRealMemberInfo::CRealMemberInfo(size_t offset, const CTypeRef& type)
    : m_Offset(offset), m_Type(type)
{
}

inline
CMemberAliasInfo::CMemberAliasInfo(const CTypeRef& containerType,
                                   const string& memberName)
    : m_ContainerType(containerType), m_MemberName(memberName)
{
}

inline
CTypedMemberAliasInfo::CTypedMemberAliasInfo(const CTypeRef& type,
                                             const CTypeRef& containerType,
                                             const string& memberName)
    : CMemberAliasInfo(containerType, memberName), m_Type(type)
{
}

#endif /* def MEMBER__HPP  &&  ndef MEMBER__INL */
