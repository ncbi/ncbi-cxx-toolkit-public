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
#include <serial/serialdef.hpp>
#include <serial/memberid.hpp>
#include <serial/member.hpp>
#include <serial/iteratorbase.hpp>
#include <vector>
#include <map>
#include <memory>

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
    typedef int TIndex;
    typedef vector<CMemberId> TMembers;
    typedef map<const char*, TIndex, StrCmp> TMembersByName;
    typedef map<TTag, TIndex> TMembersByTag;

    CMembers(void);
    ~CMembers(void);

    size_t GetSize(void) const
        {
            return m_Members.size();
        }
    bool Empty(void) const
        {
            return m_Members.empty();
        }

    const TMembers& GetMembers(void) const
        {
            return m_Members;
        }
    TMembers::const_iterator begin(void) const
        {
            return m_Members.begin();
        }
    TMembers::const_iterator end(void) const
        {
            return m_Members.end();
        }
    const TMembersByName& GetMembersByName(void) const;
    const TMembersByTag& GetMembersByTag(void) const;
    void UpdateMemberTags(void);

    void AddMember(const CMemberId& id);

    const CMemberId& GetMemberId(TIndex index) const
        {
            return m_Members[index];
        }

    TIndex FindMember(const CMemberId& id) const;
    TIndex FindMember(const string& name) const;
    TIndex FindMember(const char* name) const;
    TIndex FindMember(TTag tag) const;
    TIndex FindMember(const pair<const char*, size_t>& name, TIndex pos) const;
    TIndex FindMember(TTag tag, TIndex pos) const;

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
    typedef map<size_t, TIndex> TMembersByOffset;

    CMembersInfo(void);
    ~CMembersInfo(void);

    // AddMember will take ownership of member
    CMemberInfo* AddMember(const CMemberId& id, CMemberInfo* member);
    CMemberInfo* AddMember(const char* name, const void* member,
                           TTypeInfo type);
    CMemberInfo* AddMember(const char* name, const void* member,
                           const CTypeRef& type);

    const TMembersByOffset& GetMembersByOffset(void) const;
    size_t GetFirstMemberOffset(void) const;

    const CMemberInfo* GetMemberInfo(TIndex index) const
        {
            return m_MembersInfo[index];
        }

    bool MayContainType(TTypeInfo type) const;
    bool HaveChildren(TConstObjectPtr object) const;

    const CMemberInfo* GetMemberInfo(const CChildrenTypesIterator& cc) const;
    const CMemberInfo* GetMemberInfo(const CConstChildrenIterator& cc) const;
    const CMemberInfo* GetMemberInfo(const CChildrenIterator& cc) const;

    void BeginTypes(CChildrenTypesIterator& cc) const;
    void Begin(CConstChildrenIterator& cc) const;
    void Begin(CChildrenIterator& cc) const;
    bool ValidTypes(const CChildrenTypesIterator& cc) const;
    bool Valid(const CConstChildrenIterator& cc) const;
    bool Valid(const CChildrenIterator& cc) const;
    TTypeInfo GetChildType(const CChildrenTypesIterator& cc) const;
    void GetChild(const CConstChildrenIterator& cc,
                  CConstObjectInfo& child) const;
    void GetChild(const CChildrenIterator& cc,
                  CObjectInfo& child) const;
    void NextType(CChildrenTypesIterator& cc) const;
    void Next(CConstChildrenIterator& cc) const;
    void Next(CChildrenIterator& cc) const;

    //void Erase(CChildrenIterator& cc) const;

private:
    TMembersInfo m_MembersInfo;
    mutable auto_ptr<TMembersByOffset> m_MembersByOffset;
};

#include <serial/memberlist.inl>

END_NCBI_SCOPE

#endif  /* MEMBERLIST__HPP */
