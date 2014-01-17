#ifndef CONNECT___NCBI_NAMEDPIPE__HPP
#define CONNECT___NCBI_NAMEDPIPE__HPP

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
 */

/// @file ncbi_namedpipe.hpp
/// Portable interprocess named pipe API for:  UNIX, MS-Win
///
/// Defines classes: 
///     CNamedPipe        -  base (abstract) class to work with named pipes
///     CNamedPipeClient  -  class for client-side named pipes
///     CNamedPipeServer  -  class for server-side named pipes

#include <connect/ncbi_core_cxx.hpp>

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


/// Forward declaration of OS-specific pipe handle class.
class CNamedPipeHandle;


/////////////////////////////////////////////////////////////////////////////
///
/// CNamedPipe --
///
/// Define base abstract class for interprocess communication via named pipes.
///
/// NOTES: 
///    - On some platforms pipe can be accessed over the network;
///    - Interprocess pipe always opens in byte/binary mode.
/// 
/// For MS Windows the pipe name must have the following form: 
///
//     \\<machine_name>\pipe\<pipe_name>,       (correct syntax)
///    \\\\< machine_name>\\pipe\\< pipe_name>, (doxygen)
///
///    where "machine_name" is a network name of the PC and can be "." for
///    access to the pipe on the same machine. The "pipe_name" part of the
///    name can include any character other than a backslash, including
///    numbers and special characters. The entire pipe name string can be up
///    to 256 characters long. Pipe names are not case sensitive. 
///
/// For UNIXs the pipe name is a generic file name (with or without path).
///
/// If pipe name is specified as base file name, for example "pipe_name",
/// without path, then CNamedPipe* classes automaticaly convert it to
/// OS-specific default pipe name:
//      \\.\pipe\pipe_name,       (MS Windows)
//      /tmp/pipe_name,           (UNIX)
///
/// Initially all timeouts are infinite.
///
/// @sa
///   CNamedPipeClient, CNamedPipeServer, CPipe

class NCBI_XCONNECT_EXPORT CNamedPipe : protected CConnIniter
{
public:
    /// Exposition of default pipe buffer sizes (can be used for "pipebufsize")
    enum {
        eDefaultBufSize    = 4096,      ///< portable default (also as 0)
        eDefaultSysBufSize = kMax_Int   ///< use system-specific default
    };

    /// Constructor.
    CNamedPipe(void);

    /// Destructor. 
    virtual ~CNamedPipe();

    // Implemented in derived specializations (Client/Server).
    virtual EIO_Status Open  (const string&, const STimeout*, size_t) = 0;
    virtual EIO_Status Create(const string&, const STimeout*, size_t) = 0;
         
    /// Close pipe connection.
    ///
    /// The pipe handle becomes invalid after this function call,
    /// regardless of whether the call was successful or not.
    EIO_Status Close(void);
    
    /// Read data from the pipe.
    ///
    /// Always return eIO_Success if some data were read (regardless of pipe
    /// conditions that may include EOF/error).
    //  Return other (error) code only if no data at all could be obtained.
    /// Return in the "n_read" the number of bytes actually read, which may be
    /// less than requested "count" if an error occurs or if the end of
    /// the pipe file stream is encountered before reaching count.
    EIO_Status Read(void* buf, size_t count, size_t* n_read = 0);

    /// Write data to the pipe.
    ///
    /// Return eIO_Success if some data were written.
    /// Return other (error) code only if no data at all could be written.
    /// Return in the "n_written" the number of bytes actually written,
    /// which may be less than "count" if an error occurs or write times out.
    /// NOTE:
    ///    On MS Windows client/server must not attempt to write
    ///    a data block, whose size exceeds the pipe buffer size specified
    ///    on other side of the pipe at the time of creation:  any such
    ///    block will be rejected for writing and an error will result.
    EIO_Status Write(const void* buf, size_t count, size_t* n_written = 0);


    /// Wait for I/O readiness in the pipe.
    ///
    /// Return eIO_Success if within the specified time, an operation
    /// requested in "event" (which can be either of eIO_Read, eIO_Write, or
    /// eIO_ReadWrite) can be completed without blocking.
    /// Pipe must be in connected state for this method to work; otherwise
    /// eIO_Closed results.
    /// Note that non-blocking is not guaranteed for more than one byte of
    /// data (i.e. following Read or Write may complete with only one
    /// byte read or written, successfully).
    EIO_Status Wait(EIO_Event event, const STimeout* timeout);


