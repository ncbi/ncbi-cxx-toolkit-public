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
* Revision 1.26  1999/12/17 19:05:02  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.25  1999/11/19 18:26:00  vasilche
* Fixed conflict with SetImplicit() in generated choice variant classes.
*
* Revision 1.24  1999/10/05 14:08:33  vasilche
* Fixed class name under GCC and MSVC.
*
* Revision 1.23  1999/10/04 16:22:16  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.22  1999/09/22 20:11:54  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.21  1999/09/14 18:54:16  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.20  1999/09/01 17:38:12  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.19  1999/08/31 17:50:08  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.18  1999/08/13 15:53:50  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.17  1999/07/20 18:23:09  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.16  1999/07/13 20:18:17  vasilche
* Changed types naming.
*
* Revision 1.15  1999/07/07 19:59:03  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.14  1999/07/02 21:31:52  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.13  1999/07/01 17:55:26  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.12  1999/06/30 18:54:59  vasilche
* Fixed some errors under MSVS
*
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

static
string ClassName(const type_info& id)
{
    const char* s = id.name();
    if ( memcmp(s, "class ", 6) == 0 ) {
        // MSVC
        return s + 6;
    }
    else if ( isdigit(*s ) ) {
        // GCC
        while ( isdigit(*s) )
            ++s;
        return s;
    }
    else {
        return s;
    }
}

CClassInfoTmpl::CClassInfoTmpl(const type_info& id, size_t size)
    : CParent(ClassName(id)), m_Id(id), m_Size(size),
      m_RandomOrder(false), m_Implicit(false)
{
    Register();
}

CClassInfoTmpl::CClassInfoTmpl(const string& name, const type_info& id,
                               size_t size)
    : CParent(name), m_Id(id), m_Size(size),
      m_RandomOrder(false), m_Implicit(false)
{
    Register();
}

CClassInfoTmpl::CClassInfoTmpl(const char* name, const type_info& id,
                               size_t size)
    : CParent(name), m_Id(id), m_Size(size),
      m_RandomOrder(false), m_Implicit(false)
{
    Register();
}

CClassInfoTmpl::~CClassInfoTmpl(void)
{
    Deregister();
    DeleteElements(m_MembersInfo);
}

CClassInfoTmpl::TClasses* CClassInfoTmpl::sm_Classes = 0;
CClassInfoTmpl::TClassesById* CClassInfoTmpl::sm_ClassesById = 0;
CClassInfoTmpl::TClassesByName* CClassInfoTmpl::sm_ClassesByName = 0;

inline
CClassInfoTmpl::TClasses& CClassInfoTmpl::Classes(void)
{
    TClasses* classes = sm_Classes;
    if ( !classes ) {
        classes = sm_Classes = new TClasses;
    }
    return *classes;
}

inline
CClassInfoTmpl::TClassesById& CClassInfoTmpl::ClassesById(void)
{
    TClassesById* classes = sm_ClassesById;
    if ( !classes ) {
        classes = sm_ClassesById = new TClassesById;
        const TClasses& cc = Classes();
        for ( TClasses::const_iterator i = cc.begin(); i != cc.end(); ++i ) {
            const CClassInfoTmpl* info = *i;
            if ( info->GetId() != typeid(void) ) {
                _TRACE("class by id: " << info->GetId().name() << " : " << info->GetName());
                if ( !classes->insert(
                         TClassesById::value_type(&info->GetId(),
                                                  info)).second ) {
                    THROW1_TRACE(runtime_error,
                                 "duplicated class ids " +
                                 string(info->GetId().name()));
                }
            }
        }
    }
    return *classes;
}

inline
CClassInfoTmpl::TClassesByName& CClassInfoTmpl::ClassesByName(void)
{
    TClassesByName* classes = sm_ClassesByName;
    if ( !classes ) {
        classes = sm_ClassesByName = new TClassesByName;
        const TClasses& cc = Classes();
        for ( TClasses::const_iterator i = cc.begin(); i != cc.end(); ++i ) {
            const CClassInfoTmpl* info = *i;
            if ( !info->GetName().empty() ) {
                _TRACE("ass class by name: " << " : " << info->GetName() << info->GetId().name());
                if ( !classes->insert(
                         TClassesByName::value_type(info->GetName(),
                                                    info)).second ) {
                    THROW1_TRACE(runtime_error,
                                 "duplicated class names " + info->GetName());
                }
            }
        }
    }
    return *classes;
}

