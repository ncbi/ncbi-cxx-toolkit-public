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


CICacheHandler::CICacheHandler(CNetCache_RequestHandlerHost* host)
    : CNetCache_RequestHandler(host),
      m_SizeKnown(false),
      m_BlobSize(0)
{
#define IC_PROCESSOR(func, cnt)   SProcessorInfo(&CICacheHandler::func, cnt);

    m_Processors["STOR"] = IC_PROCESSOR(Process_IC_Store,               4);
    m_Processors["STRS"] = IC_PROCESSOR(Process_IC_StoreBlob,           5);
    m_Processors["GSIZ"] = IC_PROCESSOR(Process_IC_GetSize,             3);
    m_Processors["GBLW"] = IC_PROCESSOR(Process_IC_GetBlobOwner,        3);
    m_Processors["READ"] = IC_PROCESSOR(Process_IC_Read,                3);
    m_Processors["REMO"] = IC_PROCESSOR(Process_IC_Remove,              3);
    m_Processors["REMK"] = IC_PROCESSOR(Process_IC_RemoveKey,           1);
    m_Processors["GACT"] = IC_PROCESSOR(Process_IC_GetAccessTime,       3);
    m_Processors["HASB"] = IC_PROCESSOR(Process_IC_HasBlobs,            3);
    m_Processors["GACT"] = IC_PROCESSOR(Process_IC_GetAccessTime,       3);
    m_Processors["STSP"] = IC_PROCESSOR(Process_IC_SetTimeStampPolicy,  3);
    m_Processors["GTSP"] = IC_PROCESSOR(Process_IC_GetTimeStampPolicy,  0);
    m_Processors["GTOU"] = IC_PROCESSOR(Process_IC_GetTimeout,          0);
    m_Processors["ISOP"] = IC_PROCESSOR(Process_IC_IsOpen,              0);
    m_Processors["SVRP"] = IC_PROCESSOR(Process_IC_SetVersionRetention, 1);
    m_Processors["GVRP"] = IC_PROCESSOR(Process_IC_GetVersionRetention, 1);
    m_Processors["PRG1"] = IC_PROCESSOR(Process_IC_Purge1,              2);

#undef IC_PROCESSOR
}

void
CICacheHandler::x_ParseKeyVersion(const CNCRequestParser& parser,
                                  size_t                  param_num,
                                  SNetCache_RequestStat&  stat)
{
    m_Key     = parser.GetParam    (param_num++);
    m_Version = parser.GetUIntParam(param_num++);
    m_SubKey  = parser.GetParam    (param_num++);

    stat.blob_id = m_Key + ":"
                   + NStr::IntToString(m_Version) + ":"
                   + m_SubKey;
    stat.details = "Cache=\"";
    stat.details += m_CacheName + "\"";
    stat.details += " key=\"" + m_Key;
    stat.details += "\" version=" + NStr::IntToString(m_Version);
    stat.details += " subkey=\"" + m_SubKey +"\"";
}

void
CICacheHandler::ProcessRequest(CNCRequestParser&      parser,
                               SNetCache_RequestStat& stat)
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


    stat.type = "IC";
    stat.op_code = SBDB_CacheUnitStatistics::eErr_Unknown;

    const string& cmd = parser.GetCommand();
    parser.ShiftParams();

    if (cmd[0] != 'I'  ||  cmd[1] != 'C'  ||  cmd[2] != '('
        ||  cmd[cmd.size() - 1] != ')')
    {
        NCBI_THROW(CNCReqParserException, eNotCommand,
                   "Invalid IC request: '" + cmd + "'");
    }

    m_CacheName = cmd.substr(3, cmd.size() - 4);

    ICache* ic = m_Server->GetLocalCache(m_CacheName);
    if (ic == 0) {
        string err_msg = "Cache unknown: ";
        err_msg.append(m_CacheName);
        WriteMsg(GetSocket(), "ERR:", err_msg);
        return;
    }

    stat.details = "Cache=\"";
    stat.details += m_CacheName + "\"";

    string cmd_name = parser.GetCommand();

    TProcessorsMap::iterator it = m_Processors.find(cmd_name);
    if (it == m_Processors.end()) {
        NCBI_THROW(CNCReqParserException, eNotCommand,
                   "Invalid IC command: '" + cmd_name + "'");
    }

    size_t need_cnt = it->second.params_cnt;
    CheckParamsCount(parser, need_cnt);

    (this->*it->second.func)(*ic, parser, stat);
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
        // Forcibly close transmission - client is not going
        // to send us EOT
        return false;
    }
    return true;
}


