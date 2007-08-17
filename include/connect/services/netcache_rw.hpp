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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *   Net cache client API.
 *
 */

/// @file netcache_client.hpp
/// NetCache client specs. 
///

#include <connect/connect_export.h>
#include <corelib/ncbistl.hpp>
#include <corelib/reader_writer.hpp>
#include <util/transmissionrw.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/services/netcache_api.hpp>


BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */


class CNetSrvConnector;
class CSocket;

class NCBI_XCONNECT_EXPORT CNetCacheReader : public IReader
{
public:
    CNetCacheReader(CNetSrvConnector& connector, 
                    bool disconnect, 
                    size_t blob_size);
    virtual ~CNetCacheReader();

    virtual ERW_Result Read(void*   buf,
                                size_t  count,
                            size_t* bytes_read = 0);
    virtual ERW_Result PendingCount(size_t* count);
    
    void Close();

private:
    CNetSrvConnector* m_Connector;
    CSocket* m_Socket;
    bool m_Disconnect;
    auto_ptr<CSocketReaderWriter> m_Reader;
    /// Remaining BLOB size to be read
    size_t              m_BlobBytesToRead;

    CNetCacheReader(const CNetCacheReader&);
    CNetCacheReader& operator= (const CNetCacheReader&);    
};

///////////////////////////////////////////////////////////////////////////////
//

class NCBI_XCONNECT_EXPORT CNetCacheWriter : public IWriter
{
public:
    CNetCacheWriter(CNetCacheAPI& api,
                    CNetSrvConnector& connector, 
                    bool disconnect,
                    CTransmissionWriter::ESendEofPacket send_eof =
                            CTransmissionWriter::eDontSendEofPacket);

    virtual ~CNetCacheWriter();

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);
 
     /// Flush pending data (if any) down to output device.
    virtual ERW_Result Flush(void);

    void Close();

private:
    CNetCacheAPI& m_API;
    CNetSrvConnector* m_Connector;
    CSocket* m_Socket;
    bool m_Disconnect;
    auto_ptr<CTransmissionWriter> m_Writer;
    string m_LastError;

    bool x_IsStreamOk();
    void x_Shutdown(bool disconnect);

    CNetCacheWriter(const CNetCacheWriter&);
    CNetCacheWriter& operator= (const CNetCacheWriter&);    

};


/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_RW__HPP */
