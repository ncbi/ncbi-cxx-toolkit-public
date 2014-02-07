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
#include <connect/error_codes.hpp>
#include <connect/ncbi_pipe.hpp>
#include <connect/ncbi_util.h>
#include <corelib/ncbi_system.hpp>
#include <corelib/stream_utils.hpp>

#ifdef NCBI_OS_MSWIN

#  include <windows.h>
#  include <corelib/ncbiexec.hpp>

#elif defined NCBI_OS_UNIX

#  include <errno.h>
#  include <fcntl.h>
#  include <signal.h>
#  include <unistd.h>
#  include <sys/time.h>
#  include <sys/types.h>
#  include <sys/wait.h>

#else
#  error "Class CPipe is supported only on Windows and Unix"
#endif

#define NCBI_USE_ERRCODE_X   Connect_Pipe

#define IS_SET(flags, mask)  (((flags) & (mask)) == (mask))


BEGIN_NCBI_SCOPE


// Predefined timeout (in milliseconds)
const unsigned long kWaitPrecision = 100;


//////////////////////////////////////////////////////////////////////////////
//
// Auxiliary functions
//

static STimeout* s_SetTimeout(const STimeout* from, STimeout* to)
{
    if ( !from ) {
        return const_cast<STimeout*>(kInfiniteTimeout);
    }
    to->sec  = from->usec / 1000000 + from->sec;
    to->usec = from->usec % 1000000;
    return to;
}


static EIO_Status s_Close(const CProcess& process, CPipe::TCreateFlags flags,
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



//////////////////////////////////////////////////////////////////////////////
//
// Class CPipeHandle handles forwarded requests from CPipe.
// This class is reimplemented in a platform-specific fashion where needed.
//


#if defined(NCBI_OS_MSWIN)

#define PIPE_THROW(err, errtxt)                 \
    {                                           \
        DWORD _err = err;                       \
        string _errstr(errtxt);                 \
        throw s_WinError(_err, _errstr);        \
    }


static string s_WinError(DWORD error, string& message)
{
    TXChar* errstr = NULL;
    DWORD rv = ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                               FORMAT_MESSAGE_FROM_SYSTEM     |
                               FORMAT_MESSAGE_MAX_WIDTH_MASK  |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL, error,
                               MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
                               (TXChar*) &errstr, 0, NULL);
    if (!rv  &&  errstr) {
        ::LocalFree(errstr);
        errstr = NULL;
    }
    int dynamic = 0/*false*/;
    const char* result = ::NcbiMessagePlusError(&dynamic,
                                                message.c_str(),
                                                (int) error,
                                                _T_CSTRING(errstr));
    if (errstr) {
        ::LocalFree(errstr);
    }
    string retval;
    if (result) {
        retval = result;
        if (dynamic) {
            free((void*) result);
        }
    } else {
        retval.swap(message);
    }
    return retval;
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
                    const char* const   env[]);
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

