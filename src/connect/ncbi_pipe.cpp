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

#include <connect/ncbi_pipe.hpp>
#include <corelib/ncbi_system.hpp>
#include <memory>

#ifdef NCBI_OS_MSWIN
#  include <windows.h>
#elif defined NCBI_OS_UNIX
#  include <unistd.h>
#  include <errno.h>
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


// Default buffer size for CPipe-based iostream.
const streamsize CPipeIOStream::kDefaultBufferSize = 4096;


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

const DWORD kSleepTime = 100;  // sleep time for timeouts


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

private:
    // Get specified child i/o handle.
    HANDLE GetHandle(CPipe::EChildIOHandle from_handle) const;
    // Convert STimeout value to number of milliseconds.
    long TimeoutToMSec(const STimeout* timeout) const;
    // Switch the specified handle i/o between blocking and non-blocking mode.
    bool SetNonBlockingMode(HANDLE fd, bool nonblock = true) const;

private:
    // I/O handles for child process.
    HANDLE  m_ChildStdIn;
    HANDLE  m_ChildStdOut;
    HANDLE  m_ChildStdErr;

    // Child process descriptor.
    HANDLE m_ProcHandle;
};


CPipeHandle::CPipeHandle()
    : m_ProcHandle(INVALID_HANDLE_VALUE),
      m_ChildStdIn(INVALID_HANDLE_VALUE),
      m_ChildStdOut(INVALID_HANDLE_VALUE),
      m_ChildStdErr(INVALID_HANDLE_VALUE)
{
    return;
}


CPipeHandle::~CPipeHandle()
{
    const STimeout kZeroZimeout = {0,0};
    Close(0, &kZeroZimeout);
}


