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
 * File Description: Network cache daemon
 *
 */
#include <ncbi_pch.hpp>
#include <stdio.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbimtx.hpp>

#include <util/thread_nonstop.hpp>
#include <util/bitset/ncbi_bitset.hpp>

#include <util/cache/icache.hpp>
#include <util/cache/icache_clean_thread.hpp>
#include <util/logrotate.hpp>
#include <util/transmissionrw.hpp>

#include <bdb/bdb_blobcache.hpp>

#include <connect/threaded_server.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/services/netcache_client.hpp>

#if defined(NCBI_OS_UNIX)
# include <signal.h>
#endif

#define NETCACHED_VERSION \
      "NCBI NetCache server version=1.3.0  " __DATE__ " " __TIME__


USING_NCBI_SCOPE;

//const unsigned int kNetCacheBufSize = 64 * 1024;
const unsigned int kObjectTimeout = 60 * 60; ///< Default timeout in seconds

/// General purpose NetCache Mutex
DEFINE_STATIC_FAST_MUTEX(x_NetCacheMutex);

/// Mutex to guard vector of busy IDs 
DEFINE_STATIC_FAST_MUTEX(x_NetCacheMutex_ID);






/// Class guards the BLOB id, guarantees exclusive access to the object
///
/// @internal
class CIdBusyGuard
{
public:
    CIdBusyGuard(bm::bvector<>* id_set)
        : m_IdSet(id_set), m_Id(0)
    {}

    CIdBusyGuard(bm::bvector<>* id_set, 
                 unsigned int   id,
                 unsigned       timeout)
        : m_IdSet(id_set)
    {
        Lock(id, timeout);
    }

    void Lock(unsigned int   id,
              unsigned       timeout)
    {
        _ASSERT(id);
        unsigned cnt = 0; unsigned sleep_ms = 10;
        while (true) {
            {{
            CFastMutexGuard guard(x_NetCacheMutex_ID);
            if (!(*m_IdSet)[id]) {
                m_IdSet->set(id);
                break;
            }
            }}
            cnt += sleep_ms;
            if (cnt > timeout * 1000) {
                NCBI_THROW(CNetServiceException, 
                           eTimeout, "Failed to lock BLOB object");
            }
            SleepMilliSec(sleep_ms);
        } // while
        m_Id = id;
    }

    void Lock(const string& blob_key, unsigned timeout)
    {
        unsigned cnt = 0; unsigned sleep_ms = 10;
        unsigned id = 0;
        while (true) {
            {{
            CFastMutexGuard guard(x_NetCacheMutex_ID);

            if (id == 0) {
                id = CNetCache_GetBlobId(blob_key);
            }

            if ((*m_IdSet)[id] == false) {
                m_IdSet->set(id);
                break;
            }
            }}
            cnt += sleep_ms;
            if (cnt > timeout * 1000) {
                NCBI_THROW(CNetServiceException, 
                           eTimeout, "Failed to lock BLOB object");
            }
            SleepMilliSec(sleep_ms);
        } // while
        m_Id = id;
    }

    void LockNewId(unsigned*  max_id, unsigned timeout)
    {
        unsigned cnt = 0; unsigned sleep_ms = 10;
        unsigned id = 0;
        while (true) {
            {{
            CFastMutexGuard guard(x_NetCacheMutex_ID);

            if (id == 0) {
                id = ++(*max_id);
            }

            if (!(*m_IdSet)[id]) {
                m_IdSet->set(id);
                break;
            }
            }}
            cnt += sleep_ms;
            if (cnt > timeout * 1000) {
                NCBI_THROW(CNetServiceException, 
                           eTimeout, "Failed to lock BLOB object");
            }
            SleepMilliSec(sleep_ms);
        } // while
        m_Id = id;
    }


    ~CIdBusyGuard()
    {
        Release();
    }

    void Release()
    {
        if (m_Id) {
            CFastMutexGuard guard(x_NetCacheMutex_ID);
            m_IdSet->set(m_Id, false);
            m_Id = 0;
        }
    }

    unsigned GetId() const { return m_Id; }

private:
    CIdBusyGuard(const CIdBusyGuard&);
    CIdBusyGuard& operator=(const CIdBusyGuard&);
private:
    bm::bvector<>*   m_IdSet;
    unsigned int     m_Id;
};


/// @internal
///
/// Netcache server side request statistics
///
struct NetCache_RequestStat
{
    CTime        conn_time;    ///< request incoming time in seconds
    unsigned     req_code;     ///< 'P' put, 'G' get
    size_t       blob_size;    ///< BLOB size
    double       elapsed;      ///< total time in seconds to process request
    double       comm_elapsed; ///< time spent reading/sending data
    string       peer_address; ///< Client's IP address
};

/// @internal
class CNetCacheLogStream : public CRotatingLogStream
{
public:
    typedef CRotatingLogStream TParent;
public:
    CNetCacheLogStream(const string&    filename, 
                       CNcbiStreamoff   limit)
    : TParent(filename, limit)
    {}
protected:
    virtual string x_BackupName(string& name)
    {
        return kEmptyStr;
    }

};

/// @internal
///
/// Netcache logger
///
class CNetCache_Logger
{
public:
    CNetCache_Logger(const string&    filename, 
                     CNcbiStreamoff   limit)
      : m_Log(filename, limit)
    {}

