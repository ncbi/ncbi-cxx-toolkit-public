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
 * File Description:
 *   Portable interprocess named pipe API for:  UNIX, MS-Win
 */

#include <connect/ncbi_namedpipe.hpp>
#include <corelib/ncbi_system.hpp>

#if defined(NCBI_OS_MSWIN)

#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <errno.h>
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/time.h>
#  include <sys/un.h>
#  include <connect/ncbi_socket.h>
#else
#  error "Class CNamedPipe is supported only on Windows and Unix"
#endif


BEGIN_NCBI_SCOPE


#if !defined(HAVE_SOCKLEN_T)
typedef int socklen_t;
#endif


// Predefined timeouts
const size_t CNamedPipe::kDefaultBufferSize = 0;


//////////////////////////////////////////////////////////////////////////////
//
// Auxiliary functions
//

static STimeout* s_SetTimeout(const STimeout* from, STimeout* to)
{
    if ( !from ) {
        return const_cast<STimeout*> (kInfiniteTimeout);
    }
    to->sec  = from->usec / 1000000 + from->sec;
    to->usec = from->usec % 1000000;
    return to;
}

static string s_FormatErrorMessage(const string& where, const string& what)
{
    return "[NamedPipe::" + where + "]  " + what + ".";
}


//////////////////////////////////////////////////////////////////////////////
//
// Class CNamedPipeHandle handles forwarded requests from CNamedPipe.
// This class is reimplemented in a platform-specific fashion where needed.
//

#if defined(NCBI_OS_MSWIN)

const DWORD kSleepTime = 100;  // sleep time for timeouts


//////////////////////////////////////////////////////////////////////////////
//
// CNamedPipeHandle -- MS Windows version
//

class CNamedPipeHandle
{
public:
    CNamedPipeHandle(void);
    ~CNamedPipeHandle(void);

    // client-side

    EIO_Status Open(const string& pipename, const STimeout* timeout,
                    size_t bufsize);

    // server-side

    EIO_Status Create(const string& pipename, size_t bufsize);
    EIO_Status Listen(const STimeout* timeout);
    EIO_Status Disconnect(void);

    // common

    EIO_Status Close(void);
    EIO_Status Read(void* buf, size_t count, size_t* n_read,
                    const STimeout* timeout);
    EIO_Status Write(const void* buf, size_t count, size_t* n_written,
                     const STimeout* timeout);
    EIO_Status Status(EIO_Event direction);

private:
    long TimeoutToMSec(const STimeout* timeout) const;

private:
    HANDLE      m_Pipe;        // pipe I/O handle
    string      m_PipeName;    // pipe name 
    size_t      m_Bufsize;     // I/O buffer size
    EIO_Status  m_ReadStatus;  // last read status
    EIO_Status  m_WriteStatus; // last write status
};


CNamedPipeHandle::CNamedPipeHandle()
    : m_Pipe(INVALID_HANDLE_VALUE), m_PipeName(kEmptyStr),
      m_Bufsize(CNamedPipe::kDefaultBufferSize),
      m_ReadStatus(eIO_Closed), m_WriteStatus(eIO_Closed)
{
    return;
}


CNamedPipeHandle::~CNamedPipeHandle()
{
    Close();
}


