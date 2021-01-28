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

#if !defined(NCBI_OS_MSWIN)  &&  !defined(NCBI_OS_UNIX)
#  error "The CNamedPipe class is supported only on Windows and Unix"
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
///    access to the pipe on the same machine.  The "pipe_name" part of the
///    name can contain any characters other than a backslash, including
///    numbers and special characters.  The entire pipe name string can be up
///    to 256 characters long.  Pipe names are not case sensitive. 
///
/// For UNIXs the pipe name is a generic file name (with or without a path).
///
/// If the pipe name is specified as a base file name only, for example just
/// "pipe_name", without path component, then the CNamedPipe* classes
/// automatically convert it to an OS-specific default full pipe name:
//      \\.\pipe\pipe_name,       (MS Windows)
//      [/var]/tmp/pipe_name,     (UNIX; will use "." as last-resort fallback)
///
/// Initially all timeouts are infinite.
///
/// @sa
///   CNamedPipeClient, CNamedPipeServer, CPipe

class NCBI_XCONNECT_EXPORT CNamedPipe : protected CConnIniter
{
public:
    /// Constructor.
    CNamedPipe(size_t pipe_size = 0/*default*/);

    /// Destructor. 
    virtual ~CNamedPipe();

    /// Close pipe connection.
    ///
    /// The pipe handle becomes invalid after this function call,
    /// regardless of whether the call was successful or not.
    EIO_Status Close(void);
    
    /// Read data from the pipe.
    ///
    /// Always return eIO_Success if some data were read (regardless of pipe
    /// conditions that may include EOF(eIO_Closed)/error(other code)).
    //  Return an error code only if no data at all could be obtained.
    /// Return via "n_read" the number of bytes actually read (which can be
    /// smaller than the requested "count").
    EIO_Status Read(void* buf, size_t count, size_t* n_read = 0);

    /// Write data to the pipe.
    ///
    /// Return eIO_Success if some data were written.
    /// Return other (error) code only if no data at all could be written.
    /// Return via "n_written" the number of bytes actually written (which can
    /// be smaller than the requested "count").
    /// NOTE:
    ///    On MS-Windows client/server should not attempt to write a data
    ///    block, whose size exceeds the pipe buffer size specified at the
    ///    other side of the pipe at the time of creation:  any such block can
    ///    be rejected for writing, and an error may result.
    EIO_Status Write(const void* buf, size_t count, size_t* n_written = 0);


    /// Wait for I/O readiness in the pipe.
    ///
    /// Return eIO_Success if within the specified time, an operation
    /// requested in "event" (which can be either of eIO_Read, eIO_Write, or
    /// eIO_ReadWrite) can be completed without blocking.
    /// Pipe must be in connected state for this method to work;  otherwise
    /// eIO_Unknown results.
    /// Note that non-blocking is not guaranteed for more than one byte of
    /// data (i.e. the following Read or Write may complete with only one
    /// byte read or written, successfully).
    EIO_Status Wait(EIO_Event event, const STimeout* timeout);


    /// Return (for the specified "direction"):
    ///   eIO_Closed     -- if the pipe is closed for I/O;
    ///   eIO_Timeout    -- if the last I/O timed out;
    ///   eIO_Unknown    -- if an error was detected during the last I/O;
    ///   eIO_InvalidArg -- if "direction" is not one of:  eIO_Read, eIO_Write;
    ///   eIO_Success    -- otherwise.
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

    bool IsClientSide(void) const { return  m_IsClientSide; }

    bool IsServerSide(void) const { return !m_IsClientSide; }

    /// Return real named pipe name.
    ///
    /// @sa Open, Create
    const string& GetName(void) const { return m_PipeName; }

protected:
    // Set pipe name (expand it if necessary)
    void x_SetName(const string& pipename);

protected:
    size_t            m_PipeSize;          ///< pipe size
    string            m_PipeName;          ///< pipe name
    bool              m_IsClientSide;      ///< client/server-side pipe
    CNamedPipeHandle* m_NamedPipeHandle;   ///< OS-specific handle

    /// Timeouts
    const STimeout*   m_OpenTimeout;       ///< eIO_Open
    const STimeout*   m_ReadTimeout;       ///< eIO_Read
    const STimeout*   m_WriteTimeout;      ///< eIO_Write

private:
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
    CNamedPipeClient(size_t pipesize = 0/*use default*/);

    /// Constructor.
    ///
    /// This constructor just calls Open().
    /// NOTE: Timeout from the argument becomes new open timeout.
    ///       See CNamedPipe class description about pipe names.
    CNamedPipeClient(const string&   pipename,
                     const STimeout* timeout  = kDefaultTimeout,
                     size_t          pipesize = 0/*use default*/);

    /// Open a client-side pipe connection.
    /// If the server-end does not exist, return eIO_Closed.
    EIO_Status Open(const string&   pipename,
                    const STimeout* timeout  = kDefaultTimeout,
                    size_t          pipesize = 0/*use default*/);
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
    CNamedPipeServer(size_t pipesize = 0/*use default*/);

    /// Constructor.
    ///
    /// This constructor just calls Create().
    /// NOTES:
    ///   - See CNamedPipe class description about pipe names;
    ///   - Timeout from the argument becomes new timeout for listening;
    ///   - The "pipesize" specifies maximum size of data block that can
    ///     be transmitted through the pipe.  The actual buffer size reserved
    ///     for each end of the named pipe is the specified size rounded
    ///     up to the next allocation boundary (usually).
    CNamedPipeServer(const string&   pipename,
                     const STimeout* timeout  = kDefaultTimeout,
                     size_t          pipesize = 0/*use default*/);

    /// Create a server-side pipe.
    EIO_Status Create(const string&   pipename,
                      const STimeout* timeout  = kDefaultTimeout,
                      size_t          pipesize = 0/*use default*/);

    /// Listen on a pipe for new client connection.
    ///
    /// Wait until a new client is connected or open timeout has expired.
    /// Return eIO_Success when a client is connected and I/O may begin.
    /// Return eIO_Timeout, if open timeout had expired before any client
    /// initiated connection.  Any other return code indicates some failure.
    EIO_Status Listen(void);

    /// Disconnect the client.
    ///
    /// Disconnect the server end of a named pipe instance from the client.
    /// Reinitialize the pipe for waiting for the another (next) new client.
    /// Return eIO_Success if client is disconnected and pipe is reset.
    /// Any other return code indicates some failure.
    EIO_Status Disconnect(void);
};


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB__NCBI_NAMEDPIPE__HPP */
