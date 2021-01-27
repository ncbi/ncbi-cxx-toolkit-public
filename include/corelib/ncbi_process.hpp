#ifndef CORELIB___NCBI_PROCESS__HPP
#define CORELIB___NCBI_PROCESS__HPP

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
 * Authors:  Aaron Ucko, Vladimir Ivanov
 *
 *
 */

/// @file ncbi_process.hpp
/// Defines process management classes.
///
/// Defines classes:
///     CProcess
///     CPIDGuard
///
/// Implemented for: UNIX, MS-Windows


#include <corelib/interprocess_lock.hpp>

#if defined(NCBI_OS_UNIX)
#  include <sys/types.h>
#elif defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#else
#  error "CProcess is not implemented on this platform"
#endif


BEGIN_NCBI_SCOPE

/** @addtogroup Process
 *
 * @{
 */

/// Infinite timeout in milliseconds.
/// @deprecated Need to be removed to use constants from CProcessBase and derived classes
const unsigned long kInfiniteTimeoutMs = kMax_ULong;

// This workaround is obsolete now. LinuxThreads library has been replaced by NPTL
// which does not need it.
/*
/// Turn on/off workaround for Linux PID and PPID
#if defined(NCBI_OS_LINUX)
#  define NCBI_THREAD_PID_WORKAROUND
#endif
*/

/// Process identifier (PID) and process handle.
#if defined(NCBI_OS_UNIX)
  typedef pid_t  TPid;
  typedef TPid   TProcessHandle;
#elif defined(NCBI_OS_MSWIN)
  typedef DWORD  TPid;
  typedef HANDLE TProcessHandle;
#else
  typedef int    TPid;
  typedef TPid   TProcessHandle;
#endif



/////////////////////////////////////////////////////////////////////////////
///
/// CProcessBase --
///
/// Base class that defines common types and constants for CProcess and CCurrentProcess.
/// @sa CProcess, CCurrentProcess

  class NCBI_XNCBI_EXPORT CProcessBase
{
public:
    /// Process information "target"
    enum EWhat {
        eProcess,   ///< Current process
        eChildren,  ///< All children of the calling process
        eThread     ///< Current thread
    };

    /// Process memory usage information, in bytes.
    ///
    /// Values sets to 0 if unknown,
    /// @note
    ///    Resident set size represent physical memory usage, 
    ///    all other members - virtual memory usage.
    /// @note
    ///    Resident size alone is the wrong place to look to determine if your processes
    ///    are getting too big. If the system gets overloaded with processes that
    ///    using more and more (virtual) memory over time, that resident portion can even shrink.
    struct SMemoryUsage {
        size_t total;         ///< Total memory usage
        size_t total_peak;    ///< Peak total memory usage
        size_t resident;      ///< Resident/working set size (RSS).
                              ///< RSS is the portion of memory occupied by a process
                              ///< that is held in main memory (RAM).
        size_t resident_peak; ///< Peak resident set size ("high water mark")
        size_t shared;        ///< Shared memory usage
        size_t data;          ///< Data segment size
        size_t stack;         ///< Stack size of the initial thread in the process
        size_t text;          ///< Text (code) segment size
        size_t lib;           ///< Shared library code size
        size_t swap;          ///< Swap space usage
    };

    /// Default wait time (milliseconds) between "soft" and "hard"
    /// attempts to terminate a process.
    static const unsigned long kDefaultKillTimeout;

    /// Infinite timeout (milliseconds).
    static const unsigned long kInfiniteTimeoutMs;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CCurrentProcess --
///
/// This class defines methods related to the current process.
/// For arbitrary process please see CProcess.
///
/// Static methods only.
///
/// @sa CProcess

class NCBI_XNCBI_EXPORT CCurrentProcess : public CProcessBase
{
public:
    /////////////////////////////////////////////////////////////////////////////
    // Process information

    /// Get process handle for the current process (esp. on Windows).
    /// @note
    ///   On Windows process identifiers and process handles are different.
    ///   On UNIX handle/pid are the same and GetHandle()/GetPid()
    ///   return the same value.
    /// @sa
    ///   GetPid
    static TProcessHandle GetHandle(void);

