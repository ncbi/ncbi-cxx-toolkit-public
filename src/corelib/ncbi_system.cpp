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
 * Authors:  Vladimir Ivanov, Denis Vakatov, Anton Lavrentiev
 *
 * File Description:  System functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_system.hpp>

#if defined(NCBI_OS_MAC)
#  include <OpenTransport.h>
#endif

#if defined(NCBI_OS_UNIX)
#  if defined(NCBI_OS_SOLARIS)
#    include <corelib/ncbifile.hpp>
#  endif
#  include <sys/time.h>
#  include <sys/resource.h>
#  include <sys/times.h>
#  include <limits.h>
#  include <time.h>
#  include <unistd.h>
#  if defined(NCBI_OS_BSD) || defined(NCBI_OS_DARWIN)
#    include <sys/sysctl.h>
#  endif
#  define USE_SETHEAPLIMIT
#  define USE_SETCPULIMIT
#endif

#if defined(NCBI_OS_DARWIN)
extern "C" {
#  if defined(NCBI_COMPILER_MW_MSL)
#    include <ncbi_mslextras.h>
#  endif
#  include <mach/mach.h>
#  include <mach/mach_host.h>
#  include <mach/host_info.h>
} /* extern "C" */
#endif

#if defined(USE_SETCPULIMIT)
#  include <signal.h>
#endif

#if defined(NCBI_OS_MSWIN)
#  include <corelib/ncbidll.hpp>
#  include <crtdbg.h>
// #  include <psapi.h>
#  include <stdlib.h>
#  include <windows.h>

struct SProcessMemoryCounters
{
    DWORD  size;
    DWORD  page_fault_count;
    SIZE_T peak_working_set_size;
    SIZE_T working_set_size;
    SIZE_T quota_peak_paged_pool_usage;
    SIZE_T quota_paged_pool_usage;
    SIZE_T quota_peak_nonpaged_pool_usage;
    SIZE_T quota_nonpaged_pool_usage;
    SIZE_T pagefile_usage;
    SIZE_T peak_pagefile_usage;
};

#endif


BEGIN_NCBI_SCOPE


// MIPSpro 7.3 workarounds:
//   1) it declares set_new_handler() in both global and std:: namespaces;
//   2) it apparently gets totally confused by `extern "C"' inside a namespace.
#if defined(NCBI_COMPILER_MIPSPRO)
#  define set_new_handler std::set_new_handler
#else
extern "C" {
    static void s_ExitHandler(void);
    static void s_SignalHandler(int sig);
}
#endif  /* NCBI_COMPILER_MIPSPRO */


#ifdef NCBI_OS_UNIX

DEFINE_STATIC_FAST_MUTEX(s_ExitHandler_Mutex);
static bool                  s_ExitHandlerIsSet  = false;
static ELimitsExitCode       s_ExitCode          = eLEC_None;
static CTime                 s_TimeSet;
static size_t                s_HeapLimit         = 0;
static size_t                s_CpuTimeLimit      = 0;
static char*                 s_ReserveMemory     = 0;
static TLimitsPrintHandler   s_PrintHandler      = 0;
static TLimitsPrintParameter s_PrintHandlerParam = 0;


#if !defined(CLK_TCK)  &&  defined(CLOCKS_PER_SEC)
#  define CLK_TCK CLOCKS_PER_SEC
#endif


