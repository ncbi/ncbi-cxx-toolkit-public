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
#include <connect/ncbi_namedpipe.hpp>
#include <connect/error_codes.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <assert.h>

#if defined(NCBI_OS_MSWIN)

#elif defined(NCBI_OS_UNIX)
#  include <errno.h>
#  include <unistd.h>
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <connect/ncbi_socket_unix.h>
#else
#  error "Class CNamedPipe is supported only on Windows and Unix"
#endif


#define NCBI_USE_ERRCODE_X   Connect_Pipe


BEGIN_NCBI_SCOPE


#if defined(HAVE_SOCKLEN_T)  ||  defined(_SOCKLEN_T)
typedef socklen_t  SOCK_socklen_t;
#else
typedef int        SOCK_socklen_t;
#endif /*HAVE_SOCKLEN_T || _SOCKLEN_T*/


// Predefined timeouts
const size_t kDefaultPipeBufSize = (size_t)CNamedPipe::eDefaultBufSize;


//////////////////////////////////////////////////////////////////////////////
//
// Auxiliary functions
//

static const STimeout* s_SetTimeout(const STimeout* from, STimeout* to)
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
    return "[NamedPipe::" + where + "]  " + what;
}


inline void s_AdjustPipeBufSize(size_t& bufsize)
{
    if (!bufsize) {
        bufsize = kDefaultPipeBufSize;
    } else if (bufsize == (size_t)kMax_Int) {
        bufsize = 0 /* use system default buffer size */;
    }
}



//////////////////////////////////////////////////////////////////////////////
//
// Class CNamedPipeHandle handles forwarded requests from CNamedPipe.
// This class is reimplemented in a platform-specific fashion where needed.
//

#if defined(NCBI_OS_MSWIN)

const DWORD kSleepTime = 100;  // sleep time for timeouts (milliseconds)


static string s_WinError(DWORD error, string& message)
{
    char* errstr = NULL;
	DWORD rv = ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                               FORMAT_MESSAGE_FROM_SYSTEM     |
                               FORMAT_MESSAGE_MAX_WIDTH_MASK  |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL, error, 0, (LPTSTR) &errstr, 0, NULL);
	if (!rv  &&  errstr) {
		::LocalFree(errstr);
		errstr = NULL;
	}
    int dynamic = 0/*false*/;
    const char* result = ::NcbiMessagePlusError(&dynamic, message.c_str(),
                                                (int) error, errstr);
    if (errstr) {
        ::LocalFree(errstr);
    }
    string retval;
    if (result) {
        retval.assign(result);
        if (dynamic) {
            free(result);
        }
    } else {
        retval.swap(message);
    }
    return retval;
}


//////////////////////////////////////////////////////////////////////////////
//
// CNamedPipeHandle -- MS Windows version
//

class CNamedPipeHandle
{
public:
    CNamedPipeHandle();
    ~CNamedPipeHandle();

    // client-side

    EIO_Status Open(const string& pipename, const STimeout* timeout,
                    size_t pipebufsize);

    // server-side

    EIO_Status Create(const string& pipename, size_t pipebufsize);
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
    EIO_Status x_Disconnect(bool reconnect = false);

    HANDLE      m_Pipe;         // pipe I/O handle
    string      m_PipeName;     // pipe name 
    size_t      m_PipeBufSize;  // pipe buffer size
    EIO_Status  m_ReadStatus;   // last read status
    EIO_Status  m_WriteStatus;  // last write status
};


