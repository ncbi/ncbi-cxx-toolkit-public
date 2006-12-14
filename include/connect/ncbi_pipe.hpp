#ifndef CONNECT___NCBI_PIPE__HPP
#define CONNECT___NCBI_PIPE__HPP

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
 * Author:  Anton Lavrentiev, Vladimir Ivanov
 *
 *
 */

/// @file ncbi_pipe.hpp
/// Portable class for work with process pipes.
///
/// Defines classes: 
///     CPipe -  class for work with pipes
///
/// Implemented for: UNIX, MS-Windows


#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbistre.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <vector>

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
/// Launch child process with pipes connected to its standard I/O.
///
/// Program can read from stdin/stderr and write to stdin of the
/// executed child process using pipe object functions Read/Write.
/// Program can read from stdin/stderr and write to stdin of the
/// executed child process using pipe object functions Read/Write.
///
/// @sa
///   CNamedPipe, CExec

class NCBI_XCONNECT_EXPORT CPipe : public virtual CConnIniter
{
public:
    /// Flags for creating standard I/O handles of child process.
    ///
    /// Default is 0 
    ///    fStdIn_Open | fStdOut_Open | fStdErr_Close | fCloseOnClose.
    enum ECreateFlags {
        fStdIn_Open    =    0,  ///< Do     open child's stdin (default).
        fStdIn_Close   = 0x01,  ///< Do not open child's stdin.
        fStdOut_Open   =    0,  ///< Do     open child's stdout (default).
        fStdOut_Close  = 0x02,  ///< Do not open child's stdout.
        fStdErr_Open   = 0x04,  ///< Do     open child's stderr.
        fStdErr_Close  =    0,  ///< Do not open child's stderr (default).
        fKeepOnClose   = 0x08,  ///< Close(): just return eIO_Timeout
                                ///< if Close() cannot complete within
                                ///< the allotted time;  don't close any
                                ///< pipe handles nor signal the child.
        fCloseOnClose  =    0,  ///< Close(): always close all pipe handles
                                ///< but do not send any signal to running
                                ///< process if Close()'s timeout expired.
        fKillOnClose   = 0x10   ///< Close(): kill child process if it hasn't
                                ///< terminated within the allotted time.
                                ///< NOTE:  If both fKeepOnClose and
                                ///< fKillOnClose are set, the safer
                                ///< fKeepOnClose takes the effect.
    };
    typedef unsigned int TCreateFlags;    ///< bit-wise OR of "ECreateFlags"

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
    CPipe();

    /// Constructor.
    ///
    /// Call the Open() method to open a pipe.
    /// Throw CPipeException on failure to create the pipe.
    ///
    /// @param cmd
    ///   Command name to execute.
    /// @param args
    ///   Vector of string arguments for the command (argv[0] excluded).
    /// @param create_flags
    ///   Specifies the options to be applied when creating the pipe.
    /// @current_dir
    ///   Current directory for the new process if specified.
    /// @param env
    ///   Pointer to vector with environment variables which will be used
    ///   instead of current environment. Last value in an array must be NULL.
    /// @sa
    ///   Open
    CPipe(const string&         cmd,
          const vector<string>& args,
          TCreateFlags          create_flags = 0,
          const string&         current_dir  = kEmptyStr,
          const char* const     env[]        = 0);

    /// Destructor. 
    ///
    /// If the pipe was opened then close it by calling Close().
    ~CPipe(void);

