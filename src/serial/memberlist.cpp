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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/exception.hpp>
#include <serial/impl/memberlist.hpp>
#include <serial/impl/memberid.hpp>
#include <serial/impl/member.hpp>
#include <serial/impl/classinfob.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/impl/ptrinfo.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbithr.hpp>

BEGIN_NCBI_SCOPE

CItemsInfo::CItemsInfo(void)
    : m_ZeroTagIndex(kInvalidMember)
{
}

CItemsInfo::~CItemsInfo(void)
{
// NOTE:  This compiler bug was fixed by Jan 24 2002, test passed with:
//           CC: Sun WorkShop 6 update 2 C++ 5.3 Patch 111685-03 2001/10/19
//        We leave the workaround here for maybe half a year (for other guys).
#if defined(NCBI_COMPILER_WORKSHOP)
// We have to use two #if's here because KAI C++ cannot handle #if foo == bar
#  if (NCBI_COMPILER_VERSION == 530)
    // BW_010::  to workaround (already reported to SUN, CASE ID 62563729)
    //           internal bug of the SUN Forte 6 Update 1 and Update 2 compiler
    (void) atoi("5");
#  endif
#endif
}

void CItemsInfo::AddItem(CItemInfo* item)
{
    // clear cached maps (byname and bytag)
    m_ItemsByName.reset(0);
    m_ZeroTagIndex = kInvalidMember;
    m_ItemsByTag.reset(0);
    m_ItemsByOffset.reset(0);

    // add item
    m_Items.push_back(AutoPtr<CItemInfo>(item));
    item->m_Index = LastIndex();
}

void CItemsInfo::AssignItemsTags(CAsnBinaryDefs::ETagType containerType)
{
    if (containerType != CAsnBinaryDefs::eAutomatic) {
        NON_CONST_ITERATE( TItems, it, m_Items) {
            CItemInfo* item = it->get();
            if ( item->GetId().HasTag() && item->GetId().IsTagImplicit()) {
                item->GetId().m_TagConstructed = item->GetTypeInfo()->GetTagConstructed();
            }
        }
        return;
    }
    TTag tag = CMemberId::eFirstTag;
    NON_CONST_ITERATE( TItems, it, m_Items) {
        CItemInfo* item = it->get();
        // update item's tag
        if ( item->GetId().HaveParentTag()) {
            continue;
        }
        if ( item->GetId().HasTag()) {
            tag = item->GetId().GetTag() + 1;
            continue;
        }
        item->GetId().SetTag(tag++);
    }
 }

DEFINE_STATIC_FAST_MUTEX(s_ItemsMapMutex);

const CItemsInfo::TItemsByName& CItemsInfo::GetItemsByName(void) const
{
    TItemsByName* items = m_ItemsByName.get();
    if ( !items ) {
        CFastMutexGuard GUARD(s_ItemsMapMutex);
        items = m_ItemsByName.get();
        if ( !items ) {
            auto_ptr<TItemsByName> keep(items = new TItemsByName);
            for ( CIterator i(*this); i.Valid(); ++i ) {
                const CItemInfo* itemInfo = GetItemInfo(i);
                const string& name = itemInfo->GetId().GetName();
                if ( !items->insert(TItemsByName::value_type(name, *i)).second ) {
                    if ( !name.empty() )
                        NCBI_THROW(CSerialException,eInvalidData,
                            string("duplicate member name: ")+name);
                }
            }
            m_ItemsByName = keep;
        }
    }
    return *items;
}

const CItemsInfo::TItemsByOffset&
CItemsInfo::GetItemsByOffset(void) const
{
    TItemsByOffset* items = m_ItemsByOffset.get();
    if ( !items ) {
        CFastMutexGuard GUARD(s_ItemsMapMutex);
        items = m_ItemsByOffset.get();
        if ( !items ) {
            // create map
            auto_ptr<TItemsByOffset> keep(items = new TItemsByOffset);
            // fill map 
            for ( CIterator i(*this); i.Valid(); ++i ) {
                const CItemInfo* itemInfo = GetItemInfo(i);
                size_t offset = itemInfo->GetOffset();
                if ( !items->insert(TItemsByOffset::value_type(offset, *i)).second ) {
                    NCBI_THROW(CSerialException,eInvalidData, "conflict member offset");
                }
            }
/*
        // check overlaps
        size_t nextOffset = 0;
        for ( TItemsByOffset::const_iterator m = members->begin();
              m != members->end(); ++m ) {
            size_t offset = m->first;
            if ( offset < nextOffset ) {
                NCBI_THROW(CSerialException,eInvalidData,
                             "overlapping members");
            }
            nextOffset = offset + m_Members[m->second]->GetSize();
        }
*/
            m_ItemsByOffset = keep;
        }
    }
    return *items;
}