void
CICacheHandler::Process_IC_SetTimeStampPolicy(ICache&                 ic,
                                              const CNCRequestParser& parser,
                                              SNetCache_RequestStat&  stat)
{
    ic.SetTimeStampPolicy(parser.GetUIntParam(0),
                          parser.GetUIntParam(1),
                          parser.GetUIntParam(2));
    WriteMsg(GetSocket(), "OK:", "");
}


void
CICacheHandler::Process_IC_GetTimeStampPolicy(ICache&                 ic,
                                              const CNCRequestParser& parser,
                                              SNetCache_RequestStat&  stat)
{
    ICache::TTimeStampFlags flags = ic.GetTimeStampPolicy();
    string str;
    NStr::UIntToString(str, flags);
    WriteMsg(GetSocket(), "OK:", str);
}

void
CICacheHandler::Process_IC_SetVersionRetention(ICache&                 ic,
                                               const CNCRequestParser& parser,
                                               SNetCache_RequestStat&  stat)
{
    const string& key = parser.GetParam(0);
    ICache::EKeepVersions policy;
    if (NStr::CompareNocase(key, "KA") == 0) {
        policy = ICache::eKeepAll;
    } else 
    if (NStr::CompareNocase(key, "DO") == 0) {
        policy = ICache::eDropOlder;
    } else 
    if (NStr::CompareNocase(key, "DA") == 0) {
        policy = ICache::eDropAll;
    } else {
        WriteMsg(GetSocket(), "ERR:", "Invalid version retention code");
        return;
    }
    ic.SetVersionRetention(policy);
    WriteMsg(GetSocket(), "OK:", "");
}

void
CICacheHandler::Process_IC_GetVersionRetention(ICache&                 ic,
                                               const CNCRequestParser& parser,
                                               SNetCache_RequestStat&  stat)
{
    int p = ic.GetVersionRetention();
    string str;
    NStr::IntToString(str, p);
    WriteMsg(GetSocket(), "OK:", str);
}

void
CICacheHandler::Process_IC_GetTimeout(ICache&                 ic,
                                      const CNCRequestParser& parser,
                                      SNetCache_RequestStat&  stat)
{
    int to = ic.GetTimeout();
    string str;
    NStr::UIntToString(str, to);
    WriteMsg(GetSocket(), "OK:", str);
}

void
CICacheHandler::Process_IC_IsOpen(ICache&                 ic,
                                  const CNCRequestParser& parser,
                                  SNetCache_RequestStat&  stat)
{
    bool io = ic.IsOpen();
    string str;
    NStr::UIntToString(str, (int)io);
    WriteMsg(GetSocket(), "OK:", str);
}


void
CICacheHandler::Process_IC_Store(ICache&                 ic,
                                 const CNCRequestParser& parser,
                                 SNetCache_RequestStat&  stat)
{
    unsigned int ttl = parser.GetUIntParam(0);
    x_ParseKeyVersion(parser, 1, stat);

    WriteMsg(GetSocket(), "OK:", "");
    m_SizeKnown = false;

    m_Writer.reset(
        ic.GetWriteStream(m_Key, m_Version, m_SubKey, ttl, *m_Auth));

    m_Host->BeginReadTransmission();
}

void
CICacheHandler::Process_IC_StoreBlob(ICache&                 ic,
                                     const CNCRequestParser& parser,
                                     SNetCache_RequestStat&  stat)
{
    unsigned int ttl = parser.GetUIntParam(0);
    m_BlobSize = parser.GetUIntParam(1);
    x_ParseKeyVersion(parser, 2, stat);

    WriteMsg(GetSocket(), "OK:", "");

    if (m_BlobSize == 0) {
        ic.Store(m_Key, m_Version, m_SubKey, 0, 0, ttl, *m_Auth);
        return;
    }

    m_SizeKnown = true;

    m_Writer.reset(
        ic.GetWriteStream(m_Key, m_Version, m_SubKey, ttl, *m_Auth));

    m_Host->BeginReadTransmission();
}


void
CICacheHandler::Process_IC_GetSize(ICache&                 ic,
                                   const CNCRequestParser& parser,
                                   SNetCache_RequestStat&  stat)
{
    x_ParseKeyVersion(parser, 0, stat);

    size_t sz = ic.GetSize(m_Key, m_Version, m_SubKey);
    string str;
    NStr::UIntToString(str, sz);
    WriteMsg(GetSocket(), "OK:", str);
}

void
CICacheHandler::Process_IC_GetBlobOwner(ICache&                 ic,
                                        const CNCRequestParser& parser,
                                        SNetCache_RequestStat&  stat)
{
    x_ParseKeyVersion(parser, 0, stat);

    string owner;
    ic.GetBlobOwner(m_Key, m_Version, m_SubKey, &owner);
    WriteMsg(GetSocket(), "OK:", owner);
}

