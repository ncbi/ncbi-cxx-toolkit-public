#ifndef ITERATOR__HPP
#define ITERATOR__HPP

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
*   Iterators through object hierarchy
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/exception.hpp>
#include <serial/objecttype.hpp>
#include <serial/serialutil.hpp>
#include <serial/serialbase.hpp>
#include <set>
#include <stack>

#include <serial/pathhook.hpp>


/** @addtogroup ObjHierarchy
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CTreeIterator;

// class holding information about root of non-modifiable object hierarchy
// Do not use it directly
class NCBI_XSERIAL_EXPORT CBeginInfo : public pair<TObjectPtr, TTypeInfo>
{
    typedef pair<TObjectPtr, TTypeInfo> CParent;
public:
    typedef CObjectInfo TObjectInfo;

    CBeginInfo(TObjectPtr objectPtr, TTypeInfo typeInfo,
               bool detectLoops = false)
        : CParent(objectPtr, typeInfo), m_DetectLoops(detectLoops)
        {
        }
    CBeginInfo(const CObjectInfo& object,
               bool detectLoops = false)
        : CParent(object.GetObjectPtr(), object.GetTypeInfo()),
          m_DetectLoops(detectLoops)
        {
        }
    CBeginInfo(CSerialObject& object, bool detectLoops = false)
        : CParent(&object, object.GetThisTypeInfo()),
          m_DetectLoops(detectLoops)
        {
        }

    bool m_DetectLoops;
};

// class holding information about root of non-modifiable object hierarchy
// Do not use it directly
class NCBI_XSERIAL_EXPORT CConstBeginInfo : public pair<TConstObjectPtr, TTypeInfo>
{
    typedef pair<TConstObjectPtr, TTypeInfo> CParent;
public:
    typedef CConstObjectInfo TObjectInfo;

    CConstBeginInfo(TConstObjectPtr objectPtr, TTypeInfo typeInfo,
                    bool detectLoops = false)
        : CParent(objectPtr, typeInfo), m_DetectLoops(detectLoops)
        {
        }
    CConstBeginInfo(const CConstObjectInfo& object,
                    bool detectLoops = false)
        : CParent(object.GetObjectPtr(), object.GetTypeInfo()),
          m_DetectLoops(detectLoops)
        {
        }
    CConstBeginInfo(const CSerialObject& object,
                    bool detectLoops = false)
        : CParent(&object, object.GetThisTypeInfo()),
          m_DetectLoops(detectLoops)
        {
        }
    CConstBeginInfo(const CBeginInfo& beginInfo)
        : CParent(beginInfo.first, beginInfo.second),
          m_DetectLoops(beginInfo.m_DetectLoops)
        {
        }

    bool m_DetectLoops;
};

// class describing stack level of traversal
// do not use it directly
class NCBI_XSERIAL_EXPORT CConstTreeLevelIterator {
public:
    typedef CConstBeginInfo TBeginInfo;
    typedef TBeginInfo::TObjectInfo TObjectInfo;
    
    virtual ~CConstTreeLevelIterator(void);

    virtual bool Valid(void) const = 0;
    virtual void Next(void) = 0;
    virtual bool CanGet(void) const
    {
        return true;
    }
    virtual TObjectInfo Get(void) const = 0;
    virtual const CItemInfo* GetItemInfo(void) const = 0;

    static CConstTreeLevelIterator* Create(const TObjectInfo& object);
    static CConstTreeLevelIterator* CreateOne(const TObjectInfo& object);

    static bool HaveChildren(const CConstObjectInfo& object);
protected:
    virtual void SetItemInfo(const CItemInfo* info) = 0;
};

class NCBI_XSERIAL_EXPORT CTreeLevelIterator
{
public:
    typedef CBeginInfo TBeginInfo;
    typedef TBeginInfo::TObjectInfo TObjectInfo;

    virtual ~CTreeLevelIterator(void);

    virtual bool Valid(void) const = 0;
    virtual void Next(void) = 0;
    virtual bool CanGet(void) const
    {
        return true;
    }
    virtual TObjectInfo Get(void) const = 0;
    virtual const CItemInfo* GetItemInfo(void) const = 0;

    static CTreeLevelIterator* Create(const TObjectInfo& object);
    static CTreeLevelIterator* CreateOne(const TObjectInfo& object);

    virtual void Erase(void);
protected:
    virtual void SetItemInfo(const CItemInfo* info) = 0;
};

// CTreeIterator is base class for all iterators over non-modifiable object
// Do not use it directly
template<class LevelIterator>
class CTreeIteratorTmpl
{
    typedef CTreeIteratorTmpl<LevelIterator> TThis;
public:
    typedef typename LevelIterator::TObjectInfo TObjectInfo;
    typedef typename LevelIterator::TBeginInfo TBeginInfo;
    typedef set<TConstObjectPtr> TVisitedObjects;
    typedef list< pair< typename LevelIterator::TObjectInfo, const CItemInfo*> > TIteratorContext;

protected:
    // iterator debugging support
    bool CheckValid(void) const
        {
            return m_CurrentObject;
        }
public:

    // construct object iterator
    CTreeIteratorTmpl(void)
        {
        }
    CTreeIteratorTmpl(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
        }
    CTreeIteratorTmpl(const TBeginInfo& beginInfo, const string& filter)
        {
            Init(beginInfo, filter);
        }
    virtual ~CTreeIteratorTmpl(void)
        {
            Reset();
        }

    // get information about current object
    TObjectInfo& Get(void)
        {
            _ASSERT(CheckValid());
            return m_CurrentObject;
        }
    // get information about current object
    const TObjectInfo& Get(void) const
        {
            _ASSERT(CheckValid());
            return m_CurrentObject;
        }
    // get type information of current object
    TTypeInfo GetCurrentTypeInfo(void) const
        {
            return Get().GetTypeInfo();
        }

    // reset iterator to initial state
    void Reset(void)
        {
            m_CurrentObject.Reset();
            m_VisitedObjects.reset(0);
            while ( !m_Stack.empty() )
                m_Stack.pop();
            _ASSERT(!*this);
        }

    void Next(void)
        {
            _ASSERT(CheckValid());
            m_CurrentObject.Reset();

            _ASSERT(!m_Stack.empty());
            if ( Step(m_Stack.top()->Get()) )
                Walk();
        }

    void SkipSubTree(void)
        {
            _ASSERT(CheckValid());
            m_Stack.push(AutoPtr<LevelIterator>(LevelIterator::CreateOne(TObjectInfo())));
        }


    bool IsValid(void) const
        {
            return CheckValid();
        }

    // check whether iterator is not finished
    DECLARE_OPERATOR_BOOL(IsValid());

    // go to next object
    TThis& operator++(void)
        {
            Next();
            return *this;
        }

    // initialize iterator to new root of object hierarchy
    TThis& operator=(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
            return *this;
        }

    TIteratorContext GetContextData(void) const
        {
            TIteratorContext stk_info;
            stack< AutoPtr<LevelIterator> >& stk =
                const_cast< stack< AutoPtr<LevelIterator> >& >(m_Stack);
            stack< AutoPtr<LevelIterator> > tmp;
            while ( !stk.empty() ) {
                AutoPtr<LevelIterator>& t = stk.top();
                stk_info.push_front( make_pair( t->Get(), t->GetItemInfo()));
                tmp.push(t);
                stk.pop();
            }
            while ( !tmp.empty() ) {
                AutoPtr<LevelIterator>& t = tmp.top();
                stk.push(t);
                tmp.pop();
            }
            return stk_info;
        }

    string GetContext(void) const
        {
            string loc;
            TIteratorContext stk_info = GetContextData();
            typename TIteratorContext::const_iterator i;
            for (i = stk_info.begin(); i != stk_info.end(); ++i) {
                TTypeInfo tt = i->first.GetTypeInfo();
                const CItemInfo* ii = i->second;
                string name;
                if (ii) {
                    const CMemberId& id = ii->GetId();
                    if (!id.IsAttlist() && !id.HasNotag()) {
                        name = id.GetName();
                    }
                } else {
                    if (loc.empty()) {
                        name = tt->GetName();
                    }
                }
                if (!name.empty()) {
                    if (!loc.empty()) {
                        loc += ".";
                    }
                    loc += name;
                }
            }
            return loc;
        }
    void SetContextFilter(const string& filter)
        {
            m_ContextFilter = filter;
            if (!m_ContextFilter.empty() &&
                !CPathHook::Match(m_ContextFilter,GetContext())) {
                Next();
            }
        }

protected:
    virtual bool CanSelect(const CConstObjectInfo& obj)
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

    virtual bool CanEnter(const CConstObjectInfo& object)
        {
            return CConstTreeLevelIterator::HaveChildren(object);
        }

protected:
    // have to make these methods protected instead of private due to
    // bug in GCC
#ifdef NCBI_OS_MSWIN
    CTreeIteratorTmpl(const TThis&) { NCBI_THROW(CSerialException,eIllegalCall, "cannot copy"); }
    TThis& operator=(const TThis&) { NCBI_THROW(CSerialException,eIllegalCall, "cannot copy"); }
#else
    CTreeIteratorTmpl(const TThis&);
    TThis& operator=(const TThis&);
#endif

    void Init(const TBeginInfo& beginInfo)
        {
            Reset();
            if ( !beginInfo.first || !beginInfo.second )
                return;
            if ( beginInfo.m_DetectLoops )
                m_VisitedObjects.reset(new TVisitedObjects);
            m_Stack.push(AutoPtr<LevelIterator>(LevelIterator::CreateOne(beginInfo)));
            Walk();
        }
    void Init(const TBeginInfo& beginInfo, const string& filter)
        {
            m_ContextFilter = filter;
            Init(beginInfo);
        }

private:
    bool Step(const TObjectInfo& current)
        {
            if ( CanEnter(current) ) {
                AutoPtr<LevelIterator> nextLevel(LevelIterator::Create(current));
                if ( nextLevel && nextLevel->Valid() ) {
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

    void Walk(void)
        {
            _ASSERT(!m_Stack.empty());
            TObjectInfo current;
            do {
                while (!m_Stack.top()->CanGet()) {
                    for(;;) {
                        m_Stack.top()->Next();
                        if (m_Stack.top()->Valid()) {
                            break;
                        }
                        m_Stack.pop();
                        if (m_Stack.empty()) {
                            return;
                        }
                    }
                }
                current = m_Stack.top()->Get();
                if ( CanSelect(current) ) {
                    if (m_ContextFilter.empty() ||
                        CPathHook::Match(m_ContextFilter,GetContext())) {
                        m_CurrentObject = current;
                        return;
                    }
                }
            } while ( Step(current) );
        }

    // stack of tree level iterators
    stack< AutoPtr<LevelIterator> > m_Stack;
    // currently selected object
    TObjectInfo m_CurrentObject;
    auto_ptr<TVisitedObjects> m_VisitedObjects;
    string m_ContextFilter;

    friend class CTreeIterator;
};

typedef CTreeIteratorTmpl<CConstTreeLevelIterator> CTreeConstIterator;

// CTreeIterator is base class for all iterators over modifiable object
class NCBI_XSERIAL_EXPORT CTreeIterator : public CTreeIteratorTmpl<CTreeLevelIterator>
{
    typedef CTreeIteratorTmpl<CTreeLevelIterator> CParent;
public:
    typedef CParent::TObjectInfo TObjectInfo;
    typedef CParent::TBeginInfo TBeginInfo;

    // construct object iterator
    CTreeIterator(void)
        {
        }
    CTreeIterator(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
        }
    CTreeIterator(const TBeginInfo& beginInfo, const string& filter)
        {
            Init(beginInfo, filter);
        }

    // initialize iterator to new root of object hierarchy
    CTreeIterator& operator=(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
            return *this;
        }

    // delete currently pointed object (throws exception if failed)
    void Erase(void);
};

// template base class for CTypeIterator<> and CTypeConstIterator<>
// Do not use it directly
template<class Parent>
class CTypeIteratorBase : public Parent
{
    typedef Parent CParent;
protected:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CTypeIteratorBase(TTypeInfo needType)
        : m_NeedType(needType)
        {
        }
    CTypeIteratorBase(TTypeInfo needType, const TBeginInfo& beginInfo)
        : m_NeedType(needType)
        {
            Init(beginInfo);
        }
    CTypeIteratorBase(TTypeInfo needType, const TBeginInfo& beginInfo,
                      const string& filter)
        : m_NeedType(needType)
        {
            Init(beginInfo, filter);
        }

    virtual bool CanSelect(const CConstObjectInfo& object)
        {
            return CParent::CanSelect(object) &&
                object.GetTypeInfo()->IsType(m_NeedType);
        }
    virtual bool CanEnter(const CConstObjectInfo& object)
        {
            return CParent::CanEnter(object) &&
                object.GetTypeInfo()->MayContainType(m_NeedType);
        }

    TTypeInfo GetIteratorType(void) const
        {
            return m_NeedType;
        }

private:
    TTypeInfo m_NeedType;
};

// template base class for CTypeIterator<> and CTypeConstIterator<>
// Do not use it directly
template<class Parent>
class CLeafTypeIteratorBase : public CTypeIteratorBase<Parent>
{
    typedef CTypeIteratorBase<Parent> CParent;
protected:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CLeafTypeIteratorBase(TTypeInfo needType)
        : CParent(needType)
        {
        }
    CLeafTypeIteratorBase(TTypeInfo needType, const TBeginInfo& beginInfo)
        : CParent(needType)
        {
            Init(beginInfo);
        }
    CLeafTypeIteratorBase(TTypeInfo needType, const TBeginInfo& beginInfo,
                          const string& filter)
        : CParent(needType)
        {
            Init(beginInfo, filter);
        }

    virtual bool CanSelect(const CConstObjectInfo& object);
};

// template base class for CTypesIterator and CTypesConstIterator
// Do not use it directly
template<class Parent>
class CTypesIteratorBase : public Parent
{
    typedef Parent CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;
    typedef list<TTypeInfo> TTypeList;

    CTypesIteratorBase(void)
        {
        }
    CTypesIteratorBase(TTypeInfo type)
        {
            m_TypeList.push_back(type);
        }
    CTypesIteratorBase(TTypeInfo type1, TTypeInfo type2)
        {
            m_TypeList.push_back(type1);
            m_TypeList.push_back(type2);
        }
    CTypesIteratorBase(const TTypeList& typeList)
        : m_TypeList(typeList)
        {
        }
    CTypesIteratorBase(const TTypeList& typeList, const TBeginInfo& beginInfo)
        : m_TypeList(typeList)
        {
            Init(beginInfo);
        }
    CTypesIteratorBase(const TTypeList& typeList, const TBeginInfo& beginInfo,
                       const string& filter)
        : m_TypeList(typeList)
        {
            Init(beginInfo, filter);
        }

    const TTypeList& GetTypeList(void) const
        {
            return m_TypeList;
        }

    CTypesIteratorBase<Parent>& AddType(TTypeInfo type)
        {
            m_TypeList.push_back(type);
            return *this;
        }

    CTypesIteratorBase<Parent>& operator=(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
            return *this;
        }

    typename CParent::TObjectInfo::TObjectPtrType GetFoundPtr(void) const
        {
            return this->Get().GetObjectPtr();
        }
    TTypeInfo GetFoundType(void) const
        {
            return this->Get().GetTypeInfo();
        }
    TTypeInfo GetMatchType(void) const
        {
            return m_MatchType;
        }

protected:
#if 0
// There is an (unconfirmed) opinion that putting these two functions
// into source (iterator.cpp) reduces the size of an executable.
// Still, keeping them there is reported as bug by
// Metrowerks Codewarrior 9.0 (Mac OSX)
    virtual bool CanSelect(const CConstObjectInfo& object);
    virtual bool CanEnter(const CConstObjectInfo& object);
#else
    virtual bool CanSelect(const CConstObjectInfo& object)
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
    virtual bool CanEnter(const CConstObjectInfo& object)
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
#endif

private:
    TTypeList m_TypeList;
    TTypeInfo m_MatchType;
};

// template class for iteration on objects of class C
template<class C, class TypeGetter = C>
class CTypeIterator : public CTypeIteratorBase<CTreeIterator>
{
    typedef CTypeIteratorBase<CTreeIterator> CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CTypeIterator(void)
        : CParent(TypeGetter::GetTypeInfo())
        {
        }
    CTypeIterator(const TBeginInfo& beginInfo)
        : CParent(TypeGetter::GetTypeInfo(), beginInfo)
        {
        }
    CTypeIterator(const TBeginInfo& beginInfo, const string& filter)
        : CParent(TypeGetter::GetTypeInfo(), beginInfo, filter)
        {
        }
    explicit CTypeIterator(CSerialObject& object)
        : CParent(TypeGetter::GetTypeInfo(), TBeginInfo(object))
        {
        }

    CTypeIterator<C, TypeGetter>& operator=(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
            return *this;
        }

    C& operator*(void)
        {
            return *CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
    const C& operator*(void) const
        {
            return *CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
    C* operator->(void)
        {
            return CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
    const C* operator->(void) const
        {
            return CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
};

// template class for iteration on objects of class C (non-medifiable version)
template<class C, class TypeGetter = C>
class CTypeConstIterator : public CTypeIteratorBase<CTreeConstIterator>
{
    typedef CTypeIteratorBase<CTreeConstIterator> CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CTypeConstIterator(void)
        : CParent(TypeGetter::GetTypeInfo())
        {
        }
    CTypeConstIterator(const TBeginInfo& beginInfo)
        : CParent(TypeGetter::GetTypeInfo(), beginInfo)
        {
        }
    CTypeConstIterator(const TBeginInfo& beginInfo, const string& filter)
        : CParent(TypeGetter::GetTypeInfo(), beginInfo, filter)
        {
        }
    explicit CTypeConstIterator(const CSerialObject& object)
        : CParent(TypeGetter::GetTypeInfo(), TBeginInfo(object))
        {
        }

    CTypeConstIterator<C, TypeGetter>& operator=(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
            return *this;
        }

    const C& operator*(void) const
        {
            return *CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
    const C* operator->(void) const
        {
            return CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
};

// template class for iteration on objects of class C
template<class C>
class CLeafTypeIterator : public CLeafTypeIteratorBase<CTreeIterator>
{
    typedef CLeafTypeIteratorBase<CTreeIterator> CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CLeafTypeIterator(void)
        : CParent(C::GetTypeInfo())
        {
        }
    CLeafTypeIterator(const TBeginInfo& beginInfo)
        : CParent(C::GetTypeInfo(), beginInfo)
        {
        }
    CLeafTypeIterator(const TBeginInfo& beginInfo, const string& filter)
        : CParent(C::GetTypeInfo(), beginInfo, filter)
        {
        }
    explicit CLeafTypeIterator(CSerialObject& object)
        : CParent(C::GetTypeInfo(), TBeginInfo(object))
        {
        }

    CLeafTypeIterator<C>& operator=(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
            return *this;
        }

    C& operator*(void)
        {
            return *CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
    const C& operator*(void) const
        {
            return *CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
    C* operator->(void)
        {
            return CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
    const C* operator->(void) const
        {
            return CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
};

// template class for iteration on objects of class C (non-medifiable version)
template<class C>
class CLeafTypeConstIterator : public CLeafTypeIteratorBase<CTreeConstIterator>
{
    typedef CLeafTypeIteratorBase<CTreeConstIterator> CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CLeafTypeConstIterator(void)
        : CParent(C::GetTypeInfo())
        {
        }
    CLeafTypeConstIterator(const TBeginInfo& beginInfo)
        : CParent(C::GetTypeInfo(), beginInfo)
        {
        }
    CLeafTypeConstIterator(const TBeginInfo& beginInfo, const string& filter)
        : CParent(C::GetTypeInfo(), beginInfo, filter)
        {
        }
    explicit CLeafTypeConstIterator(const CSerialObject& object)
        : CParent(C::GetTypeInfo(), TBeginInfo(object))
        {
        }

    CLeafTypeConstIterator<C>& operator=(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
            return *this;
        }

    const C& operator*(void) const
        {
            return *CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
    const C* operator->(void) const
        {
            return CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
};

// template class for iteration on objects of standard C++ type T
template<typename T>
class CStdTypeIterator : public CTypeIterator<T, CStdTypeInfo<T> >
{
    typedef CTypeIterator<T, CStdTypeInfo<T> > CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CStdTypeIterator(void)
        {
        }
    CStdTypeIterator(const TBeginInfo& beginInfo)
        : CParent(beginInfo)
        {
        }
    CStdTypeIterator(const TBeginInfo& beginInfo, const string& filter)
        : CParent(beginInfo, filter)
        {
        }
    explicit CStdTypeIterator(CSerialObject& object)
        : CParent(object)
        {
        }

    CStdTypeIterator<T>& operator=(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
            return *this;
        }
};

// template class for iteration on objects of standard C++ type T
// (non-modifiable version)
template<typename T>
class CStdTypeConstIterator : public CTypeConstIterator<T, CStdTypeInfo<T> >
{
    typedef CTypeConstIterator<T, CStdTypeInfo<T> > CParent;
public:
    typedef typename CParent::TBeginInfo TBeginInfo;

    CStdTypeConstIterator(void)
        {
        }
    CStdTypeConstIterator(const TBeginInfo& beginInfo)
        : CParent(beginInfo)
        {
        }
    CStdTypeConstIterator(const TBeginInfo& beginInfo, const string& filter)
        : CParent(beginInfo, filter)
        {
        }
    explicit CStdTypeConstIterator(const CSerialObject& object)
        : CParent(object)
        {
        }

    CStdTypeConstIterator<T>& operator=(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
            return *this;
        }
};

// get special typeinfo of CObject
class NCBI_XSERIAL_EXPORT CObjectGetTypeInfo
{
public:
    static TTypeInfo GetTypeInfo(void);
};

// class for iteration on objects derived from class CObject
typedef CTypeIterator<CObject, CObjectGetTypeInfo> CObjectIterator;
// class for iteration on objects derived from class CObject
// (non-modifiable version)
typedef CTypeConstIterator<CObject, CObjectGetTypeInfo> CObjectConstIterator;

// class for iteration on objects of list of types
typedef CTypesIteratorBase<CTreeIterator> CTypesIterator;
// class for iteration on objects of list of types (non-modifiable version)
typedef CTypesIteratorBase<CTreeConstIterator> CTypesConstIterator;

// enum flag for turning on loop detection in object hierarchy
enum EDetectLoops {
    eDetectLoops
};

// get starting point of object hierarchy
template<class C>
inline
CBeginInfo Begin(C& obj)
{
    return CBeginInfo(&obj, C::GetTypeInfo(), false);
}

// get starting point of non-modifiable object hierarchy
template<class C>
inline
CConstBeginInfo ConstBegin(const C& obj)
{
    return CConstBeginInfo(&obj, C::GetTypeInfo(), false);
}

template<class C>
inline
CConstBeginInfo Begin(const C& obj)
{
    return CConstBeginInfo(&obj, C::GetTypeInfo(), false);
}

// get starting point of object hierarchy with loop detection
template<class C>
inline
CBeginInfo Begin(C& obj, EDetectLoops)
{
    return CBeginInfo(&obj, C::GetTypeInfo(), true);
}

// get starting point of non-modifiable object hierarchy with loop detection
template<class C>
inline
CConstBeginInfo ConstBegin(const C& obj, EDetectLoops)
{
    return CConstBeginInfo(&obj, C::GetTypeInfo(), true);
}

template<class C>
inline
CConstBeginInfo Begin(const C& obj, EDetectLoops)
{
    return CConstBeginInfo(&obj, C::GetTypeInfo(), true);
}


/* @} */


