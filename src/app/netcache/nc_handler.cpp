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

#include <connect/services/netcache_key.hpp>

#include "nc_handler.hpp"
#include "netcache_version.hpp"


BEGIN_NCBI_SCOPE


const size_t kVariableParamsCnt = (size_t)-1;


CNetCacheHandler::CNetCacheHandler(CNetCache_RequestHandlerHost* host)
    : CNetCache_RequestHandler(host),
      m_PutOK(false)
{
#define NC_PROCESSOR(func, cnt)  SProcessorInfo(&CNetCacheHandler::func, cnt);

    // All accepted commands
    m_Processors["PUT"     ] = NC_PROCESSOR(ProcessPut,  kVariableParamsCnt);
    m_Processors["PUT2"    ] = NC_PROCESSOR(ProcessPut2, kVariableParamsCnt);
    m_Processors["PUT3"    ] = NC_PROCESSOR(ProcessPut3, kVariableParamsCnt);
    m_Processors["GET"     ] = NC_PROCESSOR(ProcessGet,  kVariableParamsCnt);
    m_Processors["GET2"    ] = NC_PROCESSOR(ProcessGet,  kVariableParamsCnt);
    m_Processors["GETCONF" ] = NC_PROCESSOR(ProcessGetConfig,    0);
    m_Processors["GETSTAT" ] = NC_PROCESSOR(ProcessGetStat,      0);
    m_Processors["GSIZ"    ] = NC_PROCESSOR(ProcessGetSize,      1);
    m_Processors["GBOW"    ] = NC_PROCESSOR(ProcessGetBlobOwner, 1);
    m_Processors["HASB"    ] = NC_PROCESSOR(ProcessHasBlob,      1);
    m_Processors["ISLK"    ] = NC_PROCESSOR(ProcessIsLock,       1);
    m_Processors["REMOVE"  ] = NC_PROCESSOR(ProcessRemove,       1);
    m_Processors["RMV2"    ] = NC_PROCESSOR(ProcessRemove2,      1);
    m_Processors["DROPSTAT"] = NC_PROCESSOR(ProcessDropStat,     0);
    m_Processors["SHUTDOWN"] = NC_PROCESSOR(ProcessShutdown,     0);
    m_Processors["STAT"    ] = NC_PROCESSOR(ProcessStat,         1);
    m_Processors["VERSION" ] = NC_PROCESSOR(ProcessVersion,      0);
    m_Processors["LOG"     ] = NC_PROCESSOR(ProcessLog,          1);

#undef NC_PROCESSOR
}

void
CNetCacheHandler::x_CheckBlobIdParam(const string& req_id)
{
    if (req_id.empty()) {
        NCBI_THROW(CNCReqParserException, eWrongParams,
                   "Error in command parameter: BLOB id is empty");
    }

    try {
        CNetCacheKey blob_id(req_id);
        if (blob_id.GetVersion() != 1  ||
            blob_id.GetHost().empty()  ||
            blob_id.GetId() == 0       ||
            blob_id.GetPort() == 0) 
        {
            NCBI_THROW(CNCReqParserException, eWrongParams,
                       "BLOB id format error.");
        }
    }
    catch (CNetCacheException& ex) {
        NCBI_THROW(CNCReqParserException, eWrongParams,
                   string("BLOB id format error: ") + ex.what());
    }
}

void CNetCacheHandler::ProcessRequest(CNCRequestParser&      parser,
                                      SNetCache_RequestStat& stat)
{
    stat.type = "NC";
    stat.op_code = SBDB_CacheUnitStatistics::eErr_Unknown;

    TProcessorsMap::iterator it = m_Processors.find(parser.GetCommand());
    if (it == m_Processors.end()) {
        NCBI_THROW(CNCReqParserException, eNotCommand,
                   "Invalid command for NetCache: " + parser.GetCommand());
    }

    size_t need_cnt = it->second.params_cnt;
    if (need_cnt != kVariableParamsCnt) {
        CheckParamsCount(parser, need_cnt);
    }

    (this->*it->second.func)(parser, stat);

    stat.details = stat.blob_id;
}


void
CNetCacheHandler::ProcessShutdown(const CNCRequestParser& /* parser */,
                                  SNetCache_RequestStat&  /* stat */)
{
    m_Server->SetShutdownFlag();
    WriteMsg(GetSocket(), "OK:", "");
}