    /// Open pipe.
    ///
    /// Create a pipe and execute a command with the vector of arguments
    /// "args". The other end of the pipe is associated with the spawned
    /// command's standard input/output/error according to "create_flags".
    ///
    /// @param cmd
    ///   Command name to execute.
    ///   Be aware if the command contains relative path and 'current_dir'
    ///   parameter is specified. Because on moment of execution the current
    ///   directory is undefined, and can be still the same as in the parent
    ///   process or already be changed to 'current_dir'. So, using absolute
    ///   path is recommended in this case.
    /// @param args
    ///   Vector of string arguments for the command (argv[0] excluded).
    /// @param create_flags
    ///   Specifies options to be applied when creating the pipe.
    /// @current_dir
    ///   Current directory for the new process.
    ///   The string must be an absolute path. On MS Windows it should
    ///   also contains drive letter. If this parameter is empty, the new
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
                    const char* const     env[]        = 0);

    /// Close pipe.
    ///
    /// Wait for the spawned child process to terminate and then close
    /// the associated pipe.
    ///
    /// @param exitcode
    ///   Pointer to store the exit code at, if the child process terminated
    ///   successfully, or -1 in case of an error. Can be passed as NULL.
    /// @return
    ///   Completion status.
    ///   The returned status eIO_Timeout means that child process is still 
    ///   running and the pipe was not yet closed. Any other return status
    ///   means that the pipe is not suitable for further I/O until reopened.
    ///
    ///   eIO_Closed  - pipe was already closed;
    ///   eIO_Timeout - the eIO_Close timeout expired, child process
    ///                 is still running and the pipe has not yet closed
    ///                 (return only if fKeepOnClose create flag was set);
    ///   eIO_Success - pipe was succesfully closed.  The running status of
    ///                 child process depends on the flags:
    ///       fKeepOnClose  - process has terminated with "exitcode";
    ///       fCloseOnClose - process has self-terminated if "exitcode" != -1,
    ///                       or is still running otherwise;
    ///       fKillOnClose  - process has self-terminated if "exitcode" != -1,
    ///                       or has been forcibly terminated otherwise;
    ///   Otherwise   - an error was detected;
    /// @sa
    ///   Description for flags fKeepOnClose, fCloseOnClose, fKillOnClose.
    ///   Open
    EIO_Status Close(int* exitcode = 0);

    /// Close specified pipe handle.
    ///
    /// @param handle
    ///   Pipe handle to close
    /// @return
    ///   Completion status.
    /// @sa
    ///   Close
    EIO_Status CloseHandle(EChildIOHandle handle);

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

    /// Set standard output handle to read data from.
    ///
    /// @from_handle
    ///   Handle which used to read data (eStdOut/eStdErr).
    /// @return
    ///   Return eIO_Success if new handler is eStdOut or eStdErr.
    ///   Return eIO_Unknown otherwise.
    /// @sa
    ///   Read
    EIO_Status SetReadHandle(EChildIOHandle from_handle);

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

    class NCBI_XCONNECT_EXPORT IProcessWatcher 
    {
    public:
        enum EAction {
            eContinue,
            eStop
        };
        virtual ~IProcessWatcher();
        virtual EAction Watch(TProcessHandle) = 0;
    };

    enum EFinish {
        eDone,
        eCanceled
    };
    static EFinish ExecWait(const string&         cmd,
                            const vector<string>& args,
                            CNcbiIstream&         in,
                            CNcbiOstream&         out,
                            CNcbiOstream&         err,
                            int&                  exit_value,
                            const string&         current_dir  = kEmptyStr,
                            const char* const     env[]        = 0,
                            IProcessWatcher*      watcher      = 0);
                        

