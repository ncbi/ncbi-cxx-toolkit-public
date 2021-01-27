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
 * Authors:  Anton Lavrentiev, Mike DiCuccio, Vladimir Ivanov
 *
 * File Description:
 *   Inter-process pipe with a spawned process.
 *
 */

#include <ncbi_pch.hpp>
/* Cancel __wur (warn unused result) ill effects in GCC */
#ifdef   _FORTIFY_SOURCE
#  undef _FORTIFY_SOURCE
#endif /*_FORTIFY_SOURCE*/
#define  _FORTIFY_SOURCE 0
#include "ncbi_priv.h"
#include <connect/error_codes.hpp>
#include <corelib/ncbi_param.hpp>
#include <connect/ncbi_pipe.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/stream_utils.hpp>

#ifdef NCBI_OS_MSWIN

#  include <windows.h>
#  include <corelib/ncbiexec.hpp>

#elif defined NCBI_OS_UNIX

#  include <errno.h>
#  include <fcntl.h>
#  include <poll.h>
#  include <signal.h>
#  include <unistd.h>
#  include <sys/time.h>
#  include <sys/types.h>
#  include <sys/wait.h>

#  define _STR(s)  #s
#  define  STR(s)  _STR(s)

#else
#  error "The CPipe class is supported only on Windows and Unix"
#endif

#define NCBI_USE_ERRCODE_X   Connect_Pipe


#define IS_SET(flags, mask)  (((flags) & (mask)) == (mask))


#define PIPE_THROW(err, errtxt)                     \
    THROW0_TRACE(x_FormatError(int(err), errtxt))


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// Auxiliary functions
//

static const STimeout* s_SetTimeout(const STimeout* from, STimeout* to)
{
    if ( !from ) {
        return kInfiniteTimeout;
    }
    to->sec  = from->usec / kMicroSecondsPerSecond+ from->sec;
    to->usec = from->usec % kMicroSecondsPerSecond;
    return to;
}


typedef AutoPtr< char, CDeleter<char> >  TTempCharPtr;


static string x_FormatError(int error, const string& message)
{
    const char* errstr;

#ifdef NCBI_OS_MSWIN
    string errmsg;
    if ( error ) {
        TCHAR* tmpstr = NULL;
        DWORD rv = ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                                   FORMAT_MESSAGE_FROM_SYSTEM     |
                                   FORMAT_MESSAGE_MAX_WIDTH_MASK  |
                                   FORMAT_MESSAGE_IGNORE_INSERTS,
                                   NULL, (DWORD) error,
                                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                   (LPTSTR) &tmpstr, 0, NULL);
        if (rv  &&  tmpstr) {
            errmsg = _T_CSTRING(tmpstr);
            errstr = errmsg.c_str();
        } else {
            errstr = "";
        }
        if ( tmpstr ) {
            ::LocalFree((HLOCAL) tmpstr);
        }
    } else
#endif /*NCBI_OS_MSWIN*/
        errstr = 0;

    int dynamic = 0/*false*/;
    const char* result = ::NcbiMessagePlusError(&dynamic, message.c_str(),
                                                error, errstr);
    TTempCharPtr retval(const_cast<char*> (result),
                        dynamic ? eTakeOwnership : eNoOwnership);
    return retval.get() ? retval.get() : message;
}


static string s_FormatErrorMessage(const string& where, const string& what)
{
    return "[CPipe::" + where + "]  " + what;
}


static EIO_Status s_Close(CProcess& process, CPipe::TCreateFlags flags,
                          const STimeout* timeout, int* exitcode)
{
    CProcess::CExitInfo exitinfo;
    int x_exitcode = process.Wait(NcbiTimeoutToMs(timeout), &exitinfo);

    EIO_Status status;
    if (x_exitcode < 0) {
        if ( !exitinfo.IsPresent() ) {
            status = eIO_Unknown;
        } else if ( !exitinfo.IsAlive() ) {
            status = eIO_Unknown;
#ifdef NCBI_OS_UNIX
            if ( exitinfo.IsSignaled() ) {
                x_exitcode = -(exitinfo.GetSignal() + 1000);
            }
#endif // NCBI_OS_UNIX
        } else {
            status = eIO_Timeout;
            if ( !IS_SET(flags, CPipe::fKeepOnClose) ) {
                if ( IS_SET(flags, CPipe::fKillOnClose) ) {
                    unsigned long x_timeout;
                    if (!timeout  ||  (timeout->sec | timeout->usec)) {
                        x_timeout = CProcess::kDefaultKillTimeout;
                    } else {
                        x_timeout = 0/*fast but unsafe*/;
                    }
                    bool killed;
                    if ( IS_SET(flags, CPipe::fNewGroup) ) {
                        killed = process.KillGroup(x_timeout);
                    } else {
                        killed = process.Kill     (x_timeout);
                    }
                    status = killed ? eIO_Success : eIO_Unknown;
                } else {
                    status = eIO_Success;
                }
            }
        }
    } else {
        _ASSERT(exitinfo.IsPresent());
        _ASSERT(exitinfo.IsExited());
        _ASSERT(exitinfo.GetExitCode() == x_exitcode);
        status = eIO_Success;
    }

    if ( exitcode ) {
        *exitcode = x_exitcode;
    }
    return status;
}


static string x_GetHandleName(CPipe::EChildIOHandle handle)
{
    switch (handle) {
    case CPipe::eStdIn:
        return "eStdIn";
    case CPipe::eStdOut:
        return "eStdOut";
    case CPipe::eStdErr:
        return "eStdErr";
    default:
        _TROUBLE;
        break;
    }
    return "<Invalid handle " + NStr::NumericToString(int(handle)) + '>';
}



//////////////////////////////////////////////////////////////////////////////
//
// Class CPipeHandle handles forwarded requests from CPipe.
// This class is reimplemented in a platform-specific fashion where needed.
//

#if defined(NCBI_OS_MSWIN)


const unsigned long kWaitPrecision = 100;  // Timeout time slice (milliseconds)


static inline bool x_IsDisconnectError(DWORD error)
{
    return (error == ERROR_NO_DATA      ||
            error == ERROR_BROKEN_PIPE  ||
            error == ERROR_PIPE_NOT_CONNECTED ? true : false);
}


//////////////////////////////////////////////////////////////////////////////
//
// CPipeHandle -- MS Windows version
//

class CPipeHandle
{
public:
    CPipeHandle(void);
    ~CPipeHandle();
    EIO_Status Open(const string& cmd, const vector<string>& args,
                    CPipe::TCreateFlags create_flags,
                    const string&       current_dir,
                    const char* const   env[],
                    size_t              pipe_size);
    void       OpenSelf(void);
    EIO_Status Close(int* exitcode, const STimeout* timeout);
    EIO_Status CloseHandle(CPipe::EChildIOHandle handle);
    EIO_Status Read(void* buf, size_t count, size_t* n_read,
                    const CPipe::EChildIOHandle from_handle,
                    const STimeout* timeout) const;
    EIO_Status Write(const void* buf, size_t count, size_t* written,
                     const STimeout* timeout) const;
    CPipe::TChildPollMask Poll(CPipe::TChildPollMask mask,
                               const STimeout* timeout) const;
    TProcessHandle GetProcessHandle(void) const { return m_ProcHandle; }
    void Release(void) { x_Clear(); }

private:
    // Clear object state.
    void   x_Clear(void);
    // Get child's I/O handle.
    HANDLE x_GetHandle(CPipe::EChildIOHandle from_handle) const;
    // Trigger blocking mode on specified I/O handle.
    void   x_SetNonBlockingMode(HANDLE fd) const;
    // Wait on the file descriptors for I/O.
    CPipe::TChildPollMask x_Poll(CPipe::TChildPollMask mask,
                                 const STimeout* timeout) const;
private:
    // I/O handles for child process.
    HANDLE m_ChildStdIn;
    HANDLE m_ChildStdOut;
    HANDLE m_ChildStdErr;

    // Child process descriptor.
    HANDLE m_ProcHandle;

    // Pipe flags
    CPipe::TCreateFlags m_Flags;

    // Flag that indicates whether the m_ChildStd* and m_ProcHandle
    // member variables contain the relevant handles of the
    // current process, in which case they won't be closed.
    bool m_SelfHandles;
};


CPipeHandle::CPipeHandle(void)
    : m_ChildStdIn (INVALID_HANDLE_VALUE),
      m_ChildStdOut(INVALID_HANDLE_VALUE),
      m_ChildStdErr(INVALID_HANDLE_VALUE),
      m_ProcHandle (INVALID_HANDLE_VALUE),
      m_Flags(0), m_SelfHandles(false)
{
    return;
}


