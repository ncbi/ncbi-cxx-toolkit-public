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

#ifdef NCBI_OS_UNIX
#include <sys/types.h>
#elif defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#endif

BEGIN_NCBI_SCOPE


// Code for program exit hamndler

enum ELimitsExitCode {
    eLEC_None,    // normal exit
    eLEC_Memory,  // memory limit
    eLEC_Cpu      // CPU usage limit
};


// Parameter's type for limit's print handler
typedef void* TLimitsPrintParameter;

// Type of handler for print dump info after generating any limitation event
typedef void (*TLimitsPrintHandler)(ELimitsExitCode, size_t, CTime&, TLimitsPrintParameter);


/* [UNIX only] 
 * Set the limit for the size of dynamic memory(heap) allocated by the process.
 *
 * If, during the program execution, the heap size exceeds "max_heap_size"
 * in any `operator new' (but not malloc, etc.!), then:
 *  1) dump info about current program state to log-stream.
 *     if defined print handler "handler", then it will be used for write dump.
 *     if defined "parameter", it will be carried to print handler;
 *  2) terminate the program.
 *
 * NOTE:  "max_heap_size" == 0 will lift off the heap restrictions.
 */
extern bool SetHeapLimit(size_t max_heap_size, 
                         TLimitsPrintHandler handler = 0, 
                         TLimitsPrintParameter parameter = 0);


/* [UNIX only] 
 * Set the limit for the CPU time that can be consumed by this process.
 *
 * If current process consumes more than "max_cpu_time" seconds of CPU time:
 *  1) dump info about current program state to log-stream.
 *     if defined print handler "handler", then it will be used for write dump.
 *     if defined "parameter", it will be carried to print handler;
 *  2) terminate the program.
 *
 * NOTE:  "max_cpu_time" == 0 will lift off the CPU time restrictions.
 */
extern bool SetCpuTimeLimit(size_t max_cpu_time,
                            TLimitsPrintHandler handler = 0, 
                            TLimitsPrintParameter parameter = 0);

/* [UNIX & Windows]
 * Return number of active CPUs (never less than 1).
 */
NCBI_XNCBI_EXPORT
extern unsigned int GetCpuCount(void);


/* [UNIX & Windows]
 * Sleep specified number of microseconds/millisecond/seconds
 */
NCBI_XNCBI_EXPORT
extern void SleepSec(unsigned long sec);

NCBI_XNCBI_EXPORT
extern void SleepMilliSec(unsigned long ml_sec);

NCBI_XNCBI_EXPORT
extern void SleepMicroSec(unsigned long mc_sec);

/// Process identifier
#ifdef NCBI_OS_UNIX
typedef pid_t TPid;
#elif defined(NCBI_OS_MSWIN)
typedef DWORD TPid;
#else
typedef int TPid;
#endif

NCBI_XNCBI_EXPORT
extern TPid GetPID(void);

class NCBI_XNCBI_EXPORT CPIDGuardException
    : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eStillRunning, ///< The process listed in the file is still around.
        eCouldntOpen   ///< Unable to open the PID file for writing
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eStillRunning: return "eStillRunning";
        case eCouldntOpen:  return "eCouldntOpen";
        default:            return CException::GetErrCodeString();
        }
    }

    /// Constructor.
    CPIDGuardException(const char* file, int line,
                       const CException* prev_exception, EErrCode err_code,
                       const string& message, TPid pid = 0)
        throw()
        : CException(file, line, prev_exception, CException::eInvalid,
                     message),
          m_PID(pid)
        NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CPIDGuardException, CException);

public: // lost by N_E_D_I
    virtual void ReportExtra(ostream& out) const
        { out << "pid " << m_PID; }
    TPid GetPID(void) const throw() { return m_PID; }

protected:
    void x_Assign(const CPIDGuardException& src)
        { CException::x_Assign(src);  m_PID = src.m_PID; }

private:
    TPid m_PID;
};

class NCBI_XNCBI_EXPORT CPIDGuard
{
public:
    /// If "filename" contains no path, make it relative to "dir"
    /// (which defaults to /tmp on Unix and %HOME% on Windows).
    /// If the file already exists and identifies a live process,
    /// throws CPIDGuardException.
    CPIDGuard(const string& filename, const string& dir = kEmptyStr);

    ~CPIDGuard() { Release(); }

    /// Returns non-zero if there was a stale file.
    TPid GetOldPID(void) { return m_OldPID; }

    /// Removes the file.
    void Release(void);

    /// @param pid the new process ID to store (defaults to the
    /// current PID); useful when the real work occurs in a child
    /// process that outlives the parent.
    void UpdatePID(TPid pid = 0);

private:
    string m_Path;
    TPid   m_OldPID;
};

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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

