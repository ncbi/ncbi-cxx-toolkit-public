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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/exception.hpp>
#include <serial/objectiter.hpp>
#include <serial/iterator.hpp>
#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE

struct SIteratorFunctions {
    static bool s_ContainsType(const CConstObjectInfo& object,
                               TTypeInfo needType);
};

class CTreeLevelIteratorOne : public CTreeLevelIterator
{
public:
    CTreeLevelIteratorOne(const CObjectInfo& object)
        : m_Object(object), m_ItemInfo(0)
        {
        }

    bool Valid(void) const
        {
            return m_Object;
        }
    void Next(void)
        {
            m_Object.Reset();
        }
    CObjectInfo Get(void) const
        {
            return m_Object;
        }
    const CItemInfo* GetItemInfo(void) const
        {
            return m_ItemInfo;
        }
    TMemberIndex GetIndex(void) const
        {
            return m_ItemInfo ? m_ItemInfo->GetIndex() : kInvalidMember;
        }
protected:
    void SetItemInfo(const CItemInfo* info)
        {
            m_ItemInfo = info;
        }
private:
    CObjectInfo m_Object;
    const CItemInfo* m_ItemInfo;
};

class CConstTreeLevelIteratorOne : public CConstTreeLevelIterator
{
public:
    CConstTreeLevelIteratorOne(const CConstObjectInfo& object)
        : m_Object(object), m_ItemInfo(0)
        {
        }

    bool Valid(void) const
        {
            return m_Object;
        }
    void Next(void)
        {
            m_Object.Reset();
        }
    CConstObjectInfo Get(void) const
        {
            return m_Object;
        }
    const CItemInfo* GetItemInfo(void) const
        {
            return m_ItemInfo;
        }
    TMemberIndex GetIndex(void) const
        {
            return m_ItemInfo ? m_ItemInfo->GetIndex() : kInvalidMember;
        }
protected:
    void SetItemInfo(const CItemInfo* info)
        {
            m_ItemInfo = info;
        }
private:
    CConstObjectInfo m_Object;
    const CItemInfo* m_ItemInfo;
};

template<class ChildIterator>
class CTreeLevelIteratorMany : public CTreeLevelIterator
{
public:
    CTreeLevelIteratorMany(const CObjectInfo& object)
        : m_Iterator(object)
        {
        }

    bool Valid(void) const
        {
            return m_Iterator;
        }
    void Next(void)
        {
            m_Iterator.Next();
        }
    bool CanGet(void) const
        {
            return m_Iterator.CanGet();
        }
    CObjectInfo Get(void) const
        {
            return *m_Iterator;
        }
    void Erase(void)
        {
            m_Iterator.Erase();
        }
    const CItemInfo* GetItemInfo(void) const
        {
            return m_Iterator.GetItemInfo();
        }
    TMemberIndex GetIndex(void) const
        {
            return m_Iterator.GetIndex();
        }
protected:
    void SetItemInfo(const CItemInfo* /*info*/)
        {
        }
private:
    ChildIterator m_Iterator;
};

template<class ChildIterator>
class CConstTreeLevelIteratorMany : public CConstTreeLevelIterator
{
public:
    CConstTreeLevelIteratorMany(const CConstObjectInfo& object)
        : m_Iterator(object)
        {
        }

    bool Valid(void) const
        {
            return m_Iterator;
        }
    void Next(void)
        {
            m_Iterator.Next();
        }
    bool CanGet(void) const
        {
            return m_Iterator.CanGet();
        }
    CConstObjectInfo Get(void) const
        {
            return *m_Iterator;
        }
    const CItemInfo* GetItemInfo(void) const
        {
            return m_Iterator.GetItemInfo();
        }
    TMemberIndex GetIndex(void) const
        {
            return m_Iterator.GetIndex();
        }
protected:
    void SetItemInfo(const CItemInfo* /*info*/)
        {
        }
private:
    ChildIterator m_Iterator;
};


CConstTreeLevelIterator::~CConstTreeLevelIterator(void)
{
}