CPipeHandle::~CPipeHandle()
{
    static const STimeout kZeroTimeout = {0, 0};
    Close(0, &kZeroTimeout);
    x_Clear();
}


EIO_Status CPipeHandle::Open(const string&         cmd,
                             const vector<string>& args,
                             CPipe::TCreateFlags   create_flags,
                             const string&         current_dir,
                             const char* const     env[],
                             size_t                pipe_size)
{
    DEFINE_STATIC_FAST_MUTEX(s_Mutex);
    CFastMutexGuard guard_mutex(s_Mutex);

    if (m_SelfHandles  ||  m_ProcHandle != INVALID_HANDLE_VALUE) {
        ERR_POST_X(1, s_FormatErrorMessage("Open", "Pipe busy"));
        return eIO_Unknown;
    }
    m_Flags = create_flags;

    HANDLE child_stdin  = INVALID_HANDLE_VALUE;
    HANDLE child_stdout = INVALID_HANDLE_VALUE;
    HANDLE child_stderr = INVALID_HANDLE_VALUE;

    EIO_Status status = eIO_Unknown;

    try {
        // Prepare command line to run
        string cmd_line(cmd);
        for (auto arg : args) {
            // Add argument to command line.
            // Escape it with quotes if necessary.
            if ( !cmd_line.empty() ) {
                cmd_line += ' ';
            }
            cmd_line += CExec::QuoteArg(arg);
        }

        // Convert environment array to block form
        AutoPtr< TXChar, ArrayDeleter<TXChar> > env_block;
        if ( env ) {
            // Count block size:
            // it should have one zero char at least
            size_t size = 1; 
            int    count = 0;
            while ( env[count] ) {
                size += strlen(env[count++]) + 1/*'\0'*/;
            }
            // Allocate memory
            TXChar* block = new TXChar[size];
            env_block.reset(block);

            // Copy environment strings
            for (int i = 0;  i < count;  ++i) {
#if defined(NCBI_OS_MSWIN)  &&  defined(_UNICODE)
                TXString tmp = _T_XSTRING(env[i]);
                size_t n = tmp.size();
                memcpy(block, tmp.data(), n * sizeof(TXChar));
                block[n++] = _TX('\0');
#else
                size_t n = strlen(env[i]) + 1;
                memcpy(block, env[i], n);
#endif // NCBI_OS_MSWIN && _UNICODE
                block += n;
            }
            *block = _TX('\0');
        }

        HANDLE stdout_handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdout_handle == NULL) {
            stdout_handle  = INVALID_HANDLE_VALUE;
        }
        HANDLE stderr_handle = ::GetStdHandle(STD_ERROR_HANDLE);
        if (stderr_handle == NULL) {
            stderr_handle  = INVALID_HANDLE_VALUE;
        }
        
        // Flush stdio buffers before remap
        NcbiCout.flush();
        NcbiCerr.flush();
        ::fflush(NULL);
        if (stdout_handle != INVALID_HANDLE_VALUE) {
            ::FlushFileBuffers(stdout_handle);
        }
        if (stderr_handle != INVALID_HANDLE_VALUE) {
            ::FlushFileBuffers(stderr_handle);
        }

        // Set base security attributes
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.bInheritHandle = TRUE;
        attr.lpSecurityDescriptor = NULL;

        // Create pipe for child's stdin
        _ASSERT(CPipe::fStdIn_Close != 0);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            if ( !::CreatePipe(&child_stdin, &m_ChildStdIn,
                               &attr, (DWORD) pipe_size) ) {
                PIPE_THROW(::GetLastError(),
                           "Failed CreatePipe(stdin)");
            }
            ::SetHandleInformation(m_ChildStdIn, HANDLE_FLAG_INHERIT, 0);
            x_SetNonBlockingMode(m_ChildStdIn);
        }

        // Create pipe for child's stdout
        _ASSERT(CPipe::fStdOut_Close != 0);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            if ( !::CreatePipe(&m_ChildStdOut, &child_stdout, &attr, 0)) {
                PIPE_THROW(::GetLastError(),
                           "Failed CreatePipe(stdout)");
            }
            ::SetHandleInformation(m_ChildStdOut, HANDLE_FLAG_INHERIT, 0);
            x_SetNonBlockingMode(m_ChildStdOut);
        }

        // Create pipe for child's stderr
        _ASSERT(CPipe::fStdErr_Open != 0);
        if        ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            if ( !::CreatePipe(&m_ChildStdErr, &child_stderr, &attr, 0)) {
                PIPE_THROW(::GetLastError(),
                           "Failed CreatePipe(stderr)");
            }
            ::SetHandleInformation(m_ChildStdErr, HANDLE_FLAG_INHERIT, 0);
            x_SetNonBlockingMode(m_ChildStdErr);
        } else if ( IS_SET(create_flags, CPipe::fStdErr_Share) ) {
            if (stderr_handle != INVALID_HANDLE_VALUE) {
                HANDLE current_process = ::GetCurrentProcess();
                if ( !::DuplicateHandle(current_process, stderr_handle,
                                        current_process, &child_stderr,
                                        0, TRUE, DUPLICATE_SAME_ACCESS)) {
                    PIPE_THROW(::GetLastError(),
                               "Failed DuplicateHandle(stderr)");
                }
            }
        } else if ( IS_SET(create_flags, CPipe::fStdErr_StdOut) ) {
            child_stderr = child_stdout;
        }

        // Create child process
        STARTUPINFO sinfo;
        PROCESS_INFORMATION pinfo;
        ::ZeroMemory(&pinfo, sizeof(pinfo));
        ::ZeroMemory(&sinfo, sizeof(sinfo));
        sinfo.cb = sizeof(sinfo);
        sinfo.hStdError  = child_stderr;
        sinfo.hStdOutput = child_stdout;
        sinfo.hStdInput  = child_stdin;
        sinfo.dwFlags    = STARTF_USESTDHANDLES;
#  if defined(_UNICODE)
        DWORD dwCreationFlags = CREATE_UNICODE_ENVIRONMENT;
#  else
        DWORD dwCreationFlags = 0;
#  endif // _UNICODE
        if ( !::CreateProcess(NULL,
                              (LPTSTR)(_T_XCSTRING(cmd_line)),
                              NULL, NULL, TRUE,
                              dwCreationFlags, env_block.get(),
                              current_dir.empty()
                              ? 0 : _T_XCSTRING(current_dir),
                              &sinfo, &pinfo) ) {
            status = eIO_Closed;
            PIPE_THROW(::GetLastError(),
                       "Failed CreateProcess(\"" + cmd_line + "\")");
        }
        ::CloseHandle(pinfo.hThread);
        m_ProcHandle = pinfo.hProcess;

        _ASSERT(m_ProcHandle != INVALID_HANDLE_VALUE);

        status = eIO_Success;
    }
    catch (string& what) {
        static const STimeout kZeroZimeout = {0, 0};
        Close(0, &kZeroZimeout);
        ERR_POST_X(1, s_FormatErrorMessage("Open", what));
        x_Clear();
    }
    if (child_stdin  != INVALID_HANDLE_VALUE) {
        ::CloseHandle(child_stdin);
    }
    if (child_stdout != INVALID_HANDLE_VALUE) {
        ::CloseHandle(child_stdout);
    }
    if (child_stderr != INVALID_HANDLE_VALUE
        &&  child_stderr != child_stdout) {
        ::CloseHandle(child_stderr);
    }

    return status;
}


void CPipeHandle::OpenSelf(void)
{
    if (m_SelfHandles  ||  m_ProcHandle != INVALID_HANDLE_VALUE) {
        PIPE_THROW(0,
                   "Pipe busy");
    }

    NcbiCout.flush();
    ::fflush(stdout);
    if ( m_ChildStdIn != INVALID_HANDLE_VALUE
         &&  !::FlushFileBuffers(m_ChildStdIn) ) {
        PIPE_THROW(::GetLastError(),
                   "Failed FlushFileBuffers(stdout)");
    }
    if ((m_ChildStdIn = ::GetStdHandle(STD_OUTPUT_HANDLE))
        == INVALID_HANDLE_VALUE) {
        PIPE_THROW(::GetLastError(),
                   "Failed GetStdHandle(stdout)");
    }
    if ((m_ChildStdOut = ::GetStdHandle(STD_INPUT_HANDLE))
        == INVALID_HANDLE_VALUE) {
        PIPE_THROW(::GetLastError(),
                   "Failed GetStdHandle(stdin)");
    }
    // NB: GetCurrentProcess() returns HANDLE(-1) which is INVALID_HANDLE_VALUE
    m_ProcHandle = GetCurrentProcess();

    m_SelfHandles = true;
}