    /// Get process identifier (pid) for the current process.
    /// @sa
    ///   GetHandle
    static TPid GetPid(void);

    /// Get process identifier (pid) for the parent of the current process.
    /// @sa
    ///   GetPid
    static TPid GetParentPid(void);

    /// Get current process execution times.
    ///
    /// @param real
    ///   Pointer to a variable that receives the amount of time in second that
    ///   the current process/thread has executed, when available.
    /// @note
    ///   Here is no portable solution to get 'real' process execution time,
    ///   not all OS have an API to get such information from the system,
    ///   or it can include children times as well...
    ///   but it is available on some platforms like Windows, Linux and BSD.
    /// @param user
    ///   Pointer to a variable that receives the amount of time in seconds that
    ///   the process, process tree or thread has executed in user mode.
    ///   Note, that this value can exceed the amount of "real" time if
    ///   the process executes across multiple CPU cores (OS dependent).
    /// @param sys
    ///   Pointer to a variable that receives the amount of time in second that
    ///   the current process, process tree or thread has executed in kernel mode.
    /// @param what
    ///   Applies to user and system time parameters only, 
    ///   real time calculates for the current process only.
    ///   Defines what times to measure: current thread, process or children processes:
    ///   - eProcess:
    ///         Current process, which is the sum of resources used
    ///         by all threads in the process.
    ///   - eChildren: (limited support)
    ///         All children of the calling process that have terminated already
    ///         and been waited for. These statistics will include the resources
    ///         used by grandchildren, and further removed descendants, if all of
    ///         the intervening descendants waited on their terminated children.
    ///   - eThread: (limited support)
    ///         Usage statistics for the calling thread.
    /// @return
    ///   TRUE on success; or FALSE on error.
    ///   If some value cannot be calculated on a current platform for "what" target,
    ///   it will be set to a negative value.
    /// @note
    ///   NULL arguments will not be filled in.
    /// @sa
    ///   CProcess::GetTimes
    static bool GetTimes(double* real, double* user, double* sys, EWhat what = eProcess);

    /// Get current process memory usage.
    /// 
    /// @sa
    ///   CProcess::GetMemoryUsage, SMemoryUsage
    static bool GetMemoryUsage(SMemoryUsage& usage);

    /// Get the number of threads in the current process.
    ///
    /// @return
    ///   Number of threads in the current process, or -1 on error.
    /// @sa
    ///   CProcess::GetThreadCount
    static int GetThreadCount(void);

    /// Get the number of file descriptors consumed by the current process,
    /// and optional system wide file descriptor limits.
    ///
    /// @param soft_limit
    ///   Pointer to a variable that receives system wide soft limit.
    ///   -1 means it was impossible to get the limit. 
    /// @param hard_limit
    ///   Pointer to a variable that receives system wide hard limit.
    ///   -1 means it was impossible to get the limit. 
    /// @return
    ///   Number of file descriptors consumed by the process, or -1 on error.
    /// @note
    ///   NULL arguments will not be filled in.
    /// @sa 
    ///   CProcess::GetFileDescriptorsCount
    /// @note Unix inly
    static int GetFileDescriptorsCount(int* soft_limit = NULL, int* hard_limit = NULL);


    /////////////////////////////////////////////////////////////////////////////
    // Forks & Daemons

    /// Forking flags.
    enum FForkFlags {
        fFF_UpdateDiag      = 1,  ///< Reset diagnostics timer and log an
                                  ///< app-start message in the child process.
        fFF_AllowExceptions = 32, ///< Throw an exception if fork(2) failed.
    };
    /// Bit-wise OR of FForkFlags @sa FForkFlags
    typedef unsigned TForkFlags;

