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
#include <corelib/ncbierror.hpp>
#include "ncbisys.hpp"
#include <array>

#define NCBI_USE_ERRCODE_X   Corelib_System

#if defined(NCBI_OS_UNIX)
#  include <sys/mman.h>
#  if defined(NCBI_OS_SOLARIS)
#    include <corelib/ncbifile.hpp>
#    ifndef VALID_ATTR // correlated with madvise() prototype
extern "C" {
    extern int madvise(caddr_t, size_t, int);
}
#    endif
#  endif //NCBI_OS_SOLARIS
#  include <sys/time.h>
#  include <sys/resource.h>
#  include <sys/times.h>
#  include <sys/types.h>
#  include <dirent.h>
#  include <limits.h>
#  include <time.h>
#  include <unistd.h>
#  include <fcntl.h>
#  if defined(NCBI_OS_BSD)  ||  defined(NCBI_OS_DARWIN)
#    include <sys/sysctl.h>
#  endif //NCBI_OS_BSD || NCBI_OS_DARWIN
#  if defined(NCBI_OS_IRIX)
#    include <sys/sysmp.h>
#  endif //NCBI_OS_IRIX
#  include "ncbi_os_unix_p.hpp"
#  define USE_SETMEMLIMIT
#  define USE_SETCPULIMIT
#  define HAVE_MADVISE 1
#endif //NCBI_OS_UNIX

#if defined(NCBI_OS_LINUX)
#  include <sched.h>
#endif

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
#  include "ncbi_os_mswin_p.hpp"
#  include <dbghelp.h>
#  include <intrin.h>
#endif //NCBI_OS_MSWIN


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
static CSafeStatic<CTime>    s_TimeSet;
static size_t                s_MemoryLimitSoft   = 0;
static size_t                s_MemoryLimitHard   = 0;
static size_t                s_CpuTimeLimit      = 0;
static char*                 s_ReserveMemory     = 0;
static TLimitsPrintHandler   s_PrintHandler      = 0;
static TLimitsPrintParameter s_PrintHandlerParam = 0;


#if !defined(CLK_TCK)  &&  defined(CLOCKS_PER_SEC)
#  define CLK_TCK CLOCKS_PER_SEC
#endif


// Routine to be called at the exit from application
// It is not async-safe, so using it with SetCpuLimits() can lead to coredump
// and program crash. Be aware.
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
            limit_size = s_MemoryLimitSoft;
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
                          "by operator new (" << s_MemoryLimitSoft << " bytes)");
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
#if defined(CLK_TCK)
            if (!tick  ||  tick == (clock_t)(-1))
                tick = CLK_TCK;
#elif defined(CLOCKS_PER_SEC)
            if (!tick  ||  tick == (clock_t)(-1))
                tick = CLOCKS_PER_SEC;
#endif
            if (tick == (clock_t)(-1))
                tick = 0;
            ERR_POST_X(4, Note << "\tuser CPU time   : " << 
                          buffer.tms_utime/(tick ? tick : 1) <<
                          (tick ? " sec" : " tick"));
            ERR_POST_X(5, Note << "\tsystem CPU time : " << 
                          buffer.tms_stime/(tick ? tick : 1) <<
                          (tick ? " sec" : " tick"));
            ERR_POST_X(6, Note << "\ttotal CPU time  : " <<
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
    ERR_POST_X(7, Note << "Program's time: " << Endm <<
                  "\tstart limit - " << s_TimeSet->AsString() << Endm <<
                  "\ttermination - " << ct.AsString() << Endm);
    et.SetFormat("h:m:s");
    ERR_POST_X(8, Note << "\texecution   - " << et.AsString());
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
        
        // Store new print handler and its parameter
        s_PrintHandler = handler;
        s_PrintHandlerParam = parameter;
        
        // Reserve some memory (10Kb) to allow the diagnostic API
        // print messages on exit if memory limit is set.
        s_ReserveMemory = new char[10*1024];
    }
    return true;
}
    
#endif //NCBI_OS_UNIX



