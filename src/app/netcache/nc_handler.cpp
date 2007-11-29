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
 * Authors:  Victor Joukov
 *
 * File Description: Network cache daemon
 *
 */
#include <ncbi_pch.hpp>

#include "nc_handler.hpp"
#include "netcache_version.hpp"

BEGIN_NCBI_SCOPE

CNetCacheHandler::CNetCacheHandler(CNetCache_RequestHandlerHost* host) :
    CNetCache_RequestHandler(host), m_PutOK(false)
{}

void CNetCacheHandler::ParseRequest(const string& reqstr, SNC_Request* req)
{
    const char* s = reqstr.c_str();


    switch (s[0]) {

    case 'P':

        if (strncmp(s, "PUT3", 4) == 0) {
            req->req_type = ePut3;
            req->timeout = 0;
            req->req_id.erase();

            s += 4;
            goto put_args_parse;

        } // PUT2

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
            while (*s && isspace((unsigned char)(*s))) {
                ++s;
            }

            if (*s) {  // timeout value
                int time_out = atoi(s);
                if (time_out > 0) {
                    req->timeout = time_out;
                }
            }
            while (*s && isdigit((unsigned char)(*s))) {
                ++s;
            }
            while (*s && isspace((unsigned char)(*s))) {
                ++s;
            }
            req->req_id = s;

            return;
        } // PUT


        break;


    case 'G':

        if (strncmp(s, "GET2", 4) == 0) {
            req->req_type = eGet;
            s += 4;
            goto parse_get;
        }


        if (strncmp(s, "GET", 3) == 0) {
            req->req_type = eGet;
            s += 3;

        parse_get:

            if (isspace((unsigned char)(*s))) { // "GET"
                while (*s && isspace((unsigned char)(*s))) {
                    ++s;
                }

                req->req_id.erase();

                while (*s && !isspace((unsigned char)(*s))) {
                    char ch = *s;
                    req->req_id.append(1, ch);
                    ++s;
                }

                if (!*s) {
                    return;
                }

                // skip whitespace
                while (*s && isspace((unsigned char)(*s))) {
                    ++s;
                }

                if (!*s) {
                    return;
                }

                // NW modificator (no wait request)
                if (s[0] == 'N' && s[1] == 'W') {
                    req->no_lock = true;
                }

                return;

            } else { // GET...
                if (strncmp(s, "CONF", 4) == 0) {
                    req->req_type = eGetConfig;
                    s += 4;
                    return;
                } // GETCONF

                if (strncmp(s, "STAT", 4) == 0) {
                    req->req_type = eGetStat;
                    s += 4;
                    return;
                } // GETSTAT
            }
        } // GET*


        if (strncmp(s, "GBOW", 4) == 0) {  // get blob owner
            req->req_type = eGetBlobOwner;
            s += 4;
            goto parse_blob_id;
        } // GBOW

        break;

    case 'H':
        if (strncmp(s, "HASB", 4) == 0) {  // has blob
            req->req_type = eHasBlob;
            s += 4;
            goto parse_blob_id;
        } // HASB

        break;

    case 'I':

        if (strncmp(s, "ISLK", 4) == 0) {
            req->req_type = eIsLock;
            s += 4;

    parse_blob_id:
            req->req_id.erase();
            while (*s && isspace((unsigned char)(*s))) {
                ++s;
            }

            req->req_id = s;
            return;
        }

        break;

    case 'R':

        if (strncmp(s, "RMV2", 4) == 0) {
            req->req_type = eRemove2;
            s += 4;
            goto parse_blob_id;
        } // REMOVE

        if (strncmp(s, "REMOVE", 3) == 0) {
            req->req_type = eRemove;
            s += 6;
            goto parse_blob_id;
        } // REMOVE

        break;

    case 'D':

        if (strncmp(s, "DROPSTAT", 8) == 0) {
            req->req_type = eDropStat;
            s += 8;
            return;
        } // DROPSTAT

        break;

    case 'S':

        if (strncmp(s, "SHUTDOWN", 7) == 0) {
            req->req_type = eShutdown;
            return;
        } // SHUTDOWN

        break;

    case 'V':

        if (strncmp(s, "VERSION", 7) == 0) {
            req->req_type = eVersion;
            return;
        } // VERSION

        break;

    case 'L':

        if (strncmp(s, "LOG", 3) == 0) {
            req->req_type = eLogging;
            s += 3;
            goto parse_blob_id;  // "ON/OFF" instead of blob_id in this case
        } // LOG

        break;

    default:
        break;
    
    } // switch


    req->req_type = eError;
    req->err_msg = "Unknown request";
}