    /// Fork the process. Update PID and GUID used for logging.
    ///
    /// @return
    ///   In the parent process, the call returns the child process ID.
    ///   In the child process, the call returns zero.
    ///   In case of an error, unless the fFF_AllowExceptions flag is
    ///   given, the call returns -1.
    ///
    /// @throw
    ///   If the fFF_AllowExceptions flag is specified, throws a CCoreException
    ///   in case of a fork(2) failure. If the platform does not support process
    ///   forking, an exception is always thrown regardless of whether
    ///   the fFF_AllowExceptions flag is specified.
    ///
    static TPid Fork(TForkFlags flags = fFF_UpdateDiag);

    /// Daemonization flags
    enum FDaemonFlags {
        fDF_KeepCWD         = 1,    ///< Don't change CWD to "/"
        fDF_KeepStdin       = 2,    ///< Keep stdin open as "/dev/null" (RO)
        fDF_KeepStdout      = 4,    ///< Keep stdout open as "/dev/null" (WO)
        fDF_ImmuneTTY       = 8,    ///< Make daemon immune to re-acquiring a controlling terminal
        fDF_KeepParent      = 16,   ///< Do not exit the parent process but return
        fDF_AllowExceptions = 32,   ///< Throw an exception in case of an error.
        fDF_AllowThreads    = 64    ///< Do not fail if pre-existing threads are detected
    };
    /// Bit-wise OR of FDaemonFlags @sa FDaemonFlags
    typedef unsigned int TDaemonFlags;

    /// Go daemon.
    ///
    /// Return TPid(-1) in the daemon thread (the parent thread doesn't return
    /// unless fKeepParent is set), or TPid(daemon) in the parent thread.
    /// On error return 0 in the parent thread (no daemon created), see errno.
    ///
    /// Reopen stderr/cerr in the daemon thread if "logfile" specified as
    /// non-NULL (stderr will open to "/dev/null" if "logfile" has been passed
    /// as ""), otherwise stderr is closed in the daemon thread.
    ///
    /// Unless instructed by the "flags" parameter, the daemon thread has its
    /// stdin/cin and stdout/cout closed, and the current working directory
    /// changed to the root directory ("/").
    ///
    /// If kept open, stdin and stdout are both redirected to "/dev/null".
    /// Opening a terminal device as a controlling terminal is allowed, unless
    /// fImmuneTTY is specified in the flags, which then causes a second
    /// fork() so that the resultant process won't be allowed to open a TTY as
    /// its controlling TTY (but only with an explicit O_NOCTTY, see open(2)),
    /// thus protecting the process from any blocking via TTY signaling.
    ///
    /// Note that this call is somewhat destructive and may not be able
    /// to restore the process that called it to a state prior to the call
    /// in case of an error.  So that calling process can find the standard
    //  file pointers (and sometimes descriptors) clobbered up.
    ///
    static TPid Daemonize(const char* logfile = 0, TDaemonFlags flags = 0);

private:
#if defined NCBI_THREAD_PID_WORKAROUND
    // Flags for internal sx_GetPid()
    enum EGetPidFlag {
        // get real or cached PID depending on thread ID
        ePID_GetCurrent,
        // get parent PID - real or cached depending on thread ID
        ePID_GetParent,
        // get real PID (of the thread) but do not cache it
        ePID_GetThread
    };
    static TPid sx_GetPid(EGetPidFlag flag);
    friend class CThread;
#endif
};



/////////////////////////////////////////////////////////////////////////////
///
/// CProcess --
///
/// Process management functions.
///
/// Class works with process identifiers and process handles.
/// On Unix both terms are equivalent and correspond a pid.
/// On MS Windows they are different.
///
/// @note:  All CExec:: functions work with process handles.

class NCBI_XNCBI_EXPORT CProcess : public CProcessBase
{
public:
    /// How to interpret the used process identifier.
    enum EType {
        ePid,     ///< A real process identifier (pid).
        eHandle   ///< A process handle (MS Windows).
    };

    /// Default constructor. Uses the current process pid.
    CProcess(void);

    /// Constructor.
    CProcess(TPid process, EType type = eHandle);
#if defined(NCBI_OS_MSWIN)
    // Note:
    //   On MS Windows process identifiers and process handles are different.
    //   so we need a separate constructor for process handles.
    CProcess(TProcessHandle process, EType type = eHandle);
#endif

