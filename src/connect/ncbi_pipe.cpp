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
 * Author:  Anton Lavrentiev, Mike DiCuccio, Vladimir Ivanov
 *
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_pipe.hpp>
#include <corelib/ncbi_system.hpp>
#include <assert.h>
#include <memory>
#include <stdio.h>

#ifdef NCBI_OS_MSWIN
#  include <windows.h>
#elif defined NCBI_OS_UNIX
#  include <unistd.h>
#  include <errno.h>
#  include <sys/time.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <signal.h>
#  include <fcntl.h>
#  ifdef NCBI_COMPILER_MW_MSL
#    include <ncbi_mslextras.h>
#  endif
#else
#  error "Class CPipe is supported only on Windows and Unix"
#endif


BEGIN_NCBI_SCOPE


// Sleep time for timeouts
const unsigned int kSleepTime = 100;


//////////////////////////////////////////////////////////////////////////////
//
// Auxiliary functions
//

#define IS_SET(flags, mask) (((flags) & (mask)) == (mask))


static STimeout* s_SetTimeout(const STimeout* from, STimeout* to)
{
    if ( !from ) {
        return const_cast<STimeout*>(kInfiniteTimeout);
    }
    to->sec  = from->usec / 1000000 + from->sec;
    to->usec = from->usec % 1000000;
    return to;
}

static string s_FormatErrorMessage(const string& where, const string& what)
{
    return "[CPipe::" + where + "]  " + what + ".";
}


//////////////////////////////////////////////////////////////////////////////
//
// Class CNamedPipeHandle handles forwarded requests from CPipe.
// This class is reimplemented in a platform-specific fashion where needed.
//

#if defined(NCBI_OS_MSWIN)

//////////////////////////////////////////////////////////////////////////////
//
// CPipeHandle -- MS Windows version
//

class CPipeHandle
{
public:
    CPipeHandle();
    ~CPipeHandle();
    EIO_Status Open(const string& cmd, const vector<string>& args,
                    CPipe::TCreateFlags create_flags);
    EIO_Status Close(int* exitcode, const STimeout* timeout);
    EIO_Status CloseHandle (CPipe::EChildIOHandle handle);
    EIO_Status Read(void* buf, size_t count, size_t* n_read,
                    const CPipe::EChildIOHandle from_handle,
                    const STimeout* timeout);
    EIO_Status Write(const void* buf, size_t count, size_t* written,
                     const STimeout* timeout);
    TProcessHandle GetProcessHandle(void) const { return m_ProcHandle; };

private:
    // Clear object state.
    void x_Clear(void);
    // Get child's I/O handle.
    HANDLE x_GetHandle(CPipe::EChildIOHandle from_handle) const;
    // Convert STimeout value to number of milliseconds.
    long   x_TimeoutToMSec(const STimeout* timeout) const;
    // Trigger blocking mode on specified I/O handle.
    bool   x_SetNonBlockingMode(HANDLE fd, bool nonblock = true) const;

private:
    // I/O handles for child process.
    HANDLE  m_ChildStdIn;
    HANDLE  m_ChildStdOut;
    HANDLE  m_ChildStdErr;

    // Child process descriptor.
    HANDLE  m_ProcHandle;
    // Child process pid.
    TPid    m_Pid;

    // Pipe flags
    CPipe::TCreateFlags m_Flags;
};


