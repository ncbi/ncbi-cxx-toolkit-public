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
 * Authors:  Vladimir Ivanov
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/interprocess_lock.hpp>
#include "ncbisys.hpp"
#include <map>

#if defined(NCBI_OS_UNIX)
#  include <errno.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  include <fcntl.h>
#elif defined(NCBI_OS_MSWIN)
#  include <windows.h>
#endif


BEGIN_NCBI_SCOPE

/// System specific invalid lock handle.
#if   defined(NCBI_OS_UNIX)
    const int    kInvalidLockHandle = -1;
#elif defined(NCBI_OS_MSWIN)
    const HANDLE kInvalidLockHandle = INVALID_HANDLE_VALUE;
#endif


// List of all locks in the current process <name, ref_counter>.
typedef map<string, int> TLocks;
static CSafeStaticPtr<TLocks> s_Locks;

// Protective mutex for save access to s_Locks in MT environment.
DEFINE_STATIC_FAST_MUTEX(s_ProcessLock);



//////////////////////////////////////////////////////////////////////////////
//
// CInterProcessLock
//

CInterProcessLock::CInterProcessLock(const string& name)
    : m_Name(name)
{
    m_Handle = kInvalidLockHandle;

#if defined(NCBI_OS_UNIX)
    if ( CFile::IsAbsolutePath(m_Name) ) {
        m_SystemName = m_Name;
    } else {
        if (m_Name.find("/") == NPOS) {
            m_SystemName = "/var/tmp/" + m_Name;
        }
    }
#elif defined(NCBI_OS_MSWIN)

    // Backslash is not allowed in the mutex name
    m_SystemName = NStr::Replace(m_Name, "\\", "/"); 

#endif
    if ( m_SystemName.empty()  ||
         m_SystemName.length() > PATH_MAX) {
        NCBI_THROW(CInterProcessLockException, eNameError,
                   "Incorrect name for the lock");
    }
}


CInterProcessLock::~CInterProcessLock()
{
    if (m_Handle != kInvalidLockHandle) {
        try {
           Unlock();
        }
        catch (exception&) {}
    }
}


#if defined(NCBI_OS_UNIX)

/// Try to acquire a lock for specified file descriptor.
/// Return errno on error, or 0 on success.
static int s_UnixLock(int fd) 
{
    int x_errno = 0;
#  if defined(F_TLOCK)
    if ( lockf(fd, F_TLOCK, 0) < 0) {
        x_errno = errno;
    }
#  elif defined(F_SETLK)
    struct flock lockparam;
    lockparam.l_type   = F_WRLCK;
    lockparam.l_whence = SEEK_SET;
    lockparam.l_start  = 0;
    lockparam.l_len    = 0;  /* whole file */
    while (fcntl(fd, F_SETLK, &lockparam) < 0) {
        x_errno = errno;
        if (x_errno != EINTR) {
            break;
        }
    }
#  else
#      error "No supported lock method.  Please port this code."
#  endif
    return x_errno;
}

#endif


