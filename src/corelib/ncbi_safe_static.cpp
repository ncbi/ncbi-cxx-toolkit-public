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
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2001/03/26 20:38:35  vakatov
 * Initial revision (by A.Grichenko)
 *
 * ===========================================================================
 */

#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbistd.hpp>
#include <assert.h>
#include <memory>

BEGIN_NCBI_SCOPE


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


CSafeStaticGuard::~CSafeStaticGuard(void)
{
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
}


// Global guard - to prevent premature destruction by i.g. GNU compiler
// (it destroys all local static variables before any global one)
static CSafeStaticGuard sg_CleanupGuard;


// Initialization of the guard
CSafeStaticGuard* CSafeStaticGuard::Get(void)
{
    // Local static variable - to initialize the guard
    // as soon as the function is called (global static
    // variable may be still uninitialized at this moment)
    static CSafeStaticGuard sl_CleanupGuard;
    return &sl_CleanupGuard;
}


END_NCBI_SCOPE
