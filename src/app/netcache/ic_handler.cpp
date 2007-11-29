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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Network cache daemon
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimtx.hpp>

#include "netcached.hpp"
#include "ic_handler.hpp"

BEGIN_NCBI_SCOPE

CICacheHandler::CICacheHandler(CNetCache_RequestHandlerHost* host) :
    CNetCache_RequestHandler(host)
{}

#define NC_SKIPNUM(x)  \
    while (*x && (*x >= '0' && *x <= '9')) { ++x; }
#define NC_SKIPSPACE(x)  \
    while (*x && (*x == ' ' || *x == '\t')) { ++x; }
#define NC_RETURN_ERROR(err) \
    { req->req_type = eIC_Error; req->err_msg = err; return; }
#define NC_CHECK_END(s) \
    if (*s == 0) { \
        req->req_type = eIC_Error; req->err_msg = "Protocol error"; return; }
#define NC_GETSTRING(x, str) \
    for (;*x && !(*x == ' ' || *x == '\t'); ++x) { str.push_back(*x); }
#define NC_GETVAR(x, str) \
    if (*x != '"') NC_RETURN_ERROR("Invalid IC request"); \
    do { ++x;\
        if (*x == 0) NC_RETURN_ERROR("Invalid IC request"); \
        if (*x == '"') { ++x; break; } \
        str.push_back(*x); \
    } while (1);



inline static
bool CmpCmd4(const char* req, const char* cmd4)
{
    return req[0] == cmd4[0] && 
           req[1] == cmd4[1] &&
           req[2] == cmd4[2] &&
           req[3] == cmd4[3];
}


void CICacheHandler::ParseRequest(const string& reqstr, SIC_Request* req)
{
    // request format:
    // IC(cache_name) COMMAND_CLAUSE
    //
    // COMMAND_CLAUSE:
    //   SVRP KA | DO | DA
    //   GVRP
    //   STSP policy_mask timeout max_timeout
    //   GTSP
    //   GTOU
    //   ISOP
    //   STOR ttl "key" version "subkey"
    //   STRS ttl blob_size "key" version "subkey"
    //   GSIZ "key" version "subkey"
    //   GBLW "key" version "subkey"
    //   READ "key" version "subkey"
    //   REMO "key" version "subkey"
    //   GACT "key" version "subkey"
    //   HASB "key" 0 "subkey"
    //   PRG1 "key" keep_version


    const char* s = reqstr.c_str();

    if (s[0] != 'I' || s[1] != 'C' || s[2] != '(') {
        NC_RETURN_ERROR("Unknown request");
    }

    s+=3;

    for (; *s != ')'; ++s) {
        if (*s == 0) {
            NC_RETURN_ERROR("Invalid IC request")
        }
        req->cache_name.append(1, *s);
    }

    ++s;

    NC_SKIPSPACE(s)

    // Parsing commands


    if (CmpCmd4(s, "STOR")) { // Store
        req->req_type = eIC_Store;
        s += 4;
        NC_SKIPSPACE(s)
        req->i0 = (unsigned) atoi(s);  // ttl
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
    get_kvs:
        NC_GETVAR(s, req->key)
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->version = (unsigned) atoi(s);
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        NC_GETVAR(s, req->subkey)
        return;
    } // STOR

    if (CmpCmd4(s, "STRS")) { // Store size
        req->req_type = eIC_StoreBlob;
        s += 4;
        NC_SKIPSPACE(s)
        req->i0 = (unsigned) atoi(s);  // ttl
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        req->i1 = (unsigned) atoi(s);  // blob size
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "GSIZ")) { // GetSize
        req->req_type = eIC_GetSize;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "GBLW")) { // GetBlobOwner
        req->req_type = eIC_GetBlobOwner;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "READ")) { // Read
        req->req_type = eIC_Read;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "REMO")) { // Remove
        req->req_type = eIC_Remove;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "REMK")) { // RemoveKey
        req->req_type = eIC_Remove;
        s += 4;
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        NC_GETVAR(s, req->key);
        return;
    }

    if (CmpCmd4(s, "GACT")) { // GetAccessTime
        req->req_type = eIC_GetAccessTime;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "HASB")) { // HasBlobs
        req->req_type = eIC_HasBlobs;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }


    if (CmpCmd4(s, "STSP")) { // SetTimeStampPolicy
        req->req_type = eIC_SetTimeStampPolicy;
        s += 4;
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->i0 = (unsigned)atoi(s);
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->i1 = (unsigned)atoi(s);
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->i2 = (unsigned)atoi(s);
        return;
    }

    if (CmpCmd4(s, "GTSP")) { // GetTimeStampPolicy
        req->req_type = eIC_GetTimeStampPolicy;
        return;
    }

    if (CmpCmd4(s, "GTOU")) { // GetTimeout
        req->req_type = eIC_GetTimeout;
        return;
    }

    if (CmpCmd4(s, "ISOP")) { // IsOpen
        req->req_type = eIC_IsOpen;
        return;
    }

    if (CmpCmd4(s, "SVRP")) { // SetVersionRetention
        req->req_type = eIC_SetVersionRetention;
        s += 4;
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        NC_GETSTRING(s, req->key)
        return;
    }
    if (CmpCmd4(s, "GVRP")) { // GetVersionRetention
        req->req_type = eIC_GetVersionRetention;
        return;
    }

    if (CmpCmd4(s, "PRG1")) { // Purge1
        req->req_type = eIC_Purge1;
        s += 4;
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->i0 = (unsigned)atoi(s);
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->i1 = (unsigned)atoi(s);
        return;
    }
}


