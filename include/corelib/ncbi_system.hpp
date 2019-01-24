#ifndef NCBI_SYSTEM__HPP
#define NCBI_SYSTEM__HPP

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
 * Author:  Vladimir Ivanov
 *
 * File Description: System functions
 *
 */


#include <corelib/ncbitime.hpp>
#include <time.h>

#if defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#endif

BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
///
/// Process limits
///
/// If, during the program execution, the process exceed any from limits
/// (see ELimitsExitCodeMemory) then:
///   1) Dump info about current program's state to log-stream.
///      - if defined print handler "handler", then it will be used.
///      - if defined "parameter", it will be passed into print handler;
///   2) Terminate the program.
/// One joint print handler for all limit types is used.


/// Code for program's exit handler.
enum ELimitsExitCode {
    eLEC_None,    ///< Normal exit.
    eLEC_Memory,  ///< Memory limit.
    eLEC_Cpu      ///< CPU usage limit.
};

/// Type of parameter for print handler.
typedef void* TLimitsPrintParameter;

/// Type of handler for printing a dump information after generating
/// any limitation event.
/// @attention
///   If you use handlers together with SetCpuLimit(), be aware!
///   See SetCpuLimit() for details.
/// @attention
///   The exit print handler can be registered only once at first call to
///   SetCpuTimeLimit() or SetMemoryLimit(), even if you don't specify handler
///   directly and passed it as NULL! Be aware. All subsequent attempts to set
///   new handler will be ignored, but limits will be changed anyway.
/// @sa 
///   SetCpuTimeLimit, SetMemoryLimit
typedef void (*TLimitsPrintHandler)(ELimitsExitCode, size_t, CTime&, TLimitsPrintParameter);


/// [UNIX only]  Set memory limit.
///
/// Set the limit for the size of dynamic memory (heap) allocated
/// by the process. 
///
/// @note 
///   The implementation of malloc() can be different. Some systems use 
///   sbrk()-based implementation, other use mmap() system call to allocate
///   memory, ignoring data segment. In the second case SetHeapLimit() 
///   don't work at all. Usually don't know about how exactly malloc()
///   is implemented. We added another function - SetMemoryLimit(), that
///   supports mmap()-based memory allocation, please use it instead.
/// @param max_heap_size
///   The maximal amount of dynamic memory can be allocated by the process.
///   (including heap)
///   The 0 value lift off the heap restrictions.
/// @param handler
///   Pointer to a print handler used for dump output.
///   Use default handler if passed as NULL.
/// @param parameter
///   Parameter carried into the print handler. Can be passed as NULL.
/// @return 
///   Completion status.
/// @attention
///   The exit print handler can be registered only once at first call to
///   SetCpuTimeLimit() or SetMemoryLimit(), even if you don't specify handler
///   directly and passed it as NULL! Be aware. All subsequent attempts to set
///   new handler will be ignored, but limits will be changed anyway.
/// @sa SetMemoryLimit
/// @deprecated
NCBI_DEPRECATED
NCBI_XNCBI_EXPORT
extern bool SetHeapLimit(size_t max_size, 
                         TLimitsPrintHandler   handler   = NULL, 
                         TLimitsPrintParameter parameter = NULL);


/// [UNIX only]  Set memory limit.
///
/// Set the limit for the size of used memory allocated by the process.
/// 
/// @param max_size
///   The maximal amount of memory in bytes that can be allocated by
///   the process. Use the same limits for process's data segment
///   (including heap) and virtual memory (address space).
///   On 32-bit systems limit is at most 2 GiB, or this resource is unlimited.
///   The 0 value lift off the heap restrictions.
///   This value cannot exceed current hard limit set for the process.
/// @param handler
///   Pointer to a print handler used for dump output in the case of reaching
///   memory limit. Use default handler if passed as NULL.
/// @param parameter
///   Parameter carried into the print handler. Can be passed as NULL.
///   Useful if singular handler is used for setting some limits.
///   See also SetCpuTimeLimit().
/// @return 
///   Completion status.
///   It returns TRUE if both, the memory limits for the data segment and 
///   virtual memory, limitations were set. It can return FALSE if the limits
///   for the data segment has changed, but setting new values for
///   the virtual memory fails.
/// @note
///   By default it sets soft and hard memory limits to the same value.
/// @note
///   Setting a limits may not work on some systems, depends on OS, compilation
///   options and etc. Some systems enforce memory limits, other didn't.
///   Also, only privileged process can set hard memory limit.
/// @note
///   If the memory limit is reached, any subsequent memory allocations fails.
/// @attention
///   The exit print handler can be registered only once at first call to
///   SetCpuTimeLimit() or SetMemoryLimit(), even if you don't specify handler
///   directly and passed it as NULL! Be aware. All subsequent attempts to set
///   new handler will be ignored, but limits will be changed anyway.
/// @sa
///   SetCpuTimeLimit, TLimitsPrintHandler, SetMemoryLimitSoft, SetMemoryLimitHard
NCBI_XNCBI_EXPORT
extern bool SetMemoryLimit(size_t max_size, 
                           TLimitsPrintHandler   handler   = NULL, 
                           TLimitsPrintParameter parameter = NULL);

