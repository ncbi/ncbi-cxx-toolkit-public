#ifndef NCBI_SAFE_STATIC__HPP
#define NCBI_SAFE_STATIC__HPP

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
 *   Static variables safety - create on demand, destroy on termination
 *
 *   CSafeStaticPtr_Base::   --  base class for CSafePtr<> and CSafeRef<>
 *   CSafeStaticPtr<>::      -- create variable on demand, destroy on program
 *                              termination (see NCBIOBJ for CSafeRef<> class)
 *   CSafeStaticRef<>::      -- create variable on demand, destroy on program
 *                              termination (see NCBIOBJ for CSafeRef<> class)
 *   CSafeStaticGuard::      -- guarantee for CSafePtr<> and CSafeRef<>
 *                              destruction and cleanup
 *
 */

/// @file ncbi_safe_static.hpp
/// Static variables safety - create on demand, destroy on application
/// termination.

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_limits.h>
#include <set>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
///  CSafeStaticLifeSpan::
///
///    Class for specifying safe static object life span.
///

class NCBI_XNCBI_EXPORT CSafeStaticLifeSpan
{
public:
    /// Predefined life spans for the safe static objects
    enum ELifeSpan {
        eLifeSpan_Min      = kMin_Int, ///< std static, not adjustable
        eLifeSpan_Shortest = -20000,
        eLifeSpan_Short    = -10000,
        eLifeSpan_Normal   = 0,
        eLifeSpan_Long     = 10000,
        eLifeSpan_Longest  = 20000
    };
    /// Constructs a life span object from basic level and adjustment.
    /// Generates warning (and assertion in debug mode) if the adjustment
    /// argument is too big (<= -5000 or >= 5000). If span is eLifeSpan_Min
    /// "adjust" is ignored.
    CSafeStaticLifeSpan(ELifeSpan span, int adjust = 0);

    /// Get life span value.
    int GetLifeSpan(void) const { return m_LifeSpan; }

    /// Get default life span (set to eLifeSpan_Min).
    static CSafeStaticLifeSpan& GetDefault(void);

private:
    int m_LifeSpan;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CSafeStaticPtr_Base::
///
///    Base class for CSafeStaticPtr<> and CSafeStaticRef<> templates.
///

class NCBI_XNCBI_EXPORT CSafeStaticPtr_Base
{
public:
    /// User cleanup function type
    typedef void (*FUserCleanup)(void*  ptr);

    /// Life span
    typedef CSafeStaticLifeSpan TLifeSpan;

    ~CSafeStaticPtr_Base(void);

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
    /// @param life_span
    ///   Life span allows to control destruction of objects. Objects with
    ///   the same life span are destroyed in the order reverse to their
    ///   creation order.
    ///   @sa CSafeStaticLifeSpan
    CSafeStaticPtr_Base(FSelfCleanup self_cleanup,
                        FUserCleanup user_cleanup = 0,
                        TLifeSpan life_span = TLifeSpan::GetDefault())
        : m_SelfCleanup(self_cleanup),
          m_UserCleanup(user_cleanup),
          m_LifeSpan(life_span.GetLifeSpan()),
          m_CreationOrder(x_GetCreationOrder())
    {}

    /// Pointer to the data
    void* m_Ptr;

    /// Prepare to the object initialization: check current thread, lock
    /// the mutex and store its state to "mutex_locked", return "true"
    /// if the object must be created or "false" if already created.
    bool Init_Lock(bool* mutex_locked);
    /// Finalize object initialization: release the mutex if "mutex_locked".
    void Init_Unlock(bool mutex_locked);

private:
    friend class CSafeStatic_Less;

    FSelfCleanup m_SelfCleanup;   // Derived class' cleanup function
    FUserCleanup m_UserCleanup;   // User-provided  cleanup function
    int          m_LifeSpan;      // Life span of the object
    int          m_CreationOrder; // Creation order of the object

    static int x_GetCreationOrder(void);

    // Return true if the object should behave like regular static
    // (no delayed destruction).
    bool x_IsStdStatic(void) const
    {
        return m_LifeSpan == int(CSafeStaticLifeSpan::eLifeSpan_Min);
    }