private:
    // Clear object state.
    void   x_Clear(void);
    // Get child's I/O handle.
    HANDLE x_GetHandle(CPipe::EChildIOHandle from_handle) const;
    // Trigger blocking mode on specified I/O handle.
    void   x_SetNonBlockingMode(HANDLE fd) const;
    // Wait on the file descriptors I/O.
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
    : m_ProcHandle(INVALID_HANDLE_VALUE),
      m_ChildStdIn(INVALID_HANDLE_VALUE),
      m_ChildStdOut(INVALID_HANDLE_VALUE),
      m_ChildStdErr(INVALID_HANDLE_VALUE),
      m_Flags(0),
      m_SelfHandles(false)
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
                             const char* const     env[])
{
    DEFINE_STATIC_FAST_MUTEX(s_Mutex);
    CFastMutexGuard guard_mutex(s_Mutex);

    x_Clear();
    m_Flags = create_flags;

    EIO_Status status = eIO_Unknown;

    HANDLE child_stdin  = INVALID_HANDLE_VALUE;
    HANDLE child_stdout = INVALID_HANDLE_VALUE;
    HANDLE child_stderr = INVALID_HANDLE_VALUE;

    try {
        // Prepare command line to run
        string cmd_line(cmd);
        ITERATE (vector<string>, iter, args) {
            // Add argument to command line.
            // Escape it with quotes if necessary.
            if ( !cmd_line.empty() ) {
                cmd_line += ' ';
            }
            cmd_line += CExec::QuoteArg(*iter);
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
            for (int i = 0;  i < count;  i++) {
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
        if (stdout_handle != INVALID_HANDLE_VALUE) {
            ::FlushFileBuffers(stdout_handle);
        }
        NcbiCerr.flush();
        if (stderr_handle != INVALID_HANDLE_VALUE) {
            ::FlushFileBuffers(stderr_handle);
        }
        ::fflush(NULL);

        // Set base security attributes
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.bInheritHandle = TRUE;
        attr.lpSecurityDescriptor = NULL;

        // Create pipe for child's stdin
        _ASSERT(CPipe::fStdIn_Close);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            if ( !::CreatePipe(&child_stdin, &m_ChildStdIn, &attr, 0) ) {
                PIPE_THROW(::GetLastError(), "CreatePipe(stdin) failed");
            }
            ::SetHandleInformation(m_ChildStdIn, HANDLE_FLAG_INHERIT, 0);
            x_SetNonBlockingMode(m_ChildStdIn);
        }

        // Create pipe for child's stdout
        _ASSERT(CPipe::fStdOut_Close);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            if ( !::CreatePipe(&m_ChildStdOut, &child_stdout, &attr, 0)) {
                PIPE_THROW(::GetLastError(), "CreatePipe(stdout) failed");
            }
            ::SetHandleInformation(m_ChildStdOut, HANDLE_FLAG_INHERIT, 0);
            x_SetNonBlockingMode(m_ChildStdOut);
        }

        // Create pipe for child's stderr
        _ASSERT(CPipe::fStdErr_Open);
        if        ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            if ( !::CreatePipe(&m_ChildStdErr, &child_stderr, &attr, 0)) {
                PIPE_THROW(::GetLastError(), "CreatePipe(stderr) failed");
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
                               "DuplicateHandle(stderr) failed");
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
                       "CreateProcess(\"" + cmd_line + "\") failed");
        }
        ::CloseHandle(pinfo.hThread);
        m_ProcHandle = pinfo.hProcess;

        _ASSERT(m_ProcHandle != INVALID_HANDLE_VALUE);

        status = eIO_Success;
    }
    catch (string& what) {
        static const STimeout kZeroZimeout = {0, 0};
        Close(0, &kZeroZimeout);
        ERR_POST_X(1, what);
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
    x_Clear();

    NcbiCout.flush();
    ::fflush(stdout);
    if ( !::FlushFileBuffers(m_ChildStdIn) ) {
        PIPE_THROW(::GetLastError(), "FlushFileBuffers(stdout) failed");
    }
    if ((m_ChildStdIn = ::GetStdHandle(STD_OUTPUT_HANDLE))
        == INVALID_HANDLE_VALUE) {
        PIPE_THROW(::GetLastError(), "GetStdHandle(stdout) failed");
    }
    if ((m_ChildStdOut = ::GetStdHandle(STD_INPUT_HANDLE))
        == INVALID_HANDLE_VALUE) {
        PIPE_THROW(::GetLastError(), "GetStdHandle(stdin) failed");
    }
    m_ProcHandle = ::GetCurrentProcess();

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

    if (!m_SelfHandles) {
        CloseHandle(CPipe::eStdIn);
        CloseHandle(CPipe::eStdOut);
        CloseHandle(CPipe::eStdErr);

        if (m_ProcHandle == INVALID_HANDLE_VALUE) {
            if ( exitcode ) {
                *exitcode = -1;
            }
            status = eIO_Closed;
        } else {
            status = s_Close(CProcess(m_ProcHandle, CProcess::eHandle),
                             m_Flags, timeout, exitcode);
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
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


EIO_Status CPipeHandle::Read(void* buf, size_t count, size_t* read, 
                             const CPipe::EChildIOHandle from_handle,
                             const STimeout* timeout) const
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_ProcHandle == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw string("Pipe closed");
        }
        HANDLE fd = x_GetHandle(from_handle);
        if (fd == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw string("Pipe I/O handle closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout   = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;
        DWORD bytes_avail = 0;

        // Wait for data from the pipe with timeout.
        // Using a loop and periodically try PeekNamedPipe is inefficient,
        // but Windows doesn't have asynchronous mechanism to read
        // from a pipe.
        // NOTE:  WaitForSingleObject() doesn't work with anonymous pipes.
        // See CPipe::Poll() for more details.
        for (;;) {
            if ( !::PeekNamedPipe(fd, NULL, 0, NULL, &bytes_avail, NULL) ) {
                // Has peer closed connection?
                DWORD error = ::GetLastError();
                if (error != ERROR_BROKEN_PIPE) {
                    PIPE_THROW(error, "PeekNamedPipe() failed");
                }
                return eIO_Closed;
            }
            if ( bytes_avail ) {
                break;
            }
            unsigned long x_sleep = kWaitPrecision;
            if (x_timeout != INFINITE) {
                if (x_sleep > x_timeout) {
                    x_sleep = x_timeout;
                }
                if ( !x_sleep ) {
                    return eIO_Timeout;
                }
                x_timeout -= x_sleep;
            }
            SleepMilliSec(x_sleep);
        }

        _ASSERT(bytes_avail);
        // We must read only "count" bytes of data regardless of
        // the amount available to read
        if (bytes_avail >         count) {
            bytes_avail = (DWORD) count;
        }
        if ( !::ReadFile(fd, buf, bytes_avail, &bytes_avail, NULL) ) {
            PIPE_THROW(::GetLastError(), "Failed to read data from pipe");
        }
        if ( read ) {
            *read = (size_t) bytes_avail;
        }
        return eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(2, what);
    }
    return status;
}


EIO_Status CPipeHandle::Write(const void* buf, size_t count,
                              size_t* n_written, const STimeout* timeout) const

{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_ProcHandle == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw string("Pipe closed");
        }
        if (m_ChildStdIn == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw string("Pipe I/O handle closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;
        DWORD to_write  = (count > numeric_limits<DWORD>::max()
                           ? numeric_limits<DWORD>::max()
                           : (DWORD) count);
        DWORD bytes_written = 0;

        // Try to write data into the pipe within specified time.
        for (;;) {
            BOOL ok = ::WriteFile(m_ChildStdIn, (char*) buf, to_write,
                                  &bytes_written, NULL);
            if ( bytes_written ) {
                break;
            }
            if ( !ok ) {
                PIPE_THROW(::GetLastError(), "Failed to write data to pipe");
            }
            DWORD x_sleep = kWaitPrecision;
            if (x_timeout != INFINITE) {
                if ( x_timeout ) {
                    if (x_sleep > x_timeout) {
                        x_sleep = x_timeout;
                    }
                    x_timeout -= x_sleep;
                } else {
                    return eIO_Timeout;
                }
            }
            SleepMilliSec(x_sleep);
        }
        if ( n_written ) {
            *n_written = bytes_written;
        }
        return eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(3, what);
    }
    return status;
}


CPipe::TChildPollMask CPipeHandle::Poll(CPipe::TChildPollMask mask,
                                        const STimeout* timeout) const
{
    CPipe::TChildPollMask poll = 0;

    try {
        if (m_ProcHandle == INVALID_HANDLE_VALUE) {
            throw string("Pipe closed");
        }
        if (m_ChildStdIn  == INVALID_HANDLE_VALUE  &&
            m_ChildStdOut == INVALID_HANDLE_VALUE  &&
            m_ChildStdErr == INVALID_HANDLE_VALUE) {
            throw string("All pipe I/O handles closed");
        }
        poll = x_Poll(mask, timeout);
    }
    catch (string& what) {
        ERR_POST_X(4, what);
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
        PIPE_THROW(::GetLastError(), "x_SetNonBlockingMode() failed");
    }
}


CPipe::TChildPollMask CPipeHandle::x_Poll(CPipe::TChildPollMask mask,
                                          const STimeout* timeout) const
{
    CPipe::TChildPollMask poll = 0;
    DWORD x_timeout = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;

    // Wait for data from the pipe with timeout.
    // Using a loop and periodically try PeekNamedPipe is inefficient,
    // but Windows doesn't have asynchronous mechanism to read from a pipe.
    // NOTE: WaitForSingleObject() doesn't work with anonymous pipes.

    for (;;) {
        if ( (mask & CPipe::fStdOut)
             &&  m_ChildStdOut != INVALID_HANDLE_VALUE ) {
            DWORD bytes_avail = 0;
            if ( !::PeekNamedPipe(m_ChildStdOut, NULL, 0, NULL,
                                  &bytes_avail, NULL) ) {
                DWORD error = ::GetLastError();
                // Has peer closed connection?
                if (error != ERROR_BROKEN_PIPE) {
                    PIPE_THROW(error, "PeekNamedPipe(stdout) failed");
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
                if (error != ERROR_BROKEN_PIPE) {
                    PIPE_THROW(error, "PeekNamedPipe(stderr) failed");
                }
                poll |= CPipe::fStdErr;
            } else if ( bytes_avail ) {
                poll |= CPipe::fStdErr;
            }
        }
        if ( poll ) {
            break;
        }
        unsigned long x_sleep = kWaitPrecision;
        if (x_timeout != INFINITE) {
            if (x_sleep > x_timeout) {
                x_sleep = x_timeout;
            }
            if ( !x_sleep ) {
                break;
            }
            x_timeout -= x_sleep;
        }
        SleepMilliSec(x_sleep);
    }

    // We cannot poll child's stdin, so just copy corresponding flag
    // from source to result mask before return
    poll |= mask & CPipe::fStdIn ;
    return poll;
}


#elif defined(NCBI_OS_UNIX)

#define PIPE_THROW(err, errtxt)                  \
    {                                            \
        int _err = err;                          \
        string _errstr(errtxt);                  \
        throw s_UnixError(_err, _errstr);        \
    }


static string s_UnixError(int error, string& message)
{
    const char* errstr = error ? strerror(error) : 0;
    if (!errstr) {
        errstr = "";
    }
    int dynamic = 0/*false*/;
    const char* result = ::NcbiMessagePlusError(&dynamic, message.c_str(),
                                                (int) error, errstr);
    string retval;
    if (result) {
        retval = result;
        if (dynamic) {
            free((void*) result);
        }
    } else {
        retval.swap(message);
    }
    return retval;
}


//////////////////////////////////////////////////////////////////////////////
//
// CPipeHandle -- Unix version
//

class CPipeHandle
{
public:
    CPipeHandle(void);
    ~CPipeHandle();
    EIO_Status Open(const string&         cmd,
                    const vector<string>& args,
                    CPipe::TCreateFlags   create_flags,
                    const string&         current_dir,
                    const char* const     env[]);
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

private:
    // Clear object state.
    void x_Clear(void);
    // Get child's I/O handle.
    int  x_GetHandle(CPipe::EChildIOHandle from_handle) const;
    // Trigger blocking mode on specified I/O handle.
    bool x_SetNonBlockingMode(int fd) const;
    // Wait on the file descriptors I/O.
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
};


CPipeHandle::CPipeHandle(void)
    : m_ChildStdIn(-1), m_ChildStdOut(-1), m_ChildStdErr(-1),
      m_Pid((pid_t)(-1)), m_Flags(0),
      m_SelfHandles(false)
{
}


CPipeHandle::~CPipeHandle()
{
    static const STimeout kZeroTimeout = {0, 0};
    Close(0, &kZeroTimeout);
    x_Clear();
}


// Auxiliary function for exit from forked process with reporting errno
// on errors to specified file descriptor 
static void s_Exit(int status, int fd)
{
    int errcode = errno;
    (void) ::write(fd, &errcode, sizeof(errcode));
    ::close(fd);
    ::_exit(status);
}


// Emulate inexistent function execvpe().
// On success, execve() does not return, on error -1 is returned,
// and errno is set appropriately.

static int s_ExecShell(const char *file,
                       char *const argv[], char *const envp[])
{
    static const char kShell[] = "/bin/sh";

    // Count number of arguments
    int i;
    for (i = 0;  argv[i];  i++);
    i++; // copy last zero element also
    
    // Construct an argument list for the shell.
    // Not all compilers support next construction:
    //   const char* args[i + 1];
    const char **args = new const char*[i + 1];
    AutoPtr<const char*,  ArrayDeleter<const char*> > args_ptr(args);

    args[0] = kShell;
    args[1] = file;
    for (;  i > 1;  i--) {
        args[i] = argv[i - 1];
    }

    // Execute the shell
    return ::execve(kShell, (char**) args, envp);
}


static int s_ExecVPE(const char *file, char *const argv[], char *const envp[])
{
    // CAUTION (security):  current directory is in the path on purpose,
    //                      to be in-sync with default behavior of MS-Win.
    static const char* kPathDefault = ":/bin:/usr/bin";

    // If file name is not specified
    if (!file  ||  *file == '\0') {
        errno = ENOENT;
        return -1;
    }
    // If the file name contains path
    if ( strchr(file, '/') ) {
        ::execve(file, argv, envp);
        if (errno == ENOEXEC) {
            return s_ExecShell(file, argv, envp);
        }
        return -1;
    }
    // Get PATH environment variable   
    const char *path = getenv("PATH");
    if ( !path ) {
        path = kPathDefault;
    }
    size_t file_len = strlen(file) + 1 /* '\0' */;
    size_t buf_len = strlen(path) + file_len + 1 /* '/' */;
    char* buf = new char[buf_len];
    if ( !buf ) {
        errno = ENOMEM;
        return -1;
    }
    AutoPtr<char, ArrayDeleter<char> > buf_ptr(buf);

    bool eacces_err = false;
    const char* next = path;
    while (*next) {
        next = strchr(path,':');
        if ( !next ) {
            // Last part of the PATH environment variable
            next = path + strlen(path);
        }
        size_t len = next - path;
        if ( len ) {
            // Copy directory name into the buffer
            memmove(buf, path, next - path);
        } else {
            // Two colons side by side -- current directory
            buf[0]='.';
            len = 1;
        }
        // Add slash and file name
        if (buf[len-1] != '/') {
            buf[len++] = '/';
        }
        memcpy(buf + len, file, file_len);
        path = next + 1;

        // Try to execute file with generated name
        ::execve(buf, argv, envp);
        if (errno == ENOEXEC) {
            return s_ExecShell(buf, argv, envp);
        }
        switch (errno) {
        case EACCES:
            // Permission denied. Memorize this thing and try next path.
            eacces_err = true;
        case ENOENT:
        case ENOTDIR:
            // Try next path directory
            break;
        default:
            // We found an executable file, but could not execute it
            return -1;
        }
    }
    if ( eacces_err ) {
        errno = EACCES;
    }
    return -1;
}


static int x_SafeFD(int fd, int safe)
{
    if (fd == safe  ||  fd > STDERR_FILENO)
        return fd;
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
                             const char* const     env[])

{
    DEFINE_STATIC_FAST_MUTEX(s_Mutex);
    CFastMutexGuard guard_mutex(s_Mutex);

    x_Clear();
    m_Flags = create_flags;

    // Child process I/O handles
    int pipe_in[2], pipe_out[2], pipe_err[2];
    pipe_in[0]  = -1;
    pipe_out[1] = -1;
    pipe_err[1] = -1;

    int status_pipe[2] = {-1, -1};
    try {
        // Flush stdio
        NcbiCout.flush();
        NcbiCerr.flush();
        ::fflush(NULL);

        // Create pipe for child's stdin
        _ASSERT(CPipe::fStdIn_Close);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            if (::pipe(pipe_in) < 0
                ||  !x_SafePipe(pipe_in, 0, STDIN_FILENO)) {
                pipe_in[0] = -1;
                PIPE_THROW(errno, "Failed to create pipe for stdin");
            }
            m_ChildStdIn = pipe_in[1];
            x_SetNonBlockingMode(m_ChildStdIn);
        }

        // Create pipe for child's stdout
        _ASSERT(CPipe::fStdOut_Close);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            if (::pipe(pipe_out) < 0
                ||  !x_SafePipe(pipe_out, 1, STDOUT_FILENO)) {
                pipe_out[1] = -1;
                PIPE_THROW(errno, "Failed to create pipe for stdout");
            }
            m_ChildStdOut = pipe_out[0];
            x_SetNonBlockingMode(m_ChildStdOut);
        }

        // Create pipe for child's stderr
        _ASSERT(CPipe::fStdErr_Open);
        if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            if (::pipe(pipe_err) < 0
                ||  !x_SafePipe(pipe_err, 1, STDERR_FILENO)) {
                pipe_err[1] = -1;
                PIPE_THROW(errno, "Failed to create pipe for stderr");
            }
            m_ChildStdErr = pipe_err[0];
            x_SetNonBlockingMode(m_ChildStdErr);
        }

        // Create temporary pipe to get status of execution
        // of the child process
        if (::pipe(status_pipe) < 0
            ||  !x_SafePipe(status_pipe, -1, -1)) {
            PIPE_THROW(errno, "Failed to create status pipe");
        }
        ::fcntl(status_pipe[1], F_SETFD, 
                ::fcntl(status_pipe[1], F_GETFD, 0) | FD_CLOEXEC);

        // Fork child process
        switch (m_Pid = ::fork()) {
        case (pid_t)(-1):
            PIPE_THROW(errno, "Failed fork()");
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
            size_t cnt = args.size();
            size_t i   = 0;
            const char** x_args = new const char*[cnt + 2];
            typedef ArrayDeleter<const char*> TArgsDeleter;
            AutoPtr<const char*, TArgsDeleter> p_args = x_args;
            ITERATE (vector<string>, arg, args) {
                x_args[++i] = arg->c_str();
            }
            x_args[0] = cmd.c_str();
            x_args[cnt + 1] = 0;

            // Change current working directory if specified
            if ( !current_dir.empty() ) {
                (void) ::chdir(current_dir.c_str());
            }
            // Execute the program
            int status;
            if ( env ) {
                // Emulate inexistent execvpe()
                status = s_ExecVPE(cmd.c_str(),
                                   const_cast<char**>(x_args),
                                   const_cast<char**>(env));
            } else {
                status = ::execvp(cmd.c_str(), const_cast<char**>(x_args));
            }
            s_Exit(status, status_pipe[1]);

            // *** CHILD PROCESS DOES NOT CONTINUE BEYOND THIS LINE ***
        }

        // Close unused pipe handles
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

        // Try to read errno from forked process
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
            ::waitpid(m_Pid, NULL, 0);
            string errmsg("Failed to execute \"" + cmd + '"');
            if ((size_t) n >= sizeof(errcode)  &&  errcode) {
                PIPE_THROW(errcode, errmsg);
            }
            throw errmsg;
        }

        return eIO_Success;
    } 
    catch (string& what) {
        // Close all opened file descriptors
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
        static const STimeout kZeroZimeout = {0, 0};
        Close(0, &kZeroZimeout);
        ERR_POST_X(1, what);
        x_Clear();
    }
    return eIO_Unknown;
}