void CICacheHandler::ProcessRequest(string&               request,
                                    const string&         auth,
                                    NetCache_RequestStat& stat,
                                    NetCache_RequestInfo* info)
{
    CSocket& socket = GetSocket();
    SIC_Request req;
    ParseRequest(request, &req);

    m_Auth = &auth;

    ICache* ic = m_Server->GetLocalCache(req.cache_name);
    if (ic == 0) {
        req.err_msg = "Cache unknown: ";
        req.err_msg.append(req.cache_name);
        WriteMsg(socket, "ERR:", req.err_msg);
        return;
    }

    if (info) {
        info->type = "IC";
        info->blob_id = req.key + ":" +
            NStr::IntToString(req.version) + ":" +
            req.subkey;
        info->details = "Cache=\"";
        info->details += req.cache_name + "\"";
        info->details += " key=\"" + req.key;
        info->details += "\" version=" + NStr::IntToString(req.version);
        info->details += " subkey=\"" + req.subkey +"\"";
    }


    stat.op_code = SBDB_CacheUnitStatistics::eErr_Unknown;
    switch (req.req_type) {
    case eIC_SetTimeStampPolicy:
        Process_IC_SetTimeStampPolicy(*ic, socket, req);
        break;
    case eIC_GetTimeStampPolicy:
        Process_IC_GetTimeStampPolicy(*ic, socket, req);
        break;
    case eIC_SetVersionRetention:
        Process_IC_SetVersionRetention(*ic, socket, req);
        break;
    case eIC_GetVersionRetention:
        Process_IC_GetVersionRetention(*ic, socket, req);
        break;
    case eIC_GetTimeout:
        Process_IC_GetTimeout(*ic, socket, req);
        break;
    case eIC_IsOpen:
        Process_IC_IsOpen(*ic, socket, req);
        break;
    case eIC_Store:
        Process_IC_Store(*ic, socket, req);
        break;
    case eIC_StoreBlob:
        Process_IC_StoreBlob(*ic, socket, req);
        break;
    case eIC_GetSize:
        Process_IC_GetSize(*ic, socket, req);
        break;
    case eIC_GetBlobOwner:
        Process_IC_GetBlobOwner(*ic, socket, req);
        break;
    case eIC_Read:
        Process_IC_Read(*ic, socket, req);
        break;
    case eIC_Remove:
        Process_IC_Remove(*ic, socket, req);
        break;
    case eIC_RemoveKey:
        Process_IC_RemoveKey(*ic, socket, req);
        break;
    case eIC_GetAccessTime:
        Process_IC_GetAccessTime(*ic, socket, req);
        break;
    case eIC_HasBlobs:
        Process_IC_HasBlobs(*ic, socket, req);
        break;
    case eIC_Purge1:
        Process_IC_Purge1(*ic, socket, req);
        break;

    case eIC_Error:
        WriteMsg(socket, "ERR:", req.err_msg);
        break;
    default:
        _ASSERT(0);
    }
}


