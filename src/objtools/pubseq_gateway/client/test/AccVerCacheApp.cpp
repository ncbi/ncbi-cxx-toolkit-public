#define _XOPEN_SOURCE
#define _POSIX_SOURCE
#define _BSD_SOURCE

#include <ncbi_pch.hpp>

#include <atomic>
#include <map>
#include <thread>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <sstream>
#include <istream> 
#include <iostream>
#include <cstdio>
#include <climits>
#include <unordered_map>
#include <mutex>
#include <ctime>

#include <corelib/ncbiapp.hpp>

#if 0
#include <lmdb.h>
#endif

#include <objtools/pubseq_gateway/diag/AppLog.hpp>
#include <objtools/pubseq_gateway/rpc/UtilException.hpp>
#include <objtools/pubseq_gateway/rpc/DdRpcDataPacker.hpp>
#include <objtools/pubseq_gateway/rpc/DdRpcClient.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>

#if 0
#include "AccVerCacheDB.hpp"
#endif

#define DFLT_LOG_LEVEL 1

USING_NCBI_SCOPE;
USING_SCOPE(objects);

using namespace IdLogUtil;

#define SYBASE_DT_FORMAT "%b %d %Y %H:%M:%S"

//////////////////////////////////

enum class CSyncType {
    stSync,
    stAsync,
    stCQ1,
    stCQNcbi
};

class CAccVerCacheApp: public CNcbiApplication {
private:
	string m_IniFile;
    string m_LogFile;
    bool m_FullUpdate;
    unsigned int m_LogLevel;
    unsigned int m_NumThreads;
    CSyncType m_Sync;
    string m_HostPort;
    string m_DbPath;
    string m_Lookup;
    string m_LookupRemote;
    string m_LookupFileRemote;
    string m_DumpTo;
#if 0
    unique_ptr<CAccVerCacheDB> m_DB;
#endif
    char m_Delimiter;
    void TestLoadData();
#if 0
    void OpenDb(bool Initialize, bool Readonly);
#endif
    void Lookup(const string& AccVer);
    void RemoteLookup(const string& AccVer);
    void RemoteLookupFile(const string& FileName, CSyncType sync, unsigned int NumThreads);
    void DumpTo(const string& FileName);
    void PerformUpdate(bool IsFull);
#if 0
    void CloseDb();
#endif
    void DoDump(ostream& Out, const DDRPC::DataRow& Row, char Delimiter);
    void DumpEntry(ostream& Out, char Delimiter, const DDRPC::DataColumns &Clms, const string& Key, const string& Data);
public:
	CAccVerCacheApp() : 
        m_FullUpdate(false), 
        m_LogLevel(DFLT_LOG_LEVEL),
        m_NumThreads(1),
        m_Sync(CSyncType::stAsync),
        m_Delimiter('|')
	{}
#if 0
	virtual ~CAccVerCacheApp() {
        CloseDb();
	}
#endif
	virtual void Init() {
		unique_ptr<CArgDescriptions> argdesc(new CArgDescriptions());
		argdesc->SetUsageContext(GetArguments().GetProgramBasename(), "AccVerCache -- Application to maintain Accession.Version Cache");
		argdesc->AddDefaultKey ( "ini", "IniFile",  "File with configuration information",  CArgDescriptions::eString, "AccVerCache.ini");
		argdesc->AddOptionalKey( "o",   "log",      "Output log to",                        CArgDescriptions::eString);
		argdesc->AddOptionalKey( "l",   "loglevel", "Output verbosity level from 0 to 5",   CArgDescriptions::eInteger);
		argdesc->AddOptionalKey( "H",   "host",     "Host[:port] for remote lookups",       CArgDescriptions::eString);
		argdesc->AddFlag       ( "f",               "Full update");
		argdesc->AddOptionalKey( "db",  "database", "Path to LMDB database",                CArgDescriptions::eString);
		argdesc->AddOptionalKey( "a",   "lookup",   "Lookup individual accession.version",  CArgDescriptions::eString);
		argdesc->AddOptionalKey( "ra",  "rlookup",  "Lookup individual accession.version remotely",  CArgDescriptions::eString);
		argdesc->AddOptionalKey( "fa",  "falookup", "Lookup accession.version from a file",  CArgDescriptions::eString);
		argdesc->AddOptionalKey( "d",   "dump",     "Dump into a file",                     CArgDescriptions::eString);
		argdesc->AddOptionalKey( "t",   "threads",  "Number of threads",                    CArgDescriptions::eInteger);
        argdesc->SetConstraint(  "t",   new CArgAllow_Integers(1, 256));
		argdesc->AddOptionalKey( "s",   "sync",     "Algorithm to use: [s]ync, [a]sync, [cq1]-- single thread completion queue or [cq2]-- two thread completion queue", CArgDescriptions::eString);
        argdesc->SetConstraint(  "s",   &(*new CArgAllow_Strings, "s", "a", "cq1", "cqNcbi"), CArgDescriptions::eConstraint);
		SetupArgDescriptions(argdesc.release());
	}
	void ParseArgs() {
		const CArgs& args = GetArgs();
		m_IniFile = args[ "ini" ].AsString();
		string logfile;
		
		filebuf fb;
		fb.open(m_IniFile.c_str(), ios::in | ios::binary);
		CNcbiIstream is( &fb);
		CNcbiRegistry Registry( is, 0);
		fb.close();

		if (!Registry.Empty() ) {
			m_LogLevel = Registry.GetInt("COMMON", "LOGLEVEL", DFLT_LOG_LEVEL);
			m_LogFile = Registry.GetString("COMMON", "LOGFILE", "");
		}
		if (args["o"])
			m_LogFile = args["o"].AsString();
		if (args["l"])
			m_LogLevel = args["l"].AsInteger();

		if (args["H"])
            m_HostPort = args["H"].AsString();
		if (args["f"])
			m_FullUpdate = args["f"].AsBoolean();
		if (args["db"])
            m_DbPath = args["db"].AsString();
		if (args["a"])
			m_Lookup = args["a"].AsString();
		if (args["ra"])
			m_LookupRemote = args["ra"].AsString();
		if (args["fa"])
			m_LookupFileRemote = args["fa"].AsString();
		if (args["d"])
			m_DumpTo = args["d"].AsString();
		if (args["t"])
			m_NumThreads = args["t"].AsInteger();
        if (args["s"]) {
            if (args["s"].AsString() == "s")
                m_Sync = CSyncType::stSync;
            else if (args["s"].AsString() == "cq1")
                m_Sync = CSyncType::stCQ1;
            else if (args["s"].AsString() == "cqNcbi")
                m_Sync = CSyncType::stCQNcbi;
            else 
                m_Sync = CSyncType::stAsync;
        }
            
	}

