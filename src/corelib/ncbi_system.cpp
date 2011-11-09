/* $Id$
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

#ifdef NCBI_OS_UNIX
#  if defined(NCBI_OS_SOLARIS)
#    include <corelib/ncbifile.hpp>
#  endif //NCBI_OS_SOLARIS
#  include <sys/time.h>
#  include <sys/resource.h>
#  include <sys/times.h>
#  include <limits.h>
#  include <time.h>
#  include <unistd.h>
#  if defined(NCBI_OS_BSD) || defined(NCBI_OS_DARWIN)
#    include <sys/sysctl.h>
#  endif //NCBI_OS_BSD || NCBI_OS_DARWIN
#  if defined(NCBI_OS_IRIX)
#    include <sys/sysmp.h>
#  endif //NCBI_OS_IRIX
#  define USE_SETMEMLIMIT
#  define USE_SETCPULIMIT
#endif //NCBI_OS_UNIX

#ifdef NCBI_OS_DARWIN
extern "C" {
#  include <mach/mach.h>
#  include <mach/mach_host.h>
#  include <mach/host_info.h>
} /* extern "C" */
#endif //NCBI_OS_DARWIN

#ifdef USE_SETCPULIMIT
#  include <signal.h>
#endif //USE_SETCPULIMIT

#ifdef NCBI_OS_MSWIN
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

#endif //NCBI_OS_MSWIN

#ifdef NCBI_OS_CYGWIN
#  define WINVER 0x0500
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif //NCBI_OS_CYGWIN


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
#endif //NCBI_COMPILER_MIPSPRO


#ifdef NCBI_OS_UNIX

DEFINE_STATIC_FAST_MUTEX(s_ExitHandler_Mutex);
static bool                  s_ExitHandlerIsSet  = false;
static ELimitsExitCode       s_ExitCode          = eLEC_None;
static CSafeStaticPtr<CTime> s_TimeSet;
static size_t                s_MemoryLimit       = 0;
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
            limit_size = s_MemoryLimit;
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
                          "by operator new (" << s_MemoryLimit << " bytes)");
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
#endif //CLK_TCK
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
        s_ReserveMemory = new char[10*1024];
    }
    return true;
}
    
#endif //NCBI_OS_UNIX



/////////////////////////////////////////////////////////////////////////////
//
// SetHeapLimit
//

#ifdef USE_SETMEMLIMIT

// Handler for operator new
static void s_NewHandler(void)
{
    s_ExitCode = eLEC_Memory;
    exit(-1);
}


bool SetMemoryLimit(size_t max_size,
                    TLimitsPrintHandler handler, 
                    TLimitsPrintParameter parameter)
{
    if (s_MemoryLimit == max_size) 
        return true;
    
    if ( !s_SetExitHandler(handler, parameter) )
        return false;

    // Set new heap limit
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);
    
    rlimit rl;
    if ( max_size ) {
        set_new_handler(s_NewHandler);
        rl.rlim_cur = rl.rlim_max = max_size;
    }
    else {
        // Set off heap limit
        set_new_handler(0);
        rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    }
    if (setrlimit(RLIMIT_DATA, &rl) != 0) 
        return false;
#  if !defined(NCBI_OS_SOLARIS)
    if (setrlimit(RLIMIT_AS, &rl) != 0) 
        return false;
#  endif //NCBI_OS_SOLARIS

    s_MemoryLimit = max_size;
    return true;
}


// deprecated
bool SetHeapLimit(size_t max_size,
                  TLimitsPrintHandler handler, 
                  TLimitsPrintParameter parameter)
{
    if (s_MemoryLimit == max_size) 
        return true;
    
    if ( !s_SetExitHandler(handler, parameter) )
        return false;

    // Set new heap limit
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);
    
    rlimit rl;
    if ( max_size ) {
        set_new_handler(s_NewHandler);
        rl.rlim_cur = rl.rlim_max = max_size;
    }
    else {
        // Set off heap limit
        set_new_handler(0);
        rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    }
    if (setrlimit(RLIMIT_DATA, &rl) != 0) 
        return false;

    s_MemoryLimit = max_size;
    return true;
}


#else

bool SetMemoryLimit(size_t max_size, 
                    TLimitsPrintHandler handler, 
                    TLimitsPrintParameter parameter)
{
  return false;
}

bool SetHeapLimit(size_t max_size, 
                  TLimitsPrintHandler handler, 
                  TLimitsPrintParameter parameter)
{
  return false;
}

#endif //USE_SETMEMLIMIT



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
    // if (s_ExitHandlerIsSet) {
    //     s_ExitHandler();
    // }
    // NB: _exit() does not go over atexit() chain
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

#endif //USE_SETCPULIMIT


/////////////////////////////////////////////////////////////////////////////
//
// GetCpuCount
//