void CPipeHandle::OpenSelf(void)
{
    x_Clear();

    NcbiCout.flush();
    ::fflush(stdout);
    m_ChildStdIn  = fileno(stdout);  // NB: a macro on BSD, so no "::" scope
    m_ChildStdOut = fileno(stdin);
    m_Pid = ::getpid();

    m_SelfHandles = true;
}


void CPipeHandle::x_Clear(void)
{
    m_Pid = -1;
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
            status = s_Close(CProcess(m_Pid, CProcess::ePid),
                             m_Flags, timeout, exitcode);
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
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


EIO_Status CPipeHandle::Read(void* buf, size_t count, size_t* n_read, 
                             const CPipe::EChildIOHandle from_handle,
                             const STimeout* timeout) const
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pid == (pid_t)(-1)) {
            status = eIO_Closed;
            throw string("Pipe closed");
        }
        int fd = x_GetHandle(from_handle);
        if (fd == -1) {
            status = eIO_Closed;
            throw string("Pipe I/O handle closed");
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

            // Blocked -- wait for data to come;  exit if timeout/error
            if (errno == EAGAIN  ||  errno == EWOULDBLOCK) {
                if ( (timeout  &&  !(timeout->sec | timeout->usec ))
                     ||  !x_Poll(from_handle, timeout) ) {
                    status = eIO_Timeout;
                    break;
                }
                continue;
            }

            // Interrupted read -- restart
            if (errno != EINTR) {
                PIPE_THROW(errno, "Failed to read data from pipe");
            }
        }
    }
    catch (string& what) {
        ERR_POST_X(2, what);
    }
    return status;
}