	virtual int Run(void) {
        int rv = 0;
        try {
            ParseArgs();
            vector<pair<string, HCT::http2_end_point>> StaticMap;
            StaticMap.emplace_back(make_pair<string, HCT::http2_end_point>(ACCVER_RESOLVER_SERVICE_ID, {.schema = "http", .authority = m_HostPort, .path = "/ID/accver.resolver"}));
            DDRPC::DdRpcClient::Init(unique_ptr<DDRPC::ServiceResolver>(new DDRPC::ServiceResolver(&StaticMap)));
#if 0
            if (!m_Lookup.empty()) {
                OpenDb(false, true);
                Lookup(m_Lookup);
            }
            else
#endif
            if (!m_LookupRemote.empty()) {
#if 0
                OpenDb(false, true);
#endif
                RemoteLookup(m_LookupRemote);
            }
            else if (!m_LookupFileRemote.empty()) {
#if 0
                OpenDb(false, true);
#endif
                RemoteLookupFile(m_LookupFileRemote, m_Sync, m_NumThreads);
            }
#if 0
            else if (!m_DumpTo.empty()) {
                OpenDb(false, true);
                DumpTo(m_DumpTo);
            }
            else {
                OpenDb(m_FullUpdate, false);
TestLoadData();
                PerformUpdate(m_FullUpdate);
            }
            CloseDb();
#endif
        }
        catch(const CException& e) {
            cerr << "Abnormally terminated: " << e.what() << endl;
            rv = 1;
        }
        catch(const exception& e) {
            cerr << "Abnormally terminated: " << e.what() << endl;
            rv = 1;
        }
        catch(...) {
            cerr << "Abnormally terminated" << endl;
            rv = 3;
        }
        DDRPC::DdRpcClient::Finalize();
		return rv;
	}
	
};

