#if defined(MEMBERLIST__HPP)  &&  !defined(MEMBERLIST__INL)
#define MEMBERLIST__INL

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
* Revision 1.1  2000/04/10 18:01:52  vasilche
* Added Erase() for STL types in type iterators.
*
* ===========================================================================
*/

inline
bool CMembersInfo::HaveChildren(TConstObjectPtr /*object*/) const
{
    return !m_MembersInfo.empty();
}

inline
const CMemberInfo*
CMembersInfo::GetMemberInfo(const CChildrenTypesIterator& cc) const
{
    return m_MembersInfo[cc.GetIndex().m_Index];
}

inline
const CMemberInfo*
CMembersInfo::GetMemberInfo(const CConstChildrenIterator& cc) const
{
    return m_MembersInfo[cc.GetIndex().m_Index];
}

inline
const CMemberInfo*
CMembersInfo::GetMemberInfo(const CChildrenIterator& cc) const
{
    return m_MembersInfo[cc.GetIndex().m_Index];
}

inline
void CMembersInfo::BeginTypes(CChildrenTypesIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

inline
void CMembersInfo::Begin(CConstChildrenIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

inline
void CMembersInfo::Begin(CChildrenIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

inline
bool CMembersInfo::ValidTypes(const CChildrenTypesIterator& cc) const
{
    return cc.GetIndex().m_Index < m_MembersInfo.size();
}

inline
bool CMembersInfo::Valid(const CConstChildrenIterator& cc) const
{
    return cc.GetIndex().m_Index < m_MembersInfo.size();
}

inline
bool CMembersInfo::Valid(const CChildrenIterator& cc) const
{
    return cc.GetIndex().m_Index < m_MembersInfo.size();
}

inline
TTypeInfo CMembersInfo::GetChildType(const CChildrenTypesIterator& cc) const
{
    return GetMemberInfo(cc)->GetTypeInfo();
}

inline
void CMembersInfo::GetChild(const CConstChildrenIterator& cc,
                            CConstObjectInfo& child) const
{
    const CMemberInfo* member = GetMemberInfo(cc);
    child.Set(member->GetMember(cc.GetParentPtr()), member->GetTypeInfo());
}

inline
void CMembersInfo::GetChild(const CChildrenIterator& cc,
                         CObjectInfo& child) const
{
    const CMemberInfo* member = GetMemberInfo(cc);
    child.Set(member->GetMember(cc.GetParentPtr()), member->GetTypeInfo());
}

inline
void CMembersInfo::NextType(CChildrenTypesIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

inline
void CMembersInfo::Next(CConstChildrenIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

inline
void CMembersInfo::Next(CChildrenIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

#endif /* def MEMBERLIST__HPP  &&  ndef MEMBERLIST__INL */