void CNetCacheHandler::ProcessRequest(string&               request,
                                      const string&         auth,
                                      NetCache_RequestStat& stat,
                                      NetCache_RequestInfo* info)
{
    CSocket& socket = GetSocket();
    SNC_Request req;
    ParseRequest(request, &req);

    m_Auth = &auth;

    if (info) {
        info->type = "NC";
        info->blob_id = req.req_id;
        info->details = req.req_id;
    }

    stat.op_code = SBDB_CacheUnitStatistics::eErr_Unknown;
    switch (req.req_type) {
    case ePut:
        stat.op_code = SBDB_CacheUnitStatistics::eErr_Put;
        stat.req_code = 'P';
        ProcessPut(socket, req, stat);
        break;
    case ePut2:
        stat.op_code = SBDB_CacheUnitStatistics::eErr_Put;
        stat.req_code = 'P';
        ProcessPut2(socket, req, stat);
        break;
    case ePut3:
        stat.op_code = SBDB_CacheUnitStatistics::eErr_Put;
        stat.req_code = 'P';
        ProcessPut3(socket, req, stat);
        break;
    case eGet:
        stat.op_code = SBDB_CacheUnitStatistics::eErr_Get;
        stat.req_code = 'G';
        ProcessGet(socket, req, stat);
        break;
    case eShutdown:
        stat.req_code = 'S';
        ProcessShutdown(socket);
        break;
    case eGetConfig:
        stat.req_code = 'C';
        ProcessGetConfig(socket);
        break;
    case eGetStat:
        stat.req_code = 'T';
        ProcessGetStat(socket, req);
        break;
    case eDropStat:
        stat.req_code = 'D';
        ProcessDropStat(socket);
        break;
    case eHasBlob:
        ProcessHasBlob(socket, req);
        break;
    case eGetBlobOwner:
        ProcessGetBlobOwner(socket, req);
        break;
    case eVersion:
        stat.req_code = 'V';
        ProcessVersion(socket, req);
        break;
    case eRemove:
        stat.req_code = 'R';
        ProcessRemove(socket, req);
        break;
    case eRemove2:
        stat.req_code = 'R';
        ProcessRemove2(socket, req);
        break;
    case eLogging:
        stat.req_code = 'L';
        ProcessLog(socket, req);
        break;
    case eIsLock:
        stat.req_code = 'K';
        ProcessIsLock(socket, req);
        break;
    case eError:
        WriteMsg(socket, "ERR:", req.err_msg);
        m_Server->RegisterProtocolErr(
            SBDB_CacheUnitStatistics::eErr_Unknown, *m_Auth);
        break;
    default:
        _ASSERT(0);
    } // switch
}


void CNetCacheHandler::ProcessShutdown(CSocket& sock)
{    
    m_Server->SetShutdownFlag();
    WriteMsg(sock, "OK:", "");
}

void CNetCacheHandler::ProcessVersion(CSocket& sock, const SNC_Request& req)
{
    WriteMsg(sock, "OK:", NETCACHED_VERSION); 
}

void CNetCacheHandler::ProcessGetConfig(CSocket& sock)
{
    SOCK sk = sock.GetSOCK();
    sock.SetOwnership(eNoOwnership);
    sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sk);
    m_Server->GetRegistry().Write(ios);
}

void CNetCacheHandler::ProcessGetStat(CSocket& sock, const SNC_Request& req)
{
    //CNcbiRegistry reg;

    SOCK sk = sock.GetSOCK();
    sock.SetOwnership(eNoOwnership);
    sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sk);

    CBDB_Cache* bdb_cache = m_Server->GetCache();
    if (!bdb_cache) {
        return;
    }
    bdb_cache->Lock();

    try {

        const SBDB_CacheStatistics& cs = bdb_cache->GetStatistics();
        cs.PrintStatistics(ios);
        //cs.ConvertToRegistry(&reg);

    } catch(...) {
        bdb_cache->Unlock();
        throw;
    }
    bdb_cache->Unlock();

    //reg.Write(ios,  CNcbiRegistry::fTransient | CNcbiRegistry::fPersistent);
}

void CNetCacheHandler::ProcessDropStat(CSocket& sock)
{
    CBDB_Cache* bdb_cache = m_Server->GetCache();
    if (!bdb_cache) {
        return;
    }
	bdb_cache->InitStatistics();
    WriteMsg(sock, "OK:", "");
}


void CNetCacheHandler::ProcessRemove(CSocket& sock, const SNC_Request& req)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }

    CNetCache_Key blob_id(req_id);
    if (!x_CheckBlobId(sock, &blob_id, req_id))
        return;

    m_Server->GetCache()->Remove(req_id);
}


