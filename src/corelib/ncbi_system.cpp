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
#  include <sys/time.h>
#  include <sys/resource.h>
#  include <sys/times.h>
#  include <limits.h>
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
}
#endif

#if defined(USE_SETCPULIMIT)
#  include <signal.h>
#endif

#if defined(NCBI_OS_MSWIN)
#  include <crtdbg.h>
#  include <stdlib.h>
#  include <windows.h>
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
            LOG_POST("\tuser CPU time   : " << 
                     buffer.tms_utime/CLK_TCK << " sec");
            LOG_POST("\tsystem CPU time : " << 
                     buffer.tms_stime/CLK_TCK << " sec");
            LOG_POST("\ttotal CPU time  : " << 
                     (buffer.tms_stime + buffer.tms_utime)/CLK_TCK << " sec");
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
    exit(-1);
}


bool SetCpuTimeLimit(size_t max_cpu_time,
                     TLimitsPrintHandler handler, 
                     TLimitsPrintParameter parameter)
{
    if (s_CpuTimeLimit == max_cpu_time) 
        return true;
    
    if ( !s_SetExitHandler(handler, parameter) )
        return false;
    
    // Set new CPU time limit
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);

    struct rlimit rl;
    if ( max_cpu_time ) {
        rl.rlim_cur = rl.rlim_max = max_cpu_time;
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

bool SetCpuTimeLimit(size_t max_cpu_time,
                  TLimitsPrintHandler handler, 
                  TLimitsPrintParameter parameter)

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
    unsigned long ps = 0;

#if defined(NCBI_OS_MSWIN)
    SYSTEM_INFO si;
    GetSystemInfo(&si); 
    ps = si.dwAllocationGranularity;
#elif defined(NCBI_OS_UNIX)
    long x = 0;
#  ifdef _SC_PAGESIZE
    x = sysconf(_SC_PAGESIZE);
#  endif
#  ifdef HAVE_GETPAGESIZE
    if (x <= 0) {
        x = getpagesize();
    }
    ps = (x <= 0) ? 0 : x;
#  endif
#endif /*OS_TYPE*/
    return ps;
}


/////////////////////////////////////////////////////////////////////////////
//
// Sleep
//

void SleepMicroSec(unsigned long mc_sec)
{
#if defined(NCBI_OS_MSWIN)
    Sleep((mc_sec + 500) / 1000);
#elif defined(NCBI_OS_UNIX)
    struct timeval delay;
    delay.tv_sec  = mc_sec / kMicroSecondsPerSecond;
    delay.tv_usec = mc_sec % kMicroSecondsPerSecond;
    select(0, (fd_set*)NULL, (fd_set*)NULL, (fd_set*)NULL, &delay);
#elif defined(NCBI_OS_MAC)
    OTDelay((mc_sec + 500) / 1000);
#endif
}


void SleepMilliSec(unsigned long ml_sec)
{
#if defined(NCBI_OS_MSWIN)
    Sleep(ml_sec);
#elif defined(NCBI_OS_UNIX)
    SleepMicroSec(ml_sec * 1000);
#elif defined(NCBI_OS_MAC)
    OTDelay(ml_sec);
#endif
}


void SleepSec(unsigned long sec)
{
    SleepMicroSec(sec * kMicroSecondsPerSecond);
}



/////////////////////////////////////////////////////////////////////////////
///
/// Suppress Diagnostic Popup Messages
///

extern void SuppressSystemMessageBox(TSuppressSystemMessageBox mode)
{
#if defined(NCBI_OS_MSWIN)
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
#else
    // not implemented
#endif
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