EIO_Status CPipeHandle::Write(const void* buf, size_t count,
                              size_t* n_written, const STimeout* timeout) const

{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pid == (pid_t)(-1)) {
            status = eIO_Closed;
            throw string("Pipe closed");
        }
        if (m_ChildStdIn == -1) {
            status = eIO_Closed;
            throw string("Pipe I/O handle closed");
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
                status = eIO_Success;
                break;
            }

            // Peer has closed its end
            if (errno == EPIPE) {
                return eIO_Closed;
            }

            // Blocked -- wait for write readiness;  exit if timeout/error
            if (errno == EAGAIN  ||  errno == EWOULDBLOCK) {
                if ( (timeout  &&  !(timeout->sec | timeout->usec))
                     ||  !x_Poll(CPipe::fStdIn, timeout) ) {
                    status = eIO_Timeout;
                    break;
                }
                continue;
            }

            // Interrupted write -- restart
            if (errno != EINTR) {
                PIPE_THROW(errno, "Failed to write data into pipe");
            }
        }
    }
    catch (string& what) {
        ERR_POST_X(3, what);
    }
    return status;
}


CPipe::TChildPollMask CPipeHandle::Poll(CPipe::TChildPollMask mask,
                                        const STimeout* timeout) const
{
    CPipe::TChildPollMask poll = 0;

    try {
        if (m_Pid == (pid_t)(-1)) {
            throw string("Pipe closed");
        }
        if (m_ChildStdIn  == -1  &&
            m_ChildStdOut == -1  &&
            m_ChildStdErr == -1) {
            throw string("All pipe I/O handles closed");
        }
        poll = x_Poll(mask, timeout);
    }
    catch (string& what) {
        ERR_POST_X(4, what);
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
        break;
    }
    return -1;
}