// Routine to be called at the exit from application
//
static void s_ExitHandler(void)
{
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);

    // Free reserved memory
    if ( s_ReserveMemory ) {
        delete[] s_ReserveMemory;
        s_ReserveMemory = 0;
    }

    // User defined dump
    if ( s_PrintHandler ) {
        size_t limit_size; 

        switch ( s_ExitCode ) {
        case eLEC_Memory: {
            limit_size = s_HeapLimit;
            break;
        }
        case eLEC_Cpu: {
            limit_size = s_CpuTimeLimit;
            break;
        }
        default:
            return;
        }
        // Call user's print handler
        (*s_PrintHandler)(s_ExitCode, limit_size, s_TimeSet, 
                          s_PrintHandlerParam);
        return;
    }

    // Standard dump
    switch ( s_ExitCode ) {
        
    case eLEC_Memory:
        {
            ERR_POST("Memory heap limit exceeded in allocating memory " \
                     "by operator new (" << s_HeapLimit << " bytes)");
            break;
        }
        
    case eLEC_Cpu: 
        {
            ERR_POST("CPU time limit exceeded (" << s_CpuTimeLimit << " sec)");
            tms buffer;
            if (times(&buffer) == (clock_t)(-1)) {
                ERR_POST("Error in getting CPU time consumed by program");
                break;
            }
            clock_t tick = sysconf(_SC_CLK_TCK);
#ifdef CLK_TCK
            if (!tick  ||  tick == (clock_t)(-1))
                tick = CLK_TCK;
#endif /*CLK_TCK*/
            if (tick == (clock_t)(-1))
                tick = 0;
            LOG_POST("\tuser CPU time   : " << 
                     buffer.tms_utime/(tick ? tick : 1) <<
                     (tick ? " sec" : " tick"));
            LOG_POST("\tsystem CPU time : " << 
                     buffer.tms_stime/(tick ? tick : 1) <<
                     (tick ? " sec" : " tick"));
            LOG_POST("\ttotal CPU time  : " <<
                     (buffer.tms_stime + buffer.tms_utime)/(tick ? tick : 1) <<
                     (tick ? " sec" : " tick"));
            break;
        }

    default:
        return;
    }
    
    // Write program's time
    CTime ct(CTime::eCurrent);
    CTime et(2000, 1, 1);
    et.AddSecond((int) (ct.GetTimeT() - s_TimeSet.GetTimeT()));
    LOG_POST("Program's time: " << Endm <<
             "\tstart limit - " << s_TimeSet.AsString() << Endm <<
             "\ttermination - " << ct.AsString() << Endm);
    et.SetFormat("h:m:s");
    LOG_POST("\texecution   - " << et.AsString());
}


// Set routine to be called at the exit from application
//
static bool s_SetExitHandler(TLimitsPrintHandler handler, 
                             TLimitsPrintParameter parameter)

{
    // Set exit routine if it not set yet
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);
    if ( !s_ExitHandlerIsSet ) {
        if (atexit(s_ExitHandler) != 0) {
            return false;
        }
        s_ExitHandlerIsSet = true;
        s_TimeSet.SetCurrent();

        // Store print handler and parameter
        s_PrintHandler = handler;
        s_PrintHandlerParam = parameter;

        // Reserve some memory (10Kb)
        s_ReserveMemory = new char[10000];
    }
    return true;
}
    
#endif /* NCBI_OS_UNIX */



/////////////////////////////////////////////////////////////////////////////
//
// SetHeapLimit
//

#ifdef USE_SETHEAPLIMIT

// Handler for operator new
static void s_NewHandler(void)
{
    s_ExitCode = eLEC_Memory;
    exit(-1);
}


bool SetHeapLimit(size_t max_heap_size,
                  TLimitsPrintHandler handler, 
                  TLimitsPrintParameter parameter)
{
    if (s_HeapLimit == max_heap_size) 
        return true;
    
    if ( !s_SetExitHandler(handler, parameter) )
        return false;

    // Set new heap limit
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);
    rlimit rl;
    if ( max_heap_size ) {
        set_new_handler(s_NewHandler);
        rl.rlim_cur = rl.rlim_max = max_heap_size;
    }
    else {
        // Set off heap limit
        set_new_handler(0);
        rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    }
    if (setrlimit(RLIMIT_DATA, &rl) != 0) 
        return false;

    s_HeapLimit = max_heap_size;

    return true;
}

#else

bool SetHeapLimit(size_t max_heap_size, 
                  TLimitsPrintHandler handler, 
                  TLimitsPrintParameter parameter)
{
  return false;
}