protected:
    CPipeHandle*   m_PipeHandle;        ///< Internal pipe handle that handles
    EChildIOHandle m_ReadHandle;        ///< Default read handle

    // Last IO status
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
    /// Disable copy constructor and assignment.
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
        eInit,      ///< Pipe initialization error
        eOpen,      ///< Unable to open pipe (from constructor)
        eSetBuf     ///< setbuf() not permitted
    };

    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;
    // Standard exception boiler plate code.
    NCBI_EXCEPTION_DEFAULT(CPipeException,CCoreException);
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.35  2006/12/14 04:43:17  lavr
 * Derive from CConnIniter for auto-magical init (former CONNECT_InitInternal)
 *
 * Revision 1.34  2006/12/04 14:53:43  gouriano
 * Moved GetErrCodeString method into src
 *
 * Revision 1.33  2006/09/06 16:55:52  didenko
 * Renamed CPipe::ICallBack to CPipe::IProcessWatcher
 *
 * Revision 1.32  2006/09/05 16:53:13  didenko
 * Fix compile time error on Windows and Sun
 *
 * Revision 1.31  2006/09/05 14:33:10  didenko
 * Added ExecWait static method
 *
 * Revision 1.30  2006/05/24 14:14:23  ivanov
 * CPipe::Open - added more comments
 *
 * Revision 1.29  2006/05/23 15:39:52  ivanov
 * Set default value for current directory to kEmptyStr
 *
 * Revision 1.28  2006/05/23 14:55:40  ivanov
 * CPipe::Open() - use string instead of char* for current directory
 *
 * Revision 1.27  2006/05/23 11:15:52  ivanov
 * Added possibility to specify a current working directory and
 * environment for created child processes.
 *
 * Revision 1.26  2006/03/08 14:37:01  ivanov
 * CPipe::Poll() -- added implementation for Windows, added some comments
 *
 * Revision 1.25  2006/03/03 03:18:43  lavr
 * Rectify usage and effects of OnClose-flags
 *
 * Revision 1.24  2006/03/01 17:01:03  ivanov
 * + CPipe::Poll() -- Unix version only
 *
 * Revision 1.23  2005/05/02 16:01:36  lavr
 * Proper exception class derivation
 *
 * Revision 1.22  2005/03/17 18:01:08  lavr
 * Clarify about argv[0] not being part of args
 *
 * Revision 1.21  2004/08/19 12:43:30  dicuccio
 * Dropped unnecessary export specifier
 *
 * Revision 1.20  2003/12/04 16:29:50  ivanov
 * Added new create flags: fKeepOnClose, fCloseOnClose, fKillOnClose.
 * Added GetProcessHandle(). Comments changes.
 *
 * Revision 1.19  2003/11/13 17:51:47  lavr
 * -<stdio.h>
 *
 * Revision 1.18  2003/11/12 16:34:48  ivanov
 * Removed references to CPipeIOStream from  comments
 *
 * Revision 1.17  2003/09/23 21:03:06  lavr
 * PipeStreambuf and special stream removed: now all in ncbi_conn_stream.hpp
 *
 * Revision 1.16  2003/09/16 14:55:49  ivanov
 * Removed extra comma in the enum definition
 *
 * Revision 1.15  2003/09/09 19:23:16  ivanov
 * Comment changes
 *
 * Revision 1.14  2003/09/02 20:46:59  lavr
 * -<connect/connect_export.h> -- included from <connect/ncbi_core.h>
 *
 * Revision 1.13  2003/09/02 20:24:32  ivanov
 * Moved ncbipipe to CONNECT library from CORELIB.
 * Rewritten CPipe class using I/O timeouts.
 *
 * Revision 1.12  2003/08/26 14:06:51  siyan
 * Minor doc fixes.
 *
 * Revision 1.11  2003/08/24 22:53:03  siyan
 * Added documentation.
 *
 * Revision 1.10  2003/04/23 20:50:27  ivanov
 * Added CPipe::CloseHandle()
 *
 * Revision 1.9  2003/04/01 14:20:13  siyan
 * Added doxygen support
 *
 * Revision 1.8  2003/03/03 14:46:02  dicuccio
 * Reimplemented CPipe using separate private platform-specific implementations
 *
 * Revision 1.7  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.6  2002/07/15 18:17:52  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.5  2002/07/11 14:17:55  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.4  2002/06/12 19:38:45  ucko
 * Remove a superfluous comma from the definition of EMode.
 *
 * Revision 1.3  2002/06/11 19:25:06  ivanov
 * Added class CPipeIOStream
 *
 * Revision 1.2  2002/06/10 18:35:13  ivanov
 * Changed argument's type of a running child program from char*[]
 * to vector<string>
 *
 * Revision 1.1  2002/06/10 16:57:04  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CONNECT__NCBI_PIPE__HPP */