CPipeHandle::CPipeHandle()
    : m_ProcHandle(INVALID_HANDLE_VALUE),
      m_ChildStdIn(INVALID_HANDLE_VALUE),
      m_ChildStdOut(INVALID_HANDLE_VALUE),
      m_ChildStdErr(INVALID_HANDLE_VALUE),
      m_Flags(0)
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
                             CPipe::TCreateFlags   create_flags)
{
    x_Clear();

    bool need_restore_handles = false; 
    m_Flags = create_flags;

    HANDLE stdin_handle       = INVALID_HANDLE_VALUE;
    HANDLE stdout_handle      = INVALID_HANDLE_VALUE;
    HANDLE stderr_handle      = INVALID_HANDLE_VALUE;
    HANDLE child_stdin_read   = INVALID_HANDLE_VALUE;
    HANDLE child_stdin_write  = INVALID_HANDLE_VALUE;
    HANDLE child_stdout_read  = INVALID_HANDLE_VALUE;
    HANDLE child_stdout_write = INVALID_HANDLE_VALUE;
    HANDLE child_stderr_read  = INVALID_HANDLE_VALUE;
    HANDLE child_stderr_write = INVALID_HANDLE_VALUE;

    try {
        if (m_ProcHandle != INVALID_HANDLE_VALUE) {
            throw string("Pipe is already open");
        }

        // Save current I/O handles
        stdin_handle  = GetStdHandle(STD_INPUT_HANDLE);
        stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
        
        // Flush std.output buffers before remap
        FlushFileBuffers(stdout_handle);
        FlushFileBuffers(stderr_handle);

        // Set base security attributes
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.bInheritHandle = TRUE;
        attr.lpSecurityDescriptor = NULL;

        HANDLE current_process = GetCurrentProcess();
        need_restore_handles = true; 

        // Create pipe for child's stdin
        assert(CPipe::fStdIn_Close);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            if ( !CreatePipe(&child_stdin_read,
                             &child_stdin_write, &attr, 0) ) {
                throw string("CreatePipe() failed");
            }
            if ( !SetStdHandle(STD_INPUT_HANDLE, child_stdin_read) ) {
                throw string("Failed to remap stdin for child process");
            }
            // Duplicate the handle
            if ( !DuplicateHandle(current_process, child_stdin_write,
                                  current_process, &m_ChildStdIn,
                                  0, FALSE, DUPLICATE_SAME_ACCESS) ) {
                throw string("DuplicateHandle() failed on "
                             "child's stdin handle");
            }
            ::CloseHandle(child_stdin_write);
            x_SetNonBlockingMode(m_ChildStdIn);
        }

        // Create pipe for child's stdout
        assert(CPipe::fStdOut_Close);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            if ( !CreatePipe(&child_stdout_read,
                             &child_stdout_write, &attr, 0)) {
                throw string("CreatePipe() failed");
            }
            if ( !SetStdHandle(STD_OUTPUT_HANDLE, child_stdout_write) ) {
                throw string("Failed to remap stdout for child process");
            }
            // Duplicate the handle
            if ( !DuplicateHandle(current_process, child_stdout_read,
                                  current_process, &m_ChildStdOut,
                                  0, FALSE, DUPLICATE_SAME_ACCESS) ) {
                throw string("DuplicateHandle() failed on "
                             "child's stdout handle");
            }
            ::CloseHandle(child_stdout_read);
            x_SetNonBlockingMode(m_ChildStdOut);
        }

        // Create pipe for child's stderr
        assert(CPipe::fStdErr_Open);
        if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            if ( !CreatePipe(&child_stderr_read,
                             &child_stderr_write, &attr, 0)) {
                throw string("CreatePipe() failed");
            }
            if ( !SetStdHandle(STD_ERROR_HANDLE, child_stderr_write) ) {
                throw string("Failed to remap stderr for child process");
            }
            // Duplicate the handle
            if ( !DuplicateHandle(current_process, child_stderr_read,
                                  current_process, &m_ChildStdErr,
                                  0, FALSE, DUPLICATE_SAME_ACCESS) ) {
                throw string("DuplicateHandle() failed on "
                             "child's stderr handle");
            }
            ::CloseHandle(child_stderr_read);
            x_SetNonBlockingMode(m_ChildStdErr);
        }

        // Prepare command line to run
        string cmd_line(cmd);
        ITERATE (vector<string>, iter, args) {
            if ( !cmd_line.empty() ) {
                cmd_line += " ";
            }
            cmd_line += *iter;
        }

        // Create child process
        PROCESS_INFORMATION pinfo;
        STARTUPINFO sinfo;
        ZeroMemory(&pinfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&sinfo, sizeof(STARTUPINFO));
        sinfo.cb = sizeof(sinfo);

        if ( !CreateProcess(NULL,
                            const_cast<char*> (cmd_line.c_str()),
                            NULL, NULL, TRUE, 0,
                            NULL, NULL, &sinfo, &pinfo) ) {
            throw "CreateProcess() for \"" + cmd_line + "\" failed";
        }
        ::CloseHandle(pinfo.hThread);
        m_ProcHandle = pinfo.hProcess;
        m_Pid        = pinfo.dwProcessId; 

        assert(m_ProcHandle != INVALID_HANDLE_VALUE);

        // Restore remapped handles back to their original states
        if ( !SetStdHandle(STD_INPUT_HANDLE,  stdin_handle) ) {
            throw string("Failed to remap stdin for parent process");
        }
        if ( !SetStdHandle(STD_OUTPUT_HANDLE, stdout_handle) ) {
            throw string("Failed to remap stdout for parent process");
        }
        if ( !SetStdHandle(STD_ERROR_HANDLE,  stderr_handle) ) {
            throw string("Failed to remap stderr for parent process");
        }
        // Close unused pipe handles
        ::CloseHandle(child_stdin_read);
        ::CloseHandle(child_stdout_write);
        ::CloseHandle(child_stderr_write);

        return eIO_Success;
     }
    catch (string& what) {
        // Restore all standard handles on error
        if ( need_restore_handles ) {
            SetStdHandle(STD_INPUT_HANDLE,  stdin_handle);
            SetStdHandle(STD_OUTPUT_HANDLE, stdout_handle);
            SetStdHandle(STD_ERROR_HANDLE,  stderr_handle);

            ::CloseHandle(child_stdin_read);
            ::CloseHandle(child_stdin_write);
            ::CloseHandle(child_stdout_read);
            ::CloseHandle(child_stdout_write);
            ::CloseHandle(child_stderr_read);
            ::CloseHandle(child_stderr_write);
        }
        const STimeout kZeroZimeout = {0,0};
        Close(0, &kZeroZimeout);
        ERR_POST(s_FormatErrorMessage("Open", what));
        return eIO_Unknown;
    }
}


