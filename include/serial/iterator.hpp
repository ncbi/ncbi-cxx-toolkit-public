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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
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
#include <serial/iteratorbase.hpp>
#include <serial/typeinfo.hpp>
#include <serial/stdtypes.hpp>

BEGIN_NCBI_SCOPE

class CTreeIterator;
class CTreeConstIterator;

template<class Iterator>
class CTreeIteratorBase
{
public:
    typedef Iterator TIteratorType;
    typedef typename Iterator::TObjectInfo TObjectInfo;

    CTreeIteratorBase(void)
        : m_Valid(false), m_SkipSubTree(false), m_Stack(0), m_StackDepth(0)
        {
        }

    virtual ~CTreeIteratorBase(void);


    TObjectInfo& Get(void)
        {
            _ASSERT(Valid());
            return m_CurrentObject;
        }
    const TObjectInfo& Get(void) const
        {
            _ASSERT(Valid());
            return m_CurrentObject;
        }

    // reset iterator to initial state (!Valid() && End())
    void Reset(void);

    // points to a valid object
    bool Valid(void) const
        {
            return m_Valid;
        }
    // end of tree (also Valid() == false)
    bool End(void) const
        {
            return !Valid() && m_Stack == 0;
        }
    // post condition: Valid() || End()
    void Next(void);

    // precondition: Valid()
    // postcondition: Valid(); will skip on the next Next()
    void SkipSubTree(void)
        {
            m_SkipSubTree = true;
        }

    operator bool(void)
        {
            return !End();
        }
    CTreeIteratorBase<Iterator>& operator++(void)
        {
            Next();
            return *this;
        }

    class CTreeLevel : public TIteratorType {
    public:
        CTreeLevel(const TObjectInfo& owner, CTreeLevel* previousLevel)
            : TIteratorType(owner), m_PreviousLevel(previousLevel)
            {
            }

        CTreeLevel* Pop(void)
            {
                CTreeLevel* previousLevel = m_PreviousLevel;
                delete this;
                return previousLevel;
            }

    private:
        CTreeLevel* m_PreviousLevel;
    };

protected:
    // post condition: Valid() || End()
    void x_Begin(const TObjectInfo& object);
    virtual bool CanSelect(TTypeInfo type) const;
    virtual bool CanEnter(TTypeInfo type) const;

private:
    
    friend class CTreeIterator; // for Erase

    bool m_Valid;
    bool m_SkipSubTree;
    // stack of tree level iterators
    CTreeLevel* m_Stack;
    size_t m_StackDepth;
    // currently selected object
    TObjectInfo m_CurrentObject;
};

class CTreeIterator : public CTreeIteratorBase<CChildrenIterator>
{
    typedef CTreeIteratorBase<CChildrenIterator> CParent;
public:
    
    void Begin(const TObjectInfo& object)
        {
            x_Begin(object);
        }
    void Begin(pair<TObjectPtr, TTypeInfo> object)
        {
            x_Begin(object);
        }
    
    // precondition: Valid()
    // postcondition: !Valid() until Next()
    void Erase(void);
};

class CTreeConstIterator : public CTreeIteratorBase<CConstChildrenIterator>
{
    typedef CTreeIteratorBase<CConstChildrenIterator> CParent;
public:
    
    void Begin(const TObjectInfo& object)
        {
            x_Begin(object);
        }
    void Begin(const CObjectInfo& object)
        {
            x_Begin(object);
        }
    void Begin(pair<TConstObjectPtr, TTypeInfo> object)
        {
            x_Begin(object);
        }
    void Begin(pair<TObjectPtr, TTypeInfo> object)
        {
            x_Begin(object);
        }
};

template<class Iterator>
class CTypeIteratorBase : public Iterator
{
    typedef Iterator CParent;
protected:
    CTypeIteratorBase(TTypeInfo needType)
        : m_NeedType(needType)
        {
        }

    virtual bool CanSelect(TTypeInfo type) const;
    virtual bool CanEnter(TTypeInfo type) const;

private:
    TTypeInfo m_NeedType;
};

template<class C, class TypeGetter = C>
class CTypeIterator : public CTypeIteratorBase<CTreeIterator>
{
    typedef CTypeIteratorBase<CTreeIterator> CParent;
public:
    typedef typename CParent::TObjectInfo TObjectInfo;

    CTypeIterator(void)
        : CParent(TypeGetter::GetTypeInfo())
        {
        }
    CTypeIterator(const TObjectInfo& root)
        : CParent(TypeGetter::GetTypeInfo())
        {
            Begin(root);
        }
    CTypeIterator(pair<TObjectPtr, TTypeInfo> root)
        : CParent(TypeGetter::GetTypeInfo())
        {
            Begin(root);
        }

    CTypeIterator<C>& operator=(const TObjectInfo& root)
        {
            Begin(root);
            return *this;
        }
    CTypeIterator<C>& operator=(pair<TObjectPtr, TTypeInfo> root)
        {
            Begin(root);
            return *this;
        }

    C& operator*(void)
        {
            return *static_cast<C*>(Get().GetObjectPtr());
        }
    const C& operator*(void) const
        {
            return *static_cast<const C*>(Get().GetObjectPtr());
        }
};

template<class C, class TypeGetter = C>
class CTypeConstIterator : public CTypeIteratorBase<CTreeConstIterator>
{
    typedef CTypeIteratorBase<CTreeConstIterator> CParent;
public:
    typedef typename CParent::TObjectInfo TObjectInfo;

