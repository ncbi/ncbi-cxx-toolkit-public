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
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.2  2001/03/26 21:01:41  vakatov
 * CSafeStaticPtr, CSafeStaticRef -- added "operator *"
 *
 * Revision 1.1  2001/03/26 20:38:33  vakatov
 * Initial revision (by A.Grichenko)
 *
 * ===========================================================================
 */

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>
#include <stack>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CSafeStaticPtr_Base::
//
//    Base class for CSafeStaticPtr<> and CSafeStaticRef<> templates.
//


// Base class for CSafeStaticPtr<> and CSafeStaticRef<> templates
class CSafeStaticPtr_Base
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
    void*         m_Ptr;          // Pointer to the data

private:
    FSelfCleanup  m_SelfCleanup;  // Derived class' cleanup function
    FUserCleanup  m_UserCleanup;  // User-provided  cleanup function

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
            m_Ptr = new T;
            CSafeStaticGuard::Get()->Register(this);
        }
        return *static_cast<T*> (m_Ptr);
    }

    T* operator -> (void) { return &Get(); }
    T& operator *  (void) { return  Get(); }

private:
    // "virtual" cleanup function
    static void SelfCleanup(void** ptr)
    {
        delete static_cast<T*>(*ptr);
        *ptr = 0;
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
        CRef<T>* obj = static_cast<CRef<T>*> (m_Ptr);
        if ( !obj ) {
            m_Ptr = obj = new CRef<T>;
            obj->Reset(new T);
            CSafeStaticGuard::Get()->Register(this);
        }
        return obj->GetObject();
    }

    T* operator -> (void) { return &Get(); }
    T& operator *  (void) { return  Get(); }

    // "virtual" cleanup function
    static void SelfCleanup(void** ptr)
    {
        delete static_cast< CRef<T>* > (*ptr);
        *ptr = 0;
    }
};



/////////////////////////////////////////////////////////////////////////////
//
//  CSafeStaticGuard::
//
//    Register all on-demand variables,
//    destroy them on the program termination.

class CSafeStaticGuard
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
        sm_Stack->push(ptr);
    }

    // Initialize the guard, return pointer to it.
    static CSafeStaticGuard* Get(void);

private:
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


END_NCBI_SCOPE

#endif  /* NCBI_SAFE_STATIC__HPP */