EIO_Status CPipeHandle::Close(int* exitcode, const STimeout* timeout)
{
    if (m_ProcHandle == INVALID_HANDLE_VALUE) {
        return eIO_Closed;
    }

    DWORD      x_exitcode = -1;
    EIO_Status status     = eIO_Unknown;

    // Get exit code of child process
    if ( GetExitCodeProcess(m_ProcHandle, &x_exitcode) ) {
        if (x_exitcode == STILL_ACTIVE) {
            // Wait for the child process to exit
            DWORD ws = WaitForSingleObject(m_ProcHandle,
                                           x_TimeoutToMSec(timeout));
            if (ws == WAIT_OBJECT_0) {
                // Get exit code of child process over again
                if ( GetExitCodeProcess(m_ProcHandle, &x_exitcode) ) {
                    status = (x_exitcode == STILL_ACTIVE) ? 
                        eIO_Timeout : eIO_Success;
                }
            } else if (ws == WAIT_TIMEOUT) {
                status = eIO_Timeout;
            }
        } else {
            status = eIO_Success;
        }
    }

    // Is the process running?
    if (status == eIO_Timeout) {
        x_exitcode = -1;
        assert(CPipe::fKillOnClose);
        if ( IS_SET(m_Flags, CPipe::fKillOnClose) ) {
            status = CProcess(m_Pid, CProcess::ePid).Kill() ?
                              eIO_Success : eIO_Unknown;
        }
        // Active by default.
        assert(!CPipe::fCloseOnClose);
        if ( !IS_SET(m_Flags, CPipe::fCloseOnClose) ) {
            status = eIO_Success;
        }
        // fKeepOnClose -- nothing to do.
        assert(CPipe::fKeepOnClose);
    }

    // Is the process still running? Nothing to do.
    if (status != eIO_Timeout) {
        x_Clear();
    }
    if ( exitcode ) {
        *exitcode = (int) x_exitcode;
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
                             const STimeout* timeout)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_ProcHandle == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw string("Pipe is closed");
        }
        HANDLE fd = x_GetHandle(from_handle);
        if (fd == INVALID_HANDLE_VALUE) {
            throw string("Pipe I/O handle is closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout   = x_TimeoutToMSec(timeout);
        DWORD bytes_avail = 0;
        DWORD bytes_read  = 0;

        // Wait for data from the pipe with timeout.
        // NOTE:  The function WaitForSingleObject() does not work with pipes.
        do {
            if ( !PeekNamedPipe(fd, NULL, 0, NULL, &bytes_avail, NULL) ) {
                // Has peer closed the connection?
                if (GetLastError() == ERROR_BROKEN_PIPE) {
                    return eIO_Closed;
                }
                throw string("PeekNamedPipe() failed");
            }
            if ( bytes_avail ) {
                break;
            }
            DWORD x_sleep = kSleepTime;
            if (x_timeout != INFINITE) {
                if (x_timeout < kSleepTime) {
                    x_sleep = x_timeout;
                }
                x_timeout -= x_sleep;
            }
            SleepMilliSec(x_sleep);
        } while (x_timeout == INFINITE  ||  x_timeout);

        // Data is available to read or read request has timed out
        if ( !bytes_avail ) {
            return eIO_Timeout;
        }
        // We must read only "count" bytes of data regardless of
        // the amount available to read
        if (bytes_avail > count) {
            bytes_avail = count;
        }
        if ( !ReadFile(fd, buf, count, &bytes_avail, NULL) ) {
            throw string("Failed to read data from pipe");
        }
        if ( read ) {
            *read = bytes_avail;
        }
        status = eIO_Success;
    }
    catch (string& what) {
        ERR_POST(s_FormatErrorMessage("Read", what));
    }
    return status;
}


EIO_Status CPipeHandle::Write(const void* buf, size_t count,
                              size_t* n_written, const STimeout* timeout)