CItemsInfo::TTagAndClass
CItemsInfo::GetTagAndClass(const CItemsInfo::CIterator& i) const
{
    const CItemInfo* itemInfo = GetItemInfo(i);
    TTag tag = itemInfo->GetId().GetTag();
    CAsnBinaryDefs::ETagClass tagclass = itemInfo->GetId().GetTagClass();
    if (!itemInfo->GetId().HasTag()) {
        TTypeInfo itemType = itemInfo->GetTypeInfo();
        while (!itemType->HasTag() && itemType->GetTypeFamily() == eTypeFamilyPointer) {
            const CPointerTypeInfo* ptr =
                dynamic_cast<const CPointerTypeInfo*>(itemType);
            if (ptr) {
                itemType = ptr->GetPointedType();
            } else {
                NCBI_THROW(CSerialException,eInvalidData,
                    string("invalid type info: ") + itemInfo->GetId().GetName());
            }
        }

        if (itemType->HasTag()) {
            tag = itemType->GetTag();
            tagclass = itemType->GetTagClass();
        }
    }
    return make_pair(tag, tagclass);
}

pair<TMemberIndex, const CItemsInfo::TItemsByTag*>
CItemsInfo::GetItemsByTagInfo(void) const
{
    typedef pair<TMemberIndex, const TItemsByTag*> TReturn;
    TReturn ret(m_ZeroTagIndex, m_ItemsByTag.get());
    if ( ret.first == kInvalidMember && ret.second == 0 ) {
        CFastMutexGuard GUARD(s_ItemsMapMutex);
        ret = TReturn(m_ZeroTagIndex, m_ItemsByTag.get());
        if ( ret.first == kInvalidMember && ret.second == 0 ) {
            {
                CIterator i(*this);
                if ( i.Valid() ) {
                    if (GetItemInfo(i)->GetId().HasTag() &&
                        GetItemInfo(i)->GetId().GetTagClass() == CAsnBinaryDefs::eContextSpecific) {
                        ret.first = *i - GetItemInfo(i)->GetId().GetTag();
                        for ( ++i; i.Valid(); ++i ) {
                            if ( ret.first != *i - GetItemInfo(i)->GetId().GetTag() ||
                                 GetItemInfo(i)->GetId().GetTagClass() != CAsnBinaryDefs::eContextSpecific) {
                                ret.first = kInvalidMember;
                                break;
                            }
                        }
                    }
                }
            }
            if ( ret.first != kInvalidMember ) {
                m_ZeroTagIndex = ret.first;
            }
            else {
                auto_ptr<TItemsByTag> items(new TItemsByTag);
                for ( CIterator i(*this); i.Valid(); ++i ) {
                    TTagAndClass tc = GetTagAndClass(i);
                    if (tc.first >= 0) {
                        if ( !items->insert(TItemsByTag::value_type( tc, *i)).second &&
                            GetItemInfo(i)->GetId().HasTag() ) {
                            NCBI_THROW(CSerialException,eInvalidData, "duplicate member tag");
                        }
                    }
                }
                ret.second = items.get();
                m_ItemsByTag = items;
            }
        }
    }
    return ret;
}

TMemberIndex CItemsInfo::Find(const CTempString& name) const
{
    const TItemsByName& items = GetItemsByName();
    TItemsByName::const_iterator i = items.find(name);
    if ( i == items.end() )
        return kInvalidMember;
    return i->second;
}

TMemberIndex CItemsInfo::FindDeep(const CTempString& name, bool search_attlist) const
{
    TMemberIndex ind = Find(name);
    if (ind != kInvalidMember) {
        return ind;
    }
    for (CIterator item(*this); item.Valid(); ++item) {
        const CItemInfo* info = GetItemInfo(item);
        const CMemberId& id = info->GetId();
        if ((!id.IsAttlist() && id.HasNotag()) ||
            ( id.IsAttlist() && search_attlist)) {
            const CClassTypeInfoBase* classType =
                dynamic_cast<const CClassTypeInfoBase*>(
                    FindRealTypeInfo(info->GetTypeInfo()));
            if (classType) {
                if (classType->GetItems().FindDeep(name, search_attlist)
                    != kInvalidMember) {
                    return *item;
                }
            }
        }
    }
    return kInvalidMember;
}

const CTypeInfo* CItemsInfo::FindRealTypeInfo(const CTypeInfo* info)
{
    const CTypeInfo* type;
    for (type = info;;) {
        if (type->GetTypeFamily() == eTypeFamilyContainer) {
            const CContainerTypeInfo* cont =
                dynamic_cast<const CContainerTypeInfo*>(type);
            if (cont) {
                type = cont->GetElementType();
            }
        } else if (type->GetTypeFamily() == eTypeFamilyPointer) {
            const CPointerTypeInfo* ptr =
                dynamic_cast<const CPointerTypeInfo*>(type);
            if (ptr) {
                type = ptr->GetPointedType();
            }
        } else {
            break;
        }
    }
    return type;
}