void CPipeHandle::x_Clear(void)
{
    m_ProcHandle = INVALID_HANDLE_VALUE;
    if (m_SelfHandles) {
        m_ChildStdIn  = INVALID_HANDLE_VALUE;
        m_ChildStdOut = INVALID_HANDLE_VALUE;
        m_SelfHandles = false;
    } else {
        CloseHandle(CPipe::eStdIn);
        CloseHandle(CPipe::eStdOut);
        CloseHandle(CPipe::eStdErr);
    }
}


EIO_Status CPipeHandle::Close(int* exitcode, const STimeout* timeout)
{
    EIO_Status status;

    if ( !m_SelfHandles ) {
        CloseHandle(CPipe::eStdIn);
        CloseHandle(CPipe::eStdOut);
        CloseHandle(CPipe::eStdErr);

        if (m_ProcHandle == INVALID_HANDLE_VALUE) {
            if ( exitcode ) {
                *exitcode = -1;
            }
            status = eIO_Closed;
        } else {
            CProcess process(m_ProcHandle, CProcess::eHandle);
            status = s_Close(process, m_Flags, timeout, exitcode);
        }
    } else {
        if ( exitcode ) {
            *exitcode = 0;
        }
        status = eIO_Success;
    }

    if (status != eIO_Timeout) {
        x_Clear();
    }
    return status;
}


