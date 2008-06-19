#ifndef WEAK_REF_ORIG__HPP
#define WEAK_REF_ORIG__HPP

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
 * Authors:  Victor Joukov
 *
 * File Description:
 *   Weak reference
 *
 */

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

template<class C>
class CIntrusiveLocker
{
public:
    typedef C TObjectType;
    void Lock(const TObjectType* object) const
    {
        object->AddReference();
    }
    void Relock(const TObjectType* object) const
    {
        Lock(object);
    }
    void Unlock(const TObjectType* object) const
    {
        object->RemoveReference();
    }
};


// Proxy is created for every weak-referenced object in
// CWeakObjectBase constructor and lives longer than object
// itself, as long as there is at least one weak reference for
// the object. Weak reference is actually a CRef referring to
// weak proxy, which in turn, stores pointer to real object.
// Can be re-implemented in terms of CIntrusiveLocker
template<class C>
class CWeakProxy : public CObject
{
public:
    typedef C TObjectType;
    typedef CRef<C> TObjectRefType;

    CWeakProxy(C* object)
    {
        m_Object = object;
    }
    TObjectRefType WeakLock(void)
    {
        CMutexGuard guard(m_Lock);
        if (!m_Object) return TObjectRefType();
        return TObjectRefType(m_Object);
    }
    const TObjectRefType WeakLock(void) const
    {
        CMutexGuard guard(m_Lock);
        if (!m_Object) return TObjectRefType();
        return TObjectRefType(m_Object);
    }
private:
    CWeakProxy(const CWeakProxy &);
    CWeakProxy& operator=(const CWeakProxy &);
public: //friend class C;
    void Deleting(void)
    {
        // Guarded at caller
        m_Object = NULL;
    }
    CMutex& GetLock(void)
    {
        return m_Lock;
    }
private:
    mutable CMutex m_Lock;
    C* m_Object;
};


template <class C>
class CWeakObjectBase
{
public:
    typedef CWeakObjectBase<C> TThisType;
    typedef CWeakProxy<TThisType> TWeakProxy;
    CWeakObjectBase()
        : m_RefCounter(0)
    {
        m_ProxyRef.Reset(new TWeakProxy(this));
    }
    virtual ~CWeakObjectBase() {}
    // For CRef
    typedef CIntrusiveLocker<TThisType> TLockerType;
    void AddReference(void) const
    {
        // Proxy always exists, it's created in constructor
        // and CRef'd from object itself.
        CMutexGuard guard(m_ProxyRef->GetLock());
        m_RefCounter++;
    }
    void RemoveReference(void) const
    {
        bool delete_this = false;
        {{
            CMutexGuard guard(m_ProxyRef->GetLock());
            if (--m_RefCounter <= 0) {
                m_ProxyRef->Deleting();
                delete_this = true;
            }
            // We have to release lock here before deleting the object itself,
            // otherwise if no more weakrefs point to the object it will trigger
            // deleting proxy with mutex locked and lead to exception in
            // destructor. We can safely release lock here because weak proxy
            // does not have pinter to us anymore.
        }}
        if (delete_this) {
            delete this;
        }
    }
    // For CWeakRefOrig
    TWeakProxy* GetWeakProxy(void)
    {
        return m_ProxyRef.GetPointer();
    }
private:
    // We must use single lock, guarding both shared AND weak ref/unref,
    // and having it in proxy we don't need atomic counter here.
    mutable unsigned m_RefCounter;
    //
    mutable CRef<TWeakProxy> m_ProxyRef;
};


/* Attempt to use base-driven template specialization
Doesn't work for some reason
template <class C>
class CLockerTraits< typename CWeakObjectBase<C>::HierarchyId >
{
public:
    typedef CIntrusiveLocker< CWeakObjectBase<C> > TLockerType;
};
*/


template <class C>
class CWeakRefOrig : public CRef< CWeakProxy<C> >
{
public:
    typedef CWeakProxy<C> TProxyType;
    typedef CRef<TProxyType> TParentType;
    typedef C TObjectType;
    typedef CRef<C> TObjectRefType;

    CWeakRefOrig() : CRef<TProxyType>()
    { }

    CWeakRefOrig(TObjectRefType ptr) :
        CRef<TProxyType>((TProxyType *)ptr->GetWeakProxy())
    { }

    TObjectRefType Lock(void)
    {
        return CRef<CWeakProxy<C> >::GetObject().WeakLock();
    }

    const TObjectRefType Lock(void) const
    {
        return CRef<CWeakProxy<C> >::GetObject().WeakLock();
    }
};

END_NCBI_SCOPE

#endif /* WEAK_REF_ORIG__HPP */
