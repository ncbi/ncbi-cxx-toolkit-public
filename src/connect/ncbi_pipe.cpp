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
 * Authors:  Vladimir Ivanov
 *
 */

#include <corelib/ncbipipe.hpp>
#include <corelib/ncbiexec.hpp>
#include <memory>


#ifdef NCBI_OS_MSWIN
#  include <windows.h>
#elif defined NCBI_OS_UNIX
#  include <unistd.h>
#  include <errno.h>
#  include <sys/wait.h>
#  ifdef NCBI_COMPILER_MW_MSL
#    include <ncbi_mslextras.h>
#  endif
#else
#  error "Class CPipe is supported only on Windows and Unix"
#endif


BEGIN_NCBI_SCOPE


#if defined(NCBI_OS_MSWIN)

//
// Class CPipeHandle handles forwarded requests from CPipe
// This class is reimplemented in a platform-specific fashion where needed
//

//////////////////////////////////////////////////////////////////////////////
//
// CPipeHandle -- Win32 version
//

class CPipeHandle : public CObject
{
public:
    CPipeHandle();
    ~CPipeHandle();

    void Open(const char* cmd_line, const vector<string>& args,
              CPipe::EMode mode_stdin,
              CPipe::EMode mode_stdout,
              CPipe::EMode mode_stderr);

    int Close();
    void CloseHandle(CPipe::EChildIOHandle handle);

    size_t Read(void *buffer, size_t size,
                CPipe::EChildIOHandle from_handle) const;
    size_t Write(const void* buffer, size_t size) const;


private:
    // Handles for our child process
    HANDLE m_ChildStdin;
    HANDLE m_ChildStdout;
    HANDLE m_ChildStderr;

    // Process information for our child process
    HANDLE m_ProcHandle;
};


CPipeHandle::CPipeHandle()
    : m_ChildStdin(NULL),
      m_ChildStdout(NULL),
      m_ChildStderr(NULL)
{
    //ZeroMemory(&m_ProcInfo, sizeof(m_ProcInfo));
}


CPipeHandle::~CPipeHandle()
{
    Close();
}


