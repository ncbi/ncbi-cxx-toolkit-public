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
* File Description:
*
*   System functions:
*      SetHeapLimit()
*      SetCpuTimeLimit()
*      GetCpuCount()
*      
* ---------------------------------------------------------------------------
* $Log$
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

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_system.hpp>

#ifdef NCBI_OS_WIN
#  include <windows.h>
#endif

#ifdef NCBI_OS_UNIX
#  include <sys/resource.h>
#  include <sys/times.h>
#  include <limits.h>
#  include <unistd.h>
#  define USE_SETHEAPLIMIT
#  define USE_SETCPULIMIT
#endif

#ifdef USE_SETCPULIMIT
#  include <signal.h>
#endif


BEGIN_NCBI_SCOPE

//---------------------------------------------------------------------------

// MIPSpro 7.3 workarounds:
//   1) it declares set_new_handler() in both global and std:: namespaces;
//   2) it apparently gets totally confused by `extern "C"' inside a namespace.
#  if defined(NCBI_COMPILER_MIPSPRO)
#    define set_new_handler std::set_new_handler
#  else
extern "C" {
    static void s_ExitHandler(void);
    static void s_SignalHandler(int sig);
}
#  endif  /* NCBI_COMPILER_MIPSPRO */



//---------------------------------------------------------------------------


#ifdef NCBI_OS_UNIX

static CFastMutex            s_ExitHandler_Mutex;
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

/* Routine to be called at the exit from application
 */
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
                     "with operator new (" << s_HeapLimit << " bytes)");
            break;
        }

    case eLEC_Cpu: 
        {
            ERR_POST("CPU time limit exceeded (" << s_CpuTimeLimit << " sec)");
            tms buffer;
            if (times(&buffer) == (clock_t)(-1)) {
                ERR_POST("Error in getting CPU time consumed with program");
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


/* Set routine to be called at the exit from application
 */
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
//  SetHeapLimit
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
//  SetCpuTimeLimit
//


#ifdef USE_SETCPULIMIT

static void s_SignalHandler(int sig)
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
//  GetCpuCount
//

/*  Return number of active CPUs */

int GetCpuCount(void)
{
#if defined(NCBI_OS_MSWIN)
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return (int)sysInfo.dwNumberOfProcessors;

#elif defined(NCBI_OS_UNIX)
    int nproc = 0;
#   if defined(_SC_NPROC_ONLN)
    nproc = sysconf(_SC_NPROC_ONLN);
#   elif defined(_SC_NPROCESSORS_ONLN)
    nproc = sysconf(_SC_NPROCESSORS_ONLN);
#   endif
    return  nproc <= 0 ? 1 : nproc;

#else
    return 1;
#endif
}


END_NCBI_SCOPE