#if 0
void CAccVerCacheApp::TestLoadData() {
    ifstream infile("/export/home/dmitrie1/_GIACC_DUMP_");

    if (infile) {
        string line;
        string sNULL = "NULL";
        int64_t cnt = 0;
        int64_t datalen = 0;
        size_t lineno = 0;
        bool got_columns = false;
        DDRPC::DataColumns Clms, DataClms, KeyClms;

        while (!infile.eof()) {
            try {
                lineno++;
                getline(infile, line);
                if (line == "")
                    continue;

                if (!got_columns) {
                    got_columns = true;
                    string tok;
                    if (!getline(stringstream(line), tok, '|'))
                        EAccVerException::raise("Pipe (|) is not found on 1st line");
                    Clms.AssignAsText(tok);
                    m_DB->SaveColumns(Clms);
                    KeyClms = Clms.ExtractKeyColumns();
                    DataClms = Clms.ExtractDataColumns();
                }
                else {
                    string key;
                    string data;
                    Clms.ParseBCP(line, '|', KeyClms, DataClms, key, data);
                    if (m_DB->Storage().Update(key, data, true))
                        datalen += 4 + key.length()+ 4 + data.length();
                }

                cnt++;
                if (cnt % 1000000 == 0) {
                    cout << cnt << " :: " << datalen << endl;
                }
            }
            catch (const EAccVerException& e) {
                EAccVerException::raise(string(e.what()) + ", at line " +NStr::NumericToString(lineno), e.code());
            }
        }
    }
}

void CAccVerCacheApp::OpenDb(bool Initialize, bool Readonly) {
    if (m_DbPath.empty())
        m_DbPath = "./accvercache.db";
    m_DB.reset(new CAccVerCacheDB());
    m_DB->Open(m_DbPath, Initialize, Readonly);
}
#endif

void CAccVerCacheApp::DoDump(ostream& Out, const DDRPC::DataRow& Row, char Delimiter) {
    Row.GetAsTsv(Out, Delimiter);
}

void CAccVerCacheApp::DumpEntry(ostream& Out, char Delimiter, const DDRPC::DataColumns &Clms, const string& Key, const string& Data) {
    DDRPC::DataColumns KeyClms = Clms.ExtractKeyColumns();
    DDRPC::DataRow KeyRow; 
    KeyRow.Unpack(Key, true, KeyClms);
    DoDump(Out, KeyRow, Delimiter);

    Out << Delimiter;

    DDRPC::DataColumns DataClms = Clms.ExtractDataColumns();
    DDRPC::DataRow DataRow;
    DataRow.Unpack(Data, false, DataClms);
    DoDump(Out, DataRow, Delimiter);

    Out << endl;
}

#if 0
void CAccVerCacheApp::Lookup(const string& AccVer) {
    string Data;
    auto Clms = m_DB->Columns();
    if (m_DB->Storage().Get(AccVer, Data))
        DumpEntry(cout, m_Delimiter, Clms, AccVer, Data);
    else
        cout << AccVer << " is not found" << endl;
}
#endif

void CAccVerCacheApp::RemoteLookup(const string& AccVer) {
    string Data;
    if (m_HostPort.empty())
        EAccVerException::raise("Host is not specified, use -H command line argument");
    if (m_Sync == CSyncType::stSync)
        Data = DDRPC::DdRpcClient::SyncRequest(ACCVER_RESOLVER_SERVICE_ID, "accver=" + AccVer);
    else {
        auto Req = DDRPC::DdRpcClient::AsyncRequest(ACCVER_RESOLVER_SERVICE_ID, "accver=" + AccVer);
        Req->Wait();
        Data = Req->Get();
    }
    DDRPC::DataColumns Clms(ACCVER_RESOLVER_COLUMNS);
    DumpEntry(cout, m_Delimiter, Clms, AccVer, Data);
}