bool CPipeHandle::x_SetNonBlockingMode(int fd) const
{
    return ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) |  O_NONBLOCK) != -1;
}


CPipe::TChildPollMask CPipeHandle::x_Poll(CPipe::TChildPollMask mask,
                                          const STimeout* timeout) const
{
    CPipe::TChildPollMask poll = 0;

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
            FD_SET(m_ChildStdIn,  &wfds);
            FD_SET(m_ChildStdIn,  &efds);
            if (max < m_ChildStdIn) {
                max = m_ChildStdIn;
            }
        }
        if ( (mask & CPipe::fStdOut)  &&  m_ChildStdOut != -1 ) {
            if (!rd) {
                rd = true;
                FD_ZERO(&rfds);
            }
            FD_SET(m_ChildStdOut, &rfds);
            FD_SET(m_ChildStdOut, &efds);
            if (max < m_ChildStdOut) {
                max = m_ChildStdOut;
            }
        }
        if ( (mask & CPipe::fStdErr)  &&  m_ChildStdErr != -1 ) {
            if (!rd) {
                rd = true;
                FD_ZERO(&rfds);
            }
            FD_SET(m_ChildStdErr, &rfds);
            FD_SET(m_ChildStdErr, &efds);
            if (max < m_ChildStdErr) {
                max = m_ChildStdErr;
            }
        }
        _ASSERT(rd  ||  wr);

        int n = ::select(max + 1, rd ? &rfds : 0, wr ? &wfds : 0, &efds, tmp);

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
            PIPE_THROW(n, "Failed select() on pipe");
        }
        // continue
    }
    return poll;
}


