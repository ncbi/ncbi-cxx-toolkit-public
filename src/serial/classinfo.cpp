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
    _TRACE(ti.name());
    AddMember(new CMemberInfo(offset, parent));
    _TRACE(ti.name());
}

CClassInfoTmpl::~CClassInfoTmpl(void)
{
    DeleteElements(m_Members);
}

size_t CClassInfoTmpl::GetSize(void) const
{
    return m_Size;
}

TObjectPtr CClassInfoTmpl::Create(void) const
{
    return m_Creator();
}

CClassInfoTmpl* CClassInfoTmpl::AddMember(CMemberInfo* member)
{
    const string& name = member->GetName();
    if ( !m_Members.empty() ) {
        _TRACE("AddMember: " << name << ", " << 
               member->GetTypeInfo()->GetName() << ", " <<
               member->GetOffset() << "-" << member->GetEndOffset() << ", " <<
               " in " << GetSize());
        // check for conflicts
        if ( m_MembersByName.find(name) != m_MembersByName.end() ) {
            THROW1_TRACE(runtime_error, "duplicated members: " + name);
        }

        // check for valid offset and size
        size_t memberOffset = member->GetOffset();
        size_t memberEnd = memberOffset + member->GetSize();
        if ( member->GetSize() == 0 ) {
            THROW1_TRACE(runtime_error, "zero size members are not supported");
        }
        // check for limits of object
        if ( member->GetEndOffset() > GetSize() ) {
            THROW1_TRACE(runtime_error, "member out of object: " + name);
        }

        // member right after the one to be inserted, garanteed by lower_bound:
        // after is first member with offset >= memberEnd
        TMembersByOffset::const_iterator after =
            m_MembersByOffset.lower_bound(member->GetEndOffset());
        {
            if ( after == m_MembersByOffset.end() )
                _TRACE("after: end");
            else
                _TRACE("after: " << after->second->GetName() << ", " <<
                       after->second->GetOffset());
        }
        // if there are members before
        if ( after != m_MembersByOffset.begin() ) {
            // check for overlapping
            // member right before to be inserted:
            const CMemberInfo* memberBefore = after == m_MembersByOffset.end()?
                m_MembersByOffset.rbegin()->second:
                (--after)->second;

            _TRACE("before: " << memberBefore->GetName() << ", " <<
                   memberBefore->GetOffset() << "-" << memberBefore->GetEndOffset());

            // check overlapping
            if ( memberBefore->GetEndOffset() > member->GetOffset() ) {
                THROW1_TRACE(runtime_error, "overlapping member: " + name);
            }
        }
    }
    member->SetTag(m_Members.size());
    m_Members.push_back(member);
    m_MembersByName[name] = member;
    m_MembersByOffset[member->GetOffset()] = member;
    return this;
}

void CClassInfoTmpl::CollectExternalObjects(COObjectList& objectList,
                                            TConstObjectPtr object) const
{
    for ( TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        const CMemberInfo* memberInfo = *i;
        TTypeInfo memberTypeInfo = memberInfo->GetTypeInfo();
        TConstObjectPtr member = memberInfo->GetMember(object);
        memberTypeInfo->CollectExternalObjects(objectList, member);
    }
}

void CClassInfoTmpl::WriteData(CObjectOStream& out,
                               TConstObjectPtr object) const
{
    CObjectOStream::Block block(out);
    for ( TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        const CMemberInfo& memberInfo = **i;
        TTypeInfo memberTypeInfo = memberInfo.GetTypeInfo();
        TConstObjectPtr member = memberInfo.GetMember(object);
        TConstObjectPtr def = memberInfo.GetDefault();
        if ( !def || !memberTypeInfo->Equals(member, def) ) {
            block.Next();
            out.WriteMember(memberInfo);
            memberTypeInfo->WriteData(out, member);
        }
    }
}

void CClassInfoTmpl::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    CObjectIStream::Block block(in);
    while ( block.Next() ) {
        const string& memberName = in.ReadMemberName();
        TMembersByName::const_iterator i =
            m_MembersByName.find(memberName);
        if ( i == m_MembersByName.end() ) {
            ERR_POST("unknown member: " + memberName + ", trying to skip...");
            in.SkipValue();
            continue;
        }
        const CMemberInfo* memberInfo = i->second;
        TTypeInfo memberTypeInfo = memberInfo->GetTypeInfo();
        TObjectPtr member = memberInfo->GetMember(object);
        memberTypeInfo->ReadData(in, member);
    }
}

const CMemberInfo* CClassInfoTmpl::FindMember(const string& name) const
{
    TMembersByName::const_iterator i = m_MembersByName.find(name);
    if ( i == m_MembersByName.end() )
        return 0;
    return i->second;
}

const CMemberInfo* CClassInfoTmpl::LocateMember(TConstObjectPtr object,
                                                TConstObjectPtr member,
                                                TTypeInfo memberTypeInfo) const
{
    _TRACE("LocateMember(" << unsigned(object) << ", " << unsigned(member) << ", " << memberTypeInfo->GetName() << ")");
    TConstObjectPtr objectEnd = EndOf(object);
    TConstObjectPtr memberEnd = memberTypeInfo->EndOf(member);
    if ( member < object || memberEnd > objectEnd ) {
        return 0;
    }
    size_t memberOffset = Sub(member, object);
    size_t memberEndOffset = Sub(memberEnd, object);
    TMembersByOffset::const_iterator after =
        m_MembersByOffset.lower_bound(memberEndOffset);
    if ( after == m_MembersByOffset.begin() )
        return 0;

    const CMemberInfo* before = after == m_MembersByOffset.end()?
        m_MembersByOffset.rbegin()->second:
        (--after)->second;
    if ( memberOffset >= before->GetOffset() &&
         memberEndOffset <= before->GetEndOffset() ) {
        return before;
    }
    return 0;
}

TConstObjectPtr CClassInfoTmpl::GetDefault(void) const
{
    static TConstObjectPtr def = Create();
    return def;
}

bool CClassInfoTmpl::Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
{
    for ( TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        const CMemberInfo& member = **i;
        if ( !member.GetTypeInfo()->Equals(member.GetMember(object1),
                                           member.GetMember(object2)) )
            return false;
    }
    return true;
}

size_t CMemberInfo::GetSize(void) const
{
    return GetTypeInfo()->GetSize();
}

CMemberInfo* CMemberInfo::SetTag(TTag tag)
{
    m_Tag = tag;
    return this;
}

CMemberInfo* CMemberInfo::SetDefault(TConstObjectPtr def)
{
    m_Default = def;
    return this;
}

END_NCBI_SCOPE