unsigned int GetCpuCount(void)
{
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_CYGWIN)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (unsigned int) si.dwNumberOfProcessors;

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
# endif //UNIX_FLAVOR
    return nproc <= 0 ? 1 : (unsigned int) nproc;
#else
    return 1;
#endif //NCBI_OS_...
}



/////////////////////////////////////////////////////////////////////////////
//
// Memory utilities
//

unsigned long GetVirtualMemoryPageSize(void)
{
    static unsigned long ps = 0;

    if (!ps) {
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_CYGWIN)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        ps = (unsigned long) si.dwPageSize;
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
#endif //NCBI_OS_...
    }
    return ps;
}


unsigned long GetVirtualMemoryAllocationGranularity(void)
{
    static unsigned long ag = 0;

    if (!ag) {
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_CYGWIN)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        ag = (unsigned long) si.dwAllocationGranularity;
#else
        ag = GetVirtualMemoryPageSize();
#endif //NCBI_OS_MSWIN || NCBI_OS_CYGWIN
    }
    return ag;
}


Uint8 GetPhysicalMemorySize(void)
{
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_CYGWIN)

    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if ( GlobalMemoryStatusEx(&st) ) {
        return st.ullTotalPhys;
    }

#elif defined(NCBI_OS_UNIX)  &&  defined(_SC_PHYS_PAGES)

    unsigned long num_pages = sysconf(_SC_PHYS_PAGES);
    if (long(num_pages) != -1L) {
        return GetVirtualMemoryPageSize() * Uint8(num_pages);
    }

#elif defined(NCBI_OS_BSD)  ||  defined(NCBI_OS_DARWIN)

    size_t   len;
    int      mib[2];
#  ifdef HW_MEMSIZE
    uint64_t physmem;
    mib[1] = HW_MEMSIZE;
#  else
    /* Native BSD, may be truncated */
    int      physmem;
    mib[1] = HW_PHYSMEM;
#  endif /*HW_MEMSIZE*/
    mib[0] = CTL_HW;
    len = sizeof(physmem);
    if (sysctl(mib, 2, &physmem, &len, 0, 0) == 0  &&  len == sizeof(physmem)){
        return Uint8(physmem);
    }

#  ifdef NCBI_OS_DARWIN
    {
        /* heavier fallback */
        struct vm_statistics vm_stat;
        mach_port_t my_host = mach_host_self();
        mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
        if (host_statistics(my_host, HOST_VM_INFO,
                            (integer_t*) &vm_stat, &count) == KERN_SUCCESS) {
            return GetVirtualMemoryPageSize() *
                (Uint8(vm_stat.free_count) + Uint8(vm_stat.active_count) +
                 Uint8(vm_stat.inactive_count) + Uint8(vm_stat.wire_count));
        }
    }
#  endif //NCBI_OS_DARWIN

#elif defined(NCBI_OS_IRIX)

    struct rminfo rmi;
    if (sysmp(MP_SAGET, MPSA_RMINFO, &rmi, sizeof(rmi)) >= 0) {
        return GetVirtualMemoryPageSize() * Uint8(rmi.physmem);
    }

#endif

    return 0;
}


bool GetMemoryUsage(size_t* total, size_t* resident, size_t* shared)
{
    size_t scratch;
    if ( !total )    { total    = &scratch; }
    if ( !resident ) { resident = &scratch; }
    if ( !shared )   { shared   = &scratch; }
#if defined(NCBI_OS_MSWIN)
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
            *total    = counters.quota_paged_pool_usage +
                        counters.quota_nonpaged_pool_usage;
            *resident = counters.working_set_size;
            *shared   = 0;
            return true;
        }
    } catch (CException) {
        // Just catch all exceptions from CDll
    }
#elif defined(NCBI_OS_LINUX)
    CNcbiIfstream statm("/proc/self/statm");
    if (statm) {
        unsigned long page_size = GetVirtualMemoryPageSize();
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
#  define _DIV0(a, b) ((a) / ((b) ? (b) : 1))
    // BIG FAT NOTE:  getrusage() seems to use different size units
    struct rusage ru;
    memset(&ru, '\0', sizeof(ru));
    if (getrusage(RUSAGE_SELF, &ru) == 0  &&  ru.ru_maxrss > 0) {
        struct tms t;
        memset(&t, '\0', sizeof(t));
        if (times(&t) != (clock_t)(-1)) {
            clock_t ticks = t.tms_utime + t.tms_stime;
            *total    = _DIV0(ru.ru_ixrss + ru.ru_idrss + ru.ru_isrss, ticks);
            *resident = _DIV0(ru.ru_idrss,                             ticks);
            *shared   = _DIV0(ru.ru_ixrss,                             ticks);
            return true;
        }
    }
#  undef _DIV0
#endif
    return false;
}