    void Put(const string& auth, 
             const NetCache_RequestStat& stat,
             const string&  blob_key)
    {
        string msg, tmp;
        msg += auth;
        msg += ';';
        msg += stat.peer_address;
        msg += ';';
        msg += stat.conn_time.AsString();
        msg += ';';
        msg += (char)stat.req_code;
        msg += ';';
        NStr::UInt8ToString(tmp, stat.blob_size);
        msg += tmp;
        msg += ';';
        msg += NStr::DoubleToString(stat.elapsed, 5);
        msg += ';';
        msg += NStr::DoubleToString(stat.comm_elapsed, 5);
        msg += ';';
        msg += blob_key;
        msg += "\n";

        m_Log << msg;

        if (stat.req_code == 'V') {
            m_Log << NcbiFlush;
        }
    }
    void Rotate() { m_Log.Rotate(); }
private:
    CNetCacheLogStream m_Log;
};

class CNetCacheServer;

static CNetCacheServer* s_netcache_server = 0;


/// Netcache threaded server
///
/// @internal
class CNetCacheServer : public CThreadedServer
{
public:
    CNetCacheServer(unsigned int port,
                    ICache*      cache,
                    unsigned     max_threads,
                    unsigned     init_threads,
                    unsigned     network_timeout,
                    bool         is_log) 
        : CThreadedServer(port),
          m_MaxId(0),
          m_Cache(cache),
          m_Shutdown(false),
          m_InactivityTimeout(network_timeout),
          m_LogFlag(is_log),
          m_TLS_Size(64 * 1024)
    {
        char hostname[256];
        int status = SOCK_gethostname(hostname, sizeof(hostname));
        if (status == 0) {
            m_Host = hostname;
        }

        m_MaxThreads = max_threads ? max_threads : 25;
        m_InitThreads = init_threads ? 
            (init_threads < m_MaxThreads ? init_threads : 2)  : 10;
        m_QueueSize = m_MaxThreads + 2;

        x_CreateLog();

        s_netcache_server = this;
        m_AcceptTimeout = &m_ThrdSrvAcceptTimeout;
    }

    virtual ~CNetCacheServer() {}

    unsigned int GetTLS_Size() const { return m_TLS_Size; }
    void SetTLS_Size(unsigned int size) { m_TLS_Size = size; }

    /// Take request code from the socket and process the request
    virtual void  Process(SOCK sock);
    virtual bool ShutdownRequested(void) { return m_Shutdown; }

    typedef enum {
        eError,
        ePut,
        ePut2,   ///< PUT v.2 transmission protocol
        eGet,
        eShutdown,
        eVersion,
        eRemove,
        eLogging
    } ERequestType;
        
    void SetShutdownFlag() { if (!m_Shutdown) m_Shutdown = true; }
    
    /// Override some parent parameters
    virtual void SetParams()
    {
        m_ThrdSrvAcceptTimeout.sec = 0;
        m_ThrdSrvAcceptTimeout.usec = 500;
    }

protected:
    virtual void ProcessOverflow(SOCK sock) 
    { 
        SOCK_Close(sock); 
        ERR_POST("ProcessOverflow!");
    }

private:
    struct Request
    {
        ERequestType    req_type;
        unsigned int    timeout;
        string          req_id;
        string          err_msg;      
    };

    /// Process "PUT" request
    void ProcessPut(CSocket&              sock, 
                    Request&              req,
                    NetCache_RequestStat& stat);

    /// Process "PUT2" request
    void ProcessPut2(CSocket&              sock, 
                    Request&              req,
                    NetCache_RequestStat& stat);

    /// Process "GET" request
    void ProcessGet(CSocket&              sock, 
                    const Request&        req,
                    NetCache_RequestStat& stat);

    /// Process "VERSION" request
    void ProcessVersion(CSocket& sock, const Request& req);

    /// Process "LOG" request
    void ProcessLog(CSocket& sock, const Request& req);

    /// Process "REMOVE" request
    void ProcessRemove(CSocket& sock, const Request& req);

    /// Process "SHUTDOWN" request
    void ProcessShutdown();

    /// Returns FALSE when socket is closed or cannot be read
    bool ReadStr(CSocket& sock, string* str);

    /// Read buffer from the socket.
    // bool ReadBuffer(CSocket& sock, char* buf, size_t* buffer_length);

    /// TRUE return means we have EOF in the socket (no more data is coming)
    bool ReadBuffer(CSocket& sock, 
                    char*    buf, 
                    size_t   buf_size,
                    size_t*  read_length);

    /// TRUE return means we have EOF in the socket (no more data is coming)
    bool ReadBuffer(CSocket& sock,
                    IReader* rdr, 
                    char*    buf, 
                    size_t   buf_size,
                    size_t*  read_length);

    void ParseRequest(const string& reqstr, Request* req);

    /// Reply to the client
    void WriteMsg(CSocket& sock, 
                  const string& prefix, const string& msg);

    /// Generate unique system-wide request id
    void GenerateRequestId(const Request& req, 
                           string*        id_str,
                           unsigned int*  transaction_id);


    /// Get logger instance
    CNetCache_Logger* GetLogger();
    /// TRUE if logging is ON
    bool IsLog() const;

private:
    bool x_CheckBlobId(CSocket&       sock,
                       CNetCache_Key* blob_id, 
                       const string&  blob_key);

    void x_WriteBuf(CSocket& sock,
                    char*    buf,
                    size_t   bytes);

    void x_CreateLog();

private:
    /// Host name where server runs
    string             m_Host;
    /// ID counter
    unsigned           m_MaxId;
    /// Set of ids in use (PUT)
    bm::bvector<>      m_UsedIds;
    ICache*            m_Cache;
    /// Flags that server received a shutdown request
    volatile bool      m_Shutdown; 
    /// Time to wait for the client (seconds)
    unsigned           m_InactivityTimeout;
    /// Log writer
    auto_ptr<CNetCache_Logger>  m_Logger;
    /// Logging ON/OFF
    bool                        m_LogFlag;
    unsigned int                m_TLS_Size;
    
