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
* Revision 1.1  1999/05/19 19:56:51  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/classinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

CClassInfoTmpl::CClassInfoTmpl(const type_info& ti, size_t size,
                               const type_info& pti, offset_t offset,
                               TObjectPtr (*creator)(void))
    : CParent(ti.name()), m_Size(size), m_Creator(creator)
{
    if ( pti != typeid(void) ) {
        AddMember(CMemberInfo(offset, GetTypeInfo(pti.name())));
    }
}

size_t CClassInfoTmpl::GetSize(void) const
{
    return m_Size;
}

CTypeInfo::TObjectPtr CClassInfoTmpl::Create(void) const
{
    return m_Creator();
}

CClassInfoTmpl* CClassInfoTmpl::AddMember(const CMemberInfo& member)
{
    // check for conflicts
    if ( m_Members.find(member.GetName()) != m_Members.end() )
        throw runtime_error("duplicated members: " + member.GetName());

    //    m_MembersByOffset.Add(member.GetOffset(), member.GetTypeInfo());
    m_Members[member.GetName()] = member;
/*
    offset_t start = member.GetOffset();
    offset_t end = start + member.GetSize();
    if ( start > GetSize() || end > GetSize() )
        throw runtime_error("member expands past end of object");

    TMembersByOffset::iterator prev = m_Members.lower_bound(start);
    TMembersByOffset::iterator next = m_Members.lower_bound(end);
    
    TMembersByOffset::iterator 
    if ( i == 
*/
    return this;
}

void CClassInfoTmpl::CollectMembers(CObjectList& list,
                                    TConstObjectPtr object) const
{
    AddObject(list, *static_cast<TConstObjectPtr*>(object),
              GetRealDataTypeInfo(object));
}

void CClassInfoTmpl::WriteData(CObjectOStream& out,
                               TConstObjectPtr object) const
{
    out.WriteObject(*static_cast<TConstObjectPtr*>(object),
                    GetRealDataTypeInfo(object));
}

void CClassInfoTmpl::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    *static_cast<TConstObjectPtr*>(object) = in.ReadObject(this);
}


END_NCBI_SCOPE