//
// CPipeHandle::Open() -- Win32 version
//
void CPipeHandle::Open(const char* cmd_line, const vector<string>& args,
                       CPipe::EMode mode_stdin,
                       CPipe::EMode mode_stdout,
                       CPipe::EMode mode_stderr)
{
    // Set the base security attributes
    SECURITY_ATTRIBUTES attr;
    attr.nLength = sizeof(attr);
    attr.bInheritHandle = TRUE;
    attr.lpSecurityDescriptor = NULL;

    // Save the current stdout / stdin handles
    HANDLE stdin_handle  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
    FlushFileBuffers(stdout_handle);
    FlushFileBuffers(stderr_handle);

    // Create a pipe for the child's stdin
    HANDLE child_stdin_read;
    HANDLE child_stdin_write;
    if (mode_stdin != CPipe::eDoNotUse) {
        if ( !CreatePipe(&child_stdin_read, &child_stdin_write, &attr, 0) ) {
            NCBI_THROW(CPipeException, eBind,
                       "Cannot create pipe for child's stdout");
        }
        if ( !SetStdHandle(STD_INPUT_HANDLE, child_stdin_read) ) {
            NCBI_THROW(CPipeException, eBind,
                       "Cannot remap stdout for child process");
        }

        // Duplicate the handle
        if ( !DuplicateHandle(GetCurrentProcess(), child_stdin_write,
                              GetCurrentProcess(), &m_ChildStdin,
                              0, FALSE, DUPLICATE_SAME_ACCESS) ) {
            NCBI_THROW(CPipeException, eBind,
                       "Cannot duplicate child's stdout handle");
        }
        ::CloseHandle(child_stdin_write);
    }

    // Create a pipe for the child's stdout
    HANDLE child_stdout_read;
    HANDLE child_stdout_write;
    if (mode_stdout != CPipe::eDoNotUse) {
        if ( !CreatePipe(&child_stdout_read, &child_stdout_write, &attr, 0)) {
            NCBI_THROW(CPipeException, eBind,
                       "Cannot create pipe for child's stdout");
        }
        if ( !SetStdHandle(STD_OUTPUT_HANDLE, child_stdout_write) ) {
            NCBI_THROW(CPipeException, eBind,
                       "Cannot remap stdout for child process");
        }

        // Duplicate the handle
        if ( !DuplicateHandle(GetCurrentProcess(), child_stdout_read,
                              GetCurrentProcess(), &m_ChildStdout,
                              0, FALSE, DUPLICATE_SAME_ACCESS) ) {
            NCBI_THROW(CPipeException, eBind,
                       "Cannot duplicate child's stdout handle");
        }
        ::CloseHandle(child_stdout_read);
    }

    // Create a pipe for the child's stderr
    HANDLE child_stderr_read;
    HANDLE child_stderr_write;
    if (mode_stderr != CPipe::eDoNotUse) {
        if ( !CreatePipe(&child_stderr_read, &child_stderr_write, &attr, 0)) {
            NCBI_THROW(CPipeException, eBind,
                       "Cannot create pipe for child's stdout");
        }
        if ( !SetStdHandle(STD_ERROR_HANDLE, child_stderr_write) ) {
            NCBI_THROW(CPipeException, eBind,
                       "Cannot remap stdout for child process");
        }

        // Duplicate the handle
        if ( !DuplicateHandle(GetCurrentProcess(), child_stderr_read,
                              GetCurrentProcess(), &m_ChildStderr,
                              0, FALSE, DUPLICATE_SAME_ACCESS) ) {
            NCBI_THROW(CPipeException, eBind,
                       "Cannot duplicate child's stdout handle");
        }
        ::CloseHandle(child_stderr_read);
    }

    // Create a process to handle our command line
    PROCESS_INFORMATION pinfo;
    STARTUPINFO sinfo;
    ZeroMemory(&pinfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&sinfo, sizeof(STARTUPINFO));
    sinfo.cb = sizeof(sinfo);

    string cmds(cmd_line);
    ITERATE (vector<string>, iter, args) {
        if ( !cmds.empty() ) {
            cmds += " ";
        }
        cmds += *iter;
    }

    if ( !CreateProcess(NULL,
                        const_cast<char*> (cmds.c_str()),
                        NULL, NULL, TRUE, 0,
                        NULL, NULL, &sinfo, &pinfo) ) {
        string msg = "Cannot create child process for \"";
        msg += cmds;
        msg += "\": Error = ";
        msg += NStr::IntToString(GetLastError());
        NCBI_THROW(CPipeException, eRun, msg);
    }

    m_ProcHandle = pinfo.hProcess;

    // Restore stdout/stdin
    if ( !SetStdHandle(STD_INPUT_HANDLE,  stdin_handle) ) {
        NCBI_THROW(CPipeException, eUnbind,
                   "Cannot remap stdin for parent process");
    }
    if ( !SetStdHandle(STD_OUTPUT_HANDLE, stdout_handle) ) {
        NCBI_THROW(CPipeException, eUnbind,
                   "Cannot remap stdout for parent process");
    }
    if ( !SetStdHandle(STD_ERROR_HANDLE,  stderr_handle) ) {
        NCBI_THROW(CPipeException, eUnbind,
                   "Cannot remap stdin for parent process");
    }
    if (mode_stdout != CPipe::eDoNotUse) {
        ::CloseHandle(child_stdout_write);
    }
    if (mode_stderr != CPipe::eDoNotUse) {
        ::CloseHandle(child_stderr_write);
    }
}