const CItemInfo* CItemsInfo::FindNextMandatory(const CItemInfo* info)
{
    if (!info->GetId().HasNotag()) {
        const CMemberInfo* mem = dynamic_cast<const CMemberInfo*>(info);
        if (mem && mem->Optional()) {
            return 0;
        }
        return info;
    }
    return FindNextMandatory(info->GetTypeInfo());
}

const CItemInfo* CItemsInfo::FindNextMandatory(const CTypeInfo* info)
{
    const CItemInfo* found = 0;
    TTypeInfo type = FindRealTypeInfo(info);
    ETypeFamily family = type->GetTypeFamily();
    if (family == eTypeFamilyClass || family == eTypeFamilyChoice) {
        const CClassTypeInfoBase* classType =
            dynamic_cast<const CClassTypeInfoBase*>(type);
        _ASSERT(classType);
        const CItemsInfo& items = classType->GetItems();
        TMemberIndex i;
        const CItemInfo* found_first = 0;
        for (i = items.FirstIndex(); i <= items.LastIndex(); ++i) {

            const CItemInfo* item = classType->GetItems().GetItemInfo(i);
            ETypeFamily item_family = item->GetTypeInfo()->GetTypeFamily();
            if (item_family == eTypeFamilyPointer) {
                const CPointerTypeInfo* ptr =
                    dynamic_cast<const CPointerTypeInfo*>(item->GetTypeInfo());
                if (ptr) {
                    item_family = ptr->GetPointedType()->GetTypeFamily();
                }
            }
            if (item_family == eTypeFamilyContainer) {
                if (item->NonEmpty()) {
                    found = FindNextMandatory( item );
                }
            } else {
                found = FindNextMandatory( item );
            }
            if (family == eTypeFamilyClass) {
                if (found) {
                    return found;
                }
            } else {
                if (!found) {
                    // this is optional choice variant
                    return 0;
                }
                if (!found_first) {
                    found_first = found;
                }
            }
        }
        return found_first;
    }
    return found;
}

TMemberIndex CItemsInfo::FindEmpty(void) const
{
    for (CIterator item(*this); item.Valid(); ++item) {
        const CItemInfo* info = GetItemInfo(item);
        if (info->GetId().IsAttlist()) {
            continue;
        }
        const CTypeInfo* type;
        for (type = info->GetTypeInfo();;) {
            if (type->GetTypeFamily() == eTypeFamilyContainer) {
                // container may be empty
                return *item;
            } else if (type->GetTypeFamily() == eTypeFamilyPointer) {
                const CPointerTypeInfo* ptr =
                    dynamic_cast<const CPointerTypeInfo*>(type);
                if (ptr) {
                    type = ptr->GetPointedType();
                }
            } else {
                break;
            }
        }
    }
    return kInvalidMember;
}

TMemberIndex CItemsInfo::Find(const CTempString& name, TMemberIndex pos) const
{
    for ( CIterator i(*this, pos); i.Valid(); ++i ) {
        if ( name == GetItemInfo(i)->GetId().GetName() )
            return *i;
    }
    return kInvalidMember;
}

TMemberIndex CItemsInfo::Find(TTag tag, CAsnBinaryDefs::ETagClass tagclass) const
{
    TMemberIndex zero_index = m_ZeroTagIndex;
    if ( zero_index == kInvalidMember && !m_ItemsByTag.get() ) {
        zero_index = GetItemsByTagInfo().first;
    }
    if ( zero_index != kInvalidMember ) {
        TMemberIndex index = tag + zero_index;
        if ( index < FirstIndex() || index > LastIndex() )
            return kInvalidMember;
        return index;
    }
    else {
        TItemsByTag::const_iterator mi = m_ItemsByTag->find( make_pair(tag,tagclass));
        if ( mi == m_ItemsByTag->end() )
            return kInvalidMember;
        return mi->second;
    }
}

TMemberIndex CItemsInfo::Find(TTag tag, CAsnBinaryDefs::ETagClass tagclass, TMemberIndex pos) const
{
    TMemberIndex zero_index = m_ZeroTagIndex;
    if ( zero_index == kInvalidMember && !m_ItemsByTag.get() ) {
        zero_index = GetItemsByTagInfo().first;
    }
    if ( zero_index != kInvalidMember ) {
        TMemberIndex index = tag + zero_index;
        if ( index < pos || index > LastIndex() )
            return kInvalidMember;
        return index;
    }
    else {
        for ( CIterator i(*this, pos); i.Valid(); ++i ) {
            TTagAndClass tc = GetTagAndClass(i);
            if (tc.first == tag && tc.second == tagclass) {
                return *i;
            }
        }
        if (pos <= LastIndex()) {
            const CItemInfo* info = GetItemInfo(pos);
            if (!info->GetId().HasTag()) {
                const CMemberInfo* mem = dynamic_cast<const CMemberInfo*>(info);
                if (mem && !mem->Optional()) {
                    return pos;
                }
            }
        }
        return kInvalidMember;
    }
}


END_NCBI_SCOPE
