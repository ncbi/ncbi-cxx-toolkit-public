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
* File Description:
*      System control functions
*      
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/07/02 16:45:35  ivanov
* Initialization
*
* ===========================================================================
*/

#include <corelib/ncbireg.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbitime.hpp>


// Define HEAP_LIMIT and CPU_LIMIT (UNIX only)
#ifdef NCBI_OS_UNIX
#define USE_SETHEAPLIMIT
#define USE_SETCPULIMIT
#include <sys/times.h>
#include <limits.h>
#endif

#ifdef USE_SETCPULIMIT
#include <signal.h>
#endif


BEGIN_NCBI_SCOPE


//---------------------------------------------------------------------------


#ifdef NCBI_OS_UNIX

enum EExitCode {
    eEC_None,
    eEC_Memory,
    eEC_Cpu
};


static CFastMutex s_ExitHandler_Mutex;
static bool       s_ExitHandlerIsSet = false;
static EExitCode  s_ExitCode         = eEC_None;
static CTime      s_TimeSet;
static size_t     s_HeapLimit        = 0;
static size_t     s_CpuTimeLimit     = 0;
static void*      s_ReserveMemory    = 0;



/* Routine to be called at the exit from application
 */
static void s_ExitHandler()
{
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);

    // Free reserved memory
    if ( s_ReserveMemory ) {
        free(s_ReserveMemory);
    }

    switch(s_ExitCode) {

    case eEC_Memory:
        {
            ERR_POST("Memory heap limit attained in allocating memory " \
                     "with operator new (" << s_HeapLimit << " byte)");
            break;
        }

    case eEC_Cpu: 
        {
            ERR_POST("CPU time limit attained (" << s_CpuTimeLimit << " sec)");
            tms buffer;
            if ( times(&buffer) == (clock_t)(-1) ) {
                ERR_POST("Error get CPU time consumed with program");
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
        break;
    }
    
    // Write program's time
    CTime ct(CTime::eCurrent);
    CTime et(2000,1,1);
    et.AddSecond( ct.GetTimeT() - s_TimeSet.GetTimeT() );
    LOG_POST("Program's time: " << Endm <<
             "\tstart limit - " << s_TimeSet.AsString() << Endm <<
             "\ttermination - " << ct.AsString() << Endm);
    et.SetFormat("h:m:s");
    LOG_POST("\texecution   - " << et.AsString());
}


/* Set routine to be called at the exit from application
 */
static bool s_SetExitHandler()
{
    // Set exit routine if it not set yet
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);
    if ( !s_ExitHandlerIsSet ) {
        if ( atexit(s_ExitHandler) !=0 )
            return false;
        s_ExitHandlerIsSet = true;
        s_TimeSet.SetCurrent();
        // Reserve some memory (4Kb)
        s_ReserveMemory = malloc(4096);
    }
    return true;
}
    
#endif /* NCBI_OS_UNIX */



/////////////////////////////////////////////////////////////////////////////
//
// Ncbi_SetHeapLimit
//


#ifdef USE_SETHEAPLIMIT

/* Handler for operator new.
 */
static void s_NewHandler()
{
    s_ExitCode = eEC_Memory;
    exit(-1);
}


bool Ncbi_SetHeapLimit(size_t max_heap_size)
{
    if (s_HeapLimit == max_heap_size) 
        return true;
    
    if ( !s_SetExitHandler() )
        return false;

    // Set new heap limit
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);
    struct rlimit rl;
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

bool Ncbi_SetHeapLimit(size_t max_heap_size) {
  return false;
}

#endif /* USE_SETHEAPLIMIT */



/////////////////////////////////////////////////////////////////////////////
//
// Ncbi_SetCpuTimeLimit
//


#ifdef USE_SETCPULIMIT

static void s_SignalHandler(int sig)
{
    if ( sig == SIGXCPU ) {
        s_ExitCode = eEC_Cpu;
        exit(-1);
    }
}


bool Ncbi_SetCpuTimeLimit(size_t max_cpu_time)
{
    if (s_CpuTimeLimit == max_cpu_time) 
        return true;
    
    if ( !s_SetExitHandler() )
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
    if (setrlimit(RLIMIT_CPU, &rl) != 0) 
        return false;
    s_CpuTimeLimit = max_cpu_time;
    
    // Set signal handler for SIGXCPU
    signal(SIGXCPU, s_SignalHandler);
    
    return true;
}

#else

bool Ncbi_SetCpuTimeLimit(size_t max_cpu_time)
{
    return false;
}

#endif /* USE_SETCPULIMIT */


END_NCBI_SCOPE
