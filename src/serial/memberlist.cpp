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
* Revision 1.1  1999/06/30 16:04:52  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/memberlist.hpp>
#include <serial/member.hpp>

BEGIN_NCBI_SCOPE

CMembers::CMembers(void)
{
}

CMembers::~CMembers(void)
{
    for ( TMembers::iterator i = m_Members.begin(); i != m_Members.end(); ++i ) {
        delete const_cast<CMemberInfo*>(i->second);
    }
}

CMemberInfo* CMembers::AddMember(const CMemberId& id, CMemberInfo* member)
{
    m_MembersByName.reset(0);
    m_MembersByTag.reset(0);
    m_Members.push_back(TMembers::value_type(id, member));
    return member;
}

const CMembers::TMembersByName& CMembers::GetMembersByName(void) const
{
    TMembersByName* members = m_MembersByName.get();
    if ( !members ) {
        m_MembersByName.reset(members = new TMembersByName);
        for ( TMembers::const_iterator i = m_Members.begin();
              i != m_Members.end(); ++i ) {
            const string& name = i->first.GetName();
            if ( !members->insert(TMembersByName::
                                  value_type(name, i->second)).second ) {
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
        TTag nextTag = 0;
        for ( TMembers::const_iterator i = m_Members.begin();
              i != m_Members.end(); ++i ) {
            TTag tag = i->first.GetTag();
            if ( tag < 0 )
                tag = nextTag;
            if ( !members->insert(TMembersByTag::
                                  value_type(tag, i->second)).second ) {
                THROW1_TRACE(runtime_error,
                             "duplicated member tag: " + NStr::UIntToString(tag));
            }
            nextTag = tag + 1;
        }
    }
    return *members;
}

const CMemberInfo* CMembers::FindMember(const string& name) const
{
    const TMembersByName& members = GetMembersByName();
    TMembersByName::const_iterator i = members.find(name);
    if ( i == members.end() )
        return 0;
    return i->second;
}

const CMemberInfo* CMembers::FindMember(TTag tag) const
{
    const TMembersByTag& members = GetMembersByTag();
    TMembersByTag::const_iterator i = members.find(tag);
    if ( i == members.end() )
        return 0;
    return i->second;
}

const CMemberInfo* CMembers::FindMember(const CMemberId& id) const
{
    if ( id.GetTag() < 0 ) {
        return FindMember(id.GetName());
    }
    else {
        return FindMember(id.GetTag());
    }
}

END_NCBI_SCOPE