#endif // NCBI_OS_UNIX | NCBI_OS_MSWIN


//////////////////////////////////////////////////////////////////////////////
//
// CPipe
//

CPipe::CPipe(void)
    : m_PipeHandle(new CPipeHandle), m_ReadHandle(eStdOut),
      m_ReadStatus(eIO_Closed), m_WriteStatus(eIO_Closed),
      m_ReadTimeout(0), m_WriteTimeout(0), m_CloseTimeout(0)
{
}


CPipe::CPipe(const string&         cmd,
             const vector<string>& args,
             TCreateFlags          create_flags,
             const string&         current_dir,
             const char*   const   env[])
    : m_PipeHandle(0), m_ReadHandle(eStdOut),
      m_ReadStatus(eIO_Closed), m_WriteStatus(eIO_Closed),
      m_ReadTimeout(0), m_WriteTimeout(0), m_CloseTimeout(0)
{
    auto_ptr<CPipeHandle> handle_ptr(new CPipeHandle);
    m_PipeHandle = handle_ptr.get();
    EIO_Status status = Open(cmd, args, create_flags, current_dir, env);
    if (status != eIO_Success) {
        NCBI_THROW(CPipeException, eOpen,
                   "CPipe::Open(): " + string(IO_StatusStr(status)));
    }
    handle_ptr.release();
}