{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_ProcHandle == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw string("Pipe is closed");
        }
        if (m_ChildStdIn == INVALID_HANDLE_VALUE) {
            throw string("Pipe I/O handle is closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout     = x_TimeoutToMSec(timeout);
        DWORD bytes_written = 0;

        // Wait for data from the pipe with timeout.
        // NOTE:  The function WaitForSingleObject() does not work with pipes.
        do {
            if ( !WriteFile(m_ChildStdIn, (char*)buf, count,
                           &bytes_written, NULL) ) {
                if ( n_written ) {
                    *n_written = bytes_written;
                }
                throw string("Failed to write data into pipe");
            }
            if ( bytes_written ) {
                break;
            }
            DWORD x_sleep = kSleepTime;
            if (x_timeout != INFINITE) {
                if (x_timeout < kSleepTime) {
                    x_sleep = x_timeout;
                }
                x_timeout -= x_sleep;
            }
            SleepMilliSec(x_sleep);
        } while (x_timeout == INFINITE  ||  x_timeout);

        if ( !bytes_written ) {
            return eIO_Timeout;
        }
        if ( n_written ) {
            *n_written = bytes_written;
        }
        status = eIO_Success;
    }
    catch (string& what) {
        ERR_POST(s_FormatErrorMessage("Write", what));
    }
    return status;
}


void CPipeHandle::x_Clear(void)
{
    if (m_ChildStdIn != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_ChildStdIn);
        m_ChildStdIn = INVALID_HANDLE_VALUE;
    }
    if (m_ChildStdOut != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_ChildStdOut);
        m_ChildStdOut = INVALID_HANDLE_VALUE;
    }
    if (m_ChildStdErr != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_ChildStdErr);
        m_ChildStdErr = INVALID_HANDLE_VALUE;
    }
    m_ProcHandle = INVALID_HANDLE_VALUE;
    m_Pid = 0;
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
    }
    return INVALID_HANDLE_VALUE;
}


long CPipeHandle::x_TimeoutToMSec(const STimeout* timeout) const
{
    return timeout ? (timeout->sec * 1000) + (timeout->usec / 1000) 
        : INFINITE;
}


bool CPipeHandle::x_SetNonBlockingMode(HANDLE fd, bool nonblock) const
{
    // Pipe is in the byte-mode.
    // NOTE: We cannot get a state of a pipe handle opened for writing.
    //       We cannot set a state of a pipe handle opened for reading.
    DWORD state = nonblock ? PIPE_READMODE_BYTE | PIPE_NOWAIT :
        PIPE_READMODE_BYTE;
    return SetNamedPipeHandleState(fd, &state, NULL, NULL) != 0; 
}


#elif defined(NCBI_OS_UNIX)



//////////////////////////////////////////////////////////////////////////////
//
// CPipeHandle -- Unix version
//

