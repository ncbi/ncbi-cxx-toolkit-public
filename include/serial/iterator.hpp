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
*
* ---------------------------------------------------------------------------
* $Log$
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
* Workaroung of bug in MSVC: abstract member in template.
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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/typeinfo.hpp>
#include <serial/object.hpp>
#include <serial/stdtypes.hpp>
#include <set>
#include <stack>

BEGIN_NCBI_SCOPE

class CTreeIterator;

// class holding information about root of non-modifiable object hierarchy
// Do not use it directly
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

    bool m_DetectLoops;
};

// class holding information about root of non-modifiable object hierarchy
// Do not use it directly
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
    CConstBeginInfo(const CBeginInfo& beginInfo)
        : CParent(beginInfo.first, beginInfo.second),
          m_DetectLoops(beginInfo.m_DetectLoops)
        {
        }

    bool m_DetectLoops;
};

// class describing stack level of traversal
// do not use it directly
class CConstTreeLevelIterator {
public:
    typedef CConstBeginInfo TBeginInfo;
    typedef TBeginInfo::TObjectInfo TObjectInfo;
    
    virtual ~CConstTreeLevelIterator(void);

    virtual bool Valid(void) const = 0;
    virtual void Next(void) = 0;
    virtual TObjectInfo Get(void) const = 0;

    static CConstTreeLevelIterator* Create(const TObjectInfo& object);
    static CConstTreeLevelIterator* CreateOne(const TObjectInfo& object);

    static bool HaveChildren(const CConstObjectInfo& object);
};

class CTreeLevelIterator
{
public:
    typedef CBeginInfo TBeginInfo;
    typedef TBeginInfo::TObjectInfo TObjectInfo;

    virtual ~CTreeLevelIterator(void);

    virtual bool Valid(void) const = 0;
    virtual void Next(void) = 0;
    virtual TObjectInfo Get(void) const = 0;

    static CTreeLevelIterator* Create(const TObjectInfo& object);
    static CTreeLevelIterator* CreateOne(const TObjectInfo& object);

    virtual void Erase(void);
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

    // construct object iterator
    CTreeIteratorTmpl(void)
        {
            _DEBUG_ARG(m_LastCall = eNone);
        }
    CTreeIteratorTmpl(const TBeginInfo& beginInfo)
        {
            _DEBUG_ARG(m_LastCall = eNone);
            Init(beginInfo);
        }
    virtual ~CTreeIteratorTmpl(void);


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
    void Reset(void);

    void Next(void);

    void SkipSubTree(void);

    // check whether iterator is not finished
    operator bool(void) const
        {
            _DEBUG_ARG(m_LastCall = eValid);
            return CheckValid();
        }
    // check whether iterator is not finished
    bool operator!(void) const
        {
            _DEBUG_ARG(m_LastCall = eValid);
            return CheckValid();
        }
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

protected:
    bool CheckValid(void) const
        {
#if _DEBUG
            if ( m_LastCall != eValid)
                ERR_POST("Object iterator was used without checking its validity");
#endif
            return m_CurrentObject;
        }

    virtual bool CanSelect(const CConstObjectInfo& object);
    virtual bool CanEnter(const CConstObjectInfo& object);

protected:
    // have to make these methods protected instead of private due to
    // bug in GCC
    CTreeIteratorTmpl(const TThis&);
    TThis& operator=(const TThis&);

    void Init(const TBeginInfo& beginInfo);

private:
    bool Step(const TObjectInfo& object);
    void Walk(void);

#if _DEBUG
    mutable enum { eNone, eValid, eNext, eErase } m_LastCall;
#endif
    // stack of tree level iterators
    stack< AutoPtr<LevelIterator> > m_Stack;
    // currently selected object
    TObjectInfo m_CurrentObject;
    auto_ptr<TVisitedObjects> m_VisitedObjects;

    friend class CTreeIterator;
};

typedef CTreeIteratorTmpl<CConstTreeLevelIterator> CTreeConstIterator;

// CTreeIterator is base class for all iterators over modifiable object
class CTreeIterator : public CTreeIteratorTmpl<CTreeLevelIterator>
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

    virtual bool CanSelect(const CConstObjectInfo& object);
    virtual bool CanEnter(const CConstObjectInfo& object);

    TTypeInfo GetIteratorType(void) const
        {
            return m_NeedType;
        }

private:
    TTypeInfo m_NeedType;
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
            return Get().GetObjectPtr();
        }
    TTypeInfo GetFoundType(void) const
        {
            return Get().GetTypeInfo();
        }
    TTypeInfo GetMatchType(void) const
        {
            return m_MatchType;
        }

protected:
    virtual bool CanSelect(const CConstObjectInfo& object);
    virtual bool CanEnter(const CConstObjectInfo& object);

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
            return *CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
        }
    const C* operator->(void) const
        {
            return *CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
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
            return *CTypeConverter<C>::SafeCast(Get().GetObjectPtr());
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

    CStdTypeConstIterator<T>& operator=(const TBeginInfo& beginInfo)
        {
            Init(beginInfo);
            return *this;
        }
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

// helper template for working with CTypesIterator and CTypesConstIterator
template<class C>
class Type
{
public:
    static TTypeInfo GetTypeInfo(void)
        {
            return C::GetTypeInfo();
        }
    operator CObjectTypeInfo(void) const
        {
            return GetTypeInfo();
        }
    operator TTypeInfo(void) const
        {
            return GetTypeInfo();
        }
    static void AddTo(CTypesIterator& i)
        {
            i.AddType(GetTypeInfo());
        }
    static void AddTo(CTypesConstIterator& i)
        {
            i.AddType(GetTypeInfo());
        }
    static bool Match(const CTypesIterator& i)
        {
            return i.GetMatchType() == GetTypeInfo();
        }
    static bool Match(const CTypesConstIterator& i)
        {
            return i.GetMatchType() == GetTypeInfo();
        }
    static C* Get(const CTypesIterator& i)
        {
            if ( !Match(i) )
                return 0;
            return static_cast<C*>(i.GetFoundPtr());
        }
    static const C* Get(const CTypesConstIterator& i)
        {
            if ( !Match(i) )
                return 0;
            return static_cast<const C*>(i.GetFoundPtr());
        }
};

//#include <serial/iterator.inl>

END_NCBI_SCOPE

#endif  /* ITERATOR__HPP */