EIO_Status CPipeHandle::Open(const string&         cmd,
                             const vector<string>& args,
                             CPipe::TCreateFlags   create_flags)
{
    bool need_restore_handles = false; 

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
            throw "Pipe is already open";
        }

        // Save current io handles
        stdin_handle  = GetStdHandle(STD_INPUT_HANDLE);
        stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
        
        // Flush std.output buffers before remap
        FlushFileBuffers(stdout_handle);
        FlushFileBuffers(stderr_handle);

        // Set the base security attributes
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.bInheritHandle = TRUE;
        attr.lpSecurityDescriptor = NULL;

        HANDLE current_process = GetCurrentProcess();
        need_restore_handles = true; 

        // Create pipe for the child's stdin
        assert(CPipe::fStdIn_Close);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            if ( !CreatePipe(&child_stdin_read,
                             &child_stdin_write, &attr, 0) ) {
                throw "CreatePipe() failed";
            }
            if ( !SetStdHandle(STD_INPUT_HANDLE, child_stdin_read) ) {
                throw "Remapping stdin for child process failed";
            }
            // Duplicate the handle
            if ( !DuplicateHandle(current_process, child_stdin_write,
                                  current_process, &m_ChildStdIn,
                                  0, FALSE, DUPLICATE_SAME_ACCESS) ) {
                throw "DuplicateHandle() for child's stdin handle failed";
            }
            ::CloseHandle(child_stdin_write);
            SetNonBlockingMode(m_ChildStdIn);
        }

        // Create pipe for the child's stdout
        assert(CPipe::fStdOut_Close);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            if ( !CreatePipe(&child_stdout_read,
                             &child_stdout_write, &attr, 0)) {
                throw "CreatePipe() failed";
            }
            if ( !SetStdHandle(STD_OUTPUT_HANDLE, child_stdout_write) ) {
                throw "Remapping stdout for child process failed";
            }
            // Duplicate the handle
            if ( !DuplicateHandle(current_process, child_stdout_read,
                                  current_process, &m_ChildStdOut,
                                  0, FALSE, DUPLICATE_SAME_ACCESS) ) {
                throw "DuplicateHandle() for child's stdout handle failed";
            }
            ::CloseHandle(child_stdout_read);
            SetNonBlockingMode(m_ChildStdOut);
        }

        // Create pipe for the child's stderr
        assert(CPipe::fStdErr_Open);
        if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            if ( !CreatePipe(&child_stderr_read,
                             &child_stderr_write, &attr, 0)) {
                throw "CreatePipe() failed";
            }
            if ( !SetStdHandle(STD_ERROR_HANDLE, child_stderr_write) ) {
                throw "Remapperr stdin for child process failed";
            }
            // Duplicate the handle
            if ( !DuplicateHandle(current_process, child_stderr_read,
                                  current_process, &m_ChildStdErr,
                                  0, FALSE, DUPLICATE_SAME_ACCESS) ) {
                throw "DuplicateHandle() for child's stderr handle failed";
            }
            ::CloseHandle(child_stderr_read);
            SetNonBlockingMode(m_ChildStdErr);
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
        assert(m_ProcHandle != INVALID_HANDLE_VALUE);

        // Restore remapped handles back to original state
        if ( !SetStdHandle(STD_INPUT_HANDLE,  stdin_handle) ) {
            throw "Remapping stdin for parent process failed";
        }
        if ( !SetStdHandle(STD_OUTPUT_HANDLE, stdout_handle) ) {
            throw "Remapping stdout for parent process failed";
        }
        if ( !SetStdHandle(STD_ERROR_HANDLE,  stderr_handle) ) {
            throw "Remapping stderr for parent process failed";
        }
        // Close unused pipe handles
        ::CloseHandle(child_stdin_read);
        ::CloseHandle(child_stdout_write);
        ::CloseHandle(child_stderr_write);

        return eIO_Success;
     }
    catch (const char* what) {
        // Restore all standard handles on error
        if (need_restore_handles) {
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

    // Get child process exit code
    if ( GetExitCodeProcess(m_ProcHandle, &x_exitcode) ) {
        if (x_exitcode == STILL_ACTIVE) {
            // Wait for the child process to exit
            DWORD ws = WaitForSingleObject(m_ProcHandle,
                                           TimeoutToMSec(timeout));
            if (ws == WAIT_OBJECT_0) {
                // Get child process exit code (second try)
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
    // Is still runing? Have a good slack.
    if (status != eIO_Timeout) {
        m_ProcHandle = INVALID_HANDLE_VALUE;
        if ( m_ChildStdIn != INVALID_HANDLE_VALUE) {
            ::CloseHandle(m_ChildStdIn);
            m_ChildStdIn = INVALID_HANDLE_VALUE;
        }
        if ( m_ChildStdOut != INVALID_HANDLE_VALUE) {
            ::CloseHandle(m_ChildStdOut);
            m_ChildStdOut = INVALID_HANDLE_VALUE;
        }
        if ( m_ChildStdErr != INVALID_HANDLE_VALUE) {
            ::CloseHandle(m_ChildStdErr);
            m_ChildStdErr = INVALID_HANDLE_VALUE;
        }
    }
    if ( exitcode ) {
        *exitcode = (int)x_exitcode;
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
        return eIO_Success;
    case CPipe::eStdOut:
        if (m_ChildStdOut == INVALID_HANDLE_VALUE) {
            return eIO_Closed;
        }
        ::CloseHandle(m_ChildStdOut);
        m_ChildStdOut = INVALID_HANDLE_VALUE;
        return eIO_Success;
    case CPipe::eStdErr:
        if (m_ChildStdErr == INVALID_HANDLE_VALUE) {
            return eIO_Closed;
        }
        ::CloseHandle(m_ChildStdErr);
        m_ChildStdErr = INVALID_HANDLE_VALUE;
        return eIO_Success;
    default:
        // should never get here
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}



EIO_Status CPipeHandle::Read(void* buf, size_t count, size_t* read, 
                             const CPipe::EChildIOHandle from_handle,
                             const STimeout* timeout)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_ProcHandle == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw "Pipe is closed";
        }
        HANDLE fd = GetHandle(from_handle);
        if (fd == INVALID_HANDLE_VALUE) {
            throw "Pipe I/O handle is closed";
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout   = TimeoutToMSec(timeout);
        DWORD bytes_avail = 0;
        DWORD bytes_read  = 0;

        // Wait a data from the pipe with timeout.
        // NOTE:  The function WaitForSingleObject() do not work with pipe.
        do {
            if ( !PeekNamedPipe(fd, NULL, 0, NULL, &bytes_avail, NULL) ) {
                // Peer has been closed the connection?
                if (GetLastError() == ERROR_BROKEN_PIPE) {
                    return eIO_Closed;
                }
                throw "PeekNamedPipe() failed";
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

        // Data is available to read or time out
        if ( !bytes_avail ) {
            return eIO_Timeout;
        }
        // We must read only "count" bytes of data regardless of the number
        // available to read
        if (bytes_avail > count) {
            bytes_avail = count;
        }
        if (!ReadFile(fd, buf, count, &bytes_avail, NULL)) {
            throw "Failed to read data from the named pipe";
        }
        if ( read ) {
            *read = bytes_avail;
        }
        status = eIO_Success;
    }
    catch (const char* what) {
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
            throw "Pipe is closed";
        }
        if (m_ChildStdIn == INVALID_HANDLE_VALUE) {
            throw "Pipe I/O handle is closed";
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout     = TimeoutToMSec(timeout);
        DWORD bytes_written = 0;

        // Wait a data from the pipe with timeout.
        // NOTE:  The function WaitForSingleObject() do not work with pipe.
        do {
            if (!WriteFile(m_ChildStdIn, (char*)buf, count,
                           &bytes_written, NULL)) {
                if ( n_written ) {
                    *n_written = bytes_written;
                }
                throw "Failed to write data into the pipe";
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
    catch (const char* what) {
        ERR_POST(s_FormatErrorMessage("Write", what));
    }
    return status;
}



HANDLE CPipeHandle::GetHandle(CPipe::EChildIOHandle from_handle) const
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


long CPipeHandle::TimeoutToMSec(const STimeout* timeout) const
{
    return timeout ? (timeout->sec * 1000) + (timeout->usec / 1000) 
        : INFINITE;
}


bool CPipeHandle::SetNonBlockingMode(HANDLE fd, bool nonblock) const
{
    // Pipe is in the byte-mode.
    // NOTE: We cannot get a state for pipe handle opened for writing.
    //       We cannot set a state for pipe handle opened for reading.
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

private:
    // Get specified child i/o handle.
    int  GetHandle(CPipe::EChildIOHandle from_handle) const;
    // Switch the specified handle i/o between blocking and non-blocking mode.
    bool SetNonBlockingMode(int fd, bool nonblock = true) const;
    // Select on the pipe i/o.
    // Return eIO_Success when at pipe handle is found either ready.
    // Return eIO_Timeout, if timeout expired before pipe handle became
    // available. Throw exceptions on error.
    EIO_Status Select(int fd, EIO_Event direction,
                      const STimeout* timeout) const;

private:
    // I/O handles for child process.
    int  m_ChildStdIn;
    int  m_ChildStdOut;
    int  m_ChildStdErr;

    // Child process pid.
    pid_t m_Pid;
};


CPipeHandle::CPipeHandle()
    : m_ChildStdIn(-1), m_ChildStdOut(-1), m_ChildStdErr(-1),
      m_Pid(-1)
{
    return;
}


CPipeHandle::~CPipeHandle()
{
    const STimeout kZeroZimeout = {0,0};
    Close(0, &kZeroZimeout);
}


EIO_Status CPipeHandle::Open(const string&         cmd,
                             const vector<string>& args,
                             CPipe::TCreateFlags   create_flags)
{
    bool need_delete_handles = false; 
    int fd_pipe_in[2], fd_pipe_out[2], fd_pipe_err[2];

    // Child process I/O handles
    fd_pipe_in[0]  = -1;
    fd_pipe_out[1] = -1;
    fd_pipe_err[1] = -1;

    try {
        if (m_Pid != -1) {
            throw "Pipe is already open";
        }
        need_delete_handles = true; 

        // Create pipe for the child's stdin
        assert(CPipe::fStdIn_Close);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            if ( pipe(fd_pipe_in) == -1 ) {
                throw "Pipe binding to standard I/O handle failed";
            }
            m_ChildStdIn = fd_pipe_in[1];
        }

        // Create pipe for the child's stdout
        assert(CPipe::fStdOut_Close);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            if ( pipe(fd_pipe_out) == -1 ) {
                throw "Pipe binding to standard I/O file handle failed";
            }
            fflush(stdout);
            m_ChildStdOut = fd_pipe_out[0];
        }

        // Create pipe for the child's stderr
        assert(CPipe::fStdErr_Open);
        if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            if ( pipe(fd_pipe_err) == -1 ) {
                throw "Pipe binding to standard I/O handle failed";
            }
            fflush(stderr);
            m_ChildStdErr = fd_pipe_err[0];
        }

        // Fork child process
        switch (m_Pid = fork()) {
        case -1:
            // Fork failed
            throw "Pipe has failed to fork the current process";
        case 0:
            // Now we are in the child process
            int status = -1;

            // Bind childs standard I/O file handlers to pipe
            if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
#  ifdef NCBI_COMPILER_MW_MSL
                // Codewarrior's MSL library does not have fileno().
                if (dup2(fd_pipe_in[0], 0) < 0)
#  else
                if (dup2(fd_pipe_in[0], fileno(stdin)) < 0)
#  endif
                    {
                        _exit(status);
                    }
                close(fd_pipe_in[0]);
                close(fd_pipe_in[1]);
            }

            if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
#  ifdef NCBI_COMPILER_MW_MSL 
                if (dup2(fd_pipe_out[1], 1) < 0)
#  else
                if (dup2(fd_pipe_out[1], fileno(stdout)) < 0)
#  endif      
                    {
                        _exit(status);
                    }
                close(fd_pipe_out[0]);
                close(fd_pipe_out[1]);
            }

            if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
#  ifdef NCBI_COMPILER_MW_MSL 
                if (dup2(fd_pipe_err[1], 2) < 0)
#  else
                if (dup2(fd_pipe_err[1], fileno(stderr)) < 0)
#  endif      
                    {
                        _exit(status);
                    }
                close(fd_pipe_err[0]);
                close(fd_pipe_err[1]);
            }

            // Prepare a program arguments
            size_t cnt = args.size();
            size_t i   = 0;
            const char** x_args = new const char*[cnt + 2];
            typedef ArrayDeleter<const char*> TArgsDeleter;
            AutoPtr<const char*, TArgsDeleter> p_args = x_args;
            ITERATE (vector<string>, arg, args) {
                x_args[i+1] = arg->c_str();
                ++i;
            }
            x_args[0] = cmd.c_str();
            x_args[cnt + 1] = 0;
            
            // Spawn a program
            status = execvp(cmd.c_str(), const_cast<char**> (x_args));
            _exit(status);
        }

        // Close unused pipe handles
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            close(fd_pipe_in[0]);
        }
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            close(fd_pipe_out[1]);
        }
        if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            close(fd_pipe_err[1]);
        }

        return eIO_Success;
    } 
    catch (const char* what) {
        if (need_delete_handles) {
            close(fd_pipe_in[0]);
            close(fd_pipe_out[1]);
            close(fd_pipe_err[1]);
        }
        const STimeout kZeroZimeout = {0,0};
        Close(0, &kZeroZimeout);
        ERR_POST(s_FormatErrorMessage("Open", what));
        return eIO_Unknown;
    }
}