EIO_Status CPipeHandle::CloseHandle(CPipe::EChildIOHandle handle)
{
    switch (handle) {
    case CPipe::eStdIn:
        if (m_ChildStdIn == INVALID_HANDLE_VALUE) {
            return eIO_Closed;
        }
        ::CloseHandle(m_ChildStdIn);
        m_ChildStdIn = INVALID_HANDLE_VALUE;
        break;
    case CPipe::eStdOut:
        if (m_ChildStdOut == INVALID_HANDLE_VALUE) {
            return eIO_Closed;
        }
        ::CloseHandle(m_ChildStdOut);
        m_ChildStdOut = INVALID_HANDLE_VALUE;
        break;
    case CPipe::eStdErr:
        if (m_ChildStdErr == INVALID_HANDLE_VALUE) {
            return eIO_Closed;
        }
        ::CloseHandle(m_ChildStdErr);
        m_ChildStdErr = INVALID_HANDLE_VALUE;
        break;
    default:
        _TROUBLE;
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


EIO_Status CPipeHandle::Read(void* buf, size_t count, size_t* n_read, 
                             const CPipe::EChildIOHandle from_handle,
                             const STimeout* timeout) const
{
    _ASSERT(!n_read  ||  !*n_read);
    _ASSERT(!(from_handle & (from_handle - 1)));

    EIO_Status status = eIO_Unknown;

    try {
        if (!m_SelfHandles  &&  m_ProcHandle == INVALID_HANDLE_VALUE) {
            PIPE_THROW(0,
                       "Pipe closed");
        }
        HANDLE fd = x_GetHandle(from_handle);
        if (fd == INVALID_HANDLE_VALUE) {
            PIPE_THROW(0,
                       "Pipe I/O handle "
                       + x_GetHandleName(from_handle) + " closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout   = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;
        DWORD bytes_avail = 0;

        // Wait for data from the pipe with a timeout.
        // Using a loop and periodically try PeekNamedPipe is inefficient,
        // but Windows doesn't have asynchronous mechanism to read from a pipe.
        // NOTE:  WaitForSingleObject() doesn't work with anonymous pipes.
        // See CPipe::Poll() for more details.

        unsigned long x_sleep = 1;
        for (;;) {
            BOOL ok = ::PeekNamedPipe(fd, NULL, 0, NULL, &bytes_avail, NULL);
            if ( bytes_avail ) {
                break;
            }
            if ( !ok ) {
                // Has peer closed the connection?
                DWORD error = ::GetLastError();
                if ( !x_IsDisconnectError(error) ) {
                    // NB: status == eIO_Unknown
                    PIPE_THROW(error,
                               "Failed PeekNamedPipe("
                               + x_GetHandleName(from_handle) + ')');
                }
                return eIO_Closed;
            }

            if ( !x_timeout ) {
                return eIO_Timeout;
            }
            if (x_timeout != INFINITE) {
                if (x_sleep > x_timeout) {
                    x_sleep = x_timeout;
                }
                x_timeout -= x_sleep;
            }
            SleepMilliSec(x_sleep);
            x_sleep <<= 1;
            if (x_sleep > kWaitPrecision) {
                x_sleep = kWaitPrecision;
            }
        }
        _ASSERT(bytes_avail);

        // We must read only "count" bytes of data regardless of
        // the amount available to read
        if (bytes_avail >         count) {
            bytes_avail = (DWORD) count;
        }
        BOOL ok = ::ReadFile(fd, buf, bytes_avail, &bytes_avail, NULL);
        if ( !bytes_avail ) {
            // NB: status == eIO_Unknown
            PIPE_THROW(!ok ? ::GetLastError() : 0,
                       "Failed to read data from pipe I/O handle "
                       + x_GetHandleName(from_handle));
        } else {
            if ( n_read ) {
                *n_read = (size_t) bytes_avail;
            }
            status = eIO_Success;
        }
    }
    catch (string& what) {
        ERR_POST_X(2, s_FormatErrorMessage("Read", what));
    }

    return status;
}


EIO_Status CPipeHandle::Write(const void* buf, size_t count,
                              size_t* n_written, const STimeout* timeout) const

{
    _ASSERT(!n_written  ||  !*n_written);

    EIO_Status status = eIO_Unknown;

    try {
        if (!m_SelfHandles  &&  m_ProcHandle == INVALID_HANDLE_VALUE) {
            PIPE_THROW(0,
                       "Pipe closed");
        }
        if (m_ChildStdIn == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            PIPE_THROW(0,
                       "Pipe I/O handle "
                       + x_GetHandleName(CPipe::eStdIn) + " closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout     = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;
        DWORD to_write      = (count > numeric_limits<DWORD>::max()
                               ? numeric_limits<DWORD>::max()
                               : (DWORD) count);
        DWORD bytes_written = 0;

        unsigned long x_sleep = 1;
        for (;;) {
            BOOL ok = ::WriteFile(m_ChildStdIn, (char*) buf, to_write,
                                  &bytes_written, NULL);
            if ( bytes_written ) {
                break;
            }
            if ( !ok ) {
                DWORD error = ::GetLastError();
                if ( x_IsDisconnectError(error) ) {
                    status = eIO_Closed;
                } // NB: status == eIO_Unknown
                PIPE_THROW(error,
                           "Failed to write data to pipe I/O handle "
                           + x_GetHandleName(CPipe::eStdIn));
            }

            if ( !x_timeout ) {
                return eIO_Timeout;
            }
            if (x_timeout != INFINITE) {
                if (x_sleep > x_timeout) {
                    x_sleep = x_timeout;
                }
                x_timeout -= x_sleep;
            }
            SleepMilliSec(x_sleep);
            x_sleep <<= 1;
            if (x_sleep > kWaitPrecision) {
                x_sleep = kWaitPrecision;
            }
        }
        _ASSERT(bytes_written);

        if ( n_written ) {
            *n_written = bytes_written;
        }
        status = eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(3, s_FormatErrorMessage("Write", what));
    }

    return status;
}


CPipe::TChildPollMask CPipeHandle::Poll(CPipe::TChildPollMask mask,
                                        const STimeout* timeout) const
{
    CPipe::TChildPollMask poll = 0;

    try {
        if (!m_SelfHandles  &&  m_ProcHandle == INVALID_HANDLE_VALUE) {
            PIPE_THROW(0,
                       "Pipe closed");
        }
        if (m_ChildStdIn  == INVALID_HANDLE_VALUE  &&
            m_ChildStdOut == INVALID_HANDLE_VALUE  &&
            m_ChildStdErr == INVALID_HANDLE_VALUE) {
            PIPE_THROW(0,
                       "All pipe I/O handles closed");
        }
        poll = x_Poll(mask, timeout);
    }
    catch (string& what) {
        ERR_POST_X(4, s_FormatErrorMessage("Poll", what));
    }

    return poll;
}


HANDLE CPipeHandle::x_GetHandle(CPipe::EChildIOHandle from_handle) const
{
    switch (from_handle) {
    case CPipe::eStdIn:
        return m_ChildStdIn;
    case CPipe::eStdOut:
        return m_ChildStdOut;
    case CPipe::eStdErr:
        return m_ChildStdErr;
    default:
        _TROUBLE;
        break;
    }
    return INVALID_HANDLE_VALUE;
}


void CPipeHandle::x_SetNonBlockingMode(HANDLE fd) const
{
    // NB: Pipe is in byte-mode.
    // NOTE: We cannot get a state of a pipe handle opened for writing.
    //       We cannot set a state of a pipe handle opened for reading.
    DWORD state = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    if ( !::SetNamedPipeHandleState(fd, &state, NULL, NULL) ) {
        PIPE_THROW(::GetLastError(),
                   "Failed to set pipe I/O handle non-blocking");
    }
}


CPipe::TChildPollMask CPipeHandle::x_Poll(CPipe::TChildPollMask mask,
                                          const STimeout* timeout) const
{
    DWORD x_timeout = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;

    // We cannot poll child's stdin, so just copy corresponding flag
    CPipe::TChildPollMask poll = mask & CPipe::fStdIn;

    // Wait for data from the pipe with timeout.
    // Using a loop and periodically try PeekNamedPipe is inefficient,
    // but Windows doesn't have asynchronous mechanism to read from a pipe.
    // NOTE: WaitForSingleObject() doesn't work with anonymous pipes.

    unsigned long x_sleep = 1;
    for (;;) {
        if ( (mask & CPipe::fStdOut)
             &&  m_ChildStdOut != INVALID_HANDLE_VALUE ) {
            DWORD bytes_avail = 0;
            if ( !::PeekNamedPipe(m_ChildStdOut, NULL, 0, NULL,
                                  &bytes_avail, NULL) ) {
                DWORD error = ::GetLastError();
                // Has peer closed connection?
                if ( !x_IsDisconnectError(error) ) {
                    PIPE_THROW(error,
                               "Failed PeekNamedPipe(stdout)");
                }
                poll |= CPipe::fStdOut;
            } else if ( bytes_avail ) {
                poll |= CPipe::fStdOut;
            }
        }
        if ( (mask & CPipe::fStdErr)
             &&  m_ChildStdErr != INVALID_HANDLE_VALUE ) {
            DWORD bytes_avail = 0;
            if ( !::PeekNamedPipe(m_ChildStdErr, NULL, 0, NULL,
                                  &bytes_avail, NULL) ) {
                DWORD error = ::GetLastError();
                // Has peer closed connection?
                if ( !x_IsDisconnectError(error) ) {
                    PIPE_THROW(error,
                               "Failed PeekNamedPipe(stderr)");
                }
                poll |= CPipe::fStdErr;
            } else if ( bytes_avail ) {
                poll |= CPipe::fStdErr;
            }
        }
        if ( poll ) {
            break;
        }

        if ( !x_timeout ) {
            break;
        }
        if (x_timeout != INFINITE) {
            if (x_sleep > x_timeout) {
                x_sleep = x_timeout;
            }
            x_timeout -= x_sleep;
        }
        SleepMilliSec(x_sleep);
        x_sleep <<= 1;
        if (x_sleep > kWaitPrecision) {
            x_sleep = kWaitPrecision;
        }
    }

    _ASSERT(!(poll ^ (poll & mask)));
    return poll;
}


#elif defined(NCBI_OS_UNIX)


//////////////////////////////////////////////////////////////////////////////
//
// CPipeHandle -- Unix version
//

NCBI_PARAM_DECL  (bool, CONN, PIPE_USE_POLL);
NCBI_PARAM_DEF_EX(bool, CONN, PIPE_USE_POLL,
                  true, eParam_Default, CONN_PIPE_USE_POLL);

class CPipeHandle
{
public:
    CPipeHandle(void);
    ~CPipeHandle();
    EIO_Status Open(const string&         cmd,
                    const vector<string>& args,
                    CPipe::TCreateFlags   create_flags,
                    const string&         current_dir,
                    const char* const     env[],
                    size_t                /*pipe_size*/);
    void       OpenSelf(void);
    EIO_Status Close(int* exitcode, const STimeout* timeout);
    EIO_Status CloseHandle(CPipe::EChildIOHandle handle);
    EIO_Status Read(void* buf, size_t count, size_t* read,
                    const CPipe::EChildIOHandle from_handle,
                    const STimeout* timeout) const;
    EIO_Status Write(const void* buf, size_t count, size_t* written,
                     const STimeout* timeout) const;
    CPipe::TChildPollMask Poll(CPipe::TChildPollMask mask,
                               const STimeout* timeout) const;
    TProcessHandle GetProcessHandle(void) const { return m_Pid; }
    void Release(void) { x_Clear(); }

private:
    // Clear object state.
    void x_Clear(void);
    // Get child's I/O handle.
    int  x_GetHandle(CPipe::EChildIOHandle from_handle) const;
    // Trigger blocking mode on specified I/O handle.
    void x_SetNonBlockingMode(int fd) const;
    // Wait on the file descriptors for I/O.
    CPipe::TChildPollMask x_Poll(CPipe::TChildPollMask mask,
                                 const STimeout* timeout) const;

private:
    // I/O handles for child process.
    int  m_ChildStdIn;
    int  m_ChildStdOut;
    int  m_ChildStdErr;

    // Child process pid.
    TPid m_Pid;

    // Pipe flags
    CPipe::TCreateFlags m_Flags;

    // Flag that indicates whether the m_ChildStd* and m_Pid
    // member variables contain the relevant handles of the
    // current process, in which case they won't be closed.
    bool m_SelfHandles;

    // Use poll(2) (now default) instead of select(2) (formerly)
    bool m_UsePoll;
};


CPipeHandle::CPipeHandle(void)
    : m_ChildStdIn(-1), m_ChildStdOut(-1), m_ChildStdErr(-1),
      m_Pid((pid_t)(-1)), m_Flags(0), m_SelfHandles(false)
{
    static NCBI_PARAM_TYPE(CONN, PIPE_USE_POLL) use_poll_param;

    m_UsePoll = use_poll_param.Get();
    _TRACE("CPipeHandle using poll(): " + NStr::BoolToString(m_UsePoll));
}


CPipeHandle::~CPipeHandle()
{
    static const STimeout kZeroTimeout = {0, 0};
    Close(0, &kZeroTimeout);
    x_Clear();
}


// Auxiliary function to exit from forked process with reporting errno
// on errors into the specified file descriptor 
static void s_Exit(int status, int fd)
{
    int errcode = errno;
    (void) ::write(fd, &errcode, sizeof(errcode));
    (void) ::close(fd);
    ::_exit(status);
}


// Emulate nonexistent function execvpe().
// On success, execve() does not return, on error -1 is returned,
// and errno is set appropriately.

static int s_ExecShell(const char* executable,
                       char *const argv[], char *const envp[])
{
    static const char kShell[] = "/bin/sh";

    // Count number of arguments
    int i;
    for (i = 0;  argv[i];  ++i);
    ++i; // copy last zero element also

    // Construct argument list for the shell
    const char** args = new const char*[i + 1];
    AutoPtr< const char*, ArrayDeleter<const char*> > args_ptr(args);

    args[0] = kShell;
    args[1] = executable;
    for (;  i > 1;  --i) {
        args[i] = argv[i - 1];
    }

    // Execute the shell
    return ::execve(kShell, (char**) args, envp);
}


static int s_ExecVPE(const char* file, char* const argv[], char* const envp[])
{
    // CAUTION (security):  current directory is in the path on purpose,
    //                      to be in-sync with the default behavior on MS-Win.
    static const char* kPathDefault = ":/bin:/usr/bin";

    // If file name is not specified
    if (!file  ||  *file == '\0') {
        errno = ENOENT;
        return -1;
    }

    // If the file name contains path
    if ( strchr(file, '/') ) {
        ::execve(file, argv, envp);
        return errno == ENOEXEC ? s_ExecShell(file, argv, envp) : -1;
    }

    // Get the PATH environment variable
    CORE_LOCK_READ;
    const char* path = getenv("PATH");
    if ( !path ) {
        path = kPathDefault;
    }
    size_t file_len = strlen(file) + 1/*'\0'*/;
    char* buf = new char[strlen(path) + 1/*'/'*/ + file_len];
    AutoPtr< char, ArrayDeleter<char> > buf_ptr(buf);

    bool eacces_err = false;
    const char* next = path;
    while ( *next ) {
        next = strchr(path, ':');
        if ( !next ) {
            // Last part of the PATH environment variable
            next = path + strlen(path);
        }
        size_t len = next - path;
        if ( len ) {
            // Copy directory name into the buffer
            memmove(buf, path, len);
            // Add slash if needed
            if (buf[len - 1] != '/') {
                buf[len++]    = '/';
            }
        } else {
            // Two colons side by side -- current directory
            buf[0] = '.';
            buf[1] = '/';
            len = 2;
        }
        // Add file name
        memcpy(buf + len, file, file_len);
        path = next + 1;

        // Try to execute file with the generated name
        int error;
        ::execve(buf, argv, envp);
        if ((error = errno) == ENOEXEC) {
            CORE_UNLOCK;
            return s_ExecShell(buf, argv, envp);
        }
        switch (error) {
        case EACCES:
            // Permission denied. Memorize this thing and try next path.
            eacces_err = true;
        case ENOENT:
        case ENOTDIR:
            // Try next path element
            break;
        default:
            // We found an executable file, but could not execute it
            CORE_UNLOCK;
            return -1;
        }
    }

    CORE_UNLOCK;
    if ( eacces_err ) {
        errno = EACCES;
    }
    return -1;
}


static int x_SafeFD(int fd, int safe)
{
    if (fd == safe  ||  fd > STDERR_FILENO) {
        return fd;
    }
    int temp = ::fcntl(fd, F_DUPFD, STDERR_FILENO + 1);
    ::close(fd);
    return temp;
}


static bool x_SafePipe(int pipe[2], int n, int safe)
{
    bool retval = true;
    if        ((pipe[0] = x_SafeFD(pipe[0], n == 0 ? safe : -1)) == -1) {
        ::close(pipe[1]);
        retval = false;
    } else if ((pipe[1] = x_SafeFD(pipe[1], n == 1 ? safe : -1)) == -1) {
        ::close(pipe[0]);
        retval = false;
    }
    return retval;
}


EIO_Status CPipeHandle::Open(const string&         cmd,
                             const vector<string>& args,
                             CPipe::TCreateFlags   create_flags,
                             const string&         current_dir,
                             const char* const     env[],
                             size_t                /*unused*/)

{
    DEFINE_STATIC_FAST_MUTEX(s_Mutex);
    CFastMutexGuard guard_mutex(s_Mutex);

    if (m_Pid != (pid_t)(-1)) {
        ERR_POST_X(1, s_FormatErrorMessage("Open", "Pipe busy"));
        return eIO_Unknown;
    }
    m_Flags = create_flags;

    // Child process I/O handles: init "our" ends of the pipes
    int pipe_in[2], pipe_out[2], pipe_err[2];
    pipe_in[0]  = -1;
    pipe_out[1] = -1;
    pipe_err[1] = -1;

    EIO_Status status = eIO_Unknown;

    int status_pipe[2] = {-1, -1};
    try {
        // Flush stdio
        NcbiCout.flush();
        NcbiCerr.flush();
        ::fflush(NULL);

        // Create pipe for child's stdin
        _ASSERT(CPipe::fStdIn_Close != 0);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            if (::pipe(pipe_in) < 0
                ||  !x_SafePipe(pipe_in, 0, STDIN_FILENO)) {
                pipe_in[0] = -1;
                PIPE_THROW(errno,
                           "Failed to create pipe for stdin");
            }
            m_ChildStdIn = pipe_in[1];
            x_SetNonBlockingMode(m_ChildStdIn);
        }

        // Create pipe for child's stdout
        _ASSERT(CPipe::fStdOut_Close != 0);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            if (::pipe(pipe_out) < 0
                ||  !x_SafePipe(pipe_out, 1, STDOUT_FILENO)) {
                pipe_out[1] = -1;
                PIPE_THROW(errno,
                           "Failed to create pipe for stdout");
            }
            m_ChildStdOut = pipe_out[0];
            x_SetNonBlockingMode(m_ChildStdOut);
        }

        // Create pipe for child's stderr
        _ASSERT(CPipe::fStdErr_Open != 0);
        if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            if (::pipe(pipe_err) < 0
                ||  !x_SafePipe(pipe_err, 1, STDERR_FILENO)) {
                pipe_err[1] = -1;
                PIPE_THROW(errno,
                           "Failed to create pipe for stderr");
            }
            m_ChildStdErr = pipe_err[0];
            x_SetNonBlockingMode(m_ChildStdErr);
        }

        // Create temporary pipe to get status of execution
        // of the child process
        if (::pipe(status_pipe) < 0
            ||  !x_SafePipe(status_pipe, -1, -1)) {
            PIPE_THROW(errno,
                       "Failed to create status pipe");
        }
        ::fcntl(status_pipe[1], F_SETFD, 
                ::fcntl(status_pipe[1], F_GETFD, 0) | FD_CLOEXEC);

        // Fork child process
        switch (m_Pid = ::fork()) {
        case (pid_t)(-1):
            PIPE_THROW(errno,
                       "Failed fork()");
            /*NOTREACHED*/
            break;

        case 0:
            // *** CHILD PROCESS CONTINUES HERE ***

            // Create new process group if needed
            if ( IS_SET(create_flags, CPipe::fNewGroup) ) {
                ::setpgid(0, 0);
            }

            // Close unused pipe handle
            ::close(status_pipe[0]);

            // Bind child's standard I/O file handles to pipes
            if ( !IS_SET(create_flags, CPipe::fStdIn_Close)  ) {
                if (pipe_in[0] != STDIN_FILENO) {
                    if (::dup2(pipe_in[0], STDIN_FILENO) < 0) {
                        s_Exit(-1, status_pipe[1]);
                    }
                    ::close(pipe_in[0]);
                }
                ::close(pipe_in[1]);
                ::fflush(stdin);
            } else {
                (void) ::freopen("/dev/null", "r", stdin);
            }
            if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
                if (pipe_out[1] != STDOUT_FILENO) {
                    if (::dup2(pipe_out[1], STDOUT_FILENO) < 0) {
                        s_Exit(-1, status_pipe[1]);
                    }
                    ::close(pipe_out[1]);
                }
                ::close(pipe_out[0]);
            } else {
                (void) ::freopen("/dev/null", "w", stdout);
            }
            if        ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
                if (pipe_err[1] != STDERR_FILENO) {
                    if (::dup2(pipe_err[1], STDERR_FILENO) < 0) {
                        s_Exit(-1, status_pipe[1]);
                    }
                    ::close(pipe_err[1]);
                }
                ::close(pipe_err[0]);
            } else if ( IS_SET(create_flags, CPipe::fStdErr_Share) ) {
                /*nothing to do*/;
            } else if ( IS_SET(create_flags, CPipe::fStdErr_StdOut) ) {
                _ASSERT(STDOUT_FILENO != STDERR_FILENO);
                if (::dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
                    s_Exit(-1, status_pipe[1]);
                }
            } else {
                (void) ::freopen("/dev/null", "a", stderr);
            }

            // Restore SIGPIPE signal processing
            if ( IS_SET(create_flags, CPipe::fSigPipe_Restore) ) {
                ::signal(SIGPIPE, SIG_DFL);
            }

            // Prepare program arguments
            size_t i;
            const char** x_args = new const char*[args.size() + 2];
            AutoPtr< const char*, ArrayDeleter<const char*> > p_args(x_args);
            x_args[i = 0] = cmd.c_str();
            ITERATE (vector<string>, arg, args) {
                x_args[++i] = arg->c_str();
            }
            x_args[++i] = 0;

            // Change current working directory if specified
            if ( !current_dir.empty() ) {
                (void) ::chdir(current_dir.c_str());
            }
            // Execute the program
            int status;
            if ( env ) {
                // Emulate nonexistent execvpe()
                status = s_ExecVPE(cmd.c_str(),
                                   const_cast<char**>(x_args),
                                   const_cast<char**>(env));
            } else {
                status = ::execvp(cmd.c_str(), const_cast<char**>(x_args));
            }
            s_Exit(status, status_pipe[1]);

            // *** CHILD PROCESS DOES NOT CONTINUE BEYOND THIS LINE ***
        }

        // Close unused pipes' ends
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close)  ) {
            ::close(pipe_in[0]);
            pipe_in[0]  = -1;
        }
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            ::close(pipe_out[1]);
            pipe_out[1] = -1;
        }
        if (  IS_SET(create_flags, CPipe::fStdErr_Open)  ) {
            ::close(pipe_err[1]);
            pipe_err[1] = -1;
        }
        ::close(status_pipe[1]);
        status_pipe[1] = -1;

        // Check status pipe:
        // if it has some data, this is an errno from the child process;
        // if there is an EOF, then the child exec()'d successfully.
        // Retry if either blocked or interrupted

        // Try to read errno the from forked process
        ssize_t n;
        int errcode;
        while ((n = ::read(status_pipe[0], &errcode, sizeof(errcode))) < 0) {
            if (errno != EINTR)
                break;
        }
        ::close(status_pipe[0]);
        status_pipe[0] = -1;

        if (n > 0) {
            // Child could not run -- reap it and exit with an error
            status = eIO_Closed;
            ::waitpid(m_Pid, NULL, 0);
            PIPE_THROW((size_t) n < sizeof(errcode) ? 0 : errcode,
                       "Failed to execute \"" + cmd + '"');
        }

        return eIO_Success;
    } 
    catch (string& what) {
        // Close all open file descriptors
        if ( pipe_in[0]  != -1 ) {
            ::close(pipe_in[0]);
        }
        if ( pipe_out[1] != -1 ) {
            ::close(pipe_out[1]);
        }
        if ( pipe_err[1] != -1 ) {
            ::close(pipe_err[1]);
        }
        if ( status_pipe[0] != -1 ) {
            ::close(status_pipe[0]);
        }
        if ( status_pipe[1] != -1 ) {
            ::close(status_pipe[1]);
        }
        static const STimeout kZeroTimeout = {0, 0};
        Close(0, &kZeroTimeout);
        ERR_POST_X(1, s_FormatErrorMessage("Open", what));
        x_Clear();
    }

    return status;
}