bool CICacheHandler::ProcessTransmission(
    const char* buf, size_t buf_size, ETransmission eot)
{
    size_t bytes_written;
    if (m_SizeKnown && (buf_size > m_BlobSize)) {
        m_BlobSize = 0;
        WriteMsg(GetSocket(), "ERR:", "Blob overflow");
        return false;
    }
    if ((eot != eTransmissionLastBuffer) || buf_size) {
        ERW_Result res = 
            m_Writer->Write(buf, buf_size, &bytes_written);
        m_BlobSize -= buf_size;
        if (res != eRW_Success) {
            WriteMsg(GetSocket(), "ERR:", "Server I/O error");
            m_Server->RegisterInternalErr(
                SBDB_CacheUnitStatistics::eErr_Put, *m_Auth);
            return false;
        }
    }
    if ((eot == eTransmissionLastBuffer) ||
        // Handle STRS implementation error - it does not
        // send EOT token, but relies on byte count
        (m_SizeKnown && (m_BlobSize == 0))) {
        m_Writer->Flush();
        m_Writer.reset(0);
        if (m_SizeKnown && (m_BlobSize != 0))
            WriteMsg(GetSocket(), "ERR:",
                     "eCommunicationError:Unexpected EOF");
        _TRACE("EOT " << eot);
        // Forcibly close transmission - client not is going
        // to send us EOT
        return false;
    }
    return true;
}


void CICacheHandler::Process_IC_SetTimeStampPolicy(ICache&           ic,
                                                   CSocket&          sock, 
                                                   SIC_Request&      req)
{
    ic.SetTimeStampPolicy(req.i0, req.i1, req.i2);
    WriteMsg(sock, "OK:", "");
}


void CICacheHandler::Process_IC_GetTimeStampPolicy(ICache&           ic,
                                                   CSocket&          sock, 
                                                   SIC_Request&      req)
{
    ICache::TTimeStampFlags flags = ic.GetTimeStampPolicy();
    string str;
    NStr::UIntToString(str, flags);
    WriteMsg(sock, "OK:", str);
}

void CICacheHandler::Process_IC_SetVersionRetention(ICache&           ic,
                                                    CSocket&          sock, 
                                                    SIC_Request&      req)
{
    ICache::EKeepVersions policy;
    if (NStr::CompareNocase(req.key, "KA") == 0) {
        policy = ICache::eKeepAll;
    } else 
    if (NStr::CompareNocase(req.key, "DO") == 0) {
        policy = ICache::eDropOlder;
    } else 
    if (NStr::CompareNocase(req.key, "DA") == 0) {
        policy = ICache::eDropAll;
    } else {
        WriteMsg(sock, "ERR:", "Invalid version retention code");
        return;
    }
    ic.SetVersionRetention(policy);
    WriteMsg(sock, "OK:", "");
}

void CICacheHandler::Process_IC_GetVersionRetention(ICache&           ic,
                                                    CSocket&          sock, 
                                                    SIC_Request&      req)
{
    int p = ic.GetVersionRetention();
    string str;
    NStr::IntToString(str, p);
    WriteMsg(sock, "OK:", str);
}

void CICacheHandler::Process_IC_GetTimeout(ICache&           ic,
                                           CSocket&          sock, 
                                           SIC_Request&      req)
{
    int to = ic.GetTimeout();
    string str;
    NStr::UIntToString(str, to);
    WriteMsg(sock, "OK:", str);
}

void CICacheHandler::Process_IC_IsOpen(ICache&              ic,
                                       CSocket&              sock, 
                                       SIC_Request&          req)
{
    bool io = ic.IsOpen();
    string str;
    NStr::UIntToString(str, (int)io);
    WriteMsg(sock, "OK:", str);
}


void CICacheHandler::Process_IC_Store(ICache&              ic,
                                      CSocket&             sock, 
                                      SIC_Request&         req)
{
    WriteMsg(sock, "OK:", "");
    m_SizeKnown = false;

    m_Writer.reset(
        ic.GetWriteStream(req.key, req.version, req.subkey,
                          req.i0, *m_Auth));

    m_Host->BeginReadTransmission();
}

void CICacheHandler::Process_IC_StoreBlob(ICache&              ic,
                                          CSocket&             sock, 
                                          SIC_Request&         req)
{
    WriteMsg(sock, "OK:", "");

    size_t blob_size = req.i1;
    if (blob_size == 0) {
        ic.Store(req.key, req.version, req.subkey, 0,
                 0, req.i0, *m_Auth);
        return;
    }

    m_SizeKnown = true;
    m_BlobSize = blob_size;

    m_Writer.reset(
        ic.GetWriteStream(req.key, req.version, req.subkey,
                          req.i0, *m_Auth));

    m_Host->BeginReadTransmission();
}