    /// Get type of stored process identifier.
    /// @note
    ///   On Windows process identifiers and process handles are different.
    ///   On UNIX handle/pid is the same, and GetHandle()/GetPid() return
    ///   the same value.
    EType GetType(void) const { return m_Type; }

    /// Get stored process handle.
    /// @sa
    ///   GetCurrentHandle
    TProcessHandle GetHandle(void) const { return (TProcessHandle)m_Process; }

    /// Get stored process identifier (pid).
    /// @sa
    ///   GetCurrentPid
    TPid GetPid(void) const { return (TPid) m_Process; }

    /// Checks that stored process identifier (pid) represent the current process.
    /// @sa
    ///   GetCurrentPid
    bool IsCurrent(void);
 
    /// Check for process existence.
    ///
    /// @return
    ///   TRUE  - if the process is still running.
    ///   FALSE - if the process does exist or has already been terminated.
    /// @note
    ///   On Unix this method returns TRUE also for "zombie" processes,
    ///   those finished executing but waiting to post their exit code.
    ///   Usually the parent process should call Wait() for such processes
    ///   and release (aka reap) them.
    /// @sa
    ///   Wait
    bool IsAlive(void) const;

    /// Terminate process.
    ///
    /// First try "soft" and then try "hard" attempt to terminate the process.
    /// @note Even the "hard" attempt does not guarantee that the process will
    /// be actually killed.  Process termination can take some time, and the
    /// process may remain "alive" even after the "hard" termination attempt.
    ///
    /// @param timeout
    ///   Wait time in milliseconds between first "soft" and second "hard"
    ///   attempts to terminate the process. 
    ///   If timeout is zero then use an unsafe process termination: just
    ///   try to terminate the process without checks that it is really gone.
    /// @note
    ///   On UNIX in case of a zero or very small timeout, the killed process
    ///   may not be released immediately (and continue to persist as a zombie
    ///   process) even after this call.
    /// @return
    ///   TRUE  - if the process did not exist or was successfully terminated.
    ///   FALSE - if the process is still running, cannot be terminated, or
    ///           it is terminating right now (but is still incomplete).
    /// @sa KillGroup, KillGroupById
    bool Kill(unsigned long timeout = kDefaultKillTimeout);

    /// Terminate a group of processes.
    ///
    /// This method tries to terminate all processes in the group to which
    /// process, specified in the constructor, belongs.
    ///
    /// @param timeout
    ///   Wait time in milliseconds between first "soft" and second "hard"
    ///   attempts to terminate the process group. 
    ///   If timeout is zero then use an unsafe process termination: just
    ///   try to terminate the group without checks that it is really gone.
    /// @note
    ///   On UNIX in case of a zero or very small timeout, some processes
    ///   may not be released (and continue to persist as zombie processes)
    ///   even after this call.
    ///   On MS Windows this method tries to terminate the process itself
    ///   and all of its children.  But in case when one of the processes,
    ///   which should be terminated, spawns a new process at the very moment
    ///   that this method gets called, the new process may not be terminated.
    /// @return
    ///   TRUE  - if the process group did not exist or was successfully
    ///           terminated.
    ///   FALSE - if the process group is still running, cannot be terminated,
    ///           or it is terminating right now (but is still incomplete).
    /// @sa Kill
    bool KillGroup(unsigned long timeout = kDefaultKillTimeout) const;

    /// Terminate a group of processes with specified ID.
    ///
    /// Note: Implemented on UNIX only, on Windows returns FALSE.
    /// @param pgid
    ///   Process group ID to terminate.
    ///   if "pgid" parameter is zero, terminate the process group of
    ///   the current process
    /// @param timeout
    ///   Wait time in milliseconds between first "soft" and second "hard"
    ///   attempts to terminate the process group. 
    ///   If timeout is zero then use an unsafe process termination: just
    ///   try to terminate the process group without checks whether it is
    ///   really gone.
    /// @note
    ///   On UNIX in case of a zero or very small timeout, some processes from
    ///   the process group to be killed may not be released immediately
    ///   (and continue to persist as zombie processes) even after this call.
    /// @return
    ///   TRUE  - if the process group did not exist or was successfully
    ///           terminated.
    ///   FALSE - if the process group is still running, cannot be terminated,
    ///           or it is terminating right now (but is still incomplete).
    /// @sa Kill
    static bool KillGroupById(TPid pgid, unsigned long timeout = kDefaultKillTimeout);

