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
* Revision 1.1  1999/06/24 14:44:39  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/

inline
CMemberInfo::CMemberInfo(void)
    : m_Offset(0), m_Tag(-1), m_Default(0)
{
}

inline
CMemberInfo::CMemberInfo(size_t offset, const CTypeRef& type)
    : m_Offset(offset), m_Tag(-1), m_Default(0), m_Type(type)
{
}

inline
CMemberInfo::CMemberInfo(const string& name, size_t offset, const CTypeRef& type)
    : m_Name(name), m_Offset(offset), m_Tag(-1), m_Default(0), m_Type(type)
{
}

inline
const string& CMemberInfo::GetName(void) const
{
    return m_Name;
}

inline
size_t CMemberInfo::GetOffset(void) const
{
    return m_Offset;
}

inline
CMemberInfo::TTag CMemberInfo::GetTag(void) const
{
    return m_Tag;
}

inline
TConstObjectPtr CMemberInfo::GetDefault(void) const
{
    return m_Default;
}

inline
bool CMemberInfo::Optional(void) const
{
    return GetDefault() != 0;
}

inline
TTypeInfo CMemberInfo::GetTypeInfo(void) const
{
    return m_Type.Get();
}

inline
TObjectPtr CMemberInfo::GetMember(TObjectPtr object) const
{
    return Add(object, m_Offset);
}

inline
TConstObjectPtr CMemberInfo::GetMember(TConstObjectPtr object) const
{
    return Add(object, m_Offset);
}

inline
TObjectPtr CMemberInfo::GetContainer(TObjectPtr object) const
{
    return Add(object, -m_Offset);
}

inline
TConstObjectPtr CMemberInfo::GetContainer(TConstObjectPtr object) const
{
    return Add(object, -m_Offset);
}

inline
size_t CMemberInfo::GetEndOffset(void) const
{
    return GetOffset() + GetSize();
}

#endif /* def MEMBER__HPP  &&  ndef MEMBER__INL */