CConstTreeLevelIterator*
CConstTreeLevelIterator::CreateOne(const CConstObjectInfo& object)
{
    return new CConstTreeLevelIteratorOne(object);
}

CConstTreeLevelIterator*
CConstTreeLevelIterator::Create(const CConstObjectInfo& obj)
{
    switch ( obj.GetTypeFamily() ) {
    case eTypeFamilyClass:
        return new CConstTreeLevelIteratorMany<CConstObjectInfo::CMemberIterator>(obj);
    case eTypeFamilyContainer:
        return new CConstTreeLevelIteratorMany<CConstObjectInfo::CElementIterator>(obj);
    case eTypeFamilyPointer:
        return CreateOne(obj.GetPointedObject());
    case eTypeFamilyChoice:
        {
            CConstObjectInfo::CChoiceVariant v(obj);
            if ( v ) {
                CConstTreeLevelIterator* it = CreateOne(*v);
                it->SetItemInfo(v.GetVariantInfo());
                return it;
            }
            else
                return 0;
        }
    default:
        return 0;
    }
}

bool CConstTreeLevelIterator::HaveChildren(const CConstObjectInfo& object)
{
    if ( !object )
        return false;
    switch ( object.GetTypeFamily() ) {
    case eTypeFamilyClass:
    case eTypeFamilyChoice:
    case eTypeFamilyPointer:
    case eTypeFamilyContainer:
        return true;
    default:
        return false;
    }
}

bool SIteratorFunctions::s_ContainsType(const CConstObjectInfo& object,
                                        TTypeInfo needType)
{
    if ( object.GetTypeInfo()->IsType(needType) )
        return true;
    switch ( object.GetTypeFamily() ) {
    case eTypeFamilyClass:
        for ( CConstObjectInfo::CMemberIterator m(object); m; ++m ) {
            if ( m.GetMemberInfo()->GetTypeInfo()->MayContainType(needType) &&
                 s_ContainsType(*m, needType) )
                return true;
        }
        return false;
    case eTypeFamilyChoice:
        {
            CConstObjectInfo::CChoiceVariant v =
                object.GetCurrentChoiceVariant();
            return v &&
                v.GetVariantInfo()->GetTypeInfo()->MayContainType(needType) &&
                s_ContainsType(*v, needType);
        }
    case eTypeFamilyPointer:
        return s_ContainsType(object.GetPointedObject(), needType);
    case eTypeFamilyContainer:
        for ( CConstObjectInfo::CElementIterator e(object); e; ++e ) {
            if ( s_ContainsType(*e, needType) )
                return true;
        }
        return false;
    default:
        return false;
    }
}

CTreeLevelIterator::~CTreeLevelIterator(void)
{
}

CTreeLevelIterator*
CTreeLevelIterator::CreateOne(const CObjectInfo& object)
{
    return new CTreeLevelIteratorOne(object);
}

CTreeLevelIterator*
CTreeLevelIterator::Create(const CObjectInfo& obj)
{
    switch ( obj.GetTypeFamily() ) {
    case eTypeFamilyClass:
        return new CTreeLevelIteratorMany<CObjectInfo::CMemberIterator>(obj);
    case eTypeFamilyContainer:
        return new CTreeLevelIteratorMany<CObjectInfo::CElementIterator>(obj);
    case eTypeFamilyPointer:
        return CreateOne(obj.GetPointedObject());
    case eTypeFamilyChoice:
        {
            CObjectInfo::CChoiceVariant v(obj);
            if ( v ) {
                CTreeLevelIterator* it = CreateOne(*v);
                it->SetItemInfo(v.GetVariantInfo());
                return it;
            }
            else
                return 0;
        }
    default:
        return 0;
    }
}

void CTreeLevelIterator::Erase(void)
{
    NCBI_THROW(CSerialException,eIllegalCall, "cannot erase");
}

/////////////////

void CTreeIterator::Erase(void)
{
    _ASSERT(CheckValid());
    m_CurrentObject.Reset();

    _ASSERT(!m_Stack.empty());
    m_Stack.back()->Erase();
    Walk();
}