#endif /* USE_SETHEAPLIMIT */



/////////////////////////////////////////////////////////////////////////////
//
// SetCpuTimeLimit
//

#ifdef USE_SETCPULIMIT

static void s_SignalHandler(int _DEBUG_ARG(sig))
{
    _ASSERT(sig == SIGXCPU);
    _VERIFY(signal(SIGXCPU, SIG_IGN) != SIG_ERR);
    s_ExitCode = eLEC_Cpu;
    _exit(-1);
}


bool SetCpuTimeLimit(size_t                max_cpu_time,
                     TLimitsPrintHandler   handler, 
                     TLimitsPrintParameter parameter,
                     size_t                terminate_time)
{
    if (s_CpuTimeLimit == max_cpu_time) 
        return true;
    
    if ( !s_SetExitHandler(handler, parameter) )
        return false;
    
    // Set new CPU time limit
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);

    struct rlimit rl;
    if ( max_cpu_time ) {
        rl.rlim_cur = max_cpu_time;
        rl.rlim_max = max_cpu_time + terminate_time;
    }
    else {
        // Set off CPU time limit
        rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    }

    if (setrlimit(RLIMIT_CPU, &rl) != 0) {
        return false;
    }
    s_CpuTimeLimit = max_cpu_time;

    // Set signal handler for SIGXCPU
    if (signal(SIGXCPU, s_SignalHandler) == SIG_ERR) {
        return false;
    }

    return true;
}

#else

bool SetCpuTimeLimit(size_t                max_cpu_time,
                     TLimitsPrintHandler   handler, 
                     TLimitsPrintParameter parameter,
                     size_t                terminate_time)

{
    return false;
}

#endif /* USE_SETCPULIMIT */


/////////////////////////////////////////////////////////////////////////////
//
// GetCpuCount
//

unsigned int GetCpuCount(void)
{
#if defined(NCBI_OS_MSWIN)
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return (unsigned int) sysInfo.dwNumberOfProcessors;

#elif defined(NCBI_OS_DARWIN)
    host_basic_info_data_t hinfo;
    mach_msg_type_number_t hinfo_count = HOST_BASIC_INFO_COUNT;
    kern_return_t rc;

    rc = host_info(mach_host_self(), HOST_BASIC_INFO,
                   (host_info_t)&hinfo, &hinfo_count);

    if (rc != KERN_SUCCESS) {
        return 1;
    }
    return hinfo.avail_cpus;

#elif defined(NCBI_OS_UNIX)
    long nproc = 0;
# if defined(_SC_NPROC_ONLN)
    nproc = sysconf(_SC_NPROC_ONLN);
# elif defined(_SC_NPROCESSORS_ONLN)
    nproc = sysconf(_SC_NPROCESSORS_ONLN);
# elif defined(NCBI_OS_BSD) || defined(NCBI_OS_DAWRIN)
    size_t len = sizeof(nproc);
    int mib[2];
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    if (sysctl(mib, 2, &nproc, &len, 0, 0) < 0 || len != sizeof(nproc))
        nproc = -1;
# endif /*UNIX_FLAVOR*/
    return nproc <= 0 ? 1 : (unsigned int) nproc;
#else
    return 1;
#endif /*OS_TYPE*/
}



/////////////////////////////////////////////////////////////////////////////
//
// GetVirtualMemoryPageSize
//