void CPipeHandle::OpenSelf(void)
{
    if (m_Pid != (pid_t)(-1)) {
        PIPE_THROW(0,
                   "Pipe busy");
    }

    NcbiCout.flush();
    ::fflush(stdout);
    m_ChildStdIn  = fileno(stdout);  // NB: a macro on BSD, so no "::" scope
    m_ChildStdOut = fileno(stdin);
    m_Pid = ::getpid();

    m_SelfHandles = true;
}


void CPipeHandle::x_Clear(void)
{
    m_Pid = (pid_t)(-1);
    if (m_SelfHandles) {
        m_ChildStdIn  = -1;
        m_ChildStdOut = -1;
        m_SelfHandles = false;
    } else {
        CloseHandle(CPipe::eStdIn);
        CloseHandle(CPipe::eStdOut);
        CloseHandle(CPipe::eStdErr);
    }
}


EIO_Status CPipeHandle::Close(int* exitcode, const STimeout* timeout)
{
    EIO_Status status;

    if (!m_SelfHandles) {
        CloseHandle(CPipe::eStdIn);
        CloseHandle(CPipe::eStdOut);
        CloseHandle(CPipe::eStdErr);

        if (m_Pid == (pid_t)(-1)) {
            if ( exitcode ) {
                *exitcode = -1;
            }
            status = eIO_Closed;
        } else {
            CProcess process(m_Pid, CProcess::ePid);
            status = s_Close(process, m_Flags, timeout, exitcode);
        }
    } else {
        if ( exitcode ) {
            *exitcode = 0;
        }
        status = eIO_Success;
    }

    if (status != eIO_Timeout) {
        x_Clear();
    }
    return status;
}