/////////////////////////////////////////////////////////////////////////////
//
// Sleep
//

void SleepMicroSec(unsigned long mc_sec, EInterruptOnSignal onsignal)
{
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_CYGWIN)

    // Unlike some of its (buggy) Unix counterparts, MS-Win's Sleep() is safe
    // to use with 0, which causes the current thread to sleep at most until
    // the end of the current timeslice (and only if the CPU is not idle).
    Sleep((mc_sec + 500) / 1000);

#elif defined(NCBI_OS_UNIX)

#  if defined(HAVE_NANOSLEEP)
    struct timespec delay, unslept;
    memset(&unslept, 0, sizeof(unslept));
    delay.tv_sec  =  mc_sec / kMicroSecondsPerSecond;
    delay.tv_nsec = (mc_sec % kMicroSecondsPerSecond) * 1000;
    while (nanosleep(&delay, &unslept) < 0) {
        if (errno != EINTR  ||  onsignal == eInterruptOnSignal)
            break;
        delay = unslept;
        memset(&unslept, 0, sizeof(unslept));
    }
#  elif defined(HAVE_USLEEP)
    unsigned int sec  = mc_sec / kMicroSecondsPerSecond;
    unsigned int usec = mc_sec % kMicroSecondsPerSecond;
    if (sec) {
        while ((sec = sleep(sec)) > 0) {
            if (onsignal == eInterruptOnSignal)
                return;
        }
    }
    usleep(usec);
#  else
    // Portable but ugly.
    // Most implementations of select() do not modify timeout to reflect
    // the amount of time unslept;  but some (e.g. Linux) do.  Also, on
    // some platforms it can be interrupted by a signal, but not on others.
    // OTOH, we don't want to sandwich this with gettimeofday(), either.
    struct timeval delay;
    delay.tv_sec  = mc_sec / kMicroSecondsPerSecond;
    delay.tv_usec = mc_sec % kMicroSecondsPerSecond;
    while (select(0, (fd_set*) 0, (fd_set*) 0, (fd_set*) 0, &delay) < 0) {
#    if defined(SELECT_UPDATES_TIMEOUT)
        if (errno != EINTR  ||  onsignal == eInterruptOnSignal)
#    endif
            break;
    }
#  endif //HAVE FINE SLEEP API

#endif //NCBI_OS_...
}


void SleepMilliSec(unsigned long ml_sec, EInterruptOnSignal onsignal)
{
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_CYGWIN)
    Sleep(ml_sec);
#elif defined(NCBI_OS_UNIX)
    SleepMicroSec(ml_sec * 1000, onsignal);
#endif //NCBI_OS_...
}


void SleepSec(unsigned long sec, EInterruptOnSignal onsignal)
{
    SleepMicroSec(sec * kMicroSecondsPerSecond, onsignal);
}



/////////////////////////////////////////////////////////////////////////////
///
/// Suppress Diagnostic Popup Messages (all MS-Win specific)
///

#ifdef NCBI_OS_MSWIN

static bool s_EnableSuppressSystemMessageBox = true;
static bool s_DoneSuppressSystemMessageBox   = false;
static bool s_SuppressedDebugSystemMessageBox = false;

// Handler for "Unhandled" exceptions
static LONG CALLBACK _SEH_Handler(EXCEPTION_POINTERS* ep)
{
    // Always terminate a program
    return EXCEPTION_EXECUTE_HANDLER;
}

#endif //NCBI_OS_MSWIN

extern void SuppressSystemMessageBox(TSuppressSystemMessageBox mode)
{
#ifdef NCBI_OS_MSWIN
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
    if ( !IsDebuggerPresent() ) {
        if ( (mode & fSuppress_Debug) == fSuppress_Debug ) {
            _CrtSetReportFile(_CRT_WARN,   _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
            s_SuppressedDebugSystemMessageBox = true;
        }
    }
    // Exceptions
    if ( (mode & fSuppress_Exception) == fSuppress_Exception ) {
        SetUnhandledExceptionFilter(_SEH_Handler);
    }
    s_DoneSuppressSystemMessageBox = true;
#endif //NCBI_OS_MSWIN
}


extern void DisableSuppressSystemMessageBox(void)
{
#ifdef NCBI_OS_MSWIN
    if ( s_DoneSuppressSystemMessageBox ) {
        ERR_POST_X(9, Critical << "SuppressSystemMessageBox() already called");
    }
    s_EnableSuppressSystemMessageBox = false;
#endif //NCBI_OS_MSWIN
}


extern bool IsSuppressedDebugSystemMessageBox(void)
{
#ifdef NCBI_OS_MSWIN
    return s_DoneSuppressSystemMessageBox  &&
        s_SuppressedDebugSystemMessageBox;
#else
    return false;
#endif
}


END_NCBI_SCOPE