CNamedPipeHandle::CNamedPipeHandle()
    : m_Pipe(INVALID_HANDLE_VALUE), m_PipeName(kEmptyStr),
      m_PipeBufSize(0),
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
                                  size_t          /* pipebufsize */)
{
    try {
        if (m_Pipe != INVALID_HANDLE_VALUE) {
            throw string("Named pipe already open");
        }
        // Save parameters
        m_PipeName    = pipename;
        m_PipeBufSize = 0 /* pipebufsize is not used on client side */;

        // Set the base security attributes
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.bInheritHandle = TRUE;
        attr.lpSecurityDescriptor = NULL;

        // Wait until either a time-out interval elapses or an instance of
        // the specified named pipe is available for connection (that is, the
        // pipe's server process has a pending Listen() operation on the pipe).

        // NOTE:  We do not use here WaitNamedPipe() because it works
        //        incorrectly in some cases.

        DWORD x_timeout = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;

        do {
            // Open existing pipe
            m_Pipe = ::CreateFile
                (m_PipeName.c_str(),
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
    catch (string& what) {
        ERR_POST_X(10, s_FormatErrorMessage("Open", what));
    }

    return eIO_Unknown;
}


EIO_Status CNamedPipeHandle::Create(const string& pipename,
                                    size_t        pipebufsize)
{
    try {
        if (m_Pipe != INVALID_HANDLE_VALUE) {
            throw string("Named pipe already open");
        }
        // Save parameters
        m_PipeName    = pipename;
        m_PipeBufSize = pipebufsize;

        // Set the base security attributes
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.bInheritHandle = TRUE;
        attr.lpSecurityDescriptor = NULL;

        // Create pipe
        m_Pipe = ::CreateNamedPipe
            (m_PipeName.c_str(),            // pipe name 
             PIPE_ACCESS_DUPLEX,            //  read/write access 
             PIPE_TYPE_BYTE | PIPE_NOWAIT,  // byte-type, nonblocking mode 
             1,                             // one instance only 
             pipebufsize,                   // output buffer size 
             pipebufsize,                   // input buffer size 
             INFINITE,                      // client time-out by default
             &attr);                        // security attributes

        if (m_Pipe == INVALID_HANDLE_VALUE) {
            DWORD error = ::GetLastError();
            string message("CreateNamedPipe(\"" + m_PipeName + "\") failed");
            throw s_WinError(error, message),
        }

        m_ReadStatus  = eIO_Success;
        m_WriteStatus = eIO_Success;
        return eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(11, s_FormatErrorMessage("Create", what));
    }

    return eIO_Unknown;
}


EIO_Status CNamedPipeHandle::Listen(const STimeout* timeout)
{
    EIO_Status status = eIO_Unknown;

    // Wait for the client to connect, or time out.
    // NOTE:  WaitForSingleObject() does not work with pipes.
    DWORD x_timeout = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;

    try {
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw string("Named pipe closed");
        }
        do {
            BOOL connected = ::ConnectNamedPipe(m_Pipe, NULL);
            if ( !connected ) {
                DWORD error = ::GetLastError();
                connected = error == ERROR_PIPE_CONNECTED ? TRUE : FALSE;
                // If client closed connection before we serve it
                if (error == ERROR_NO_DATA) {
                    if (x_Disconnect(true) != eIO_Success) {
                        throw string("Failed to close broken client session");
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
    catch (string& what) {
        ERR_POST_X(12, s_FormatErrorMessage("Listen", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::x_Disconnect(bool reconnect)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pipe == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw string("Named pipe already closed");
        }
        ::FlushFileBuffers(m_Pipe); 
        if (!::DisconnectNamedPipe(m_Pipe)) {
            DWORD error = ::GetLastError();
            string message("DisconnectNamedPipe() failed");
            throw s_WinError(error, message);
        } 
        Close();
        return reconnect ? Create(m_PipeName, m_PipeBufSize) : eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(13, s_FormatErrorMessage("Disconnect", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::Disconnect(void)
{
    return x_Disconnect();
}


EIO_Status CNamedPipeHandle::Close(void)
{
    if (m_Pipe == INVALID_HANDLE_VALUE) {
        return eIO_Closed;
    }
    ::FlushFileBuffers(m_Pipe);
    ::CloseHandle(m_Pipe);
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
            throw string("Named pipe closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout   = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;
        DWORD bytes_avail = 0;

        // Wait for data from the pipe with timeout.
        // NOTE:  WaitForSingleObject() does not work with pipes.

        do {
            if ( !::PeekNamedPipe(m_Pipe, NULL, 0, NULL, &bytes_avail, NULL) ){
                // Peer has been closed the connection?
                if (::GetLastError() == ERROR_BROKEN_PIPE) {
                    m_ReadStatus = eIO_Closed;
                    return m_ReadStatus;
                }
                throw string("Cannot peek data from named pipe");
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
        // We must read only "count" bytes of data regardless of the amount
        // available to read.
        if (bytes_avail > count) {
            bytes_avail = count;
        }
        if ( !::ReadFile(m_Pipe, buf, count, &bytes_avail, NULL) ) {
            throw string("Failed to read data from named pipe");
        }
        if ( n_read ) {
            *n_read = bytes_avail;
        }
        status = eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(14, s_FormatErrorMessage("Read", what));
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
            throw string("Named pipe closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout     = timeout ? NcbiTimeoutToMs(timeout) : INFINITE;
        DWORD bytes_written = 0;

        // Wait for data from the pipe with timeout.
        // NOTE:  WaitForSingleObject() does not work with pipes.

        do {
            if (!::WriteFile(m_Pipe, (char*)buf, count, &bytes_written, NULL)){
                if ( n_written ) {
                    *n_written = bytes_written;
                }
                throw string("Failed to write data into named pipe");
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
        ERR_POST_X(15, s_FormatErrorMessage("Write", what));
    }

    m_WriteStatus = status;
    return status;
}


EIO_Status CNamedPipeHandle::Wait(EIO_Event, const STimeout*)
{
    return eIO_Success;
}


EIO_Status CNamedPipeHandle::Status(EIO_Event direction) const
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
    CNamedPipeHandle();
    ~CNamedPipeHandle();

    // client-side

    EIO_Status Open(const string& pipename,
                    const STimeout* timeout, size_t pipebufsize);

    // server-side

    EIO_Status Create(const string& pipename, size_t pipebufsize);
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
    // Set socket i/o buffer size (dir: SO_SNDBUF, SO_RCVBUF)
    bool x_SetSocketBufSize(int sock, size_t pipebufsize, int dir);
    // Disconnect implementation
    EIO_Status x_Disconnect(void);

private:
    LSOCK   m_LSocket;      // listening socket
    SOCK    m_IoSocket;     // I/O socket
    size_t  m_PipeBufSize;  // pipe buffer size
};


CNamedPipeHandle::CNamedPipeHandle()
    : m_LSocket(0), m_IoSocket(0), m_PipeBufSize(0)
{
    return;
}


CNamedPipeHandle::~CNamedPipeHandle()
{
    Close();
}


EIO_Status CNamedPipeHandle::Open(const string&   pipename,
                                  const STimeout* timeout,
                                  size_t          pipebufsize)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_LSocket  ||  m_IoSocket) {
            throw string("Named pipe already open");
        }

        status = SOCK_CreateUNIX(pipename.c_str(), timeout, &m_IoSocket,
                                 NULL, 0, 0/*flags*/);
        if (status != eIO_Success) {
            throw string("Named pipe SOCK_CreateUNIX() failed: ")
                + IO_StatusStr(status);
        }

        m_PipeBufSize = pipebufsize;

        // Set buffer size
        if (m_PipeBufSize) {
            int fd;
            if (SOCK_GetOSHandle(m_IoSocket, &fd, sizeof(fd)) == eIO_Success) {
                if (!x_SetSocketBufSize(fd, m_PipeBufSize, SO_SNDBUF)  ||
                    !x_SetSocketBufSize(fd, m_PipeBufSize, SO_RCVBUF)) {
                    int error = errno;
                    throw string("UNIX socket set buffer size failed: ")
                        + strerror(error);
                }
            }
        }

        return eIO_Success;
    }
    catch (string& what) {
        ERR_POST_X(10, s_FormatErrorMessage("Open", what));
    }

    return status;
}


EIO_Status CNamedPipeHandle::Create(const string& pipename,
                                    size_t        pipebufsize)
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_LSocket  ||  m_IoSocket) {
            throw string("Named pipe already open");
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
            status = eIO_Closed;
            throw "Named pipe path \"" + pipename + "\" already exists";
        }

        status = LSOCK_CreateUNIX(pipename.c_str(),
                                  kListenQueueSize,
                                  &m_LSocket, 0);
        if (status != eIO_Success) {
            throw string("Named pipe LSOCK_CreateUNIX() failed: ")
                + IO_StatusStr(status);
        }

        m_PipeBufSize = pipebufsize;

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
            throw string("Named pipe not listening");
        }

        status = LSOCK_Accept(m_LSocket, timeout, &m_IoSocket);
        if (status != eIO_Success) {
            throw string("Named pipe LSOCK_Accept() failed: ")
                + IO_StatusStr(status);
        }
        // Set buffer size
        if (m_PipeBufSize) {
            int fd;
            if (SOCK_GetOSHandle(m_IoSocket, &fd, sizeof(fd)) == eIO_Success) {
                if (!x_SetSocketBufSize(fd, m_PipeBufSize, SO_SNDBUF)  ||
                    !x_SetSocketBufSize(fd, m_PipeBufSize, SO_RCVBUF)) {
                    int error = errno;
                    throw string("UNIX socket set buffer size failed: ")
                        + strerror(error);
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


EIO_Status CNamedPipeHandle::x_Disconnect(void)
{
    // Close I/O socket
    _ASSERT(m_IoSocket);
    EIO_Status status = SOCK_Close(m_IoSocket);
    m_IoSocket = 0;
    return status;
}


EIO_Status CNamedPipeHandle::Disconnect(void)
{
    if ( !m_IoSocket ) {
        ERR_POST_X(13, s_FormatErrorMessage("Disconnect",
                                            "Named pipe already closed"));
        return eIO_Closed;
    }
    return x_Disconnect();
}


EIO_Status CNamedPipeHandle::Close(void)
{
    // Disconnect current client
    EIO_Status status = m_IoSocket ? x_Disconnect() : eIO_Closed;

    // Close listening socket
    if (m_LSocket) {
        LSOCK_Close(m_LSocket);
        m_LSocket = 0;
    }
    return status;
}


EIO_Status CNamedPipeHandle::Read(void* buf, size_t count, size_t* n_read,
                                  const STimeout* timeout)
{
    EIO_Status status = eIO_Closed;

    try {
        if ( !m_IoSocket ) {
            throw string("Named pipe closed");
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
    catch (string& what) {
        ERR_POST_X(14, s_FormatErrorMessage("Read", what));
    }
    return status;
}


EIO_Status CNamedPipeHandle::Write(const void* buf, size_t count,
                                   size_t* n_written, const STimeout* timeout)

{
    EIO_Status status = eIO_Closed;

    try {
        if ( !m_IoSocket ) {
            throw string("Named pipe closed");
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
    catch (string& what) {
        ERR_POST_X(15, s_FormatErrorMessage("Write", what));
    }
    return status;
}


EIO_Status CNamedPipeHandle::Wait(EIO_Event event, const STimeout* timeout)
{
    if ( m_IoSocket ) {
        return SOCK_Wait(m_IoSocket, event, timeout);
    }
    ERR_POST_X(16, s_FormatErrorMessage("Wait", "Named pipe closed"));
    return eIO_Closed;
}


EIO_Status CNamedPipeHandle::Status(EIO_Event direction) const
{
    if ( !m_IoSocket ) {
        return eIO_Closed;
    }
    return SOCK_Status(m_IoSocket, direction);
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


CNamedPipe::CNamedPipe()
    : m_PipeName(kEmptyStr), m_PipeBufSize(kDefaultPipeBufSize),
      m_OpenTimeout(0), m_ReadTimeout(0), m_WriteTimeout(0)
{
    m_NamedPipeHandle = new CNamedPipeHandle;
}


CNamedPipe::~CNamedPipe()
{
    Close();
    delete m_NamedPipeHandle;
#ifdef NCBI_OS_UNIX
    if ( IsServerSide()  &&  !m_PipeName.empty() ) {
        unlink(m_PipeName.c_str());
    }
#endif /*NCBI_OS_UNIX*/
}


EIO_Status CNamedPipe::Close(void)
{
    return m_NamedPipeHandle ? m_NamedPipeHandle->Close() : eIO_Unknown;
}
     

EIO_Status CNamedPipe::Read(void* buf, size_t count, size_t* n_read)
{
    if ( n_read ) {
        *n_read = 0;
    }
    if (count  &&  !buf) {
        return eIO_InvalidArg;
    }
    return m_NamedPipeHandle
        ? m_NamedPipeHandle->Read(buf, count, n_read, m_ReadTimeout)
        : eIO_Unknown;
}


EIO_Status CNamedPipe::Write(const void* buf, size_t count, size_t* n_written)
{
    if ( n_written ) {
        *n_written = 0;
    }
    if (count  &&  !buf) {
        return eIO_InvalidArg;
    }
    return m_NamedPipeHandle
        ? m_NamedPipeHandle->Write(buf, count, n_written, m_WriteTimeout)
        : eIO_Unknown;
}


EIO_Status CNamedPipe::Wait(EIO_Event event, const STimeout* timeout)
{
    switch (event) {
    case eIO_Read:
    case eIO_Write:
    case eIO_ReadWrite:
        break;
    default:
        return eIO_InvalidArg;
    }
    return m_NamedPipeHandle
        ? m_NamedPipeHandle->Wait(event, timeout)
        : eIO_Unknown;
}


EIO_Status CNamedPipe::Status(EIO_Event direction) const
{
    switch (direction) {
    case eIO_Read:
    case eIO_Write:
        break;
    default:
        return eIO_InvalidArg;
    }
    return m_NamedPipeHandle
        ? m_NamedPipeHandle->Status(direction)
        : eIO_Closed;
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
        break;
    }
    return kDefaultTimeout;
}


void CNamedPipe::x_SetName(const string& pipename)
{
#ifdef NCBI_OS_MSWIN
    static const char separators[] = ":/\\";
#else
    static const char separators[] = "/";
#endif
    if (pipename.find_first_of(separators) != NPOS) {
        m_PipeName = pipename;
        return;
    }

#if defined(NCBI_OS_MSWIN)
    m_PipeName = "\\\\.\\pipe\\" + pipename;
#elif defined(NCBI_OS_UNIX)
    static const mode_t k_writeable = S_IWUSR | S_IWGRP | S_IWOTH;
    struct stat st;

    const char* pipedir = "/var/tmp";
    if (::stat(pipedir, &st) != 0  ||  !S_ISDIR(st.st_mode)
        ||  (st.st_mode & k_writeable) != k_writeable) {
        pipedir = "/tmp";
        if (::stat(pipedir, &st) != 0  ||  !S_ISDIR(st.st_mode)
            ||  (st.st_mode & k_writeable) != k_writeable) {
            pipedir = ".";
        }
    }
    m_PipeName = string(pipedir) + "/" + pipename;
#else
    m_PipeName = pipename;
#endif
}



//////////////////////////////////////////////////////////////////////////////
//
// CNamedPipeClient
//

CNamedPipeClient::CNamedPipeClient()
{
    m_IsClientSide = true;
}


CNamedPipeClient::CNamedPipeClient(const string&   pipename,
                                   const STimeout* timeout, 
                                   size_t          pipebufsize)
{
    m_IsClientSide = true;
    Open(pipename, timeout, pipebufsize);
}


EIO_Status CNamedPipeClient::Open(const string&    pipename,
                                  const STimeout*  timeout,
                                  size_t           pipebufsize)
{
    if ( !m_NamedPipeHandle ) {
        return eIO_Unknown;
    }
    s_AdjustPipeBufSize(pipebufsize);
    m_PipeBufSize = pipebufsize;
    x_SetName(pipename);

    SetTimeout(eIO_Open, timeout);
    return m_NamedPipeHandle->Open(m_PipeName, m_OpenTimeout, m_PipeBufSize);
}


EIO_Status CNamedPipeClient::Create(const string&, const STimeout*, size_t)
{
    return eIO_InvalidArg;
}



//////////////////////////////////////////////////////////////////////////////
//
// CNamedPipeServer
//


CNamedPipeServer::CNamedPipeServer()
{
    m_IsClientSide = false;
}


CNamedPipeServer::CNamedPipeServer(const string&   pipename,
                                   const STimeout* timeout,
                                   size_t          pipebufsize)
{
    m_IsClientSide = false;
    Create(pipename, timeout, pipebufsize);
}


EIO_Status CNamedPipeServer::Create(const string&   pipename,
                                    const STimeout* timeout,
                                    size_t          pipebufsize)
{
    if ( !m_NamedPipeHandle ) {
        return eIO_Unknown;
    }
    s_AdjustPipeBufSize(pipebufsize);
    m_PipeBufSize = pipebufsize;
    x_SetName(pipename);

    SetTimeout(eIO_Open, timeout);
    return m_NamedPipeHandle->Create(m_PipeName, pipebufsize);
}


EIO_Status CNamedPipeServer::Open(const string&, const STimeout*, size_t)
{
    return eIO_InvalidArg;
}


EIO_Status CNamedPipeServer::Listen(void)
{
    return m_NamedPipeHandle
        ? m_NamedPipeHandle->Listen(m_OpenTimeout)
        : eIO_Unknown;
}


EIO_Status CNamedPipeServer::Disconnect(void)
{
    return m_NamedPipeHandle
        ? m_NamedPipeHandle->Disconnect()
        : eIO_Unknown;
}


END_NCBI_SCOPE