    CTypeConstIterator(void)
        : CParent(TypeGetter::GetTypeInfo())
        {
        }
    CTypeConstIterator(const TObjectInfo& root)
        : CParent(TypeGetter::GetTypeInfo())
        {
            Begin(root);
        }
    CTypeConstIterator(const CObjectInfo& root)
        : CParent(TypeGetter::GetTypeInfo())
        {
            Begin(root);
        }
    CTypeConstIterator(pair<TConstObjectPtr, TTypeInfo> root)
        : CParent(TypeGetter::GetTypeInfo())
        {
            Begin(root);
        }
    CTypeConstIterator(pair<TObjectPtr, TTypeInfo> root)
        : CParent(TypeGetter::GetTypeInfo())
        {
            Begin(root);
        }

    CTypeConstIterator<C>& operator=(const TObjectInfo& root)
        {
            Begin(root);
            return *this;
        }
    CTypeConstIterator<C>& operator=(const CObjectInfo& root)
        {
            Begin(root);
            return *this;
        }
    CTypeConstIterator<C>& operator=(pair<TConstObjectPtr, TTypeInfo> root)
        {
            Begin(root);
            return *this;
        }
    CTypeConstIterator<C>& operator=(pair<TObjectPtr, TTypeInfo> root)
        {
            Begin(root);
            return *this;
        }

    const C& operator*(void) const
        {
            return *static_cast<const C*>(Get().GetObjectPtr());
        }
};

template<typename T>
class CStdTypeIterator : public CTypeIterator<T, CStdTypeInfo<T> >
{
    typedef CTypeIterator<T, CStdTypeInfo<T> > CParent;
public:
    typedef typename CParent::TObjectInfo TObjectInfo;

    CStdTypeIterator(void)
        {
        }
    CStdTypeIterator(const TObjectInfo& root)
        : CParent(root)
        {
        }
    CStdTypeIterator(pair<TObjectPtr, TTypeInfo> root)
        : CParent(root)
        {
        }

    CStdTypeIterator<T>& operator=(const TObjectInfo& root)
        {
            Begin(root);
            return *this;
        }
};

template<typename T>
class CStdTypeConstIterator : public CTypeConstIterator<T, CStdTypeInfo<T> >
{
    typedef CTypeConstIterator<T, CStdTypeInfo<T> > CParent;
public:
    typedef typename CParent::TObjectInfo TObjectInfo;

    CStdTypeConstIterator(void)
        {
        }
    CStdTypeConstIterator(const TObjectInfo& root)
        : CParent(root)
        {
        }
    CStdTypeConstIterator(const CObjectInfo& root)
        : CParent(root)
        {
        }
    CStdTypeConstIterator(pair<TConstObjectPtr, TTypeInfo> root)
        : CParent(root)
        {
        }
    CStdTypeConstIterator(pair<TObjectPtr, TTypeInfo> root)
        : CParent(root)
        {
        }

    CStdTypeConstIterator<T>& operator=(const TObjectInfo& root)
        {
            Begin(root);
            return *this;
        }
    CStdTypeConstIterator<T>& operator=(const CObjectInfo& root)
        {
            Begin(root);
            return *this;
        }
    CStdTypeConstIterator<T>& operator=(pair<TConstObjectPtr, TTypeInfo> root)
        {
            Begin(root);
            return *this;
        }
    CStdTypeConstIterator<T>& operator=(pair<TObjectPtr, TTypeInfo> root)
        {
            Begin(root);
            return *this;
        }
};

typedef CTypeIterator<CObject> CObjectsIterator;
typedef CTypeConstIterator<CObject> CObjectsConstIterator;

template<class Iterator>
class CTypesIteratorBase : public Iterator
{
    typedef Iterator CParent;
public:
    typedef typename CParent::TObjectInfo TObjectInfo;
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

    const TTypeList& GetTypeList(void) const
        {
            return m_TypeList;
        }

    CTypesIteratorBase<Iterator>& AddType(TTypeInfo type)
        {
            m_TypeList.push_back(type);
            return *this;
        }

    CTypesIteratorBase<Iterator>& operator=(const TObjectInfo& root)
        {
            Begin(root);
            return *this;
        }

    typename TObjectInfo::TObjectPtr GetFoundPtr(void) const
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
    virtual bool CanSelect(TTypeInfo type) const;
    virtual bool CanEnter(TTypeInfo type) const;

private:
    TTypeList m_TypeList;
    mutable TTypeInfo m_MatchType;
};

typedef CTypesIteratorBase<CTreeIterator> CTypesIterator;
typedef CTypesIteratorBase<CTreeConstIterator> CTypesConstIterator;

template<class C>
inline
pair<TObjectPtr, TTypeInfo> Begin(C& obj)
{
    return pair<TObjectPtr, TTypeInfo>(&obj, C::GetTypeInfo());
}

template<class C>
inline
pair<TConstObjectPtr, TTypeInfo> Begin(const C& obj)
{
    return pair<TConstObjectPtr, TTypeInfo>(&obj, C::GetTypeInfo());
}

template<class C>
inline
pair<TConstObjectPtr, TTypeInfo> ConstBegin(const C& obj)
{
    return pair<TConstObjectPtr, TTypeInfo>(&obj, C::GetTypeInfo());
}

template<class C>
class Type
{
public:
    static TTypeInfo GetTypeInfo(void)
        {
            return C::GetTypeInfo();
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
