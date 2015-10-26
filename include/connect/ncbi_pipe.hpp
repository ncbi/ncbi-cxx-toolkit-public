#ifndef CONNECT___NCBI_PIPE__HPP
#define CONNECT___NCBI_PIPE__HPP

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
 * Authors:  Anton Lavrentiev, Vladimir Ivanov
 *
 *
 */

/// @file ncbi_pipe.hpp
/// Portable class to work with process pipes.
///
/// Defines classes: 
///    CPipe - class to work with pipes
///
/// Implemented for: UNIX, MS-Windows

#include <corelib/ncbi_process.hpp>
#include <connect/ncbi_core_cxx.hpp>

#if !defined(NCBI_OS_MSWIN)  &&  !defined(NCBI_OS_UNIX)
#  error "Class CPipe is only supported on Windows and Unix"
#endif


/** @addtogroup Pipes
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Forward declaration.
class CPipeHandle;


/////////////////////////////////////////////////////////////////////////////
///
/// CPipe --
///
/// Launch a child process with pipes connected to its standard I/O.
///
/// A program can read from stdin/stderr and write to stdin of the
/// executed child process using the Read/Write methods of the pipe object.
///
/// @sa
///   CNamedPipe, CExec

class NCBI_XCONNECT_EXPORT CPipe : protected CConnIniter
{
public:
    /// Flags for creating standard I/O handles of child process.
    /// @note@  Flags pertaining to the same stdio handle processed in the
    ///         order of their appearance in the definition below.
    ///
    /// Default is 0:
    ///    fStdIn_Open | fStdOut_Open | fStdErr_Close | fCloseOnClose.
    enum ECreateFlag {
        fStdIn_Open      =     0, ///< Do     open child's stdin (default).
        fStdIn_Close     = 0x001, ///< Do not open child's stdin.
        fStdOut_Open     =     0, ///< Do     open child's stdout (default).
        fStdOut_Close    = 0x002, ///< Do not open child's stdout.
        fStdErr_Open     = 0x004, ///< Do     open child's stderr.
        fStdErr_Close    =     0, ///< Do not open child's stderr (default).
        fStdErr_Share    = 0x008, ///< Keep stderr (share it with child).
        fStdErr_StdOut   = 0x080, ///< Redirect stderr to whatever stdout goes.
        fKeepOnClose     = 0x010, ///< Close(): just return eIO_Timeout
                                  ///< if Close() cannot complete within
                                  ///< the allotted time;  don't close any
                                  ///< pipe handles nor signal the child.
        fCloseOnClose    =     0, ///< Close(): always close all pipe handles
                                  ///< but do not send any signal to running
                                  ///< process if Close()'s timeout expired.
        fKillOnClose     = 0x020, ///< Close(): kill child process if it hasn't
                                  ///< terminated within the allotted time.
                                  ///< NOTE:  If both fKeepOnClose and
                                  ///< fKillOnClose are set, the safer
                                  ///< fKeepOnClose takes the effect.
        fSigPipe_Restore = 0x040, ///< Restore SIGPIPE processing for child
                                  ///< process to system default.
        fNewGroup        = 0x100  ///< UNIX: new process group will be
                                  ///< created and child become the leader
                                  ///< of the new process group.
    };
    typedef unsigned int TCreateFlags;  ///< bitwise OR of "ECreateFlag"

    /// Which of the child I/O handles to use.
    enum EChildIOHandle {
        fStdIn     = (1 << 0),
        fStdOut    = (1 << 1),
        fStdErr    = (1 << 2),
        fDefault   = (1 << 3),
        eStdIn     = fStdIn,
        eStdOut    = fStdOut,
        eStdErr    = fStdErr,
        eDefault   = fDefault   ///< see SetReadHandle()
    };
    typedef unsigned int TChildPollMask;  ///< bit-wise OR of "EChildIOHandle"

    /// Constructor.
    CPipe(size_t pipe_size = 0/*use default*/);

    /// Constructor.
    ///
    /// Call the Open() method to actually open the pipe.
    /// Throw CPipeException on failure to create the pipe.
    ///
    /// @param cmd
    ///   Command name to execute.
    /// @param args
    ///   Vector of string arguments for the command (argv[0] excluded).
    /// @param create_flags
    ///   Specifies the options to be applied when creating the pipe.
    /// @param current_dir
    ///   Current working directory for the new process if specified.
    /// @param env
    ///   Pointer to vector with environment variables which will be used
    ///   instead of current environment.  Last value in an array must be NULL.
    /// @sa
    ///   Open
    CPipe(const string&         cmd,
          const vector<string>& args,
          TCreateFlags          create_flags = 0,
          const string&         current_dir  = kEmptyStr,
          const char* const     env[]        = 0,
          size_t                pipe_size    = 0/*use default*/);

    /// Destructor. 
    ///
    /// If the pipe was opened then the destructor first closes it by calling
    /// Close().
    /// @sa
    ///   Close
    ~CPipe(void);

    /// Open pipe.
    ///
    /// Execute a command with the vector of arguments "args".  The other end
    /// of the pipe is associated with the spawned command's standard
    /// input/output/error according to "create_flags".
    ///
    /// @param cmd
    ///   Command name to execute.
    ///   Note when specifying both "cmd" with relative path and non-empty
    ///   "current_dir":  at run-time the current directory must be considered
    ///   undefined, as it may still be the same of the parent process that
    ///   issues the call, or it can already be changed to the specified
    ///   "current_dir".  So, using the absolute path for "cmd" is always
    ///   recommended in such cases.
    /// @param args
    ///   Vector of string arguments for the command (argv[0] excluded).
    /// @param create_flags
    ///   Specifies options to be applied when creating the pipe.
    /// @param current_dir
    ///   Current working directory for the new process.
    ///   The string must be an absolute path.  On MS Windows it should
    ///   also contain a drive letter. If this parameter is empty, the new
    ///   process will have the same current directory as the calling process.
    /// @param env
    ///   Pointer to vector with environment variables which will be used
    ///   instead of current environment. Last value in an array must be NULL.
    /// @return 
    ///   Completion status.
    /// @sa
    ///   Read, Write, Close
    EIO_Status Open(const string&         cmd,
                    const vector<string>& args,
                    TCreateFlags          create_flags = 0,
                    const string&         current_dir  = kEmptyStr,
                    const char* const     env[]        = 0,
                    size_t                pipe_size    = 0/*use default*/);

    /// Open the standard streams of the current process.
    ///
    /// The standard input stream is opened as if it's the output
    /// stream of a child process, so it can be read from.  Similarly,
    /// the standard output stream is opened as if it's a child input
    /// stream, so it can be written.  The standard error stream is left
    /// untouched.
    ///
    /// @sa
    ///   Read, Write, Close
    void       OpenSelf(void);

    /// Close pipe.
    ///
    /// Sever communication channel with the spawned child process, and then
    /// wait for the process to terminate.
    ///
    /// @note A CPipe opened with OpenSelf() always closes with eIO_Success,
    /// and *exitcode returns as 0 (yet the current process continues to run).
    ///
    /// @param exitcode
    ///   Pointer to store the exit code at, if the child process terminated
    ///   successfully, or -1 in case of an error.  Can be passed as NULL.
    /// @return
    ///   Completion status.
    ///   The returned status eIO_Timeout means that child process is still 
    ///   running (yet the communication with it has been severed).  Any other
    ///   return status means that the pipe is not suitable for further I/O
    ///   until reopened.
    ///
    ///   eIO_Closed  - pipe was already closed;
    ///   eIO_Timeout - the eIO_Close timeout expired, child process is still
    ///                 running (return only if fKeepOnClose was set);
    ///   eIO_Success - pipe was successfully closed.  The running status of
    ///                 the child process depends on the flags:
    ///       fKeepOnClose  - process has terminated with "exitcode";
    ///       fCloseOnClose - process has self-terminated if "exitcode" != -1,
    ///                       or is still running otherwise;
    ///       fKillOnClose  - process has self-terminated if "exitcode" != -1,
    ///                       or has been forcibly terminated otherwise;
    ///   Otherwise   - an error was detected.
    /// @sa
    ///   Open, OpenSelf, fKeepOnClose, fCloseOnClose, fKillOnClose, fNewGroup
    EIO_Status Close(int* exitcode = 0);

    /// Close specified pipe handle (even for CPipe opened with OpenSelf()).
    ///
    /// @param handle
    ///   Pipe handle to close
    /// @return
    ///   Completion status.
    /// @sa
    ///   Close, OpenSelf
    EIO_Status CloseHandle(EChildIOHandle handle);

    /// Set standard output handle to read data from.
    ///
    /// @param from_handle
    ///   Handle which used to read data (eStdOut/eStdErr).
    /// @return
    ///   Return eIO_Success if new handler is eStdOut or eStdErr.
    ///   Return eIO_InvalidArg otherwise.
    /// @sa
    ///   Read
    EIO_Status SetReadHandle(EChildIOHandle from_handle);

    /// Get standard output handle to read data from.
    ///
    /// @return
    ///   Return either eStdOut(default) or eStdErr
    /// @sa
    ///   SetReadHandle
    EChildIOHandle GetReadHandle(void) const { return m_ReadHandle; }

    /// Read data from pipe. 
    ///
    /// @param buf
    ///   Buffer into which data is read.
    /// @param count
    ///   Number of bytes to read.
    /// @param read
    ///   Number of bytes actually read, which may be less than "count". 
    /// @param from_handle
    ///   Handle to read data from.
    /// @return
    ///   Always return eIO_Success if some data were read (regardless of pipe
    ///   conditions that may include EOF/error).
    ///   Return other (error) status only if no data at all could be obtained.
    /// @sa
    ///   Write, SetTimeout
    EIO_Status Read(void*          buf, 
                    size_t         count, 
                    size_t*        read = 0,
                    EChildIOHandle from_handle = eDefault);

    /// Write data to pipe. 
    ///
    /// @param buf
    ///   Buffer from which data is written.
    /// @param count
    ///   Number of bytes to write.
    /// @param written
    ///   Number of bytes actually written, which may be less than "count".
    /// @return
    ///   Return eIO_Success if some data were written.
    ///   Return other (error) code only if no data at all could be written.
    /// @sa
    ///   Read, SetTimeout
    EIO_Status Write(const void* buf,
                     size_t      count,
                     size_t*     written = 0);
                     
    /// Wait for I/O event(s). 
    ///
    /// Block until at least one of the I/O handles enlisted in poll mask
    /// becomes available for I/O, or until timeout expires.
    /// Throw CPipeException on failure to create the pipe.
    /// NOTE: MS Windows doesn't have mechanism to get status of 'write end'
    /// of the pipe, so only fStdOut/fStdErr/fDefault can be used for polling
    /// child stdin/stderr handles. If fStdIn flag is set in the 'mask',
    /// that it will be copied to resulting mask also.
    ///
    /// @param mask
    ///   Mask of I/O handles to poll.
    /// @param timeout
    ///   Timeout value to set.
    ///   If "timeout" is NULL then set the timeout to be infinite.
    /// @return
    ///   Mask of I/O handles that ready for I/O.
    ///   Return zero on timeout or if all I/O handles are closed.
    ///   If fDefault is polled and the corresponding Err/Out is ready
    ///   then return fDefault, and not the "real" fStdOut/fStdErr.
    TChildPollMask Poll(TChildPollMask mask, const STimeout* timeout = 0);

    /// Return a status of the last I/O operation.
    /// 
    /// @param direction
    ///   Direction to get status for.
    /// @return
    ///   I/O status for the specified direction.
    ///   eIO_Closed     - if the pipe is closed;
    ///   eIO_Unknown    - if an error was detected during the last I/O;
    ///   eIO_InvalidArg - if "direction" is not one of:  eIO_Read, eIO_Write;
    ///   eIO_Timeout    - if the timeout was on last I/O;
    ///   eIO_Success    - otherwise.
    /// @sa
    ///   Read, Write
    EIO_Status Status(EIO_Event direction) const;

    /// Specify timeout for the pipe I/O.
    ///
    /// @param event
    ///   I/O event for which the timeout is set.
    /// @param timeout
    ///   Timeout value to set.
    ///   If "timeout" is NULL then set the timeout to be infinite.
    ///   - By default, initially all timeouts are infinite;
    ///   - kDefaultTimeout has no effect.
    /// @return
    ///   Completion status.
    /// @sa
    ///   Read, Write, Close, GetTimeout
    EIO_Status SetTimeout(EIO_Event event, const STimeout* timeout);

    /// Get pipe I/O timeout.
    ///
    /// @param event
    ///   I/O event for which timeout is obtained.
    /// @return
    //    Timeout for specified event (or NULL, if the timeout is infinite).
    ///   The returned timeout is guaranteed to be pointing to a valid
    ///   (and correct) structure in memory at least until the pipe is
    ///   closed or SetTimeout() is called for this pipe.
    /// @sa
    ///   SetTimeout
    const STimeout* GetTimeout(EIO_Event event) const;

    /// Get the process handle for the piped child.
    ///
    /// @return
    ///   Returned value greater than 0 is a child process handle.
    ///   Return 0 if child process is not running.
    /// @sa
    ///   Open, Close, CProcess class
    TProcessHandle GetProcessHandle(void) const;


    /// Callback interface for ExecWait()
    ///
    /// @sa ExecWait
    class NCBI_XCONNECT_EXPORT IProcessWatcher
    {
    public:
        /// An action which the ExecWait() method should take 
        /// after the Watch() method has returned.
        enum EAction {
            eContinue, ///< Continue running
            eStop,     ///< Kill the child process and exit
            eExit      ///< Exit without waiting for the child process
        };
        virtual ~IProcessWatcher();

        /// This method is called when the process has just
        /// been started by the ExecWait() method.
        ///
        /// @param pid
        ///   Process Id to monitor
        /// @return
        ///   eStop if the process should be killed, eContinue otherwise
        virtual EAction OnStart(TProcessHandle /*pid*/) { return eContinue; }

        /// This method is getting called periodically during
        /// the process execution by the ExecWait() method.
        ///
        /// @param pid
        ///   Process Id to monitor
        /// @return
        ///   eStop if the process should be killed, eContinue otherwise
        virtual EAction Watch(TProcessHandle /*pid*/) = 0;
    };

    /// ExecWait return code
    enum EFinish {
        eDone,     ///< Process finished normally
        eCanceled  ///< Watcher requested process termination
    };

    /// Execute a command with a vector of arguments and wait for its
    /// completion.
    /// 
    /// @param cmd
    ///   Command name to execute.
    ///   Beware if both the command contains a relative path and 'current_dir'
    ///   parameter is specified.  In this case, the current directory must
    ///   be considered undefined, as it can either still be the same as in the
    ///   parent process or already be changed to 'current_dir'.  So using of
    ///   an absolute path is always recommended in such cases.
    /// @param args
    ///   Vector of string arguments for the command (argv[0] excluded).
    /// @param in
    ///   Stream this data which will be sent to the child process's stdin
    /// @param out
    ///   Stream where the child process's stdout will be written to
    /// @param err
    ///   Stream where the child process's stderr will be written to
    /// @param exit_code
    ///   The child process's exit_code
    /// @param current_dir
    ///   Current working directory for the new process.
    ///   The string must be an absolute path.  On MS Windows it should
    ///   also contain a drive letter.  If this parameter is empty, the new
    ///   process will have the same current directory as the calling process.
    /// @param env
    ///   Pointer to a vector with environment variables, which will be used
    ///   instead of the current environment.  Last element in the array must
    ///   be NULL.
    /// @param watcher
    ///   Call back object to monitor the child process execution status
    /// @param kill_timeout
    ///   Wait time between first "soft" and second "hard" attempts to
    ///   terminate the process. 
    ///   @note that on UNIX in case of a zero or a very small timeout
    ///   the killed process may not be released immediately and continue to
    ///   persist as a zombie process even after this call completes.
    /// @return 
    ///   eDone if process has finished normally, or eCanceled if a watcher 
    ///   decided to stop it.
    ///
    /// @sa IProcessWatcher
    static EFinish ExecWait(const string&         cmd,
                            const vector<string>& args,
                            CNcbiIstream&         in,
                            CNcbiOstream&         out,
                            CNcbiOstream&         err,
                            int&                  exit_code,
                            const string&         current_dir  = kEmptyStr,
                            const char* const     env[]        = 0,
                            IProcessWatcher*      watcher      = 0,
                            const STimeout*       kill_timeout = 0,
                            size_t                pipe_size    = 0/*default*/);