    // To be called by CSafeStaticGuard on the program termination
    friend class CSafeStaticGuard;
    void x_Cleanup(void)
    {
        if ( m_UserCleanup )
            m_UserCleanup(m_Ptr);
        if ( m_SelfCleanup )
            m_SelfCleanup(&m_Ptr);
    }

};


/// Comparison for safe static ptrs. Defines order of objects' destruction:
/// short living objects go first; if life span of several objects is the same,
/// the order of destruction is reverse to the order of their creation.
class CSafeStatic_Less
{
public:
    typedef CSafeStaticPtr_Base* TPtr;
    bool operator()(const TPtr& ptr1, const TPtr& ptr2) const
    {
        if (ptr1->m_LifeSpan == ptr2->m_LifeSpan) {
            return ptr1->m_CreationOrder > ptr2->m_CreationOrder;
        }
        return ptr1->m_LifeSpan < ptr2->m_LifeSpan;
    }
};


/////////////////////////////////////////////////////////////////////////////
///
///  CSafeStaticPtr<>::
///
///    For simple on-demand variables.
///    Create the variable of type "T" on demand,
///    destroy it on the program termination.
///    Should be used only as static object. Otherwise
///    the correct initialization is not guaranteed.

template <class T>
class CSafeStaticPtr : public CSafeStaticPtr_Base
{
public:
    typedef CSafeStaticLifeSpan TLifeSpan;

    /// Constructor.
    ///
    /// @param user_cleanup
    ///   User-provided cleanup function to be executed on destruction.
    /// @param life_span
    ///   Life span allows to control destruction of objects.
    /// @sa CSafeStaticPtr_Base
    CSafeStaticPtr(FUserCleanup user_cleanup = 0,
                   TLifeSpan life_span = TLifeSpan::GetDefault())
        : CSafeStaticPtr_Base(x_SelfCleanup, user_cleanup, life_span)
    {}

    /// Create the variable if not created yet, return the reference.
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
    /// not yet initialized safe-static variables.
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
///  CSafeStaticRef<>::
///
///    For on-demand CObject-derived object.
///    Create the variable of type "T" using CRef<>
///    (to avoid premature destruction).
///    Should be used only as static object. Otherwise
///    the correct initialization is not guaranteed.

template <class T>
class CSafeStaticRef : public CSafeStaticPtr_Base
{
public:
    typedef CSafeStaticLifeSpan TLifeSpan;

    /// Constructor.
    ///
    /// @param user_cleanup
    ///   User-provided cleanup function to be executed on destruction.
    /// @param life_span
    ///   Life span allows to control destruction of objects.
    /// @sa CSafeStaticPtr_Base
    CSafeStaticRef(FUserCleanup user_cleanup = 0,
                   TLifeSpan life_span = TLifeSpan::GetDefault())
        : CSafeStaticPtr_Base(x_SelfCleanup, user_cleanup, life_span)
    {}

    /// Create the variable if not created yet, return the reference.
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
    /// Set() works only for un-initialized safe-static variables.
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
///
///  CSafeStaticGuard::
///
///    Register all on-demand variables,
///    destroy them on the program termination.

class NCBI_XNCBI_EXPORT CSafeStaticGuard
{
public:
    /// Check if already initialized. If not - create the stack,
    /// otherwise just increment the reference count.
    CSafeStaticGuard(void);

    /// Check reference count, and if it is zero, then destroy
    /// all registered variables.
    ~CSafeStaticGuard(void);

    /// Add new on-demand variable to the cleanup stack.
    static void Register(CSafeStaticPtr_Base* ptr)
    {
        if ( ptr->x_IsStdStatic() ) {
            // Do not add the object to the stack
            return;
        }
        if ( !sm_Stack ) {
            x_Get();
        }
        sm_Stack->insert(ptr);
    }

private:
    // Initialize the guard, return pointer to it.
    static CSafeStaticGuard* x_Get(void);

    // Stack to keep registered variables.
    typedef multiset<CSafeStaticPtr_Base*, CSafeStatic_Less> TStack;
    static TStack* sm_Stack;