    /// Extended exit information for waited process.
    /// All information about the process available only after Wait() method
    /// with specified parameter 'info' and if IsPresent() method returns TRUE.
    class NCBI_XNCBI_EXPORT CExitInfo
    {
    public:
        /// Constructor.
        CExitInfo(void);

        /// TRUE if the object contains information about the process state.
        ///
        /// All other methods' return values are good only if this method
        /// returns TRUE, otherwise they may throw exceptions.
        bool IsPresent(void) const;

        /// TRUE if the process is still alive.
        bool IsAlive(void) const;

        /// TRUE if the process terminated normally.
        bool IsExited(void) const;

        /// TRUE if the process terminated by a signal (UNIX only).
        bool IsSignaled(void) const;

        /// Get process exit code.
        /// Works only if IsExited() returns TRUE, otherwise returns -1.
        int GetExitCode(void) const;

        /// Get the signal number that has caused the process to terminate
        /// (UNIX only).
        /// Works only if IsSignaled() returns TRUE, otherwise returns -1.
        int GetSignal(void) const;

    private:
        int state;    ///< Process state (unknown/alive/terminated).
        int status;   ///< Process status information.

        friend class CProcess;
    };

    /// Wait until process terminates.
    ///
    /// Wait until the process terminates or timeout expires.
    /// Return immediately if the specified process has already terminated.
    /// @param timeout
    ///   Time interval in milliseconds (infinite by default) to wait.
    /// @param info
    ///   Extended exit information for terminated process.
    ///   Note that if CProcess::Kill() was used to terminate the process
    ///   then the extended information may not be available.
    /// @return
    ///   - Exit code of the process, if the call completed without errors.
    ///   - (-1) if an error occurred or it was impossible to get the exit
    ///     code of the process.  If 'info' parameter is specified, then it
    ///     is filled with additional information about the process.
    /// @note
    ///   It is recommended to call this method for all processes started 
    ///   in eNoWait or eDetach modes (except on Windows for eDetach), because
    ///   it releases "zombie" processes, i.e. those finished running and
    ///   waiting for their parent to obtain their exit code.  If Wait() is
    ///   not called somewhere, then the child process will be completely
    ///   removed from the system only when its parent process ends.
    /// @note
    ///   Infinite timeout can cause an application to stop responding. 
    /// @sa
    ///   IsAlive, CExitInfo, CExec, WaitInfinite, WaitTimeout
    int Wait(unsigned long timeout = kInfiniteTimeoutMs, CExitInfo* info = 0) const;

    /// Wait until the process terminates or timeout expires.
    /// @sa Wait
    int WaitTimeout(unsigned long timeout, CExitInfo* info = 0) const {
        return Wait(timeout, info);
    }

    /// Wait indefinitely until the process terminates.
    /// @sa Wait
    int WaitInfinite(CExitInfo* info = 0) const {
        return Wait(kInfiniteTimeoutMs, info);
    }


    /////////////////////////////////////////////////////////////////////////////
    // Process information

    /// @deprecated Please use CCurrentProcess::GetHandle() instead
    NCBI_DEPRECATED
    static TProcessHandle GetCurrentHandle(void) {
        return CCurrentProcess::GetHandle();
    }

    /// @deprecated Please use CCurrentProcess::GetPid() instead
    NCBI_DEPRECATED
    static TPid GetCurrentPid(void) {
        return CCurrentProcess::GetPid();
    }

    /// @deprecated Please use CCurrentProcess::GetParentPid() instead
    NCBI_DEPRECATED
    static TPid GetParentPid(void) {
        return CCurrentProcess::GetParentPid();
    }