protected:
    size_t         m_PipeSize;          ///< Pipe size

    CPipeHandle*   m_PipeHandle;        ///< Internal pipe handle that handles
    EChildIOHandle m_ReadHandle;        ///< Default read handle

    // Last I/O status
    EIO_Status     m_ReadStatus;        ///< Last read status
    EIO_Status     m_WriteStatus;       ///< Last write status

    // Timeouts
    STimeout*      m_ReadTimeout;       ///< eIO_Read timeout
    STimeout*      m_WriteTimeout;      ///< eIO_Write timeout
    STimeout*      m_CloseTimeout;      ///< eIO_Close timeout
    STimeout       m_ReadTimeoutValue;  ///< Storage for m_ReadTimeout
    STimeout       m_WriteTimeoutValue; ///< Storage for m_WriteTimeout
    STimeout       m_CloseTimeoutValue; ///< Storage for m_CloseTimeout

private:
    // Disable copy constructor and assignment
    CPipe(const CPipe&);
    CPipe& operator= (const CPipe&);
};



/////////////////////////////////////////////////////////////////////////////
/// CPipeException --
///
/// Define exceptions generated by CPipe.
///
/// CPipeException inherits its basic functionality from CCoreException
/// and defines additional error codes for CPipe.

class NCBI_XCONNECT_EXPORT CPipeException
    : EXCEPTION_VIRTUAL_BASE public CCoreException
{
public:
    /// Error types for pipe exceptions.
    enum EErrCode {
        eOpen ///< Unable to open pipe
    };
    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;
    // Standard exception boiler plate code.
    NCBI_EXCEPTION_DEFAULT(CPipeException, CCoreException);
};


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT__NCBI_PIPE__HPP */
