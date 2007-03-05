#ifndef NETBVSTORE__HPP
#define NETBVSTORE__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Network split storage for bit-vectors
 *
 */

#include <util/logrotate.hpp>

#include <connect/threaded_server.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <bdb/bdb_split_blob.hpp>


BEGIN_NCBI_SCOPE


/// NC requests
///
/// @internal
typedef enum {
    eError,
    eGet,
    eShutdown,
    eVersion
} EBVS_RequestType;



/// Parsed BVS request 
///
/// @internal
struct SBVS_Request
{
    EBVS_RequestType req_type;
    string           err_msg;
    string           store_name;

    unsigned         id;
    unsigned         range_from;
    unsigned         range_to;

    void Init() 
    {
        req_type = eError;
        id = range_from = range_to = 0;
    }
};



/// Thread specific data for threaded server
///
/// @internal
///
struct SBVS_ThreadData
{
    char          request[2048];                  ///< request string
    string        auth;                           ///< Authentication string
    SBVS_Request  req;                            ///< parsed NC request
    CBDB_RawFile::TBuffer  buf;                            ///< BLOB buffer
    string        tmp;

    SBVS_ThreadData(size_t size)
        : buf(size)
    {}
};



/// @internal
class CNetBVSLogStream : public CRotatingLogStream
{
public:
    typedef CRotatingLogStream TParent;
public:
    CNetBVSLogStream(const string&    filename, 
                     CNcbiStreamoff   limit)
    : TParent(filename, limit)
    {}
protected:
    virtual string x_BackupName(string& name)
    {
        return kEmptyStr;
    }
};




/// Netcache threaded server 
///
/// @internal
class CNetBVSServer : public CThreadedServer
{
public:
    /// Named local cache instances
    typedef CBDB_BlobSplitStore<bm::bvector<>, CBDB_BlobDeMux, CFastMutex>
                                                                TBlobStore;
    typedef map<string, TBlobStore*>   TBlobStoreMap;

public:
    CNetBVSServer(unsigned int     port,
                  unsigned         max_threads,
                  unsigned         init_threads,
                  unsigned         network_timeout,
                  const IRegistry& reg);

    virtual ~CNetBVSServer();

    void MountBVStore(const string& data_path,
                      const string& storage_name, 
                      unsigned cache_size=0);

    /// Take request code from the socket and process the request
    virtual void Process(SOCK sock);
    virtual bool ShutdownRequested(void) { return m_Shutdown; }

        
    void SetShutdownFlag() { if (!m_Shutdown) m_Shutdown = true; }
    
    /// Override some parent parameters
    virtual void SetParams();

protected:
    virtual void ProcessOverflow(SOCK sock);

    TBlobStore* GetBVS(const string& bvs_name);

    void Process(CSocket&               socket,
                 EBVS_RequestType&      req_type,
                 SBVS_Request&          req,
                 SBVS_ThreadData&       tdata,
                 TBlobStore*            bvs);

    /// Process "GET" request
    void ProcessGet(CSocket&               sock,
                    const SBVS_Request&    req,
                    SBVS_ThreadData&       tdata,
                    TBlobStore*            bvs);

    /// Process "VERSION" request
    void ProcessVersion(CSocket& sock, const SBVS_Request& req);

    /// Process "SHUTDOWN" request
    void ProcessShutdown();

private:

    /// Returns FALSE when socket is closed or cannot be read
    bool ReadStr(CSocket& sock, string* str);


    void ParseRequest(const char* reqstr, SBVS_Request* req);

    /// Reply to the client
    void WriteMsg(CSocket& sock, 
                  const string& prefix, const string& msg);

    static
    bool WaitForReadSocket(CSocket& sock, unsigned time_to_wait);

private:

    void x_WriteBuf(CSocket& sock,
                    const CBDB_RawFile::TBuffer::value_type* buf,
                    size_t                                   bytes);

    /// Check if we have active thread data for this thread.
    /// Setup thread data if we don't.
    SBVS_ThreadData* x_GetThreadData(); 


private:
    /// Host name where server runs
    string             m_Host;
    TBlobStoreMap      m_BVS_Map;
    CFastMutex         m_BVS_Map_Lock;
    /// Flags that server received a shutdown request
    volatile bool      m_Shutdown; 
    /// Time to wait for the client (seconds)
    unsigned           m_InactivityTimeout;
    /// Accept timeout for threaded server
    STimeout                     m_ThrdSrvAcceptTimeout;
    /// Configuration
    const IRegistry&             m_Reg;
};



END_NCBI_SCOPE

#endif
