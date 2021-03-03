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
 * Author:  Anton Lavrentiev, Mike DiCuccio, Vladimir Ivanov
 *
 * File Description:
 *   Portable interprocess named pipe API for:  UNIX, MS-Win
 *
 */

#include <ncbi_pch.hpp>
#include <connect/error_codes.hpp>
#include <connect/ncbi_namedpipe.hpp>
#include <connect/ncbi_util.h>
#include <corelib/ncbifile.hpp>

#if defined(NCBI_OS_UNIX)

#  include <connect/ncbi_socket_unix.h>
#  include <errno.h>
#  include <unistd.h>
#  include <sys/socket.h>
#  include <sys/types.h>

#elif !defined(NCBI_OS_MSWIN)
#  error "The CNamedPipe class is supported only on Windows and Unix"
#endif


#define NCBI_USE_ERRCODE_X   Connect_Pipe


#define NAMEDPIPE_THROW(err, errtxt)                \
    THROW0_TRACE(x_FormatError(int(err), errtxt))


BEGIN_NCBI_SCOPE


#if defined(HAVE_SOCKLEN_T)  ||  defined(_SOCKLEN_T)
typedef socklen_t  SOCK_socklen_t;
#else
typedef int        SOCK_socklen_t;
#endif /*HAVE_SOCKLEN_T || _SOCKLEN_T*/


//////////////////////////////////////////////////////////////////////////////
//
// Auxiliary functions
//

static inline const STimeout* s_SetTimeout(const STimeout* from, STimeout* to)
{
    if ( !from ) {
        return kInfiniteTimeout;
    }
    to->sec  = from->usec / kMicroSecondsPerSecond + from->sec;
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
    return "[CNamedPipe::" + where + "]  " + what;
}



//////////////////////////////////////////////////////////////////////////////
//
// Class CNamedPipeHandle handles forwarded requests from CNamedPipe.
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
// CNamedPipeHandle -- MS Windows version
//

class CNamedPipeHandle
{
public:
    CNamedPipeHandle(void);
    ~CNamedPipeHandle();

    // client-side

    EIO_Status Open(const string& pipename, const STimeout* timeout,
                    size_t pipesize, CNamedPipeClient::TFlags flags);

    // server-side

    EIO_Status Create(const string& pipename, size_t pipesize);
    EIO_Status Listen(const STimeout* timeout);
    EIO_Status Disconnect(void);

    // common

    EIO_Status Close(void);
    EIO_Status Read (void*       buf, size_t count, size_t* n_read,
                     const STimeout* timeout);
    EIO_Status Write(const void* buf, size_t count, size_t* n_written,
                     const STimeout* timeout);
    EIO_Status Wait(EIO_Event event, const STimeout* timeout);
    EIO_Status Status(EIO_Event direction) const;

private:
    EIO_Status x_Flush(void);
    EIO_Status x_Disconnect(bool orderly = true);
    EIO_Status x_WaitForRead(const STimeout* timeout, DWORD* in_avail);

    HANDLE     m_Pipe;         // pipe I/O handle
    string     m_PipeName;     // pipe name
    int        m_Connected;    // if connected (-1=server; 1=client)
    EIO_Status m_ReadStatus;   // last read status
    EIO_Status m_WriteStatus;  // last write status
};


CNamedPipeHandle::CNamedPipeHandle(void)
    : m_Pipe(INVALID_HANDLE_VALUE),
      m_Connected(0), m_ReadStatus(eIO_Closed), m_WriteStatus(eIO_Closed)
{
    return;
}


CNamedPipeHandle::~CNamedPipeHandle()
{
    Close();
}