EIO_Status CPipeHandle::CloseHandle(CPipe::EChildIOHandle handle)
{
    switch ( handle ) {
    case CPipe::eStdIn:
        if (m_ChildStdIn == -1) {
            return eIO_Closed;
        }
        ::close(m_ChildStdIn);
        m_ChildStdIn = -1;
        break;
    case CPipe::eStdOut:
        if (m_ChildStdOut == -1) {
            return eIO_Closed;
        }
        ::close(m_ChildStdOut);
        m_ChildStdOut = -1;
        break;
    case CPipe::eStdErr:
        if (m_ChildStdErr == -1) {
            return eIO_Closed;
        }
        ::close(m_ChildStdErr);
        m_ChildStdErr = -1;
        break;
    default:
        _TROUBLE;
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


EIO_Status CPipeHandle::Read(void* buf, size_t count, size_t* n_read, 
                             const CPipe::EChildIOHandle from_handle,
                             const STimeout* timeout) const
{
    _ASSERT(!n_read  ||  !*n_read);
    _ASSERT(!(from_handle & (from_handle - 1)));

    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pid == (pid_t)(-1)) {
            PIPE_THROW(0,
                       "Pipe closed");
        }
        int fd = x_GetHandle(from_handle);
        if (fd == -1) {
            PIPE_THROW(0,
                       "Pipe I/O handle "
                       + x_GetHandleName(from_handle) + " closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        // Retry if either blocked or interrupted
        for (;;) {
            // Try to read
            ssize_t bytes_read = ::read(fd, buf, count);
            if (bytes_read >= 0) {
                if ( n_read ) {
                    *n_read = (size_t) bytes_read;
                }
                status = bytes_read ? eIO_Success : eIO_Closed;
                break;
            }
            int error = errno;

            if (error == EAGAIN  ||  error == EWOULDBLOCK) {
                // Blocked -- wait for data to come;  exit if timeout/error
                if ( (timeout  &&  !(timeout->sec | timeout->usec ))
                     ||  !x_Poll(from_handle, timeout) ) {
                    status = eIO_Timeout;
                    break;
                }
                continue;
            }
            if (error != EINTR) {
                PIPE_THROW(error,
                           "Failed to read data from pipe I/O handle "
                           + x_GetHandleName(from_handle));
            }
            if (SOCK_SetInterruptOnSignalAPI(eDefault) == eOn) {
                status = eIO_Interrupt;
                break;
            }
            // Interrupted read -- restart
        }
    }
    catch (string& what) {
        ERR_POST_X(2, s_FormatErrorMessage("Read", what));
    }

    return status;
}


EIO_Status CPipeHandle::Write(const void* buf, size_t count,
                              size_t* n_written, const STimeout* timeout) const

{
    _ASSERT(!n_written  ||  !*n_written);

    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pid == (pid_t)(-1)) {
            PIPE_THROW(0,
                       "Pipe closed");
        }
        if (m_ChildStdIn == -1) {
            status = eIO_Closed;
            PIPE_THROW(0,
                       "Pipe I/O handle "
                       + x_GetHandleName(CPipe::eStdIn) + " closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        // Retry if either blocked or interrupted
        for (;;) {
            // Try to write
            ssize_t bytes_written = ::write(m_ChildStdIn, buf, count);
            if (bytes_written >= 0) {
                if ( n_written ) {
                    *n_written = (size_t) bytes_written;
                }
                status = bytes_written ? eIO_Success : eIO_Unknown;
                break;
            }
            int error = errno;

            if (errno == EAGAIN  ||  errno == EWOULDBLOCK) {
                // Blocked -- wait for write readiness;  exit if timeout/error
                if ( (timeout  &&  !(timeout->sec | timeout->usec))
                     ||  !x_Poll(CPipe::fStdIn, timeout) ) {
                    status = eIO_Timeout;
                    break;
                }
                continue;
            }
            if (errno != EINTR) {
                if (error == EPIPE) {
                    // Peer has closed its end
                    status = eIO_Closed;
                } // NB: status == eIO_Unknown
                PIPE_THROW(errno,
                           "Failed to write data to pipe I/O handle "
                           + x_GetHandleName(CPipe::eStdIn));
            }
            if (SOCK_SetInterruptOnSignalAPI(eDefault) == eOn) {
                status = eIO_Interrupt;
                break;
            }
            // Interrupted write -- restart
        }
    }
    catch (string& what) {
        ERR_POST_X(3, s_FormatErrorMessage("Write", what));
    }

    return status;
}


CPipe::TChildPollMask CPipeHandle::Poll(CPipe::TChildPollMask mask,
                                        const STimeout* timeout) const
{
    CPipe::TChildPollMask poll = 0;

    try {
        if (m_Pid == (pid_t)(-1)) {
            PIPE_THROW(0,
                       "Pipe closed");
        }
        if (m_ChildStdIn  == -1  &&
            m_ChildStdOut == -1  &&
            m_ChildStdErr == -1) {
            PIPE_THROW(0,
                       "All I/O handles closed");
        }
        poll = x_Poll(mask, timeout);
    }
    catch (string& what) {
        ERR_POST_X(4, s_FormatErrorMessage("Poll", what));
    }

    return poll;
}


int CPipeHandle::x_GetHandle(CPipe::EChildIOHandle from_handle) const
{
    switch (from_handle) {
    case CPipe::eStdIn:
        return m_ChildStdIn;
    case CPipe::eStdOut:
        return m_ChildStdOut;
    case CPipe::eStdErr:
        return m_ChildStdErr;
    default:
        _TROUBLE;
        break;
    }
    return -1;
}


void CPipeHandle::x_SetNonBlockingMode(int fd) const
{
    if (::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) |  O_NONBLOCK) < 0) {
        PIPE_THROW(errno,
                   "Failed to set pipe I/O handle non-blocking");
    }
}


