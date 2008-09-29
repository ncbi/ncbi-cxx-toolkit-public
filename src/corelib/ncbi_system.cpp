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
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Corelib_System


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
static CSafeStaticPtr<CTime> s_TimeSet;
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
        (*s_PrintHandler)(s_ExitCode, limit_size, s_TimeSet.Get(), 
                          s_PrintHandlerParam);
        return;
    }

    // Standard dump
    switch ( s_ExitCode ) {
        
    case eLEC_Memory:
        {
            ERR_POST_X(1, "Memory heap limit exceeded in allocating memory " \
                          "by operator new (" << s_HeapLimit << " bytes)");
            break;
        }
        
    case eLEC_Cpu: 
        {
            ERR_POST_X(2, "CPU time limit exceeded (" << s_CpuTimeLimit << " sec)");
            tms buffer;
            if (times(&buffer) == (clock_t)(-1)) {
                ERR_POST_X(3, "Error in getting CPU time consumed by program");
                break;
            }
            clock_t tick = sysconf(_SC_CLK_TCK);
#ifdef CLK_TCK
            if (!tick  ||  tick == (clock_t)(-1))
                tick = CLK_TCK;
#endif /*CLK_TCK*/
            if (tick == (clock_t)(-1))
                tick = 0;
            LOG_POST_X(4, "\tuser CPU time   : " << 
                          buffer.tms_utime/(tick ? tick : 1) <<
                          (tick ? " sec" : " tick"));
            LOG_POST_X(5, "\tsystem CPU time : " << 
                          buffer.tms_stime/(tick ? tick : 1) <<
                          (tick ? " sec" : " tick"));
            LOG_POST_X(6, "\ttotal CPU time  : " <<
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
    et.AddSecond((int) (ct.GetTimeT() - s_TimeSet->GetTimeT()));
    LOG_POST_X(7, "Program's time: " << Endm <<
                  "\tstart limit - " << s_TimeSet->AsString() << Endm <<
                  "\ttermination - " << ct.AsString() << Endm);
    et.SetFormat("h:m:s");
    LOG_POST_X(8, "\texecution   - " << et.AsString());
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
        s_TimeSet->SetCurrent();

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

unsigned long GetPhysicalMemorySize(void)
{
    unsigned long num_pages = 0, page_size = 0;

#if defined(NCBI_OS_MSWIN)
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if ( !GlobalMemoryStatusEx(&st) ) {
        return 0;
    }
    return st.ullTotalPhys;
#elif defined (NCBI_OS_DARWIN)
    page_size = GetVirtualMemoryPageSize();
    struct vm_statistics vm_stat;
    mach_port_t my_host = mach_host_self();
    mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
    if (host_statistics(my_host, HOST_VM_INFO, (integer_t*)&vm_stat, &count) !=
        KERN_SUCCESS) {
        return 0;
    }
    return (vm_stat.free_count + vm_stat.active_count + vm_stat.inactive_count +
        vm_stat.wire_count) * page_size;
#elif defined(_SC_PHYS_PAGES)
    page_size = GetVirtualMemoryPageSize();
    if ( ((long)(num_pages = sysconf(_SC_PHYS_PAGES))) == -1L) {
        num_pages = 0;
    }
#endif
    return num_pages * page_size;
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
        *total    = (size_t)len;
        *resident = (size_t)len; // conservative estimate
        *shared   = 0;           // does this info exist anywhere?
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
static bool s_DoneSuppressSystemMessageBox   = false;

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
    s_DoneSuppressSystemMessageBox = true;
#else
    // not implemented
#endif
}


extern void DisableSuppressSystemMessageBox()
{
#if defined(NCBI_OS_MSWIN)
    if ( s_DoneSuppressSystemMessageBox ) {
        ERR_POST_X(9, Critical << "SuppressSystemMessageBox() was already called");
    }
    s_EnableSuppressSystemMessageBox = false;
#else
    // not implemented
#endif
}


END_NCBI_SCOPE