bool
CICacheHandler::ProcessWrite()
{
    char buf[4096];
    size_t bytes_read;
    ERW_Result res;
    {{
        CTimeGuard time_guard(m_Stat->db_elapsed);
        res = m_Reader->Read(buf, sizeof(buf), &bytes_read);
    }}
    if (res != eRW_Success || !bytes_read) {
        m_Reader.reset(0);
        return false;
    }
    {{
        CTimeGuard time_guard(m_Stat->comm_elapsed);
        CNetCacheServer::WriteBuf(GetSocket(), buf, bytes_read);
    }}
    return true;
}


void
CICacheHandler::Process_IC_Read(ICache&                 ic,
                                const CNCRequestParser& parser,
                                SNetCache_RequestStat&  stat)
{
    x_ParseKeyVersion(parser, 0, stat);

    ICache::SBlobAccessDescr ba_descr;
    ba_descr.buf = 0;
    ba_descr.buf_size = 0;
    ic.GetBlobAccess(m_Key, m_Version, m_SubKey, &ba_descr);

    if (!ba_descr.blob_found) {
        string msg = "BLOB not found. ";
        //msg += req_id;
        WriteMsg(GetSocket(), "ERR:", msg);
        return;
    }
    if (ba_descr.blob_size == 0) {
        WriteMsg(GetSocket(), "OK:", "BLOB found. SIZE=0");
        return;
    }
 
    // re-translate reader to the network

    m_Reader.reset(ba_descr.reader.release());
    if (!m_Reader.get()) {
        string msg = "BLOB not found. ";
        //msg += req_id;
        WriteMsg(GetSocket(), "ERR:", msg);
        return;
    }
    // Write first chunk right here
    char buf[4096];
    size_t bytes_read;
    ERW_Result io_res = m_Reader->Read(buf, sizeof(buf), &bytes_read);
    if (io_res != eRW_Success || !bytes_read) { // TODO: should we check here for bytes_read?
        string msg = "BLOB not found. ";
        WriteMsg(GetSocket(), "ERR:", msg);
        m_Reader.reset(0);
        return;
    }
    string msg("BLOB found. SIZE=");
    string sz;
    NStr::UIntToString(sz, ba_descr.blob_size);
    msg += sz;
    WriteMsg(GetSocket(), "OK:", msg);
    
    // translate BLOB fragment to the network
    CNetCacheServer::WriteBuf(GetSocket(), buf, bytes_read);

    // TODO: Can we check here that bytes_read is less than sizeof(buf)
    // and optimize out delayed write?
    m_Host->BeginDelayedWrite();
}

void
CICacheHandler::Process_IC_Remove(ICache&                 ic,
                                  const CNCRequestParser& parser,
                                  SNetCache_RequestStat&  stat)
{
    x_ParseKeyVersion(parser, 0, stat);

    ic.Remove(m_Key, m_Version, m_SubKey);
    WriteMsg(GetSocket(), "OK:", "");
}

void
CICacheHandler::Process_IC_RemoveKey(ICache&                 ic,
                                     const CNCRequestParser& parser,
                                     SNetCache_RequestStat&  stat)
{
    ic.Remove(parser.GetParam(0));
    WriteMsg(GetSocket(), "OK:", "");
}


void
CICacheHandler::Process_IC_GetAccessTime(ICache&                 ic,
                                         const CNCRequestParser& parser,
                                         SNetCache_RequestStat&  stat)
{
    x_ParseKeyVersion(parser, 0, stat);

    time_t t = ic.GetAccessTime(m_Key, m_Version, m_SubKey);
    string str;
    NStr::UIntToString(str, static_cast<unsigned long>(t));
    WriteMsg(GetSocket(), "OK:", str);
}


void
CICacheHandler::Process_IC_HasBlobs(ICache&                 ic,
                                    const CNCRequestParser& parser,
                                    SNetCache_RequestStat&  stat)
{
    x_ParseKeyVersion(parser, 0, stat);

    bool hb = ic.HasBlobs(m_Key, m_SubKey);
    string str;
    NStr::UIntToString(str, (int)hb);
    WriteMsg(GetSocket(), "OK:", str);
}


void
CICacheHandler::Process_IC_Purge1(ICache&                 ic,
                                  const CNCRequestParser& parser,
                                  SNetCache_RequestStat&  stat)
{
    ICache::EKeepVersions keep_versions
                             = (ICache::EKeepVersions)parser.GetUIntParam(0);
    ic.Purge(parser.GetUIntParam(1), keep_versions);
    WriteMsg(GetSocket(), "OK:", "");
}


END_NCBI_SCOPE
