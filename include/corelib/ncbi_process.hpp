#ifndef CORELIB__NCBIPROCESS__HPP
#define CORELIB__NCBIPROCESS__HPP

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
 * Authors:  Aaron Ucko, Vladimir Ivanov
 *
 *
 */

/// @file ncbi_process.hpp 
/// Defines a process management classes.
///
/// Defines classes: 
///     CProcess
///     CPIDGuard
///
/// Implemented for: UNIX, MS-Windows


#include <corelib/ncbi_limits.hpp>

#if defined(NCBI_OS_UNIX)
#  include <sys/types.h>
#elif defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#endif


/** @addtogroup Process
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Process identifier (PID) and process handle.
#if defined(NCBI_OS_UNIX)
  typedef pid_t  TPid;
  typedef TPid   TProcessHandle;
#elif defined(NCBI_OS_MSWIN)
  typedef DWORD  TPid;
  typedef HANDLE TProcessHandle;
#else
  typedef int    TPid;
  typedef TPid   TProcessHandle;
#endif


/////////////////////////////////////////////////////////////////////////////
///
/// CProcess --
///
/// Process management functions.
///
/// Class can work with process identifiers and process handles.
/// On Unix both said terms are equivalent and correspond a pid.
/// On MS Windows they are different.
///
/// NOTE:  All CExec:: functions works with process handle.

class NCBI_XNCBI_EXPORT CProcess
{
public:
    /// How to interpret passed process identifier.
    enum EProcessType {
        ePid,     ///< As real process identifier (pid).
        eHandle   ///< As a process handle.
    };

    /// Default process termination timeout.
    static const unsigned long kDefaultKillTimeout;

    /// Constructor.
    CProcess(long process, EProcessType type = eHandle);
#if defined(NCBI_OS_MSWIN)
    CProcess(HANDLE process, EProcessType type = eHandle);
#endif

    /// Get process identifier for a current running process.
    static TPid GetCurrentPid(void);
    /// Get process identifier for a parent of the current process.
    static TPid GetParentPid(void);

    /// Check process existence.
    ///
    /// @return
    ///   TRUE  - if the process is still running.
    ///   FALSE - if the process did not exist or was already terminated.
    /// @sa
    ///   Wait(), WaitForAlive(), WaitForTerminate()
    bool IsAlive(void) const;

    /// Terminate process.
    ///
    /// @timeout
    ///   Wait time in milliseconds between first "soft" and second "hard"
    ///   attempts to terminate the process.
    ///   Note that in case of zero or very small timeout the killing 
    ///   process can be not released and continue to persists as zombie
    ///   process even after call of this function.
    /// @return
    ///   TRUE  - if the process did not exist or was successfully terminated.
    ///   FALSE - if the process is still running and cannot be terminated.
    bool Kill(unsigned long timeout = kDefaultKillTimeout) const;

    /// Wait until process terminates.
    ///
    /// Wait until the process has terminates or timeout expired.
    /// Return immediately if specifed process has already terminated.
    /// @param timeout
    ///   Time-out interval in milliceconds. By default it is infinite.
    /// @return
    ///   - Exit code of the process, if no errors.
    ///   - (-1), if error has occurred.
    /// @sa
    ///   IsAlive(), WaitForAlive(), WaitForTerminate()
    int Wait(unsigned long timeout = kMax_ULong) const;

private:
    long          m_Process;   ///< Process identifier.
    EProcessType  m_Type;      ///< Type of process identifier.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CPIDGuardException --
///

class CPIDGuardException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eStillRunning, ///< The process listed in the file is still around.
        eWrite         ///< Unable to write into the PID file.
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eStillRunning: return "eStillRunning";
        case eWrite:        return "eWrite";
        default:            return CException::GetErrCodeString();
        }
    }

    /// Constructor.
    CPIDGuardException(const CDiagCompileInfo& info,
                       const CException* prev_exception, EErrCode err_code,
                       const string& message, TPid pid = 0)
        : CException(info, prev_exception, CException::eInvalid,
                     message),
          m_PID(pid)
        NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CPIDGuardException, CException);

public:
    virtual void ReportExtra(ostream& out) const
    {
        out << "pid " << m_PID;
    }

    TPid GetPID(void) const throw() { return m_PID; }

protected:
    virtual void x_Assign(const CException& src)
    {
        CException::x_Assign(src);
        m_PID = dynamic_cast<const CPIDGuardException&>(src).m_PID;
    }

private:
    TPid  m_PID;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CPIDGuard --
///

class NCBI_XNCBI_EXPORT CPIDGuard
{
public:
    /// If "filename" contains no path, make it relative to "dir"
    /// (which defaults to /tmp on Unix and %HOME% on Windows).
    /// If the file already exists and identifies a live process,
    /// throws CPIDGuardException.
    CPIDGuard(const string& filename, const string& dir = kEmptyStr);

    /// Destructor.
    ///
    /// Just calls Release();
    ~CPIDGuard(void);

    /// Returns non-zero if there was a stale file.
    TPid GetOldPID(void) { return m_OldPID; }

    /// Release PID.
    ///
    /// Decrease reference counter for current PID and remove the file
    /// if it is not used more (reference counter is 0).
    void Release(void);

    /// Remove the file.
    ///
    /// Remove PID file forcibly, ignoring any reference counter.
    void Remove(void);

    /// Update PID in the file.
    ///
    /// @param pid
    ///   The new process ID to store (defaults to the current PID);
    ///   useful when the real work occurs in a child process that outlives
    ///   the parent.
    void UpdatePID(TPid pid = 0);

private:
    string  m_Path;     //< File path to store PID.
    TPid    m_OldPID;   //< Old PID read from file.
    TPid    m_NewPID;   //< New PID wroted to file.
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2005/02/11 04:39:49  lavr
 * +GetParentPid()
 *
 * Revision 1.11  2004/11/15 22:21:48  grichenk
 * Doxygenized comments, fixed group names.
 *
 * Revision 1.10  2004/09/22 13:32:16  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 * 	CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.9  2004/08/19 12:43:47  dicuccio
 * Dropped unnecessary export specifier
 *
 * Revision 1.8  2004/07/04 19:11:23  vakatov
 * Do not use "throw()" specification after constructors and assignment
 * operators of exception classes inherited from "std::exception" -- as it
 * causes ICC 8.0 generated code to abort in Release mode.
 *
 * Revision 1.7  2004/05/18 16:59:09  ivanov
 * CProcess::
 *     + WaitForAlive(), WaitForTerminate().
 * CPIDGuard::
 *     Fixed CPIDGuard to use reference counters in PID file.
 *     Added CPIDGuard::Remove().
 *
 * Revision 1.6  2004/04/01 14:14:01  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 1.5  2003/12/23 19:05:24  ivanov
 * Get rid of Sun Workshop compilation warning about x_Assign
 *
 * Revision 1.4  2003/12/04 18:45:06  ivanov
 * Added helper constructor for MS Windows to avoid cast from HANDLE to long
 *
 * Revision 1.3  2003/12/03 17:04:00  ivanov
 * Comments changes
 *
 * Revision 1.2  2003/10/01 20:23:58  ivanov
 * Added const specifier to CProcess class member functions
 *
 * Revision 1.1  2003/09/25 16:52:08  ivanov
 * Initial revision. CPIDGuard class moved from ncbi_system.hpp.
 *
 * ===========================================================================
 */

#endif  /* CORELIB__NCBIPROCESS__HPP */