unsigned long GetVirtualMemoryPageSize(void)
{
    static unsigned long ps = 0;

    if (!ps) {
#if defined(NCBI_OS_MSWIN)
        SYSTEM_INFO si;
        GetSystemInfo(&si); 
        ps = si.dwAllocationGranularity;
#elif defined(NCBI_OS_UNIX) 
#  if   defined(_SC_PAGESIZE)
#    define NCBI_SC_PAGESIZE _SC_PAGESIZE
#  elif defined(_SC_PAGE_SIZE)
#    define NCBI_SC_PAGESIZE _SC_PAGE_SIZE
#  elif defined(NCBI_SC_PAGESIZE)
#    undef  NCBI_SC_PAGESIZE
#  endif
#  ifndef   NCBI_SC_PAGESIZE
        long x = 0;
#  else
        long x = sysconf(NCBI_SC_PAGESIZE);
#    undef  NCBI_SC_PAGESIZE
#  endif
        if (x <= 0) {
#  ifdef HAVE_GETPAGESIZE
            if ((x = getpagesize()) <= 0)
                return 0;
#  endif
            return 0;
        }
        ps = x;
#endif /*OS_TYPE*/
    }
    return ps;
}


bool GetMemoryUsage(size_t* total, size_t* resident, size_t* shared)
{
    size_t scratch;
    if ( !total )    { total    = &scratch; }
    if ( !resident ) { resident = &scratch; }
    if ( !shared )   { shared   = &scratch; }
#ifdef NCBI_OS_MSWIN
    try {
        // Load PSAPI dynamic library -- it should exist on MS-Win NT/2000/XP
        CDll psapi_dll("psapi.dll", CDll::eLoadNow, CDll::eAutoUnload);
        BOOL (STDMETHODCALLTYPE FAR * dllGetProcessMemoryInfo)
            (HANDLE process, SProcessMemoryCounters& counters, DWORD size) = 0;
        dllGetProcessMemoryInfo
            = psapi_dll.GetEntryPoint_Func("GetProcessMemoryInfo",
                                           &dllGetProcessMemoryInfo);
        if (dllGetProcessMemoryInfo) {
            SProcessMemoryCounters counters;
            dllGetProcessMemoryInfo(GetCurrentProcess(), counters,
                                    sizeof(counters));
            *total    = counters.quota_paged_pool_usage
                        + counters.quota_nonpaged_pool_usage;
            *resident = counters.working_set_size;
            *shared   = 0;
            return true;
        }
    } catch (CException) {
        // Just catch all exceptions from CDll
    }
#elif defined(NCBI_OS_LINUX)
    static unsigned long page_size = GetVirtualMemoryPageSize();
    CNcbiIfstream statm("/proc/self/statm");
    if (statm) {
        statm >> *total >> *resident >> *shared;
        *total    *= page_size;
        *resident *= page_size;
        *shared   *= page_size;
        return true;
    }
#elif defined(NCBI_OS_SOLARIS)
    Int8 len = CFile("/proc/self/as").GetLength();
    if (len > 0) {
        *total    = len;
        *resident = len; // conservative estimate
        *shared   = 0;   // does this info exist anywhere?
        return true;
    }
#elif defined(HAVE_GETRUSAGE)
    struct rusage ru;
    memset(&ru, '0', sizeof(ru));
    if (getrusage(RUSAGE_SELF, &ru) >= 0  &&  ru.ru_maxrss > 0) {
        // XXX - account for "ticks of execution"?
        *total = ru.ru_ixrss + ru.ru_idrss + ru.ru_isrss;
        *resident = ru.ru_maxrss;
        *shared = ru.ru_ixrss;
        return true;
    }
#endif
    return false;
}


/////////////////////////////////////////////////////////////////////////////
//
// Sleep
//

