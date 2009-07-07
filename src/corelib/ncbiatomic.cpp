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
 * Author:  Aaron Ucko
 *
 * File Description:
 *      NCBI_SwapPointers     -- atomic pointer swap operation
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/impl/ncbi_atomic_defs.h>

#if defined(NCBI_TCHECK)  &&  defined(NCBI_SWAP_POINTERS_EXTERN)
#  define NCBI_SWAP_POINTERS_IMPLEMENTATION
#endif

#include <corelib/ncbiatomic.h>

#ifdef NCBI_SLOW_ATOMIC_SWAP

#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE

// weak protection, as it doesn't guard against *other* simultaneous
// accesses. :-/
DEFINE_STATIC_FAST_MUTEX(sx_AtomicPointerMutex);

extern "C"
void* NCBI_SwapPointers(void * volatile * nv_loc, void* new_value)
{
    CFastMutexGuard LOCK(sx_AtomicPointerMutex);
    void* old_value = *nv_loc;
    *nv_loc = new_value;
    return old_value;
}

END_NCBI_SCOPE

#endif//NCBI_SLOW_ATOMIC_SWAP
