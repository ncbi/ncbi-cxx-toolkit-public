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
* Revision 1.12  2000/10/03 17:22:42  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.11  2000/09/18 20:00:22  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.10  2000/09/01 13:16:15  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.9  2000/07/03 18:42:43  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.8  2000/06/01 19:07:02  vasilche
* Added parsing of XML data.
*
* Revision 1.7  2000/05/05 17:59:06  vasilche
* Unfortunately MSVC doesn't support explicit instantiation of template methods.
*
* Revision 1.6  2000/05/05 16:26:56  vasilche
* Simplified iterator templates.
*
* Revision 1.5  2000/05/05 13:08:21  vasilche
* Simplified CTypesIterator interface.
*
* Revision 1.4  2000/05/04 16:23:12  vasilche
* Updated CTypesIterator and CTypesConstInterator interface.
*
* Revision 1.3  2000/04/10 21:01:48  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.2  2000/03/29 18:02:40  vasilche
* Workaroung of bug in MSVC: abstract member in template.
*
* Revision 1.1  2000/03/29 15:55:27  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE

class CTreeLevelIteratorOne : public CTreeLevelIterator
{
public:
    CTreeLevelIteratorOne(const CObjectInfo& object)
        : m_Object(object)
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
private:
    CObjectInfo m_Object;
};

class CConstTreeLevelIteratorOne : public CConstTreeLevelIterator
{
public:
    CConstTreeLevelIteratorOne(const CConstObjectInfo& object)
        : m_Object(object)
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
private:
    CConstObjectInfo m_Object;
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
    CObjectInfo Get(void) const
        {
            return *m_Iterator;
        }
    void Erase(void)
        {
            m_Iterator.Erase();
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
    CConstObjectInfo Get(void) const
        {
            return *m_Iterator;
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
            if ( v )
                return CreateOne(*v);
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
            if ( v )
                return CreateOne(*v);
            else
                return 0;
        }
    default:
        return 0;
    }
}

void CTreeLevelIterator::Erase(void)
{
    THROW1_TRACE(runtime_error, "cannot erase");
}

/////////////////

template<class LevelIterator>
CTreeIteratorTmpl<LevelIterator>::~CTreeIteratorTmpl(void)
{
    Reset();
}

template<class LevelIterator>
void CTreeIteratorTmpl<LevelIterator>::Reset(void)
{
    m_CurrentObject.Reset();
    m_VisitedObjects.reset(0);
    _DEBUG_ARG(m_LastCall = eNone);
    while ( !m_Stack.empty() )
        m_Stack.pop();
    _ASSERT(!*this);
}

template<class LevelIterator>
void CTreeIteratorTmpl<LevelIterator>::Init(const TBeginInfo& beginInfo)
{
    Reset();
    if ( !beginInfo.first || !beginInfo.second )
        return;
    if ( beginInfo.m_DetectLoops )
        m_VisitedObjects.reset(new TVisitedObjects);
    m_Stack.push(AutoPtr<LevelIterator>(LevelIterator::CreateOne(beginInfo)));
    Walk();
}

template<class LevelIterator>
void CTreeIteratorTmpl<LevelIterator>::Next(void)
{
    _ASSERT(CheckValid());
    m_CurrentObject.Reset();

    _ASSERT(!m_Stack.empty());
    if ( Step(m_Stack.top()->Get()) )
        Walk();
}

void CTreeIterator::Erase(void)
{
    _ASSERT(CheckValid());
    m_CurrentObject.Reset();

    _ASSERT(!m_Stack.empty());
    m_Stack.top()->Erase();
    Walk();
}

template<class LevelIterator>
void CTreeIteratorTmpl<LevelIterator>::SkipSubTree(void)
{
    _ASSERT(CheckValid());
    m_Stack.push(AutoPtr<LevelIterator>(LevelIterator::CreateOne(TObjectInfo())));
}

template<class LevelIterator>
void CTreeIteratorTmpl<LevelIterator>::Walk(void)
{
    _ASSERT(!m_Stack.empty());
    TObjectInfo current;
    do {
        current = m_Stack.top()->Get();
        if ( CanSelect(current) ) {
            m_CurrentObject = current;
            return;
        }
    } while ( Step(current) );
}

template<class LevelIterator>
bool CTreeIteratorTmpl<LevelIterator>::Step(const TObjectInfo& current)
{
    if ( CanEnter(current) ) {
        AutoPtr<LevelIterator> nextLevel(LevelIterator::Create(current));
        if ( nextLevel ) {
            _ASSERT(nextLevel->Valid());
            m_Stack.push(nextLevel);
            return true;
        }
    }
    // skip all finished iterators
    _ASSERT(!m_Stack.empty());
    do {
        m_Stack.top()->Next();
        if ( m_Stack.top()->Valid() ) {
            // next child on this level
            return true;
        }
        m_Stack.pop();
    } while ( !m_Stack.empty() );
    return false;
}

template<class LevelIterator>
void CTreeIteratorTmpl<LevelIterator>::ReportNonValid(void) const
{
    ERR_POST("Object iterator was used without checking its validity");
}

template<class LevelIterator>
bool CTreeIteratorTmpl<LevelIterator>::CanSelect(const CConstObjectInfo& obj)
{
    if ( !obj )
        return false;
    TVisitedObjects* visitedObjects = m_VisitedObjects.get();
    if ( visitedObjects ) {
        if ( !visitedObjects->insert(obj.GetObjectPtr()).second ) {
            // already visited
            return false;
        }
    }
	return true;
}

template<class LevelIterator>
bool CTreeIteratorTmpl<LevelIterator>::CanEnter(const CConstObjectInfo& object)
{
    return CConstTreeLevelIterator::HaveChildren(object);
}

template<class Parent>
bool CTypeIteratorBase<Parent>::CanSelect(const CConstObjectInfo& object)
{
    return CParent::CanSelect(object) &&
        object.GetTypeInfo()->IsType(m_NeedType);
}

template<class Parent>
bool CTypeIteratorBase<Parent>::CanEnter(const CConstObjectInfo& object)
{
    return CParent::CanEnter(object) &&
        object.GetTypeInfo()->MayContainType(m_NeedType);
}

template<class Parent>
bool CTypesIteratorBase<Parent>::CanSelect(const CConstObjectInfo& object)
{
    if ( !CParent::CanSelect(object) )
        return false;
    m_MatchType = 0;
    TTypeInfo type = object.GetTypeInfo();
    iterate ( TTypeList, i, GetTypeList() ) {
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
    iterate ( TTypeList, i, GetTypeList() ) {
        if ( type->MayContainType(*i) )
            return true;
    }
    return false;
}

template class CTreeIteratorTmpl<CTreeLevelIterator>;
template class CTreeIteratorTmpl<CConstTreeLevelIterator>;
template class CTypeIteratorBase<CTreeIterator>;
template class CTypeIteratorBase<CTreeConstIterator>;
template class CTypesIteratorBase<CTreeIterator>;
template class CTypesIteratorBase<CTreeConstIterator>;

END_NCBI_SCOPE