EIO_Status CNamedPipeHandle::Open(const string&   pipename,
                                  const STimeout* timeout,
                                  size_t          bufsize)
{
    try {
        if (m_Pipe != INVALID_HANDLE_VALUE) {
            throw "Pipe is already open";
        }
        // Save parameters
        m_PipeName = pipename;
        m_Bufsize  = 0;

        // Set the base security attributes
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.bInheritHandle = TRUE;
        attr.lpSecurityDescriptor = NULL;

        // Waits until either a time-out interval elapses or an instance of
        // the specified named pipe is available for connection (that is, the
        // pipe's server process has a pending Listen() operation on the pipe).

        // NOTE: We do not use here a WaitNamedPipe() because it works
        //       incorrect in some cases.

        DWORD x_timeout = TimeoutToMSec(timeout);
        do {
            // Open existing pipe
            m_Pipe = CreateFile(pipename.c_str(),
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                &attr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                NULL);
            if (m_Pipe != INVALID_HANDLE_VALUE) {
                m_ReadStatus  = eIO_Success;
                m_WriteStatus = eIO_Success;
                return eIO_Success;
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

        return eIO_Timeout;
    }
    catch (const char* what) {
        Close();
        ERR_POST(s_FormatErrorMessage("Open", what));
        return eIO_Unknown;
    }
}


EIO_Status CNamedPipeHandle::Create(const string& pipename,
                                    size_t        bufsize)
{
    try {
        if (m_Pipe != INVALID_HANDLE_VALUE) {
            throw "Pipe is already open";
        }
        // Save parameters
        m_PipeName = pipename;
        m_Bufsize  = bufsize;

        // Set the base security attributes
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.bInheritHandle = TRUE;
        attr.lpSecurityDescriptor = NULL;

        // Create pipe
        m_Pipe = CreateNamedPipe(
                 pipename.c_str(),              // pipe name 
                 PIPE_ACCESS_DUPLEX,            // read/write access 
                 PIPE_TYPE_BYTE | PIPE_NOWAIT,  // byte-type, nonblocking mode 
                 1,                             // one instance only 
                 bufsize,                       // output buffer size 
                 bufsize,                       // input buffer size 
                 INFINITE,                      // client time-out by default
                 &attr);                        // security attributes

        if (m_Pipe == INVALID_HANDLE_VALUE) {
            throw "Create named pipe \"" + pipename + "\" failed";
        }
        m_ReadStatus  = eIO_Success;
        m_WriteStatus = eIO_Success;
        return eIO_Success;
    }
    catch (const char* what) {
        Close();
        ERR_POST(s_FormatErrorMessage("Create", what));
        return eIO_Unknown;
    }
}


EIO_Status CNamedPipeHandle::Listen(const STimeout* timeout)
{
    EIO_Status status = eIO_Unknown;

    // Wait for the client to connect, or time out.
    // NOTE: The function WaitForSingleObject() do not work with pipes.
    DWORD x_timeout = TimeoutToMSec(timeout);

    try {
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw "Pipe is closed";
        }
        do {
            BOOL connected = ConnectNamedPipe(m_Pipe, NULL);
            if ( !connected ) {
                DWORD error = GetLastError();
                connected = (error == ERROR_PIPE_CONNECTED); 
                // If client closed connection before we serve it
                if (error == ERROR_NO_DATA) {
                    if (Disconnect() != eIO_Success) {
                        throw "Listening pipe: failed to close broken "
                            "client session";
                    } 
                }
            }
            if ( connected ) {
                return eIO_Success;
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

        return eIO_Timeout;
    }
    catch (const char* what) {
        ERR_POST(s_FormatErrorMessage("Listen", what));
        return status;
    }
}


EIO_Status CNamedPipeHandle::Disconnect(void)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw "Pipe is already closed";
        }
        FlushFileBuffers(m_Pipe); 
        if (!DisconnectNamedPipe(m_Pipe)) {
            throw "DisconnectNamedPipe() failed";
        } 
        Close();
        return Create(m_PipeName, m_Bufsize);
    }
    catch (const char* what) {
        ERR_POST(s_FormatErrorMessage("Disconnect", what));
        return status;
    }
}


EIO_Status CNamedPipeHandle::Close(void)
{
    if (m_Pipe == INVALID_HANDLE_VALUE) {
        return eIO_Closed;
    }
    FlushFileBuffers(m_Pipe);
	CloseHandle(m_Pipe);
    m_Pipe = INVALID_HANDLE_VALUE;
    m_ReadStatus  = eIO_Closed;
    m_WriteStatus = eIO_Closed;
    return eIO_Success;
}


