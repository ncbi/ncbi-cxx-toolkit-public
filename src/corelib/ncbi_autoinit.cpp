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
 *   Auto-init variables - create on demand, destroy on termination
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbi_autoinit.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/error_codes.hpp>
#include <memory>
#include <assert.h>


#define NCBI_USE_ERRCODE_X Corelib_Static


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CAutoInitPtr_Base::
//

// Protective mutex and the owner thread ID to avoid
// multiple initializations and deadlocks
DEFINE_STATIC_MUTEX(s_AutoInitPtrMutex);

static CThreadSystemID s_AutoInitPtrMutexOwner;
// true if s_MutexOwner has been set (while the mutex is locked)
static bool            s_AutoInitPtrMutexLocked;

bool CAutoInitPtr_Base::Init_Lock(bool* mutex_locked)
{
    // Check if already locked by the same thread to avoid deadlock
    // in case of nested calls to Get() by T constructor
    // Lock only if unlocked or locked by another thread
    // to prevent initialization by another thread
    CThreadSystemID id = CThreadSystemID::GetCurrent();
    if (!s_AutoInitPtrMutexLocked  ||  s_AutoInitPtrMutexOwner != id) {
        s_AutoInitPtrMutex.Lock();
        s_AutoInitPtrMutexLocked = true;
        *mutex_locked = true;
        s_AutoInitPtrMutexOwner = id;
    }
    return m_Ptr == 0;
}


void CAutoInitPtr_Base::Init_Unlock(bool mutex_locked)
{
    // Unlock the mutex only if it was locked by the same call to Get()
    if ( mutex_locked ) {
        s_AutoInitPtrMutexLocked = false;
        s_AutoInitPtrMutex.Unlock();
    }
}


CAutoInitPtr_Base::~CAutoInitPtr_Base(void)
{
    bool mutex_locked = false;
    if ( !Init_Lock(&mutex_locked) ) {
        try
        {
            Cleanup();
        }
        catch (...)
        {
            Init_Unlock(mutex_locked);
            throw;
        }
    }
    Init_Unlock(mutex_locked);
}


END_NCBI_SCOPE