    /// Get process execution times.
    /// For current process pid/handle it is the same as CCurrentProcess::GetTimes().
    ///
    /// @param real
    ///   Pointer to a variable that receives the amount of time in second that
    ///   the process/thread has executed, when available.
    /// @note
    ///   Here is no portable solution to get 'real' process execution time,
    ///   not all OS have an API to get such information from the system,
    ///   or it can include children times as well...
    ///   but it is available on some platforms like Windows, Linux and BSD.
    /// @param user
    ///   Pointer to a variable that receives the amount of time in seconds that
    ///   the process, process tree or thread has executed in user mode.
    ///   Note, that this value can exceed the amount of "real" time if
    ///   the process executes across multiple CPU cores (OS dependent).
    /// @param sys
    ///   Pointer to a variable that receives the amount of time in second that
    ///   the current process, process tree or thread has executed in kernel mode.
    /// @param what
    ///   Defines what times to measure: current thread, process or children processes:
    ///   - eProcess:
    ///         Current process, which is the sum of resources used
    ///         by all threads in the process.
    ///   - eChildren: (limited support)
    ///         All children of the process that have terminated already
    ///         and been waited for. These statistics will include the resources
    ///         used by grandchildren, and further removed descendants, if all of
    ///         the intervening descendants waited on their terminated children.
    ///   - eThread: (limited support)
    ///         Usage statistics for the calling thread.
    ///         CProcess should be initialized with the current process pid/handle,
    ///         or result is undefined.
    /// @return
    ///   TRUE on success; or FALSE on error.
    ///   If some value cannot be calculated on a current platform for "what" target,
    ///   it will be set to a negative value.
    /// @note
    ///   NULL arguments will not be filled in.
    /// @note
    ///   Usually it is possible to query information from users own processes,
    ///   but you should have an appropriate permissions to query other processes.
    /// @note
    ///   On Windows it is possible to get process times even for terminated processes,
    ///   that are Wait()-ed for, even if IsAlive() returns false.
    /// @sa
    ///   CCurrentProcess::GetTimes
    bool GetTimes(double* real, double* user, double* sys, EWhat what = eProcess);

    /// Get process memory usage.
    /// For current process pid/handle it is the same as CCurrentProcess::GetMemoryUsage().
    /// 
    /// @param usage
    ///   Total memory usage by a process, in bytes. See SMemoryUsage structure.
    /// @param resident
    ///   Resident set size (RSS), in bytes.
    ///   RSS is the portion of memory occupied by a process that is held in main memory (RAM).
    /// @param shared
    ///   (optional) Shared memory usage, in bytes.
    /// @param data
    ///   (optional) Data + stack usage, in bytes.
    /// @return
    ///   TRUE on success; or FALSE otherwise. 
    /// @note
    ///   NULL arguments will not be filled in.
    /// @sa
    ///   CCurrentProcess::GetMemoryUsage, SMemoryUsage
    bool GetMemoryUsage(SMemoryUsage& usage);

    /// Get the number of threads in the process.
    /// For current process pid/handle it is the same as CCurrentProcess::GetThreadCount().
    ///
    /// @return
    ///   Number of threads in the current process, or -1 on error.
    /// @sa
    ///   CCurrentProcess::GetThreadCount
    int GetThreadCount(void);

    /// Get the number of file descriptors consumed by the current process.
    /// For current process pid/handle it is the same as CCurrentProcess::GetFileDescriptorsCount().
    ///
    /// @return
    ///   Number of file descriptors consumed by the process, or -1 on error.
    /// @sa 
    ///   CCurrentProcess::GetFileDescriptorsCount
    /// @note Unix inly
    int GetFileDescriptorsCount(void);


    /////////////////////////////////////////////////////////////////////////////
    // Forks & Daemons

    /// Forking flags.
    /// @ deprecated Please use CCurrentProcess::FForkFlags instead
    enum FForkFlags {
        fFF_UpdateDiag      = CCurrentProcess::fFF_UpdateDiag,
        fFF_AllowExceptions = CCurrentProcess::fFF_AllowExceptions
    };
    /// Bit-wise OR of FForkFlags @sa FForkFlags
    /// @ deprecated Please use CCurrentProcess::TForkFlags instead
    typedef unsigned TForkFlags;