EIO_Status CNamedPipeHandle::Read(void* buf, size_t count, size_t* n_read,
                                  const STimeout* timeout)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw "Pipe is closed";
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout   = TimeoutToMSec(timeout);
        DWORD bytes_avail = 0;

        // Wait a data from the pipe with timeout.
        // NOTE:  The function WaitForSingleObject() do not work with pipe.

        do {
            if ( !PeekNamedPipe(m_Pipe, NULL, 0, NULL, &bytes_avail, NULL) ) {
                // Peer has been closed the connection?
                if (GetLastError() == ERROR_BROKEN_PIPE) {
                    return eIO_Closed;
                }
                throw "Cannot peek data from the named pipe";
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
        if (!ReadFile(m_Pipe, buf, count, &bytes_avail, NULL)) {
            throw "Failed to read data from the named pipe";
        }
        if ( n_read ) {
            *n_read = bytes_avail;
        }
        status = eIO_Success;
    }
    catch (const char* what) {
        ERR_POST(s_FormatErrorMessage("Read", what));
    }
    m_ReadStatus = status;
    return status;
}


EIO_Status CNamedPipeHandle::Write(const void* buf, size_t count,
                                   size_t* n_written, const STimeout* timeout)

{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw "Pipe is closed";
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout     = TimeoutToMSec(timeout);
        DWORD bytes_written = 0;

        // Wait a data from the pipe with timeout.
        // NOTE:  The function WaitForSingleObject() do not work with pipe.

        do {
            if (!WriteFile(m_Pipe, (char*)buf, count, &bytes_written, NULL)) {
                if ( n_written ) {
                    *n_written = bytes_written;
                }
                throw "Failed to write data into the named pipe";
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
    m_WriteStatus = status;
    return status;
}


EIO_Status CNamedPipeHandle::Status(EIO_Event direction)
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


// Convert STimeout value to number of milliseconds
long CNamedPipeHandle::TimeoutToMSec(const STimeout* timeout) const
{
    return timeout ? (timeout->sec * 1000) + (timeout->usec / 1000) 
                   : INFINITE;
}



#elif defined(NCBI_OS_UNIX)



//////////////////////////////////////////////////////////////////////////////
//
// CNamedPipeHandle -- Unix version
//


// The maximum length the queue of pending connections may grow to
const int kListenQueueSize = 32;


class CNamedPipeHandle
{
public:
    CNamedPipeHandle(void);
    ~CNamedPipeHandle(void);

    // client-side

    EIO_Status Open(const string& pipename, const STimeout* timeout,
                    size_t bufsize);

    // server-side

    EIO_Status Create(const string& pipename, size_t bufsize);
    EIO_Status Listen(const STimeout* timeout);
    EIO_Status Disconnect(void);

    // common

    EIO_Status Close(void);
    EIO_Status Read (void* buf, size_t count, size_t* n_read,
                     const STimeout* timeout);
    EIO_Status Write(const void* buf, size_t count, size_t* n_written,
                     const STimeout* timeout);
    EIO_Status Status(EIO_Event direction);

private:
    // Close socket persistently
    bool CloseSocket(int sock);
    // Set socket i/o buffer size (dir: SO_SNDBUF, SO_RCVBUF)
    bool SetSocketBufferSize(int sock, size_t bufsize, int dir);

private:
    int     m_LSocket;    // Listening socket
    SOCK    m_IoSocket;   // I/O socket
    size_t  m_Bufsize;    // IO buffer size
};


CNamedPipeHandle::CNamedPipeHandle()
    : m_LSocket(-1), m_IoSocket(0), m_Bufsize(CNamedPipe::kDefaultBufferSize)
{
    return;
}


CNamedPipeHandle::~CNamedPipeHandle()
{
    Close();
}


EIO_Status CNamedPipeHandle::Open(const string&   pipename,
                                  const STimeout* timeout,
                                  size_t          bufsize)
{
    struct sockaddr_un addr;
    EIO_Status status = eIO_Unknown;

    int sock = -1;
    try {
        if (m_LSocket >= 0  ||  m_IoSocket) {
            throw "Pipe is already open";
        }
        if (sizeof(addr.sun_path) <= pipename.length()) {
            status = eIO_InvalidArg;
            throw "Pipe name too long";
        }
        m_Bufsize = bufsize;

        // Create a UNIX socket
        if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
            throw "UNIX socket() failed";
        }

        // Set buffer size
        if ( m_Bufsize != CNamedPipe::kDefaultBufferSize) {
            if ( !SetSocketBufferSize(sock, m_Bufsize, SO_SNDBUF)  ||
                 !SetSocketBufferSize(sock, m_Bufsize, SO_RCVBUF) ) {
                throw "UNIX socket set buffer size failed";
            }
        }

        // Set non-blocking mode
        if (fcntl(sock, F_SETFL,
                  fcntl(sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
            throw "UNIX socket set to non-blocking failed";
        }
        
        // Connect to server
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
#ifdef HAVE_SIN_LEN
    	addr.sun_len = (socklen_t) sizeof(addr);
#endif
        strcpy(addr.sun_path, pipename.c_str());
        
        int x_errno = 0;
        int n;
        // Auto-resume if interrupted by a signal
        for (n = 0; ; n = 1) {
            if (connect(sock, (struct sockaddr*) &addr, sizeof(addr)) == 0) {
                break;
            }
            x_errno = errno;
            if (x_errno != EINTR) { 
                break;
            }
        }
        // If not connected
        if ( x_errno ) {
            if ((n != 0 || x_errno != EINPROGRESS)  &&
                (n == 0 || x_errno != EALREADY)     &&
                x_errno != EWOULDBLOCK) {
                if (x_errno == EINTR) {
                    status = eIO_Interrupt;
                }
                throw "UNIX socket connect() failed";
            }
            
            // Wait for socket to become ready (if timeout is set or infinite)
            if (!timeout  ||  timeout->sec  ||  timeout->usec) {
                // Auto-resume if interrupted by a signal
                for (;;) {
                    struct timeval  tm;
                    struct timeval* tmp = 0;

                    if ( !timeout ) {
                        tm.tv_sec = timeout->sec;
                        tm.tv_usec = timeout->usec;
                        tmp = &tm;
                    } 
                    fd_set wfds;
                    fd_set efds;
                    FD_ZERO(&wfds);
                    FD_ZERO(&efds);
                    FD_SET(sock, &wfds);
                    FD_SET(sock, &efds);
                    n = select(sock + 1, 0, &wfds, &efds, tmp);
                    if (n < 0 || FD_ISSET(sock, &efds)) {
                        if (errno == EINTR) {
                            continue;
                        }
                        throw "UNIX socket select() failed";
                    }
                    if (n == 0) {
                        CloseSocket(sock);
                        Close();
                        return eIO_Timeout;
                    }
                    assert(FD_ISSET(sock, &wfds));
                    break;
                }
                // Check connection
                x_errno = 0;
                socklen_t x_len = (socklen_t) sizeof(x_errno);
                if ((getsockopt(sock, SOL_SOCKET, SO_ERROR, &x_errno, 
                                &x_len) != 0  ||  x_errno != 0)) {
                    throw "UNIX socket getsockopt() failed";
                }
                if (x_errno == ECONNREFUSED) {
                    status = eIO_Closed;
                    throw "UNIX socket connection refused";
                }
            }
        }

        // Create I/O socket
        if (SOCK_CreateOnTop(&sock, sizeof(sock), &m_IoSocket) !=
            eIO_Success) {
            throw "UNIX socket cannot convert to SOCK";
        }
    }
    catch (const char* what) {
        if (sock >= 0) {
            CloseSocket(sock);
        }
        Close();
        ERR_POST(s_FormatErrorMessage("Open", what));
        return status;
    }
    return eIO_Success;
}


EIO_Status CNamedPipeHandle::Create(const string& pipename,
                                    size_t        bufsize)
{
    struct sockaddr_un addr;
    mode_t u;
    EIO_Status status = eIO_Unknown;

    try {
        if (m_LSocket >= 0  ||  m_IoSocket) {
            throw "Pipe is already open";
        }
        if (sizeof(addr.sun_path) <= pipename.length()) {
            status = eIO_InvalidArg;
            throw "Pipe name too long";
        }
        m_Bufsize = bufsize;

        // Create a UNIX socket
        if ((m_LSocket = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
            throw "UNIX socket() failed";
        }
        // Remove any pre-existing socket (or other file)
        if (unlink(pipename.c_str()) != 0  &&  errno != ENOENT) {
            throw "UNIX socket unlink(\"" + pipename + "\") failed";
        }

        // Bind socket
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
#ifdef HAVE_SIN_LEN
        addr.sun_len = (socklen_t) sizeof(addr);
#endif
        strcpy(addr.sun_path, pipename.c_str());
        u = umask(0);
        if (bind(m_LSocket, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
            umask(u);
            throw "UNIX socket bind(\"" + pipename + "\") failed";
        }
        umask(u);
#ifndef NCBI_OS_IRIX
        fchmod(m_LSocket, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
        // Listen for connections on a socket
        if (listen(m_LSocket, kListenQueueSize) != 0) {
            throw "UNIX socket listen() failed";
        }
        if (fcntl(m_LSocket, F_SETFL,
                  fcntl(m_LSocket, F_GETFL, 0) | O_NONBLOCK) == -1) {
            throw "UNIX socket set to non-blocking failed";
        }
    }
    catch (const char* what) {
        Close();
        ERR_POST(s_FormatErrorMessage("Create", what));
        return status;
    }
    return eIO_Success;
}


EIO_Status CNamedPipeHandle::Listen(const STimeout* timeout)
{
    EIO_Status status = eIO_Unknown;
    int        sock   = -1;

    try {
        if (m_LSocket < 0) {
            status = eIO_Closed;
            throw "Pipe is closed";
        }

        // Auto-resume if interrupted by a signal
        for (;;) {
            
            // Wait for the client to connect
            fd_set          fd;
            struct timeval  tm;
            struct timeval* tmp = 0;

            if ( !timeout ) {
                tm.tv_sec = timeout->sec;
                tm.tv_usec = timeout->usec;
                tmp = &tm;
            }   
            FD_ZERO(&fd);
            FD_SET(m_LSocket, &fd);

            int n = select(m_LSocket + 1, &fd, 0, 0, tmp);
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw "UNIX socket select() failed";
            }
            if (n == 0) {
                return eIO_Timeout;
            }
            assert(n == 1);

            // Can accept next connection from the list of waiting ones
            struct sockaddr_un addr;
            socklen_t addrlen = (socklen_t) sizeof(addr);
            memset(&addr, 0, sizeof(addr));
#  ifdef HAVE_SIN_LEN
            addr.sun_len = sizeof(addr);
#  endif
            if ((sock = accept(m_LSocket, (struct sockaddr*)&addr,
                               &addrlen)) < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw "UNIX socket accept() failed";
            }
            break;
        }
        // Set buffer size
        if ( m_Bufsize != CNamedPipe::kDefaultBufferSize) {
            if ( !SetSocketBufferSize(sock, m_Bufsize, SO_SNDBUF)  ||
                 !SetSocketBufferSize(sock, m_Bufsize, SO_RCVBUF) ) {
                throw "UNIX socket set buffer size failed";
            }
        }
        // Create new I/O socket
        if (SOCK_CreateOnTop(&sock, sizeof(sock), &m_IoSocket) !=
            eIO_Success) {
            throw "UNIX socket cannot convert to SOCK";
        }
    }
    catch (const char* what) {
        if (sock >= 0) {
            CloseSocket(sock);
        }
        Close();
        ERR_POST(s_FormatErrorMessage("Listen", what));
        return status;
    }
    return eIO_Success;
}


EIO_Status CNamedPipeHandle::Disconnect(void)
{
    if ( !m_IoSocket ) {
        return eIO_Closed;
    }
    // Close I/O socket
    EIO_Status status = SOCK_Close(m_IoSocket);
    m_IoSocket = 0;
    return status;
}


EIO_Status CNamedPipeHandle::Close(void)
{
    // Disconnect current client
    EIO_Status status = Disconnect();

    // Close command socket
    if (m_LSocket >= 0) {
        if ( !CloseSocket(m_LSocket) ) {
            ERR_POST(s_FormatErrorMessage("Close",
                                          "UNIX socket close() failed"));
        }
        m_LSocket = -1;
    }
    return status;
}


EIO_Status CNamedPipeHandle::Read(void* buf, size_t count, size_t* n_read,
                                  const STimeout* timeout)
{
    EIO_Status status = eIO_Closed;

    try {
        if ( !m_IoSocket ) {
            throw "Pipe is closed";
        }
        if ( !count ) {
            // *n_read == 0
            return eIO_Success;
        }
        status = SOCK_SetTimeout(m_IoSocket, eIO_Read, timeout);
        if (status == eIO_Success) {
            status = SOCK_Read(m_IoSocket, buf, count, n_read, eIO_ReadPlain);
        }
    }
    catch (const char* what) {
        ERR_POST(s_FormatErrorMessage("Read", what));
    }
    return status;
}


EIO_Status CNamedPipeHandle::Write(const void* buf, size_t count,
                                   size_t* n_written, const STimeout* timeout)

{
    EIO_Status status = eIO_Closed;

    try {
        if ( !m_IoSocket ) {
            throw "Pipe is closed";
        }
        if ( !count ) {
            // *n_written == 0
            return eIO_Success;
        }
        status = SOCK_SetTimeout(m_IoSocket, eIO_Write, timeout);
        if (status == eIO_Success) {
            status = SOCK_Write(m_IoSocket, buf, count, n_written,
                                eIO_WritePlain);
        }
    }
    catch (const char* what) {
        ERR_POST(s_FormatErrorMessage("Write", what));
    }
    return status;
}


EIO_Status CNamedPipeHandle::Status(EIO_Event direction)
{
    if ( !m_IoSocket ) {
        return eIO_Closed;
    }
    return SOCK_Status(m_IoSocket, direction);
}


bool CNamedPipeHandle::CloseSocket(int sock)
{
    if (sock >= 0) {
        for (;;) {
            if (close(sock) == 0) {
                break;
            }
            if (errno != EINTR) {
                return false;
            }
        }
    }
    return true;
}


bool CNamedPipeHandle::SetSocketBufferSize(int sock, size_t bufsize, int dir)
{
    socklen_t bs_old = 0;
    socklen_t bs_len = sizeof(bs_old);
    socklen_t bs_new = (socklen_t)bufsize;

    if (getsockopt(sock, SOL_SOCKET, dir, &bs_old, &bs_len) == 0  &&
        bs_new > bs_old) {
        if (setsockopt(sock, SOL_SOCKET, dir, &bs_new, bs_len) != 0) {
            return false;
        }
    }
    return true;
}


#endif  /* NCBI_OS_UNIX | NCBI_OS_MSWIN */



//////////////////////////////////////////////////////////////////////////////
//
// CNamedPipe
//


CNamedPipe::CNamedPipe(void)
    : m_PipeName(kEmptyStr), m_PipeHandle(0), m_Bufsize(kDefaultBufferSize),
      m_OpenTimeout(0), m_ReadTimeout(0), m_WriteTimeout(0)
{
    // Create a new OS-specific pipe handle
    m_PipeHandle = new CNamedPipeHandle();
}


CNamedPipe::~CNamedPipe(void)
{
    Close();
}


EIO_Status CNamedPipe::Close()
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    return m_PipeHandle->Close();
}
     

EIO_Status CNamedPipe::Read(void* buf, size_t count, size_t* n_read)
{
    if ( n_read ) {
        *n_read = 0;
    }
    if ( !buf ) {
        return eIO_InvalidArg;
    }
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    return m_PipeHandle->Read(buf, count, n_read, m_ReadTimeout);
}


EIO_Status CNamedPipe::Write(const void* buf, size_t count, size_t*  n_written)
{
    if ( n_written ) {
        *n_written = 0;
    }
    if ( !buf ) {
        return eIO_InvalidArg;
    }
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    return m_PipeHandle->Write(buf, count, n_written, m_WriteTimeout);
}


EIO_Status CNamedPipe::Status(EIO_Event direction)
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    return m_PipeHandle->Status(direction);
}


EIO_Status CNamedPipe::SetTimeout(EIO_Event event, const STimeout* timeout)
{
    if (timeout == kDefaultTimeout) {
        return eIO_Success;
    }
    switch ( event ) {
    case eIO_Open:
        m_OpenTimeout  = s_SetTimeout(timeout, &m_OpenTimeoutValue);
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


const STimeout* CNamedPipe::GetTimeout(EIO_Event event) const
{
    switch ( event ) {
    case eIO_Open:
        return m_OpenTimeout;
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
// CNamedPipeClient
//

CNamedPipeClient::CNamedPipeClient(void)
{
    m_IsClientSide = true;
}


CNamedPipeClient::CNamedPipeClient(const string&   pipename,
                                   const STimeout* timeout, 
                                   size_t          bufsize)
{
    m_IsClientSide = true;
    Open(pipename, timeout, bufsize);
}


EIO_Status CNamedPipeClient::Open(const string&    pipename,
                                  const STimeout*  timeout,
                                  size_t           bufsize)
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    m_PipeName = pipename;
    m_Bufsize  = bufsize;

    SetTimeout(eIO_Open, timeout);
    return m_PipeHandle->Open(pipename, m_OpenTimeout, m_Bufsize);
}



//////////////////////////////////////////////////////////////////////////////
//
// CNamedPipeServer
//


CNamedPipeServer::CNamedPipeServer(void)
{
    m_IsClientSide = false;
}


CNamedPipeServer::CNamedPipeServer(const string&   pipename,
                                   const STimeout* timeout,
                                   size_t          bufsize)
{
    m_IsClientSide = false;
    Create(pipename, timeout, bufsize);
}


EIO_Status CNamedPipeServer::Create(const string&  pipename,
                                   const STimeout* timeout,
                                   size_t          bufsize)
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    m_PipeName = pipename;
    m_Bufsize  = bufsize;

    SetTimeout(eIO_Open, timeout);
    return m_PipeHandle->Create(pipename, bufsize);
}


EIO_Status CNamedPipeServer::Listen(void)
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    return m_PipeHandle->Listen(m_OpenTimeout);
}


EIO_Status CNamedPipeServer::Disconnect(void)
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    // Disconnect current client
    return m_PipeHandle->Disconnect();
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2003/09/05 19:52:37  ivanov
 * + UNIX CNamedPipeHandle::SetSocketBufferSize()
 *
 * Revision 1.12  2003/09/03 14:48:32  ivanov
 * Fix for previous commit
 *
 * Revision 1.11  2003/09/03 14:29:58  ivanov
 * Set r/w status to eIO_Success in the CNamedPipeHandle::Open/Create
 *
 * Revision 1.10  2003/09/02 19:51:17  ivanov
 * Fixed incorrect infinite timeout handling in the CNamedPipeHandle::Open()
 *
 * Revision 1.9  2003/08/28 16:03:05  ivanov
 * Use os-specific Status() function
 *
 * Revision 1.8  2003/08/25 14:41:22  lavr
 * Employ new k..Timeout constants
 *
 * Revision 1.7  2003/08/20 14:22:20  ivanov
 * Get rid of warning -- double variable declaration
 *
 * Revision 1.6  2003/08/19 21:02:12  ivanov
 * Other fix for error messages and comments.
 *
 * Revision 1.5  2003/08/19 20:52:45  ivanov
 * UNIX: Fixed a waiting method for socket connection in the
 * CNamedPipeHandle::Open() (by Anton Lavrentiev). Fixed some error
 * messages and comments. UNIX sockets can have a zero value.
 *
 * Revision 1.4  2003/08/19 14:34:43  ivanov
 * + #include <sys/time.h> for UNIX
 *
 * Revision 1.3  2003/08/18 22:53:59  vakatov
 * Fix for the platforms which are missing type 'socklen_t'
 *
 * Revision 1.2  2003/08/18 20:51:56  ivanov
 * Retry 'connect()' syscall if interrupted and allowed to restart
 * (by Anton Lavrentiev)
 *
 * Revision 1.1  2003/08/18 19:18:23  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
