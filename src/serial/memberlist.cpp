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
    : m_ItemsByName(0),
      m_ZeroTagIndex(kInvalidMember),
      m_ItemsByTag(0),
      m_ItemsByOffset(0)
{
}

CItemsInfo::~CItemsInfo(void)
{
    ClearIndexes();
}

void CItemsInfo::ClearIndexes()
{
    // clear cached maps (byname and bytag)
    delete m_ItemsByName.exchange(nullptr);
    m_ZeroTagIndex = kInvalidMember;
    delete m_ItemsByTag.exchange(nullptr);
    delete m_ItemsByOffset.exchange(nullptr);

}

void CItemsInfo::AddItem(CItemInfo* item)
{
    ClearIndexes();
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

void CItemsInfo::DataSpec(EDataSpec spec)
{
    if (spec != EDataSpec::eASN) {
        for (auto& i : m_Items) {
            i->GetId().SetNoPrefix();
            i->UpdateFunctions();
        }
    }
}

DEFINE_STATIC_FAST_MUTEX(s_ItemsMapMutex);

const CItemsInfo::TItemsByName& CItemsInfo::GetItemsByName(void) const
{
    TItemsByName* items = m_ItemsByName.load(memory_order_acquire);
    if ( !items ) {
        CFastMutexGuard GUARD(s_ItemsMapMutex);
        items = m_ItemsByName.load(memory_order_acquire);
        if ( !items ) {
            unique_ptr<TItemsByName> keep = make_unique<TItemsByName>();
            items = keep.get();
            for ( CIterator i(*this); i.Valid(); ++i ) {
                const CItemInfo* itemInfo = GetItemInfo(i);
                const string& name = itemInfo->GetId().GetName();
                if ( !items->insert(TItemsByName::value_type(name, *i)).second ) {
                    if ( !name.empty() )
                        NCBI_THROW(CSerialException,eInvalidData,
                            string("duplicate member name: ")+name);
                }
            }
            m_ItemsByName.store(items, memory_order_release);
            keep.release();
        }
    }
    return *items;
}

const CItemsInfo::TItemsByOffset&
CItemsInfo::GetItemsByOffset(void) const
{
    TItemsByOffset* items = m_ItemsByOffset.load(memory_order_acquire);
    if ( !items ) {
        CFastMutexGuard GUARD(s_ItemsMapMutex);
        items = m_ItemsByOffset.load(memory_order_acquire);
        if ( !items ) {
            // create map
            unique_ptr<TItemsByOffset> keep = make_unique<TItemsByOffset>();
            items = keep.get();
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
            m_ItemsByOffset.store(items, memory_order_release);
            keep.release();
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
    TReturn ret(m_ZeroTagIndex.load(memory_order_acquire),
                m_ItemsByTag.load(memory_order_acquire));
    if ( ret.first == kInvalidMember && ret.second == 0 ) {
        CFastMutexGuard GUARD(s_ItemsMapMutex);
        ret = TReturn(m_ZeroTagIndex.load(memory_order_acquire),
                      m_ItemsByTag.load(memory_order_acquire));
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
                m_ZeroTagIndex.store(ret.first, memory_order_release);
            }
            else {
                unique_ptr<TItemsByTag> items = make_unique<TItemsByTag>();
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
                m_ItemsByTag.store(items.release(), memory_order_release);
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

TMemberIndex CItemsInfo::FindDeep(const CTempString& name, bool search_attlist,
    const CClassTypeInfoBase** pclassInfo) const
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
                    if (pclassInfo) {
                        *pclassInfo = classType;
                    }
                    return *item;
                }
            }
        }
    }
    return kInvalidMember;
}

TMemberIndex CItemsInfo::FindDeep(const CTempString& name, TMemberIndex pos) const
{
    TMemberIndex ind = Find(name, pos);
    if (ind != kInvalidMember) {
        return ind;
    }
    for (CIterator item(*this, pos); item.Valid(); ++item) {
        const CItemInfo* info = GetItemInfo(item);
        const CClassTypeInfoBase* classType =
            dynamic_cast<const CClassTypeInfoBase*>(
                FindRealTypeInfo(info->GetTypeInfo()));
        if (classType) {
            if (classType->GetItems().FindDeep(name)
                != kInvalidMember) {
                return *item;
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
    if (!info->GetId().HasNotag() && !info->GetId().IsAttlist()) {
        return info->Optional() ? 0 : info;
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
            if (item->Optional()) {
                if (family == eTypeFamilyChoice) {
                    return 0;
                }
                continue;
            }
            if (!item->GetId().HasNotag()) {
                return item;
            }
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
            } else if (item_family == eTypeFamilyPrimitive) {
                found = item->Optional() ? 0 : item;
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
        if (info->NonEmpty() || info->GetId().IsAttlist()) {
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
    TMemberIndex zero_index = m_ZeroTagIndex.load(memory_order_acquire);
    const TItemsByTag* items_by_tag = m_ItemsByTag.load(memory_order_acquire);
    if ( zero_index == kInvalidMember && !items_by_tag ) {
        tie(zero_index, items_by_tag) = GetItemsByTagInfo();
    }
    if ( zero_index != kInvalidMember ) {
        TMemberIndex index = tag + zero_index;
        if ( index < FirstIndex() || index > LastIndex() )
            return kInvalidMember;
        return index;
    }
    else {
        TItemsByTag::const_iterator mi = items_by_tag->find( make_pair(tag,tagclass));
        if ( mi == items_by_tag->end() )
            return kInvalidMember;
        return mi->second;
    }
}

TMemberIndex CItemsInfo::Find(TTag tag, CAsnBinaryDefs::ETagClass tagclass, TMemberIndex pos) const
{
    TMemberIndex zero_index = m_ZeroTagIndex;
    if ( zero_index == kInvalidMember && !m_ItemsByTag.load(memory_order_acquire) ) {
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
