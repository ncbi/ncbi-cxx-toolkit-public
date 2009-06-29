#ifndef CORELIB___IPC_LOCK__HPP
#define CORELIB___IPC_LOCK__HPP

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

/// @file ipc_lock.hpp 
/// Defines a simple inter-process lock class.
///
/// Defines classes: 
///     CInterProcessLock
///     CInterProcessLockException


#include <corelib/ncbistd.hpp>

#if !defined(NCBI_OS_MSWIN)  &&  !defined(NCBI_OS_UNIX)
#  error "CInterProcessLock is not implemented on this platform"
#endif


/** @addtogroup Process
 *
 * @{
 */

BEGIN_NCBI_SCOPE

/// System specific lock handle.
#if defined(NCBI_OS_UNIX)
  typedef int    TLockHandle;
#elif defined(NCBI_OS_MSWIN)
  typedef HANDLE TLockHandle;
#endif


/////////////////////////////////////////////////////////////////////////////
///
/// CInterProcessLock -- 
///
/// Simple inter-process lock. It is not really locking, because you cannot
/// block and wait for a lock. All you can do is get a lock or an exception.
/// CInterProcessLock throws CInterProcessLockException on errors.But this
/// is enough to make sure that there is only one process that using some
/// resource. 
///
/// Please note that due cross-platform limitations, multiple 
/// CInterProcessLock objects works not very well inside single process
/// in the MT environment. Only one object can be locked at one time. 
/// Please consider to use mutexes (corelib/ncbimtx.hpp) in this case.
///
/// Only single lock with specified name can exists on the system.
/// If the process terminates, normally or forcedly, all locks will be 
/// removed automaticaly.On Unix, the lock file can be left on the
/// filesystem, but the lock itself will be removed anyway and file
/// can be reused.
///
/// Please consider to use class CGuard<> for locking resources in
/// an exception-safe manner.

class NCBI_XNCBI_EXPORT CInterProcessLock
{
public:
    /// Constructor.
    ///
    /// Create locking object.
    /// For locking Lock() or TryLock() methods should be called.
    /// @name
    ///   Name of the lock. 
    ///   If name is empty string, GenerateUniqueName() will be used.
    /// @note
    ///   The size of the name is limited to the allowed file path size on
    ///   current platform and is case sensitive. The name should not
    ///   contains backslash character, some OS does not support it.
    ///   So, it is better to avoid it at all. For compatibility of
    //    cross-platform coding it is recommended to use Unix-style path
    ///   for the name of the lock.
    /// @note
    ///   UNIX: 
    ///     If directory name is specified, it should exists.
    ///     Please avoid using lock files on network filesystems (NFS),
    ///     it can works there, but is implementation-dependent.
    ///   MS-WINDOWS: 
    ///     The lock name can contains unexistent path. No any objects
    ///     will be created on the filesystem.
    /// @sa Lock, TryLock, GenerateUniqueName
    CInterProcessLock(const string& name = kEmptyStr);

    /// Destructor.
    ///
    /// Just calls Unlock();
    ~CInterProcessLock(void);

    /// @name Locking interface (to use with CGuard class)
    /// @{
    /// Try to create lock with specified name (in the constructor).
    /// If the lock already exists, throws CInterProcessLockException.
    /// @sa TryLock
    void Lock();
    /// Remove lock if it has acquired.
    void Unlock();
    /// @}

    /// Try to acquire the lock.
    ///
    /// @return
    ///   - TRUE, if the lock successfully acquired.
    ///   - FALSE, if the lock is already acquired by another thread/process
    ///     or an error occurs.
    /// @sa Lock
    bool TryLock(void);

    /// Get name of the current lock.
    const string& GetName() const { return m_Name; }

    /// Generate unigue lock name.
    ///
    /// @return
    ///   Unique name, that can be used as lock name.
    ///   Please note that obtained name can be used by other process
    ///   between call of this function and its real usage in the current
    ///   process.
    static const string GenerateUniqueName();

private:
    string       m_Name;     ///< Name of the lock.
    TLockHandle  m_Handle;   ///< System lock handle.
};


/////////////////////////////////////////////////////////////////////////////
///
/// CInterProcessLockException --
///
/// Define exceptions generated for file operations.
///
/// CFileException inherits its basic functionality from CCoreException
/// and defines additional error codes for file operations.

class NCBI_XNCBI_EXPORT CInterProcessLockException : public CCoreException
{
public:
    /// Error types that file operations can generate.
    enum EErrCode {
        eAlreadyExists,   ///< The lock already exists.
        eCreateError,     ///< Error creating the lock.
        eUniqueName       ///< Error obtaining unique lock name.
    };

    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CInterProcessLockException, CCoreException);
};


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___IPC_LOCK__HPP */