EIO_Status CPipeHandle::Close(int* exitcode, const STimeout* timeout)
{    
    if (m_Pid == -1) {
        return eIO_Closed;
    }
//?????
///    if (kill(0, m_Pid);
/*
0  running;
-1, errno = ESRCH no process
-1, errno = EPERM running
other - trouble

     If close() is interrupted by a signal that is to be  caught,
     it  will  return -1 with errno set to EINTR and the state of
     fildes is unspecified.

     When all file descriptors associated with  a  pipe  or  FIFO
     special  file  are closed, any data remaining in the pipe or
     FIFO will be discarded.
     If fildes is associated with one end of  a  pipe,  the  last
     close()  causes  a  hangup  to occur on the other end of the
     pipe.  In addition, if the other end of the  pipe  has  been
     named by fattach(3C), then the last close() forces the named
     end to be detached by fdetach(3C).  If the named end has  no
    open  file descriptors associated with it and gets detached,
     the STREAM associated with that end is also dismantled.

RETURN VALUES
     Upon successful completion, 0 is returned. Otherwise, -1  is
     returned and errno is set to indicate the error.

ERRORS
     The close() function will fail if:
     EINTR The close() function was interrupted by a signal.


*/  
    EIO_Status status = eIO_Unknown;

    int x_exitcode = -1;
    if ( waitpid(m_Pid, &x_exitcode, 0) == -1 ) {
        x_exitcode = -1;
    } else {
        x_exitcode = WEXITSTATUS(x_exitcode);
    }
    if ( m_ChildStdIn  != -1 ) {
        close(m_ChildStdIn);
    }
    if ( m_ChildStdOut != -1 ) {
        close(m_ChildStdOut);
    }
    if ( m_ChildStdErr != -1 ) {
        close(m_ChildStdErr);
    }
    m_Pid = m_ChildStdIn = m_ChildStdOut = m_ChildStdErr = -1;
    status = eIO_Success;

    if ( exitcode ) {
        *exitcode = x_exitcode;
    }
    return status;
}


