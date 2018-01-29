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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include <string>
#include <memory>
#include <sstream>
#include <cstdlib>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>

#include <objtools/pubseq_gateway/diag/AppLog.hpp>
#include <objtools/pubseq_gateway/diag/AppPerf.hpp>
#include <objtools/pubseq_gateway/cassandra/CassBlobOp.hpp>
#include <objtools/pubseq_gateway/cassandra/CassFactory.hpp>

#include <objtools/pubseq_gateway/rpc/UtilException.hpp>
#include <objtools/pubseq_gateway/rpc/HttpServerTransport.hpp>
#include <objtools/pubseq_gateway/rpc/DdRpcDataPacker.hpp>

#include "AccVerCacheDB.hpp"


USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;
using namespace IdLogUtil;

class CPendingOperation {
public:
    CPendingOperation(const CPendingOperation&) = delete;
    CPendingOperation& operator=(const CPendingOperation&) = delete;
    CPendingOperation(CPendingOperation&&) = default;
    CPendingOperation& operator=(CPendingOperation&&) = default;

    CPendingOperation(string&& sat_name, int sat_key, shared_ptr<CCassConnection> Conn, unsigned int timeout) :
        m_reply(nullptr),
        m_cancelled(false),
        m_finished_read(false),
        m_sat_key(sat_key)
    {
        m_loader.reset(new CCassBlobLoader(&m_op, timeout, Conn, std::move(sat_name), sat_key, true, true, nullptr,  nullptr));
    }

    ~CPendingOperation() {
        LOG5(("CPendingOperation::CPendingOperation: this: %p, m_loader: %p", this, m_loader.get()));
    }

    void Clear() {
        LOG5(("CPendingOperation::Clear: this: %p, m_loader: %p", this, m_loader.get()));
        if (m_loader) {
            m_loader->SetDataReadyCB(nullptr, nullptr);
            m_loader->SetErrorCB(nullptr);
            m_loader->SetDataChunkCB(nullptr);
            m_loader = nullptr;
        }
        m_chunks.clear();
        m_reply = nullptr;
        m_cancelled = false;
        m_finished_read = false;
    }

    void Start(HST::CHttpReply<CPendingOperation>& resp) {
        m_reply = &resp;

        m_loader->SetDataReadyCB(HST::CHttpReply<CPendingOperation>::s_DataReady, &resp);
        m_loader->SetErrorCB(
            [&resp](const char* Text, CCassBlobWaiter* Waiter) {
                ERRLOG1(("%s", Text));
            }
        );
        m_loader->SetDataChunkCB(
            [this, &resp](const unsigned char* Data, unsigned int Size, int ChunkNo, bool IsLast) {
                LOG3(("Chunk: [%d]: %u", ChunkNo, Size));
                assert(!m_finished_read);
                if (m_cancelled) {
                    m_loader->Cancel();
                    m_finished_read = true;
                    return;
                }
                if (resp.IsFinished()) {
                    ERRLOG1(("Unexpected data received while the output has finished, ignoring"));
                    return;
                }
                if (resp.GetState() == HST::CHttpReply<CPendingOperation>::stInitialized) {
                    resp.SetContentLength(m_loader->GetBlobSize());
                }
                if (Data && Size > 0)
                    m_chunks.push_back(resp.PrepadeChunk(Data, Size));
                if (IsLast)
                    m_finished_read = true;
                if (resp.IsOutputReady())
                    Peek(resp);
            }
        );

        m_loader->Wait();
    }

    void Peek(HST::CHttpReply<CPendingOperation>& resp) {
        if (m_cancelled) {
            if (resp.IsOutputReady() && !resp.IsFinished()) {
                resp.Send(nullptr, 0, true, true);
            }
            return;
        }
        // 1 -> call m_loader->Wait1 to pick data
        // 2 -> check if we have ready-to-send buffers
        // 3 -> call resp->  to send what we have if it is ready
        if (m_loader) {
            m_loader->Wait();
            if (m_loader->HasError() && resp.IsOutputReady() && !resp.IsFinished()) {
                resp.Send503("error", m_loader->LastError().c_str());
                return;
            }
        }

        if (resp.IsOutputReady() && (!m_chunks.empty() || m_finished_read)) {
            resp.Send(m_chunks, m_finished_read);
            m_chunks.clear();
        }

/*
                void *dest = m_buf.Reserve(Size);
                if (resp.IsReady()) {

                    size_t sz = m_len;
                    if (sz > sizeof(m_buf))
                        sz = sizeof(m_buf);
                    bool is_final = (m_len == sz);
                    resp.SetOnReady(nullptr);
                    resp.Send(m_buf, sz, true, is_final);
                    m_len -= sz;
                    if (!is_final) {
                        if (!m_wake)
                            EAccVerException::raise("WakeCB is not assigned");
                        m_wake();
                    }
                }
                else {
                    if (!m_wake)
                        EAccVerException::raise("WakeCB is not assigned");
                    resp.SetOnReady(m_wake);
                }
*/

    }