void
CNetCacheHandler::ProcessVersion(const CNCRequestParser& /* parser */,
                                 SNetCache_RequestStat&  /* stat */)
{
    WriteMsg(GetSocket(), "OK:", NETCACHED_VERSION); 
}

void
CNetCacheHandler::ProcessGetConfig(const CNCRequestParser& /* parser */,
                                   SNetCache_RequestStat&  /* stat */)
{
    CSocket& sock = GetSocket();
    SOCK sk = sock.GetSOCK();
    sock.SetOwnership(eNoOwnership);
    sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sk);
    m_Server->GetRegistry().Write(ios);
}

void
CNetCacheHandler::ProcessGetStat(const CNCRequestParser& /* parser */,
                                 SNetCache_RequestStat&  /* stat */)
{
    //CNcbiRegistry reg;

    CSocket& sock = GetSocket();
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

void
CNetCacheHandler::ProcessDropStat(const CNCRequestParser& /* parser */,
                                  SNetCache_RequestStat&  /* stat */)
{
    CBDB_Cache* bdb_cache = m_Server->GetCache();
    if (!bdb_cache) {
        return;
    }
	bdb_cache->InitStatistics();
    WriteMsg(GetSocket(), "OK:", "");
}


void
CNetCacheHandler::ProcessRemove(const CNCRequestParser& parser,
                                SNetCache_RequestStat&  /* stat */)
{
    const string& req_id = parser.GetParam(0);
    x_CheckBlobIdParam(req_id);
    m_Server->GetCache()->Remove(req_id);
}


void
CNetCacheHandler::ProcessRemove2(const CNCRequestParser& parser,
                                 SNetCache_RequestStat&  stat)
{
    ProcessRemove(parser, stat);
    WriteMsg(GetSocket(), "OK:", "");
}


void
CNetCacheHandler::ProcessLog(const CNCRequestParser& parser,
                             SNetCache_RequestStat&  /* stat */)
{
    const char* str = parser.GetParam(0).c_str();
    int level = CNetCacheServer::kLogLevelBase;
    if (NStr::strcasecmp(str, "ON") == 0)
        level = CNetCacheServer::kLogLevelMax;
    else if (NStr::strcasecmp(str, "OFF") == 0)
        level = CNetCacheServer::kLogLevelBase;
    else if (NStr::strcasecmp(str, "Operation") == 0 ||
             NStr::strcasecmp(str, "Op") == 0)
        level = CNetCacheServer::kLogLevelOp;
    else if (NStr::strcasecmp(str, "Request") == 0)
        level = CNetCacheServer::kLogLevelRequest;
    else {
        try {
            level = NStr::StringToInt(str);
        } catch (...) {
            NCBI_THROW(CNCReqParserException, eWrongParams,
                       "NetCache Log command: invalid parameter");
        }
    }
    m_Server->SetLogLevel(level);
    WriteMsg(GetSocket(), "OK:", "");
}


void
CNetCacheHandler::ProcessStat(const CNCRequestParser& parser,
                              SNetCache_RequestStat&  /* stat */)
{
    CBDB_Cache* bdb_cache = m_Server->GetCache();
    if (!bdb_cache) {
        return;
    }

    const char* str = parser.GetParam(0).c_str();
    if (NStr::strcasecmp(str, "ON") == 0)
        bdb_cache->SetSaveStatistics(true);
    else if (NStr::strcasecmp(str, "OFF") == 0)
        bdb_cache->SetSaveStatistics(false);
    else {
        NCBI_THROW(CNCReqParserException, eWrongParams,
                   "NetCache Stat command: invalid parameter");
    }

    WriteMsg(GetSocket(), "OK:", "");
}


bool CNetCacheHandler::ProcessWrite()
{
    return ProcessWriteAndReport(0);
}


bool CNetCacheHandler::ProcessWriteAndReport(unsigned blob_size,
                                             const string* req_id)
{
    char buf[kNetworkBufferSize];
    size_t bytes_read;
    ERW_Result res;
    {{
        CTimeGuard time_guard(m_Stat->db_elapsed, m_Stat);
        res = m_Reader->Read(buf, sizeof(buf), &bytes_read);
    }}
    if (res != eRW_Success || !bytes_read) {
        if (blob_size) {
            string msg("BLOB not found. ");
            if (req_id) msg += *req_id;
            WriteMsg(GetSocket(), "ERR:", msg);
        }
        m_Reader.reset(0);
        return false;
    }
    if (blob_size) {
        string msg("BLOB found. SIZE=");
        string sz;
        NStr::UIntToString(sz, blob_size);
        msg += sz;
        WriteMsg(GetSocket(), "OK:", msg);
    }
    {{
        CTimeGuard time_guard(m_Stat->comm_elapsed, m_Stat);
        CNetCacheServer::WriteBuf(GetSocket(), buf, bytes_read);
    }}
    ++(m_Stat->io_blocks);

    // TODO: Check here that bytes_read is less than sizeof(buf)
    // and optimize out delayed write?
    // This code does not work properly, that is why it is commented
//    if (blob_size && (blob_size <= bytes_read))
//        return false;

    return true;
}


void
CNetCacheHandler::ProcessGet(const CNCRequestParser& parser,
                             SNetCache_RequestStat&  stat)
{
    stat.op_code = SBDB_CacheUnitStatistics::eErr_Get;

    size_t params_cnt = parser.GetParamsCount();
    if (params_cnt == 0) {
        NCBI_THROW(CNCReqParserException, eWrongParamsCnt,
                   "NetCache Get command: not enough parameters");
    }

    const string& req_id = parser.GetParam(0);
    x_CheckBlobIdParam(req_id);

    stat.blob_id = req_id;
    int param_ind = 1;
    int no_lock = false;

    if (NStr::strcasecmp(parser.GetParam(param_ind).c_str(), "NW") == 0) {
        no_lock = true;
        ++param_ind;
    }

    CheckParamsCount(parser, param_ind);

    if (no_lock) {
        bool locked = m_Server->GetCache()->IsLocked(req_id, 0, kEmptyStr);
        if (locked) {
            WriteMsg(GetSocket(), "ERR:", "BLOB locked by another client"); 
            return;
        }
    }

    ICache::SBlobAccessDescr ba_descr;
    ba_descr.buf = 0;
    ba_descr.buf_size = 0;

    unsigned blob_id = CNetCacheKey::GetBlobId(req_id);

    CBDB_Cache* bdb_cache = m_Server->GetCache();
    for (int repeats = 0; repeats < 1000; ++repeats) {
        CTimeGuard time_guard(stat.db_elapsed, &stat);
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
            WriteMsg(GetSocket(), "ERR:", string("BLOB not found. ") + req_id);
            return;
        } else {
            break;
        }
    }

    if (ba_descr.blob_size == 0) {
        WriteMsg(GetSocket(), "OK:", "BLOB found. SIZE=0");
        return;
    }

    stat.blob_size = ba_descr.blob_size;

    // re-translate reader to the network
    m_Reader.reset(ba_descr.reader.release());
    if (!m_Reader.get()) {
        WriteMsg(GetSocket(), "ERR:", string("BLOB not found. ") + req_id);
        return;
    }

    // Write first chunk right here
    if (!ProcessWriteAndReport(ba_descr.blob_size, &req_id))
        return;

    m_Host->BeginDelayedWrite();
}