void SleepMicroSec(unsigned long mc_sec, EInterruptOnSignal onsignal)
{
#if defined(NCBI_OS_MSWIN)
    Sleep((mc_sec + 500) / 1000);
#elif defined(NCBI_OS_UNIX)
#  if defined(HAVE_NANOSLEEP)
    struct timespec delay, unslept;
    delay.tv_sec  =  mc_sec / kMicroSecondsPerSecond;
    delay.tv_nsec = (mc_sec % kMicroSecondsPerSecond) * 1000;
    while (nanosleep(&delay, &unslept) < 0) {
        if (errno != EINTR  ||  onsignal == eInterruptOnSignal)
            break;
        delay = unslept;
    }
#  else
    // Portable but ugly.
    // Most implementations of select() do not modifies timeout to reflect
    // the amount of time not slept; but some do this. Also, on some
    // platforms it can be interrupted by a signal, on other not.
    struct timeval delay;
    delay.tv_sec  = mc_sec / kMicroSecondsPerSecond;
    delay.tv_usec = mc_sec % kMicroSecondsPerSecond;
    while (select(0, (fd_set*) 0, (fd_set*) 0, (fd_set*) 0, &delay) < 0) {
#    if defined(SELECT_UPDATES_TIMEOUT)
        if (errno != EINTR  ||  onsignal == eInterruptOnSignal)
#    endif
            break;
    }
#  endif /*HAVE_NANOSLEEP*/

#elif defined(NCBI_OS_MAC)
    OTDelay((mc_sec + 500) / 1000);
#endif /*NCBI_OS_...*/
}


void SleepMilliSec(unsigned long ml_sec, EInterruptOnSignal onsignal)
{
#if defined(NCBI_OS_MSWIN)
    Sleep(ml_sec);
#elif defined(NCBI_OS_UNIX)
    SleepMicroSec(ml_sec * 1000, onsignal);
#elif defined(NCBI_OS_MAC)
    OTDelay(ml_sec);
#endif
}


void SleepSec(unsigned long sec, EInterruptOnSignal onsignal)
{
    SleepMicroSec(sec * kMicroSecondsPerSecond, onsignal);
}



/////////////////////////////////////////////////////////////////////////////
///
/// Suppress Diagnostic Popup Messages
///

#if defined(NCBI_OS_MSWIN)

static bool s_EnableSuppressSystemMessageBox = true;

// Handler for "Unhandled" exceptions
static LONG CALLBACK _SEH_Handler(EXCEPTION_POINTERS* ep)
{
    // Always terminate a program
    return EXCEPTION_EXECUTE_HANDLER;
}

#endif

extern void SuppressSystemMessageBox(TSuppressSystemMessageBox mode)
{
#if defined(NCBI_OS_MSWIN)
    if ( !s_EnableSuppressSystemMessageBox ) {
        return;
    }
    // System errors
    if ( (mode & fSuppress_System) == fSuppress_System ) {
       SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                    SEM_NOOPENFILEERRORBOX);
    }
    // Runtime library
    if ( (mode & fSuppress_Runtime) == fSuppress_Runtime ) {
        _set_error_mode(_OUT_TO_STDERR);
    }
    // Debug library
    if ( (mode & fSuppress_Debug) == fSuppress_Debug ) {
        _CrtSetReportFile(_CRT_WARN,   _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    }
    // Exceptions
    if ( (mode & fSuppress_Exception) == fSuppress_Exception ) {
        SetUnhandledExceptionFilter(_SEH_Handler);
    }
#else
    // not implemented
#endif
}