    /// Accept timeout for threaded server
    STimeout                    m_ThrdSrvAcceptTimeout;
    /// Quick local timer
    CFastLocalTime              m_LocalTimer;
};

/// @internal
static
bool s_WaitForReadSocket(CSocket& sock, unsigned time_to_wait)
{
    STimeout to = {time_to_wait, 0};
    EIO_Status io_st = sock.Wait(eIO_Read, &to);
    switch (io_st) {    
    case eIO_Success:
    case eIO_Closed: // following read will return EOF
        return true;  
    case eIO_Timeout:
        NCBI_THROW(CNetServiceException, eTimeout, kEmptyStr);
    default:
        NCBI_THROW(CNetServiceException, 
                   eCommunicationError, "Socket Wait error.");
        return false;
    }
    return false;
}        

/// Thread specific data for threaded server
///
/// @internal
///
struct ThreadData
{
    ThreadData(unsigned int size) 
        : buffer(new char[size + 256]) {}
    AutoPtr<char, ArrayDeleter<char> >  buffer;
};

///@internal
static
void TlsCleanup(ThreadData* p_value, void* /* data */ )
{
    delete p_value;
}

static
CRef< CTls<ThreadData> > s_tls(new CTls<ThreadData>);


void CNetCacheServer::Process(SOCK sock)
{
    string auth, request;

    try {
        
        NetCache_RequestStat    stat;
        CStopWatch              sw(false); // OFF by default
        bool is_log = IsLog();

        CSocket socket;
        socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);

        if (is_log) {
            stat.conn_time = m_LocalTimer.GetLocalTime();
            stat.blob_size = 0;
            stat.comm_elapsed = 0;
            stat.peer_address = socket.GetPeerAddress();
                       
            // get only host name
            string::size_type offs = stat.peer_address.find_first_of(":");
            if (offs != string::npos) {
                stat.peer_address.erase(offs, stat.peer_address.length());
            }

            sw.Start();
        }


        // Set socket parameters

        socket.DisableOSSendDelay();
        STimeout to = {m_InactivityTimeout, 0};
        socket.SetTimeout(eIO_ReadWrite , &to);

        // Set thread local data (buffers, etc.)

        ThreadData* tdata = s_tls->GetValue();
        if (tdata) {
        } else {
            tdata = new ThreadData(GetTLS_Size());
            s_tls->SetValue(tdata, TlsCleanup);
        }

        // Process request
        Request req;

        s_WaitForReadSocket(socket, m_InactivityTimeout);

        if (ReadStr(socket, &auth)) {
            
            s_WaitForReadSocket(socket, m_InactivityTimeout);

            if (ReadStr(socket, &request)) {                
                ParseRequest(request, &req);

                switch (req.req_type) {
                case ePut:
                    stat.req_code = 'P';
                    ProcessPut(socket, req, stat);
                    break;
                case ePut2:
                    stat.req_code = 'P';
                    ProcessPut2(socket, req, stat);
                    break;
                case eGet:
                    stat.req_code = 'G';
                    ProcessGet(socket, req, stat);
                    break;
                case eShutdown:
                    stat.req_code = 'S';
                    ProcessShutdown();
                    break;
                case eVersion:
                    stat.req_code = 'V';
                    ProcessVersion(socket, req);
                    break;
                case eRemove:
                    stat.req_code = 'R';
                    ProcessRemove(socket, req);
                    break;
                case eLogging:
                    stat.req_code = 'L';
                    ProcessLog(socket, req);
                    break;
                case eError:
                    WriteMsg(socket, "ERR:", req.err_msg);
                    break;
                default:
                    _ASSERT(0);
                } // switch
            }
        }

        // cleaning up the input wire, in case if there is some
        // trailing input.
        // i don't want to know wnat is it, but if i don't read it
        // some clients will receive PEER RESET error trying to read
        // servers answer.
        //
        // I'm reading fixed length sized buffer, so any huge ill formed 
        // request still will end up with an error on the client side
        // (considered a client's fault (DDOS attempt))
        to.sec = to.usec = 0;
        socket.SetTimeout(eIO_Read, &to);
        socket.Read(NULL, 1024);

        //
        // Logging.
        //
        if (is_log) {
            stat.elapsed = sw.Elapsed();
            CNetCache_Logger* lg = GetLogger();
            _ASSERT(lg);
            lg->Put(auth, stat, req.req_id);
        }
    } 
    catch (CNetCacheException &ex)
    {
        ERR_POST("Server error: " << ex.what());
    }
    catch (CNetServiceException& ex)
    {
        ERR_POST("Server error: " << ex.what());
    }
    catch (exception& ex)
    {
        ERR_POST("Execution error in command " << request << " " << ex.what());
    }
}

void CNetCacheServer::ProcessShutdown()
{    
    SetShutdownFlag();
}

void CNetCacheServer::ProcessVersion(CSocket& sock, const Request& req)
{
    WriteMsg(sock, "OK:", NETCACHED_VERSION); 
}

void CNetCacheServer::ProcessRemove(CSocket& sock, const Request& req)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }

    CNetCache_Key blob_id;
    if (!x_CheckBlobId(sock, &blob_id, req_id))
        return;

    CIdBusyGuard guard(&m_UsedIds, blob_id.id, m_InactivityTimeout);

    m_Cache->Remove(req_id);
}