void CNetCacheHandler::ProcessRemove2(CSocket& sock, const SNC_Request& req)
{
    ProcessRemove(sock, req);
    WriteMsg(sock, "OK:", "");
}


void CNetCacheHandler::ProcessLog(CSocket&  sock, const SNC_Request&  req)
{
    const char* str = req.req_id.c_str();
    bool sw_log;
    if (NStr::strcasecmp(str, "ON") == 0)
        sw_log = true;
    else if (NStr::strcasecmp(str, "OFF") == 0)
        sw_log = false;
    else {
        WriteMsg(sock, "ERR:", "");
        return;
    }
    m_Server->SwitchLog(sw_log);
    WriteMsg(sock, "OK:", "");
}


bool CNetCacheHandler::ProcessWrite()
{
    char buf[4096];
    size_t bytes_read;
    ERW_Result io_res = m_Reader->Read(buf, sizeof(buf), &bytes_read);
    if (io_res != eRW_Success || !bytes_read) {
        m_Reader.reset(0);
        return false;
    }
    // CStopWatch  sw(CStopWatch::eStart);
    CNetCacheServer::WriteBuf(GetSocket(), buf, bytes_read);
    return true;
}


void CNetCacheHandler::ProcessGet(CSocket&               sock, 
                                  const SNC_Request&     req,
                                  NetCache_RequestStat&  stat)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        m_Server->RegisterProtocolErr(stat.op_code, *m_Auth);
        return;
    }

    if (req.no_lock) {
        bool locked = m_Server->GetCache()->IsLocked(req_id, 0, kEmptyStr);
        if (locked) {
            WriteMsg(sock, "ERR:", "BLOB locked by another client"); 
            return;
        }
    }

    ICache::SBlobAccessDescr ba_descr;
    ba_descr.buf = 0;
    ba_descr.buf_size = 0;

    unsigned blob_id = CNetCache_Key::GetBlobId(req_id);

    CBDB_Cache* bdb_cache = m_Server->GetCache();
    for (int repeats = 0; repeats < 1000; ++repeats) {
        bdb_cache->GetBlobAccess(req_id, 0, kEmptyStr, &ba_descr);

        if (!ba_descr.blob_found) {
            // check if id is locked maybe blob record not
            // yet commited
            {{
                if (bdb_cache->IsLocked(blob_id)) {
                    if (repeats < 999) {
                        SleepMilliSec(repeats);
                        continue;
                    }
                } else {
                    bdb_cache->GetBlobAccess(req_id, 0, kEmptyStr, &ba_descr);
                    if (ba_descr.blob_found) {
                        break;
                    }

                }
            }}
            string msg = "BLOB not found. ";
            msg += req_id;
            WriteMsg(sock, "ERR:", msg);
            return;
        } else {
            break;
        }
    }

    if (ba_descr.blob_size == 0) {
        WriteMsg(sock, "OK:", "BLOB found. SIZE=0");
        return;
    }

    stat.blob_size = ba_descr.blob_size;

    // re-translate reader to the network

    m_Reader.reset(ba_descr.reader.release());
    if (!m_Reader.get()) {
        string msg = "BLOB not found. ";
        msg += req_id;
        WriteMsg(sock, "ERR:", msg);
        return;
    }

    // Write first chunk right here
    char buf[4096];
    size_t bytes_read;
    ERW_Result io_res = m_Reader->Read(buf, sizeof(buf), &bytes_read);
    if (io_res != eRW_Success || !bytes_read) { // TODO: should we check here for bytes_read?
        string msg = "BLOB not found. ";
        msg += req_id;
        WriteMsg(sock, "ERR:", msg);
        m_Reader.reset(0);
        return;
    }
    string msg("BLOB found. SIZE=");
    string sz;
    NStr::UIntToString(sz, ba_descr.blob_size);
    msg += sz;
    WriteMsg(sock, "OK:", msg);
    
    // translate BLOB fragment to the network
    CStopWatch  sw(CStopWatch::eStart);

    CNetCacheServer::WriteBuf(sock, buf, bytes_read);

    // TODO: stat should be exposed to ProcessWrite somehow
    stat.comm_elapsed += sw.Elapsed();
    ++stat.io_blocks;

    // TODO: Can we check here that bytes_read is less than sizeof(buf)
    // and optimize out delayed write?
    m_Host->BeginDelayedWrite();
}