    void Cancel() {
        m_cancelled = true;
    }

private:
    HST::CHttpReply<CPendingOperation>* m_reply;
    bool m_cancelled;
    bool m_finished_read;
    const int m_sat_key;
    CAppOp m_op;
    unique_ptr<CCassBlobLoader> m_loader;
    vector<h2o_iovec_t> m_chunks;
};

class CAccVerCacheDaemonApplication: public CNcbiApplication {
    string m_ini_file;
    string m_db_path;
    string m_log_file;
    unsigned int m_log_level;
    unsigned int m_log_level_file;
    string m_address;
    vector<string> m_sat_names;

    unsigned short m_http_port;
    unsigned short m_http_workers;
    unsigned int m_lstnr_backlog;
    unsigned short m_tcp_max_conn;
    unsigned short m_http_max_backlog;
    unsigned short m_http_max_pending;
//    unique_ptr<CAppVerCacheDaemon> m_daemon;
    unique_ptr<CAccVerCacheDB> m_db;
    shared_ptr<CCassConnection> m_cass_conn;
    shared_ptr<CCassConnectionFactory> m_cass_fact;
    unsigned int m_timeout_ms;
public:
    CAccVerCacheDaemonApplication() :
        m_log_level(0),
        m_log_level_file(1),
        m_http_port(2180),
        m_http_workers(32),
        m_lstnr_backlog(256),
        m_tcp_max_conn(4096),
        m_http_max_backlog(4096),
        m_http_max_pending(512),
        m_cass_fact(CCassConnectionFactory::Create()),
        m_timeout_ms(30000)
    {
    }
    virtual void Init() {
        unique_ptr<CArgDescriptions> argdesc(new CArgDescriptions());
        m_cass_fact->AppInit(argdesc.get());

        argdesc->SetUsageContext(GetArguments().GetProgramBasename(), "AccVerCacheD -- Daemon to service Accession.Version Cache requests");
        argdesc->AddDefaultKey ( "ini", "IniFile",      "File with configuration information",  CArgDescriptions::eString, "AccVerCacheD.ini");
        argdesc->AddDefaultKey ( "P",   "port",         "HTTP port",                            CArgDescriptions::eInteger, "2180");
        argdesc->SetConstraint(  "P",   new CArgAllow_Integers(1, 65534));
        argdesc->AddDefaultKey ( "H",   "Address",      "Address to bind listening socket to, 0.0.0.0 by default",  CArgDescriptions::eString, "0.0.0.0");
        argdesc->AddOptionalKey( "o",   "log",          "Output log to",                        CArgDescriptions::eString);
        argdesc->AddOptionalKey( "l",   "loglevel",     "Output verbosity level from 0 to 5",   CArgDescriptions::eInteger);
        argdesc->AddOptionalKey( "lf",  "loglevelfile", "File logging verbosity level from 0 to 5",   CArgDescriptions::eInteger);
        argdesc->AddOptionalKey( "db",  "database",     "Path to LMDB database",                CArgDescriptions::eString);
        argdesc->AddOptionalKey( "w",   "HttpWorkers",  "HTTP workers from 1 to 100",           CArgDescriptions::eInteger);
        argdesc->SetConstraint(  "w",   new CArgAllow_Integers(1, 100));
        argdesc->AddOptionalKey( "b",   "backlog",      "Listener backlog from 5 to 2048",      CArgDescriptions::eInteger);
        argdesc->SetConstraint(  "b",   new CArgAllow_Integers(5, 2048));
        argdesc->AddOptionalKey( "n",   "maxcon",       "Max number of connections 5 to 65000",      CArgDescriptions::eInteger);
        argdesc->SetConstraint(  "n",   new CArgAllow_Integers(5, 65000));

        SetupArgDescriptions(argdesc.release());
    }
    void ParseArgs() {        
        const CArgs& args = GetArgs();
        m_ini_file = args[ "ini" ].AsString();
        string logfile;

        filebuf fb;
        fb.open(m_ini_file.c_str(), ios::in | ios::binary);
        CNcbiIstream is(&fb);
        CNcbiRegistry Registry(is, 0);
        fb.close();

        if (!Registry.Empty() ) {
            m_log_level  = Registry.GetInt("COMMON", "LOGLEVEL", 0);
            m_log_level_file = Registry.GetInt("COMMON", "LOGLEVELFILE", 1);
            m_log_file   = Registry.GetString("COMMON", "LOGFILE", "");
            m_http_port  = Registry.GetInt("HTTP", "PORT", 2180);
            m_address   = Registry.GetString("HTTP", "Address", "");
            m_http_workers = Registry.GetInt("HTTP", "Workers", 32);
            m_lstnr_backlog = Registry.GetInt("HTTP", "Backlog", 256);
            m_tcp_max_conn = Registry.GetInt("HTTP", "MaxConn", 4096);
            m_timeout_ms = Registry.GetInt("COMMON", "OPTIMEOUT", 30000);
        }
        if (args["o"])
            m_log_file = args["o"].AsString();
        if (args["l"])
            m_log_level = args["l"].AsInteger();
        if (args["lf"])
            m_log_level_file = args["lf"].AsInteger();
        IdLogUtil::CAppLog::SetLogLevel(m_log_level);
        IdLogUtil::CAppLog::SetLogLevelFile(m_log_level_file);        
        if (!m_log_file.empty())
            IdLogUtil::CAppLog::SetLogFile(m_log_file);

        m_cass_fact->AppParseArgs(args);
        m_cass_fact->LoadConfig(m_ini_file, "");

        if (args["P"])
            m_http_port = args["P"].AsInteger();
        if (args["H"])
            m_address = args["H"].AsString();
        if (args["w"])
            m_http_workers = args["w"].AsInteger();
        if (args["b"])
            m_lstnr_backlog = args["b"].AsInteger();
        if (args["n"])
            m_tcp_max_conn = args["n"].AsInteger();
        if (args["db"])
            m_db_path = args["db"].AsString();

        list<string> entries;
        Registry.EnumerateEntries("satnames", &entries);
        for (const auto& it : entries) {
            size_t idx = NStr::StringToNumeric<size_t>(it);
            while (m_sat_names.size() <= idx)
                m_sat_names.push_back("");
            m_sat_names[idx] = Registry.GetString("satnames", it, "");
        }
        if (m_sat_names.size() == 0)
            EAccVerException::raise("[satnames] section in ini file is empty");
    }

