#ifndef CONNECT___NCBI_NAMEDPIPE__HPP
#define CONNECT___NCBI_NAMEDPIPE__HPP

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

/// @file ncbi_namedpipe.hpp
/// Portable interprocess named pipe API for:  UNIX, MS-Win
///
/// Defines classes: 
///     CNamedPipe        -  base class for work with named pipes
///     CNamedPipeClient  -  class for client-side named pipes
///     CNamedPipeServer  -  class for server-side named pipes


#include <connect/ncbi_core.h>
#include <corelib/ncbistd.hpp>
#include <vector>

#if defined(NCBI_OS_MSWIN)
#elif defined(NCBI_OS_UNIX)
#else
#  error "Class CNamedPipe is supported only on Windows and Unix"
#endif


/** @addtogroup Pipes
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Forward declaraton for os-specific pipe handle class.
class CNamedPipeHandle;


/////////////////////////////////////////////////////////////////////////////
///
/// CNamedPipe --
///
/// Define base class for interprocess communication via named pipes.
///
/// NOTES: - On some platforms pipe can be accessed over the network;
///        - Interprocess pipe always opens in byte/binary mode.
/// 
/// For MS Windows the pipe name must have the following form: 
///
///    \\<machine_name>\pipe\<pipename>,
///
///    where <machine_name> is a network name of the PC and can be "." for
///    access to the pipe on the same machine. The <pipename> part of the
///    name can include any character other than a backslash, including
///    numbers and special characters. The entire pipe name string can be up
///    to 256 characters long. Pipe names are not case sensitive. 
///
/// For UNIXs the pipe name is a generic file name (with or without path).
///
/// @sa
///   CNamedPipeClient, CNamedPipeServer, CPipe

class NCBI_XCONNECT_EXPORT CNamedPipe
{
public:
    /// Special values for timeouts as accepted by member functions.
    /// Initially, all timeouts are infinite.
    static const STimeout* const kDefaultTimeout;    ///< use value last set
    static const STimeout* const kInfiniteTimeout;   ///< ad infinitum
    static const STimeout* const kNoWait;            ///< zero timeout

    /// Default I/O buffer size
    static const size_t          kDefaultBufferSize; ///< default I/O buf.size

    /// Constructor.
    CNamedPipe(void);

    /// Destructor. 
    ~CNamedPipe(void);

    /// Close pipe connection.
    EIO_Status Close(void);
    
    /// Read data from the pipe.
    ///
    /// Return in the "read" the number of bytes actually read, which may be
    /// less than requested "count" if an error occurs or if the end of
    /// the pipe file stream is encountered before reaching count.
    EIO_Status Read(void* buf, size_t count, size_t* read = 0);

    /// Writes data to the pipe.
    ///
    /// Returns  in the "written" the number of bytes actually written,
    /// which may be less than  "count" if an error occurs or times out.
    /// NOTE: On MS Windows client/server do not must to write into a pipe
    ///       at a heat a data block with size more than size of pipe's
    ///       buffer specified on the server side using
    ///       CNamedPipeServer::Create().Such data block will be never written.
    EIO_Status Write(const void* buf, size_t count, size_t* written = 0);

    /// Return (for the specified "direction"):
    ///   eIO_Closed     -- if the pipe is closed;
    ///   eIO_Unknown    -- if an error was detected during the last I/O;
    ///   eIO_InvalidArg -- if "direction" is not one of:  eIO_Read, eIO_Write;
    ///   eIO_Timeout    -- if the timeout was on last I/O;
    ///   eIO_Success    -- otherwise.
    EIO_Status Status(EIO_Event direction);

    /// Specify timeout for the pipe I/O (see Open|Read|Write functions).
    ///
    /// If "timeout" is NULL then set the timeout to be infinite.
    /// NOTE: By default, initially all timeouts are infinite;
    /// NOTE: kDefaultTimeout has no effect.
    EIO_Status SetTimeout(EIO_Event event, const STimeout* timeout);

    /// Get the pipe I/O timeout (or NULL, if the timeout is infinite).
    ///
    /// NOTE: The returned timeout is guaranteed to be pointing to a valid
    ///       (and correct) structure in memory at least until the pipe is
    ///       closed or SetTimeout() is called for this pipe.
    const STimeout* GetTimeout(EIO_Event event) const;

    bool IsClientSide(void) const;
    bool IsServerSide(void) const;

protected:
    string            m_PipeName;          ///< pipe name 
    CNamedPipeHandle* m_PipeHandle;        ///< os-specific handle
    bool              m_IsClientSide;      ///< client/server-side pipe
    size_t            m_Bufsize;           ///< IO buffer size

    /// Last IO status
    EIO_Status        m_ReadStatus;        ///< read status
    EIO_Status        m_WriteStatus;       ///< write status

    /// Timeouts
    STimeout*         m_OpenTimeout;       ///< eIO_Open
    STimeout*         m_ReadTimeout;       ///< eIO_Read
    STimeout*         m_WriteTimeout;      ///< eIO_Write
    STimeout          m_OpenTimeoutValue;  ///< storage for m_OpenTimeout
    STimeout          m_ReadTimeoutValue;  ///< storage for m_ReadTimeout
    STimeout          m_WriteTimeoutValue; ///< storage for m_WriteTimeout

    /// Disable copy constructor and assignment.
    CNamedPipe(const CNamedPipe&);
    CNamedPipe& operator= (const CNamedPipe&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CNamedPipeClient  --
///
/// Client-side named pipes
///
/// @sa
///   CNamedPipe, CNamedPipeServer


class NCBI_XCONNECT_EXPORT CNamedPipeClient : public CNamedPipe
{
public:
    /// Default constructor.
    CNamedPipeClient(void);

    /// Constructor.
    ///
    /// This constructor just calls Open().
    /// NOTE: Timeout from the argument becomes new open timeout.
    CNamedPipeClient
        (const string&   pipename,
         const STimeout* timeout = kDefaultTimeout,
         size_t          bufsize = kDefaultBufferSize
         );

    /// Open a client-side pipe connection.
    ///
    /// NOTE: Should not be called if already opened.
    EIO_Status Open
        (const string&   pipename,
         const STimeout* timeout = kDefaultTimeout,
         size_t          bufsize = kDefaultBufferSize
         );

private:
    /// Disable copy constructor and assignment.
    CNamedPipeClient(const CNamedPipeClient&);
    CNamedPipeClient& operator= (const CNamedPipeClient&);
};
 


/////////////////////////////////////////////////////////////////////////////
///
/// CNamedPipeServer --
///
/// Server-side named pipes
///
/// @sa
///   CNamedPipe, CNamedPipeClient

class NCBI_XCONNECT_EXPORT CNamedPipeServer : public CNamedPipe
{
public:
    /// Default constructor.
    CNamedPipeServer(void);

    /// Constructor.
    ///
    /// This constructor just calls Create().
    /// NOTE: Timeout from the argument becomes new timeout for a listening.
    /// NOTE: The "bufsize" specify a maxium size of data block that can be
    ///       transmitted through the pipe. The actual buffer size reserved
    ///       for each end of the named pipe is the specified size rounded
    ///       up to the next allocation boundary.
    CNamedPipeServer
        (const string&   pipename,
         const STimeout* timeout  = kDefaultTimeout,
         size_t          bufsize  = kDefaultBufferSize
         );

    /// Create a server-side pipe
    ///.
    /// NOTE: Should not be called if already created.
    EIO_Status Create
        (const string&   pipename,
         const STimeout* timeout  = kDefaultTimeout,
         size_t          bufsize  = kDefaultBufferSize
         );

    /// Listen a pipe for new client connection.
    ///
    /// Wait until new client will be connected or open timeout has been
    /// expired.
    /// Return eIO_Success when client is connected.
    /// Return eIO_Timeout, if open timeout expired before any client
    /// initiate connection. Any other return code indicates some failure.
    EIO_Status Listen(void);

    /// Disconnect a connected client.
    ///
    /// Disconnect the server end of a named pipe instance from a client
    /// process. Reinitialize the pipe for waiting a new client.
    /// Return eIO_Success if client is disconnected and pipe is reinitialized.
    /// Any other return code indicates some failure.
    EIO_Status Disconnect(void);

private:
    /// Disable copy constructor and assignment.
    CNamedPipeServer(const CNamedPipeServer&);
    CNamedPipeServer& operator= (const CNamedPipeServer&);
};


/* @} */


// Inline (getters)

inline bool CNamedPipe::IsClientSide(void) const
{
    return m_IsClientSide;
}

inline bool CNamedPipe::IsServerSide(void) const
{
    return !m_IsClientSide;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/08/20 19:01:05  ivanov
 * Fixed cvs log
 *
 * ===========================================================================
 */

#endif  /* CORELIB__NCBI_NAMEDPIPE__HPP */