void CNetCacheServer::x_WriteBuf(CSocket& sock,
                                 char*    buf,
                                 size_t   bytes)
{
    do {
        size_t n_written;
        EIO_Status io_st;
        io_st = sock.Write(buf, bytes, &n_written);
        switch (io_st) {
        case eIO_Success:
            break;
        case eIO_Timeout:
            NCBI_THROW(CNetServiceException, 
                       eTimeout, "Communication timeout error");
            break;
        default:
            NCBI_THROW(CNetServiceException, 
              eCommunicationError, "Communication error (cannot send data)");
            break;
        } // switch
        bytes -= n_written;
        buf += n_written;
    } while (bytes > 0);
}


void CNetCacheServer::ProcessGet(CSocket&               sock, 
                                 const Request&         req,
                                 NetCache_RequestStat&  stat)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }

    CIdBusyGuard guard(&m_UsedIds);
    guard.Lock(req_id, m_InactivityTimeout);

    ThreadData* tdata = s_tls->GetValue();
    _ASSERT(tdata);
    char* buf = tdata->buffer.get();

    ICache::BlobAccessDescr ba_descr;
    buf += 100;
    ba_descr.buf = buf;
    ba_descr.buf_size = GetTLS_Size() - 100;

    m_Cache->GetBlobAccess(req_id, 0, kEmptyStr, &ba_descr);
    if (ba_descr.blob_size == 0) { // not found
        if (ba_descr.reader == 0) {
blob_not_found:
            WriteMsg(sock, "ERR:", "BLOB not found.");
            return;
        } else {
            WriteMsg(sock, "OK:", "BLOB found. SIZE=0");
            return;
        }
    }

    stat.blob_size = ba_descr.blob_size;

    if (ba_descr.reader == 0) {  // all in buffer
        string msg("OK:BLOB found. SIZE=");
        string sz;
        NStr::UIntToString(sz, ba_descr.blob_size);
        msg += sz;

        const char* msg_begin = msg.c_str();
        const char* msg_end = msg_begin + msg.length();

        for (; msg_end >= msg_begin; --msg_end) {
            --buf;
            *buf = *msg_end;
            ++ba_descr.blob_size;
        }

        // translate BLOB fragment to the network
        CStopWatch  sw(true);

        x_WriteBuf(sock, buf, ba_descr.blob_size);

        stat.comm_elapsed += sw.Elapsed();

        return;

    } // inline BLOB


    // re-translate reader to the network

    auto_ptr<IReader> rdr(ba_descr.reader);
    if (!rdr.get()) {
        goto blob_not_found;
    }
    size_t blob_size = ba_descr.blob_size;

    bool read_flag = false;
    
    buf = tdata->buffer.get();

    size_t bytes_read;
    do {
        ERW_Result io_res = rdr->Read(buf, GetTLS_Size(), &bytes_read);
        if (io_res == eRW_Success && bytes_read) {
            if (!read_flag) {
                read_flag = true;
                string msg("BLOB found. SIZE=");
                string sz;
                NStr::UIntToString(sz, blob_size);
                msg += sz;
                WriteMsg(sock, "OK:", msg);
            }

            // translate BLOB fragment to the network
            CStopWatch  sw(true);

            x_WriteBuf(sock, buf, bytes_read);

            stat.comm_elapsed += sw.Elapsed();

        } else {
            break;
        }
    } while(1);
    if (!read_flag) {
        goto blob_not_found;
    }

}

void CNetCacheServer::ProcessPut(CSocket&              sock, 
                                 Request&              req,
                                 NetCache_RequestStat& stat)
{
    string& rid = req.req_id;
    CNetCache_Key blob_id;

    CIdBusyGuard guard(&m_UsedIds);

    if (!req.req_id.empty()) {  // UPDATE request
        guard.Lock(req.req_id, m_InactivityTimeout * 2);
    } else {
        guard.LockNewId(&m_MaxId, m_InactivityTimeout);
        unsigned int id = guard.GetId();
        CNetCache_GenerateBlobKey(&rid, id, m_Host, GetPort());
    }


    WriteMsg(sock, "ID:", rid);


    auto_ptr<IWriter> iwrt;

    // Reconfigure socket for no-timeout operation

    STimeout to;
    to.sec = to.usec = 0;
    sock.SetTimeout(eIO_Read, &to);

    ThreadData* tdata = s_tls->GetValue();
    _ASSERT(tdata);
    char* buf = tdata->buffer.get();
    size_t buf_size = GetTLS_Size();

    bool not_eof;

    do {
        size_t nn_read;

        CStopWatch  sw(true);

        not_eof = ReadBuffer(sock, buf, buf_size, &nn_read);

        if (nn_read == 0 && !not_eof) {
            m_Cache->Store(rid, 0, kEmptyStr, 
                            buf, nn_read, req.timeout);
            break;
        }

        stat.comm_elapsed += sw.Elapsed();
        stat.blob_size += nn_read;

        if (nn_read) {
            if (iwrt.get() == 0) { // first read

                if (not_eof == false) { // complete read
                    m_Cache->Store(rid, 0, kEmptyStr, 
                                   buf, nn_read, req.timeout);
                    return;
                }

                iwrt.reset(
                    m_Cache->GetWriteStream(rid, 0, kEmptyStr, req.timeout));
            }
            size_t bytes_written;
            ERW_Result res = 
                iwrt->Write(buf, nn_read, &bytes_written);
            if (res != eRW_Success) {
                WriteMsg(sock, "Err:", "Server I/O error");
                return;
            }
        } // if (nn_read)

    } while (not_eof);

    if (iwrt.get()) {
        iwrt->Flush();
        iwrt.reset(0);
    }
}


