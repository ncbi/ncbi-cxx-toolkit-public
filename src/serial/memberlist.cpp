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
* Revision 1.2  1999/07/01 17:55:29  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.1  1999/06/30 16:04:52  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/memberlist.hpp>
#include <serial/memberid.hpp>

BEGIN_NCBI_SCOPE

CMembers::CMembers(void)
{
}

CMembers::~CMembers(void)
{
}

void CMembers::AddMember(const CMemberId& id)
{
    m_MembersByName.reset(0);
    m_MembersByTag.reset(0);
    m_Members.push_back(id);
}

CMemberId CMembers::GetCompleteMemberId(TIndex index) const
{
    const CMemberId& id = GetMemberId(index);
    if ( id.GetTag() >= 0 )
        return id;
    // regenerate tag
    TTag tag = 0;
    while ( --index >= 0 ) {
        TTag baseTag = GetMemberId(index).GetTag();
        if ( baseTag >= 0 ) {
            tag += baseTag;
            break;
        }
        ++tag;
    }
    return CMemberId(id.GetName(), tag);
}

const CMembers::TMembersByName& CMembers::GetMembersByName(void) const
{
    TMembersByName* members = m_MembersByName.get();
    if ( !members ) {
        m_MembersByName.reset(members = new TMembersByName);
        for ( TIndex i = 0, size = m_Members.size(); i < size; ++i ) {
            const string& name = m_Members[i].GetName();
            if ( !members->insert(TMembersByName::
                      value_type(name, i)).second ) {
                THROW1_TRACE(runtime_error, "duplicated member name: " + name);
            }
        }
    }
    return *members;
}

const CMembers::TMembersByTag& CMembers::GetMembersByTag(void) const
{
    TMembersByTag* members = m_MembersByTag.get();
    if ( !members ) {
        m_MembersByTag.reset(members = new TMembersByTag);
        CMemberId currentId;
        for ( TIndex i = 0, size = m_Members.size(); i < size; ++i ) {
            currentId.SetNext(m_Members[i]);
            if ( !members->insert(TMembersByTag::
                      value_type(currentId.GetTag(), i)).second ) {
                THROW1_TRACE(runtime_error,
                             "duplicated member tag: " + currentId.ToString());
            }
        }
    }
    return *members;
}

CMembers::TIndex CMembers::FindMember(const string& name) const
{
    const TMembersByName& members = GetMembersByName();
    TMembersByName::const_iterator i = members.find(name);
    if ( i == members.end() )
        return 0;
    return i->second;
}

CMembers::TIndex CMembers::FindMember(TTag tag) const
{
    const TMembersByTag& members = GetMembersByTag();
    TMembersByTag::const_iterator i = members.find(tag);
    if ( i == members.end() )
        return 0;
    return i->second;
}

CMembers::TIndex CMembers::FindMember(const CMemberId& id) const
{
    if ( id.GetTag() < 0 ) {
        return FindMember(id.GetName());
    }
    else {
        return FindMember(id.GetTag());
    }
}

END_NCBI_SCOPE
