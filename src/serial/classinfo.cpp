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
* Revision 1.11  1999/06/30 16:04:48  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.10  1999/06/24 14:44:53  vasilche
* Added binary ASN.1 output.
*
* Revision 1.9  1999/06/17 18:38:52  vasilche
* Fixed order of members in class.
* Added checks for overlapped members.
*
* Revision 1.8  1999/06/16 20:35:29  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.7  1999/06/15 16:19:47  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.6  1999/06/10 21:06:45  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.5  1999/06/07 20:42:58  vasilche
* Fixed compilation under MS VS
*
* Revision 1.4  1999/06/07 19:59:40  vasilche
* offset_t -> size_t
*
* Revision 1.3  1999/06/07 19:30:24  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:44  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:51  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/classinfo.hpp>
#include <serial/member.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <set>

BEGIN_NCBI_SCOPE

CClassInfoTmpl::CClassInfoTmpl(const type_info& ti, size_t size,
                               TObjectPtr (*creator)(void))
    : CParent(ti), m_Size(size), m_Creator(creator)
{
    _TRACE(ti.name());
}

CClassInfoTmpl::CClassInfoTmpl(const type_info& ti, size_t size,
                               TObjectPtr (*creator)(void),
                               const CTypeRef& parent, size_t offset)
    : CParent(ti), m_Size(size), m_Creator(creator)
{
    AddMember(NcbiEmptyString, new CRealMemberInfo(offset, parent));
}

CClassInfoTmpl::~CClassInfoTmpl(void)
{
}

size_t CClassInfoTmpl::GetSize(void) const
{
    return m_Size;
}

TObjectPtr CClassInfoTmpl::Create(void) const
{
    return m_Creator();
}

CMemberInfo* CClassInfoTmpl::AddMember(const CMemberId& id, CMemberInfo* member)
{
    m_MembersByOffset.reset(0);
    return m_Members.AddMember(id, member);
}

CMemberInfo* CClassInfoTmpl::AddMember(const string& name, CMemberInfo* member)
{
    return AddMember(CMemberId(name), member);
}

void CClassInfoTmpl::CollectExternalObjects(COObjectList& objectList,
                                            TConstObjectPtr object) const
{
    for ( CMembers::TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        const CMemberInfo* memberInfo = i->second;
        TTypeInfo memberTypeInfo = memberInfo->GetTypeInfo();
        TConstObjectPtr member = memberInfo->GetMember(object);
        memberTypeInfo->CollectExternalObjects(objectList, member);
    }
}

void CClassInfoTmpl::WriteData(CObjectOStream& out,
                               TConstObjectPtr object) const
{
    CObjectOStream::Block block(out);
    CMemberId currentId;
    for ( CMembers::TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        const CMemberInfo& memberInfo = *i->second;
        TTypeInfo memberTypeInfo = memberInfo.GetTypeInfo();
        TConstObjectPtr member = memberInfo.GetMember(object);
        TConstObjectPtr def = memberInfo.GetDefault();
        currentId.SetNext(i->first);
        if ( !def || !memberTypeInfo->Equals(member, def) ) {
            block.Next();
            out.WriteMember(currentId);
            memberTypeInfo->WriteData(out, member);
        }
    }
}

void CClassInfoTmpl::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    if ( RandomOrder() ) {
        set<const CMemberInfo*> read;
        CObjectIStream::Block block(in);
        while ( block.Next() ) {
            CMemberId memberId = in.ReadMember();
            const CMemberInfo* memberInfo = m_Members.FindMember(memberId);
            if ( !memberInfo ) {
                ERR_POST("unknown member: " +
                         memberId.ToString() + ", skipping");
                in.SkipValue();
                continue;
            }
            if ( read.find(memberInfo) != read.end() ) {
                ERR_POST("duplicated member: " +
                         memberId.ToString() + ", skipping");
                in.SkipValue();
                continue;
            }
            read.insert(memberInfo);
            TTypeInfo memberTypeInfo = memberInfo->GetTypeInfo();
            TObjectPtr member = memberInfo->GetMember(object);
            memberTypeInfo->ReadData(in, member);
        }
        // init all absent members
        for ( CMembers::TMembers::const_iterator i = m_Members.begin();
              i != m_Members.end(); ++i ) {
            const CMemberInfo* memberInfo = i->second;
            if ( read.find(memberInfo) == read.end() ) {
                // check if this member have defaults
                TConstObjectPtr def = memberInfo->GetDefault();
                if ( def != 0 ) {
                    // copy defult
                    memberInfo->GetTypeInfo()->
                        Assign(memberInfo->GetMember(object), def);
                }
                else {
                    // error: absent member w/o defult
                    THROW1_TRACE(runtime_error,
                                 "member " + i->first.GetName() +
                                 " is missing and doesn't have default");
                }
            }
        }
    }
    else {
        // sequential order
        CMembers::TMembers::const_iterator i = m_Members.begin();
        CObjectIStream::Block block(in);
        CMemberId currentId;
        while ( block.Next() ) {
            CMemberId memberId = in.ReadMember();
            // find desired member
            const CMemberInfo* memberInfo;
            for ( ;; ++i ) {
                if ( i == m_Members.end() ) {
                    THROW1_TRACE(runtime_error,
                                 "unexpected member: " + memberId.ToString());
                }
                memberInfo = i->second;
                currentId.SetNext(i->first);
                if ( currentId == memberId )
                    break;
                
                TConstObjectPtr def = memberInfo->GetDefault();
                if ( def == 0 ) {
                    THROW1_TRACE(runtime_error, "member " +
                                 i->first.GetName() + " expected");
                }
                memberInfo->GetTypeInfo()->
                    Assign(memberInfo->GetMember(object), def);
            }
            ++i;
            memberInfo->GetTypeInfo()->
                ReadData(in, memberInfo->GetMember(object));
        }
        for ( ; i != m_Members.end(); ++i ) {
            const CMemberInfo* memberInfo = i->second;
            TConstObjectPtr def = memberInfo->GetDefault();
            if ( def == 0 ) {
                THROW1_TRACE(runtime_error,
                             "member " + i->first.GetName() + " expected");
            }
            memberInfo->GetTypeInfo()->
                Assign(memberInfo->GetMember(object), def);
        }
    }
}