void CNetCacheServer::ProcessPut2(CSocket&              sock, 
                                  Request&              req,
                                  NetCache_RequestStat& stat)
{
    string& rid = req.req_id;

    CIdBusyGuard guard(&m_UsedIds);

    if (!req.req_id.empty()) {  // UPDATE request
        guard.Lock(req.req_id, m_InactivityTimeout);
    } else {
        guard.LockNewId(&m_MaxId, m_InactivityTimeout);
        unsigned int id = guard.GetId();
        CNetCache_GenerateBlobKey(&rid, id, m_Host, GetPort());
    }


    WriteMsg(sock, "ID:", rid);


    auto_ptr<IWriter> iwrt;

    ThreadData* tdata = s_tls->GetValue();
    _ASSERT(tdata);
    char* buf = tdata->buffer.get();
    size_t buf_size = GetTLS_Size();

    bool not_eof;

    CSocketReaderWriter  comm_reader(&sock, eNoOwnership);
    CTransmissionReader  transm_reader(&comm_reader, eNoOwnership);

    do {
        size_t nn_read = 0;

        CStopWatch  sw(true);

        not_eof = ReadBuffer(sock, &transm_reader, buf, buf_size, &nn_read);

        stat.comm_elapsed += sw.Elapsed();
        stat.blob_size += nn_read;
        

        if (nn_read == 0 && !not_eof) {
            m_Cache->Store(rid, 0, kEmptyStr, 
                            buf, nn_read, req.timeout);
            break;
        }

        if (nn_read) {
            if (iwrt.get() == 0) { // first read

                if (not_eof == false) { // complete read
                    m_Cache->Store(rid, 0, kEmptyStr, 
                                   buf, nn_read, req.timeout);
                    break;
                }

                iwrt.reset(
                    m_Cache->GetWriteStream(rid, 0, kEmptyStr, req.timeout));
            }
            size_t bytes_written;
            ERW_Result res = 
                iwrt->Write(buf, nn_read, &bytes_written);
            if (res != eRW_Success) {
                WriteMsg(sock, "Err:", "Server I/O error");
                return;
            }
        } // if (nn_read)

    } while (not_eof);

    if (iwrt.get()) {
        iwrt->Flush();
        iwrt.reset(0);
    }

}


void CNetCacheServer::WriteMsg(CSocket&       sock, 
                               const string&  prefix, 
                               const string&  msg)
{
    string err_msg(prefix);
    err_msg.append(msg);
    err_msg.append("\r\n");
    size_t n_written;
    /* EIO_Status io_st = */
        sock.Write(err_msg.c_str(), err_msg.length(), &n_written);
}

void CNetCacheServer::ParseRequest(const string& reqstr, Request* req)
{
    const char* s = reqstr.c_str();

    if (strncmp(s, "PUT2", 4) == 0) {
        req->req_type = ePut2;
        req->timeout = 0;
        req->req_id.erase();

        s += 4;
        goto put_args_parse;

    } // PUT2

    if (strncmp(s, "PUT", 3) == 0) {
        req->req_type = ePut;
        req->timeout = 0;
        req->req_id.erase();

        s += 3;
put_args_parse:
        while (*s && isspace(*s)) {
            ++s;
        }

        if (*s) {  // timeout value
            int time_out = atoi(s);
            if (time_out > 0) {
                req->timeout = time_out;
            }
        }
        while (*s && isdigit(*s)) {
            ++s;
        }
        while (*s && isspace(*s)) {
            ++s;
        }
        req->req_id = s;

        return;
    } // PUT

    if (strncmp(s, "GET", 3) == 0) {
        req->req_type = eGet;
        s += 3;
parse_blob_id:
        while (*s && isspace(*s)) {
            ++s;
        }

        req->req_id = s;
        return;
    } // GET

    if (strncmp(s, "REMOVE", 3) == 0) {
        req->req_type = eRemove;
        s += 6;
        goto parse_blob_id;
    } // REMOVE

    if (strncmp(s, "SHUTDOWN", 7) == 0) {
        req->req_type = eShutdown;
        return;
    } // SHUTDOWN

    if (strncmp(s, "VERSION", 7) == 0) {
        req->req_type = eVersion;
        return;
    } // VERSION

    if (strncmp(s, "LOG", 3) == 0) {
        req->req_type = eLogging;
        s += 3;
        goto parse_blob_id;  // "ON/OFF" instead of blob_id in this case
    } // LOG


    req->req_type = eError;
    req->err_msg = "Unknown request";
}

bool CNetCacheServer::ReadBuffer(CSocket& sock,
                                 IReader* rdr, 
                                 char*    buf, 
                                 size_t   buf_size,
                                 size_t*  read_length)
{
    *read_length = 0;
    size_t nn_read = 0;

    bool ret_flag = true;

    while (ret_flag) {
        if (!s_WaitForReadSocket(sock, m_InactivityTimeout)) {
            break;
        }

        ERW_Result io_res = rdr->Read(buf, buf_size, &nn_read);
        switch (io_res) 
        {
        case eRW_Success:
            break;
        case eRW_Timeout:
            if (*read_length == 0) {
                NCBI_THROW(CNetServiceException, eTimeout, "IReader timeout");
            }
            break;
        case eRW_Eof:
            ret_flag = false;
            break;
        default: // invalid socket or request
            NCBI_THROW(CNetServiceException, eCommunicationError, kEmptyStr);

        } // switch
        *read_length += nn_read;
        buf_size -= nn_read;

        if (buf_size <= 10) {  // buffer too small to read again
            break;
        }
        buf += nn_read;
    }

    return ret_flag;  // false means we hit "eIO_Closed"

}