class CPipeHandle
{
public:
    CPipeHandle();
    ~CPipeHandle();
    EIO_Status Open(const string& cmd, const vector<string>& args,
                    CPipe::TCreateFlags create_flags);
    EIO_Status Close(int* exitcode, const STimeout* timeout);
    EIO_Status CloseHandle (CPipe::EChildIOHandle handle);
    EIO_Status Read(void* buf, size_t count, size_t* read,
                    const CPipe::EChildIOHandle from_handle,
                    const STimeout* timeout);
    EIO_Status Write(const void* buf, size_t count, size_t* written,
                     const STimeout* timeout);
    TProcessHandle GetProcessHandle(void) const { return m_Pid; };

private:
    // Clear object state.
    void x_Clear(void);
    // Get child's I/O handle.
    int  x_GetHandle(CPipe::EChildIOHandle from_handle) const;
    // Trigger blocking mode on specified I/O handle.
    bool x_SetNonBlockingMode(int fd, bool nonblock = true) const;
    // Wait on the file descriptor I/O.
    // Return eIO_Success when "fd" is found ready for the I/O.
    // Return eIO_Timeout, if timeout expired before I/O became available.
    // Throw an exception on error.
    EIO_Status x_Wait(int fd, EIO_Event direction,
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
};


CPipeHandle::CPipeHandle()
    : m_ChildStdIn(-1), m_ChildStdOut(-1), m_ChildStdErr(-1),
      m_Pid((pid_t)(-1)), m_Flags(0)
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
                             CPipe::TCreateFlags   create_flags)
{
    x_Clear();

    bool need_delete_handles = false; 
    int  fd_pipe_in[2], fd_pipe_out[2], fd_pipe_err[2];

    m_Flags = create_flags;

    // Child process I/O handles
    fd_pipe_in[0]  = -1;
    fd_pipe_out[1] = -1;
    fd_pipe_err[1] = -1;

    try {
        if (m_Pid != (pid_t)(-1)) {
            throw string("Pipe is already open");
        }
        need_delete_handles = true; 

        // Create pipe for child's stdin
        assert(CPipe::fStdIn_Close);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            if (pipe(fd_pipe_in) == -1) {
                throw string("Failed to create pipe for stdin");
            }
            m_ChildStdIn = fd_pipe_in[1];
            x_SetNonBlockingMode(m_ChildStdIn);
        }
        // Create pipe for child's stdout
        assert(CPipe::fStdOut_Close);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            if (pipe(fd_pipe_out) == -1) {
                throw string("Failed to create pipe for stdout");
            }
            fflush(stdout);
            m_ChildStdOut = fd_pipe_out[0];
            x_SetNonBlockingMode(m_ChildStdOut);
        }
        // Create pipe for child's stderr
        assert(CPipe::fStdErr_Open);
        if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            if (pipe(fd_pipe_err) == -1) {
                throw string("Failed to create pipe for stderr");
            }
            fflush(stderr);
            m_ChildStdErr = fd_pipe_err[0];
            x_SetNonBlockingMode(m_ChildStdErr);
        }

        // Fork child process
        switch (m_Pid = fork()) {
        case (pid_t)(-1):
            // Fork failed
            throw string("Failed to fork process");
        case 0:
            // Now we are in the child process
            int status = -1;

            // Bind child's standard I/O file handles to pipe
            assert(CPipe::fStdIn_Close);
            if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
                if (dup2(fd_pipe_in[0], STDIN_FILENO) < 0) {
                    _exit(status);
                }
                close(fd_pipe_in[0]);
                close(fd_pipe_in[1]);
            } else {
                fclose(stdin);
            }
            assert(CPipe::fStdOut_Close);
            if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
                if (dup2(fd_pipe_out[1], STDOUT_FILENO) < 0) {
                    _exit(status);
                }
                close(fd_pipe_out[0]);
                close(fd_pipe_out[1]);
            } else {
                fclose(stdout);
            }
            assert(!CPipe::fStdErr_Close);
            if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
                if (dup2(fd_pipe_err[1], STDERR_FILENO) < 0) {
                    _exit(status);
                }
                close(fd_pipe_err[0]);
                close(fd_pipe_err[1]);
            } else {
                fclose(stderr);
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

            // Execute the program
            status = execvp(cmd.c_str(), const_cast<char**> (x_args));
            _exit(status);
        }

        // Close unused pipe handles
        assert(CPipe::fStdIn_Close);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            close(fd_pipe_in[0]);
        }
        assert(CPipe::fStdOut_Close);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            close(fd_pipe_out[1]);
        }
        assert(!CPipe::fStdErr_Close);
        if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            close(fd_pipe_err[1]);
        }
        return eIO_Success;
    } 
    catch (string& what) {
        if ( need_delete_handles ) {
            close(fd_pipe_in[0]);
            close(fd_pipe_out[1]);
            close(fd_pipe_err[1]);
        }
        // Close all opened file descriptors (close timeout doesn't apply here)
        Close(0, 0);
        ERR_POST(s_FormatErrorMessage("Open", what));
        return eIO_Unknown;
    }
}