    /// Fork the process.
    /// @deprecated Please use CCurrentProcess::Fork() instead
    NCBI_DEPRECATED
    static TPid Fork(TForkFlags flags = fFF_UpdateDiag) {
        return CCurrentProcess::Fork(flags);
    }

    /// Daemonization flags
    /// @ deprecated Please use CCurrentProcess::FDaemonFlags instead
    enum FDaemonFlags {
        fDF_KeepCWD         = CCurrentProcess::fDF_KeepCWD,
        fDF_KeepStdin       = CCurrentProcess::fDF_KeepStdin,
        fDF_KeepStdout      = CCurrentProcess::fDF_KeepStdout,
        fDF_ImmuneTTY       = CCurrentProcess::fDF_ImmuneTTY,
        fDF_KeepParent      = CCurrentProcess::fDF_KeepParent,
        fDF_AllowExceptions = CCurrentProcess::fDF_AllowExceptions,
        fDF_AllowThreads    = CCurrentProcess::fDF_AllowThreads,

        fDontChroot         = CCurrentProcess::fDF_KeepCWD,
        fKeepStdin          = CCurrentProcess::fDF_KeepStdin,
        fKeepStdout         = CCurrentProcess::fDF_KeepStdout,
        fImmuneTTY          = CCurrentProcess::fDF_ImmuneTTY,
        fKeepParent         = CCurrentProcess::fDF_KeepParent
    };
    /// Bit-wise OR of FDaemonFlags @sa FDaemonFlags
    /// @ deprecated Please use CCurrentProcess::TDaemonFlags instead
    typedef unsigned int TDaemonFlags;

    /// Go daemon.
    ///
    /// Return TPid(-1) in the daemon thread (the parent thread doesn't return
    /// unless fKeepParent is set), or TPid(daemon) in the parent thread.
    /// On error return 0 in the parent thread (no daemon created), see errno.
    ///
    /// Reopen stderr/cerr in the daemon thread if "logfile" specified as
    /// non-NULL (stderr will open to "/dev/null" if "logfile" has been passed
    /// as ""), otherwise stderr is closed in the daemon thread.
    ///
    /// Unless instructed by the "flags" parameter, the daemon thread has its
    /// stdin/cin and stdout/cout closed, and the current working directory
    /// changed to the root directory ("/").
    ///
    /// If kept open, stdin and stdout are both redirected to "/dev/null".
    /// Opening a terminal device as a controlling terminal is allowed, unless
    /// fImmuneTTY is specified in the flags, which then causes a second
    /// fork() so that the resultant process won't be allowed to open a TTY as
    /// its controlling TTY (but only with an explicit O_NOCTTY, see open(2)),
    /// thus protecting the process from any blocking via TTY signaling.
    ///
    /// Note that this call is somewhat destructive and may not be able
    /// to restore the process that called it to a state prior to the call
    /// in case of an error.  So that calling process can find the standard
    //  file pointers (and sometimes descriptors) clobbered up.
    /// @deprecated Please use CCurrentProcess::Daemonize() instead
    NCBI_DEPRECATED
    static TPid Daemonize(const char* logfile = 0, TDaemonFlags flags = 0) {
        return CCurrentProcess::Daemonize(logfile, flags);
    };

private:
#if defined(NCBI_OS_MSWIN)
    // Return pid, converts from handle if necessary
    TPid x_GetPid(void) const;

    // Return process handle, converts from pid with desired rights if necessary.
    // Return NULL on error. Don't forget to close handle with x_CloseHandle().
    TProcessHandle x_GetHandle(DWORD desired_access, DWORD* errcode = 0 /*optional errcode to return*/) const;

    // Close handle, if necessary
    void x_CloseHandle(TProcessHandle handle) const;
#endif