void CClassInfoTmpl::Register(void)
{
    delete sm_ClassesById;
    sm_ClassesById = 0;
    delete sm_ClassesByName;
    sm_ClassesByName = 0;
    Classes().push_back(this);
}

void CClassInfoTmpl::Deregister(void) const
{
}

void CClassInfoTmpl::RegisterSubClasses(void) const
{
    const TSubClasses* subclasses = m_SubClasses.get();
    if ( subclasses ) {
        for ( TSubClasses::const_iterator i = subclasses->begin();
              i != subclasses->end();
              ++i ) {
            dynamic_cast<const CClassInfoTmpl*>(i->second.Get())->
                RegisterSubClasses();
        }
    }
}

TTypeInfo CClassInfoTmpl::GetClassInfoById(const type_info& id)
{
    TClassesById& types = ClassesById();
    TClassesById::iterator i = types.find(&id);
    if ( i == types.end() ) {
        THROW1_TRACE(runtime_error, "class not found: " + string(id.name()));
    }
    return i->second;
}

TTypeInfo CClassInfoTmpl::GetClassInfoByName(const string& name)
{
    TClassesByName& classes = ClassesByName();
    TClassesByName::iterator i = classes.find(name);
    if ( i == classes.end() ) {
        THROW1_TRACE(runtime_error, "class not found: " + name);
    }
    return i->second;
}

TTypeInfo CClassInfoTmpl::GetRealTypeInfo(TConstObjectPtr object) const
{
    const type_info* ti = GetCPlusPlusTypeInfo(object);
    if ( ti == 0 )
        return this;
    RegisterSubClasses();
    return GetClassInfoById(*ti);
}

size_t CClassInfoTmpl::GetSize(void) const
{
    return m_Size;
}

CMemberInfo* CClassInfoTmpl::AddMember(const CMemberId& id,
                                       CMemberInfo* member)
{
    _TRACE(GetName() << ".AddMember:" << id.ToString() << ", " << member->GetOffset());
    m_MembersByOffset.reset(0);
    m_Members.AddMember(id);
    m_MembersInfo.push_back(member);
    return member;
}

CMemberInfo* CClassInfoTmpl::AddMember(const char* name, const void* member,
                                       TTypeInfo type)
{
    return AddMember(name, new CRealMemberInfo(size_t(member), type));
}

CMemberInfo* CClassInfoTmpl::AddMember(const char* name, const void* member,
                                       const CTypeRef& type)
{
    return AddMember(name, new CRealMemberInfo(size_t(member), type));
}

/*
CMemberInfo* CClassInfoTmpl::AddMember(const string& name,
                                       CMemberInfo* member)
{
    return AddMember(CMemberId(name), member);
}

CMemberInfo* CClassInfoTmpl::AddMember(const char* name,
                                       CMemberInfo* member)
{
    return AddMember(CMemberId(name), member);
}
*/

void CClassInfoTmpl::AddSubClass(const CMemberId& id,
                                 const CTypeRef& type)
{
    TSubClasses* subclasses = m_SubClasses.get();
    if ( !subclasses )
        m_SubClasses.reset(subclasses = new TSubClasses);
    subclasses->push_back(make_pair(id, type));
}

void CClassInfoTmpl::AddSubClass(const char* id, TTypeInfoGetter getter)
{
    AddSubClass(CMemberId(id), getter);
}

void CClassInfoTmpl::CollectExternalObjects(COObjectList& objectList,
                                            TConstObjectPtr object) const
{
    for ( TMemberIndex i = 0, size = m_MembersInfo.size(); i < size; ++i ) {
        const CMemberInfo& memberInfo = *m_MembersInfo[i];
        TTypeInfo memberTypeInfo = memberInfo.GetTypeInfo();
        TConstObjectPtr member = memberInfo.GetMember(object);
        memberTypeInfo->CollectExternalObjects(objectList, member);
    }
}

