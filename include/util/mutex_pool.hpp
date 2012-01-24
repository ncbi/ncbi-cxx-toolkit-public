#ifndef UTIL___MUTEX_POOL__HPP
#define UTIL___MUTEX_POOL__HPP

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
*   CMutexPool -- to distribute mutex pool among several objects.
*
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>

#include <list>

BEGIN_NCBI_SCOPE

class CInitMutexPool;
class CInitMutex_Base;
class CInitGuard;

////////////////////////////////////////////////////////////////////
//
//  CMutexPool::
//
//    Distribute a mutex pool among multiple objects
//


class NCBI_XUTIL_EXPORT CInitMutexPool
{
public:
    CInitMutexPool(void);
    ~CInitMutexPool(void);

    class CPoolMutex : public CObject
    {
    public:
        CPoolMutex(CInitMutexPool& pool)
            : m_Pool(pool)
            {
            }
        ~CPoolMutex(void)
            {
            }

        CInitMutexPool& GetPool(void) const
            {
                return m_Pool;
            }
        CMutex& GetMutex(void)
            {
                return m_Mutex;
            }

    private:
        CInitMutexPool& m_Pool;
        CMutex      m_Mutex;
    };
    typedef CPoolMutex TMutex;

protected:
    friend class CInitGuard;

    bool AcquireMutex(CInitMutex_Base& init, CRef<TMutex>& mutex);
    void ReleaseMutex(CInitMutex_Base& init, CRef<TMutex>& mutex);

private:
    typedef list< CRef<TMutex> > TMutexList;
    TMutexList m_MutexList;
    CFastMutex m_Pool_Mtx;

private:
    CInitMutexPool(const CInitMutexPool&);
    const CInitMutexPool& operator=(const CInitMutexPool&);
};


class NCBI_XUTIL_EXPORT CInitMutex_Base
{
public:
    DECLARE_OPERATOR_BOOL_REF(m_Object);

protected:
    CInitMutex_Base(void)
        {
        }
    // Copy constructor to allow CInitMutex_Base placement in STL containers.
    // It doesn't copy mutex/object, just verifies that source is empty too.
    CInitMutex_Base(const CInitMutex_Base& _DEBUG_ARG(mutex))
        {
            _ASSERT(!mutex.m_Mutex && !mutex.m_Object);
        }
    ~CInitMutex_Base(void)
        {
            _ASSERT(!m_Mutex || m_Mutex->ReferencedOnlyOnce());
        }

    friend class CInitMutexPool;

    typedef CInitMutexPool::TMutex TMutex;

    CRef<TMutex>  m_Mutex;
    CRef<CObject> m_Object;

private:
    const CInitMutex_Base& operator=(const CInitMutex_Base&);
};


template<class C>
class CInitMutex : public CInitMutex_Base
{
public:
    typedef C TObjectType;

    void Reset(void)
        {
            m_Object.Reset();
        }
    void Reset(TObjectType* object)
        {
            m_Object.Reset(object);
        }

    inline
    TObjectType& GetObject(void)
        {
            return static_cast<TObjectType&>(m_Object.GetObject());
        }
    inline
    const TObjectType& GetObject(void) const
        {
            return static_cast<const TObjectType&>(m_Object.GetObject());
        }
    inline
    TObjectType* GetPointer(void)
        {
            return static_cast<TObjectType*>(m_Object.GetPointer());
        }
    inline
    const TObjectType* GetPointer(void) const
        {
            return static_cast<const TObjectType*>(m_Object.GetPointer());
        }
    inline
    TObjectType* GetPointerOrNull(void)
        {
            return static_cast<TObjectType*>(m_Object.GetPointerOrNull());
        }
    inline
    const TObjectType* GetPointerOrNull(void) const
        {
            return
                static_cast<const TObjectType*>(m_Object.GetPointerOrNull());
        }

    inline
    TObjectType& operator*(void)
        {
            return GetObject();
        }
    inline
    TObjectType* operator->(void)
        {
            return GetPointer();
        }
    inline
    const TObjectType& operator*(void) const
        {
            return GetObject();
        }
    inline
    const TObjectType* operator->(void) const
        {
            return GetPointer();
        }

    const CInitMutex<TObjectType>& operator=(const CRef<TObjectType>& ref)
        {
            m_Object.Reset(ref.GetNCPointerOrNull());
            return *this;
        }
    operator CRef<TObjectType>(void) const
        {
            return CRef<TObjectType>(const_cast<TObjectType*>(GetPointer()));
        }
    operator CConstRef<TObjectType>(void) const
        {
            return CConstRef<TObjectType>(GetPointer());
        }
};


class CInitGuard
{
public:
    CInitGuard(CInitMutex_Base& init, CInitMutexPool& pool)
        : m_Init(init), m_Guard(eEmptyGuard)
        {
            if ( !init && pool.AcquireMutex(init, m_Mutex) ) {
                m_Guard.Guard(m_Mutex->GetMutex());
                if ( init ) {
                    x_Release();
                }
            }
        }
    ~CInitGuard(void)
        {
            Release();
        }

    void Release(void)
        {
            if ( m_Mutex ) {
                x_Release();
            }
        }

    // true means that this thread should perform initialization
    DECLARE_OPERATOR_BOOL(!m_Init);

protected:
    typedef CInitMutexPool::TMutex TMutex;

    void x_Release(void)
        {
            m_Mutex->GetPool().ReleaseMutex(m_Init, m_Mutex);
            m_Guard.Release();
        }

    CInitMutex_Base& m_Init;
    CRef<TMutex>     m_Mutex;
    CMutexGuard      m_Guard;

private:
    CInitGuard(const CInitGuard&);
    const CInitGuard& operator=(const CInitGuard&);
};


END_NCBI_SCOPE

#endif  /* UTIL___MUTEX_POOL__HPP */
