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
#include <serial/classinfo.hpp>
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
    AddMember(CMemberInfo(offset, parent));
    _TRACE(ti.name());
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
        THROW1_TRACE(runtime_error, "duplicated members: " + member.GetName());

    m_Members[member.GetName()] = member;
    return this;
}

void CClassInfoTmpl::CollectExternalObjects(COObjectList& objectList,
                                            TConstObjectPtr object) const
{
    for ( TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        const CMemberInfo& memberInfo = i->second;
        TTypeInfo memberTypeInfo = memberInfo.GetTypeInfo();
        TConstObjectPtr member = memberInfo.GetMember(object);
        memberTypeInfo->CollectExternalObjects(objectList, member);
    }
}

void CClassInfoTmpl::WriteData(CObjectOStream& out,
                               TConstObjectPtr object) const
{
    CObjectOStream::VarBlock block(out);
    for ( TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        const CMemberInfo& memberInfo = i->second;
        TTypeInfo memberTypeInfo = memberInfo.GetTypeInfo();
        TConstObjectPtr member = memberInfo.GetMember(object);
        if ( !memberTypeInfo->IsDefault(member) ) {
            block.Next();
            out.WriteMemberName(memberInfo.GetName());
            memberTypeInfo->WriteData(out, member);
        }
    }
}

void CClassInfoTmpl::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    CObjectIStream::VarBlock block(in);
    while ( block.Next() ) {
        const string& memberName = in.ReadMemberName();
        TMembers::const_iterator i = m_Members.find(memberName);
        if ( i == m_Members.end() ) {
            ERR_POST("unknown member: "+memberName+", trying to skip...");
            in.SkipValue();
            continue;
        }
        const CMemberInfo& memberInfo = i->second;
        TTypeInfo memberTypeInfo = memberInfo.GetTypeInfo();
        TObjectPtr member = memberInfo.GetMember(object);
        memberTypeInfo->ReadData(in, member);
    }
}

const CMemberInfo* CClassInfoTmpl::FindMember(const string& name) const
{
    TMembers::const_iterator i = m_Members.find(name);
    if ( i == m_Members.end() )
        return 0;
    return &i->second;
}

const CMemberInfo* CClassInfoTmpl::LocateMember(TConstObjectPtr object,
                                                TConstObjectPtr member,
                                                TTypeInfo memberTypeInfo) const
{
    TConstObjectPtr objectEnd = EndOf(object);
    TConstObjectPtr memberEnd = memberTypeInfo->EndOf(member);
    if ( member < object || memberEnd > objectEnd ) {
        return 0;
    }
    size_t memberOffset = Sub(member, object);
    size_t memberEndOffset = Sub(memberEnd, object);
    for ( TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        if ( memberOffset >= i->second.GetOffset() &&
             memberEndOffset <= i->second.GetEndOffset() ) {
            return &i->second;
        }
    }
    return 0;
}

END_NCBI_SCOPE