extern void DisableSuppressSystemMessageBox()
{
#if defined(NCBI_OS_MSWIN)
    s_EnableSuppressSystemMessageBox = false;
#else
    // not implemented
#endif
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.57  2006/09/11 20:15:44  ivanov
 * Fixed compilation error
 *
 * Revision 1.56  2006/09/11 19:51:14  ivanov
 * MSWin:
 * Added unhandled exception filter to ESuppressSystemMessageBox.
 * Added function DisableSuppressSystemMessageBox().
 *
 * Revision 1.55  2006/09/08 12:12:57  ivanov
 * Fixed compilation error on MS Windows.
 *
 * Revision 1.54  2006/09/07 23:21:14  ucko
 * +<corelib/ncbifile.hpp> on Solaris.
 *
 * Revision 1.53  2006/09/07 19:46:01  ucko
 * Add a semi-portable GetMemoryUsage method.
 *
 * Revision 1.52  2006/03/24 13:51:33  ivanov
 * SleepMicroSec [Unix]: use advantage of SELECT_UPDATES_TIMEOUT
 *
 * Revision 1.51  2006/03/17 14:32:05  ivanov
 * SleepMicroSec() [Unix] -- ignore 'onsignal'parameter for platforms that
 * use select(), sleeping less is more preferble here than sleeping longer.
 *
 * Revision 1.50  2006/03/14 13:24:34  ivanov
 * Sleep*(): added parameter of EInterruptOnSignal type.
 * [Unix] try to utilize unslept part of the time if interrupted by a signal.
 *
 * Revision 1.49  2006/03/13 15:27:43  ivanov
 * Use _exit() in the signal handler
 *
 * Revision 1.48  2006/03/02 23:52:49  lavr
 * Show Sleep... inconsistency at least for Linux
 *
 * Revision 1.47  2006/02/23 17:44:36  lavr
 * GetVirtualMemoryPageSize(): use sysctl() first (fallback to getpagesize())
 *
 * Revision 1.46  2006/02/23 15:07:04  lavr
 * GetVirtualMemoryPageSize():  bug fixed;  reworked to cache the result
 *
 * Revision 1.45  2006/01/12 19:41:54  ivanov
 * SetCpuTimeLimit: added new parameter - maximum process termination
 * time (default 5 seconds)
 *
 * Revision 1.44  2006/01/12 19:19:40  ivanov
 * SetCpuTimeLimit: set hard limit to 1 sec more than soft limit
 *
 * Revision 1.43  2005/06/10 19:23:30  lavr
 * _SC_PAGE_SIZE case added in GetVirtualMemoryPageSize()
 *
 * Revision 1.42  2005/05/03 18:08:21  lavr
 * Better implementation of Sleep*() on Linux; better CLK_TCK usage
 *
 * Revision 1.41  2005/03/01 17:40:23  ivanov
 * SleepMicroSec(): when converting sleeping time to milliseconds on OS which
 * doesn't support it round sleeping time instead of truncate.
 *
 * Revision 1.40  2005/02/23 13:45:37  ivanov
 * + SuppressSystemMessageBox() (Windows specific)
 *
 * Revision 1.39  2004/08/03 11:56:34  ivanov
 * + GetVirtualMemoryPageSize()
 *
 * Revision 1.38  2004/05/18 17:57:21  ucko
 * GetCpuCount(): return 1 rather than -1 (-> UINT_MAX !) if host_info
 * fails on Darwin.
 *
 * Revision 1.37  2004/05/14 13:59:26  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.36  2004/02/11 18:06:28  lavr
 * Fix typo (nrproc->nproc)
 *
 * Revision 1.35  2004/02/11 13:36:14  lavr
 * Added code for figuring out CPU count on *BSD/Darwin
 *
 * Revision 1.34  2003/11/10 17:13:52  vakatov
 * - #include <connect/ncbi_core.h>
 *
 * Revision 1.33  2003/09/25 16:54:07  ivanov
 * CPIDGuard class moved to ncbi_process.cpp.
 *
 * Revision 1.32  2003/09/23 21:13:53  ucko
 * +CPIDGuard::UpdatePID (code mostly factored out of constructor)
 *
 * Revision 1.31  2003/08/12 17:24:58  ucko
 * Add support for PID files.
 *
 * Revision 1.30  2003/05/19 21:07:51  vakatov
 * s_SignalHandler() -- get rid of "unused func arg" compilation warning
 *
 * Revision 1.29  2003/04/04 16:02:37  lavr
 * Lines wrapped at 79th column; some minor reformatting
 *
 * Revision 1.28  2003/04/03 14:15:48  rsmith
 * combine pp symbols NCBI_COMPILER_METROWERKS & _MSL_USING_MW_C_HEADERS
 * into NCBI_COMPILER_MW_MSL
 *
 * Revision 1.27  2003/04/02 16:22:33  rsmith
 * clean up metrowerks ifdefs.
 *
 * Revision 1.26  2003/04/02 13:29:29  rsmith
 * include ncbi_mslextras.h when compiling with MSL libs in Codewarrior.
 *
 * Revision 1.25  2003/03/06 23:46:50  ucko
 * Move extra NCBI_OS_DARWIN headers up with the rest, and surround them
 * with extern "C" because they seem to lack it.
 *
 * Revision 1.24  2003/03/06 15:46:46  ivanov
 * Added George Coulouris's GetCpuCount() implementation for NCBI_OS_DARWIN
 *
 * Revision 1.23  2002/09/19 20:05:42  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.22  2002/07/19 18:39:02  lebedev
 * NCBI_OS_MAC: SleepMicroSec implementation added
 *
 * Revision 1.21  2002/07/16 13:37:48  ivanov
 * Little modification and optimization of the Sleep* functions
 *
 * Revision 1.20  2002/07/15 21:43:04  ivanov
 * Added functions SleepMicroSec, SleepMilliSec, SleepSec
 *
 * Revision 1.19  2002/04/11 21:08:00  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.18  2002/04/01 18:52:49  ivanov
 * Cosmetic changes
 *
 * Revision 1.17  2002/03/25 18:11:08  ucko
 * Include <sys/time.h> before <sys/resource.h> for FreeBSD build.
 *
 * Revision 1.16  2001/12/09 06:27:39  vakatov
 * GetCpuCount() -- get rid of warning (in 64-bit mode), change ret.val. type
 *
 * Revision 1.15  2001/11/16 16:39:58  ivanov
 * Typo, fixed NCBI_OS_WIN -> NCBI_OS_MSWIN
 *
 * Revision 1.14  2001/11/16 16:34:34  ivanov
 * Added including "windows.h" under MS Windows
 *
 * Revision 1.13  2001/11/09 16:22:16  ivanov
 * Polish source code of GetCpuCount()
 *
 * Revision 1.12  2001/11/08 21:31:44  ivanov
 * Renamed GetCPUNumber() -> GetCpuCount()
 *
 * Revision 1.11  2001/11/08 21:10:04  ivanov
 * Added function GetCPUNumber()
 *
 * Revision 1.10  2001/09/10 17:15:51  grichenk
 * Added definition of CLK_TCK (absent on some systems).
 *
 * Revision 1.9  2001/07/24 13:19:14  ivanov
 * Remove semicolon after functions header (for NCBI_OS_WIN)
 *
 * Revision 1.8  2001/07/23 16:09:26  ivanov
 * Added possibility using user defined dump print handler (fix comment)
 *
 * Revision 1.7  2001/07/23 15:23:58  ivanov
 * Added possibility using user defined dump print handler
 *
 * Revision 1.6  2001/07/05 22:09:07  vakatov
 * s_ExitHandler() -- do not printout the final statistics if the process
 * has terminated "normally"
 *
 * Revision 1.5  2001/07/05 05:27:49  vakatov
 * Get rid of the redundant <ncbireg.hpp>.
 * Added workarounds for the capricious IRIX MIPSpro 7.3 compiler.
 *
 * Revision 1.4  2001/07/04 20:03:37  vakatov
 * Added missing header <unistd.h>.
 * Check for the exit code of the signal() function calls.
 *
 * Revision 1.3  2001/07/02 21:33:07  vakatov
 * Fixed against SIGXCPU during the signal handling.
 * Increase the amount of reserved memory for the memory limit handler
 * to 10K (to fix for the 64-bit WorkShop compiler).
 * Use standard C++ arg.processing (ncbiargs) in the test suite.
 * Cleaned up the code. Get rid of the "Ncbi_" prefix.
 *
 * Revision 1.2  2001/07/02 18:45:14  ivanov
 * Added #include <sys/resource.h> and extern "C" to handlers
 *
 * Revision 1.1  2001/07/02 16:45:35  ivanov
 * Initialization
 *
 * ===========================================================================
 */
