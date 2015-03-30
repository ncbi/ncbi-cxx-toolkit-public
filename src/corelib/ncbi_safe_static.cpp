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
#include <corelib/error_codes.hpp>
#include <memory>
#include <assert.h>


#define NCBI_USE_ERRCODE_X   Corelib_Static


BEGIN_NCBI_SCOPE


CSafeStaticLifeSpan::CSafeStaticLifeSpan(ELifeSpan span, int adjust)
    : m_LifeSpan(int(span) + adjust)
{
    if (span == eLifeSpan_Min) {
        m_LifeSpan = int(span); // ignore adjustments
        adjust = 0;
    }
    if (adjust >= 5000  ||  adjust <= -5000) {
        ERR_POST_X(1, Warning
            << "CSafeStaticLifeSpan level adjustment out of range: "
            << adjust);
    }
    _ASSERT(adjust > -5000  &&  adjust < 5000);
}


CSafeStaticLifeSpan& CSafeStaticLifeSpan::GetDefault(void)
{
    static CSafeStaticLifeSpan s_DefaultSpan(eLifeSpan_Min);
    return s_DefaultSpan;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CSafeStaticPtr_Base::
//

// Protective mutex and the owner thread ID to avoid
// multiple initializations and deadlocks
DEFINE_CLASS_STATIC_MUTEX(CSafeStaticPtr_Base::sm_Mutex);


int CSafeStaticPtr_Base::x_GetCreationOrder(void)
{
    static CAtomicCounter s_CreationOrder;
    return int(s_CreationOrder.Add(1));
}


CSafeStaticPtr_Base::~CSafeStaticPtr_Base(void)
{
    if ( x_IsStdStatic() ) {
        CMutexGuard guard(sm_Mutex);
        x_Cleanup(guard);
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
    CMutexGuard guard(CSafeStaticPtr_Base::sm_Mutex);

    // Protect CSafeStaticGuard destruction
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

    for ( int pass = 0; pass < 3; ++pass ) {
        // Call Cleanup() for all variables registered
        TStack cur_Stack;
        swap(cur_Stack, *sm_Stack);
        NON_CONST_ITERATE(TStack, it, cur_Stack) {
            (*it)->x_Cleanup(guard);
            guard.Guard(CSafeStaticPtr_Base::sm_Mutex);
        }
    }

    delete sm_Stack;
    sm_Stack = 0;
}


// Global guard - to prevent premature destruction by e.g. GNU compiler
// (it destroys all local static variables before any global one)
static CSafeStaticGuard sg_CleanupGuard;


// Initialization of the guard
CSafeStaticGuard* CSafeStaticGuard::x_Get(void)
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