CPipe::~CPipe()
{
    Close();
    if ( m_PipeHandle ) {
        delete m_PipeHandle;
    }
}


EIO_Status CPipe::Open(const string& cmd, const vector<string>& args,
                       TCreateFlags  create_flags,
                       const string& current_dir,
                       const char*   const env[])
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }

    EIO_Status status = m_PipeHandle->Open(cmd, args, create_flags,
                                           current_dir, env);
    if (status == eIO_Success) {
        m_ReadStatus  = eIO_Success;
        m_WriteStatus = eIO_Success;
    }
    return status;
}


void CPipe::OpenSelf(void)
{
    try {
        if ( !m_PipeHandle ) {
            throw string("Pipe invalid");
        }
        m_PipeHandle->OpenSelf();
    }
    catch (string& err) {
        NCBI_THROW(CPipeException, eOpen,
                   "CPipe::OpenSelf(): " + err);
    }
    m_ReadStatus  = eIO_Success;
    m_WriteStatus = eIO_Success;
}


EIO_Status CPipe::Close(int* exitcode)
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    m_ReadStatus  = eIO_Closed;
    m_WriteStatus = eIO_Closed;

    return m_PipeHandle->Close(exitcode, m_CloseTimeout);
}


EIO_Status CPipe::CloseHandle(EChildIOHandle handle)
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    return m_PipeHandle->CloseHandle(handle);
}


EIO_Status CPipe::SetReadHandle(EChildIOHandle from_handle)
{
    if (from_handle == eStdIn) {
        return eIO_InvalidArg;
    }
    m_ReadHandle = from_handle == eDefault ? eStdOut : from_handle;
    return eIO_Success;
}