void CICacheHandler::Process_IC_GetSize(ICache&              ic,
                                        CSocket&             sock,
                                        SIC_Request&         req)
{
    size_t sz = ic.GetSize(req.key, req.version, req.subkey);
    string str;
    NStr::UIntToString(str, sz);
    WriteMsg(sock, "OK:", str);
}

void CICacheHandler::Process_IC_GetBlobOwner(ICache&              ic,
                                             CSocket&             sock, 
                                             SIC_Request&         req)
{
    string owner;
    ic.GetBlobOwner(req.key, req.version, req.subkey, &owner);
    WriteMsg(sock, "OK:", owner);
}

bool CICacheHandler::ProcessWrite()
{
    char buf[4096];
    size_t bytes_read;
    ERW_Result io_res = m_Reader->Read(buf, sizeof(buf), &bytes_read);
    if (io_res != eRW_Success || !bytes_read) {
        m_Reader.reset(0);
        return false;
    }
    CNetCacheServer::WriteBuf(GetSocket(), buf, bytes_read);
    return true;
}


void CICacheHandler::Process_IC_Read(ICache&              ic,
                                     CSocket&             sock, 
                                     SIC_Request&         req)
{
    ICache::SBlobAccessDescr ba_descr;
    ba_descr.buf = 0;
    ba_descr.buf_size = 0;
    ic.GetBlobAccess(req.key, req.version, req.subkey, &ba_descr);

    if (!ba_descr.blob_found) {
        string msg = "BLOB not found. ";
        //msg += req_id;
        WriteMsg(sock, "ERR:", msg);
        return;
    }
    if (ba_descr.blob_size == 0) {
        WriteMsg(sock, "OK:", "BLOB found. SIZE=0");
        return;
    }


    // re-translate reader to the network

    m_Reader.reset(ba_descr.reader.release());
    if (!m_Reader.get()) {
        string msg = "BLOB not found. ";
        //msg += req_id;
        WriteMsg(sock, "ERR:", msg);
        return;
    }
    // Write first chunk right here
    char buf[4096];
    size_t bytes_read;
    ERW_Result io_res = m_Reader->Read(buf, sizeof(buf), &bytes_read);
    if (io_res != eRW_Success || !bytes_read) { // TODO: should we check here for bytes_read?
        string msg = "BLOB not found. ";
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
    CNetCacheServer::WriteBuf(sock, buf, bytes_read);

    // TODO: Can we check here that bytes_read is less than sizeof(buf)
    // and optimize out delayed write?
    m_Host->BeginDelayedWrite();
}

void CICacheHandler::Process_IC_Remove(ICache&              ic,
                                       CSocket&             sock, 
                                       SIC_Request&         req)
{
    ic.Remove(req.key, req.version, req.subkey);
    WriteMsg(sock, "OK:", "");
}

void CICacheHandler::Process_IC_RemoveKey(ICache&              ic,
                                          CSocket&             sock, 
                                          SIC_Request&         req)
{
    ic.Remove(req.key);
    WriteMsg(sock, "OK:", "");
}


void CICacheHandler::Process_IC_GetAccessTime(ICache&              ic,
                                              CSocket&             sock, 
                                              SIC_Request&         req)
{
    time_t t = ic.GetAccessTime(req.key, req.version, req.subkey);
    string str;
    NStr::UIntToString(str, t);
    WriteMsg(sock, "OK:", str);
}


void CICacheHandler::Process_IC_HasBlobs(ICache&              ic,
                                         CSocket&             sock, 
                                         SIC_Request&         req)
{
    bool hb = ic.HasBlobs(req.key, req.subkey);
    string str;
    NStr::UIntToString(str, (int)hb);
    WriteMsg(sock, "OK:", str);
}


void CICacheHandler::Process_IC_Purge1(ICache&              ic,
                                       CSocket&             sock, 
                                       SIC_Request&         req)
{
    ICache::EKeepVersions keep_versions = (ICache::EKeepVersions) req.i1;
    ic.Purge(req.i0, keep_versions);
    WriteMsg(sock, "OK:", "");
}


END_NCBI_SCOPE