/// [UNIX only]  Set soft memory limit.
/// @sa SetMemoryLimit
NCBI_XNCBI_EXPORT
extern bool SetMemoryLimitSoft(size_t max_size, 
                           TLimitsPrintHandler   handler   = NULL, 
                           TLimitsPrintParameter parameter = NULL);

/// [UNIX only]  Set hard memory limit.
/// @note
///   Current soft memory limit will be automatically decreased,
///   if it exceed new value for the hard memory limit.
/// @note
///   Only privileged process can increase current hard level limit.
/// @sa SetMemoryLimit
NCBI_XNCBI_EXPORT
extern bool SetMemoryLimitHard(size_t max_size, 
                           TLimitsPrintHandler   handler   = NULL, 
                           TLimitsPrintParameter parameter = NULL);


/// [UNIX only]  Set CPU usage limit.
///
/// Set the limit for the CPU time that can be consumed by current process.
/// 
/// @param max_cpu_time
///   The maximal amount of seconds of CPU time can be consumed by the process.
///   The 0 value lifts off the CPU time restrictions if allowed to do so.
/// @param terminate_delay_time
///   The time in seconds that the process will have to terminate itself after
///   receiving a signal about exceeding CPU usage limit. After that it can
///   be killed by OS.
/// @param handler
///   Pointer to a print handler used for dump output in the case of reaching
///   CPU usage limit. Use default handler if passed as NULL.
///   Note, that default handler is not async-safe (see attention below),
///   and can lead to coredump and application crash instead of program 
///   termination, so use it on your own risk.         
/// @param parameter
///   Parameter carried into the print handler. Can be passed as NULL.
/// @return 
///   Completion status.
/// @note
///   Setting a low CPU time limit cannot be generally undone to a value
///   higher than "max_cpu_time + terminate_delay_time" at a later time.
/// @attention
///   The exit print handler can be registered only once at first call to
///   SetCpuTimeLimit() or SetMemoryLimit(), even if you don't specify handler
///   directly and passed it as NULL! Be aware. All subsequent attempts to set
///   new handler will be ignored, but limits will be changed anyway.
/// @attention
///   Only async-safe library functions and system calls can be used in 
///   the print handler. For example, you cannot use C++ streams (cout/cerr)
///   and printf() calls here, but write() is allowed... You can find a list
///   of such functions in the C++ documentation (see man, internet and etc).
///   Also, avoid to alter any shared (global) variables, except that are
///   declared to be of storage class and type "volatile sig_atomic_t".
/// @sa SetMemoryLimit, TLimitsPrintHandler
NCBI_XNCBI_EXPORT
extern bool SetCpuTimeLimit(unsigned int          max_cpu_time,
                            unsigned int          terminate_delay_time,
                            TLimitsPrintHandler   handler, 
                            TLimitsPrintParameter parameter);

NCBI_DEPRECATED
NCBI_XNCBI_EXPORT
extern bool SetCpuTimeLimit(size_t                max_cpu_time,
                            TLimitsPrintHandler   handler = NULL, 
                            TLimitsPrintParameter parameter = NULL,
                            size_t                terminate_delay_time = 5);



/////////////////////////////////////////////////////////////////////////////
///
/// CSystemInfo --
///
/// Return various system information.
/// All methods work on UNIX and Windows, unless otherwise stated.

