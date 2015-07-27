#ifndef NCBI_AUTOINIT__HPP
#define NCBI_AUTOINIT__HPP

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
 * Author:   Aleksey Grichenko
 *
 * File Description:
 *   Auto-init variables - create on demand, destroy on termination. This
 *                         classes should not be used for static variables.
 *                         If you need a static auto-init variable, use
 *                         CSafeStaticPtr/Ref instead.
 *
 *   CAutoInitPtr_Base::   -- base class for CAutoInitPtr<> and CAutoInitRef<>
 *   CAutoInitPtr<>::      -- create variable on demand, destroy on CAutoInitPtr
 *                            destruction
 *   CAutoInitRef<>::      -- create CObject-derived variable on demand,
 *                            destroy on CAutoInitRef destruction
 *
 */

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_limits.h>
#include <set>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
///  CAutoInitPtr_Base::
///
///    Base class for CAutoInitPtr<> and CAutoInitRef<> templates.

class NCBI_XNCBI_EXPORT CAutoInitPtr_Base
{
public:
    /// User cleanup function type
    typedef void (*FUserCleanup)(void*  ptr);

    ~CAutoInitPtr_Base(void);

    bool IsInitialized(void) const;

protected:
    /// Cleanup function type used by derived classes
    typedef void (*FSelfCleanup)(void** ptr);

    /// Constructor.
    ///
    /// @param self_cleanup
    ///   Cleanup function to be executed on destruction,
    ///   provided by a derived class.
    /// @param user_cleanup
    ///   User-provided cleanup function to be executed on destruction.
    CAutoInitPtr_Base(FSelfCleanup self_cleanup,
                      FUserCleanup user_cleanup = 0)
        : m_Ptr(0),
          m_SelfCleanup(self_cleanup),
          m_UserCleanup(user_cleanup)
    {}

    /// Pointer to the data
    void* m_Ptr;

    DECLARE_CLASS_STATIC_MUTEX(sm_Mutex);

private:
    FSelfCleanup m_SelfCleanup;   // Derived class' cleanup function
    FUserCleanup m_UserCleanup;   // User-provided  cleanup function

    void x_Cleanup(void)
    {
        if ( m_UserCleanup )
            m_UserCleanup(m_Ptr);
        if ( m_SelfCleanup )
            m_SelfCleanup(&m_Ptr);
    }
};


/////////////////////////////////////////////////////////////////////////////
//
///  CAutoInitPtr<>::
///
///    For simple on-demand variables.
///    Create the variable of type "T" on demand,
///    destroy it on the CAutoInitPtr termination.
///    The class should not be used for static variables,
///    use CSafeStaticPtr class instead.

template <class T>
class CAutoInitPtr : public CAutoInitPtr_Base
{
public:
    /// Constructor.
    ///
    /// @param user_cleanup
    ///   User-provided cleanup function to be executed on destruction.
    /// @sa CAutoInitPtr_Base
    CAutoInitPtr(FUserCleanup user_cleanup = 0)
        : CAutoInitPtr_Base(x_SelfCleanup, user_cleanup)
    {}

    /// Create the variable if not created yet, return the reference
    T& Get(void)
    {
        if ( !m_Ptr ) {
            x_Init();
        }
        return *static_cast<T*> (m_Ptr);
    }
    /// Get the existing object or create a new one using the provided
    /// FUserCreate object.
    template <class FUserCreate>
    T& Get(FUserCreate user_create)
    {
        if ( !m_Ptr ) {
            x_Init(user_create);
        }
        return *static_cast<T*> (m_Ptr);
    }

    T* operator -> (void) { return &Get(); }
    T& operator *  (void) { return  Get(); }

    /// Initialize with an existing object. The object MUST be
    /// allocated with "new T" -- it will be destroyed with
    /// "delete object" in the end. Set() works only for
    /// not yet initialized CAutoInitPtr variables.
    void Set(T* object);

private:
    // Initialize the object
    void x_Init(void);

    template <class FUserCreate>
    void x_Init(FUserCreate user_create);