const CClassInfoTmpl::TMembersByOffset& CClassInfoTmpl::GetMembersByOffset(void) const
{
    TMembersByOffset* members = m_MembersByOffset.get();
    if ( !members ) {
        m_MembersByOffset.reset(members = new TMembersByOffset);
        const CMembers::TMembers& mlist = m_Members.GetMembers();
        for ( CMembers::TMembers::const_iterator i = mlist.begin();
              i != mlist.end(); ++i ) {
            const CMemberInfo* info = i->second;
            size_t offset = info->GetOffset();
            if ( !members->insert(TMembersByOffset::
                                  value_type(offset,
                                             make_pair(&i->first,
                                                       i->second))).second ) {
                THROW1_TRACE(runtime_error,
                             "conflict member offset: " +
                             NStr::UIntToString(offset));
            }
        }
        size_t nextOffset = 0;
        for ( TMembersByOffset::const_iterator i = members->begin();
              i != members->end(); ++i ) {
            size_t offset = i->first;
            if ( offset < nextOffset ) {
                THROW1_TRACE(runtime_error,
                             "overlapping members");
            }
            nextOffset = offset + i->second.second->GetSize();
        }
    }
    return *members;
}

const CMemberInfo* CClassInfoTmpl::FindMember(const string& name) const
{
    return m_Members.FindMember(name);
}

pair<const CMemberId*, const CMemberInfo*>
CClassInfoTmpl::LocateMember(TConstObjectPtr object,
                             TConstObjectPtr member,
                             TTypeInfo memberTypeInfo) const
{
    _TRACE("LocateMember(" << unsigned(object) << ", " << unsigned(member) <<
           ", " << memberTypeInfo->GetName() << ")");
    TConstObjectPtr objectEnd = EndOf(object);
    TConstObjectPtr memberEnd = memberTypeInfo->EndOf(member);
    if ( member < object || memberEnd > objectEnd ) {
        return pair<const CMemberId*, const CMemberInfo*>(0, 0);
    }
    size_t memberOffset = Sub(member, object);
    size_t memberEndOffset = Sub(memberEnd, object);
    const TMembersByOffset& members = GetMembersByOffset();
    TMembersByOffset::const_iterator after =
        members.lower_bound(memberEndOffset);
    if ( after == members.begin() )
        return pair<const CMemberId*, const CMemberInfo*>(0, 0);

    pair<const CMemberId*, const CMemberInfo*> before = after == members.end()?
        members.rbegin()->second:
        (--after)->second;

    if ( memberOffset < before.second->GetOffset() ||
         memberEndOffset > before.second->GetEndOffset() ) {
        return pair<const CMemberId*, const CMemberInfo*>(0, 0);
    }
    return before;
}

bool CClassInfoTmpl::Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
{
    for ( CMembers::TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        const CMemberInfo& member = *i->second;
        if ( !member.GetTypeInfo()->Equals(member.GetMember(object1),
                                           member.GetMember(object2)) )
            return false;
    }
    return true;
}

void CClassInfoTmpl::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    for ( CMembers::TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        const CMemberInfo& member = *i->second;
        member.GetTypeInfo()->Assign(member.GetMember(dst),
                                     member.GetMember(src));
    }
}

END_NCBI_SCOPE
