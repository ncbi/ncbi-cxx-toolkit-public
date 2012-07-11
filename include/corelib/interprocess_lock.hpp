#ifndef CORELIB___INTERPROCESS_LOCK__HPP
#define CORELIB___INTERPROCESS_LOCK__HPP

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
 * Authors:  Vladimir Ivanov, Denis Vakatov
 *
 *
 */

/// @file interprocess_lock.hpp 
/// Simple inter-process lock.
///
/// Defines classes: 
///     CInterProcessLock
///     PInterProcessLock
///     CInterProcessLockException


#include <corelib/ncbitime.hpp>
#include <corelib/guard.hpp>

#if !defined(NCBI_OS_MSWIN)  &&  !defined(NCBI_OS_UNIX)
#  error "CInterProcessLock is not implemented on this platform"
#endif


/** @addtogroup Process
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CInterProcessLock -- 
///
/// Simple inter-process lock.
///
/// When the process terminates (normally or otherwise) all locks will be 
/// removed automaticaly. On Unix, the lock file can be left in the
/// filesystem, but the lock itself will be removed anyway and the lock file
/// can be reused.
///
/// Please consider to use class CGuard<> for locking resources in
/// an exception-safe manner. See also CInterProcessLock_Guard to help specify
/// timeouts for the locking operation when using CGuard<>.
///
/// The lock can be safely acquired from different threads (and, with all
/// timeouts observed).

class NCBI_XNCBI_EXPORT CInterProcessLock
{
public:
    /// @param name
    ///  Name of the lock.
    ///  The name must be a Unix-path like, and its length must not
    ///  exceed the maximum path length that is specific for the OS where the
    ///  application runs.
    /// @note
    ///  This constructor will not try to acquire a lock on creation.
    /// @note
    ///  - On Unix, if a pathless file name is given, then the corresponding
    ///    file will be created in a well-known system-specific location
    ///    (usually as '/var/tmp/<name>');  if the name represents an absolute
    ///    file path, then the exact path will be used to create the lock file.
    ///  - On Windows, no actual files will be created at the specified path.
    /// @attention
    ///  - Relative paths are not allowed (regardless of the OS).
    ///  - Empty name is not allowed.
    ///  - Locking may not work for the files located on network filesystems
    ///    (like NFS).
    /// @sa Lock, TryLock, GetName
    CInterProcessLock(const string& name);

    /// Call Unlock()
    ~CInterProcessLock(void);

    /// @name
    ///  Lock/guard interface (to use with CGuard<> or CInterProcessLock_Guard)
    /// @{
    ///  Try to acquire the lock.
    ///  On any error, or if the lock is already held by another process and
    ///  it could not be acquired within the allotted time period,
    ///  throw CInterProcessLockException.
    /// @param timeout
    ///  How much time Lock() should wait before giving up (with error).
    ///  Default timeout value is interpreted as infinite.
    /// @param granularity
    ///  On the systems (such as Unix) where there is no support for waiting
    ///  for the lock with a timeout, the waiting is implemented using a
    ///  loop of "try to lock" calls. This parameter specifies how much time to
    ///  wait between these calls. It will be auto-limited to the closest
    ///  whole delimiter of the 'timeout' value (and it will never exceed it).
    ///  Default or infinite granularity timeout is converted into:
    ///    MIN(timeout/5, 0.5sec).
    /// @note
    ///  If 'timeout' is passed infinite, then the 'granularity' parameter
    ///  actually will not be used at all -- both on Windows and on Unix -- as
    ///  they both support a "honest" infinite waiting.
    /// @note
    ///  One process can acquire the lock more than once -- but it need
    ///  to use the same CInterProcessLock object for Lock(), and then it will
    ///  need to call Unlock() as many times in order to release the lock for
    ///  other processes.
    /// @note
    ///  If the lock object (such as file on UNIX or system mutex on Windows)
    ///  doesn't exist it will be created.
    /// @sa TryLock, SetLockTimeout
    void Lock(const CTimeout& timeout     = CTimeout(CTimeout::eInfinite),
              const CTimeout& granularity = CTimeout(CTimeout::eInfinite));

    /// Release the lock.
    /// On any error (including when the lock is not held by the process),
    /// throw CInterProcessLockException.
    /// @sa Lock, TryLock, Remove
    void Unlock(void);
    /// @}

    /// Try to acquire the lock.
    ///
    /// Acquire the lock if it can be done right away. Return immediately.
    /// @return
    ///   - TRUE,  if the lock has been successfully acquired.
    ///   - FALSE, if the lock is already held by another process
    ///            or if any error occurs.
    /// @sa Lock
    bool TryLock(void);

    /// Call Unlock() and removes lock object from the system.
    ///
    /// On Unix, the Unlock() method do not remove used lock file from
    /// the system, it just release a lock. If you don't need this file
    /// anymore, use this method to remove it. We cannot remove it 
    /// automaticaly in the Unlock(), because on Unix locking/unlocking 
    /// is not an atomic operations and race condition is possible if
    /// at time of deleting some other process wait to acquire a lock
    /// using the same lock file.
    /// @note
    ///  On Windows, it works almost as Unlock().
    /// @sa Unlock
    void Remove(void);

    /// Get the original name of the lock -- exactly as it was specified
    /// in the constructor
    const string& GetName(void) const { return m_Name; }

    /// Get the internal name of the lock -- as it is being used in the calls
    /// to the OS (such as '/var/tmp/<name>' rather than just '<name>' on UNIX)
    const string& GetSystemName(void) const { return m_SystemName; }

private:
    /// Original name of the lock
    string m_Name;
     /// Adjusted name of the lock
    string m_SystemName;

    /// OS-specific lock handle
#if   defined(NCBI_OS_UNIX)
    typedef int    TLockHandle;
#elif defined(NCBI_OS_MSWIN)
    typedef HANDLE TLockHandle;
#endif
    TLockHandle  m_Handle;
};



/////////////////////////////////////////////////////////////////////////////
///
/// PInterProcessLock -- 
///
/// Helper functor for CGuard<> to e.g. help specify the waiting timeout.
///
/// @example
///  CInterProcessLock ipl("MyLock");
///  CInterProcessLock_Guard ipl_guard
///      (ipl,
///       PInterProcessLock(3.456, 0.333));

class PInterProcessLock
{
public:
    typedef CInterProcessLock resource_type;

    PInterProcessLock(const CTimeout& timeout,
                      const CTimeout& granularity = CTimeout(CTimeout::eInfinite))
        : m_Timeout(timeout),
          m_Granularity(granularity)
    {
    }

    void operator()(resource_type& resource) const
    {
        resource.Lock(m_Timeout, m_Granularity);
    }

private:
    CTimeout m_Timeout;
    CTimeout m_Granularity;
};


/// Convenience typedef for PInterProcessLock
/// @sa PInterProcessLock
typedef CGuard<CInterProcessLock, PInterProcessLock> CInterProcessLock_Guard;



/////////////////////////////////////////////////////////////////////////////
///
/// CInterProcessLockException --
///

class NCBI_XNCBI_EXPORT CInterProcessLockException : public CCoreException
{
public:
    /// Error types that file operations can generate.
    enum EErrCode {
        eLockTimeout,   ///< The lock could not be acquired in the time allotted
        eNameError,     ///< Incorrect name for a lock
        eCreateError,   ///< Cannot create the lock object in the OS
        eLockError,     ///< Cannot acquire a lock (not eLockTimeout, eCreateError)
        eUnlockError,   ///< Cannot release the lock
        eMultipleLocks, ///< Attempt to lock already locked object in the same process
        eNotLocked      ///< Attempt to unlock a not-yet-acquired lock
    };

    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CInterProcessLockException, CCoreException);
};


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___INTERPROCESS_LOCK__HPP */
