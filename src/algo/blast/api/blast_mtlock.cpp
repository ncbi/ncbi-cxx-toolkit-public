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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file blast_mtlock.cpp
 * Initialization for the mutex locking interface. 
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbimtx.hpp>
#include <algo/blast/api/blast_mtlock.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

extern "C" {

/** Locking callback for the Blast MT_LOCK implementation. */
static int BlastLockHandler(void* user_data, EMT_Lock how)
{
    CFastMutex* lock = (CFastMutex*) user_data;
    
    switch ( how ) {
    case eMT_Lock:
        lock->Lock();
        break;
    case eMT_Unlock:
        lock->Unlock();
        break;
    default:
        break;
    }
    
    return 1;
}

/** Cleanup callback for the Blast MT_LOCK implementation. */
static void BlastLockCleanup(void* user_data)
{
    CFastMutex* lock = (CFastMutex*) user_data;
    delete lock;
}

}

/// Initializes the C++ style locking mechanism for BLAST.
MT_LOCK Blast_CMT_LOCKInit()
{
    CFastMutex* mutex = new CFastMutex();
    MT_LOCK lock = 
        MT_LOCK_Create((void*)mutex, BlastLockHandler, BlastLockCleanup);
    return lock;
}

END_SCOPE(blast)
END_NCBI_SCOPE