CPipe::TChildPollMask CPipeHandle::x_Poll(CPipe::TChildPollMask mask,
                                          const STimeout* timeout) const
{
    CPipe::TChildPollMask poll = 0;

    if ( m_UsePoll ) {
        struct pollfd poll_fds[3] = {
            { m_ChildStdIn,  POLLOUT | POLLERR },
            { m_ChildStdOut, POLLIN,           },
            { m_ChildStdErr, POLLIN,           }
        };
        int timeout_msec(timeout
                         ? timeout->sec * 1000 + (timeout->usec + 500) / 1000
                         : -1/*infinite*/);

        // Negative FDs OK, poll should ignore them
        // Check the mask
        if (!(mask & CPipe::fStdIn))
            poll_fds[0].fd = -1;
        if (!(mask & CPipe::fStdOut))
            poll_fds[1].fd = -1;
        if (!(mask & CPipe::fStdErr))
            poll_fds[2].fd = -1;

        for (;;) { // Auto-resume if interrupted by a signal
            int n = ::poll(poll_fds, 3, timeout_msec);

            if (n == 0) {
                // timeout
                break;
            }
            if (n > 0) {
                // no need to check mask here
                if (poll_fds[0].revents) {
                    poll |= CPipe::fStdIn;
                }
                if (poll_fds[1].revents) {
                    poll |= CPipe::fStdOut;
                }
                if (poll_fds[2].revents) {
                    poll |= CPipe::fStdErr;
                }
                break;
            }
            // n < 0
            if ((n = errno) != EINTR) {
                PIPE_THROW(n,
                           "Failed poll()");
            }
            if (SOCK_SetInterruptOnSignalAPI(eDefault) == eOn) {
                break;
            }
            // continue, no need to recreate either timeout or poll_fds
        }
    } else { // Using select(2), as before
        for (;;) { // Auto-resume if interrupted by a signal
            struct timeval* tmp;
            struct timeval  tmo;

            if ( timeout ) {
                // NB: Timeout has already been normalized
                tmo.tv_sec  = timeout->sec;
                tmo.tv_usec = timeout->usec;
                tmp = &tmo;
            } else {
                tmp = 0;
            }

            fd_set rfds;
            fd_set wfds;
            fd_set efds;

            int max = -1;
            bool rd = false;
            bool wr = false;

            FD_ZERO(&efds);

            if ( (mask & CPipe::fStdIn)   &&  m_ChildStdIn  != -1 ) {
                wr = true;
                FD_ZERO(&wfds);
                if (m_ChildStdIn < FD_SETSIZE) {
                    FD_SET(m_ChildStdIn,  &wfds);
                    FD_SET(m_ChildStdIn,  &efds);
                }
                if (max < m_ChildStdIn) {
                    max = m_ChildStdIn;
                }
            }
            if ( (mask & CPipe::fStdOut)  &&  m_ChildStdOut != -1 ) {
                if (!rd) {
                    rd = true;
                    FD_ZERO(&rfds);
                }
                if (m_ChildStdOut < FD_SETSIZE) {
                    FD_SET(m_ChildStdOut, &rfds);
                    FD_SET(m_ChildStdOut, &efds);
                }
                if (max < m_ChildStdOut) {
                    max = m_ChildStdOut;
                }
            }
            if ( (mask & CPipe::fStdErr)  &&  m_ChildStdErr != -1 ) {
                if (!rd) {
                    rd = true;
                    FD_ZERO(&rfds);
                }
                if (m_ChildStdErr < FD_SETSIZE) {
                    FD_SET(m_ChildStdErr, &rfds);
                    FD_SET(m_ChildStdErr, &efds);
                }
                if (max < m_ChildStdErr) {
                    max = m_ChildStdErr;
                }
            }
            _ASSERT(rd  ||  wr);

            if (max >= FD_SETSIZE) {
                PIPE_THROW(0,
                           "File descriptor " + NStr::NumericToString(max)
                           + " too large (" + string(STR(FD_SETSIZE)) + ')');
            }

            int n = ::select(max + 1,
                             rd ? &rfds : 0,
                             wr ? &wfds : 0, &efds, tmp);

            if (n == 0) {
                // timeout
                break;
            }
            if (n > 0) {
                if ( wr
                     &&  ( FD_ISSET(m_ChildStdIn,  &wfds)  ||
                           FD_ISSET(m_ChildStdIn,  &efds) ) ) {
                    poll |= CPipe::fStdIn;
                }
                if ( (mask & CPipe::fStdOut)  &&  m_ChildStdOut != -1
                     &&  ( FD_ISSET(m_ChildStdOut, &rfds)  ||
                           FD_ISSET(m_ChildStdOut, &efds) ) ) {
                    poll |= CPipe::fStdOut;
                }
                if ( (mask & CPipe::fStdErr)  &&  m_ChildStdErr != -1
                     &&  ( FD_ISSET(m_ChildStdErr, &rfds)  ||
                           FD_ISSET(m_ChildStdErr, &efds) ) ) {
                    poll |= CPipe::fStdErr;
                }
                break;
            }
            if ((n = errno) != EINTR) {
                PIPE_THROW(n,
                           "Failed select()");
            }
            if (SOCK_SetInterruptOnSignalAPI(eDefault) == eOn) {
                break;
            }
            // continue
        }
    }

    _ASSERT(!(poll ^ (poll & mask)));
    return poll;
}


#endif // NCBI_OS_UNIX | NCBI_OS_MSWIN


//////////////////////////////////////////////////////////////////////////////
//
// CPipe
//

CPipe::CPipe(size_t pipe_size)
    : m_PipeSize(pipe_size),
      m_PipeHandle(new CPipeHandle), m_ReadHandle(eStdOut),
      m_ReadStatus(eIO_Closed), m_WriteStatus(eIO_Closed),
      m_ReadTimeout(0), m_WriteTimeout(0), m_CloseTimeout(0)
{
    return;
}


CPipe::CPipe(const string&         cmd,
             const vector<string>& args,
             TCreateFlags          create_flags,
             const string&         current_dir,
             const char* const     env[],
             size_t                pipe_size)
    : m_PipeSize(pipe_size),
      m_PipeHandle(0), m_ReadHandle(eStdOut),
      m_ReadStatus(eIO_Closed), m_WriteStatus(eIO_Closed),
      m_ReadTimeout(0), m_WriteTimeout(0), m_CloseTimeout(0)
{
    unique_ptr<CPipeHandle> pipe_handle_ptr(new CPipeHandle);
    EIO_Status status = pipe_handle_ptr->Open(cmd, args, create_flags,
                                              current_dir, env, pipe_size);
    if (status != eIO_Success) {
        NCBI_THROW(CPipeException, eOpen,
                   "[CPipe::CPipe]  Failed: " + string(IO_StatusStr(status)));
    }
    m_PipeHandle = pipe_handle_ptr.release();
}


CPipe::~CPipe()
{
    Close();
    delete m_PipeHandle;
}


EIO_Status CPipe::Open(const string&         cmd,
                       const vector<string>& args,
                       TCreateFlags          create_flags,
                       const string&         current_dir,
                       const char* const     env[],
                       size_t                pipe_size)
{
    _ASSERT(m_PipeHandle);
    if (pipe_size) {
        m_PipeSize = pipe_size;
    }

    EIO_Status status = m_PipeHandle->Open(cmd, args, create_flags,
                                           current_dir, env, m_PipeSize);
    if (status == eIO_Success) {
        m_ReadHandle  = eStdOut;
        m_ReadStatus  = eIO_Success;
        m_WriteStatus = eIO_Success;
    }
    return status;
}


void CPipe::OpenSelf(void)
{
    _ASSERT(m_PipeHandle);
    try {
        m_PipeHandle->OpenSelf();
    }
    catch (string& err) {
        NCBI_THROW(CPipeException, eOpen, err);
    }
    m_ReadHandle  = eStdOut;
    m_ReadStatus  = eIO_Success;
    m_WriteStatus = eIO_Success;
}


EIO_Status CPipe::Close(int* exitcode)
{
    _ASSERT(m_PipeHandle);
    m_ReadStatus  = eIO_Closed;
    m_WriteStatus = eIO_Closed;
    return m_PipeHandle->Close(exitcode, m_CloseTimeout);
}


EIO_Status CPipe::CloseHandle(EChildIOHandle handle)
{
    _ASSERT(m_PipeHandle);
    return m_PipeHandle->CloseHandle(handle);
}


EIO_Status CPipe::SetReadHandle(EChildIOHandle from_handle)
{
    _ASSERT(m_PipeHandle);
    if (from_handle == eStdIn) {
        return eIO_InvalidArg;
    }
    m_ReadHandle = from_handle == eDefault ? eStdOut : from_handle;
    return eIO_Success;
}