    // Reference counter. The stack is destroyed when
    // the last reference is removed.
    static int sm_RefCount;
};



/////////////////////////////////////////////////////////////////////////////
///
/// This static variable must be present in all modules using
/// on-demand static variables. The guard must be created first
/// regardless of the modules initialization order.

static CSafeStaticGuard s_CleanupGuard;


/////////////////////////////////////////////////////////////////////////////
//
// Large inline methods

template <class T>
inline
void CSafeStaticPtr<T>::Set(T* object)
{
    bool mutex_locked = false;
    if ( Init_Lock(&mutex_locked) ) {
        // Set the new object and register for cleanup
        try {
            m_Ptr = object;
            CSafeStaticGuard::Register(this);
        }
        catch (CException& e) {
            Init_Unlock(mutex_locked);
            NCBI_RETHROW_SAME(e, "CSafeStaticPtr::Set: Register() failed");
        }
        catch (...) {
            Init_Unlock(mutex_locked);
            NCBI_THROW(CCoreException,eCore,
                       "CSafeStaticPtr::Set: Register() failed");
        }
    }
    Init_Unlock(mutex_locked);
}


template <class T>
inline
void CSafeStaticPtr<T>::x_Init(void)
{
    bool mutex_locked = false;
    if ( Init_Lock(&mutex_locked) ) {
        // Create the object and register for cleanup
        T* ptr = 0;
        try {
            ptr = new T;
            CSafeStaticGuard::Register(this);
            m_Ptr = ptr;
        }
        catch (CException& e) {
            delete ptr;
            Init_Unlock(mutex_locked);
            NCBI_RETHROW_SAME(e, "CSafeStaticPtr::Init: Register() failed");
        }
        catch (...) {
            delete ptr;
            Init_Unlock(mutex_locked);
            NCBI_THROW(CCoreException,eCore,
                       "CSafeStaticPtr::Init: Register() failed");
        }
    }
    Init_Unlock(mutex_locked);
}


template <class T>
template <class FUserCreate>
inline
void CSafeStaticPtr<T>::x_Init(FUserCreate user_create)
{
    bool mutex_locked = false;
    if ( Init_Lock(&mutex_locked) ) {
        // Create the object and register for cleanup
        try {
            T* ptr = user_create();
            m_Ptr = ptr;
            if ( m_Ptr ) {
                CSafeStaticGuard::Register(this);
            }
        }
        catch (CException& e) {
            Init_Unlock(mutex_locked);
            NCBI_RETHROW_SAME(e, "CSafeStaticPtr::Init: Register() failed");
        }
        catch (...) {
            Init_Unlock(mutex_locked);
            NCBI_THROW(CCoreException,eCore,
                       "CSafeStaticPtr::Init: Register() failed");
        }
    }
    Init_Unlock(mutex_locked);
}


template <class T>
inline
void CSafeStaticRef<T>::Set(T* object)
{
    bool mutex_locked = false;
    if ( Init_Lock(&mutex_locked) ) {
        // Set the new object and register for cleanup
        try {
            if ( object ) {
                object->AddReference();
                m_Ptr = object;
                CSafeStaticGuard::Register(this);
            }
        }
        catch (CException& e) {
            Init_Unlock(mutex_locked);
            NCBI_RETHROW_SAME(e, "CSafeStaticRef::Set: Register() failed");
        }
        catch (...) {
            Init_Unlock(mutex_locked);
            NCBI_THROW(CCoreException,eCore,
                       "CSafeStaticRef::Set: Register() failed");
        }
    }
    Init_Unlock(mutex_locked);
}


template <class T>
inline
void CSafeStaticRef<T>::x_Init(void)
{
    bool mutex_locked = false;
    if ( Init_Lock(&mutex_locked) ) {
        // Create the object and register for cleanup
        try {
            T* ptr = new T;
            ptr->AddReference();
            m_Ptr = ptr;
            CSafeStaticGuard::Register(this);
        }
        catch (CException& e) {
            Init_Unlock(mutex_locked);
            NCBI_RETHROW_SAME(e, "CSafeStaticRef::Init: Register() failed");
        }
        catch (...) {
            Init_Unlock(mutex_locked);
            NCBI_THROW(CCoreException,eCore,
                       "CSafeStaticRef::Init: Register() failed");
        }
    }
    Init_Unlock(mutex_locked);
}


template <class T>
template <class FUserCreate>
inline
void CSafeStaticRef<T>::x_Init(FUserCreate user_create)
{
    bool mutex_locked = false;
    if ( Init_Lock(&mutex_locked) ) {
        // Create the object and register for cleanup
        try {
            CRef<T> ref(user_create());
            if ( ref ) {
                ref->AddReference();
                m_Ptr = ref.Release();
                CSafeStaticGuard::Register(this);
            }
        }
        catch (CException& e) {
            Init_Unlock(mutex_locked);
            NCBI_RETHROW_SAME(e, "CSafeStaticRef::Init: Register() failed");
        }
        catch (...) {
            Init_Unlock(mutex_locked);
            NCBI_THROW(CCoreException,eCore,
                       "CSafeStaticRef::Init: Register() failed");
        }
    }
    Init_Unlock(mutex_locked);
}



END_NCBI_SCOPE

#endif  /* NCBI_SAFE_STATIC__HPP */