    // itptr_t type can store both pid and process handle.
    intptr_t   m_Process;    ///< Process identifier.
    EType      m_Type;       ///< Type of process identifier.
    ETriState  m_IsCurrent;  ///< Status that m_Process represent the current process.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CPIDGuardException --
///

class NCBI_XNCBI_EXPORT CPIDGuardException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eStillRunning, ///< The process listed in the file is still around.
        eWrite         ///< Unable to write into the PID file.
    };

    virtual const char* GetErrCodeString(void) const override;

    /// Constructor.
    CPIDGuardException(const CDiagCompileInfo& info,
                       const CException* prev_exception, EErrCode err_code,
                       const string& message, TPid pid = 0, 
                       EDiagSev severity = eDiag_Error)
        : CException(info, prev_exception, message, severity),
          m_PID(pid)
        NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CPIDGuardException, CException);

public:
    virtual void ReportExtra(ostream& out) const override
    {
        out << "pid " << m_PID;
    }

    TPid GetPID(void) const throw() { return m_PID; }

protected:
    virtual void x_Assign(const CException& src) override
    {
        CException::x_Assign(src);
        m_PID = dynamic_cast<const CPIDGuardException&>(src).m_PID;
    }

private:
    TPid  m_PID;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CPIDGuard -- Process guard.
///
/// If file already exists, CPIDGuard try to check that the process with PID
/// specified in the file is running or not. If guarded process is still
/// running, CPIDGuard throw an exception. Otherwise, it create file with
/// current PID. CPIDGuard use reference counters in the PID file.
/// This means that some CPIDGuard objects can be created for the same PID
/// and file name.
///
/// @note
///   If you need something just to prevent run some application twice,
///   please look to CInterProcessLock class.
/// @note
///   CPIDGuard know nothing about PID file modification or deletion
///   by other processes directly, without using this API. Be aware.
/// 

class NCBI_XNCBI_EXPORT CPIDGuard
{
public:
    /// Create/check PID file.
    ///
    /// If the file already exists and identifies a live process,
    /// throws CPIDGuardException.
    /// @param filename
    ///   Name of the file to store PID. 
    ///   If "filename" contains path, it should be absolute
    ///   and points to an existing directory.
    ///   If "filename" contains no path, that $TMPDIR will be used on Unix,
    ///   and %HOME% on Windows to store it.
    CPIDGuard(const string& filename);

    /// Create/check PID file.
    ///
    /// If the file already exists and identifies a live process,
    /// throws CPIDGuardException.
    ///
    /// @param filename
    ///   Name of the file to store PID. 
    ///   If should not include any path, relative or absolute.
    /// @param dir
    ///   An absolute path to the existing directory on the file system
    ///   to store PID file "filename".
    ///   If "dir" is empty and "filename" contains no path,
    ///   that $TMPDIR will be used on Unix, and %HOME% on Windows to store it.
    /// @deprecated
    NCBI_DEPRECATED
    CPIDGuard(const string& filename, const string& dir);

    /// Destructor.
    ///
    /// Just calls Release();
    ~CPIDGuard(void);

    /// Release PID.
    ///
    /// Decrease reference counter for current PID and remove the file
    /// if it is not used more (reference counter is 0).
    void Release(void);

    /// Remove the file.
    ///
    /// Remove PID file forcibly, ignoring any reference counter.
    void Remove(void);

    /// Update PID in the file.
    ///
    /// @param pid
    ///   The new process ID to store (defaults to the current PID);
    ///   useful when the real work occurs in a child process that outlives
    ///   the parent.
    void UpdatePID(TPid pid = 0);

    /// Returns non-zero if there was a stale file.
    /// Always return 0, because new implementation don't allow
    /// any stale file left on the file system.
    ///
    /// @deprecated
    NCBI_DEPRECATED
    TPid GetOldPID(void) { return 0; }

private:
    string  m_Path;  //< File path to store PID.
    TPid    m_PID;   //< Sored PID.
    unique_ptr<CInterProcessLock> m_MTGuard;  //< MT-Safe protection guard.
    unique_ptr<CInterProcessLock> m_PIDGuard; //< Guard to help with "PID reuse" problem.
};


/* @} */

END_NCBI_SCOPE


#endif  /* CORELIB___NCBI_PROCESS__HPP */