void
CNetCacheHandler::ProcessPut(const CNCRequestParser& /* parser */,
                             SNetCache_RequestStat&  /* stat */)
{
    WriteMsg(GetSocket(), "ERR:", "Obsolete");
}


void
CNetCacheHandler::ProcessPut2(const CNCRequestParser& parser,
                              SNetCache_RequestStat&  stat)
{
    stat.op_code = SBDB_CacheUnitStatistics::eErr_Put;

    size_t params_cnt = parser.GetParamsCount();
    size_t param_ind = 0;
    int timeout = 0;
    if (params_cnt > param_ind) {
        timeout = parser.GetIntParam(param_ind);
        ++param_ind;
    }

    string rid;
    if (params_cnt > param_ind) {
        rid = parser.GetParam(param_ind);
        if (CNetCacheKey::IsValidKey(rid)) {
            ++param_ind;
        }
        else {
            rid.clear();
        }
    }

    CheckParamsCount(parser, param_ind);

    bool do_id_lock = true;

    CBDB_Cache* bdb_cache = m_Server->GetCache();
    unsigned int id = 0;
    _TRACE("Getting an id, socket " << &GetSocket());
    if (rid.empty()) {
        CTimeGuard time_guard(stat.db_elapsed, &stat);
        id = bdb_cache->GetNextBlobId(do_id_lock);
        time_guard.Stop();
        CNetCacheKey::GenerateBlobKey(&rid, id,
            m_Server->GetHost(), m_Server->GetPort());
        do_id_lock = false;
    } else {
        id = CNetCacheKey::GetBlobId(rid);
    }
    stat.blob_id = rid;
    _TRACE("Got id " << id);

    // BLOB already locked, it is safe to return BLOB id
    if (!do_id_lock) {
        CTimeGuard time_guard(stat.comm_elapsed, &stat);
        WriteMsg(GetSocket(), "ID:", rid);
    }

    // create the reader up front to guarantee correct BLOB locking
    // the possible problem (?) here is that we have to do double buffering
    // of the input stream
    {{
        CTimeGuard time_guard(stat.db_elapsed, &stat);
        m_Writer.reset(
            bdb_cache->GetWriteStream(id, rid, 0, kEmptyStr, do_id_lock,
                                      timeout, *m_Auth));
    }}

    if (do_id_lock) {
        CTimeGuard time_guard(stat.comm_elapsed, &stat);
        WriteMsg(GetSocket(), "ID:", rid);
    }

    _TRACE("Begin read transmission");
    m_Host->BeginReadTransmission();
}