void CAccVerCacheApp::RemoteLookupFile(const string& FileName, CSyncType sync, unsigned int NumThreads) {

    ifstream infile(FileName);
    vector<pair<string, string>> data;
    
    vector<CBioId> src_data_ncbi;
    vector<CBlobId> rslt_data_ncbi;
    mutex rslt_data_ncbi_mux;


    if (m_HostPort.empty())
        EAccVerException::raise("Host is not specified, use -H command line argument");
    if (NumThreads == 0 || NumThreads > 1000)
        EAccVerException::raise("Invalid number of threads");

    if (infile) {
        string line;
        size_t lineno = 0;
        size_t line_count = 0;
        while (!infile.eof()) {
            lineno++;
            getline(infile, line);
            if (line == "")
                continue;
            if (sync == CSyncType::stCQNcbi)
                src_data_ncbi.emplace_back(line);
            else
                data.emplace_back(line, "");
            ++line_count;
        }

        {{
            vector<unique_ptr<thread, function<void(thread*)>>> threads;
            threads.resize(NumThreads);
            size_t start_index = 0, next_index, i = 0;

            
            for (auto & it : threads) {
                i++;
                next_index = ((uint64_t)line_count * i) / NumThreads;
                it = unique_ptr<thread, function<void(thread*)>>(new thread(
                    [start_index, next_index, &data, line_count, &src_data_ncbi, &rslt_data_ncbi, &rslt_data_ncbi_mux, sync, this]() {
                        size_t max = next_index > line_count ? line_count : next_index;

                        switch (sync) {
                            case CSyncType::stAsync: {
                                const int MAX_PENDING = 1024;
                                vector<unique_ptr<DDRPC::Request>> pending;
                                pending.resize(MAX_PENDING);

                                auto wait_unfinished = [&pending, &data]()->bool {
                                    auto last_busy = pending.end();
                                    bool any_replaced = false;
                                    for (auto it = pending.begin(); it != pending.end(); ++it) {
                                        if (it->get()) {
                                            long index = (*it)->GetTag();
                                            if (index >= 0) {
                                                if ((*it)->IsDone()) {
                                                    any_replaced = true;
                                                    auto & it_data = data[index];
                                                    (*it)->SetTag(-1);
                                                    try {
                                                        it_data.second = (*it)->Get();
                                                    }
                                                    catch (const std::runtime_error& e) {
                                                        ERRLOG0((e.what()));
                                                    }
                                                }
                                                else
                                                    last_busy = it;
                                            }
                                        }
                                    }
                                    if (!any_replaced) {
                                        if (last_busy == pending.end())
                                            return false; // means pending is empty

                                        (*last_busy)->WaitAny();
                                    }
                                    return true;
            
                                };

                                size_t i = start_index;

                                while (i < max) {
                                    for (auto & it : pending) {
                                        if (!it.get() || it->GetTag() < 0) {
                                            const auto & it_data = data[i];
                                            try {
                                                it = DDRPC::DdRpcClient::AsyncRequest(ACCVER_RESOLVER_SERVICE_ID, "accver=" + it_data.first);
                                                it->SetTag(i);
                                            }
                                            catch (const std::runtime_error& e) {
                                                ERRLOG0((e.what()));
                                            }
                                            ++i;
                                            if (i >= max)
                                                break;
                                        }
                                    }
                                    wait_unfinished();
                                }
                                while (wait_unfinished());
                                break;
                            }
                            case CSyncType::stSync: {
                                for (size_t i = start_index; i < max; ++i) {
                                    try {
                                        auto & it_data = data[i];
                                        it_data.second = DDRPC::DdRpcClient::SyncRequest(ACCVER_RESOLVER_SERVICE_ID, "accver=" + it_data.first);
                                        LOG3(("%s: %lu bytes", it_data.first.c_str(), it_data.second.size()));
                                    }
                                    catch (const std::runtime_error& e) {
                                        ERRLOG0((e.what()));
                                    }
                                }
                                break;
                            }
                            case CSyncType::stCQ1: {
                                DDRPC::CRequestQueue cq;
                                vector<tuple<string, string, HCT::TagHCT>> srcvec;
                                
                                const unsigned int MAX_SRC_AT_ONCE = 1024;
                                const unsigned int PUSH_TIMEOUT_MS = 15000;
                                const unsigned int POP_TIMEOUT_MS = 15000;
                                size_t pushed = 0, popped = 0;
                                const string prefix = "accver=";
                                auto push = [&cq, &srcvec, &pushed](long timeout_ms) {
                                    size_t cnt = srcvec.size();
                                    cq.Submit(srcvec, chrono::milliseconds(timeout_ms));
                                    pushed += cnt - srcvec.size();
                                };
                                auto pop = [&cq, &data, &popped](long timeout_ms) {
                                    vector<unique_ptr<DDRPC::CRequestQueueItem>> rsltvec = cq.WaitResults(chrono::milliseconds(timeout_ms));
                                    for (auto& it : rsltvec) {
                                        size_t index = it->GetTag();
                                        assert(index >= 0 && index < data.size());
                                        auto& it_data = data[index];
                                        if (it->HasError())
                                            ERRLOG0(("failed to resolve: [%lu](%s), error: %s", index, it_data.first.c_str(), it->GetErrorDescription().c_str()));
                                        else {
                                            it_data.second = it->Get();
                                            LOG3(("Received %lu bytes for [%lu](%s)", it_data.second.length(), index, it_data.first.c_str()));
                                        }
                                        ++popped;
                                    }
                                };
                                for (size_t i = start_index; i < max; ++i) {
                                    try {
                                        auto & it_data = data[i];
                                        LOG3(("Adding [%lu](%s)", i, it_data.first.c_str()));

                                        srcvec.emplace_back(ACCVER_RESOLVER_SERVICE_ID, "accver=" + it_data.first, i);
                                        
                                        if (srcvec.size() >= MAX_SRC_AT_ONCE) {
                                            push(PUSH_TIMEOUT_MS);
                                            pop(0);
                                        }
                                    }
                                    catch (const std::runtime_error& e) {
                                        ERRLOG0((e.what()));
                                    }
                                }
                                while (srcvec.size() > 0) {
                                    push(PUSH_TIMEOUT_MS);
                                    pop(0);
                                }
                                while (!cq.IsEmpty())
                                    pop(POP_TIMEOUT_MS);
                                assert(cq.IsEmpty());
                                LOG3(("thread finished: start: %lu, max: %lu, count: %lu, pushed: %lu, popped: %lu", start_index, max, max - start_index, pushed, popped));
                                break;
                            }
                            case CSyncType::stCQNcbi: {
                                CBioIdResolutionQueue cq;
                                vector<CBioId> srcvec;
                                vector<CBlobId> l_rslt_data_ncbi;
                                
                                const unsigned int MAX_SRC_AT_ONCE = 1024;
                                const unsigned int PUSH_TIMEOUT_MS = 15000;
                                const unsigned int POP_TIMEOUT_MS = 15000;
                                size_t pushed = 0, popped = 0;
                                const string Prefix = "accver=";
                                auto push = [&cq, &srcvec, &pushed](const CDeadline& deadline) {
                                    size_t cnt = srcvec.size();
                                    cq.Resolve(&srcvec, deadline);
                                    pushed += cnt - srcvec.size();
                                };
                                auto pop = [&cq, &l_rslt_data_ncbi, &popped](const CDeadline& deadline = CDeadline(0)) {
                                    try {
                                        vector<CBlobId> rsltvec = cq.GetBlobIds(deadline);
                                        popped += rsltvec.size();
                                        std::move(rsltvec.begin(), rsltvec.end(), std::back_inserter(l_rslt_data_ncbi));
                                    }
                                    catch (const std::runtime_error& e) {
                                        ERRLOG0((e.what()));
                                    }
                                };
                                for (size_t i = start_index; i < max; ++i) {
                                    try {
                                        auto & it_data = src_data_ncbi[i];
                                        LOG3(("Adding [%lu](%s)", i, it_data.GetId().c_str()));

                                        srcvec.emplace_back(std::move(it_data));
                                        
                                        if (srcvec.size() >= MAX_SRC_AT_ONCE) {
                                            push(PUSH_TIMEOUT_MS);
                                            pop(0);
                                        }
                                    }
                                    catch (const std::runtime_error& e) {
                                        ERRLOG0((e.what()));
                                    }
                                }
                                while (srcvec.size() > 0) {
                                    push(PUSH_TIMEOUT_MS);
                                    pop(0);
                                }
                                while (!cq.IsEmpty())
                                    pop(POP_TIMEOUT_MS);
                                assert(cq.IsEmpty());
                                
                                {{
                                    unique_lock<mutex> _(rslt_data_ncbi_mux);
                                    std::move(l_rslt_data_ncbi.begin(), l_rslt_data_ncbi.end(), std::back_inserter(rslt_data_ncbi));
                                }}
                                LOG3(("thread finished: start: %lu, max: %lu, count: %lu, pushed: %lu, popped: %lu", start_index, max, max - start_index, pushed, popped));
                                break;
                            }
                            default:
                                ERRLOG0(("unimplemented"));
                                assert(false);
                        }
                    }), 
                    [](thread* thrd){
                        thrd->join();
                        delete thrd;
                    });
                start_index = next_index;
            }
        }}
    }
    if (sync == CSyncType::stCQNcbi) {
        for (const auto& it : rslt_data_ncbi) {
            if (it.GetStatus() == CBlobId::eResolved) {
                cout 
                    << it.GetBioId().GetId() << "||" 
                    << it.GetBlobInfo().gi << "|" 
                    << it.GetBlobInfo().seq_length << "|"
                    << it.GetID2BlobId().sat << "|"
                    << it.GetID2BlobId().sat_key << "|"
                    << it.GetBlobInfo().tax_id << "|"
                    << (it.GetBlobInfo().date_queued ? DDRPC::DateTimeToStr(it.GetBlobInfo().date_queued) : "") << "|"
                    << it.GetBlobInfo().state << "|"
                << std::endl;
            }
            else {
                cerr << it.GetBioId().GetId() << ": failed to resolve:" << it.GetMessage() << std::endl;
            }
        }
    }
    else {
        DDRPC::DataColumns Clms(ACCVER_RESOLVER_COLUMNS);
        size_t index = 0;
        for (const auto& it : data) {
            try {
                DumpEntry(cout, m_Delimiter, Clms, it.first, it.second);
            }
            catch (const EAccVerException& e) {
                cerr << "FAILED: [" << index << "](" << it.first << ") " << e.what() << endl;
            }
            ++index;
        }
    }
}