    /// Return (for the specified "direction"):
    ///   eIO_Closed     -- if the pipe is closed;
    ///   eIO_Unknown    -- if an error was detected during the last I/O;
    ///   eIO_InvalidArg -- if "direction" is not one of:  eIO_Read, eIO_Write;
    ///   eIO_Success    -- otherwise.
    /// Note that eIO_Timeout does not get returned.
    EIO_Status Status(EIO_Event direction) const;

    /// Specify timeout for the pipe I/O (see Open|Read|Write functions).
    ///
    /// If "timeout" is NULL then set the timeout to be infinite.
    /// NOTE: 
    ///    - By default, initially all timeouts are infinite;
    ///    - kDefaultTimeout has no effect.
    EIO_Status SetTimeout(EIO_Event event, const STimeout* timeout);

    /// Get the pipe I/O timeout (or NULL, if the timeout is infinite).
    ///
    /// NOTE: 
    ///    The returned timeout is guaranteed to be pointing to a valid
    ///    (and correct) structure in memory at least until the pipe is
    ///    closed or SetTimeout() is called for this pipe.
    const STimeout* GetTimeout(EIO_Event event) const;

    bool IsClientSide(void) const;
    bool IsServerSide(void) const;

    /// Return real named pipe name.
    ///
    /// @sa Open, Create
    const string& GetName(void) const;

protected:
    // Set pipe name (expand it if necessary)
    void x_SetName(const string& pipename);

protected:
    string            m_PipeName;          ///< pipe name 
    CNamedPipeHandle* m_NamedPipeHandle;   ///< os-specific handle
    size_t            m_PipeBufSize;       ///< pipe buffer size
    bool              m_IsClientSide;      ///< client/server-side pipe

    /// Timeouts
    const STimeout*   m_OpenTimeout;       ///< eIO_Open
    const STimeout*   m_ReadTimeout;       ///< eIO_Read
    const STimeout*   m_WriteTimeout;      ///< eIO_Write
    STimeout          m_OpenTimeoutValue;  ///< storage for m_OpenTimeout
    STimeout          m_ReadTimeoutValue;  ///< storage for m_ReadTimeout
    STimeout          m_WriteTimeoutValue; ///< storage for m_WriteTimeout

private:
    // Disable copy constructor and assignment
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
    ///       See CNamedPipe class description about pipe names.
    CNamedPipeClient(const string&   pipename,
                     const STimeout* timeout     = kDefaultTimeout,
                     size_t          pipebufsize = 0/*use default*/);

    /// Open a client-side pipe connection.
    ///
    /// NOTE: Should not be called if already opened.
    virtual EIO_Status Open(const string&   pipename,
                            const STimeout* timeout     = kDefaultTimeout,
                            size_t          pipebufsize = 0/*use default*/);

    // Always returns eIO_Unknown in this class.
    virtual EIO_Status Create(const string&, const STimeout*, size_t);
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
    /// NOTES:
    ///   - See CNamedPipe class description about pipe names;
    ///   - Timeout from the argument becomes new timeout for a listening;
    ///   - The "pipebufsize" specify a maxium size of data block that can
    ///     be transmitted through the pipe.  The actual buffer size reserved
    ///     for each end of the named pipe is the specified size rounded
    ///     up to the next allocation boundary.
    CNamedPipeServer(const string&   pipename,
                     const STimeout* timeout     = kDefaultTimeout,
                     size_t          pipebufsize = 0/*use default*/);

    /// Create a server-side pipe.
    ///
    /// NOTE: Should not be called if already created.
    virtual EIO_Status Create(const string&   pipename,
                              const STimeout* timeout     = kDefaultTimeout,
                              size_t          pipebufsize = 0/*use default*/);

    /// Always returns eIO_Unknown in this class.
    virtual EIO_Status Open(const string&, const STimeout*, size_t);

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
};


/* @} */


// Inline


inline bool CNamedPipe::IsClientSide(void) const
{
    return m_IsClientSide;
}


inline bool CNamedPipe::IsServerSide(void) const
{
    return !m_IsClientSide;
}


inline const string& CNamedPipe::GetName(void) const
{
    return m_PipeName;
}


END_NCBI_SCOPE

#endif  /* CORELIB__NCBI_NAMEDPIPE__HPP */