void CNetCacheHandler::ProcessPut(CSocket&              sock, 
                                  SNC_Request&          req,
                                  NetCache_RequestStat& stat)
{
    WriteMsg(sock, "ERR:", "Obsolete");
}


void CNetCacheHandler::ProcessPut2(CSocket&              sock, 
                                   SNC_Request&          req,
                                   NetCache_RequestStat& stat)
{
    string& rid = req.req_id;
    bool do_id_lock = true;

    CBDB_Cache* bdb_cache = m_Server->GetCache();
    unsigned int id = 0;
    _TRACE("Getting an id, socket " << &sock);
    if (req.req_id.empty()) {
        id = bdb_cache->GetNextBlobId(do_id_lock);
        CNetCache_Key::GenerateBlobKey(&rid, id,
            m_Server->GetHost(), m_Server->GetPort());
        do_id_lock = false;
    } else {
        id = CNetCache_Key::GetBlobId(req.req_id);
    }
    _TRACE("Got id " << id);

    // BLOB already locked, it is safe to return BLOB id
    if (!do_id_lock) {
        WriteMsg(sock, "ID:", rid);
    }

    // create the reader up front to guarantee correct BLOB locking
    // the possible problem (?) here is that we have to do double buffering
    // of the input stream
    m_Writer.reset(
        bdb_cache->GetWriteStream(id, rid, 0, kEmptyStr, do_id_lock,
                                  req.timeout, *m_Auth));

    if (do_id_lock) {
        WriteMsg(sock, "ID:", rid);
    }

    _TRACE("Begin read transmission");
    m_Host->BeginReadTransmission();
}


bool CNetCacheHandler::ProcessTransmission(
    const char* buf, size_t buf_size, ETransmission eot)
{
    size_t bytes_written;
    ERW_Result res = 
        m_Writer->Write(buf, buf_size, &bytes_written);
    if (res != eRW_Success) {
        _TRACE("Transmission failed, socket " << &GetSocket());
        WriteMsg(GetSocket(), "ERR:", "Server I/O error");
        m_Writer->Flush();
        m_Writer.reset(0);
        m_Server->RegisterInternalErr(
            SBDB_CacheUnitStatistics::eErr_Put, *m_Auth);
        return false;
    }
    if (eot == eTransmissionLastBuffer) {
        _TRACE("Flushing transmission, socket " << &GetSocket());
        m_Writer->Flush();
        m_Writer.reset(0);
        if (m_PutOK) {
            _TRACE("OK, socket " << &GetSocket());
            WriteMsg(GetSocket(), "OK:", "");
            m_PutOK = false;
        }
    }
    return true;
}


void CNetCacheHandler::ProcessPut3(CSocket&              sock, 
                                   SNC_Request&          req,
                                   NetCache_RequestStat& stat)
{
    m_PutOK = true;
    ProcessPut2(sock, req, stat);
}


void CNetCacheHandler::ProcessHasBlob(CSocket&           sock, 
                                      const SNC_Request& req)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }
    CNetCache_Key blob_id(req_id);
    _TRACE("Checking blob " << req_id);
    if (!x_CheckBlobId(sock, &blob_id, req_id))
        return;

    bool hb = m_Server->GetCache()->HasBlobs(req_id, kEmptyStr);
    string str;
    NStr::UIntToString(str, (int)hb);
    WriteMsg(sock, "OK:", str);
}


void CNetCacheHandler::ProcessGetBlobOwner(CSocket&           sock, 
                                           const SNC_Request& req)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }
    CNetCache_Key blob_id(req_id);
    if (!x_CheckBlobId(sock, &blob_id, req_id))
        return;

    string owner;
    m_Server->GetCache()->GetBlobOwner(req_id, 0, kEmptyStr, &owner);

    WriteMsg(sock, "OK:", owner);
}


void CNetCacheHandler::ProcessIsLock(CSocket& sock, const SNC_Request& req)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }
    CNetCache_Key blob_id(req_id);
    if (!x_CheckBlobId(sock, &blob_id, req_id))
        return;

    if (m_Server->GetCache()->IsLocked(blob_id.id)) {
        WriteMsg(sock, "OK:", "1");
    } else {
        WriteMsg(sock, "OK:", "0");
    }

}


bool CNetCacheHandler::x_CheckBlobId(CSocket&       sock,
                                    CNetCache_Key* blob_id, 
                                    const string&  blob_key)
{
    if (blob_id->version != 1     ||
        blob_id->hostname.empty() ||
        blob_id->id == 0          ||
        blob_id->port == 0) 
    {
        WriteMsg(sock, "ERR:", "BLOB id format error.");
        return false;
    }
    return true;
}


END_NCBI_SCOPE