EIO_Status CPipeHandle::CloseHandle(CPipe::EChildIOHandle handle)
{
    switch ( handle ) {
    case CPipe::eStdIn:
        if ( m_ChildStdIn == -1 ) {
            return eIO_Closed;
        }
        close(m_ChildStdIn);
        m_ChildStdIn = -1;
        return eIO_Success;
    case CPipe::eStdOut:
        if ( m_ChildStdOut == -1 ) {
            return eIO_Closed;
        }
        close(m_ChildStdOut);
        m_ChildStdOut = -1;
        return eIO_Success;
    case CPipe::eStdErr:
        if ( m_ChildStdErr == -1 ) {
            return eIO_Closed;
        }
        close(m_ChildStdErr);
        m_ChildStdErr = -1;
        return eIO_Success;
    default:
        // should never get here
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}



EIO_Status CPipeHandle::Read(void* buf, size_t count, size_t* n_read, 
                             const CPipe::EChildIOHandle from_handle,
                             const STimeout* timeout)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pid == -1) {
            status = eIO_Closed;
            throw "Pipe is closed";
        }
        int fd = GetHandle(from_handle);
        if (fd == -1) {
            throw "Pipe I/O handle is closed";
        }
        if ( !count ) {
            return eIO_Success;
        }

        // Retry if either blocked or interrupted (optional)
        for (;;) {
            // Try to read
            ssize_t bytes_read = read(fd, buf, count);

            if (bytes_read > 0) {
                if ( n_read ) {
                    *n_read = (size_t)bytes_read;
                }
                status = eIO_Success;
                break;
            }
            int x_errno = errno;

            // Blocked -- wait for data to come;  exit if timeout/error
            if (x_errno == EAGAIN) {
                status = Select(fd, eIO_Read, timeout);
                if (status != eIO_Success) {
                    break;
                }
                continue;
            }

            if (x_errno != EINTR) {
                throw "Failed to read data from the pipe";
            }
        }
    }
    catch (const char* what) {
        ERR_POST(s_FormatErrorMessage("Read", what));
    }
    return status;
}


