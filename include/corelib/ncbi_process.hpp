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

/// @file ncbip_rocess.hpp 
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

    /// Get process identifier for a current running process.
    static TPid GetCurrentPid(void);

    /// Check process existence.
    ///
    /// @return
    ///   TRUE  - if the process is still running.
    ///   FALSE - if the process did not exist or was already terminated.
    bool IsAlive(void);

    /// Terminate process.
    ///
    /// @timeout
    ///   Wait time in milliseconds between first "soft" and second "hard"
    //    attempts to terminate the process.
    /// @return
    ///   TRUE  - if the process did not exist or was successfully terminated.
    ///   FALSE - if the process is still running and cannot be terminated.
    bool Kill(unsigned long timeout = kDefaultKillTimeout);

    /// Wait until process terminates.
    ///
    /// Wait until the process has terminates or timeout expired.
    //  Return immediately if specifed process has already terminated.
    /// @param timeout
    ///   Time-out interval. By default it is infinite.
    /// @return
    ///   - Exit code of the process, if no errors.
    ///   - (-1), if error has occured.
    int Wait(unsigned long timeout = kMax_ULong);

private:
    long          m_Process;   ///< Process identifier.
    EProcessType  m_Type;      ///< Type of process identifier.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CPIDGuard --
///

class NCBI_XNCBI_EXPORT CPIDGuardException
    : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eStillRunning, ///< The process listed in the file is still around.
        eCouldntOpen   ///< Unable to open the PID file for writing.
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
    TPid GetPID(void) const throw()
        { return m_PID; }

protected:
    void x_Assign(const CPIDGuardException& src)
        { CException::x_Assign(src);  m_PID = src.m_PID; }

private:
    TPid  m_PID;
};


class NCBI_XNCBI_EXPORT CPIDGuard
{
public:
    /// If "filename" contains no path, make it relative to "dir"
    /// (which defaults to /tmp on Unix and %HOME% on Windows).
    /// If the file already exists and identifies a live process,
    /// throws CPIDGuardException.
    CPIDGuard(const string& filename, const string& dir = kEmptyStr);

    ~CPIDGuard(void) { Release(); }

    /// Returns non-zero if there was a stale file.
    TPid GetOldPID(void) { return m_OldPID; }

    /// Removes the file.
    void Release(void);

    /// @param pid
    ///   The new process ID to store (defaults to the current PID);
    ///   useful when the real work occurs in a child process that outlives
    ///   the parent.
    void UpdatePID(TPid pid = 0);

private:
    string  m_Path;
    TPid    m_OldPID;
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/09/25 16:52:08  ivanov
 * Initial revision. CPIDGuard class moved from ncbi_system.hpp.
 *
 * ===========================================================================
 */

#endif  /* CORELIB__NCBIPROCESS__HPP */