bool
CNetCacheHandler::ProcessTransmission(const char*   buf,
                                      size_t        buf_size,
                                      ETransmission eot)
{
    size_t bytes_written;
    ERW_Result res;
    {{
        CTimeGuard time_guard(m_Stat->db_elapsed, m_Stat);
        res = m_Writer->Write(buf, buf_size, &bytes_written);
    }}
    m_Stat->blob_size += buf_size;
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
        {{
            CTimeGuard time_guard(m_Stat->db_elapsed, m_Stat);
            m_Writer->Flush();
            m_Writer.reset(0);
        }}
        if (m_PutOK) {
            _TRACE("OK, socket " << &GetSocket());
            CTimeGuard time_guard(m_Stat->comm_elapsed, m_Stat);
            WriteMsg(GetSocket(), "OK:", "");
            m_PutOK = false;
        }
    }
    return true;
}


void
CNetCacheHandler::ProcessPut3(const CNCRequestParser& parser,
                              SNetCache_RequestStat&  stat)
{
    m_PutOK = true;
    ProcessPut2(parser, stat);
}


void
CNetCacheHandler::ProcessHasBlob(const CNCRequestParser& parser,
                                 SNetCache_RequestStat&  /* stat */)
{
    const string& req_id = parser.GetParam(0);
    x_CheckBlobIdParam(req_id);

    bool hb = m_Server->GetCache()->HasBlobs(req_id, kEmptyStr);
    string str;
    NStr::UIntToString(str, (int)hb);
    WriteMsg(GetSocket(), "OK:", str);
}


void
CNetCacheHandler::ProcessGetBlobOwner(const CNCRequestParser& parser,
                                      SNetCache_RequestStat&  /* stat */)
{
    const string& req_id = parser.GetParam(0);
    x_CheckBlobIdParam(req_id);

    string owner;
    m_Server->GetCache()->GetBlobOwner(req_id, 0, kEmptyStr, &owner);

    WriteMsg(GetSocket(), "OK:", owner);
}


void
CNetCacheHandler::ProcessIsLock(const CNCRequestParser& parser,
                                SNetCache_RequestStat&  /* stat */)
{
    const string& req_id = parser.GetParam(0);
    x_CheckBlobIdParam(req_id);

    if (m_Server->GetCache()->IsLocked(CNetCacheKey::GetBlobId(req_id))) {
        WriteMsg(GetSocket(), "OK:", "1");
    } else {
        WriteMsg(GetSocket(), "OK:", "0");
    }

}


void
CNetCacheHandler::ProcessGetSize(const CNCRequestParser& parser,
                                 SNetCache_RequestStat&  /* stat */)
{
    const string& req_id = parser.GetParam(0);
    x_CheckBlobIdParam(req_id);

    size_t size;
    if (!m_Server->GetCache()->GetSizeEx(req_id, 0, kEmptyStr, &size)) {
        WriteMsg(GetSocket(), "ERR:", "BLOB not found. " + req_id);
    } else {
        WriteMsg(GetSocket(), "OK:", NStr::UIntToString(size));
    }
}


END_NCBI_SCOPE