EIO_Status CPipeHandle::Write(const void* buf, size_t count,
                              size_t* n_written, const STimeout* timeout)

{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pid == -1) {
            status = eIO_Closed;
            throw "Pipe is closed";
        }
        if (m_ChildStdIn == -1) {
            throw "Pipe I/O handle is closed";
        }
        if ( !count ) {
            return eIO_Success;
        }

        // Retry if either blocked or interrupted (optional)
        for (;;) {
            // Try to read
            ssize_t bytes_written = write(m_ChildStdIn, buf, count);
            
            if ( bytes_written > 0) {
                if ( n_written ) {
                    *n_written = (size_t)bytes_written;
                }
                status = eIO_Success;
                break;
            }
            int x_errno = errno;

            // Blocked -- wait for data to come;  exit if timeout/error
            if (x_errno == EAGAIN) {
                status = Select(m_ChildStdIn, eIO_Read, timeout);
                if (status != eIO_Success) {
                    break;
                }
                continue;
            }

            if (x_errno != EINTR) {
                throw "Failed to write data into the pipe";
            }
        }
    }
    catch (const char* what) {
        ERR_POST(s_FormatErrorMessage("Write", what));
    }
    return status;
}


int CPipeHandle::GetHandle(CPipe::EChildIOHandle from_handle) const
{
    switch (from_handle) {
    case CPipe::eStdIn:
        return m_ChildStdIn;
    case CPipe::eStdOut:
        return m_ChildStdOut;
    case CPipe::eStdErr:
        return m_ChildStdErr;
    }
    return -1;
}


bool CPipeHandle::SetNonBlockingMode(int fd, bool nonblock) const
{
    return fcntl(fd, F_SETFL,
                 nonblock ?
                 fcntl(fd, F_GETFL, 0) | O_NONBLOCK :
                 fcntl(fd, F_GETFL, 0) & (int) ~O_NONBLOCK) != -1;
}