EIO_Status CPipe::Read(void* buf, size_t count, size_t* read,
                       EChildIOHandle from_handle)
{
    if ( read ) {
        *read = 0;
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
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    m_ReadStatus = m_PipeHandle->Read(buf, count, read, from_handle,
                                      m_ReadTimeout);
    return m_ReadStatus;
}


EIO_Status CPipe::Write(const void* buf, size_t count, size_t* written)
{
    if ( written ) {
        *written = 0;
    }
    if (count  &&  !buf) {
        return eIO_InvalidArg;
    }
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    m_WriteStatus = m_PipeHandle->Write(buf, count, written,
                                        m_WriteTimeout);
    return m_WriteStatus;
}


CPipe::TChildPollMask CPipe::Poll(TChildPollMask mask, 
                                  const STimeout* timeout)
{
    if (!mask  ||  !m_PipeHandle) {
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
    switch ( direction ) {
    case eIO_Read:
        return m_PipeHandle ? m_ReadStatus  : eIO_Closed;
    case eIO_Write:
        return m_PipeHandle ? m_WriteStatus : eIO_Closed;
    default:
        break;
    }
    return eIO_InvalidArg;
}


EIO_Status CPipe::SetTimeout(EIO_Event event, const STimeout* timeout)
{
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
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


const STimeout* CPipe::GetTimeout(EIO_Event event) const
{
    switch ( event ) {
    case eIO_Close:
        return m_CloseTimeout;
    case eIO_Read:
        return m_ReadTimeout;
    case eIO_Write:
        return m_WriteTimeout;
    default:
        break;
    }
    return kDefaultTimeout;
}


TProcessHandle CPipe::GetProcessHandle(void) const
{
    return m_PipeHandle ? m_PipeHandle->GetProcessHandle() : 0;
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
                               const STimeout*         kill_timeout)
{
    STimeout ktm;

    if (kill_timeout) {
        ktm = *kill_timeout;
    } else {
        NcbiMsToTimeout(&ktm, CProcess::kDefaultKillTimeout);
    }

    CPipe pipe;
    EIO_Status st = pipe.Open(cmd, args, 
                              fStdErr_Open | fSigPipe_Restore
                              | fNewGroup | fKillOnClose,
                              current_dir, env);
    if (st != eIO_Success) {
        NCBI_THROW(CPipeException, eOpen, "Cannot execute \"" + cmd + "\"");
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
    
    const size_t buf_size = 4096;
    char buf[buf_size];
    size_t bytes_in_inbuf = 0;
    size_t total_bytes_written = 0;
    char inbuf[buf_size];

    TChildPollMask mask = fStdIn | fStdOut | fStdErr;
    try {
        STimeout wait_time = {1, 0};
        while (!out_done  ||  !err_done) {
            EIO_Status rstatus;
            size_t bytes_read;

            TChildPollMask rmask = pipe.Poll(mask, &wait_time);
            if (rmask & fStdIn  &&  !in_done) {
                if (in.good()  &&  bytes_in_inbuf == 0) {
                    bytes_in_inbuf =
                        (size_t)CStreamUtils::Readsome(in, inbuf, buf_size);
                    total_bytes_written = 0;
                }

                size_t bytes_written;
                if (bytes_in_inbuf > 0) {
                    rstatus =
                        pipe.Write(inbuf + total_bytes_written,
                                   bytes_in_inbuf, &bytes_written);
                    if (rstatus != eIO_Success) {
                        ERR_POST_X(5, "Cannot send all data to child process");
                        in_done = true;
                    }
                    total_bytes_written += bytes_written;
                    bytes_in_inbuf      -= bytes_written;
                }

                if ((!in.good()  &&  bytes_in_inbuf == 0)  ||  in_done) {
                    pipe.CloseHandle(eStdIn);
                    mask &= ~fStdIn;
                }

            }
            if (rmask & fStdOut) {
                // read stdout
                if (!out_done) {
                    rstatus = pipe.Read(buf, buf_size, &bytes_read);
                    out.write(buf, bytes_read);
                    if (rstatus != eIO_Success) {
                        out_done = true;
                        mask &= ~fStdOut;
                    }
                }

            }
            if ((rmask & fStdErr)  &&  !err_done) {
                rstatus = pipe.Read(buf, buf_size, &bytes_read, eStdErr);
                err.write(buf, bytes_read);
                if (rstatus != eIO_Success) {
                    err_done = true;
                    mask &= ~fStdErr;
                }
            }
            if (!CProcess(pid).IsAlive())
                break;
            if (watcher && watcher->Watch(pid) != IProcessWatcher::eContinue) {
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
