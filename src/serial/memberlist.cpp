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
* Revision 1.20  2000/09/26 17:38:21  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.19  2000/09/18 20:00:22  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.18  2000/09/01 13:16:15  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.17  2000/08/15 19:44:47  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.16  2000/07/03 18:42:44  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.15  2000/06/16 16:31:19  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.14  2000/06/07 19:45:58  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.13  2000/06/01 19:07:03  vasilche
* Added parsing of XML data.
*
* Revision 1.12  2000/05/24 20:08:47  vasilche
* Implemented XML dump.
*
* Revision 1.11  2000/04/10 21:01:49  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.10  2000/04/10 18:01:57  vasilche
* Added Erase() for STL types in type iterators.
*
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
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

CMembersInfo::CMembersInfo(void)
    : m_ZeroTagIndex(kInvalidMember)
{
}

CMembersInfo::~CMembersInfo(void)
{
}

void CMembersInfo::AddItem(CItemInfo* item)
{
    // clear cached maps (byname and bytag)
    m_MembersByName.reset(0);
    m_ZeroTagIndex = kInvalidMember;
    m_MembersByTag.reset(0);
    m_MembersByOffset.reset(0);
    m_Items.push_back(AutoPtr<CItemInfo>(item));
    item->m_Id.m_MemberList = this;
    item->m_Index = LastIndex();
}

size_t CMembersInfo::GetFirstItemOffset(void) const
{
    size_t offset = INT_MAX;
    iterate ( TItems, i, m_Items ) {
        offset = min(offset, (*i)->GetOffset());
    }
    return offset;
}

const CMembersInfo::TMembersByName& CMembersInfo::GetMembersByName(void) const
{
    TMembersByName* members = m_MembersByName.get();
    if ( !members ) {
        m_MembersByName.reset(members = new TMembersByName);
        // TMembers is vector so we'll use index access instead iterator
        // because we need index value to inser in map too
        TMemberIndex lastIndex = LastIndex();
        for ( TMemberIndex index = FirstIndex();
              index <= lastIndex; ++index ) {
            const string& name = GetItemInfo(index)->GetId().GetName();
            if ( !members->insert(TMembersByName::value_type(name,
                                                             index)).second ) {
                if ( !name.empty() )
                    THROW1_TRACE(runtime_error, "duplicated member name");
            }
        }
    }
    return *members;
}

const CMembersInfo::TMembersByOffset&
CMembersInfo::GetMembersByOffset(void) const
{
    TMembersByOffset* members = m_MembersByOffset.get();
    if ( !members ) {
        // create map
        m_MembersByOffset.reset(members = new TMembersByOffset);
        // fill map
        
        TMemberIndex lastIndex = LastIndex();
        for ( TMemberIndex index = FirstIndex();
              index <= lastIndex; ++index ) {
            const CItemInfo* memberInfo = GetItemInfo(index);
            size_t offset = memberInfo->GetOffset();
            if ( !members->insert(TMembersByOffset::value_type(offset, index)).second ) {
                THROW1_TRACE(runtime_error, "conflict member offset");
            }
        }
/*
        // check overlaps
        size_t nextOffset = 0;
        for ( TMembersByOffset::const_iterator m = members->begin();
              m != members->end(); ++m ) {
            size_t offset = m->first;
            if ( offset < nextOffset ) {
                THROW1_TRACE(runtime_error,
                             "overlapping members");
            }
            nextOffset = offset + m_Members[m->second]->GetSize();
        }
*/
    }
    return *members;
}

void CMembersInfo::UpdateTags(void) const
{
    m_ZeroTagIndex = kInvalidMember;
    m_MembersByTag.reset(0);

    TMemberIndex zeroTagIndex = kInvalidMember;

    TTag nextTag = CMemberId::eFirstTag;
    TMemberIndex lastIndex = LastIndex();
    for ( TMemberIndex index = FirstIndex();
          index <= lastIndex; ++index ) {
        CMemberId& id = x_GetItemInfo(index)->m_Id;
        TTag tag = id.GetExplicitTag();
        if ( tag == CMemberId::eNoExplicitTag ) {
            if ( index == FirstIndex() && id.GetName().empty() ) {
                // parent class
                tag = CMemberId::eParentTag - 1;
            }
            else {
                tag = nextTag;
            }
        }
        if ( zeroTagIndex == kInvalidMember ) {
            if ( index == FirstIndex() )
                zeroTagIndex = index - tag;
        }
        else {
            if ( zeroTagIndex != index - tag )
                zeroTagIndex = kInvalidMember;
        }
        id.m_Tag = tag;
        nextTag = tag + 1;
    }

    m_ZeroTagIndex = zeroTagIndex;
}

TMemberIndex CMembersInfo::Find(const CLightString& name) const
{
    const TMembersByName& members = GetMembersByName();
    TMembersByName::const_iterator i = members.find(name);
    if ( i == members.end() )
        return kInvalidMember;
    return i->second;
}

TMemberIndex CMembersInfo::Find(const CLightString& name,
                                TMemberIndex pos) const
{
    TMemberIndex lastIndex = LastIndex();
    for ( TMemberIndex index = pos; index <= lastIndex; ++index ) {
        const CMemberId& id = GetItemInfo(index)->GetId();
        if ( name == id.GetName() )
            return index;
    }
    return kInvalidMember;
}

TMemberIndex CMembersInfo::Find(TTag tag) const
{
    if ( m_ZeroTagIndex == kInvalidMember && !m_MembersByTag.get() )
        UpdateTags();

    if ( m_ZeroTagIndex != kInvalidMember ) {
        TMemberIndex index = tag + m_ZeroTagIndex;
        if ( index < FirstIndex() || index > LastIndex() )
            return kInvalidMember;
        return index;
    }

    TMembersByTag* members = m_MembersByTag.get();
    if ( !members ) {
        m_MembersByTag.reset(members = new TMembersByTag);
        TTag nextTag = CMemberId::eFirstTag;
        TMemberIndex lastIndex = LastIndex();
        for ( TMemberIndex index = FirstIndex();
              index <= lastIndex; ++index ) {
            const CMemberId& id = GetItemInfo(index)->GetId();
            TTag currentTag = id.GetTag();
            if ( currentTag == CMemberId::eNoExplicitTag ) {
                if ( index == FirstIndex() && id.GetName().empty() ) {
                    // parent class - skip it
                    currentTag = CMemberId::eParentTag - 1;
                }
                else {
                    currentTag = nextTag;
                }
            }
            if ( !members->insert(TMembersByTag::value_type(currentTag,
                                                            index)).second ) {
                THROW1_TRACE(runtime_error, "duplicated member tag");
            }
            nextTag = currentTag + 1;
        }
    }

    TMembersByTag::const_iterator mi = members->find(tag);
    if ( mi == members->end() )
        return kInvalidMember;
    return mi->second;
}

TMemberIndex CMembersInfo::Find(TTag tag, TMemberIndex pos) const
{
    TMemberIndex lastIndex = LastIndex();
    for ( TMemberIndex index = pos; index <= lastIndex; ++index ) {
        const CMemberId& id = GetItemInfo(index)->GetId();
        if ( id.GetTag() == tag )
            return index;
    }
    return kInvalidMember;
}

END_NCBI_SCOPE