    // "virtual" cleanup function
    static void x_SelfCleanup(void** ptr)
    {
        T* tmp = static_cast<T*> (*ptr);
        *ptr = 0;
        delete tmp;
    }
};



/////////////////////////////////////////////////////////////////////////////
///
///  CAutoInitRef<>::
///
///    For on-demand CObject-derived object.
///    Create the variable of type "T" using CRef<>
///    (to avoid premature destruction).
///    The class should not be used for static variables,
///    use CSafeStaticRef class instead.

template <class T>
class CAutoInitRef : public CAutoInitPtr_Base
{
public:
    /// Constructor.
    ///
    /// @param user_cleanup
    ///   User-provided cleanup function to be executed on destruction.
    /// @sa CAutoInitPtr_Base
    CAutoInitRef(FUserCleanup user_cleanup = 0)
        : CAutoInitPtr_Base(x_SelfCleanup, user_cleanup)
    {}

    /// Create the variable if not created yet, return the reference
    T& Get(void)
    {
        if ( !m_Ptr ) {
            x_Init();
        }
        return *static_cast<T*>(m_Ptr);
    }
    /// Get the existing object or create a new one using the provided
    /// FUserCreate object.
    template <class FUserCreate>
    T& Get(FUserCreate user_create)
    {
        if ( !m_Ptr ) {
            x_Init(user_create);
        }
        return *static_cast<T*>(m_Ptr);
    }

    T* operator -> (void) { return &Get(); }
    T& operator *  (void) { return  Get(); }

    /// Initialize with an existing object. The object MUST be
    /// allocated with "new T" to avoid premature destruction.
    /// Set() works only for un-initialized CAutoInitRef variables.
    void Set(T* object);

private:
    // Initialize the object and the reference
    void x_Init(void);

    template <class FUserCreate>
    void x_Init(FUserCreate user_create);

    // "virtual" cleanup function
    static void x_SelfCleanup(void** ptr)
    {
        T* tmp = static_cast<T*>(*ptr);
        if ( tmp ) {
            tmp->RemoveReference();
            *ptr = 0;
        }
    }
};


/////////////////////////////////////////////////////////////////////////////
//
// Large inline methods

inline
bool CAutoInitPtr_Base::IsInitialized(void) const
{
    CMutexGuard guard(sm_Mutex);
    return m_Ptr != NULL;
}


template <class T>
inline
void CAutoInitPtr<T>::Set(T* object)
{
    CMutexGuard guard(sm_Mutex);
    if ( m_Ptr ) return;
    m_Ptr = object;
}


template <class T>
inline
void CAutoInitPtr<T>::x_Init(void)
{
    CMutexGuard guard(sm_Mutex);
    if ( m_Ptr ) return;
    m_Ptr = new T;
}


template <class T>
template <class FUserCreate>
inline
void CAutoInitPtr<T>::x_Init(FUserCreate user_create)
{
    CMutexGuard guard(sm_Mutex);
    if ( m_Ptr ) return;
    m_Ptr = user_create();
}


template <class T>
inline
void CAutoInitRef<T>::Set(T* object)
{
    CMutexGuard guard(sm_Mutex);
    if ( m_Ptr ) return;
    if ( object ) {
        object->AddReference();
        m_Ptr = object;
    }
}


template <class T>
inline
void CAutoInitRef<T>::x_Init(void)
{
    CMutexGuard guard(sm_Mutex);
    if ( m_Ptr ) return;
    CRef<T> ref(new T);
    ref->AddReference();
    m_Ptr = ref.Release();
}


template <class T>
template <class FUserCreate>
inline
void CAutoInitRef<T>::x_Init(FUserCreate user_create)
{
    CMutexGuard guard(sm_Mutex);
    if ( m_Ptr ) return;
    CRef<T> ref(user_create());
    if ( ref ) {
        ref->AddReference();
        m_Ptr = ref.Release();
    }
}


END_NCBI_SCOPE

#endif  /* NCBI_AUTOINIT__HPP */