/////////////////////////////////////////////////////////////////////////////
//
// Memory limits
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
    if (s_MemoryLimitSoft == max_size) {
        return true;
    }
    if (!s_SetExitHandler(handler, parameter)) {
        return false;
    }
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);
    
    rlimit rl;
    if ( max_size ) {
        set_new_handler(s_NewHandler);
        rl.rlim_cur = rl.rlim_max = max_size;
    } else {
        set_new_handler(0);
        rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    }
    if (setrlimit(RLIMIT_DATA, &rl) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
#  if !defined(NCBI_OS_SOLARIS)
    if (setrlimit(RLIMIT_AS, &rl) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
#  endif //NCBI_OS_SOLARIS

    s_MemoryLimitSoft = max_size;
    s_MemoryLimitHard = max_size;
    if ( max_size ) {
        set_new_handler(s_NewHandler);
    } else {
        set_new_handler(0);
    }
    return true;
}


bool SetMemoryLimitSoft(size_t max_size,
                        TLimitsPrintHandler handler, 
                        TLimitsPrintParameter parameter)
{
    if (s_MemoryLimitSoft == max_size) {
        return true;
    }
    if (!s_SetExitHandler(handler, parameter)) {
        return false;
    }
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);

    rlimit rl;
    if (getrlimit(RLIMIT_DATA, &rl) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
    if ( max_size ) {
        rl.rlim_cur = max_size;
    } else {
        rl.rlim_cur = RLIM_INFINITY;
    }
    if (setrlimit(RLIMIT_DATA, &rl) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
#  if !defined(NCBI_OS_SOLARIS)
    rlimit rlas;
    if (getrlimit(RLIMIT_AS, &rlas) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
    rl.rlim_max = rlas.rlim_max;
    if (setrlimit(RLIMIT_AS, &rl) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
#  endif //NCBI_OS_SOLARIS

    s_MemoryLimitSoft = max_size;
    if ( max_size ) {
        set_new_handler(s_NewHandler);
    } else {
        set_new_handler(0);
    }
    return true;
}


bool SetMemoryLimitHard(size_t max_size,
                        TLimitsPrintHandler handler, 
                        TLimitsPrintParameter parameter)
{
    if (s_MemoryLimitHard == max_size) {
        return true;
    }
    if (!s_SetExitHandler(handler, parameter)) {
        return false;
    }
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);

    size_t cur_soft_limit = 0;
    rlimit rl;
    if (getrlimit(RLIMIT_DATA, &rl) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
    if ( max_size ) {
        rl.rlim_max = max_size;
        if (rl.rlim_cur > max_size) {
            rl.rlim_cur = max_size;
        }
        cur_soft_limit = rl.rlim_cur;
    } else {
        rl.rlim_max = RLIM_INFINITY;
    }
    if (setrlimit(RLIMIT_DATA, &rl) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
#  if !defined(NCBI_OS_SOLARIS)
    rlimit rlas;
    if (getrlimit(RLIMIT_AS, &rlas) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
    if ( max_size ) {
        rlas.rlim_max = max_size;
        // Decrease current soft limit of the virtual memory
        // to the size of data segment -- min(DATA,AS). 
        if (rlas.rlim_cur > cur_soft_limit) {
            rlas.rlim_cur = cur_soft_limit;
        }
        // And use this size as current value for the soft limit
        // in the print handler 
        cur_soft_limit = rlas.rlim_cur;
    } else {
        rlas.rlim_max = RLIM_INFINITY;
    }
    if (setrlimit(RLIMIT_AS, &rlas) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
#  endif //NCBI_OS_SOLARIS

    s_MemoryLimitSoft = cur_soft_limit;
    s_MemoryLimitHard = max_size;
    if ( max_size ) {
        set_new_handler(s_NewHandler);
    } else {
        set_new_handler(0);
    }
    return true;
}


// @deprecated
bool SetHeapLimit(size_t max_size,
                  TLimitsPrintHandler handler, 
                  TLimitsPrintParameter parameter)
{
    if (s_MemoryLimitSoft == max_size) { 
        return true;
    }
    if (!s_SetExitHandler(handler, parameter)) {
        return false;
    }
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);
    
    rlimit rl;
    if ( max_size ) {
        rl.rlim_cur = rl.rlim_max = max_size;
    } else {
        rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    }
    if (setrlimit(RLIMIT_DATA, &rl) != 0) {
        CNcbiError::SetFromErrno();
        return false;
    }
    s_MemoryLimitSoft = max_size;
    if ( max_size ) {
        set_new_handler(s_NewHandler);
    } else {
        set_new_handler(0);
    }
    return true;
}


size_t GetVirtualMemoryLimitSoft(void)
{
    // Query limits from kernel, s_MemoryLimit* values can not reflect real limits.
    rlimit rl = {0,0};
#  if !defined(NCBI_OS_SOLARIS)
    if (getrlimit(RLIMIT_AS, &rl) != 0) {
        CNcbiError::SetFromErrno();
        return 0;
    }
    if (rl.rlim_cur == RLIM_INFINITY) {
        return 0;
    }
#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif
    return rl.rlim_cur;
}


size_t GetVirtualMemoryLimitHard(void)
{
    // Query limits from kernel, s_MemoryLimit* values can not reflect real limits.
    rlimit rl = {0,0};
#  if !defined(NCBI_OS_SOLARIS)
    if (getrlimit(RLIMIT_AS, &rl) != 0) {
        CNcbiError::SetFromErrno();
        return 0;
    }
    if (rl.rlim_max == RLIM_INFINITY) {
        return 0;
    }
#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif
    return rl.rlim_max;
}


#else

bool SetMemoryLimit(size_t max_size, 
                    TLimitsPrintHandler handler, 
                    TLimitsPrintParameter parameter)
{
    CNcbiError::Set(CNcbiError::eNotSupported);
    return false;
}

bool SetMemoryLimitSoft(size_t max_size, 
                    TLimitsPrintHandler handler, 
                    TLimitsPrintParameter parameter)
{
    CNcbiError::Set(CNcbiError::eNotSupported);
    return false;
}

bool SetMemoryLimitHard(size_t max_size, 
                    TLimitsPrintHandler handler, 
                    TLimitsPrintParameter parameter)
{
    CNcbiError::Set(CNcbiError::eNotSupported);
    return false;
}

bool SetHeapLimit(size_t max_size, 
                  TLimitsPrintHandler handler, 
                  TLimitsPrintParameter parameter)
{
    CNcbiError::Set(CNcbiError::eNotSupported);
    return false;
}

size_t GetVirtualMemoryLimitSoft(void)
{
    CNcbiError::Set(CNcbiError::eNotSupported);
    return 0;
}

size_t GetVirtualMemoryLimitHard(void)
{
    CNcbiError::Set(CNcbiError::eNotSupported);
    return 0;
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
    // _exit() does not go over atexit() chain, so just call registered
    // handler directly. Be aware that it should be async-safe!
    if (s_ExitHandlerIsSet) {
        s_ExitHandler();
    }
    _exit(-1);
}

bool SetCpuTimeLimit(unsigned int          max_cpu_time,
                     unsigned int          terminate_delay_time,
                     TLimitsPrintHandler   handler, 
                     TLimitsPrintParameter parameter)
{
    if (s_CpuTimeLimit == max_cpu_time) {
        return true;
    }
    if (!s_SetExitHandler(handler, parameter)) {
        return false;
    }
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);

    rlimit rl;
    if ( max_cpu_time ) {
        rl.rlim_cur = max_cpu_time;
        rl.rlim_max = max_cpu_time + terminate_delay_time;
    } else {
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

bool SetCpuTimeLimit(unsigned int          max_cpu_time,
                     unsigned int          terminate_delay_time,
                     TLimitsPrintHandler   handler, 
                     TLimitsPrintParameter parameter)
{
    return false;
}

#endif //USE_SETCPULIMIT


// @deprecated
bool SetCpuTimeLimit(size_t                max_cpu_time,
                     TLimitsPrintHandler   handler, 
                     TLimitsPrintParameter parameter,
                     size_t                terminate_delay_time)
{
    return SetCpuTimeLimit((unsigned int)max_cpu_time,
                           (unsigned int)terminate_delay_time,
                           handler, parameter);
}



/////////////////////////////////////////////////////////////////////////////
//
// System information
//


string CSystemInfo::GetUserName(void)
{
#if defined(NCBI_OS_UNIX)
    return CUnixFeature::GetUserNameByUID(geteuid());
#elif defined(NCBI_OS_MSWIN)
    return CWinSecurity::GetUserName();
#else
    return string();
#endif
}


unsigned int CSystemInfo::GetCpuCount(void)
{
    static unsigned int cpu = 0;
    if (!cpu) {
#if defined(NCBI_OS_MSWIN)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        cpu = (unsigned int)si.dwNumberOfProcessors;

#elif defined(NCBI_OS_DARWIN)
        host_basic_info_data_t hinfo;
        mach_msg_type_number_t hinfo_count = HOST_BASIC_INFO_COUNT;
        kern_return_t rc;
        rc = host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)&hinfo, &hinfo_count);
        if (rc == KERN_SUCCESS) {
            cpu = hinfo.avail_cpus;
        }

#elif defined(NCBI_OS_UNIX)
        long x = 0;
        #if defined(_SC_NPROC_ONLN)
            x = sysconf(_SC_NPROC_ONLN);
        #elif defined(_SC_NPROCESSORS_ONLN)
            x = sysconf(_SC_NPROCESSORS_ONLN);
        #elif defined(NCBI_OS_BSD) /*|| defined(NCBI_OS_DAWRIN)*/
            int v;
            size_t len = sizeof(v);
            int mib[2] = { CTL_HW, HW_NCPU };
            if (sysctl(mib, 2, &v, &len, NULL, 0) == 0) {
                x = v;
            }
        #endif // UNIX_FLAVOR
        cpu = (x <= 0 ? 1 : (unsigned int)x);
#endif //NCBI_OS_...

        // default assumption
        if (!cpu) {
            cpu = 1;
        }
    }
    return cpu;
}


unsigned int CSystemInfo::GetCpuCountAllowed(void)
{

#if defined(NCBI_OS_MSWIN)

    DWORD_PTR proc_mask = 0, sys_mask = 0;
    if (!::GetProcessAffinityMask(::GetCurrentProcess(), &proc_mask, &sys_mask)) {
        return 0;
    }
    unsigned int n = 0;  // number of bits set in proc_mask
    for (; proc_mask; proc_mask >>= 1) {
        n += proc_mask & 1;
    }
    return n;

#elif defined(NCBI_OS_LINUX)
  
    unsigned int total_cpus = CSystemInfo::GetCpuCount();
    if (total_cpus == 1) {
        // GetCpuCount() returns 1 if unable to get real number
        return 1;
    }
    // Standard type cpu_set_t can be limited if used directly,
    // so use dynamic allocation approach
    cpu_set_t* cpuset_ptr = CPU_ALLOC(total_cpus);
    if (cpuset_ptr == NULL) {
        return 0;
    }
    size_t cpuset_size = CPU_ALLOC_SIZE(total_cpus);
    CPU_ZERO_S(cpuset_size, cpuset_ptr);
   
    if (sched_getaffinity(getpid(), cpuset_size, cpuset_ptr) != 0) {
        CPU_FREE(cpuset_ptr);
        return 0;
    }
    int n = CPU_COUNT_S(cpuset_size, cpuset_ptr);
    CPU_FREE(cpuset_ptr);
    return (n < 0) ? 0 : static_cast<unsigned int>(n);

#endif //NCBI_OS_...

    // TODO: add support for other UNIX versions where possible

    return 0;
}


double CSystemInfo::GetUptime(void)
{
#if defined(NCBI_OS_MSWIN)
    return (double)GetTickCount64() / kMilliSecondsPerSecond;

#elif defined(NCBI_OS_UNIX)  &&  defined(CLOCK_UPTIME)
    struct timespec ts;
    if (clock_gettime(CLOCK_UPTIME, &ts) == 0) {
        return ts.tv_sec + (double)ts.tv_nsec/kNanoSecondsPerSecond;
   }

#elif defined(NCBI_OS_LINUX)
    // "/proc/uptime" provide more accurate information than
    // sysinfo(), that is rounded to seconds
    CNcbiIfstream is("/proc/uptime");
    if (is) {
        double ut;
        is >> ut;
        return ut;
    }

#elif defined(NCBI_OS_BSD)  ||  defined(NCBI_OS_DAWRIN)
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    if (sysctl(mib, 2, &boottime, &len, NULL, 0) < 0) {
        return -1.0;
    }
    return  time(NULL) - boottime.tv_sec + (double)boottime.tv_usec/kMicroSecondsPerSecond;

#endif
    CNcbiError::Set(CNcbiError::eNotSupported);
    return -1.0;
}


unsigned long CSystemInfo::GetVirtualMemoryPageSize(void)
{
    static unsigned long ps = 0;
    if (ps) {
        return ps;
    }

#if defined(NCBI_OS_MSWIN)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    ps = (unsigned long) si.dwPageSize;

#elif defined(NCBI_OS_UNIX) 
    long x = 0;
    #if defined(HAVE_GETPAGESIZE)
        x = getpagesize();
    #elif defined(_SC_PAGESIZE)
        x = sysconf(_SC_PAGESIZE);
    #elif defined(_SC_PAGE_SIZE)
        x = sysconf(_SC_PAGE_SIZE);
    #endif
    if (x <= 0) {
        CNcbiError::SetFromErrno();
        return 0;
    }
    ps = x;
#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif //NCBI_OS_...

    return ps;
}


unsigned long CSystemInfo::GetVirtualMemoryAllocationGranularity(void)
{
    static unsigned long ag = 0;
    if (!ag) {
#if defined(NCBI_OS_MSWIN)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        ag = (unsigned long) si.dwAllocationGranularity;
#else
        ag = GetVirtualMemoryPageSize();
#endif
    }
    return ag;
}


Uint8 CSystemInfo::GetTotalPhysicalMemorySize(void)
{
    static Uint8 ms = 0;
    if (ms) {
        return ms;
    }

#if defined(NCBI_OS_MSWIN)
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if ( ::GlobalMemoryStatusEx(&st) ) {
        ms = st.ullTotalPhys;
    }

#elif defined(NCBI_OS_UNIX)  &&  defined(_SC_PHYS_PAGES)
    unsigned long num_pages = sysconf(_SC_PHYS_PAGES);
    if (long(num_pages) != -1L) {
        ms = GetVirtualMemoryPageSize() * Uint8(num_pages);
    }

#elif defined(NCBI_OS_BSD)  ||  defined(NCBI_OS_DARWIN)
    int mib[2];
    mib[0] = CTL_HW;
    #ifdef HW_MEMSIZE
        uint64_t physmem;
        mib[1] = HW_MEMSIZE;
    #else
        // Native BSD, may be truncated
        int physmem;
        mib[1] = HW_PHYSMEM;
    #endif
    size_t len = sizeof(physmem);
    if (sysctl(mib, 2, &physmem, &len, NULL, 0) == 0  &&  len == sizeof(physmem)){
        ms = Uint8(physmem);
    }
    #if defined(NCBI_OS_DARWIN)
    {
        // heavier fallback
        struct vm_statistics vm_stat;
        mach_port_t my_host = mach_host_self();
        mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
        if (host_statistics(my_host, HOST_VM_INFO,
                            (integer_t*) &vm_stat, &count) == KERN_SUCCESS) {
            ms = GetVirtualMemoryPageSize() *
                   ( Uint8(vm_stat.free_count) + 
                     Uint8(vm_stat.active_count) +
                     Uint8(vm_stat.inactive_count) + 
                     Uint8(vm_stat.wire_count) );
        }
    }
    #endif //NCBI_OS_DARWIN

#elif defined(NCBI_OS_IRIX)
    struct rminfo rmi;
    if (sysmp(MP_SAGET, MPSA_RMINFO, &rmi, sizeof(rmi)) >= 0) {
        ms = GetVirtualMemoryPageSize() * Uint8(rmi.physmem);
    }

#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif
    return ms;
}


Uint8 CSystemInfo::GetAvailPhysicalMemorySize(void)
{
#if defined(NCBI_OS_MSWIN)
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if ( ::GlobalMemoryStatusEx(&st) ) {
        return st.ullAvailPhys;
    }

#elif defined(NCBI_OS_UNIX)  &&  defined(_SC_AVPHYS_PAGES)
    unsigned long num_pages = sysconf(_SC_AVPHYS_PAGES);
    if (long(num_pages) != -1L) {
        return GetVirtualMemoryPageSize() * Uint8(num_pages);
    }

#elif (defined(NCBI_OS_BSD) || defined(NCBI_OS_DARWIN))  && defined(HW_USERMEM)
    int mib[2] = { CTL_HW, HW_USERMEM };
    uint64_t mem;
    size_t len = sizeof(mem);
    if (sysctl(mib, 2, &mem, &len, NULL, 0) == 0  &&  len == sizeof(mem)){
        return Uint8(mem);
    }
    #if defined(NCBI_OS_DARWIN)
    {
        // heavier fallback
        struct vm_statistics vm_stat;
        mach_port_t my_host = mach_host_self();
        mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
        if (host_statistics(my_host, HOST_VM_INFO,
                            (integer_t*) &vm_stat, &count) == KERN_SUCCESS) {
            return GetVirtualMemoryPageSize() * Uint8(vm_stat.free_count);
        }
    }
    #endif //NCBI_OS_DARWIN

#elif defined(NCBI_OS_IRIX)
    struct rminfo rmi;
    if (sysmp(MP_SAGET, MPSA_RMINFO, &rmi, sizeof(rmi)) >= 0) {
        return GetVirtualMemoryPageSize() * Uint8(rmi.availrmem);
    }

#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif
    return 0;
}


clock_t CSystemInfo::GetClockTicksPerSecond(void)
{
#if defined(NCBI_OS_MSWIN)
    return CLOCKS_PER_SEC;

#elif defined(NCBI_OS_UNIX)
    static clock_t ticks = 0;
    if ( ticks ) {
        return ticks;
    }
    clock_t t = sysconf(_SC_CLK_TCK);
    #if defined(CLK_TCK)
        if (!t  ||  t == (clock_t)(-1)) t = CLK_TCK;
    #elif defined(CLOCKS_PER_SEC)
        if (!t  ||  t == (clock_t)(-1)) t = CLOCKS_PER_SEC;
    #endif
    if (t  &&  t != (clock_t)(-1)) {
        ticks = t;
        return t;
    }
    CNcbiError::SetFromErrno();

#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif
    return 0;
}


// @deprecated
bool GetMemoryUsage(size_t* total, size_t* resident, size_t* shared)
{
    size_t scratch;
    if ( !total )    { total    = &scratch; }
    if ( !resident ) { resident = &scratch; }
    if ( !shared )   { shared   = &scratch; }

#if defined(NCBI_OS_MSWIN)
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
    try {
        // Load PSAPI dynamic library -- it should exist on MS-Win NT/2000/XP
        CDll psapi_dll("psapi.dll", CDll::eLoadNow, CDll::eAutoUnload);
        BOOL (STDMETHODCALLTYPE FAR * dllGetProcessMemoryInfo) 
             (HANDLE process, SProcessMemoryCounters& counters, DWORD size) = 0;
        dllGetProcessMemoryInfo = psapi_dll.GetEntryPoint_Func("GetProcessMemoryInfo", &dllGetProcessMemoryInfo);
        if (dllGetProcessMemoryInfo) {
            SProcessMemoryCounters counters;
            dllGetProcessMemoryInfo(GetCurrentProcess(), counters, sizeof(counters));
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
        unsigned long page_size = CSystemInfo::GetVirtualMemoryPageSize();
        statm >> *total >> *resident >> *shared;
        *total    *= page_size;
        *resident *= page_size;
        *shared   *= page_size;
        return true;
    }

#elif defined(NCBI_OS_SOLARIS)
    Int8 len = CFile("/proc/self/as").GetLength();
    if (len > 0) {
        *total    = (size_t) len;
        *resident = (size_t) len; // conservative estimate
        *shared   = 0;            // does this info exist anywhere?
        return true;
    }

#elif defined(HAVE_GETRUSAGE)
    #define _DIV0(a, b) ((a) / ((b) ? (b) : 1))
    // BIG FAT NOTE:  getrusage() seems to use different size units
    struct rusage ru;
    memset(&ru, '\0', sizeof(ru));
    if (getrusage(RUSAGE_SELF, &ru) == 0  &&  ru.ru_maxrss > 0) {
        struct tms t;
        memset(&t, '\0', sizeof(t));
        if (times(&t) != (clock_t)(-1)) {
            clock_t ticks = t.tms_utime + t.tms_stime;
            *resident = _DIV0(ru.ru_idrss,                             ticks);
            *shared   = _DIV0(ru.ru_ixrss,                             ticks);
            *total    = _DIV0(ru.ru_ixrss + ru.ru_idrss + ru.ru_isrss, ticks);
    #ifdef NCBI_OS_DARWIN
            if (*total > 0)
    #endif
                return true;
        }
    }
    #undef _DIV0

    #if defined(NCBI_OS_DARWIN)
        #ifdef MACH_TASK_BASIC_INFO
            task_flavor_t               flavor       = MACH_TASK_BASIC_INFO;
            struct mach_task_basic_info t_info;
            mach_msg_type_number_t      t_info_count = MACH_TASK_BASIC_INFO_COUNT;
        #else
            task_flavor_t               flavor       = TASK_BASIC_INFO;
            struct task_basic_info      t_info;
            mach_msg_type_number_t      t_info_count = TASK_BASIC_INFO_COUNT;
        #endif
        if (task_info(mach_task_self(), flavor, (task_info_t)&t_info, &t_info_count) == KERN_SUCCESS) {
            *total = *resident = t_info.resident_size;
            *shared = 0; // unavailable, even with mach_task_basic_info
            return true;
        }
    #endif

#else
    CNcbiError::Set(CNcbiError::eNotSupported);
#endif
    return false;
}


/// @deprecated
bool GetCurrentProcessTimes(double* user_time, double* system_time)
{
#if defined(NCBI_OS_MSWIN)
    // Each FILETIME structure contains the number of 100-nanosecond time units
    FILETIME ft_creation, ft_exit, ft_kernel, ft_user;
    if (!::GetProcessTimes(GetCurrentProcess(),
                           &ft_creation, &ft_exit, &ft_kernel, &ft_user)) {
        return false;
    }
    if ( system_time ) {
        *system_time = (ft_kernel.dwLowDateTime +
                        ((Uint8) ft_kernel.dwHighDateTime << 32)) * 1.0e-7;
    }
    if ( user_time ) {
        *user_time = (ft_user.dwLowDateTime +
                      ((Uint8) ft_user.dwHighDateTime << 32)) * 1.0e-7;
    }

#elif defined(NCBI_OS_UNIX)

    tms buf;
    clock_t t = times(&buf);
    if ( t == (clock_t)(-1) ) {
        return false;
    }
    clock_t tick = sysconf(_SC_CLK_TCK);
#if defined(CLK_TCK)
    if (!tick  ||  tick == (clock_t)(-1))
        tick = CLK_TCK;
#elif defined(CLOCKS_PER_SEC)
    if (!tick  ||  tick == (clock_t)(-1))
        tick = CLOCKS_PER_SEC;
#endif
    if (tick == (clock_t)(-1)) {
        return false;
    }
    if ( system_time ) {
        *system_time = (double)buf.tms_stime / (double)tick;
    }
    if ( user_time ) {
        *user_time = (double)buf.tms_utime / (double)tick;
    }
#endif
    return true;
}


/// @deprecated
extern int GetProcessFDCount(int* soft_limit, int* hard_limit)
{
#ifdef NCBI_OS_UNIX
    int            fd_count = 0;
    struct dirent* dp;

    rlim_t  cur_limit = -1;
    rlim_t  max_limit = -1;

    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        cur_limit = rlim.rlim_cur;
        max_limit = rlim.rlim_max;
    } else {
        // fallback to sysconf
        cur_limit = static_cast<rlim_t>(sysconf(_SC_OPEN_MAX));
    }

    DIR* dir = opendir("/proc/self/fd/");
    if (dir) {
        while ((dp = readdir(dir)) != NULL) {
            ++fd_count;
        }
        closedir(dir);
        fd_count -= 3;  // '.', '..' and the one for 'opendir'
        if (fd_count < 0) {
            fd_count = -1;
        }
    } else {
        // Fallback to analysis via looping over all fds
        if (cur_limit > 0) {
            int     max_fd = static_cast<int>(cur_limit);
            if (cur_limit > INT_MAX) {
                max_fd = INT_MAX;
            }
            for (int fd = 0;  fd < max_fd;  ++fd) {
                if (fcntl(fd, F_GETFD, 0) == -1) {
                    if (errno == EBADF) {
                        continue;
                    }
                }
                ++fd_count;
            }
        } else {
            fd_count = -1;
        }
    }

    if (soft_limit) {
        if (cur_limit > INT_MAX)
            *soft_limit = INT_MAX;
        else
            *soft_limit = static_cast<int>(cur_limit);
    }
    if (hard_limit) {
        if (max_limit > INT_MAX)
            *hard_limit = INT_MAX;
        else
            *hard_limit = static_cast<int>(max_limit);
    }
    return fd_count;

#else
    if (soft_limit)
        *soft_limit = -1;
    if (hard_limit)
        *hard_limit = -1;
    return -1;

#endif //NCBI_OS_UNIX
}


/// @deprecated
extern int GetProcessThreadCount(void)
{
#ifdef NCBI_OS_LINUX
    int            thread_count = 0;
    struct dirent* dp;

    DIR* dir = opendir("/proc/self/task/");
    if (dir) {
        while ((dp = readdir(dir)) != NULL)
            ++thread_count;
        closedir(dir);
        thread_count -= 2;  // '.' and '..'
        if (thread_count <= 0)
            thread_count = -1;
        return thread_count;
    }
    return -1;
#else
    return -1;
#endif //NCBI_OS_LINUX
}



/////////////////////////////////////////////////////////////////////////////
//
// Memory advise
//

#ifndef HAVE_MADVISE

bool MemoryAdvise(void*, size_t, EMemoryAdvise) {
    return false;
}

#else //HAVE_MADVISE

bool MemoryAdvise(void* addr, size_t len, EMemoryAdvise advise)
{
    if ( !addr /*|| !len*/ ) {
        ERR_POST_X(11, "Memory address is not specified");
        CNcbiError::Set(CNcbiError::eBadAddress);
        return false;
    }
    int adv;
    switch (advise) {
    case eMADV_Normal:
        adv = MADV_NORMAL;     break;
    case eMADV_Random:
        adv = MADV_RANDOM;     break;
    case eMADV_Sequential:
        adv = MADV_SEQUENTIAL; break;
    case eMADV_WillNeed:
        adv = MADV_WILLNEED;   break;
    case eMADV_DontNeed:
        adv = MADV_DONTNEED;   break;
    case eMADV_DontFork:
        #if defined(MADV_DONTFORK)
            adv = MADV_DONTFORK;
            break;
        #else
            ERR_POST_X_ONCE(12, Warning << "MADV_DONTFORK not supported");
            CNcbiError::Set(CNcbiError::eNotSupported);
            return false;
        #endif        
    case eMADV_DoFork:
        #if defined(MADV_DOFORK)
            adv = MADV_DOFORK;
            break;
        #else
            ERR_POST_X_ONCE(12, Warning << "MADV_DOFORK not supported");
            CNcbiError::Set(CNcbiError::eNotSupported);
            return false;
        #endif        
    case eMADV_Mergeable:
        #if defined(MADV_MERGEABLE)
            adv = MADV_MERGEABLE;
            break;
        #else
            ERR_POST_X_ONCE(12, Warning << "MADV_MERGEABLE not supported");
            CNcbiError::Set(CNcbiError::eNotSupported);
            return false;
        #endif        
    case eMADV_Unmergeable:
        #if defined(MADV_UNMERGEABLE)
            adv = MADV_UNMERGEABLE;
            break;
        #else
            ERR_POST_X_ONCE(12, Warning << "MADV_UNMERGEABLE not supported");
            CNcbiError::Set(CNcbiError::eNotSupported);
            return false;
        #endif        
    default:
        _TROUBLE;
        return false;
    }
    // Conversion type of "addr" to char* -- Sun Solaris fix
    if ( madvise((char*) addr, len, adv) != 0 ) {
        int x_errno = errno; \
        ERR_POST_X(13, "madvise() failed: " << 
                   _T_STDSTRING(NcbiSys_strerror(x_errno)));
        CNcbiError::SetErrno(errno = x_errno);
        return false;
    }
    return true;
}
#endif //HAVE_MADVISE



/////////////////////////////////////////////////////////////////////////////
//
// Sleep
//

void SleepMicroSec(unsigned long mc_sec, EInterruptOnSignal onsignal)
{
#if defined(NCBI_OS_MSWIN)

    // Unlike some of its (buggy) Unix counterparts, MS-Win's Sleep() is safe
    // to use with 0, which causes the current thread to sleep at most until
    // the end of the current timeslice (and only if the CPU is not idle).
    static const unsigned long kMicroSecondsPerMilliSecond = kMicroSecondsPerSecond / kMilliSecondsPerSecond;
    Sleep((mc_sec + (kMicroSecondsPerMilliSecond / 2)) / kMicroSecondsPerMilliSecond);

#elif defined(NCBI_OS_UNIX)

#  if defined(HAVE_NANOSLEEP)
    struct timespec delay, unslept;
    memset(&unslept, 0, sizeof(unslept));
    delay.tv_sec  =   mc_sec / kMicroSecondsPerSecond;
    delay.tv_nsec = ((mc_sec % kMicroSecondsPerSecond) * (kNanoSecondsPerSecond / kMicroSecondsPerSecond));
    while (nanosleep(&delay, &unslept) < 0) {
        if (errno != EINTR || onsignal == eInterruptOnSignal) {
            break;
        }
        delay = unslept;
        memset(&unslept, 0, sizeof(unslept));
    }
#  elif defined(HAVE_USLEEP)
    unsigned int sec  = mc_sec / kMicroSecondsPerSecond;
    unsigned int usec = mc_sec % kMicroSecondsPerSecond;
    if (sec) {
        while ((sec = sleep(sec)) > 0) {
            if (onsignal == eInterruptOnSignal) {
                return;
            }
        }
    }
    // usleep() detects errors (such as EINTR) but can't tell unslept time :-/
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
#    ifdef SELECT_UPDATES_TIMEOUT
        if (errno != EINTR  ||  onsignal == eInterruptOnSignal)
#    endif
            break;
    }
#  endif //HAVE FINE SLEEP API

#endif //NCBI_OS_...
}


void SleepMilliSec(unsigned long ml_sec, EInterruptOnSignal onsignal)
{
#ifdef NCBI_OS_MSWIN
    Sleep(ml_sec);
#else
    SleepMicroSec(ml_sec * (kMicroSecondsPerSecond / kMilliSecondsPerSecond), onsignal);
#endif //NCBI_OS_MSWIN
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

static bool s_EnableSuppressSystemMessageBox  = true;
static bool s_DoneSuppressSystemMessageBox    = false;
static bool s_SuppressedDebugSystemMessageBox = false;

// Handler for "Unhandled" exceptions
static LONG CALLBACK _SEH_Handler(EXCEPTION_POINTERS* ep)
{
#ifdef _UNICODE
    wchar_t dumpname[64], tmp[64];
    wcscat( wcscat( wcscpy(dumpname, L"core."), _ltow(GetCurrentProcessId(), tmp, 10)), L".dmp");
#else
    char dumpname[64], tmp[64];
    strcat( strcat( strcpy(dumpname, "core."), _ltoa(GetCurrentProcessId(), tmp, 10)), ".dmp");
#endif
    HANDLE hf = CreateFile(dumpname,
        GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION ei;
        ei.ThreadId = GetCurrentThreadId(); 
        ei.ExceptionPointers = ep;
        ei.ClientPointers = FALSE;
        MiniDumpWriteDump(
            GetCurrentProcess(), GetCurrentProcessId(),
            hf,  MiniDumpNormal, &ei, NULL, NULL);
        CloseHandle(hf);
        cerr << "Unhandled exception: " << hex
             << ep->ExceptionRecord->ExceptionCode << " at "
             << ep->ExceptionRecord->ExceptionAddress << endl;
    }
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
    if ((mode & fSuppress_System) == fSuppress_System ) {
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

#else
    // dummy, to avoid compilation warning
    mode = 0;

#endif
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
    return s_DoneSuppressSystemMessageBox  &&  s_SuppressedDebugSystemMessageBox;
#else
    return false;
#endif //NCBI_OS_MSWIN
}



/////////////////////////////////////////////////////////////////////////////
///
/// CPU
///

// Code credit for CPU capabilities checks:
// https://attractivechaos.wordpress.com/2017/09/04/on-cpu-dispatch/
//
bool VerifyCpuCompatibility(string* message)
{
    // Compiles with SSE 4.2 support -- verify that CPU allow it

    #if defined(NCBI_SSE)  &&  NCBI_SSE >= 42
        unsigned eax, ebx, ecx, edx;
        #ifdef NCBI_OS_MSWIN
            int cpuid[4];
            __cpuid(cpuid, 1);
            eax = cpuid[0], ebx = cpuid[1], ecx = cpuid[2], edx = cpuid[3];
        #else
            asm volatile("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (1));
        #endif
        if ( !(ecx>>20 & 1) ) // SSE 4.2
        {
            if (message) {
                message->assign("Application requires a CPU with SSE 4.2 support");
            }
            return false;
        }
    #endif

    return true;
}


// Get all information on the first usage
const CCpuFeatures::InstructionSet& CCpuFeatures::IS(void)
{
    static CCpuFeatures::InstructionSet* instruction_set = new CCpuFeatures::InstructionSet();
    return *instruction_set;
}

CCpuFeatures::InstructionSet::InstructionSet(void)
    : f01_ECX_  { 0 },
      f01_EDX_  { 0 },
      f07_EBX_  { 0 },
      f07_ECX_  { 0 },
      f81_ECX_  { 0 },
      f81_EDX_  { 0 }
{
    int nIds   = 0;
    int nExIds = 0;

    using TRegisters = std::array<int,4>;  // [EAX, EBX, ECX, EDX]
    TRegisters registers;
    vector<TRegisters> m_Data;
    vector<TRegisters> m_ExtData;

    // Local function
    auto CPUID = [](TRegisters& r, int func_id) {
        #ifdef NCBI_OS_MSWIN
            __cpuid(r.data(), func_id);
        #else
            asm volatile("cpuid" : "=a" (r[0]), "=b" (r[1]), "=c" (r[2]), "=d" (r[3]) : "a" (func_id));
        #endif
    };
    auto CPUID_EX = [](TRegisters& r, int func_id, int sub_id) {
        #ifdef NCBI_OS_MSWIN
            __cpuidex(r.data(), func_id, sub_id);
        #else
            asm volatile("cpuid" : "=a" (r[0]), "=b" (r[1]), "=c" (r[2]), "=d" (r[3]) : "a" (func_id), "c" (sub_id));
        #endif
    };

    // Calling __cpuid with 0x0 as the function_id argument
    // gets the number of the highest valid function ID.
    CPUID(registers, 0);
    nIds = registers[0];

    for (int i = 0; i <= nIds; ++i) {
        CPUID_EX(registers, i, 0);
        m_Data.push_back(registers);
    }

    // Capture vendor string
    char vendor[0x20];
    memset(vendor, 0, sizeof(vendor));
    *reinterpret_cast<int*>(vendor)     = m_Data[0][1];
    *reinterpret_cast<int*>(vendor + 4) = m_Data[0][3];
    *reinterpret_cast<int*>(vendor + 8) = m_Data[0][2];
    m_VendorStr = vendor;
    if (m_VendorStr == "GenuineIntel") {
        m_Vendor = eIntel;
    }
    else if (m_VendorStr == "AuthenticAMD") {
        m_Vendor = eAMD;
    }

    // Load bitset with flags for function 0x00000001
    if (nIds >= 1) {
        f01_ECX_ = m_Data[1][2];
        f01_EDX_ = m_Data[1][3];
    }
    // Load bitset with flags for function 0x00000007
    if (nIds >= 7) {
        f07_EBX_ = m_Data[7][1];
        f07_ECX_ = m_Data[7][2];
    }

    // Calling cpuid with 0x80000000 as the function_id argument
    // gets the number of the highest valid extended ID.
    CPUID(registers, 0x80000000);
    nExIds = registers[0];
    //
    for (int i = 0x80000000; i <= nExIds; ++i) {
        CPUID_EX(registers, i, 0);
        m_ExtData.push_back(registers);
    }

    // Load bitset with flags for function 0x80000001
    if (nExIds >= (int)0x80000001) {
        f81_ECX_ = m_ExtData[1][2];
        f81_EDX_ = m_ExtData[1][3];
    }

    // Interpret CPU brand string if reported
    if (nExIds >= (int)0x80000004) {
        char brand[0x40];
        memset(brand, 0, sizeof(brand));
        memcpy(brand,      m_ExtData[2].data(), sizeof(registers));
        memcpy(brand + 16, m_ExtData[3].data(), sizeof(registers));
        memcpy(brand + 32, m_ExtData[4].data(), sizeof(registers));
        m_BrandStr = brand;
    }
};


void CCpuFeatures::Print(void)
{
    list<string> supported;
    list<string> unsupported;

    auto check = [&supported,&unsupported](string feature, bool is_supported) {
        if (is_supported) {
            supported.push_back(feature);
        } else {
            unsupported.push_back(feature);
        }
    };

    // sorted order

    check( "_3DNOW",      CCpuFeatures::_3DNOW()     );
    check( "_3DNOWEXT",   CCpuFeatures::_3DNOWEXT()  );
    check( "ABM",         CCpuFeatures::ABM()        );
    check( "ADX",         CCpuFeatures::ADX()        );
    check( "AES",         CCpuFeatures::AES()        );
    check( "AVX",         CCpuFeatures::AVX()        );
    check( "AVX2",        CCpuFeatures::AVX2()       );
    check( "AVX512CD",    CCpuFeatures::AVX512CD()   );
    check( "AVX512ER",    CCpuFeatures::AVX512ER()   );
    check( "AVX512F",     CCpuFeatures::AVX512F()    );
    check( "AVX512PF",    CCpuFeatures::AVX512PF()   );
    check( "BMI1",        CCpuFeatures::BMI1()       );
    check( "BMI2",        CCpuFeatures::BMI2()       );
    check( "CLFSH",       CCpuFeatures::CLFSH()      );
    check( "CMPXCHG16B",  CCpuFeatures::CMPXCHG16B() );
    check( "CX8",         CCpuFeatures::CX8()        );
    check( "ERMS",        CCpuFeatures::ERMS()       );
    check( "F16C",        CCpuFeatures::F16C()       );
    check( "FMA",         CCpuFeatures::FMA()        );
    check( "FSGSBASE",    CCpuFeatures::FSGSBASE()   );
    check( "FXSR",        CCpuFeatures::FXSR()       );
    check( "HLE",         CCpuFeatures::HLE()        );
    check( "INVPCID",     CCpuFeatures::INVPCID()    );
    check( "LAHF",        CCpuFeatures::LAHF()       );
    check( "LZCNT",       CCpuFeatures::LZCNT()      );
    check( "MMX",         CCpuFeatures::MMX()        );
    check( "MMXEXT",      CCpuFeatures::MMXEXT()     );
    check( "MONITOR",     CCpuFeatures::MONITOR()    );
    check( "MOVBE",       CCpuFeatures::MOVBE()      );
    check( "MSR",         CCpuFeatures::MSR()        );
    check( "OSXSAVE",     CCpuFeatures::OSXSAVE()    );
    check( "PCLMULQDQ",   CCpuFeatures::PCLMULQDQ()  );
    check( "POPCNT",      CCpuFeatures::POPCNT()     );
    check( "PREFETCHWT1", CCpuFeatures::PREFETCHWT1());
    check( "RDRAND",      CCpuFeatures::RDRAND()     );
    check( "RDSEED",      CCpuFeatures::RDSEED()     );
    check( "RDTSCP",      CCpuFeatures::RDTSCP()     );
    check( "RTM",         CCpuFeatures::RTM()        );
    check( "SEP",         CCpuFeatures::SEP()        );
    check( "SHA",         CCpuFeatures::SHA()        );
    check( "SSE",         CCpuFeatures::SSE()        );
    check( "SSE2",        CCpuFeatures::SSE2()       );
    check( "SSE3",        CCpuFeatures::SSE3()       );
    check( "SSE4.1",      CCpuFeatures::SSE41()      );
    check( "SSE4.2",      CCpuFeatures::SSE42()      );
    check( "SSE4a",       CCpuFeatures::SSE4a()      );
    check( "SSSE3",       CCpuFeatures::SSSE3()      );
    check( "SYSCALL",     CCpuFeatures::SYSCALL()    );
    check( "TBM",         CCpuFeatures::TBM()        );
    check( "XOP",         CCpuFeatures::XOP()        );
    check( "XSAVE",       CCpuFeatures::XSAVE()      );

    cout << "CPU vendor ID    : " << CCpuFeatures::VendorStr() << endl;
    cout << "CPU brand string : " << CCpuFeatures::BrandStr()  << endl;

    cout << endl << "Supported features:" << endl;
    for (auto feature : supported) {
        cout << feature << " ";
    }
    cout << endl;
    cout << endl << "Not supported features:" << endl;
    for (auto feature : unsupported) {
        cout << feature << " ";
    }
    cout << endl;
}



END_NCBI_SCOPE