EIO_Status CPipeHandle::Close(int* exitcode, const STimeout* timeout)
{    
    EIO_Status    status     = eIO_Unknown;
    unsigned long x_timeout  = 1;
    int           x_options  = 0;
    int           x_exitcode = -1;

    if (m_Pid == (pid_t)(-1)) {
        status = eIO_Closed;
    } else {
        // If timeout is not infinite
        if ( timeout ) {
            x_timeout = (timeout->sec * 1000) + (timeout->usec / 1000);
            x_options = WNOHANG;
        }

        // Retry if interrupted by signal
        for (;;) {
            pid_t ws = waitpid(m_Pid, &x_exitcode, x_options);
            if (ws > 0) {
                // Process has terminated
                status = eIO_Success;
                break;
            } else if (ws == 0) {
                // Process is still running
                assert(timeout);
                if ( !x_timeout ) {
                    status = eIO_Timeout;
                    break;
                }
                unsigned long x_sleep = kSleepTime;
                if (x_timeout < kSleepTime) {
                    x_sleep = x_timeout;
                }
                x_timeout -= x_sleep;
                SleepMilliSec(x_sleep);
            } else {
                // Some error
                if (errno != EINTR) {
                    break;
                }
            }
        }
    }

    // Is the process running?
    if (status == eIO_Timeout) {
        x_exitcode = -1;
        assert(CPipe::fKillOnClose);
        if ( IS_SET(m_Flags, CPipe::fKillOnClose) ) {
            status = CProcess(m_Pid, CProcess::ePid).Kill() ?
                              eIO_Success : eIO_Unknown;
        }
        // Active by default.
        assert(!CPipe::fCloseOnClose);
        if ( !IS_SET(m_Flags, CPipe::fCloseOnClose) ) {
            status = eIO_Success;
        }
        // fKeepOnClose -- nothing to do.
        assert(CPipe::fKeepOnClose);
    }

    // Is the process still running? Nothing to do.
    if (status != eIO_Timeout) {
        x_Clear();
    }
    if ( exitcode ) {
        // Get real exit code or -1 on error
        *exitcode = (status == eIO_Success  &&  x_exitcode != -1) ?
            WEXITSTATUS(x_exitcode) : -1;
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
        close(m_ChildStdIn);
        m_ChildStdIn = -1;
        break;
    case CPipe::eStdOut:
        if (m_ChildStdOut == -1) {
            return eIO_Closed;
        }
        close(m_ChildStdOut);
        m_ChildStdOut = -1;
        break;
    case CPipe::eStdErr:
        if (m_ChildStdErr == -1) {
            return eIO_Closed;
        }
        close(m_ChildStdErr);
        m_ChildStdErr = -1;
        break;
    default:
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


EIO_Status CPipeHandle::Read(void* buf, size_t count, size_t* n_read, 
                             const CPipe::EChildIOHandle from_handle,
                             const STimeout* timeout)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pid == (pid_t)(-1)) {
            status = eIO_Closed;
            throw string("Pipe is closed");
        }
        int fd = x_GetHandle(from_handle);
        if (fd == -1) {
            throw string("Pipe I/O handle is closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        // Retry if either blocked or interrupted
        for (;;) {
            // Try to read
            ssize_t bytes_read = read(fd, buf, count);
            if (bytes_read >= 0) {
                if ( n_read ) {
                    *n_read = (size_t)bytes_read;
                }
                status = bytes_read ? eIO_Success : eIO_Closed;
                break;
            }

            // Blocked -- wait for data to come;  exit if timeout/error
            if (errno == EAGAIN  ||  errno == EWOULDBLOCK) {
                status = x_Wait(fd, eIO_Read, timeout);
                if (status != eIO_Success) {
                    break;
                }
                continue;
            }
            // Interrupted read -- restart
            if (errno != EINTR) {
                throw string("Failed to read data from pipe");
            }
        }
    }
    catch (string& what) {
        ERR_POST(s_FormatErrorMessage("Read", what));
    }
    return status;
}


EIO_Status CPipeHandle::Write(const void* buf, size_t count,
                              size_t* n_written, const STimeout* timeout)

{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pid == (pid_t)(-1)) {
            status = eIO_Closed;
            throw string("Pipe is closed");
        }
        if (m_ChildStdIn == -1) {
            throw string("Pipe I/O handle is closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        // Retry if either blocked or interrupted
        for (;;) {
            // Try to write
            ssize_t bytes_written = write(m_ChildStdIn, buf, count);
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
                status = x_Wait(m_ChildStdIn, eIO_Write, timeout);
                if (status != eIO_Success) {
                    break;
                }
                continue;
            }
            // Interrupted write -- restart
            if (errno != EINTR) {
                throw string("Failed to write data into pipe");
            }
        }
    }
    catch (string& what) {
        ERR_POST(s_FormatErrorMessage("Write", what));
    }
    return status;
}