class NCBI_XNCBI_EXPORT CSystemInfo
{
public:
    /// Get actual user name for the current process.
    /// @note
    ///   Doesn't use environment as it can be changed.
    /// @return
    ///   Process owner user name, or empty string if it cannot be determined.
    static string GetUserName(void);

    /// Return number of active CPUs (never less than 1).
    static unsigned int GetCpuCount(void);
    
    /// Get system uptime in seconds.
    /// @return
    ///   Seconds since last boot, or negative number if cannot determine it
    ///   on current platform, or on error.
    static double GetUptime(void);

    /// Return the amount of actual physical memory, in bytes.
    /// On some platforms it can be less then installed physical memory,
    /// and represent a total usable RAM (physical RAM minus reserved and the kernel).
    /// @return
    ///   0, if cannot determine it on current platform, or if an error occurs.
    static Uint8 GetTotalPhysicalMemorySize(void);

    /// Return the amount of available physical memory, in bytes.
    /// @return
    ///   0, if cannot determine it on current platform, or on error.
    static Uint8 GetAvailPhysicalMemorySize(void);

    /// Return virtual memory page size.
    /// @return
    ///   0, if cannot determine it on current platform, or on error.
    static unsigned long GetVirtualMemoryPageSize(void);

    /// Return size of an allocation unit (usually it is a multiple of page size).
    /// @return
    ///   0, if cannot determine it on current platform, or on error.
    static unsigned long GetVirtualMemoryAllocationGranularity(void);

    /// Get number of (statistics) clock ticks per second.
    /// Many OS system information values use it as a size unit.
    /// @return
    ///   0, if cannot determine it on current platform, or on error.
    static clock_t GetClockTicksPerSecond(void);
};



/////////////////////////////////////////////////////////////////////////////
///
/// System/memory information (deprecated, please use CSystemInfo class)
///

/// Get process owner actual user name
/// @deprecated  Please use CSystemInfo::GetUserName()
inline NCBI_DEPRECATED
string GetProcessUserName(void)
{
    return CSystemInfo::GetUserName();
}

/// Return number of active CPUs (never less than 1).
/// @deprecated  Please use CSystemInfo::GetCpuCount()
inline NCBI_DEPRECATED
unsigned int GetCpuCount(void)
{
    return CSystemInfo::GetCpuCount();
}

/// Return virtual memory page size.
/// @deprecated  Please use CSystemInfo::GetVirtualMemoryPageSize()
inline NCBI_DEPRECATED
unsigned long GetVirtualMemoryPageSize(void)
{
    return CSystemInfo::GetVirtualMemoryPageSize();
}

/// Return size of an allocation unit (usually it is a multiple of page size).
/// @deprecated  Please use CSystemInfo::GetVirtualMemoryAllocationGranularity()
inline NCBI_DEPRECATED
unsigned long GetVirtualMemoryAllocationGranularity(void)
{
    return CSystemInfo::GetVirtualMemoryAllocationGranularity();
}

/// Return the amount of physical memory available in the system.
/// @deprecated  Please use CSystemInfo::GetTotalPhysicalMemorySize()
inline NCBI_DEPRECATED
Uint8 GetPhysicalMemorySize(void)
{
    return CSystemInfo::GetTotalPhysicalMemorySize();
}

/// @deprecated  Please use C[Current]Process::GetMemoryUsage()
NCBI_XNCBI_EXPORT
NCBI_DEPRECATED
extern bool GetMemoryUsage(size_t* total, size_t* resident, size_t* shared);


/// @deprecated  Please use C[Current]Process::GetTimes()
NCBI_XNCBI_EXPORT 
NCBI_DEPRECATED
extern bool GetCurrentProcessTimes(double* user_time, double* system_time);


/// @deprecated  Please use C[Current]Process::GetFileDescriptorsCount
NCBI_XNCBI_EXPORT
NCBI_DEPRECATED
extern int GetProcessFDCount(int* soft_limit = NULL, int* hard_limit = NULL);


/// [Linux only]  Provides the number of threads in the current process.
/// @deprecated  Please use C[Current]Process::GetThreadCount
NCBI_XNCBI_EXPORT
NCBI_DEPRECATED
extern int GetProcessThreadCount(void);



/////////////////////////////////////////////////////////////////////////////
///
/// Memory advise
///