void CClassInfoTmpl::WriteData(CObjectOStream& out,
                               TConstObjectPtr object) const
{
    if ( Implicit() ) {
        // special case: class contains only one implicit member
        // we'll behave as this one member
        const CMemberInfo* info;
        if ( m_MembersInfo.size() == 2 ) {
            _ASSERT(m_Members.GetMemberId(0).GetName().empty());
            info = m_MembersInfo[1];
        }
        else {
            _ASSERT(m_MembersInfo.size() == 1);
            info = m_MembersInfo[0];
        }
        TConstObjectPtr member = info->GetMember(object);
        info->GetTypeInfo()->WriteData(out, member);
        return;
    }
    CObjectOStream::Block block(out, RandomOrder());
    CMemberId currentId;
    for ( TMemberIndex i = 0, size = m_MembersInfo.size(); i < size; ++i ) {
        const CMemberInfo& info = *m_MembersInfo[i];
        TConstObjectPtr member = info.GetMember(object);
        currentId.SetNext(m_Members.GetMemberId(i));
        if ( !info.Optional() ||
             !info.GetTypeInfo()->IsDefault(member) ) {
            block.Next();
            CObjectOStream::Member m(out, currentId);
            info.GetTypeInfo()->WriteData(out, member);
        }
    }
}

void CClassInfoTmpl::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    if ( Implicit() ) {
        // special case: class contains only one implicit member
        // we'll behave as this one member
        const CMemberInfo* info;
        if ( m_MembersInfo.size() == 2 ) {
            _ASSERT(m_Members.GetMemberId(0).GetName().empty());
            info = m_MembersInfo[1];
        }
        else {
            _ASSERT(m_MembersInfo.size() == 1);
            info = m_MembersInfo[0];
        }
        TObjectPtr member = info->GetMember(object);
        info->GetTypeInfo()->ReadData(in, member);
        return;
    }
    CObjectIStream::Block block(in, RandomOrder());
    if ( RandomOrder() ) {
        set<TMemberIndex> read;
        while ( block.Next() ) {
            CObjectIStream::Member memberId(in);
            TMemberIndex index = m_Members.FindMember(memberId);
            if ( index < 0 ) {
                ERR_POST("unknown member: " +
                         memberId.Id().ToString() + ", skipping");
                in.SkipValue();
                continue;
            }
            if ( !read.insert(index).second ) {
                ERR_POST("duplicated member: " +
                         memberId.Id().ToString() + ", skipping");
                in.SkipValue();
                continue;
            }
            const CMemberInfo& info = *m_MembersInfo[index];
            TObjectPtr member = info.GetMember(object);
            info.GetTypeInfo()->ReadData(in, member);
        }
        // init all absent members
        for ( TMemberIndex i = 0, size = m_MembersInfo.size(); i < size; ++i ) {
            if ( read.find(i) == read.end() ) {
                // check if this member have defaults
                const CMemberInfo& info = *m_MembersInfo[i];
                TObjectPtr member = info.GetMember(object);
                if ( info.Optional() ) {
                    // copy default
                    TConstObjectPtr def = info.GetDefault();
                    if ( def == 0 )
                        info.GetTypeInfo()->SetDefault(member);
                    else
                        info.GetTypeInfo()->Assign(member, info.GetDefault());
                }
                else {
                    // error: absent member w/o defult
                    THROW1_TRACE(runtime_error,
                                 "member " + m_Members.GetMemberId(i).ToString() +
                                 " is missing and doesn't have default");
                }
            }
        }
    }
    else {
        // sequential order
        TMemberIndex currentIndex = 0, size = m_MembersInfo.size();
        CMemberId currentId;
        while ( block.Next() ) {
            CObjectIStream::Member memberId(in);
            // find desired member
            const CMemberInfo* info;
            TObjectPtr member;
            for ( ;; ++currentIndex ) {
                if ( currentIndex == size ) {
                    THROW1_TRACE(runtime_error,
                                 "unexpected member: " + memberId.Id().ToString());
                }
                currentId.SetNext(m_Members.GetMemberId(currentIndex));
                info = m_MembersInfo[currentIndex];
                member = info->GetMember(object);
                if ( currentId == memberId )
                    break;

                if ( !info->Optional() ) {
                    THROW1_TRACE(runtime_error, "member " +
                                 currentId.ToString() + " expected");
                }
                TConstObjectPtr def = info->GetDefault();
                if ( def == 0 )
                    info->GetTypeInfo()->SetDefault(member);
                else
                    info->GetTypeInfo()->Assign(member, info->GetDefault());
            }
            ++currentIndex;
            info->GetTypeInfo()->ReadData(in, member);
        }
        for ( ; currentIndex != size; ++currentIndex ) {
            const CMemberInfo* info = m_MembersInfo[currentIndex];
            TObjectPtr member = info->GetMember(object);
            if ( !info->Optional() ) {
                THROW1_TRACE(runtime_error, "member " +
                             m_Members.GetMemberId(currentIndex).ToString() +
                             " expected");
            }
            TConstObjectPtr def = info->GetDefault();
            if ( def == 0 )
                info->GetTypeInfo()->SetDefault(member);
            else
                info->GetTypeInfo()->Assign(member, info->GetDefault());
        }
    }
}

