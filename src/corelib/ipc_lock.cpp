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
#include <corelib/ipc_lock.hpp>
#include <map>

#if defined(NCBI_OS_UNIX)
#  include <errno.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  include <fcntl.h>
#endif


BEGIN_NCBI_SCOPE

/// System specific invalid lock handle.
#if defined(NCBI_OS_MSWIN)
    const TLockHandle kInvalidLockHandle = INVALID_HANDLE_VALUE;
#else
    const TLockHandle kInvalidLockHandle = -1;
#endif


#if defined(NCBI_OS_UNIX)
    // UNIX implementation of CInterProcessLock works only with different
    // processes. To allow it works inside one process we need some kind of
    // protection. We use global list of all locks in the current process.
    // This solution is not ideal in the case of relative path names :(

    // List of all locks in the current process.
    typedef map<string, int> TLocks;
    static CSafeStaticPtr<TLocks> s_Locks;

    // Protective mutex for save access to s_Locks in MT environment.
    DEFINE_STATIC_FAST_MUTEX(s_ProcessLock);
#endif



//////////////////////////////////////////////////////////////////////////////
//
// CInterProcessLock
//

CInterProcessLock::CInterProcessLock(const string& name)
    : m_Name(name)
{
    m_Handle = kInvalidLockHandle;
    if ( m_Name.empty() ) {
        m_Name = GenerateUniqueName();
    }
}


CInterProcessLock::~CInterProcessLock()
{
    Unlock();
}


void CInterProcessLock::Lock()
{
    if (m_Handle != kInvalidLockHandle) {
        NCBI_THROW(CInterProcessLockException, eAlreadyExists,
                   "The lock already exists, recurring Lock() is not allowed");
    }

#if defined(NCBI_OS_UNIX)
    CFastMutexGuard LOCK(s_ProcessLock);

    // Check that lock with current name not already locked in the current process.
    TLocks::iterator it = s_Locks->find(m_Name);
    if ( it != s_Locks->end() ) {
        NCBI_THROW(CInterProcessLockException, eAlreadyExists,
                   "The lock already exists");
    }
        
    // Open lock file
    mode_t perm = CDirEntry::MakeModeT(
        CDirEntry::fRead | CDirEntry::fWrite /* user */,
        CDirEntry::fRead | CDirEntry::fWrite /* group */,
        0, 0 /* other & special */);
    int fd = open(m_Name.c_str(), O_CREAT | O_RDWR, perm);
    if (fd == -1) {
        NCBI_THROW(CInterProcessLockException, eCreateError,
                   string("Error creating lockfile ") + m_Name + ": " + strerror(errno));
    }

    // Try to acquire the lock

#  if defined(F_TLOCK)
    int res = lockf(fd, F_TLOCK, 0);
#  elif defined(F_SETLK)
    struct flock lockparam;
    lockparam.l_type   = F_WRLCK;
    lockparam.l_whence = SEEK_SET;
    lockparam.l_start  = 0;
    lockparam.l_len    = 0;  /* whole file */
    int res = fcntl(fd, F_SETLK, &lockparam);
#else
#   error "No supported lock method.  Please port this code."
#endif

    if (res != 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        if (errno == EAGAIN) {
            NCBI_THROW(CInterProcessLockException, eAlreadyExists,
                       "The lock already exists");
        } else {
            NCBI_THROW(CInterProcessLockException, eCreateError,
                       string("Unable to lock file ") + m_Name + ": " + strerror(errno));
        }
    }
    m_Handle = fd;
    (*s_Locks)[m_Name] = 1;


#elif defined(NCBI_OS_MSWIN)
    HANDLE handle = ::CreateMutex(NULL, TRUE, m_Name.c_str());
    errno_t errcode = ::GetLastError();
    if (handle == kInvalidLockHandle) {
        switch(errcode) {
            case ERROR_ACCESS_DENIED:
                // Mutex with specified name already exists, but we don't have enougth rigths to open it.
                NCBI_THROW(CInterProcessLockException, eAlreadyExists,
                           "The lock already exists");
                break;
            case ERROR_INVALID_HANDLE:
                // Some system object with the same name already exists
                NCBI_THROW(CInterProcessLockException, eCreateError,
                           "Error creating lock, some system object with the same name already exists");
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
            ::CloseHandle(handle);
            NCBI_THROW(CInterProcessLockException, eAlreadyExists,
                       "The lock already exists");
        }
        m_Handle = handle;
    }
#endif
}


void CInterProcessLock::Unlock()
{
    if (m_Handle == kInvalidLockHandle) {
        return;
    }

#if defined(NCBI_OS_UNIX)
    CFastMutexGuard LOCK(s_ProcessLock);
    
    // Unlocking on Unix is not an atomic operation :(
    // so delete lock file first, and only then remove lock itself.
    unlink(m_Name.c_str());
    
#  if defined(F_TLOCK)
    lockf(m_Handle, F_ULOCK, 0);
#  elif defined(F_SETLK)
    struct flock lockparam;
    lockparam.l_type   = F_UNLCK;
    lockparam.l_whence = SEEK_SET;
    lockparam.l_start  = 0;
    lockparam.l_len    = 0;  /* whole file */
    fcntl(m_Handle, F_SETLK, &lockparam);
#else
#   error "No supported lock method.  Please port this code."
#endif
    close(m_Handle);
    s_Locks->erase(m_Name);
    
#elif defined(NCBI_OS_MSWIN)
    ::CloseHandle(m_Handle);
#endif
    m_Handle = kInvalidLockHandle;
}


bool CInterProcessLock::TryLock()
{
    try {
        Lock();
    }
    catch (CInterProcessLockException&) {
        return false;
    }
    return true;
}

const string CInterProcessLock::GenerateUniqueName()
{
    string name;

#if defined(NCBI_OS_UNIX)
    name = CFile::GetTmpName();
    if (!name.empty()) {
        return name;
    }
#elif defined(NCBI_OS_MSWIN)
    char buffer[MAX_PATH];
    unsigned long ofs = rand();
    // Generate unique mutex name
    while ( ofs < numeric_limits<unsigned long>::max() ) {
        _ultoa((unsigned long)ofs, buffer, 24);
        name = string("ipc_lock_") + buffer;
        HANDLE handle = ::CreateMutex(NULL, TRUE, name.c_str());
        errno_t errcode = ::GetLastError();
        ::CloseHandle(handle);
        if ( (handle == NULL) || ((handle != NULL) && (errcode == ERROR_ALREADY_EXISTS)) ) {
            ofs++;
        } else {
            break;
        }
    }
    if (ofs != numeric_limits<unsigned long>::max() ) {
        return name;
    }
#endif
    NCBI_THROW(CInterProcessLockException, eUniqueName,
               "Unable to obtain unique lock name");
    return name;
}


//////////////////////////////////////////////////////////////////////////////
//
// CInterProcessLockException
//

const char* CInterProcessLockException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
        case eAlreadyExists: return "eAlreadyExists";
        case eCreateError:   return "eCreateError";
        case eUniqueName:    return "eUniqueName";
        default:             return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
