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
 *   CSafeStaticGuard::      -- guarantee for CSafePtr<> and CSafeRef<>
 *                              destruction and cleanup
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbimtx.hpp>
#include <memory>
#include <assert.h>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CSafeStaticPtr_Base::
//

// Protective mutex and the owner thread ID to avoid
// multiple initializations and deadlocks
DEFINE_STATIC_FAST_MUTEX(s_Mutex);

static CThreadSystemID s_MutexOwner;
// true if s_MutexOwner has been set (while the mutex is locked)
static bool            s_MutexLocked;

bool CSafeStaticPtr_Base::Init_Lock(bool* mutex_locked)
{
    // Check if already locked by the same thread to avoid deadlock
    // in case of nested calls to Get() by T constructor
    // Lock only if unlocked or locked by another thread
    // to prevent initialization by another thread
    CThreadSystemID id = CThreadSystemID::GetCurrent();
    if (!s_MutexLocked  ||  s_MutexOwner != id) {
        s_Mutex.Lock();
        s_MutexLocked = true;
        *mutex_locked = true;
        s_MutexOwner = id;
    }
    return m_Ptr == 0;
}


void CSafeStaticPtr_Base::Init_Unlock(bool mutex_locked)
{
    // Unlock the mutex only if it was locked by the same call to Get()
    if ( mutex_locked ) {
        s_MutexLocked = false;
        s_Mutex.Unlock();
    }
}


/////////////////////////////////////////////////////////////////////////////
//
//  CSafeStaticGuard::
//

// Cleanup stack to keep all on-demand variables
CSafeStaticGuard::TStack* CSafeStaticGuard::sm_Stack;


// CSafeStaticGuard reference counter
int CSafeStaticGuard::sm_RefCount;


CSafeStaticGuard::CSafeStaticGuard(void)
{
    // Initialize the guard only once
    if (sm_RefCount == 0) {
        CSafeStaticGuard::sm_Stack = new CSafeStaticGuard::TStack;
    }

    sm_RefCount++;
}

static CSafeStaticGuard* sh_CleanupGuard;

CSafeStaticGuard::~CSafeStaticGuard(void)
{
    if ( sh_CleanupGuard ) {
        CSafeStaticGuard* tmp = sh_CleanupGuard;
        sh_CleanupGuard = 0;
        delete tmp;
    }
    // If this is not the last reference, then do not destroy stack
    if (--sm_RefCount > 0) {
        return;
    }
    assert(sm_RefCount == 0);

    // Call Cleanup() for all variables registered
    while ( !sm_Stack->empty() ) {
        sm_Stack->top()->Cleanup();
        sm_Stack->pop();
    }

    delete sm_Stack;
    sm_Stack = 0;
}


// Global guard - to prevent premature destruction by e.g. GNU compiler
// (it destroys all local static variables before any global one)
static CSafeStaticGuard sg_CleanupGuard;


// Initialization of the guard
CSafeStaticGuard* CSafeStaticGuard::Get(void)
{
    // Local static variable - to initialize the guard
    // as soon as the function is called (global static
    // variable may be still uninitialized at this moment)
    static CSafeStaticGuard sl_CleanupGuard;
    if ( !sh_CleanupGuard )
        sh_CleanupGuard = new CSafeStaticGuard;
    return &sl_CleanupGuard;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2004/05/14 13:59:26  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.6  2002/09/19 20:05:42  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.5  2002/04/11 20:00:45  ivanov
 * Returned standard assert() vice CORE_ASSERT()
 *
 * Revision 1.4  2002/04/10 18:39:10  ivanov
 * Changed assert() to CORE_ASSERT()
 *
 * Revision 1.3  2001/12/07 18:48:50  grichenk
 * Improved CSafeStaticGuard behaviour.
 *
 * Revision 1.2  2001/03/30 23:10:12  grichenk
 * Protected from double initializations and deadlocks in multithread
 * environment
 *
 * Revision 1.1  2001/03/26 20:38:35  vakatov
 * Initial revision (by A.Grichenko)
 *
 * ===========================================================================
 */