END_NCBI_SCOPE

#endif  /* ITERATOR__HPP */

/*
 * ---------------------------------------------------------------------------
* $Log$
* Revision 1.37  2005/03/17 21:07:22  vasilche
* Removed enforced check for call of IsValid().
*
* Revision 1.36  2005/01/24 21:58:46  vasilche
* Fixed check for validity of CTypeIterator<>.
*
* Revision 1.35  2005/01/24 17:05:48  vasilche
* Safe boolean operators.
*
* Revision 1.34  2004/12/28 18:02:10  gouriano
* Added context filter
*
* Revision 1.33  2004/07/28 17:03:29  gouriano
* Minor tune-up for GCC34
*
* Revision 1.32  2004/07/28 16:55:57  gouriano
* Changed definition of TIteratorContext to add more info
*
* Revision 1.31  2004/07/27 17:11:48  gouriano
* Give access to the context of tree iterator
*
* Revision 1.30  2004/04/26 16:40:59  ucko
* Tweak for GCC 3.4 compatibility.
*
* Revision 1.29  2003/09/30 17:12:30  gouriano
* Modified TypeIterators to skip unset optional members
*
* Revision 1.28  2003/09/15 20:01:15  gouriano
* fixed the definition of CTypesIteratorBase to eliminate compilation warnings
*
* Revision 1.27  2003/04/15 14:15:21  siyan
* Added doxygen support
*
* Revision 1.26  2003/03/26 16:13:32  vasilche
* Removed TAB symbols. Some formatting.
*
* Revision 1.25  2003/03/10 18:52:37  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.24  2003/02/04 16:05:48  dicuccio
* Header file clean-up - removed redundant includes
*
* Revision 1.23  2002/12/23 19:02:30  dicuccio
* Moved template function bodies from .cpp -> .hpp to make MSVC happy.  Added an
* #ifdef'd wrapper to unimplemented protected ctors - MSVC doesn't like these.
* Log to end
*
* Revision 1.21  2002/06/13 15:15:19  ucko
* Add [explicit] CSerialObject-based constructors, which should get rid
* of the need for [Const]Begin() in the majority of cases.
*
* Revision 1.20  2001/05/17 14:56:45  lavr
* Typos corrected
*
* Revision 1.19  2000/11/09 15:21:25  vasilche
* Fixed bugs in iterators.
* Added iterator constructors from CObjectInfo.
* Added CLeafTypeIterator.
*
* Revision 1.18  2000/11/08 19:24:17  vasilche
* Added CLeafTypeIterator<Type> and CLeafTypeConstIterator<Type>.
*
* Revision 1.17  2000/10/20 15:51:23  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.16  2000/10/03 17:22:32  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.15  2000/09/19 20:16:52  vasilche
* Fixed type in CStlClassInfo_auto_ptr.
* Added missing include serialutil.hpp.
*
* Revision 1.14  2000/09/18 20:00:02  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.13  2000/07/03 18:42:33  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.12  2000/06/07 19:45:42  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.11  2000/06/01 19:06:55  vasilche
* Added parsing of XML data.
*
* Revision 1.10  2000/05/24 20:08:12  vasilche
* Implemented XML dump.
*
* Revision 1.9  2000/05/09 16:38:32  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.8  2000/05/05 17:59:02  vasilche
* Unfortunately MSVC doesn't support explicit instantiation of template methods.
*
* Revision 1.7  2000/05/05 16:26:51  vasilche
* Simplified iterator templates.
*
* Revision 1.6  2000/05/05 13:08:16  vasilche
* Simplified CTypesIterator interface.
*
* Revision 1.5  2000/05/04 16:23:09  vasilche
* Updated CTypesIterator and CTypesConstInterator interface.
*
* Revision 1.4  2000/04/10 21:01:38  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.3  2000/03/29 18:02:39  vasilche
* Workaround of bug in MSVC: abstract member in template.
*
* Revision 1.2  2000/03/29 17:22:34  vasilche
* Fixed ambiguity in Begin() template function.
*
* Revision 1.1  2000/03/29 15:55:19  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* ===========================================================================
*/