bool CNetCacheServer::ReadBuffer(CSocket& sock, 
                                 char*    buf, 
                                 size_t   buf_size,
                                 size_t*  read_length)
{
    *read_length = 0;
    size_t nn_read = 0;

    bool ret_flag = true;

    while (ret_flag) {
        if (!s_WaitForReadSocket(sock, m_InactivityTimeout)) {
            break;
        }

        EIO_Status io_st = sock.Read(buf, buf_size, &nn_read);
        *read_length += nn_read;
        switch (io_st) 
        {
        case eIO_Success:
            break;
        case eIO_Timeout:
            if (*read_length == 0) {
                NCBI_THROW(CNetServiceException, eTimeout, kEmptyStr);
            }
            break;
        case eIO_Closed:
            ret_flag = false;
            break;
        default: // invalid socket or request
            NCBI_THROW(CNetServiceException, eCommunicationError, kEmptyStr);
        };
        buf_size -= nn_read;

        if (buf_size <= 10) {  // buffer too small to read again
            break;
        }
        buf += nn_read;
    }

    return ret_flag;  // false means we hit "eIO_Closed"

}



bool CNetCacheServer::ReadStr(CSocket& sock, string* str)
{
    _ASSERT(str);

    str->erase();
    char ch;
    EIO_Status io_st;

    char szBuf[1024] = {0,};
    unsigned str_len = 0;
    size_t n_read = 0;

    for (bool flag = true; flag; ) {
        io_st = sock.Read(szBuf, 256, &n_read, eIO_ReadPeek);
        switch (io_st) 
        {
        case eIO_Success:
            flag = false;
            break;
        case eIO_Timeout:
            NCBI_THROW(CNetServiceException, eTimeout, kEmptyStr);
            break;
        default: // invalid socket or request, bailing out
            return false;
        };
    }

    for (str_len = 0; str_len < n_read; ++str_len) {
        ch = szBuf[str_len];
        if (ch == 0)
            break;
        if (ch == '\n' || ch == '\r') {
            // analyse next char for \r\n sequence
            ++str_len;
            ch = szBuf[str_len];
            if (ch == '\n' || ch == '\r') {} else {--str_len;}
            break;
        }
        *str += ch;
    }

    if (str_len == 0) {
        return false;
    }
    io_st = sock.Read(szBuf, str_len + 1);
    return true;
}


void CNetCacheServer::GenerateRequestId(const Request& req, 
                                        string*        id_str,
                                        unsigned int*  transaction_id)
{
    long id;
    {{
    CFastMutexGuard guard(x_NetCacheMutex);
    id = ++m_MaxId;
    }}
    *transaction_id = id;

    CNetCache_GenerateBlobKey(id_str, id, m_Host, GetPort());
}


bool CNetCacheServer::x_CheckBlobId(CSocket&       sock,
                                    CNetCache_Key* blob_id, 
                                    const string&  blob_key)
{
    try {
        CNetCache_ParseBlobKey(blob_id, blob_key);
        if (blob_id->version != 1     ||
            blob_id->hostname.empty() ||
            blob_id->id == 0          ||
            blob_id->port == 0) 
        {
            NCBI_THROW(CNetCacheException, eKeyFormatError, kEmptyStr);
        }
    } 
    catch (CNetCacheException& )
    {
        WriteMsg(sock, "ERR:", "BLOB id format error.");
        return false;
    }
    return true;
}

void CNetCacheServer::ProcessLog(CSocket&  sock, const Request&  req)
{
    const char* str = req.req_id.c_str();
    if (NStr::strcasecmp(str, "ON")==0) {
        CFastMutexGuard guard(x_NetCacheMutex);
        if (!m_LogFlag) {
            m_LogFlag = true;
            m_Logger->Rotate();
        }
    }
    if (NStr::strcasecmp(str, "OFF")==0) {
        CFastMutexGuard guard(x_NetCacheMutex);
        m_LogFlag = false;
    }
}


CNetCache_Logger* CNetCacheServer::GetLogger()
{
    CFastMutexGuard guard(x_NetCacheMutex);

    return m_Logger.get();
}

bool CNetCacheServer::IsLog() const
{
    CFastMutexGuard guard(x_NetCacheMutex);
    return m_LogFlag;
}

void CNetCacheServer::x_CreateLog()
{
    CFastMutexGuard guard(x_NetCacheMutex);

    if (m_Logger.get()) {
        return; // nothing to do
    }
    m_Logger.reset(
        new CNetCache_Logger("netcached.log", 100 * 1024 * 1024));
}

///////////////////////////////////////////////////////////////////////


/// NetCache daemon application
///
/// @internal
///
class CNetCacheDApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
private:
    CRef<CCacheCleanerThread>  m_PurgeThread;
};

void CNetCacheDApp::Init(void)
{
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_DateTime);

    // Setup command line arguments and parameters
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netcached");

    arg_desc->AddFlag("reinit", "Recreate the storage directory.");

    SetupArgDescriptions(arg_desc.release());
}



