#ifndef MEMBERLIST__HPP
#define MEMBERLIST__HPP

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
* Revision 1.13  2000/06/16 16:31:05  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.12  2000/06/01 19:06:56  vasilche
* Added parsing of XML data.
*
* Revision 1.11  2000/05/24 20:08:12  vasilche
* Implemented XML dump.
*
* Revision 1.10  2000/04/10 21:01:39  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.9  2000/04/10 18:01:51  vasilche
* Added Erase() for STL types in type iterators.
*
* Revision 1.8  2000/03/29 15:55:20  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.7  2000/02/01 21:44:35  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.6  2000/01/10 19:46:31  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.5  1999/09/22 20:11:48  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.4  1999/09/14 18:54:03  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.3  1999/07/20 18:22:54  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.2  1999/07/01 17:55:18  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.1  1999/06/30 16:04:26  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/lightstr.hpp>
#include <serial/memberid.hpp>
#include <serial/member.hpp>
#include <vector>
#include <map>

BEGIN_NCBI_SCOPE

class CMemberId;
class CMemberInfo;
class CChildrenTypesIterator;
class CConstChildrenIterator;
class CChildrenIterator;
class CConstObjectInfo;
class CObjectInfo;

// This class supports sets of members with IDs
class CMembers {
public:
    typedef CMemberId::TTag TTag;
    typedef int TMemberIndex;
    typedef vector<CMemberId> TMembers;
    typedef map<CLightString, TMemberIndex> TMembersByName;
    typedef map<TTag, TMemberIndex> TMembersByTag;

    CMembers(void);
    ~CMembers(void);

    bool Empty(void) const
        {
            return m_Members.empty();
        }

    const TMembers& GetMembers(void) const
        {
            return m_Members;
        }

    const TMembersByName& GetMembersByName(void) const;
    const TMembersByTag& GetMembersByTag(void) const;
    void UpdateMemberTags(void);

    void AddMember(const CMemberId& id);

    TMemberIndex GetMembersCount(void) const
        {
            return GetMembers().size();
        }
    const CMemberId& GetMemberId(TMemberIndex index) const
        {
            _ASSERT(index >= 0 && index < GetMembersCount());
            return m_Members[index];
        }

    TMemberIndex FindMember(const CLightString& name) const;
    TMemberIndex FindMember(const CLightString& name, TMemberIndex pos) const;
    TMemberIndex FindMember(TTag tag) const;
    TMemberIndex FindMember(TTag tag, TMemberIndex pos) const;

private:
    TMembers m_Members;
    mutable auto_ptr<TMembersByName> m_MembersByName;
    mutable auto_ptr<TMembersByTag> m_MembersByTag;

    CMembers(const CMembers&);
    CMembers& operator=(const CMembers&);
};

class CMembersInfo : public CMembers
{
public:
    typedef vector<CMemberInfo*> TMembersInfo;
    typedef map<size_t, TMemberIndex> TMembersByOffset;

    CMembersInfo(void);
    ~CMembersInfo(void);

    // AddMember will take ownership of member
    CMemberInfo* AddMember(const CMemberId& id, CMemberInfo* member);
    CMemberInfo* AddMember(const CMemberId& id,
                           TConstObjectPtr member, TTypeInfo type);
    CMemberInfo* AddMember(const CMemberId& id,
                           TConstObjectPtr member, const CTypeRef& type);
    CMemberInfo* AddMember(const char* name, CMemberInfo* member);
    CMemberInfo* AddMember(const char* name,
                           TConstObjectPtr member, TTypeInfo type);
    CMemberInfo* AddMember(const char* name,
                           TConstObjectPtr member, const CTypeRef& type);

    const TMembersByOffset& GetMembersByOffset(void) const;
    size_t GetFirstMemberOffset(void) const;

    const CMemberInfo* GetMemberInfo(TMemberIndex index) const
        {
            _ASSERT(index >= 0 && index < GetMembersCount());
            return m_MembersInfo[index];
        }

private:
    TMembersInfo m_MembersInfo;
    mutable auto_ptr<TMembersByOffset> m_MembersByOffset;
};

//#include <serial/memberlist.inl>

END_NCBI_SCOPE

#endif  /* MEMBERLIST__HPP */
