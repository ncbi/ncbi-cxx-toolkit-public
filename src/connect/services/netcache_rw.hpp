#ifndef CONN___NETCACHE_RW__HPP
#define CONN___NETCACHE_RW__HPP

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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *   Net cache client API.
 *
 */

/// @file netcache_rw.hpp
/// NetCache client specs.
///

#include "netservice_api_impl.hpp"
#include "netcache_params.hpp"

#include <connect/ncbi_conn_reader_writer.hpp>

#include <util/transmissionrw.hpp>

#include <limits>

BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */

#if SIZEOF_SIZE_T < 8
inline size_t CheckBlobSize(Uint8 blob_size)
{
    if (blob_size > std::numeric_limits<std::size_t>::max()) {
        NCBI_THROW(CNetCacheException, eBlobClipped, "Blob is too big");
    }
    return (size_t) blob_size;
}
#else
#define CheckBlobSize(blob_size) ((size_t) (blob_size))
#endif


class NCBI_XCONNECT_EXPORT CNetCacheReader : public IReader
{
public:
    CNetCacheReader(SNetCacheAPIImpl* impl,
        const string& blob_id,
        CNetServer::SExecResult& exec_result,
        size_t* blob_size_ptr,
        const CNetCacheAPIParameters* parameters);

    virtual ~CNetCacheReader();

    virtual ERW_Result Read(void* buf, size_t count,
        size_t* bytes_read_ptr = 0);

    virtual ERW_Result PendingCount(size_t* count);

    virtual void Close();

    const string& GetBlobID() const {return m_BlobID;}

    Uint8 GetBlobSize() const {return m_BlobSize;}

    bool Eof() const {return m_BlobBytesToRead == 0;}

private:
    void SocketRead(void* buf, size_t count, size_t* bytes_read);

    string m_BlobID;

    CNetServerConnection m_Connection;
    Uint8 m_BlobSize;
    Uint8 m_BlobBytesToRead; // Remaining number of bytes to be read

    CFileIO m_CacheFile;
    bool m_CachingEnabled;
};

///////////////////////////////////////////////////////////////////////////////
//

enum ENetCacheResponseType {
    eNetCache_Wait,
    eICache_NoWait,
};

class NCBI_XCONNECT_EXPORT CNetCacheWriter : public IEmbeddedStreamWriter
{
public:
    CNetCacheWriter(SNetCacheAPIImpl* impl,
        string* blob_id,
        const string& key,
        ENetCacheResponseType response_type,
        const CNetCacheAPIParameters* parameters);

    virtual ~CNetCacheWriter();

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    /// Flush pending data (if any) down to the output device.
    virtual ERW_Result Flush(void);

    virtual void Close();
    virtual void Abort();

    void WriteBufferAndClose(const char* buf_ptr, size_t buf_size);

    ENetCacheResponseType GetResponseType() const {return m_ResponseType;}

    const string& GetBlobID() const {return m_BlobID;}
    const string& GetKey() const {return m_Key;}

    void SetBlobID(const string& blob_id) {m_BlobID = blob_id;}

private:
    bool IsConnectionOpen() { return m_TransmissionWriter.get() != NULL; }
    void ResetWriters();
    void AbortConnection();
    EIO_Status TransmitImpl(const char* buf, size_t count);
    void Transmit(const void* buf, size_t count, size_t* bytes_written);

    void EstablishConnection();
    void UploadCacheFile();

    CNetServerConnection m_Connection;
    auto_ptr<CSocketReaderWriter> m_SocketReaderWriter;
    auto_ptr<CTransmissionWriter> m_TransmissionWriter;
    ENetCacheResponseType m_ResponseType;

    CNetCacheAPI m_NetCacheAPI;
    string m_BlobID;
    string m_Key; // Only for ICache mode.
    const CNetCacheAPIParameters* m_Parameters;

    CFileIO m_CacheFile;
    bool m_CachingEnabled;
};

/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_RW__HPP */
