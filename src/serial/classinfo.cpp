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

CClassInfoTmpl::CClassInfoTmpl(const string& name, const type_info& id,
                               size_t size, bool randomOrder)
    : CParent(name), m_Id(id), m_Size(size), m_RandomOrder(randomOrder)
{
    Register();
}

CClassInfoTmpl::CClassInfoTmpl(const type_info& id, size_t size,
                               bool randomOrder)
    : CParent(id.name()), m_Id(id), m_Size(size), m_RandomOrder(randomOrder)
{
    Register();
}

CClassInfoTmpl::CClassInfoTmpl(const type_info& id, size_t size,
                               bool randomOrder,
                               const CTypeRef& parent, size_t offset)
    : CParent(id.name()), m_Id(id), m_Size(size), m_RandomOrder(randomOrder)
{
    Register();
    AddMember(NcbiEmptyString, new CRealMemberInfo(offset, parent));
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

TTypeInfo CClassInfoTmpl::GetClassInfoById(const type_info& id)
{
    TClassesById& types = ClassesById();
    TClassesById::iterator i = types.find(&id);
    if ( i == types.end() ) {
        THROW1_TRACE(runtime_error, "class not found: "+string(id.name()));
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

size_t CClassInfoTmpl::GetSize(void) const
{
    return m_Size;
}

CMemberInfo* CClassInfoTmpl::AddMember(const CMemberId& id, CMemberInfo* member)
{
    m_MembersByOffset.reset(0);
    m_Members.AddMember(id);
    m_MembersInfo.push_back(member);
    return member;
}

CMemberInfo* CClassInfoTmpl::AddMember(const string& name, CMemberInfo* member)
{
    return AddMember(CMemberId(name), member);
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
    CObjectOStream::Block block(out, RandomOrder());
    CMemberId currentId;
    for ( TMemberIndex i = 0, size = m_MembersInfo.size(); i < size; ++i ) {
        const CMemberInfo& memberInfo = *m_MembersInfo[i];
        TTypeInfo memberTypeInfo = memberInfo.GetTypeInfo();
        TConstObjectPtr member = memberInfo.GetMember(object);
        TConstObjectPtr def = memberInfo.GetDefault();
        currentId.SetNext(m_Members.GetMemberId(i));
        if ( !def || !memberTypeInfo->Equals(member, def) ) {
            block.Next();
            CObjectOStream::Member m(out, currentId);
            memberTypeInfo->WriteData(out, member);
        }
    }
}

void CClassInfoTmpl::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    CObjectIStream::Block block(in, RandomOrder());
    if ( RandomOrder() ) {
        set<TMemberIndex> read;
        while ( block.Next() ) {
            CObjectIStream::Member memberId(in);
            TMemberIndex index = m_Members.FindMember(memberId);
            if ( index < 0 ) {
                ERR_POST("unknown member: " +
                         memberId.ToString() + ", skipping");
                in.SkipValue();
                continue;
            }
            if ( !read.insert(index).second ) {
                ERR_POST("duplicated member: " +
                         memberId.ToString() + ", skipping");
                in.SkipValue();
                continue;
            }
            const CMemberInfo& memberInfo = *m_MembersInfo[index];
            TTypeInfo memberTypeInfo = memberInfo.GetTypeInfo();
            TObjectPtr member = memberInfo.GetMember(object);
            memberTypeInfo->ReadData(in, member);
        }
        // init all absent members
        for ( TMemberIndex i = 0, size = m_MembersInfo.size(); i < size; ++i ) {
            if ( read.find(i) == read.end() ) {
                // check if this member have defaults
                const CMemberInfo& memberInfo = *m_MembersInfo[i];
                TConstObjectPtr def = memberInfo.GetDefault();
                if ( def != 0 ) {
                    // copy defult
                    memberInfo.GetTypeInfo()->
                        Assign(memberInfo.GetMember(object), def);
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
            const CMemberInfo* memberInfo;
            TTypeInfo typeInfo;
            TObjectPtr member;
            for ( ;; ++currentIndex ) {
                if ( currentIndex == size ) {
                    THROW1_TRACE(runtime_error,
                                 "unexpected member: " + memberId.ToString());
                }
                currentId.SetNext(m_Members.GetMemberId(currentIndex));
                memberInfo = m_MembersInfo[currentIndex];
                typeInfo = memberInfo->GetTypeInfo();
                member = memberInfo->GetMember(object);
                if ( currentId == memberId )
                    break;

                TConstObjectPtr def = memberInfo->GetDefault();
                if ( def == 0 ) {
                    THROW1_TRACE(runtime_error, "member " +
                                 currentId.ToString() + " expected");
                }
                typeInfo->Assign(member, def);
            }
            ++currentIndex;
            typeInfo->ReadData(in, member);
        }
        for ( ; currentIndex != size; ++currentIndex ) {
            const CMemberInfo* memberInfo = m_MembersInfo[currentIndex];
            TTypeInfo typeInfo = memberInfo->GetTypeInfo();
            TObjectPtr member = memberInfo->GetMember(object);
            TConstObjectPtr def = memberInfo->GetDefault();
            if ( def == 0 ) {
                THROW1_TRACE(runtime_error, "member " +
                             m_Members.GetMemberId(currentIndex).ToString() +
                             " expected");
            }
            typeInfo->Assign(member, def);
        }
    }
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

void CClassInfoTmpl::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    for ( TMemberIndex i = 0, size = m_MembersInfo.size(); i < size; ++i ) {
        const CMemberInfo& member = *m_MembersInfo[i];
        member.GetTypeInfo()->Assign(member.GetMember(dst),
                                     member.GetMember(src));
    }
}

TObjectPtr CStructInfoTmpl::Create(void) const
{
    return calloc(GetSize(), 1);
}

END_NCBI_SCOPE
