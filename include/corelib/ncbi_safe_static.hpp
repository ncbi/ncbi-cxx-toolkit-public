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

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbithr.hpp>
#include <stack>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CSafeStaticPtr_Base::
//
//    Base class for CSafeStaticPtr<> and CSafeStaticRef<> templates.
//


// Base class for CSafeStaticPtr<> and CSafeStaticRef<> templates
class NCBI_XNCBI_EXPORT CSafeStaticPtr_Base
{
public:
    // User cleanup function type
    typedef void (*FSelfCleanup)(void** ptr);
    typedef void (*FUserCleanup)(void*  ptr);

    // Set user-provided cleanup function to be executed on destruction
    CSafeStaticPtr_Base(FSelfCleanup self_cleanup,
                        FUserCleanup user_cleanup = 0)
        : m_SelfCleanup(self_cleanup),
          m_UserCleanup(user_cleanup)
    {}

protected:
    void* m_Ptr;          // Pointer to the data

    // Prepare to the object initialization: check current thread, lock
    // the mutex and store its state to "mutex_locked", return "true"
    // if the object must be created or "false" if already created.
    bool Init_Lock(bool* mutex_locked);
    // Finalize object initialization: release the mutex if "mutex_locked"
    void Init_Unlock(bool mutex_locked);

private:
    FSelfCleanup m_SelfCleanup;  // Derived class' cleanup function
    FUserCleanup m_UserCleanup;  // User-provided  cleanup function

    // To be called by CSafeStaticGuard on the program termination
    friend class CSafeStaticGuard;
    void Cleanup(void)
    {
        if ( m_UserCleanup )  m_UserCleanup(m_Ptr);
        if ( m_SelfCleanup )  m_SelfCleanup(&m_Ptr);
    }

};



/////////////////////////////////////////////////////////////////////////////
//
//  CSafeStaticPtr<>::
//
//    For simple on-demand variables.
//    Create the variable of type "T" on demand,
//    destroy it on the program termination.
//


template <class T>
class CSafeStaticPtr : public CSafeStaticPtr_Base
{
public:
    // Set the cleanup function to be called on variable destruction
    CSafeStaticPtr(FUserCleanup user_cleanup = 0)
        : CSafeStaticPtr_Base(SelfCleanup, user_cleanup)
    {}

    // Create the variable if not created yet, return the reference
    T& Get(void)
    {
        if ( !m_Ptr ) {
            Init();
        }
        return *static_cast<T*> (m_Ptr);
    }

    T* operator -> (void) { return &Get(); }
    T& operator *  (void) { return  Get(); }

    // Initialize with an existing object. The object MUST be
    // allocated with "new T" -- it will be destroyed with
    // "delete object" in the end. Set() works only for
    // not yet initialized safe-static variables.
    void Set(T* object);

private:
    // Initialize the object
    void Init(void);

    // "virtual" cleanup function
    static void SelfCleanup(void** ptr)
    {
        T* tmp = static_cast<T*> (*ptr);
        *ptr = 0;
        delete tmp;
    }
};



/////////////////////////////////////////////////////////////////////////////
//
//  CSafeStaticRef<>::
//
//    For on-demand CObject-derived object.
//    Create the variable of type "T" using CRef<>
//    (to avoid premature destruction).
//


template <class T>
class CSafeStaticRef : public CSafeStaticPtr_Base
{
public:
    // Set the cleanup function to be called on variable destruction
    CSafeStaticRef(FUserCleanup user_cleanup = 0)
        : CSafeStaticPtr_Base(SelfCleanup, user_cleanup)
    {}

    // Create the variable if not created yet, return the reference
    T& Get(void)
    {
        if ( !m_Ptr ) {
            Init();
        }
        return static_cast<CRef<T>*> (m_Ptr)->GetObject();
    }

    T* operator -> (void) { return &Get(); }
    T& operator *  (void) { return  Get(); }

    // Initialize with an existing object. The object MUST be
    // allocated with "new T" to avoid premature destruction.
    // Set() works only for un-initialized safe-static variables.
    void Set(T* object);

private:
    // Initialize the object and the reference
    void Init(void);

    // "virtual" cleanup function
    static void SelfCleanup(void** ptr)
    {
        CRef<T>* tmp = static_cast< CRef<T>* > (*ptr);
        *ptr = 0;
        delete tmp;
    }
};



/////////////////////////////////////////////////////////////////////////////
//
//  CSafeStaticGuard::
//
//    Register all on-demand variables,
//    destroy them on the program termination.

class NCBI_XNCBI_EXPORT CSafeStaticGuard
{
public:
    // Check if already initialized. If not - create the stack,
    // otherwise just increment the reference count.
    CSafeStaticGuard(void);

    // Check reference count, and if it is zero, then destroy
    // all registered variables.
    ~CSafeStaticGuard(void);

    // Add new on-demand variable to the cleanup stack.
    static void Register(CSafeStaticPtr_Base* ptr)
    {
        if ( !sm_Stack ) {
            Get();
        }
        sm_Stack->push(ptr);
    }

private:
    // Initialize the guard, return pointer to it.
    static CSafeStaticGuard* Get(void);

    // Stack to keep registered variables.
    typedef stack<CSafeStaticPtr_Base*> TStack;
    static TStack* sm_Stack;

    // Reference counter. The stack is destroyed when
    // the last reference is removed.
    static int sm_RefCount;
};



/////////////////////////////////////////////////////////////////////////////
//
// This static variable must be present in all modules using
// on-demand variables. The guard must be created first
// regardless of the modules initialization order.
//

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
void CSafeStaticPtr<T>::Init(void)
{
    bool mutex_locked = false;
    if ( Init_Lock(&mutex_locked) ) {
        // Create the object and register for cleanup
        try {
            m_Ptr = new T;
            CSafeStaticGuard::Register(this);
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
            m_Ptr = new CRef<T> (object);
            CSafeStaticGuard::Register(this);
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
void CSafeStaticRef<T>::Init(void)
{
    bool mutex_locked = false;
    if ( Init_Lock(&mutex_locked) ) {
        // Create the object and register for cleanup
        try {
            m_Ptr = new CRef<T> (new T);
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



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2004/04/26 14:28:59  ucko
 * Move large inline methods [Set(), Init()] from CSafeStatic{Ptr,Ref}'s
 * definitions to the end of the file, both to reduce clutter and because
 * GCC 3.4 insists on seeing CSafeStaticGuard first.
 *
 * Revision 1.11  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.10  2002/09/19 20:05:41  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.9  2002/07/15 18:17:50  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.8  2002/07/11 14:17:53  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.7  2002/04/11 20:39:15  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.6  2001/12/07 18:48:48  grichenk
 * Improved CSafeStaticGuard behaviour.
 *
 * Revision 1.5  2001/08/24 13:42:37  grichenk
 * Added CSafeStaticXXX::Set() methods for initialization with an
 * existing object
 *
 * Revision 1.4  2001/04/06 15:45:26  grichenk
 * Modified SelfCleanup() methods for more safety
 *
 * Revision 1.3  2001/03/30 23:10:10  grichenk
 * Protected from double initializations and deadlocks in multithread
 * environment
 *
 * Revision 1.2  2001/03/26 21:01:41  vakatov
 * CSafeStaticPtr, CSafeStaticRef -- added "operator *"
 *
 * Revision 1.1  2001/03/26 20:38:33  vakatov
 * Initial revision (by A.Grichenko)
 *
 * ===========================================================================
 */

#endif  /* NCBI_SAFE_STATIC__HPP */