/// What type of data access pattern will be used for specified memory region.
///
/// Advises the VM system that the a certain region of memory will be
/// accessed following a type of pattern. The VM system uses this
/// information to optimize work with mapped memory.
///
/// NOTE: Works on UNIX platform only.
typedef enum {
    eMADV_Normal,      ///< No further special treatment -- by default
    eMADV_Random,      ///< Expect random page references
    eMADV_Sequential,  ///< Expect sequential page references
    eMADV_WillNeed,    ///< Expect access in the near future
    eMADV_DontNeed,    ///< Do not expect access in the near future
    // Available since Linux kernel 2.6.16
    eMADV_DoFork,      ///< Do inherit across fork() -- by default
    eMADV_DontFork,    ///< Don't inherit across fork()
    // Available since Linux kernel 2.6.32
    eMADV_Mergeable,   ///< KSM may merge identical pages
    eMADV_Unmergeable  ///< KSM may not merge identical pages -- by default
} EMemoryAdvise;


/// [UNIX only]  Advise on memory usage for specified memory region.
///
/// @param addr
///   Address of memory region whose usage is being advised.
///   Some implementation requires that the address start be page-aligned. 
/// @param len
///   Length of memory region whose usage is being advised.
/// @param advise
///   Advise on expected memory usage pattern.
/// @return
///   - TRUE, if memory advise operation successful.
///   - FALSE, if memory advise operation not successful, or is
///     not supported on current platform.
/// @sa
///   EMemoryAdvise
NCBI_XNCBI_EXPORT
extern bool MemoryAdvise(void* addr, size_t len, EMemoryAdvise advise);



/////////////////////////////////////////////////////////////////////////////
///
/// Sleep
///
/// Suspend execution for a time.
///
/// Sleep for at least the specified number of microsec/millisec/seconds.
/// Time slice restrictions are imposed by platform/OS.
/// On UNIX the sleep can be interrupted by a signal.
/// Sleep*Sec(0) have no effect (but may cause context switches).
///
/// [UNIX & Windows]

NCBI_XNCBI_EXPORT
extern void SleepSec(unsigned long sec,
                     EInterruptOnSignal onsignal = eRestartOnSignal);

NCBI_XNCBI_EXPORT
extern void SleepMilliSec(unsigned long ml_sec,
                          EInterruptOnSignal onsignal = eRestartOnSignal);

NCBI_XNCBI_EXPORT
extern void SleepMicroSec(unsigned long mc_sec,
                          EInterruptOnSignal onsignal = eRestartOnSignal);



/////////////////////////////////////////////////////////////////////////////
///
/// Suppress Diagnostic Popup Messages
///

/// Suppress modes
enum ESuppressSystemMessageBox {
    fSuppress_System    = (1<<0),     ///< System errors
    fSuppress_Runtime   = (1<<1),     ///< Runtime library
    fSuppress_Debug     = (1<<2),     ///< Debug library
    fSuppress_Exception = (1<<3),     ///< Unhandled exceptions
    fSuppress_All       = fSuppress_System | fSuppress_Runtime | 
                          fSuppress_Debug  | fSuppress_Exception,
    fSuppress_Default   = fSuppress_All
};
/// Binary OR of "ESuppressSystemMessageBox"
typedef int TSuppressSystemMessageBox;  

/// Suppress popup messages on execution errors.
///
/// NOTE: MS Windows-specific.
/// Suppresses all error message boxes in both runtime and in debug libraries,
/// as well as all General Protection Fault messages.
NCBI_XNCBI_EXPORT
extern void SuppressSystemMessageBox(TSuppressSystemMessageBox mode = fSuppress_Default);

/// Prevent run of SuppressSystemMessageBox().
///
/// NOTE: MS Windows-specific.
/// If this function is called, all following calls of
/// SuppressSystemMessageBox() will be ignored. If SuppressSystemMessageBox()
/// was executed before, that this function print out a critical error message.
/// For example can be used in CGI applications where SuppressSystemMessageBox
/// always calls in the CCgiApplication constructor.
/// 
NCBI_XNCBI_EXPORT
extern void DisableSuppressSystemMessageBox();


/// Check if system message box has been suppressed for debug library.
///
/// NOTE: MS Windows-specific.
NCBI_XNCBI_EXPORT
extern bool IsSuppressedDebugSystemMessageBox();


END_NCBI_SCOPE

#endif  /* NCBI_SYSTEM__HPP */
