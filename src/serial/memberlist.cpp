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
* Revision 1.9  2000/03/29 15:55:27  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.8  2000/02/01 21:47:22  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.7  2000/01/10 19:46:39  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.6  1999/10/04 16:22:16  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.5  1999/07/22 19:40:55  vasilche
* Fixed bug with complex object graphs (pointers to members of other objects).
*
* Revision 1.4  1999/07/14 18:58:08  vasilche
* Fixed ASN.1 types/field naming.
*
* Revision 1.3  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
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
#include <serial/member.hpp>
#include <serial/iterator.hpp>
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

CMembers::CMembers(void)
{
}

CMembers::~CMembers(void)
{
}

void CMembers::AddMember(const CMemberId& id)
{
    // clear cached maps (byname and bytag)
    m_MembersByName.reset(0);
    m_MembersByTag.reset(0);
    m_MemberTags.reset(0);
    m_Members.push_back(id);
}

const CMembers::TMembersByName& CMembers::GetMembersByName(void) const
{
    TMembersByName* members = m_MembersByName.get();
    if ( !members ) {
        m_MembersByName.reset(members = new TMembersByName);
        // TMembers is vector so we'll use index access instead iterator
        // because we need index value to inser in map too
        for ( TIndex i = 0, size = m_Members.size(); i < size; ++i ) {
            const string& name = m_Members[i].GetName();
            if ( !members->insert(TMembersByName::
                      value_type(name.c_str(), i)).second ) {
                if ( !name.empty() )
                    THROW1_TRACE(runtime_error,
                                 "duplicated member name: " + name);
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
        TTag currentTag = -1;
        // TMembers is vector so we'll use index access instead iterator
        // because we need index value to inser in map too
        for ( TIndex i = 0, size = m_Members.size(); i < size; ++i ) {
            TTag t = m_Members[i].GetTag();
            if ( t < 0 ) {
                if ( i == 0 && m_Members[i].GetName().empty() ) {
                    // parent class - skip it
                    continue;
                }
                t = currentTag + 1;
            }
            if ( !members->insert(TMembersByTag::
                      value_type(t, i)).second ) {
                THROW1_TRACE(runtime_error,
                             "duplicated member tag: " +
                             m_Members[i].ToString());
            }
            currentTag = t;
        }
    }
    return *members;
}

const CMembers::TMemberTags& CMembers::GetMemberTags(void) const
{
    TMemberTags* members = m_MemberTags.get();
    if ( !members ) {
        m_MemberTags.reset(members = new TMemberTags(m_Members.size()));
        TTag currentTag = -1;
        // TMembers is vector so we'll use index access instead iterator
        // because we need index value to inser in map too
        for ( TIndex i = 0, size = m_Members.size(); i < size; ++i ) {
            TTag t = m_Members[i].GetTag();
            if ( t < 0 ) {
                if ( i == 0 && m_Members[i].GetName().empty() ) {
                    // parent class - skip it
                    (*members)[i] = -1;
                    continue;
                }
                t = currentTag + 1;
            }
            (*members)[i] = t;
            currentTag = t;
        }
    }
    return *members;
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

CMembers::TIndex CMembers::FindMember(const string& name) const
{
    return FindMember(name.c_str());
}

CMembers::TIndex CMembers::FindMember(const char* name) const
{
    const TMembersByName& members = GetMembersByName();
    TMembersByName::const_iterator i = members.find(name);
    if ( i == members.end() )
        return -1;
    return i->second;
}

CMembers::TIndex CMembers::FindMember(TTag tag) const
{
    const TMembersByTag& members = GetMembersByTag();
    TMembersByTag::const_iterator i = members.find(tag);
    if ( i == members.end() )
        return -1;
    return i->second;
}

CMembers::TIndex CMembers::FindMember(const pair<const char*, size_t>& name,
                                      TIndex pos) const
{
    for ( size_t i = pos + 1, size = m_Members.size(); i < size; ++i ) {
        const string& s = m_Members[i].GetName();
        if ( s.size() == name.second &&
             memcmp(s.data(), name.first, name.second) == 0 )
            return i;
    }
    return -1;
}

CMembers::TIndex CMembers::FindMember(TTag tag, TIndex pos) const
{
    const TMemberTags& tags = GetMemberTags();
    for ( size_t i = pos + 1, size = m_Members.size(); i < size; ++i ) {
        if ( tags[i] == tag )
            return i;
    }
    return -1;
}

CMembersInfo::CMembersInfo(void)
{
}

CMembersInfo::~CMembersInfo(void)
{
    DeleteElements(m_MembersInfo);
}

CMemberInfo* CMembersInfo::AddMember(const CMemberId& id,
                                     CMemberInfo* member)
{
    m_MembersByOffset.reset(0);
    CMembers::AddMember(id);
    m_MembersInfo.push_back(member);
    return member;
}

CMemberInfo* CMembersInfo::AddMember(const char* name, const void* member,
                                     TTypeInfo type)
{
    return AddMember(name, new CRealMemberInfo(size_t(member), type));
}

CMemberInfo* CMembersInfo::AddMember(const char* name, const void* member,
                                     const CTypeRef& type)
{
    return AddMember(name, new CRealMemberInfo(size_t(member), type));
}

size_t CMembersInfo::GetFirstMemberOffset(void) const
{
    size_t offset = INT_MAX;
    for ( TMembersInfo::const_iterator i = m_MembersInfo.begin();
          i != m_MembersInfo.end();
          ++i ) {
        offset = min(offset, (*i)->GetOffset());
    }
    return offset;
}

const CMembersInfo::TMembersByOffset&
CMembersInfo::GetMembersByOffset(void) const
{
    TMembersByOffset* members = m_MembersByOffset.get();
    if ( !members ) {
        // create map
        m_MembersByOffset.reset(members = new TMembersByOffset);
        // fill map
        
        for ( TIndex i = 0, size = m_MembersInfo.size();
              i != size; ++i ) {
            const CMemberInfo& info = *m_MembersInfo[i];
            size_t offset = info.GetOffset();
            if ( !members->insert(TMembersByOffset::
                                  value_type(offset, i)).second ) {
                THROW1_TRACE(runtime_error,
                             "conflict member offset: " +
                             NStr::UIntToString(offset));
            }
        }
        // check overlaps
        size_t nextOffset = 0;
        for ( TMembersByOffset::const_iterator m = members->begin();
              m != members->end(); ++m ) {
            size_t offset = m->first;
            if ( offset < nextOffset ) {
                THROW1_TRACE(runtime_error,
                             "overlapping members");
            }
            nextOffset = offset + m_MembersInfo[m->second]->GetSize();
        }
    }
    return *members;
}

bool CMembersInfo::MayContainType(TTypeInfo typeInfo) const
{
    iterate ( TMembersInfo, i, m_MembersInfo ) {
        TTypeInfo childType = (*i)->GetTypeInfo();
        if ( childType->IsType(typeInfo) ||
            childType->MayContainType(typeInfo) )
            return true;
    }
    return false;
}

bool CMembersInfo::HaveChildren(TConstObjectPtr /*object*/) const
{
    return !m_MembersInfo.empty();
}

void CMembersInfo::BeginTypes(CChildrenTypesIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

void CMembersInfo::Begin(CConstChildrenIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

void CMembersInfo::Begin(CChildrenIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

bool CMembersInfo::ValidTypes(const CChildrenTypesIterator& cc) const
{
    return cc.GetIndex().m_Index < m_MembersInfo.size();
}

bool CMembersInfo::Valid(const CConstChildrenIterator& cc) const
{
    return cc.GetIndex().m_Index < m_MembersInfo.size();
}

bool CMembersInfo::Valid(const CChildrenIterator& cc) const
{
    return cc.GetIndex().m_Index < m_MembersInfo.size();
}

TTypeInfo CMembersInfo::GetChildType(const CChildrenTypesIterator& cc) const
{
    return m_MembersInfo[cc.GetIndex().m_Index]->GetTypeInfo();
}

void CMembersInfo::GetChild(const CConstChildrenIterator& cc,
                            CConstObjectInfo& child) const
{
    const CMemberInfo* member = m_MembersInfo[cc.GetIndex().m_Index];
    child.Set(member->GetMember(cc.GetParent().GetObjectPtr()),
              member->GetTypeInfo());
}

void CMembersInfo::GetChild(const CChildrenIterator& cc,
                         CObjectInfo& child) const
{
    const CMemberInfo* member = m_MembersInfo[cc.GetIndex().m_Index];
    child.Set(member->GetMember(cc.GetParentPtr()), member->GetTypeInfo());
}

void CMembersInfo::NextType(CChildrenTypesIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

void CMembersInfo::Next(CConstChildrenIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

void CMembersInfo::Next(CChildrenIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

END_NCBI_SCOPE