size_t CClassInfoTmpl::GetFirstMemberOffset(void) const
{
    size_t offset = INT_MAX;
    for ( TMembersInfo::const_iterator i = m_MembersInfo.begin();
          i != m_MembersInfo.end();
          ++i ) {
        offset = min(offset, (*i)->GetOffset());
    }
    return offset;
}

const CClassInfoTmpl::TMembersByOffset&
CClassInfoTmpl::GetMembersByOffset(void) const
{
    TMembersByOffset* members = m_MembersByOffset.get();
    if ( !members ) {
        // create map
        m_MembersByOffset.reset(members = new TMembersByOffset);
        // fill map
        
        for ( TMemberIndex i = 0, size = m_MembersInfo.size();
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

const CMemberId* CClassInfoTmpl::GetMemberId(TMemberIndex index) const
{
    return &m_Members.GetMemberId(index);
}

const CMemberInfo* CClassInfoTmpl::GetMemberInfo(TMemberIndex index) const
{
    return m_MembersInfo[index];
}

CTypeInfo::TMemberIndex CClassInfoTmpl::FindMember(const string& name) const
{
    return m_Members.FindMember(name);
}

CTypeInfo::TMemberIndex CClassInfoTmpl::LocateMember(TConstObjectPtr object,
                                                     TConstObjectPtr member,
                                                     TTypeInfo memberTypeInfo) const
{
    _TRACE("LocateMember(" << unsigned(object) << ", " << unsigned(member) <<
           ", " << memberTypeInfo->GetName() << ")");
    TConstObjectPtr objectEnd = EndOf(object);
    TConstObjectPtr memberEnd = memberTypeInfo->EndOf(member);
    if ( member < object || memberEnd > objectEnd ) {
        return -1;
    }
    size_t memberOffset = Sub(member, object);
    size_t memberEndOffset = Sub(memberEnd, object);
    const TMembersByOffset& members = GetMembersByOffset();
    TMembersByOffset::const_iterator after =
        members.lower_bound(memberEndOffset);
    if ( after == members.begin() )
        return -1;

    TMemberIndex before = after == members.end()?
        members.rbegin()->second:
        (--after)->second;

    const CMemberInfo& memberInfo = *m_MembersInfo[before];
    if ( memberOffset < memberInfo.GetOffset() ||
         memberEndOffset > memberInfo.GetEndOffset() ) {
        return -1;
    }
    return before;
}

bool CClassInfoTmpl::IsDefault(TConstObjectPtr object) const
{
    for ( TMemberIndex i = 0, size = m_MembersInfo.size(); i < size; ++i ) {
        const CMemberInfo& member = *m_MembersInfo[i];
        if ( !member.GetTypeInfo()->IsDefault(member.GetMember(object)) )
            return false;
    }
    return true;
}

bool CClassInfoTmpl::Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
{
    for ( TMemberIndex i = 0, size = m_MembersInfo.size(); i < size; ++i ) {
        const CMemberInfo& member = *m_MembersInfo[i];
        if ( !member.GetTypeInfo()->Equals(member.GetMember(object1),
                                           member.GetMember(object2)) )
            return false;
    }
    return true;
}

void CClassInfoTmpl::SetDefault(TObjectPtr dst) const
{
    for ( TMemberIndex i = 0, size = m_MembersInfo.size(); i < size; ++i ) {
        const CMemberInfo& member = *m_MembersInfo[i];
        member.GetTypeInfo()->SetDefault(member.GetMember(dst));
    }
}

void CClassInfoTmpl::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    for ( TMemberIndex i = 0, size = m_MembersInfo.size(); i < size; ++i ) {
        const CMemberInfo& member = *m_MembersInfo[i];
        member.GetTypeInfo()->Assign(member.GetMember(dst),
                                     member.GetMember(src));
    }
}

const type_info* CClassInfoTmpl::GetCPlusPlusTypeInfo(TConstObjectPtr ) const
{
    return 0;
}

TObjectPtr CStructInfoTmpl::Create(void) const
{
    TObjectPtr object = calloc(GetSize(), 1);
    if ( object == 0 )
        THROW_TRACE(bad_alloc, ());
    _TRACE("Create: " << GetName() << ": " << unsigned(object));
    return object;
}

END_NCBI_SCOPE