void CInterProcessLock::Lock(const CTimeout& timeout,
                             const CTimeout& granularity)
{
    CFastMutexGuard LOCK(s_ProcessLock);

    // Check that lock with specified name not already locked
    // in the current process.
    TLocks::iterator it = s_Locks->find(m_SystemName);

    if (m_Handle != kInvalidLockHandle) {
        // The lock is already set in this CInterProcessLock object,
        // just increase reference counter.
        _VERIFY(it != s_Locks->end());
        it->second++;
        return;
    } else {
        if (it != s_Locks->end()) {
            // The lock already exists in the current process.
            // We can use one CInterProcessLock object with
            // multiple Lock() calls, but not with different
            // CInterProcessLock objects. For example, on MS-Windows,
            // we cannot wait on the same mutex in the same thread.
            // So, two different objects can set locks simultaneously.
            // And for OS-compatibility we can do nothing here,
            // except throwing an exception.
            NCBI_THROW(CInterProcessLockException, eMultipleLocks,
                       "Attempt to lock already locked object " \
                       "in the same process");
        }
    }

    // Try to acquire a lock with specified timeout

#if defined(NCBI_OS_UNIX)

    // Open lock file
    mode_t perm = CDirEntry::MakeModeT(
        CDirEntry::fRead | CDirEntry::fWrite /* user */,
        CDirEntry::fRead | CDirEntry::fWrite /* group */,
        0, 0 /* other & special */);
    int fd = open(m_SystemName.c_str(), O_CREAT | O_RDWR, perm);
    if (fd == -1) {
        NCBI_THROW(CInterProcessLockException, eCreateError,
                   string("Error creating lockfile ") + m_SystemName + 
                   ": " + strerror(errno));
    }

    // Try to acquire the lock
    
    int x_errno = 0;
    
    if (timeout.IsInfinite()  ||  timeout.IsDefault()) {
        while ((x_errno = s_UnixLock(fd))) {
            if (errno != EAGAIN)
                break;
        }

    } else {
        unsigned long ms = timeout.GetAsMilliSeconds();
        if ( !ms ) {
            // Timeout == 0
            x_errno = s_UnixLock(fd);
        } else {
            // Timeout > 0
            unsigned long ms_gran;
            if ( granularity.IsInfinite()  ||
                 granularity.IsDefault() ) 
            {
                ms_gran = min(ms/5, (unsigned long)500);
            } else {
                ms_gran = granularity.GetAsMilliSeconds();
            }
            // Try to lock within specified timeout
            for (;;) {
                x_errno = s_UnixLock(fd);
                if ( !x_errno ) {
                    // Successfully locked
                    break;
                }
                if (x_errno != EACCES  &&
                    x_errno != EAGAIN ) {
                    // Error
                    break;
                }
                // Otherwise -- sleep granularity timeout
                unsigned long ms_sleep = ms_gran;
                if (ms_sleep > ms) {
                    ms_sleep = ms;
                }
                if ( !ms_sleep ) {
                     break;
                }
                SleepMilliSec(ms_sleep);
                ms -= ms_sleep;
            }
            // Timeout
            if ( !ms ) {
                close(fd);
                NCBI_THROW(CInterProcessLockException, eLockTimeout,
                           "The lock could not be acquired in the time " \
                           "allotted");
            }
        } // if (!ms)
    } // if (timeout.IsInfinite())
    
    // Error
    if ( x_errno ) {
        close(fd);
        NCBI_THROW(CInterProcessLockException, eLockError,
                   "Error creating lock");
    }
    // Success
    m_Handle = fd;

#elif defined(NCBI_OS_MSWIN)

    HANDLE  handle  = ::CreateMutex(NULL, TRUE, _T_XCSTRING(m_SystemName));
    errno_t errcode = ::GetLastError();
    if (handle == kInvalidLockHandle) {
        switch(errcode) {
            case ERROR_ACCESS_DENIED:
                // Mutex with specified name already exists, 
                // but we don't have enough rights to open it.
                NCBI_THROW(CInterProcessLockException, eLockError,
                           "The lock already exists");
                break;
            case ERROR_INVALID_HANDLE:
                // Some system object with the same name already exists
                NCBI_THROW(CInterProcessLockException, eLockError,
                           "Error creating lock, system object with the same" \
                           "name already exists");
                break;
            default:
                // Unknown error
                NCBI_THROW(CInterProcessLockException, eCreateError,
                           "Error creating lock");
                break;
        }
    } else {
        // Mutex with specified name already exists
        if (errcode == ERROR_ALREADY_EXISTS) {
            // Wait
            DWORD res;
            if (timeout.IsInfinite()  ||  timeout.IsDefault()) {
                res = WaitForSingleObject(handle, INFINITE);
            } else {
                res = WaitForSingleObject(handle, timeout.GetAsMilliSeconds());
            }
            switch(res) {
                case WAIT_OBJECT_0:
                    // The lock has been acquired
                    break;
                case WAIT_TIMEOUT:
                    ::CloseHandle(handle);
                    NCBI_THROW(CInterProcessLockException, eLockTimeout,
                               "The lock could not be acquired in the time " \
                               "allotted");
                    break;
                case WAIT_ABANDONED:
                    // The lock is in abandoned state... Other thread/process
                    // owning it was terminated. We can reuse this mutex, but 
                    // it is better to wait until it will be released by OS.
                    /* FALLTHRU */
                default:
                    ::CloseHandle(handle);
                    NCBI_THROW(CInterProcessLockException, eLockError,
                               "Error creating lock");
                    break;
            }
        }
        m_Handle = handle;
    }
#endif
    // Set reference counter to 1
    (*s_Locks)[m_SystemName] = 1;
}


void CInterProcessLock::Unlock()
{
    if (m_Handle == kInvalidLockHandle) {
        NCBI_THROW(CInterProcessLockException, eNotLocked,
                   "Attempt to unlock not-yet-acquired lock");
    }
    CFastMutexGuard LOCK(s_ProcessLock);

    // Check that lock with specified name not already locked
    // in the current process.
    TLocks::iterator it = s_Locks->find(m_SystemName);
    _VERIFY(it != s_Locks->end());

    if ( it->second > 1 ) {
        // Just decrease reference counter
        it->second--;
        return;
    }

    // Release lock

#if defined(NCBI_OS_UNIX)

#  if defined(F_TLOCK)
    int res = lockf(m_Handle, F_ULOCK, 0);
#  elif defined(F_SETLK)
    struct flock lockparam;
    lockparam.l_type   = F_UNLCK;
    lockparam.l_whence = SEEK_SET;
    lockparam.l_start  = 0;
    lockparam.l_len    = 0;  /* whole file */
    int res = fcntl(m_Handle, F_SETLK, &lockparam);
#  else
#   error "No supported lock method.  Please port this code."
#  endif
    if ( res < 0 ) {
        NCBI_THROW(CInterProcessLockException, eUnlockError,
                   "Cannot release the lock");
    }
    close(m_Handle);
    
#elif defined(NCBI_OS_MSWIN)
    if ( !::ReleaseMutex(m_Handle) ) {
        NCBI_THROW(CInterProcessLockException, eUnlockError,
                   "Cannot release the lock");
    }
    ::CloseHandle(m_Handle);
#endif
    m_Handle = kInvalidLockHandle;
    s_Locks->erase(m_SystemName);
}


void CInterProcessLock::Remove()
{
    if (m_Handle != kInvalidLockHandle) {
        Unlock();
    }
    NcbiSys_unlink(_T_XCSTRING(m_SystemName));
}


bool CInterProcessLock::TryLock()
{
    try {
        Lock(CTimeout(0,0));
    }
    catch (CInterProcessLockException&) {
        return false;
    }
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CInterProcessLockException
//

const char* CInterProcessLockException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
        case eLockTimeout:   return "eLockTimeout";
        case eCreateError:   return "eCreateError";
        case eLockError:     return "eLockError";
        case eUnlockError:   return "eUnlockError";
        case eMultipleLocks: return "eMultipleLocks";
        case eNotLocked:     return "eNotLocked";
        default:             return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