//
// CPipeHandle::Close() -- Win32 version
//
int CPipeHandle::Close()
{
    // Wait for the child process to exit
    DWORD exit_code;
    if (!GetExitCodeProcess(m_ProcHandle, &exit_code) ||
        exit_code == STILL_ACTIVE) {
        return -1;
    }
    m_ProcHandle = NULL;
    if ( m_ChildStdin ) {
        ::CloseHandle(m_ChildStdin);
        m_ChildStdin = NULL;
    }
    if ( m_ChildStdout ) {
        ::CloseHandle(m_ChildStdout);
        m_ChildStdout = NULL;
    }
    if ( m_ChildStderr ) {
        ::CloseHandle(m_ChildStderr);
        m_ChildStderr = NULL;
    }
    return exit_code;
}


//
// CPipeHandle::CloseHandle() -- Win32 version
//
void CPipeHandle::CloseHandle(CPipe::EChildIOHandle handle)
{
    switch(handle) {
    case CPipe::eStdIn:
        if ( m_ChildStdin ) {
            ::CloseHandle(m_ChildStdin);
            m_ChildStdin = NULL;
        }
        break;
    case CPipe::eStdOut:
        if ( m_ChildStdout ) {
            ::CloseHandle(m_ChildStdout);
            m_ChildStdout = NULL;
        }
        break;
    case CPipe::eStdErr:
        if ( m_ChildStderr ) {
            ::CloseHandle(m_ChildStderr);
            m_ChildStderr = NULL;
        }
        break;
    }
}


//
// CPipeHandle::Read() -- Win32 version
//
size_t CPipeHandle::Read(void* buf, size_t size,
                         CPipe::EChildIOHandle from_handle) const
{
    DWORD bytes_read = 0;

    switch (from_handle) {
    case CPipe::eStdOut:
        if ( !ReadFile(m_ChildStdout, buf, size, &bytes_read, NULL) ) {
            return 0;
        }
        break;

    case CPipe::eStdErr:
        if ( !ReadFile(m_ChildStderr, buf, size, &bytes_read, NULL) ) {
            return 0;
        }
        break;
    }

    return bytes_read;
}


//
// CPipeHandle::Write() -- Win32 version
//
size_t CPipeHandle::Write(const void* buf, size_t size) const
{
    DWORD bytes_written = 0;
    if ( !WriteFile(m_ChildStdin, buf, size, &bytes_written, NULL) ) {
        return 0;
    }

    return bytes_written;
}


#elif defined(NCBI_OS_UNIX)



//////////////////////////////////////////////////////////////////////////////
//
// CPipeHandle -- Unix version
//

class CPipeHandle : public CObject
{
public:
    CPipeHandle();
    ~CPipeHandle();

    void Open(const char* cmd_line, const vector<string>& args,
              CPipe::EMode mode_stdin,
              CPipe::EMode mode_stdout,
              CPipe::EMode mode_stderr);

    int Close();
    void CloseHandle(CPipe::EChildIOHandle handle);

    size_t Read(void *buffer, size_t size,
                CPipe::EChildIOHandle from_handle) const;
    size_t Write(const void* buffer, size_t size) const;

private:
    // I/O handles of the child process for:
    int m_StdIn;    // write to the shild stdin
    int m_StdOut;   // read from the child stdout
    int m_StdErr;   // read from the child stderr

    // Pid of running child process
    pid_t m_Pid;
};


CPipeHandle::CPipeHandle()
    : m_StdIn(-1),
      m_StdOut(-1),
      m_StdErr(-1),
      m_Pid(-1)
{
}


CPipeHandle::~CPipeHandle()
{
}