#if 0
void CAccVerCacheApp::DumpTo(const string& FileName) {
    ofstream ofile(FileName,  ofstream::out | ofstream::trunc);
    if (ofile) {
        auto Clms = m_DB->Columns();
        DDRPC::DataColumns KeyClms = Clms.ExtractKeyColumns();
        DDRPC::DataColumns DataClms = Clms.ExtractDataColumns();

        auto end = m_DB->Storage().end();
        for (auto it = m_DB->Storage().begin(); it != end; ++it) {
            DDRPC::DataRow KeyRow;
            KeyRow.Unpack(it->first, true, KeyClms);
            DoDump(ofile, KeyRow, m_Delimiter);

            DDRPC::DataRow DataRow;
            DataRow.Unpack(it->second, false, DataClms);
            DoDump(ofile, DataRow, m_Delimiter);

            ofile << endl;
        }
    }
}
#endif

void CAccVerCacheApp::PerformUpdate(bool IsFull) {
    
}

#if 0
void CAccVerCacheApp::CloseDb() {
    if (m_DB)
        m_DB->Close();
}
#endif

/////////////////////////////////////////////////////////////////////////////
//  main

int main(int argc, const char* argv[])
{
	srand(time(NULL));
	
    IdLogUtil::CAppLog::SetLogLevelFile(0);
    IdLogUtil::CAppLog::SetLogLevel(0);
    IdLogUtil::CAppLog::SetLogFile("LOG1");
//	CThread::InitializeMainThreadId();
	return CAccVerCacheApp().AppMain(argc, argv);
}
