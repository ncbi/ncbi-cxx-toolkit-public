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


#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>

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
typedef void (*TLimitsPrintHandler)(ELimitsExitCode, size_t, CTime&, TLimitsPrintParameter);


/// [UNIX only]  Set memory limit.
///
/// Set the limit for the size of dynamic memory (heap) allocated
/// by the process. 
/// 
/// @param max_heap_size
///   The maximal amount of dynamic memory can be allocated by the process
///   in any `operator new' (but not malloc, etc.!).
///   The 0 value lift off the heap restrictions.
/// @param handler
///   Pointer to a print handler used for dump output.
///   Use default handler if passed as NULL.
/// @param parameter
///   Parameter carried into the print handler. Can be passed as NULL.
/// @return 
///   Completion status.
NCBI_XNCBI_EXPORT
extern bool SetHeapLimit(size_t max_heap_size, 
                         TLimitsPrintHandler handler = 0, 
                         TLimitsPrintParameter parameter = 0);


/// [UNIX only]  Set CPU usage limit.
///
/// Set the limit for the CPU time that can be consumed by current process.
/// 
/// @param max_cpu_time
///   The maximal amount of seconds of CPU time can be consumed by the process.
///   The 0 value lift off the CPU time restrictions.
/// @param handler
///   Pointer to a print handler used for dump output.
///   Use default handler if passed as NULL.
/// @param parameter
///   Parameter carried into the print handler. Can be passed as NULL.
/// @return 
///   Completion status.
NCBI_XNCBI_EXPORT
extern bool SetCpuTimeLimit(size_t max_cpu_time,
                            TLimitsPrintHandler handler = 0, 
                            TLimitsPrintParameter parameter = 0);


/////////////////////////////////////////////////////////////////////////////
///
/// System information
///

/// [UNIX & Windows]  Return number of active CPUs (never less than 1).
NCBI_XNCBI_EXPORT
extern unsigned int GetCpuCount(void);


/// [UNIX & Windows]
/// Return granularity with which virtual memory is allocated.
/// Return 0 if cannot determine it on current platform or if an error occurs.
NCBI_XNCBI_EXPORT
extern unsigned long GetVirtualMemoryPageSize(void);


/////////////////////////////////////////////////////////////////////////////
///
/// Sleep
///
/// Suspend execution for a time.
///
/// Sleep for at least the specified number of microsec/millisec/seconds.
/// Time slice restrictions are imposed by platform/OS.
/// On UNIX the sleep can be interrupted by a signal.

/// [UNIX & Windows]

NCBI_XNCBI_EXPORT
extern void SleepSec(unsigned long sec);

NCBI_XNCBI_EXPORT
extern void SleepMilliSec(unsigned long ml_sec);

NCBI_XNCBI_EXPORT
extern void SleepMicroSec(unsigned long mc_sec);



/////////////////////////////////////////////////////////////////////////////
///
/// Suppress Diagnostic Popup Messages
///
/// Suppress popup messages on execution errors.
/// NOTE: Windows-specific, suppresses all error message boxes in both
/// runtime and in debug libraries, as well as all General Protection Fault
/// messages. Usefull to suppress messages about missing loaded DLLs,
/// for example.

/// [Windows]

/// Suppress modes
enum ESuppressSystemMessageBox {
    fSuppress_System  = (1<<0),     ///< System errors
    fSuppress_Runtime = (1<<1),     ///< Runtime library
    fSuppress_Debug   = (1<<2),     ///< Debug library

    fSuppress_All     = fSuppress_System | fSuppress_Runtime | fSuppress_Debug,
    fSuppress_Default = fSuppress_System | fSuppress_Runtime
};
/// Binary OR of "ESuppressSystemMessageBox"
typedef int TSuppressSystemMessageBox;  

NCBI_XNCBI_EXPORT
extern void SuppressSystemMessageBox(TSuppressSystemMessageBox mode = 
                                     fSuppress_Default);


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.16  2005/02/23 13:45:25  ivanov
 * + SuppressSystemMessageBox() (Windows specific)
 *
 * Revision 1.15  2004/08/03 11:56:45  ivanov
 * + GetVirtualMemoryPageSize()
 *
 * Revision 1.14  2003/09/25 16:52:47  ivanov
 * CPIDGuard class moved to ncbi_process.hpp
 *
 * Revision 1.13  2003/09/23 21:13:44  ucko
 * +CPIDGuard::UpdatePID
 *
 * Revision 1.12  2003/08/12 17:37:45  ucko
 * Cleaned up CPIDGuardException a bit.
 *
 * Revision 1.11  2003/08/12 17:24:51  ucko
 * Add support for PID files.
 *
 * Revision 1.10  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.9  2002/07/16 13:38:00  ivanov
 * Little modification and optimization of the Sleep* functions
 *
 * Revision 1.8  2002/07/15 21:43:25  ivanov
 * Added functions SleepMicroSec, SleepMilliSec, SleepSec
 *
 * Revision 1.7  2002/04/11 20:39:16  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.6  2001/12/09 06:27:36  vakatov
 * GetCpuCount() -- get rid of warning (in 64-bit mode), change ret.val. type
 *
 * Revision 1.5  2001/11/08 21:31:07  ivanov
 * Renamed GetCPUNumber() -> GetCpuCount()
 *
 * Revision 1.4  2001/11/08 21:10:22  ivanov
 * Added function GetCPUNumber()
 *
 * Revision 1.3  2001/07/23 15:59:36  ivanov
 * Added possibility using user defined dump print handler
 *
 * Revision 1.2  2001/07/02 21:33:05  vakatov
 * Fixed against SIGXCPU during the signal handling.
 * Increase the amount of reserved memory for the memory limit handler
 * to 10K (to fix for the 64-bit WorkShop compiler).
 * Use standard C++ arg.processing (ncbiargs) in the test suite.
 * Cleaned up the code. Get rid of the "Ncbi_" prefix.
 *
 * Revision 1.1  2001/07/02 16:47:25  ivanov
 * Initialization
 *
 * ===========================================================================
 */

#endif  /* NCBI_SYSTEM__HPP */