    void OpenDb(bool Initialize, bool Readonly) {
        if (m_db_path.empty())
            m_db_path = "./accvercache.db";
        m_db.reset(new CAccVerCacheDB());
        m_db->Open(m_db_path, Initialize, Readonly);
    }

    void OpenCass() {
        m_cass_conn = m_cass_fact->CreateInstance();
        m_cass_conn->Connect();
    }

    void CloseCass() {
        m_cass_conn = nullptr;
        m_cass_fact = nullptr;
    }

    bool SatToSatName(size_t sat, string& sat_name) {
        if (sat < m_sat_names.size()) {
            sat_name = m_sat_names[sat];
            return !sat_name.empty();
        }
        else
            return false;
    }

    template<typename P>
    int OnAccVerResolver(HST::CHttpRequest& req, HST::CHttpReply<P> &resp) {
        const char *value;
        size_t value_len;

        if (req.GetParam("accver", sizeof("accver") - 1, true, &value, &value_len)) {
            string AccVer(value, value_len);

            string Data;
            if (m_db->Storage().Get(AccVer, Data))
                resp.SendOk(Data.c_str(), Data.length(), false);
            else {
                stringstream ss;
                ss << "Entry (" << AccVer << ") not found";
                resp.Send404("notfound", ss.str().c_str());
            }
        }
        else
            resp.Send503("invalid", "invalid request");
        return 0;
    }

    template<typename P>
    int OnGetBlob(HST::CHttpRequest& req, HST::CHttpReply<P> &resp) {
        const char *ssat, *ssat_key;
        size_t sat_len, sat_key_len;
        if (req.GetParam("sat", sizeof("sat") - 1, true, &ssat, &sat_len) &&
            req.GetParam("sat_key", sizeof("sat_key") - 1, true, &ssat_key, &sat_key_len)) 
        {
            int sat = atoi(ssat);
            string sat_name;
            if (SatToSatName(sat, sat_name)) {
                int sat_key = atoi(ssat_key);
                resp.Postpone(CPendingOperation(std::move(sat_name), sat_key, m_cass_conn, m_timeout_ms));
                return 0;
            }
            else {
                resp.Send404("not found", "invalid/unsupported satellite number");
            }
        }
        else
            resp.Send503("invalid", "invalid request");
        return 0;
    }

    virtual int Run(void) {
        srand(time(NULL));
        ParseArgs();
        OpenDb(false, true);
        OpenCass();

        vector<HST::CHttpHandler<CPendingOperation>> h;
        HST::CHttpGetParser gparser;

        h.emplace_back("/ID/accver.resolver", [this](HST::CHttpRequest& req, HST::CHttpReply<CPendingOperation> &resp)->int{ return OnAccVerResolver(req, resp); }, &gparser, nullptr);
        h.emplace_back("/ID/getblob", [this](HST::CHttpRequest& req, HST::CHttpReply<CPendingOperation> &resp)->int{ return OnGetBlob(req, resp); }, &gparser, nullptr);

        unique_ptr<HST::CHttpDaemon<CPendingOperation>> d(new HST::CHttpDaemon<CPendingOperation>(h, m_address, m_http_port, m_http_workers, m_lstnr_backlog, m_tcp_max_conn));
        d->Run([this](TSL::CTcpDaemon<HST::CHttpProto<CPendingOperation>, HST::CHttpConnection<CPendingOperation>, HST::CHttpDaemon<CPendingOperation>>& tcp_daemon) {
            printf("%ld %d %ld\n", tcp_daemon.NumOfRequests(), tcp_daemon.NumOfConnections(), m_cass_conn->GetActiveStatements());
        });
        CloseCass();
        return 0;
    }

};

int main(int argc, const char* argv[])
{
    srand(time(NULL));
    CThread::InitializeMainThreadId();
    CAppLog::FormatTimeStamp(true);
    return CAccVerCacheDaemonApplication().AppMain(argc, argv);
}