EIO_Status CPipeHandle::Select(int fd, EIO_Event direction,
                               const STimeout* timeout) const
{
    // Wait for file descriptor to become ready (if timeout is set or infinite)
    if (!timeout  ||  timeout->sec  ||  timeout->usec) {
        return eIO_Timeout;
    }
    // Auto-resume if interrupted by a signal
    for (;;) {
        struct timeval  tm;
        struct timeval* tmp = 0;
        
        if ( !timeout ) {
            tm.tv_sec = timeout->sec;
            tm.tv_usec = timeout->usec;
            tmp = &tm;
        } 
        fd_set rfds;
        fd_set wfds;
        fd_set efds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);

        if (direction == eIO_Read) {
            FD_SET(fd, &wfds);
        } else if (direction == eIO_Write) {
            FD_SET(fd, &efds);
        }
        int n = select(fd + 1, &rfds, &wfds, &efds, tmp);
        if (n < 0 || FD_ISSET(fd, &efds)) {
            if (errno == EINTR) {
                continue;
            }
            throw "UNIX select() failed";
        }
        if (n == 0) {
            return eIO_Timeout;
        }
        assert(FD_ISSET(fd, &wfds));
        break;
    }
    return eIO_Success;
}


#endif  /* NCBI_OS_UNIX | NCBI_OS_MSWIN */



//////////////////////////////////////////////////////////////////////////////
//
// CPipe
//

CPipe::CPipe(void)
    : m_PipeHandle(0), m_ReadStatus(eIO_Closed),m_WriteStatus(eIO_Closed),
      m_ReadTimeout(0), m_WriteTimeout(0), m_CloseTimeout(0)
 
{
    // Create a new OS-specific pipe handle
    m_PipeHandle = new CPipeHandle();
    if ( !m_PipeHandle ) {
        NCBI_THROW(CPipeException, eInit,
                   "Cannot create OS-specific pipe handle");
    }
}


CPipe::CPipe(const string& cmd, const vector<string>& args,
             TCreateFlags create_flags)
    : m_PipeHandle(0), m_ReadStatus(eIO_Closed), m_WriteStatus(eIO_Closed),
      m_ReadTimeout(0), m_WriteTimeout(0), m_CloseTimeout(0)
{
    // Create a new OS-specific pipe handle
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
}


EIO_Status CPipe::Open(const string& cmd, const vector<string>& args,
                       TCreateFlags create_flags)
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    return m_PipeHandle->Open(cmd, args, create_flags);
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
                       const EChildIOHandle from_handle)
{
    if ( read ) {
        *read = 0;
    }
    if ( !buf  ||  from_handle == eStdIn) {
        return eIO_InvalidArg;
    }
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    EIO_Status status = m_PipeHandle->Read(buf, count, read, from_handle,
                                           m_ReadTimeout);
    m_ReadStatus = status;
    return status;
}


EIO_Status CPipe::Write(const void* buf, size_t count, size_t* written)
{
    if ( written ) {
        *written = 0;
    }
    if ( !buf ) {
        return eIO_InvalidArg;
    }
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    EIO_Status status = m_PipeHandle->Write(buf, count, written,
                                            m_WriteTimeout);
    m_WriteStatus = status;
    return status;
}