void CPipeHandle::x_Clear(void)
{
    if (m_ChildStdIn  != -1) {
        close(m_ChildStdIn);
        m_ChildStdIn   = -1;
    }
    if (m_ChildStdOut != -1) {
        close(m_ChildStdOut);
        m_ChildStdOut  = -1;
    }
    if (m_ChildStdErr != -1) {
        close(m_ChildStdErr);
        m_ChildStdErr  = -1;
    }
    m_Pid = -1;
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


bool CPipeHandle::x_SetNonBlockingMode(int fd, bool nonblock) const
{
    return fcntl(fd, F_SETFL,
                 nonblock ?
                 fcntl(fd, F_GETFL, 0) | O_NONBLOCK :
                 fcntl(fd, F_GETFL, 0) & (int) ~O_NONBLOCK) != -1;
}


EIO_Status CPipeHandle::x_Wait(int fd, EIO_Event direction,
                               const STimeout* timeout) const
{
    // Wait for the file descriptor to become ready only
    // if timeout is set or infinite
    if (timeout  &&  !timeout->sec  &&  !timeout->usec) {
        return eIO_Timeout;
    }
    for (;;) { // Auto-resume if interrupted by a signal
        struct timeval* tmp;
        struct timeval  tm;

        if ( timeout ) {
            // NB: Timeout has been normalized already
            tm.tv_sec  = timeout->sec;
            tm.tv_usec = timeout->usec;
            tmp = &tm;
        } else
            tmp = 0;
        fd_set rfds;
        fd_set wfds;
        fd_set efds;
        switch (direction) {
        case eIO_Read:
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            break;
        case eIO_Write:
            FD_ZERO(&wfds);
            FD_SET(fd, &wfds);
            break;
        default:
            assert(0);
            return eIO_Unknown;
        }
        FD_ZERO(&efds);
        FD_SET(fd, &efds);

        int n = select(fd + 1,
                       direction == eIO_Read  ? &rfds : 0,
                       direction == eIO_Write ? &wfds : 0,
                       &efds, tmp);
        if (n == 0) {
            return eIO_Timeout;
        } else if (n > 0) {
            switch (direction) {
            case eIO_Read:
                if ( !FD_ISSET(fd, &rfds) ) {
                    assert( FD_ISSET(fd, &efds) );
                    return eIO_Closed;
                }
                break;
            case eIO_Write:
                if ( !FD_ISSET(fd, &wfds) ) {
                    assert( FD_ISSET(fd, &efds) );
                    return eIO_Closed;
                }
                break;
            default:
                assert(0);
                return eIO_Unknown;
            }
            break;
        } else if (errno != EINTR) {
            throw string("Failed select() on pipe");
        }
    }
    return eIO_Success;
}


#endif  /* NCBI_OS_UNIX | NCBI_OS_MSWIN */



//////////////////////////////////////////////////////////////////////////////
//
// CPipe
//

CPipe::CPipe(void)
    : m_PipeHandle(0), m_ReadHandle(eStdOut),
      m_ReadStatus(eIO_Closed), m_WriteStatus(eIO_Closed),
      m_ReadTimeout(0), m_WriteTimeout(0), m_CloseTimeout(0)
 
{
    // Create new OS-specific pipe handle
    m_PipeHandle = new CPipeHandle();
    if ( !m_PipeHandle ) {
        NCBI_THROW(CPipeException, eInit,
                   "Cannot create OS-specific pipe handle");
    }
}


CPipe::CPipe(const string& cmd, const vector<string>& args,
             TCreateFlags create_flags)
    : m_PipeHandle(0), m_ReadHandle(eStdOut),
      m_ReadStatus(eIO_Closed), m_WriteStatus(eIO_Closed),
      m_ReadTimeout(0), m_WriteTimeout(0), m_CloseTimeout(0)
{
    // Create new OS-specific pipe handle
    m_PipeHandle = new CPipeHandle();
    if ( !m_PipeHandle ) {
        NCBI_THROW(CPipeException, eInit,
                   "Cannot create OS-specific pipe handle");
    }
    EIO_Status status = Open(cmd, args, create_flags);
    if (status != eIO_Success) {
        NCBI_THROW(CPipeException, eOpen, "CPipe::Open() failed");
    }
}


CPipe::~CPipe(void)
{
    Close();
    if ( m_PipeHandle ) {
        delete m_PipeHandle;
    }
}


EIO_Status CPipe::Open(const string& cmd, const vector<string>& args,
                       TCreateFlags create_flags)
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    assert(CPipe::fStdIn_Close);


    EIO_Status status = m_PipeHandle->Open(cmd, args, create_flags);
    if (status == eIO_Success) {
        m_ReadStatus  = eIO_Success;
        m_WriteStatus = eIO_Success;
    }
    return status;
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


EIO_Status CPipe::Read(void* buf, size_t count, size_t* read,
                       EChildIOHandle from_handle)
{
    if ( read ) {
        *read = 0;
    }
    if (from_handle == eDefault)
        from_handle = m_ReadHandle;
    if (from_handle == eStdIn) {
        return eIO_InvalidArg;
    }
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


EIO_Status CPipe::SetReadHandle(EChildIOHandle from_handle)
{
    if (from_handle == eStdIn) {
        return eIO_Unknown;
    }
    if (from_handle != eDefault) {
        m_ReadHandle = from_handle;
    }
    return eIO_Success;
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


EIO_Status CPipe::Status(EIO_Event direction) const
{
    switch ( direction ) {
    case eIO_Read:
        return m_ReadStatus;
    case eIO_Write:
        return m_WriteStatus;
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
        ;
    }
    return kDefaultTimeout;
}


TProcessHandle CPipe::GetProcessHandle(void) const
{
    return m_PipeHandle ? m_PipeHandle->GetProcessHandle() : 0;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.38  2005/04/20 19:13:59  lavr
 * +<assert.h>
 *
 * Revision 1.37  2004/05/17 20:58:13  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.36  2003/12/04 16:28:52  ivanov
 * CPipeHandle::Close(): added handling new create flags f*OnClose.
 * Added GetProcessHandle(). Throw/catch string exceptions instead char*.
 *
 * Revision 1.35  2003/11/14 13:06:01  ucko
 * +<stdio.h> (still needed by source...)
 *
 * Revision 1.34  2003/11/04 03:11:51  lavr
 * x_GetHandle(): default switch case added [for GCC2.95.2 to be happy]
 *
 * Revision 1.33  2003/10/24 16:52:38  lavr
 * Check RW bits before E bits in select()
 *
 * Revision 1.32  2003/09/25 04:40:42  lavr
 * Fixed uninitted member in ctor; few more minor changes
 *
 * Revision 1.31  2003/09/23 21:08:37  lavr
 * PipeStreambuf and special stream removed: now all in ncbi_conn_stream.cpp
 *
 * Revision 1.30  2003/09/16 13:42:36  ivanov
 * Added deleting OS-specific pipe handle in the destructor
 *
 * Revision 1.29  2003/09/09 19:30:33  ivanov
 * Fixed I/O timeout handling in the UNIX CPipeHandle. Cleanup code.
 * Comments and messages changes.
 *
 * Revision 1.28  2003/09/04 13:55:47  kans
 * added include sys/time.h to fix Mac compiler error
 *
 * Revision 1.27  2003/09/03 21:37:05  ivanov
 * Removed Linux ESPIPE workaround
 *
 * Revision 1.26  2003/09/03 20:53:02  ivanov
 * UNIX CPipeHandle::Read(): Added workaround for Linux -- read() returns
 * ESPIPE if a second pipe end is closed.
 * UNIX CPipeHandle::Close(): Implemented close timeout handling.
 *
 * Revision 1.25  2003/09/03 14:35:59  ivanov
 * Fixed previous accidentally commited log message
 *
 * Revision 1.24  2003/09/03 14:29:58  ivanov
 * Set r/w status to eIO_Success in the CPipe::Open()
 *
 * Revision 1.23  2003/09/02 20:32:34  ivanov
 * Moved ncbipipe to CONNECT library from CORELIB.
 * Rewritten CPipe class using I/O timeouts.
 *
 * Revision 1.22  2003/06/18 21:21:48  vakatov
 * Formal code formatting
 *
 * Revision 1.21  2003/06/18 18:59:33  rsmith
 * put in 0,1,2 for fileno(stdin/out/err) for library that does not have
 * fileno().
 *
 * Revision 1.20  2003/05/19 21:21:38  vakatov
 * Get rid of unnecessary unreached statement
 *
 * Revision 1.19  2003/05/08 20:13:35  ivanov
 * Cleanup #include <util/...>
 *
 * Revision 1.18  2003/04/23 20:49:21  ivanov
 * Entirely rewritten the Unix version of the CPipeHandle.
 * Added CPipe/CPipeHandle::CloseHandle().
 * Added flushing a standart output streams before it redirecting.
 *
 * Revision 1.17  2003/04/04 16:02:38  lavr
 * Lines wrapped at 79th column; some minor reformatting
 *
 * Revision 1.16  2003/04/03 14:15:48  rsmith
 * combine pp symbols NCBI_COMPILER_METROWERKS & _MSL_USING_MW_C_HEADERS
 * into NCBI_COMPILER_MW_MSL
 *
 * Revision 1.15  2003/04/02 16:22:34  rsmith
 * clean up metrowerks ifdefs.
 *
 * Revision 1.14  2003/04/02 13:29:53  rsmith
 * include ncbi_mslextras.h when compiling with MSL libs in Codewarrior.
 *
 * Revision 1.13  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 1.12  2003/03/07 16:19:39  ivanov
 * MSWin CPipeHandle::Close() -- Handling GetExitCodeProcess() returns
 * value 259 (STILL_ACTIVE)
 *
 * Revision 1.11  2003/03/06 21:12:10  ivanov
 * Formal comments rearrangement
 *
 * Revision 1.10  2003/03/03 14:47:20  dicuccio
 * Remplemented CPipe using private platform specific classes.  Remplemented
 * Win32 pipes using CreatePipe() / CreateProcess() - enabled CPipe in windows
 * subsystem
 *
 * Revision 1.9  2002/09/05 18:38:07  vakatov
 * Formal code rearrangement to get rid of comp.warnings;  some nice-ification
 *
 * Revision 1.8  2002/07/15 18:17:24  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.7  2002/07/11 14:18:27  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.6  2002/07/02 16:22:25  ivanov
 * Added closing file descriptors of the local end of the pipe in Close()
 *
 * Revision 1.5  2002/06/12 14:20:44  ivanov
 * Fixed some bugs in CPipeStreambuf class.
 *
 * Revision 1.4  2002/06/12 13:48:56  ivanov
 * Fixed contradiction in types of constructor CPipeStreambuf and its
 * realization
 *
 * Revision 1.3  2002/06/11 19:25:35  ivanov
 * Added class CPipeIOStream, CPipeStreambuf
 *
 * Revision 1.2  2002/06/10 18:35:27  ivanov
 * Changed argument's type of a running child program from char*[]
 * to vector<string>
 *
 * Revision 1.1  2002/06/10 16:59:39  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