EIO_Status CNamedPipeHandle::Open(const string&            pipename,
                                  const STimeout*          timeout,
                                  size_t                   /*pipesize*/,
                                  CNamedPipeClient::TFlags flags)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pipe != INVALID_HANDLE_VALUE) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + "\" already open");
        }

        // Set the base security attributes
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.bInheritHandle = TRUE;
        attr.lpSecurityDescriptor = NULL;

        // Wait until either a time-out interval elapses or an instance of
        // the specified named pipe is available for connection (that is, the
        // pipe's server process has a pending Listen() operation on the pipe).

        // NOTE:  We do not use WaitNamedPipe() here because it works
        //        incorrectly in some cases!
        HANDLE pipe;

        DWORD x_timeout = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;

        unsigned long x_sleep = 1;
        for (;;) {
            // Try to open existing pipe
            pipe = ::CreateFile(_T_XCSTRING(pipename),
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                &attr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                NULL);
            DWORD error;
            if (pipe != INVALID_HANDLE_VALUE) {
                DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;  // non-blocking
                if ( ::SetNamedPipeHandleState(pipe, &mode, NULL, NULL) ) {
                    break;
                }
                error = ::GetLastError();
                ::CloseHandle(pipe);
            } else
                error = ::GetLastError();
            // NB: "pipe" is closed at this point
            if ((pipe == INVALID_HANDLE_VALUE
                 &&  error != ERROR_PIPE_BUSY)  ||
                (pipe != INVALID_HANDLE_VALUE
                 &&  error != ERROR_PIPE_NOT_CONNECTED)) {
                if (pipe == INVALID_HANDLE_VALUE
                    &&  error == ERROR_FILE_NOT_FOUND) {
                    status = eIO_Closed;
                    if (flags & CNamedPipeClient::fNoLogIfClosed) {
                        return status;
                    }
                }
                NAMEDPIPE_THROW(error,
                                "Named pipe \"" + pipename
                                + "\" failed to "
                                + string(pipe == INVALID_HANDLE_VALUE
                                         ? "open" : "set non-blocking"));
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
        _ASSERT(pipe != INVALID_HANDLE_VALUE);

        m_Pipe        = pipe;
        m_PipeName    = pipename;
        m_Connected   = 1/*client*/;
        m_ReadStatus  = eIO_Success;
        m_WriteStatus = eIO_Success;
        return eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(10, s_FormatErrorMessage("Open", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::Create(const string& pipename,
                                    size_t        pipesize)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pipe != INVALID_HANDLE_VALUE) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + "\" already exists");
        }

        if (pipesize > numeric_limits<DWORD>::max()) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + pipename
                            + "\" buffer size "
                            + NStr::NumericToString(pipesize)
                            + " too large");
        }

        // Set the base security attributes
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.bInheritHandle = TRUE;
        attr.lpSecurityDescriptor = NULL;

        // Create pipe
        m_Pipe = ::CreateNamedPipe
            (_T_XCSTRING(pipename),         // pipe name 
             PIPE_ACCESS_DUPLEX,            // read/write access 
             PIPE_TYPE_BYTE | PIPE_NOWAIT,  // byte-type, non-blocking mode 
             1,                             // one instance only 
             (DWORD) pipesize,              // output buffer size 
             (DWORD) pipesize,              // input  buffer size 
             INFINITE,                      // default client timeout
             &attr);                        // security attributes

        if (m_Pipe == INVALID_HANDLE_VALUE) {
            DWORD error = ::GetLastError();
            if (error == ERROR_ALREADY_EXISTS) {
                status = eIO_Closed;
            }
            NAMEDPIPE_THROW(error,
                            "Named pipe \"" + pipename
                            + "\" failed to create");
        }

        _ASSERT(!m_Connected);
        m_PipeName    = pipename;
        m_ReadStatus  = eIO_Success;
        m_WriteStatus = eIO_Success;
        return eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(11, s_FormatErrorMessage("Create", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::Listen(const STimeout* timeout)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pipe == INVALID_HANDLE_VALUE  ||  m_Connected) {
            status = eIO_Closed;
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + '"' + string(m_Pipe == INVALID_HANDLE_VALUE
                                           ? " closed"
                                           : " busy"));
        }

        DWORD x_timeout = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;

        // Wait for the client to connect, or time out.
        // NOTE:  WaitForSingleObject() does not work with pipes.

        unsigned long x_sleep = 1;
        for (;;) {
            if ( ::ConnectNamedPipe(m_Pipe, NULL) ) {
                break; // client connected while within ConnectNamedPipe()
            }
            DWORD error = ::GetLastError();
            if (error == ERROR_PIPE_CONNECTED) {
                break; // client connected before ConnectNamedPipe() was called
            }
            
            // NB: status == eIO_Unknown
            if (error == ERROR_NO_DATA/*not disconnected???*/) {
                if (x_Disconnect(false/*abort*/) == eIO_Success) {
                    continue; // try again
                }
                NAMEDPIPE_THROW(error,
                                "Named pipe \"" + m_PipeName
                                + "\" still connected");
            }
            if (error != ERROR_PIPE_LISTENING) {
                NAMEDPIPE_THROW(error,
                                "Named pipe \"" + m_PipeName
                                + "\" not listening");
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

        // Pipe connected
        m_Connected   = -1/*server*/;
        m_ReadStatus  = eIO_Success;
        m_WriteStatus = eIO_Success;
        return eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(12, s_FormatErrorMessage("Listen", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::x_Flush(void)
{
    if (m_Connected  &&  !::FlushFileBuffers(m_Pipe)) {
        NAMEDPIPE_THROW(::GetLastError(),
                        "Named pipe \"" + m_PipeName
                        + "\" failed to flush");
    }
    return eIO_Success;
}


EIO_Status CNamedPipeHandle::x_Disconnect(bool orderly)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Connected <= 0  &&  orderly) {
            if (m_Pipe == INVALID_HANDLE_VALUE  ||  !m_Connected) {
                status = eIO_Closed;
                NAMEDPIPE_THROW(0,
                                "Named pipe \"" + m_PipeName
                                + "\" already disconnected");
            }
            status = x_Flush();
        } else {
            status = eIO_Success;
        }
        if (m_Connected <= 0  &&  !::DisconnectNamedPipe(m_Pipe)) {
            status = eIO_Unknown;
            if (orderly) {
                NAMEDPIPE_THROW(::GetLastError(),
                                "Named pipe \"" + m_PipeName
                                + "\" failed to disconnect");
            }
        } else {
            // Per documentation, another client can now connect again
            m_Connected = 0;
        }
    }
    catch (string& what) {
        ERR_POST_X(13, s_FormatErrorMessage("Disconnect", what));
    }

    m_ReadStatus  = eIO_Closed;
    m_WriteStatus = eIO_Closed;
    return status;
}


EIO_Status CNamedPipeHandle::Disconnect(void)
{
    return x_Disconnect(/*orderly*/);
}


EIO_Status CNamedPipeHandle::Close(void)
{
    if (m_Pipe == INVALID_HANDLE_VALUE) {
        return eIO_Closed;
    }
    EIO_Status status = eIO_Unknown;
    try {
        status = x_Flush();
    }
    catch (string& what) {
        ERR_POST_X(8, s_FormatErrorMessage("Close", what));
    }
    (void) ::CloseHandle(m_Pipe);
    m_Pipe = INVALID_HANDLE_VALUE;
    m_ReadStatus  = eIO_Closed;
    m_WriteStatus = eIO_Closed;
    return status;
}


EIO_Status CNamedPipeHandle::x_WaitForRead(const STimeout* timeout,
                                           DWORD*          in_avail)
{
    _ASSERT(m_Connected);

    *in_avail = 0;

    DWORD x_timeout = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;

    // Wait for data from the pipe with timeout.
    // NOTE:  WaitForSingleObject() does not work with pipes.

    unsigned long x_sleep = 1;
    for (;;) {
        BOOL ok = ::PeekNamedPipe(m_Pipe, NULL, 0, NULL, in_avail, NULL);
        if ( *in_avail ) {
            if (!(m_Connected & 2)) {
                m_Connected |= 2;
            }
            break;
        }
        if ( !ok ) {
            // Has peer closed the connection?
            DWORD error = ::GetLastError();
            if ((m_Connected & 2)  ||  error != ERROR_PIPE_NOT_CONNECTED) {
                if ( !x_IsDisconnectError(error) ) {
                    m_ReadStatus = eIO_Unknown;
                    return eIO_Unknown;
                }
                m_ReadStatus  = eIO_Closed;
                m_WriteStatus = eIO_Closed;
                return eIO_Closed;
            }
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

    _ASSERT(*in_avail);
    return eIO_Success;
}


EIO_Status CNamedPipeHandle::Read(void* buf, size_t count, size_t* n_read,
                                  const STimeout* timeout)
{
    _ASSERT(n_read  &&  !*n_read);

    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pipe == INVALID_HANDLE_VALUE  ||  !m_Connected) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + "\" closed");
        }
        if (m_ReadStatus == eIO_Closed) {
            return eIO_Closed;
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD bytes_avail;
        if ((status = x_WaitForRead(timeout, &bytes_avail)) == eIO_Success) {
            _ASSERT(bytes_avail);
            // We must read only "count" bytes of data regardless of the amount
            // available to read.
            if (bytes_avail >         count) {
                bytes_avail = (DWORD) count;
            }
            BOOL ok = ::ReadFile(m_Pipe, buf, bytes_avail, &bytes_avail, NULL);
            if ( !bytes_avail ) {
                status = eIO_Unknown;
                m_ReadStatus = eIO_Unknown;
                NAMEDPIPE_THROW(!ok ? ::GetLastError() : 0,
                                "Named pipe \"" + m_PipeName
                                + "\" read failed");
            } else {
                // NB: status == eIO_Success
                m_ReadStatus = eIO_Success;
                *n_read = bytes_avail;
            }
        } else if (status == eIO_Timeout) {
            m_ReadStatus = eIO_Timeout;
        } else if (status != eIO_Closed) {
            NAMEDPIPE_THROW(::GetLastError(),
                            "Named pipe \"" + m_PipeName
                            + "\" peek failed");
        }
    }
    catch (string& what) {
        ERR_POST_X(14, s_FormatErrorMessage("Read", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::Write(const void* buf, size_t count,
                                   size_t* n_written, const STimeout* timeout)

{
    _ASSERT(n_written  &&  !*n_written);

    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pipe == INVALID_HANDLE_VALUE  ||  !m_Connected) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + "\" closed");
        }
        if (m_WriteStatus == eIO_Closed) {
            return eIO_Closed;
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;
        DWORD to_write  = (count > numeric_limits<DWORD>::max()
                           ? numeric_limits<DWORD>::max()
                           : (DWORD) count);
        DWORD bytes_written = 0;

        unsigned long x_sleep = 1;
        for (;;) {
            BOOL ok = ::WriteFile(m_Pipe, buf, to_write, &bytes_written, NULL);
            if ( bytes_written ) {
                if (!(m_Connected & 2)) {
                    m_Connected |= 2;
                }
                m_WriteStatus = ok ? eIO_Success : eIO_Unknown;
                break;
            }
            if ( !ok ) {                
                DWORD error = ::GetLastError();
                if ((m_Connected & 2)  ||  error != ERROR_PIPE_NOT_CONNECTED) {
                    if ( x_IsDisconnectError(error) ) {
                        m_ReadStatus  = eIO_Closed;
                        m_WriteStatus = eIO_Closed;
                        status        = eIO_Closed;
                    } else {
                        m_WriteStatus = eIO_Unknown;
                        // NB: status == eIO_Unknown
                    }
                    NAMEDPIPE_THROW(error,
                                    "Named pipe \"" + m_PipeName
                                    + "\" write failed");
                }
            }

            if ( !x_timeout ) {
                m_WriteStatus = eIO_Timeout;
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

        *n_written = bytes_written;
        status     = eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(15, s_FormatErrorMessage("Write", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::Wait(EIO_Event event, const STimeout* timeout)
{
    if (m_Pipe == INVALID_HANDLE_VALUE  ||  !m_Connected) {
        ERR_POST_X(9, s_FormatErrorMessage("Wait",
                                           "Named pipe \"" + m_PipeName
                                           + "\" closed"));
        return eIO_Unknown;
    }
    if (m_ReadStatus  == eIO_Closed) {
        event = (EIO_Event)(event & ~eIO_Read);
    }
    if (m_WriteStatus == eIO_Closed) {
        event = (EIO_Event)(event & ~eIO_Write);
    }
    if (!(event & eIO_Read)) {
        return event ? eIO_Success : eIO_Closed;
    }
    DWORD x_avail;
    EIO_Status status = x_WaitForRead(timeout, &x_avail);
    return status == eIO_Closed ? eIO_Success : status;
}


EIO_Status CNamedPipeHandle::Status(EIO_Event direction) const
{
    _ASSERT(m_Connected  ||  (m_ReadStatus  == eIO_Closed  &&
                              m_WriteStatus == eIO_Closed));
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


#elif defined(NCBI_OS_UNIX)


//////////////////////////////////////////////////////////////////////////////
//
// CNamedPipeHandle -- Unix version
//

// The maximum length the queue of pending connections may grow to
static const int kListenQueueSize = 64;

class CNamedPipeHandle
{
public:
    CNamedPipeHandle(void);
    ~CNamedPipeHandle();

    // client-side

    EIO_Status Open(const string& pipename, const STimeout* timeout,
                    size_t pipesize, CNamedPipeClient::TFlags flags);

    // server-side

    EIO_Status Create(const string& pipename, size_t pipesize);
    EIO_Status Listen(const STimeout* timeout);
    EIO_Status Disconnect(void);

    // common

    EIO_Status Close(void);
    EIO_Status Read (void* buf, size_t count, size_t* n_read,
                     const STimeout* timeout);
    EIO_Status Write(const void* buf, size_t count, size_t* n_written,
                     const STimeout* timeout);
    EIO_Status Wait(EIO_Event event, const STimeout* timeout);
    EIO_Status Status(EIO_Event direction) const;

private:
    // Set socket I/O buffer size (dir: SO_SNDBUF, SO_RCVBUF)
    bool x_SetSocketBufSize(int sock, size_t bufsize, int dir);
    // Disconnect implementation
    EIO_Status x_Disconnect(const char* where);

private:
    LSOCK      m_LSocket;   // listening socket
    SOCK       m_IoSocket;  // I/O socket
    size_t     m_PipeSize;  // pipe size
    string     m_PipeName;  // pipe name
};


CNamedPipeHandle::CNamedPipeHandle(void)
    : m_LSocket(0), m_IoSocket(0), m_PipeSize(0)
{
    return;
}


CNamedPipeHandle::~CNamedPipeHandle()
{
    Close();
}


EIO_Status CNamedPipeHandle::Open(const string&            pipename,
                                  const STimeout*          timeout,
                                  size_t                   pipesize,
                                  CNamedPipeClient::TFlags flags)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_LSocket  ||  m_IoSocket) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + "\" already open");
        }

        status = SOCK_CreateUNIX(pipename.c_str(), timeout, &m_IoSocket,
                                 NULL, 0, 0/*flags*/);
        if (status == eIO_Closed
            &&  (flags & CNamedPipeClient::fNoLogIfClosed)) {
            return status;
        }
        if (status != eIO_Success) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + pipename
                            + "\" failed to open UNIX socket: "
                            + string(IO_StatusStr(status)));
        }
        SOCK_SetTimeout(m_IoSocket, eIO_Close, timeout);

        // Set buffer size
        if (pipesize) {
            int fd;
            if (SOCK_GetOSHandle(m_IoSocket, &fd, sizeof(fd)) == eIO_Success) {
                if (!x_SetSocketBufSize(fd, pipesize, SO_SNDBUF)  ||
                    !x_SetSocketBufSize(fd, pipesize, SO_RCVBUF)) {
                    int error = errno;
                    _ASSERT(error);
                    NAMEDPIPE_THROW(error,
                                    "Named pipe \"" + pipename
                                    + "\" failed to set"
                                    " UNIX socket buffer size "
                                    + NStr::NumericToString(pipesize));
                }
            }
        }

        m_PipeSize = 0/*not needed*/;
        m_PipeName = pipename;
        return eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(10, s_FormatErrorMessage("Open", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::Create(const string& pipename,
                                    size_t        pipesize)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_LSocket  ||  m_IoSocket) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + "\" already exists");
        }

        CDirEntry pipe(pipename);
        switch (pipe.GetType()) {
        case CDirEntry::eSocket:
            pipe.Remove();
            /*FALLTHRU*/
        case CDirEntry::eUnknown:
            // File does not exist
            break;
        default:
            status = eIO_Unknown;
            NAMEDPIPE_THROW(0,
                            "Named pipe path \"" + pipename
                            + "\" already exists");
        }

        status = LSOCK_CreateUNIX(pipename.c_str(),
                                  kListenQueueSize,
                                  &m_LSocket, 0);
        if (status != eIO_Success) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + pipename
                            + "\" failed to create listening"
                            " UNIX socket: " + string(IO_StatusStr(status)));
        }

        m_PipeSize = pipesize;
        m_PipeName = pipename;
        return eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(11, s_FormatErrorMessage("Create", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::Listen(const STimeout* timeout)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (!m_LSocket  ||  m_IoSocket) {
            status = eIO_Closed;
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + '"' + string(m_LSocket
                                           ? " closed"
                                           : " busy"));
        }

        status = LSOCK_Accept(m_LSocket, timeout, &m_IoSocket);
        if (status == eIO_Timeout) {
            return status;
        }
        if (status != eIO_Success) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + "\" failed to accept in UNIX socket: "
                            + string(IO_StatusStr(status)));
        }
        _ASSERT(m_IoSocket);

        // Set buffer size
        if (m_PipeSize) {
            int fd;
            if (SOCK_GetOSHandle(m_IoSocket, &fd, sizeof(fd)) == eIO_Success) {
                if (!x_SetSocketBufSize(fd, m_PipeSize, SO_SNDBUF)  ||
                    !x_SetSocketBufSize(fd, m_PipeSize, SO_RCVBUF)) {
                    NAMEDPIPE_THROW(errno,
                                    "Named pipe \"" + m_PipeName
                                    + "\" failed to set UNIX socket buffer "
                                    "size "+NStr::NumericToString(m_PipeSize));
                }
            }
        }

        return eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(12, s_FormatErrorMessage("Listen", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::x_Disconnect(const char* where)
{
    // Close I/O socket
    _ASSERT(m_IoSocket);
    EIO_Status status = SOCK_Close(m_IoSocket);
    m_IoSocket = 0;

    if (status != eIO_Success) {
        ERR_POST_X(8, s_FormatErrorMessage(where,
                                           x_FormatError(0,
                                                         "Named pipe \""
                                                         + m_PipeName + "\""
                                                         " failed to"
                                                         " disconnect")));
    }
    return status;
}


EIO_Status CNamedPipeHandle::Disconnect(void)
{
    if ( !m_IoSocket ) {
        ERR_POST_X(13, s_FormatErrorMessage("Disconnect",
                                            "Named pipe \"" + m_PipeName
                                            + "\" already disconnected"));
        return eIO_Closed;
    }
    return x_Disconnect("Disconnect");
}


EIO_Status CNamedPipeHandle::Close(void)
{
    if ( !m_LSocket  &&  !m_IoSocket) {
        return eIO_Closed;
    }

    // Disconnect if connected
    EIO_Status status = m_IoSocket ? x_Disconnect("Close") : eIO_Success;

    // Close listening socket
    if ( m_LSocket ) {
        (void) LSOCK_Close(m_LSocket);
        m_LSocket = 0;
    }
    return status;
}


EIO_Status CNamedPipeHandle::Read(void* buf, size_t count, size_t* n_read,
                                  const STimeout* timeout)
{
    _ASSERT(n_read  &&  !*n_read);

    EIO_Status status = eIO_Unknown;

    try {
        if ( !m_IoSocket ) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + "\" closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        _VERIFY(SOCK_SetTimeout(m_IoSocket, eIO_Read, timeout) == eIO_Success);
        status = SOCK_Read(m_IoSocket, buf, count, n_read,
                           eIO_ReadPlain);
        if (status != eIO_Success) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + "\" read failed: "
                            + string(IO_StatusStr(status)));
        }
    }
    catch (string& what) {
        ERR_POST_X(14, s_FormatErrorMessage("Read", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::Write(const void* buf, size_t count,
                                   size_t* n_written, const STimeout* timeout)

{
    _ASSERT(n_written  &&  !*n_written);

    EIO_Status status  = eIO_Unknown;

    try {
        if ( !m_IoSocket ) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + "\" closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        _VERIFY(SOCK_SetTimeout(m_IoSocket, eIO_Write, timeout)== eIO_Success);
        status = SOCK_Write(m_IoSocket, buf, count, n_written,
                            eIO_WritePlain);
        if (status != eIO_Success) {
            NAMEDPIPE_THROW(0,
                            "Named pipe \"" + m_PipeName
                            + "\" write failed: "
                            + string(IO_StatusStr(status)));
        }
    }
    catch (string& what) {
        ERR_POST_X(15, s_FormatErrorMessage("Write", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::Wait(EIO_Event event, const STimeout* timeout)
{
    if ( !m_IoSocket ) {
        ERR_POST_X(9, s_FormatErrorMessage("Wait",
                                           "Named pipe \"" + m_PipeName
                                           + "\" closed"));
        return eIO_Unknown;
    }
    return SOCK_Wait(m_IoSocket, event, timeout);
}


EIO_Status CNamedPipeHandle::Status(EIO_Event direction) const
{
    return !m_IoSocket ? eIO_Closed : SOCK_Status(m_IoSocket, direction);
}


bool CNamedPipeHandle::x_SetSocketBufSize(int sock, size_t bufsize, int dir)
{
    int            bs_old = 0;
    int            bs_new = (int) bufsize;
    SOCK_socklen_t bs_len = (SOCK_socklen_t) sizeof(bs_old);

    if (::getsockopt(sock, SOL_SOCKET, dir, &bs_old, &bs_len) == 0
        &&  bs_new > bs_old) {
        if (::setsockopt(sock, SOL_SOCKET, dir, &bs_new, bs_len) != 0) {
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


CNamedPipe::CNamedPipe(size_t pipesize)
    : m_PipeSize(pipesize),
      m_OpenTimeout(0), m_ReadTimeout(0), m_WriteTimeout(0)
{
    m_NamedPipeHandle = new CNamedPipeHandle;
}


CNamedPipe::~CNamedPipe()
{
    Close();
    delete m_NamedPipeHandle;
}


EIO_Status CNamedPipe::Close(void)
{
    _ASSERT(m_NamedPipeHandle);
    return m_NamedPipeHandle->Close();
}
     

EIO_Status CNamedPipe::Read(void* buf, size_t count, size_t* n_read)
{
    _ASSERT(m_NamedPipeHandle);
    size_t x_read;
    if ( !n_read ) {
        n_read = &x_read;
    }
    *n_read = 0;
    if (count  &&  !buf) {
        return eIO_InvalidArg;
    }
    return m_NamedPipeHandle->Read(buf, count, n_read, m_ReadTimeout);
}


EIO_Status CNamedPipe::Write(const void* buf, size_t count, size_t* n_written)
{
    _ASSERT(m_NamedPipeHandle);
    size_t x_written;
    if ( !n_written ) {
        n_written = &x_written;
    }
    *n_written = 0;
    if (count  &&  !buf) {
        return eIO_InvalidArg;
    }
    return m_NamedPipeHandle->Write(buf, count, n_written, m_WriteTimeout);
}


EIO_Status CNamedPipe::Wait(EIO_Event event, const STimeout* timeout)
{
    _ASSERT(m_NamedPipeHandle);
    if (timeout == kDefaultTimeout) {
        _TROUBLE;
        return eIO_InvalidArg;
    }
    switch (event) {
    case eIO_Read:
    case eIO_Write:
    case eIO_ReadWrite:
        break;
    default:
        _TROUBLE;
        return eIO_InvalidArg;
    }
    return m_NamedPipeHandle->Wait(event, timeout);
}


EIO_Status CNamedPipe::Status(EIO_Event direction) const
{
    _ASSERT(m_NamedPipeHandle);
    switch (direction) {
    case eIO_Read:
    case eIO_Write:
        break;
    default:
        _TROUBLE;
        return eIO_InvalidArg;
    }
    return m_NamedPipeHandle->Status(direction);
}


EIO_Status CNamedPipe::SetTimeout(EIO_Event event, const STimeout* timeout)
{
    _ASSERT(m_NamedPipeHandle);
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
        _TROUBLE;
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


const STimeout* CNamedPipe::GetTimeout(EIO_Event event) const
{
    _ASSERT(m_NamedPipeHandle);
    switch ( event ) {
    case eIO_Open:
        return m_OpenTimeout;
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


void CNamedPipe::x_SetName(const string& pipename)
{
#ifdef NCBI_OS_MSWIN
    static const char kSeparators[] = ":/\\";
#else
    static const char kSeparators[] = "/";
#endif
    if (pipename.find_first_of(kSeparators) != NPOS) {
        m_PipeName = pipename;
        return;
    }

#if defined(NCBI_OS_MSWIN)
    m_PipeName = "\\\\.\\pipe\\" + pipename;
#elif defined(NCBI_OS_UNIX)
    struct stat st;

    const char* pipedir = "/var/tmp";
    if (::stat(pipedir, &st) != 0  ||  !S_ISDIR(st.st_mode)
        ||  ::access(pipedir, W_OK) != 0) {
        pipedir = "/tmp";
        if (::stat(pipedir, &st) != 0  ||  !S_ISDIR(st.st_mode)
            ||  ::access(pipedir, W_OK) != 0) {
            pipedir = ".";
        }
    }
    m_PipeName = string(pipedir) + '/' + pipename;
#else
    m_PipeName = pipename;
#endif
}



//////////////////////////////////////////////////////////////////////////////
//
// CNamedPipeClient
//

CNamedPipeClient::CNamedPipeClient(size_t pipesize)
    : CNamedPipe(pipesize)
{
    _ASSERT(m_NamedPipeHandle);
    m_IsClientSide = true;
}


CNamedPipeClient::CNamedPipeClient(const string&            pipename,
                                   const STimeout*          timeout, 
                                   size_t                   pipesize,
                                   CNamedPipeClient::TFlags flags)
    : CNamedPipe(pipesize)
{
    m_IsClientSide = true;
    Open(pipename, timeout, flags);
}


EIO_Status CNamedPipeClient::Open(const string&            pipename,
                                  const STimeout*          timeout,
                                  size_t                   pipesize,
                                  CNamedPipeClient::TFlags flags)
{
    _ASSERT(m_NamedPipeHandle  &&  m_IsClientSide);
    if (pipesize) {
        m_PipeSize = pipesize;
    }
    x_SetName(pipename);

    SetTimeout(eIO_Open, timeout);
    return m_NamedPipeHandle->Open(m_PipeName, m_OpenTimeout,
                                   m_PipeSize, flags);
}



//////////////////////////////////////////////////////////////////////////////
//
// CNamedPipeServer
//


CNamedPipeServer::CNamedPipeServer(size_t pipesize)
    : CNamedPipe(pipesize)
{
    _ASSERT(m_NamedPipeHandle);
    m_IsClientSide = false;
}


CNamedPipeServer::CNamedPipeServer(const string&   pipename,
                                   const STimeout* timeout,
                                   size_t          pipesize)
    : CNamedPipe(pipesize)
{
    m_IsClientSide = false;
    Create(pipename, timeout);
}


EIO_Status CNamedPipeServer::Create(const string&   pipename,
                                    const STimeout* timeout,
                                    size_t          pipesize)
{
    _ASSERT(m_NamedPipeHandle  &&  !m_IsClientSide);
    if (pipesize) {
        m_PipeSize = pipesize;
    }
    x_SetName(pipename);

    SetTimeout(eIO_Open, timeout);
    return m_NamedPipeHandle->Create(m_PipeName, m_PipeSize);
}


EIO_Status CNamedPipeServer::Listen(void)
{
    _ASSERT(m_NamedPipeHandle  &&  !m_IsClientSide);
    return m_NamedPipeHandle->Listen(m_OpenTimeout);
}


EIO_Status CNamedPipeServer::Disconnect(void)
{
    _ASSERT(m_NamedPipeHandle  &&  !m_IsClientSide);
    return m_NamedPipeHandle->Disconnect();
}


END_NCBI_SCOPE