EIO_Status CPipe::Status(EIO_Event direction)
{
    switch ( direction ) {
    case eIO_Read:
        return m_ReadStatus;
    case eIO_Write:
        return m_WriteStatus;
    default:
        // Should never get here
        assert(0);
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
        m_CloseTimeout  = s_SetTimeout(timeout, &m_CloseTimeoutValue);
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



//////////////////////////////////////////////////////////////////////////////
//
// CPipeStreambuf
//

class CPipeStreambuf : public streambuf
{
public:
    CPipeStreambuf(CPipe& pipe, streamsize buf_size);
    virtual ~CPipeStreambuf();

    // Set handle of the child process for reading by default
    // (eStdOut / eStdErr)
    void SetReadHandle(CPipe::EChildIOHandle handle);

protected:
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual CT_INT_TYPE underflow(void);
    virtual int         sync(void);

    // This method is declared here to be disabled (exception) at run-time
    virtual streambuf*  setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

private:
    CPipe*                m_Pipe;       // underlying connection pipe
    CT_CHAR_TYPE*         m_Buf;        // of size 2 * m_BufSize
    CT_CHAR_TYPE*         m_WriteBuf;   // m_Buf
    CT_CHAR_TYPE*         m_ReadBuf;    // m_Buf + m_BufSize
    streamsize            m_BufSize;    // of m_WriteBuf, m_ReadBuf
    CPipe::EChildIOHandle m_ReadHandle; // handle for reading by default
};


CPipeStreambuf::CPipeStreambuf(CPipe& pipe, streamsize buf_size)
    : m_Pipe(&pipe),
      m_Buf(0),
      m_BufSize(buf_size ? buf_size : 1),
      m_ReadHandle(CPipe::eStdOut)
{
    auto_ptr<CT_CHAR_TYPE> bp(new CT_CHAR_TYPE[2 * m_BufSize]);

    m_WriteBuf = bp.get();
    setp(m_WriteBuf, m_WriteBuf + m_BufSize);

    m_ReadBuf = bp.get() + m_BufSize;
    setg(0, 0, 0); // we wish to have underflow() called at the first read
    m_Buf = bp.release();
}


CPipeStreambuf::~CPipeStreambuf()
{
    sync();
    delete[] m_Buf;
}


void CPipeStreambuf::SetReadHandle(CPipe::EChildIOHandle handle)
{
    m_ReadHandle = handle;
}


CT_INT_TYPE CPipeStreambuf::overflow(CT_INT_TYPE c)
{
    size_t n_written;

    if ( pbase() ) {
        // Send buffer
        size_t n_write = pptr() - pbase();
        if ( !n_write ) {
            return CT_NOT_EOF(CT_EOF);
        }
        m_Pipe->Write(m_WriteBuf, n_write * sizeof(CT_CHAR_TYPE), &n_written);
        _ASSERT(n_written);

        // Update buffer content (get rid of the sent data)
        if ((n_written /= sizeof(CT_CHAR_TYPE)) != n_write) {
            memmove(m_WriteBuf, pbase() + n_written,
                    (n_write - n_written) * sizeof(CT_CHAR_TYPE));
        }
        setp(m_WriteBuf + n_write - n_written, m_WriteBuf + m_BufSize);

        // Store char
        return CT_EQ_INT_TYPE(c, CT_EOF)
            ? CT_NOT_EOF(CT_EOF) : sputc(CT_TO_CHAR_TYPE(c));
    }

    if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        CT_CHAR_TYPE b = CT_TO_CHAR_TYPE(c);
        // Send char
        m_Pipe->Write(&b, sizeof(b), &n_written);
        _ASSERT(n_written == sizeof(b));
        return c;
    }
    return CT_NOT_EOF(CT_EOF);
}


CT_INT_TYPE CPipeStreambuf::underflow(void)
{
    _ASSERT(!gptr() || gptr() >= egptr());

    // Read from the pipe
    size_t n_read = 0;
    m_Pipe->Read(m_ReadBuf, m_BufSize * sizeof(CT_CHAR_TYPE),
                 &n_read, m_ReadHandle);        
    if (n_read == 0) {
        return CT_EOF;
    }
    // Update input buffer with the data we just read
    setg(m_ReadBuf, m_ReadBuf, m_ReadBuf + n_read / sizeof(CT_CHAR_TYPE));

    return CT_TO_INT_TYPE(*m_ReadBuf);
}


int CPipeStreambuf::sync(void)
{
    do {
        if (CT_EQ_INT_TYPE(overflow(CT_EOF), CT_EOF)) {
            return -1;
        }
    } while (pbase()  &&  pptr() > pbase());
    return 0;
}


streambuf* CPipeStreambuf::setbuf(CT_CHAR_TYPE* /*buf*/,
                                  streamsize    /*buf_size*/)
{
    NCBI_THROW(CPipeException,eSetBuf,"CPipeStreambuf::setbuf() not allowed");
    return this; /*NOTREACHED*/
}



//////////////////////////////////////////////////////////////////////////////
//
// CPipeIOStream
//


CPipeIOStream::CPipeIOStream(CPipe& pipe, streamsize buf_size)
    : iostream(0), m_StreamBuf(0)
{
    auto_ptr<CPipeStreambuf> sb(new CPipeStreambuf(pipe, buf_size));
    init(sb.get());
    m_StreamBuf = sb.release();
}


CPipeIOStream::~CPipeIOStream(void)
{
#if !defined(HAVE_IOS_XALLOC) || defined(HAVE_BUGGY_IOS_CALLBACKS)
    streambuf* sb = rdbuf();
    delete sb;
    if (sb != m_StreamBuf)
#endif
        delete m_StreamBuf;
#ifdef AUTOMATIC_STREAMBUF_DESTRUCTION
    rdbuf(0);
#endif
}


void CPipeIOStream::SetReadHandle(const CPipe::EChildIOHandle handle) const
{
    if (m_StreamBuf) {
        m_StreamBuf->SetReadHandle(handle);
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