EIO_Status CPipe::Read(void* buf, size_t count, size_t* n_read,
                       EChildIOHandle from_handle)
{
    _ASSERT(m_PipeHandle);
    if ( n_read ) {
        *n_read = 0;
    }
    if (from_handle == eStdIn) {
        return eIO_InvalidArg;
    }
    if (from_handle == eDefault) {
        from_handle  = m_ReadHandle;
    }
    _ASSERT(m_ReadHandle == eStdOut  ||  m_ReadHandle == eStdErr);
    if (count  &&  !buf) {
        return eIO_InvalidArg;
    }
    m_ReadStatus = m_PipeHandle->Read(buf, count, n_read, from_handle,
                                      m_ReadTimeout);
    return m_ReadStatus;
}


EIO_Status CPipe::Write(const void* buf, size_t count, size_t* n_written)
{
    _ASSERT(m_PipeHandle);
    if ( n_written ) {
        *n_written = 0;
    }
    if (count  &&  !buf) {
        return eIO_InvalidArg;
    }
    m_WriteStatus = m_PipeHandle->Write(buf, count, n_written,
                                        m_WriteTimeout);
    return m_WriteStatus;
}


CPipe::TChildPollMask CPipe::Poll(TChildPollMask mask, 
                                  const STimeout* timeout)
{
    _ASSERT(m_PipeHandle  &&  timeout != kDefaultTimeout);
    if (!mask  ||  timeout == kDefaultTimeout) {
        return 0;
    }
    TChildPollMask x_mask = mask;
    if ( mask & fDefault ) {
        _ASSERT(m_ReadHandle == eStdOut ||  m_ReadHandle == eStdErr);
        x_mask |= m_ReadHandle;
    }
    TChildPollMask poll = m_PipeHandle->Poll(x_mask, timeout);
    if ( mask & fDefault ) {
        if ( poll & m_ReadHandle ) {
            poll |= fDefault;
        }
        poll &= mask;
    }
    // Result may not be a bigger set
    _ASSERT(!(poll ^ (poll & mask)));
    return poll;
}


EIO_Status CPipe::Status(EIO_Event direction) const
{
    _ASSERT(m_PipeHandle);
    switch ( direction ) {
    case eIO_Read:
        return m_ReadStatus;
    case eIO_Write:
        return m_WriteStatus;
    default:
        _TROUBLE;
        break;
    }
    return eIO_InvalidArg;
}


EIO_Status CPipe::SetTimeout(EIO_Event event, const STimeout* timeout)
{
    _ASSERT(m_PipeHandle);
    if (timeout == kDefaultTimeout) {
        return eIO_Success;
    }
    switch ( event ) {
    case eIO_Close:
        m_CloseTimeout = s_SetTimeout(timeout, &m_CloseTimeoutValue);
        break;
    case eIO_Read:
        m_ReadTimeout  = s_SetTimeout(timeout, &m_ReadTimeoutValue);
        break;
    case eIO_Write:
        m_WriteTimeout = s_SetTimeout(timeout, &m_WriteTimeoutValue);
        break;
    case eIO_ReadWrite:
        m_ReadTimeout  = s_SetTimeout(timeout, &m_ReadTimeoutValue);
        m_WriteTimeout = s_SetTimeout(timeout, &m_WriteTimeoutValue);
        break;
    default:
        _TROUBLE;
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


const STimeout* CPipe::GetTimeout(EIO_Event event) const
{
    _ASSERT(m_PipeHandle);
    switch ( event ) {
    case eIO_Close:
        return m_CloseTimeout;
    case eIO_Read:
        return m_ReadTimeout;
    case eIO_Write:
        return m_WriteTimeout;
    default:
        _TROUBLE;
        break;
    }
    return kDefaultTimeout;
}


TProcessHandle CPipe::GetProcessHandle(void) const
{
    _ASSERT(m_PipeHandle);
    return m_PipeHandle->GetProcessHandle();
}


CPipe::IProcessWatcher::~IProcessWatcher()
{
}


/* static */
CPipe::EFinish CPipe::ExecWait(const string&           cmd,
                               const vector<string>&   args,
                               CNcbiIstream&           in,
                               CNcbiOstream&           out,
                               CNcbiOstream&           err,
                               int&                    exit_code,
                               const string&           current_dir,
                               const char* const       env[],
                               CPipe::IProcessWatcher* watcher,
                               const STimeout*         kill_timeout,
                               size_t                  pipe_size)
{
    STimeout ktm;

    if (kill_timeout) {
        ktm = *kill_timeout;
    } else {
        NcbiMsToTimeout(&ktm, CProcess::kDefaultKillTimeout);
    }

    CPipe pipe(pipe_size);
    EIO_Status st = pipe.Open(cmd, args, 
                              fStdErr_Open | fSigPipe_Restore
                              | fNewGroup | fKillOnClose,
                              current_dir, env);
    if (st != eIO_Success) {
        NCBI_THROW(CPipeException, eOpen,
                   "[CPipe::ExecWait]  Cannot execute \"" + cmd + '"');
    }

    TProcessHandle pid = pipe.GetProcessHandle();

    if (watcher  &&  watcher->OnStart(pid) != IProcessWatcher::eContinue) {
        pipe.SetTimeout(eIO_Close, &ktm);
        pipe.Close(&exit_code);
        return eCanceled;
    }

    EFinish finish = eDone;
    bool out_done = false;
    bool err_done = false;
    bool in_done  = false;
    
#ifndef NCBI_OS_LINUX
    const size_t buf_size = 16 * 1024;
#else
    const size_t buf_size = 192 * 1024;
#endif
    size_t total_bytes_written = 0;
    size_t bytes_in_inbuf = 0;
    char   inbuf[buf_size];
    char   buf[buf_size];

    TChildPollMask mask = fStdIn | fStdOut | fStdErr;
    try {
        STimeout wait_time = {1, 0};
        while (!(in_done  &&  out_done  &&  err_done)) {
            size_t bytes_read;
            EIO_Status status;

            TChildPollMask rmask = pipe.Poll(mask, &wait_time);
            if (bytes_in_inbuf  ||  ((rmask & fStdIn)  &&  !in_done)) {
                if (bytes_in_inbuf == 0  &&  in.good()) {
                    bytes_in_inbuf  =
                        (size_t) CStreamUtils::Readsome(in, inbuf, buf_size);
                    total_bytes_written = 0;
                }

                if (bytes_in_inbuf > 0) {
                    size_t bytes_written;
                    status = pipe.Write(inbuf + total_bytes_written,
                                        bytes_in_inbuf, &bytes_written);
                    if (status != eIO_Success) {
                        ERR_POST_X(5,
                                   s_FormatErrorMessage
                                   ("ExecWait",
                                    "Cannot send all data to child process: "
                                    + string(IO_StatusStr(status))));
                        in_done = true;
                    }
                    total_bytes_written += bytes_written;
                    bytes_in_inbuf      -= bytes_written;
                }

                if ((bytes_in_inbuf == 0  &&  !in.good())  ||  in_done) {
                    pipe.CloseHandle(eStdIn);
                    in_done = true;
                    mask &= ~fStdIn;
                }
            }
            if ((rmask & fStdOut)  &&  !out_done) {
                status = pipe.Read(buf, buf_size, &bytes_read);
                out.write(buf, bytes_read);
                if (status != eIO_Success) {
                    out_done = true;
                    mask &= ~fStdOut;
                }
            }
            if ((rmask & fStdErr)  &&  !err_done) {
                status = pipe.Read(buf, buf_size, &bytes_read, eStdErr);
                err.write(buf, bytes_read);
                if (status != eIO_Success) {
                    err_done = true;
                    mask &= ~fStdErr;
                }
            }
            if (!CProcess(pid).IsAlive())
                break;
            if (watcher) {
                switch (watcher->Watch(pid)) {
                case IProcessWatcher::eContinue:
                    continue;
                case IProcessWatcher::eStop:
                    break;
                case IProcessWatcher::eExit:
                    if (pipe.m_PipeHandle) {
                        pipe.m_PipeHandle->Release();
                    }
                    return eCanceled;
                }

                // IProcessWatcher::eStop
                pipe.SetTimeout(eIO_Close, &ktm);
                finish = eCanceled;
                break;
            }
        }
    } catch (...) {
        pipe.SetTimeout(eIO_Close, &ktm);
        pipe.Close(&exit_code);
        throw;
    }
    pipe.Close(&exit_code);
    return finish;
}


const char* CPipeException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eOpen:  return "eOpen";
    default:     break;
    }
    return CCoreException::GetErrCodeString();
}


END_NCBI_SCOPE
