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

#if defined(NCBI_OS_MSWIN)
#  include <fcntl.h>
#  if !defined(popen)
#    define popen _popen
#  endif
#  if !defined(pclose)
#    define pclose _pclose
#  endif
#  if !defined(dup)
#    define dup _dup
#  endif
#  if !defined(dup2)
#    define dup2 _dup2
#  endif
#elif defined(NCBI_OS_UNIX)
#  include <errno.h>
#elif defined(NCBI_OS_MAC)
#  error "Class CPipe defined only for MS Windows and UNIX platforms"
#endif


BEGIN_NCBI_SCOPE


// Throw exception with diagnostic message
static void s_ThrowException(const string& what)
{
    const char* errmsg = strerror(errno);
    if ( !errmsg ) {
        errmsg = "unknown reason";
    }
    // Throw exception
    throw CPipe::CException("CPipe: " + what + ": " + errmsg);
}


//////////////////////////////////////////////////////////////////////////////
//
// CPipe
//


// Create pipe and bind original handle
static int s_BindHandle(const int fd_orig, const bool is_input, 
                        const bool is_binary, int* fd_save)
{
    // Index in "fd_pipe[]"
    int i_orig = is_input ? 0 : 1;
    int i_peer = is_input ? 1 : 0;

    // Create a pipe
    int fd_pipe[2];

#  if defined(NCBI_OS_MSWIN)
    int mode = is_binary ? O_BINARY : O_TEXT;
    int result = _pipe(fd_pipe, 512, mode | O_NOINHERIT);
#  elif defined(NCBI_OS_UNIX)
    int result = pipe(fd_pipe);
#  elif defined(NCBI_OS_MAC)
    ?
#  endif   

    if (result == -1)  {
        *fd_save = -1;
        return -1;
    }

    // Duplicate one side of the pipe to the original handle;
    // store the duplicated copy of original handle in "fd_save"
    *fd_save = dup(fd_orig);

#  if defined(NCBI_OS_MSWIN)
    if (*fd_save < 0  ||  dup2(fd_pipe[i_orig], fd_orig) != 0) {
#  elif defined(NCBI_OS_UNIX)
    if (*fd_save < 0  ||  dup2(fd_pipe[i_orig], fd_orig) < 0) {
#  endif
        if (*fd_save != -1)
            close(*fd_save);
        else
            *fd_save = -1;
        close(fd_pipe[i_peer]);
        fd_pipe[i_peer] = -1;
    }
    close(fd_pipe[i_orig]);

    //  The peer for the original file handle
    return fd_pipe[i_peer];
}


// Restore the original I/O handle
static bool s_RestoreHandle(const int fd_orig, const int fd_save)
{
  if (fd_save < 0)
    return true;

#  if defined(NCBI_OS_MSWIN)
  if (dup2(fd_save, fd_orig) != 0)
#  elif defined(NCBI_OS_UNIX)
  if (dup2(fd_save, fd_orig) < 0)
#  endif
    return false;

  close(fd_save);
  return true;
}


CPipe::CPipe() 
{
    Init();
}


CPipe::CPipe(const char *cmdname, const char *const *args, 
             const EMode mode_stdin, const EMode mode_stdout, 
             const EMode mode_stderr)
{
    Init();
    Open(cmdname, args, mode_stdin, mode_stdout, mode_stderr);
}


CPipe::~CPipe()
{
    Close();
}


void CPipe::Init()
{
    m_StdIn = m_StdOut = m_StdErr = -1;
    m_Pid = -1;
}


void CPipe::Open(const char *cmdname, const char *const *args,
                 const EMode mode_stdin, const EMode mode_stdout, 
                 const EMode mode_stderr)
{
    // Close pipe if it already was opened
    Close();

    // Save original handles here
    int fd_stdin_save = -1, fd_stdout_save = -1, fd_stderr_save = -1;

    // Bind standard I/O file handlers to pipes
    if ( mode_stdin != eDoNotUse ) {
        m_StdIn  = s_BindHandle(fileno(stdin), true,
                                mode_stdin == eBinary, &fd_stdin_save);
        if ( m_StdIn == -1 )
            s_ThrowException("Bind stdin failed");
    }
    if ( mode_stdout != eDoNotUse ) {
        m_StdOut = s_BindHandle(fileno(stdout), false,
                                mode_stdout == eBinary, &fd_stdout_save);
        if ( m_StdOut == -1 )
           s_ThrowException("Bind stdout failed");
    }
    if ( mode_stderr != eDoNotUse ) {
        m_StdErr = s_BindHandle(fileno(stderr), false,
                                mode_stderr == eBinary, &fd_stderr_save);
        if ( m_StdErr == -1 )
            s_ThrowException("Bind stderr failed");
    }

    // Spawn the child process
    if ( args ) {
        m_Pid = CExec::SpawnVP(CExec::eNoWait, cmdname, args);
    } else {
        char* x_args[2];
        x_args[1] = 0;
        m_Pid = CExec::SpawnVP(CExec::eNoWait, cmdname, x_args);
    }
    if ( m_Pid == -1 ) {
        s_ThrowException("Error execution a child process");
    }

    // Restore the standard IO file handlers to their original state
    if ( mode_stdin != eDoNotUse ) {
        if ( !s_RestoreHandle(fileno(stdin ), fd_stdin_save) )
            s_ThrowException("Restore stdin failed");
    }
    if ( mode_stdout != eDoNotUse ) {
        if ( !s_RestoreHandle(fileno(stdout), fd_stdout_save) )
            s_ThrowException("Restore stdout failed");
    }
    if ( mode_stderr != eDoNotUse ) {
        if ( !s_RestoreHandle(fileno(stderr), fd_stderr_save) )
            s_ThrowException("Restore stderr failed");
    }
}


int CPipe::Close()
{
    m_StdIn = m_StdOut = m_StdErr = -1;
    if ( m_Pid == -1 )
        return -1;
    int status = CExec::Wait(m_Pid);
    m_Pid = -1;
    return status;
}


size_t CPipe::Read(void *buffer, const size_t count, const EChildIOHandle from_handle)
{
    int fd = (from_handle == eStdOut) ? m_StdOut : m_StdErr;
    if ( m_Pid == -1  ||  fd == -1 ) {
        s_ThrowException("Reading from unopened pipe");
    } 
    return read(fd, buffer, count);
}


size_t CPipe::Write(const void *buffer, const size_t count)
{
    if ( m_Pid == -1  ||  m_StdIn == -1 ) {
        s_ThrowException("Writing to unopened pipe");
    }
    return write(m_StdIn, buffer, count);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/06/10 16:59:39  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