void CPipeHandle::Open(const char* cmdname, const vector<string>& args,
                       CPipe::EMode mode_stdin,
                       CPipe::EMode mode_stdout,
                       CPipe::EMode mode_stderr)
{
    // Create a pipes
    int fd_pipe_in[2], fd_pipe_out[2], fd_pipe_err[2];

    if ( mode_stdin != CPipe::eDoNotUse ) {
        if ( pipe(fd_pipe_in) == -1 ) {
            NCBI_THROW(CPipeException, eBind,
                       "Pipe binding to standard I/O file handle failed");
        }
        m_StdIn = fd_pipe_in[1];
    }
    if ( mode_stdout != CPipe::eDoNotUse ) {
        if ( pipe(fd_pipe_out) == -1 ) {
            NCBI_THROW(CPipeException, eBind,
                       "Pipe binding to standard I/O file handle failed");
        }
        fflush(stdout);
        m_StdOut = fd_pipe_out[0];
    }
    if ( mode_stderr != CPipe::eDoNotUse ) {
        if ( pipe(fd_pipe_err) == -1 ) {
            NCBI_THROW(CPipeException, eBind,
                       "Pipe binding to standard I/O file handle failed");
        }
        fflush(stderr);
        m_StdErr = fd_pipe_err[0];
    }

    // Fork child process
    switch (m_Pid = fork()) {
    case -1:
        // fork failed
        NCBI_THROW(CPipeException, eRun,
                   "Pipe has failed to fork the current process");
    case 0:
        // Now we are in the child process
        int status = -1;

        // Bind childs standard I/O file handlers to pipe
        if ( mode_stdin != CPipe::eDoNotUse ) {
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
        if ( mode_stdout != CPipe::eDoNotUse ) {
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
        if ( mode_stderr != CPipe::eDoNotUse ) {
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
        x_args[0] = cmdname;
        x_args[cnt + 1] = 0;

        // Spawn a program
        status = execvp(cmdname, const_cast<char**> (x_args));
        _exit(status);
    }

    // Close unused pipe handles
    if ( mode_stdin != CPipe::eDoNotUse ) {
        close(fd_pipe_in[0]);
    }
    if ( mode_stdout != CPipe::eDoNotUse ) {
        close(fd_pipe_out[1]);
    }
    if ( mode_stderr != CPipe::eDoNotUse ) {
        close(fd_pipe_err[1]);
    }
}


int CPipeHandle::Close()
{
    int exit_code = -1;
    if ( m_Pid != -1 ) {
        if ( waitpid(m_Pid, &exit_code, 0) == -1 ) {
            exit_code = -1;
        } else {
            exit_code = WEXITSTATUS(exit_code);
        }
    }
    if ( m_StdIn  != -1 ) {
        close(m_StdIn);
    }
    if ( m_StdOut != -1 ) {
        close(m_StdOut);
    }
    if ( m_StdErr != -1 ) {
        close(m_StdErr);
    }
    m_Pid = m_StdIn = m_StdOut = m_StdErr = -1;

    return exit_code;
}


void CPipeHandle::CloseHandle(CPipe::EChildIOHandle handle)
{
    switch ( handle ) {
    case CPipe::eStdIn:
        if ( m_StdIn != -1 ) {
            close(m_StdIn);
        }
        m_StdIn = -1;
        break;
    case CPipe::eStdOut:
        if ( m_StdOut != -1 ) {
            close(m_StdOut);
        }
        m_StdOut = -1;
        break;
    case CPipe::eStdErr:
        if ( m_StdErr != -1 ) {
            close(m_StdErr);
        }
        m_StdErr = -1;
        break;
    }
}


size_t CPipeHandle::Read(void* buffer, size_t size,
                         CPipe::EChildIOHandle from_handle) const
{
    int fd = (from_handle == CPipe::eStdOut) ? m_StdOut : m_StdErr;
    if ( m_Pid == -1  ||  fd == -1 ) {
        NCBI_THROW(CPipeException,eNoInit,"Pipe is not initialized properly");
    } 
    return read(fd, buffer, size);
}


size_t CPipeHandle::Write(const void* buffer, size_t size) const
{
    if ( m_Pid == -1  ||  m_StdIn == -1 ) {
        NCBI_THROW(CPipeException,eNoInit,"Pipe is not initialized properly");
    }
    return write(m_StdIn, buffer, size);
}

#endif  /* NCBI_OS_UNIX | NCBI_OS_MSWIN */



//////////////////////////////////////////////////////////////////////////////
//
// CPipe
//

CPipe::CPipe() 
{
}


CPipe::CPipe(const char* cmdname, const vector<string>& args, 
             const EMode mode_stdin, const EMode mode_stdout, 
             const EMode mode_stderr)
{
    Open(cmdname, args, mode_stdin, mode_stdout, mode_stderr);
}


CPipe::~CPipe()
{
    Close();
}


void CPipe::Open(const char* cmdname, const vector<string>& args,
                 const EMode mode_stdin, const EMode mode_stdout, 
                 const EMode mode_stderr)
{
    // Close pipe if it already was opened
    Close();

    // Create a new handle to forward our request
    m_PipeHandle.Reset(new CPipeHandle());
    m_PipeHandle->Open(cmdname, args, mode_stdin, mode_stdout, mode_stderr);
}


int CPipe::Close()
{
    int exit_code = -1;
    if (m_PipeHandle) {
        exit_code = m_PipeHandle->Close();
    }
    m_PipeHandle.Reset();
    return exit_code;
}


void CPipe::CloseHandle(EChildIOHandle handle)
{
    if (m_PipeHandle) {
        m_PipeHandle->CloseHandle(handle);
    }
}


size_t CPipe::Read(void* buffer, const size_t count,
                   const EChildIOHandle from_handle) const
{
    if ( !m_PipeHandle ) {
        return 0;
    }

    return m_PipeHandle->Read(buffer, count, from_handle);
}


size_t CPipe::Write(const void* buffer, const size_t count) const
{
    if ( !m_PipeHandle ) {
        return 0;
    }

    return m_PipeHandle->Write(buffer, count);
}


//////////////////////////////////////////////////////////////////////////////
//
// CPipeStrembuf
//

class CPipeStreambuf : public streambuf
{
public:
    CPipeStreambuf(const CPipe& pipe, streamsize buf_size);
    virtual ~CPipeStreambuf();

    // Set handle of the child process for reading by default
    // (eStdOut / eStdErr)
    void SetReadHandle(CPipe::EChildIOHandle handle);

protected:
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual CT_INT_TYPE underflow(void);
    virtual int         sync(void);

    // this method is declared here to be disabled (exception) at run-time
    virtual streambuf*  setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

private:
    const CPipe*          m_Pipe;       // underlying connection pipe
    CT_CHAR_TYPE*         m_Buf;        // of size 2 * m_BufSize
    CT_CHAR_TYPE*         m_WriteBuf;   // m_Buf
    CT_CHAR_TYPE*         m_ReadBuf;    // m_Buf + m_BufSize
    streamsize            m_BufSize;    // of m_WriteBuf, m_ReadBuf
    CPipe::EChildIOHandle m_ReadHandle; // handle for reading by default
};


CPipeStreambuf::CPipeStreambuf(const CPipe& pipe, streamsize buf_size)
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
        n_written = m_Pipe->Write(m_WriteBuf, n_write * sizeof(CT_CHAR_TYPE));
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
        n_written = m_Pipe->Write(&b, sizeof(b));
        _ASSERT(n_written == sizeof(b));
        return c;
    }
    return CT_NOT_EOF(CT_EOF);
}


CT_INT_TYPE CPipeStreambuf::underflow(void)
{
    _ASSERT(!gptr() || gptr() >= egptr());

    // Read from the pipe
    size_t n_read = m_Pipe->Read(m_ReadBuf, m_BufSize * sizeof(CT_CHAR_TYPE),
                                 m_ReadHandle);
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


CPipeIOStream::CPipeIOStream(const CPipe& pipe, streamsize buf_size)
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