template<class Parent>
bool CLeafTypeIteratorBase<Parent>::CanSelect(const CConstObjectInfo& object)
{
    return CParent::CanSelect(object) &&
        SIteratorFunctions::s_ContainsType(object, this->GetIteratorType());
}

#if 0
// There is an (unconfirmed) opinion that putting these two functions
// into header (iterator.hpp) increases the size of an executable.
// Still, keeping them here is reported as bug by
// Metrowerks Codewarrior 9.0 (Mac OSX)
template<class Parent>
bool CTypesIteratorBase<Parent>::CanSelect(const CConstObjectInfo& object)
{
    if ( !CParent::CanSelect(object) )
        return false;
    m_MatchType = 0;
    TTypeInfo type = object.GetTypeInfo();
    ITERATE ( TTypeList, i, GetTypeList() ) {
        if ( type->IsType(*i) ) {
            m_MatchType = *i;
            return true;
        }
    }
    return false;
}

template<class Parent>
bool CTypesIteratorBase<Parent>::CanEnter(const CConstObjectInfo& object)
{
    if ( !CParent::CanEnter(object) )
        return false;
    TTypeInfo type = object.GetTypeInfo();
    ITERATE ( TTypeList, i, GetTypeList() ) {
        if ( type->MayContainType(*i) )
            return true;
    }
    return false;
}

#if 0 // these are obsolete
template class NCBI_XSERIAL_EXPORT CTreeIteratorTmpl<CTreeLevelIterator>;
template class NCBI_XSERIAL_EXPORT CTreeIteratorTmpl<CConstTreeLevelIterator>;
template class NCBI_XSERIAL_EXPORT CTypeIteratorBase<CTreeIterator>;
template class NCBI_XSERIAL_EXPORT CTypeIteratorBase<CTreeConstIterator>;
template class NCBI_XSERIAL_EXPORT CLeafTypeIteratorBase<CTreeIterator>;
template class NCBI_XSERIAL_EXPORT CLeafTypeIteratorBase<CTreeConstIterator>;
#endif
template class NCBI_XSERIAL_EXPORT CTypesIteratorBase<CTreeIterator>;
template class NCBI_XSERIAL_EXPORT CTypesIteratorBase<CTreeConstIterator>;
#endif

bool CType_Base::Match(const CTypesIterator& it, TTypeInfo typeInfo)
{
    return it.GetMatchType() == typeInfo;
}

bool CType_Base::Match(const CTypesConstIterator& it, TTypeInfo typeInfo)
{
    return it.GetMatchType() == typeInfo;
}

void CType_Base::AddTo(CTypesIterator& it, TTypeInfo typeInfo)
{
    it.AddType(typeInfo);
}

void CType_Base::AddTo(CTypesConstIterator& it,  TTypeInfo typeInfo)
{
    it.AddType(typeInfo);
}

TObjectPtr CType_Base::GetObjectPtr(const CTypesIterator& it)
{
    return it.GetFoundPtr();
}

TConstObjectPtr CType_Base::GetObjectPtr(const CTypesConstIterator& it)
{
    return it.GetFoundPtr();
}

TObjectPtr CType_Base::GetParentObjectPtr(CObjectIStream& in,
                                          TTypeInfo typeInfo,
                                          size_t max_depth,
                                          size_t min_depth)
{
    return in.GetParentObjectPtr(typeInfo, max_depth, min_depth);
}

class CCObjectClassInfo : public CVoidTypeInfo
{
    typedef CTypeInfo CParent;
public:
    virtual bool IsParentClassOf(const CClassTypeInfo* classInfo) const;
};

TTypeInfo CObjectGetTypeInfo::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CCObjectClassInfo;
    return typeInfo;
}

bool CCObjectClassInfo::IsParentClassOf(const CClassTypeInfo* classInfo) const
{
    return classInfo->IsCObject();
}

END_NCBI_SCOPE
