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
#include <serial/impl/objecttype.hpp>
#include <serial/serialutil.hpp>
#include <serial/serialbase.hpp>
#include <set>

#include <serial/impl/pathhook.hpp>


/** @addtogroup ObjHierarchy
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CTreeIterator;

/// Class holding information about root of non-modifiable object hierarchy
/// Do not use it directly
class CBeginInfo : public pair<TObjectPtr, TTypeInfo>
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

/// Class holding information about root of non-modifiable object hierarchy
/// Do not use it directly
class CConstBeginInfo : public pair<TConstObjectPtr, TTypeInfo>
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

/// Class describing stack level of traversal
/// do not use it directly
class NCBI_XSERIAL_EXPORT CConstTreeLevelIterator {
public:
    typedef CConstBeginInfo TBeginInfo;
    typedef TBeginInfo::TObjectInfo TObjectInfo;
    
    virtual ~CConstTreeLevelIterator(void);

    virtual bool Valid(void) const = 0;
    virtual TMemberIndex GetIndex(void) const
    {
        return kInvalidMember;
    }
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
    virtual TMemberIndex GetIndex(void) const
    {
        return kInvalidMember;
    }
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

/// Base class for all iterators over non-modifiable object
/// Do not use it directly
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

    /// Get information about current object
    TObjectInfo& Get(void)
        {
            _ASSERT(CheckValid());
            return m_CurrentObject;
        }
    /// Get information about current object
    const TObjectInfo& Get(void) const
        {
            _ASSERT(CheckValid());
            return m_CurrentObject;
        }
    /// Get type information of current object
    TTypeInfo GetCurrentTypeInfo(void) const
        {
            return Get().GetTypeInfo();
        }

    /// Reset iterator to initial state
    void Reset(void)
        {
            m_CurrentObject.Reset();
            m_VisitedObjects.reset(0);
            while ( !m_Stack.empty() )
                m_Stack.pop_back();
            _ASSERT(!*this);
        }

    void Next(void)
        {
            _ASSERT(CheckValid());
            m_CurrentObject.Reset();

            _ASSERT(!m_Stack.empty());
            if ( Step(m_Stack.back()->Get()) )
                Walk();
        }

    void SkipSubTree(void)
        {
            _ASSERT(CheckValid());
            m_Stack.push_back(AutoPtr<LevelIterator>(LevelIterator::CreateOne(TObjectInfo())));
        }


    bool IsValid(void) const
        {
            return CheckValid();
        }

    // check whether iterator is not finished
    DECLARE_OPERATOR_BOOL(IsValid());

    /// Go to next object
    TThis& operator++(void)
        {
            Next();
            return *this;
        }

    /// Initialize iterator to new root of object hierarchy
    TThis& operator=(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
            return *this;
        }

    /// Get raw context data
    TIteratorContext GetContextData(void) const
        {
            TIteratorContext stk_info;
            typename vector< AutoPtr<LevelIterator> >::const_iterator i;
            for (i = m_Stack.begin(); i != m_Stack.end(); ++i) {
                stk_info.push_back( make_pair( (*i)->Get(), (*i)->GetItemInfo()));
            }
            return stk_info;
        }

    /// Get context data as string
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
                    const CMemberId& mid = ii->GetId();
                    if (!mid.IsAttlist() && !mid.HasNotag()) {
                        name = mid.GetName();
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
    
    /// Check context against filter
    ///
    /// @param filter
    ///   Context filter string
    bool MatchesContext(const string& filter) const
    {
        if (filter.empty()) {
            return true;
        }
        return CPathHook::Match(filter,GetContext());
    }
    
    /// Set context filter
    ///
    /// @param filter
    ///   Context filter string
    void SetContextFilter(const string& filter)
        {
            m_ContextFilter = filter;
            if (!MatchesContext(filter)) {
                Next();
            }
        }

    /// Return element index in STL container
    int GetContainerElementIndex(void) const
        {
            TMemberIndex ind = kInvalidMember;
            if (m_Stack.size() > 1) {
                LevelIterator* l( m_Stack[ m_Stack.size()-2 ].get());
                ind = l->GetIndex();
            }
            return ind - kInvalidMember - 1;
        }

    /// Return member index in sequence, or variant index in choice
    int GetItemIndex(void) const
        {
            TMemberIndex ind = kInvalidMember;
            if (!m_Stack.empty()) {
                ind = m_Stack.back().get()->GetIndex();
            }
            return ind - kInvalidMember - 1;
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
            m_Stack.push_back(AutoPtr<LevelIterator>(LevelIterator::CreateOne(beginInfo)));
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
                    m_Stack.push_back(nextLevel);
                    return true;
                }
            }
            // skip all finished iterators
            _ASSERT(!m_Stack.empty());
            do {
                m_Stack.back()->Next();
                if ( m_Stack.back()->Valid() ) {
                    // next child on this level
                    return true;
                }
                m_Stack.pop_back();
            } while ( !m_Stack.empty() );
            return false;
        }

    void Walk(void)
        {
            _ASSERT(!m_Stack.empty());
            TObjectInfo current;
            do {
                while (!m_Stack.back()->CanGet()) {
                    for(;;) {
                        m_Stack.back()->Next();
                        if (m_Stack.back()->Valid()) {
                            break;
                        }
                        m_Stack.pop_back();
                        if (m_Stack.empty()) {
                            return;
                        }
                    }
                }
                current = m_Stack.back()->Get();
                if ( CanSelect(current) ) {
                    if (MatchesContext(m_ContextFilter)) {
                        m_CurrentObject = current;
                        return;
                    }
                }
            } while ( Step(current) );
        }

    // stack of tree level iterators
    vector< AutoPtr<LevelIterator> > m_Stack;
    // currently selected object
    TObjectInfo m_CurrentObject;
    auto_ptr<TVisitedObjects> m_VisitedObjects;
    string m_ContextFilter;

    friend class CTreeIterator;
}; /* NCBI_FAKE_WARNING: MSVC */

typedef CTreeIteratorTmpl<CConstTreeLevelIterator> CTreeConstIterator;

/// Base class for all iterators over modifiable object
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

    /// Delete currently pointed object (throws exception if failed)
    void Erase(void);
};

/// template base class for CTypeIterator<> and CTypeConstIterator<>
/// Do not use it directly
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

/// Template base class for CTypeIterator<> and CTypeConstIterator<>
/// Do not use it directly
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

/// Template base class for CTypesIterator and CTypesConstIterator
/// Do not use it directly
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

/// Template class for iteration on objects of class C
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

/// Template class for iteration on objects of class C (non-medifiable version)
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

/// Template class for iteration on objects of class C
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

/// Template class for iteration on objects of class C (non-medifiable version)
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

/// Template class for iteration on objects of standard C++ type T
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

/// Template class for iteration on objects of standard C++ type T
/// Non-modifiable version
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

/// Get starting point of object hierarchy
template<class C>
inline
CBeginInfo Begin(C& obj)
{
    return CBeginInfo(&obj, C::GetTypeInfo(), false);
}

/// Get starting point of non-modifiable object hierarchy
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

/// Get starting point of object hierarchy with loop detection
template<class C>
inline
CBeginInfo Begin(C& obj, EDetectLoops)
{
    return CBeginInfo(&obj, C::GetTypeInfo(), true);
}

/// Get starting point of non-modifiable object hierarchy with loop detection
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
