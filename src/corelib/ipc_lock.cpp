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

#if defined(NCBI_OS_UNIX)
#  include <errno.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
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
    // processes. To allow it works inside one process we need protective
    // mutex.
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

    bool plock = s_ProcessLock.TryLock();
    if (!plock) {
        NCBI_THROW(CInterProcessLockException, eCreateError,
                   "Current process already use CInterProcessLock, multiple locks are not supported");
    }
    
    // Open lock file
    mode_t perm = CDirEntry::MakeModeT(
        CDirEntry::fRead | CDirEntry::fWrite /* user */,
        CDirEntry::fRead | CDirEntry::fWrite /* group */,
        0, 0 /* other & special */);
    int fd = open(m_Name.c_str(), O_CREAT | O_RDWR, perm);
    if (fd == -1) {
        s_ProcessLock.Unlock();
        NCBI_THROW(CInterProcessLockException, eCreateError,
                   string("Error creating lockfile ") + m_Name + ": " + strerror(errno));
    }
    // Try to acquire the lock
    if (lockf(fd, F_TLOCK, 0) != 0) {
        int saved = errno;
        close(fd);
        s_ProcessLock.Unlock();
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
    // Unlocking on Unix is not an atomic operation :(
    // so delete lock file first, and only then remove lock itself.
    unlink(m_Name.c_str());
    lockf(m_Handle, F_ULOCK, 0);
    close(m_Handle);
    s_ProcessLock.Unlock();
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