int CNetCacheDApp::Run(void)
{
    CBDB_Cache* bdb_cache = 0;
    CArgs args = GetArgs();

    LOG_POST(NETCACHED_VERSION);

    try {
        const CNcbiRegistry& reg = GetConfig();

        CConfig conf(reg);

        const CConfig::TParamTree* param_tree = conf.GetTree();
        const TPluginManagerParamTree* bdb_tree = 
            param_tree->FindSubNode(kBDBCacheDriverName);


        auto_ptr<ICache> cache;

        if (bdb_tree) {

            CConfig bdb_conf((CConfig::TParamTree*)bdb_tree, eNoOwnership);
            const string& db_path = 
                bdb_conf.GetString("netcached", "path", 
                                    CConfig::eErr_Throw, kEmptyStr);

            if (args["reinit"]) {  // Drop the database directory
                CDir dir(db_path);
                dir.Remove();
            }
            

            LOG_POST("Initializing BDB cache");

            typedef CPluginManager<ICache> TCachePM;
            CPluginManager<ICache> pm_cache;
            pm_cache.RegisterWithEntryPoint(NCBI_EntryPoint_xcache_bdb);

            ICache* ic;

            try {
                ic = 
                    pm_cache.CreateInstance(
                        kBDBCacheDriverName,
                        CVersionInfo(TCachePM::TInterfaceVersion::eMajor,
                                     TCachePM::TInterfaceVersion::eMinor,
                                     TCachePM::TInterfaceVersion::ePatchLevel), 
                                     bdb_tree);
            } 
            catch (CBDB_Exception& ex)
            {
                bool drop_db = reg.GetBool("server", "drop_db", 
                                           true, 0, CNcbiRegistry::eReturn);
                if (drop_db) {
                    LOG_POST("Error initializing BDB ICache interface.");
                    LOG_POST(ex.what());
                    LOG_POST("Database directory will be droppped.");

                    CDir dir(db_path);
                    dir.Remove();

                    ic = 
                      pm_cache.CreateInstance(
                        kBDBCacheDriverName,
                        CVersionInfo(TCachePM::TInterfaceVersion::eMajor,
                                     TCachePM::TInterfaceVersion::eMinor,
                                     TCachePM::TInterfaceVersion::ePatchLevel), 
                                     bdb_tree);

                } else {
                    throw;
                }
            }

            bdb_cache = dynamic_cast<CBDB_Cache*>(ic);
            cache.reset(ic);

            if (bdb_cache) {
                bdb_cache->CleanLog();
            }

        } else {
            string msg = 
                "Configuration error. Cannot init storage. Driver name:";
            msg += kBDBCacheDriverName;
            ERR_POST(msg);
            return 1;
        }

        // cache storage media has been created

        if (!cache->IsOpen()) {
            ERR_POST("Configuration error. Cache not open.");
            return 1;
        }

        int port = 
            reg.GetInt("server", "port", 9000, 0, CNcbiRegistry::eReturn);

        unsigned max_threads =
            reg.GetInt("server", "max_threads", 25, 0, CNcbiRegistry::eReturn);
        unsigned init_threads =
            reg.GetInt("server", "init_threads", 5, 0, CNcbiRegistry::eReturn);
        unsigned network_timeout =
            reg.GetInt("server", "network_timeout", 10, 0, CNcbiRegistry::eReturn);
        if (network_timeout == 0) {
            LOG_POST(Warning << 
                "INI file sets 0 sec. network timeout. Assume 10 seconds.");
            network_timeout =  10;
        } else {
            LOG_POST("Network IO timeout " << network_timeout);
        }

        bool is_log =
            reg.GetBool("server", "log", false, 0, CNcbiRegistry::eReturn);
        unsigned tls_size =
            reg.GetInt("server", "tls_size", 64 * 1024, 0, CNcbiRegistry::eReturn);

        auto_ptr<CNetCacheServer> thr_srv(
            new CNetCacheServer(port, 
                                cache.get(), 
                                max_threads, 
                                init_threads,
                                network_timeout,
                                is_log));

        thr_srv->SetTLS_Size(tls_size);

        LOG_POST(Info << "Running server on port " << port);
        thr_srv->Run();
    }
    catch (CBDB_ErrnoException& ex)
    {
        NcbiCerr << "Error: DBD errno exception:" << ex.what();
        return 1;
    }
    catch (CBDB_LibException& ex)
    {
        NcbiCerr << "Error: DBD library exception:" << ex.what();
        return 1;
    }

    return 0;
}



/// @internal
extern "C" 
void Threaded_Server_SignalHandler( int )
{
    if (s_netcache_server && 
        (!s_netcache_server->ShutdownRequested()) ) {
        s_netcache_server->SetShutdownFlag();
        LOG_POST("Interrupt signal. Shutdown requested.");
    }
}



int main(int argc, const char* argv[])
{

#if defined(NCBI_OS_UNIX)
    // attempt to get server gracefully shutdown on signal
     signal( SIGINT, Threaded_Server_SignalHandler);
     signal( SIGTERM, Threaded_Server_SignalHandler);    
#endif

    return CNetCacheDApp().AppMain(argc, argv, 0, eDS_Default, "netcached.ini");
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.52  2005/04/25 15:37:58  kuznets
 * Improved logging
 *
 * Revision 1.51  2005/04/19 14:19:09  kuznets
 * New protocol for puit blob
 *
 * Revision 1.50  2005/03/31 19:31:15  kuznets
 * Corrected use of preprocessor
 *
 * Revision 1.49  2005/03/31 19:16:49  kuznets
 * Added build date to the version string
 *
 * Revision 1.48  2005/03/24 20:20:00  kuznets
 * Suppressed communication error (carries no information anyway)
 *
 * Revision 1.47  2005/03/24 01:23:44  vakatov
 * Fix accidental mix-up of 'flags' vs 'action' arg in calls to
 * CNcbiRegistry::Get*()
 *
 * Revision 1.46  2005/03/22 18:55:18  kuznets
 * Reflecting changes in connect library layout
 *
 * Revision 1.45  2005/03/21 20:12:05  kuznets
 * Fixed race condition with setting accept timeout
 *
 * Revision 1.44  2005/03/17 21:34:24  kuznets
 * Use CFastLocalTime
 *
 * Revision 1.43  2005/02/17 16:15:27  kuznets
 * Added ProcessOverflow with ERR_POST
 *
 * Revision 1.42  2005/02/15 15:16:50  kuznets
 * Rotated log stream, overloaded x_BackupName to self-rotate
 *
 * Revision 1.41  2005/02/09 19:36:39  kuznets
 * Restored accidentally removed SetTimeout()
 *
 * Revision 1.40  2005/02/09 19:34:08  kuznets
 * Trail buffer read using socket.Read(NULL, ...)
 *
 * Revision 1.39  2005/02/09 13:36:14  kuznets
 * Limit log size to 100M, rotate log every time its turned ON
 *
 * Revision 1.38  2005/02/07 18:55:40  kuznets
 * REmoved port number from the connection log
 *
 * Revision 1.37  2005/02/07 13:06:37  kuznets
 * Logging improvements
 *
 * Revision 1.36  2005/01/24 17:21:40  vasilche
 * Removed redundant comparison "bool != false".
 *
 * Revision 1.35  2005/01/05 15:34:51  kuznets
 * Fast shutdown through low small accept timeout, restored signal procesing
 *
 * Revision 1.34  2005/01/04 18:55:30  kuznets
 * Commented out signal handlers
 *
 * Revision 1.33  2005/01/04 17:33:34  kuznets
 * Added graceful shutdown on SIGTERM(unix)
 *
 * Revision 1.32  2005/01/03 14:29:51  kuznets
 * Improved logging
 *
 * Revision 1.31  2004/12/29 15:35:37  kuznets
 * Fixed bug in comm. protocol
 *
 * Revision 1.29  2004/12/27 19:14:07  kuznets
 * Use NStr::strcasecmp instead of stricmp
 *
 * Revision 1.28  2004/12/27 16:31:32  kuznets
 * Implemented server side logging
 *
 * Revision 1.27  2004/12/22 21:02:53  grichenk
 * BDB and DBAPI caches split into separate libs.
 * Added entry point registration, fixed driver names.
 *
 * Revision 1.26  2004/12/22 14:36:13  kuznets
 * Performance optimization (ProcessGet)
 *
 * Revision 1.25  2004/11/08 16:02:53  kuznets
 * BLOB timeout passed to ICache
 *
 * Revision 1.24  2004/11/01 16:16:02  kuznets
 * Use NStr instead of itoa
 *
 * Revision 1.23  2004/11/01 16:03:39  kuznets
 * Added blob size to GET response
 *
 * Revision 1.22  2004/11/01 14:40:24  kuznets
 * Implemented BLOB update, fixed bug in object locking
 *
 * Revision 1.21  2004/10/28 16:14:42  kuznets
 * Implemented REMOVE
 *
 * Revision 1.20  2004/10/27 17:08:57  kuznets
 * Purge thread has been delegated to CBDB_Cache
 *
 * Revision 1.19  2004/10/27 14:18:02  kuznets
 * BLOB key parser moved from netcached
 *
 * Revision 1.18  2004/10/26 14:21:41  kuznets
 * New parameter network_timeout
 *
 * Revision 1.17  2004/10/26 13:36:27  kuznets
 * new startup flag -reinit and drop_db ini parameter
 *
 * Revision 1.16  2004/10/25 16:06:18  kuznets
 * Better timeout handling, use larger network bufers, VERSION command
 *
 * Revision 1.15  2004/10/21 17:21:42  kuznets
 * Reallocated buffer replaced with TLS data
 *
 * Revision 1.14  2004/10/21 15:51:21  kuznets
 * removed unused variable
 *
 * Revision 1.13  2004/10/21 15:06:10  kuznets
 * New parameter run_purge_thread
 *
 * Revision 1.12  2004/10/21 13:39:27  kuznets
 * New parameters max_threads, init_threads
 *
 * Revision 1.11  2004/10/20 21:21:02  ucko
 * Make CNetCacheServer::ERequestType public so that struct Request can
 * make use of it when building with WorkShop.
 *
 * Revision 1.10  2004/10/20 14:50:22  kuznets
 * Code cleanup
 *
 * Revision 1.9  2004/10/20 13:45:37  kuznets
 * Disabled TCP/IP delay on write
 *
 * Revision 1.8  2004/10/19 18:20:26  kuznets
 * Use ReadPeek to avoid net delays
 *
 * Revision 1.7  2004/10/19 15:53:36  kuznets
 * Code cleanup
 *
 * Revision 1.6  2004/10/18 13:46:57  kuznets
 * Removed common buffer (was shared between threads)
 *
 * Revision 1.5  2004/10/15 14:34:46  kuznets
 * Fine tuning of networking, cleaning thread interruption, etc
 *
 * Revision 1.4  2004/10/04 12:33:24  kuznets
 * Complete implementation of GET/PUT
 *
 * Revision 1.3  2004/09/23 16:37:21  kuznets
 * Reflected changes in ncbi_config.hpp
 *
 * Revision 1.2  2004/09/23 14:17:36  kuznets
 * Fixed compilation bug
 *
 * Revision 1.1  2004/09/23 13:57:01  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
