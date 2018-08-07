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
 * Author: Dmitri Dmitrienko
 *
 */

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

#include <objtools/pubseq_gateway/impl/diag/AppLog.hpp>
#include <objtools/pubseq_gateway/impl/rpc/UtilException.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>

#define DFLT_LOG_LEVEL 1

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//////////////////////////////////

class CAccVerCacheApp: public CNcbiApplication
{
private:
    string m_IniFile;
    unsigned int m_NumThreads;
    string m_HostPort;
    string m_BioId;
    string m_BlobId;
    string m_LookupFileRemote;
    string m_BlobIdFile;
    char m_Delimiter;
    mutex m_CoutMutex;
    mutex m_CerrMutex;
    shared_ptr<CPSG_Queue> m_Queue;

    template <class TRequest>
    void ProcessId(const string& id);

    template <class TRequest>
    void ProcessFile(const string& filename);

    void ProcessReply(shared_ptr<CPSG_Reply> reply);
    void PrintErrors(EPSG_Status status, const CPSG_ReplyItem* item);
    void PrintBlob(const CPSG_Blob*);
public:
    CAccVerCacheApp() :
        m_NumThreads(1),
        m_Delimiter('|')
    {}
    virtual void Init()
    {
        unique_ptr<CArgDescriptions> argdesc(new CArgDescriptions());
        argdesc->SetUsageContext(GetArguments().GetProgramBasename(), "AccVerCache -- Application to maintain Accession.Version Cache");
        argdesc->AddDefaultKey ( "ini", "IniFile",  "File with configuration information",  CArgDescriptions::eString, "AccVerCache.ini");
        argdesc->AddOptionalKey( "o",   "log",      "Output log to",                        CArgDescriptions::eString);
        argdesc->AddOptionalKey( "l",   "loglevel", "Output verbosity level from 0 to 5",   CArgDescriptions::eInteger);
        argdesc->AddOptionalKey( "H",   "host",     "Host[:port] for remote lookups",       CArgDescriptions::eString);
        argdesc->AddOptionalKey( "la",  "bio_id",   "Individual bio ID lookup and retrieval", CArgDescriptions::eString);
        argdesc->AddOptionalKey( "gb",  "blob_id",  "Individual blob retrieval",            CArgDescriptions::eString);
        argdesc->AddOptionalKey( "fa",  "bio_id_file", "Lookup Bio IDs from a file",        CArgDescriptions::eString);
        argdesc->AddOptionalKey( "fb",  "blob_id_file", "Retrieval blobs from a file",      CArgDescriptions::eString);
        argdesc->AddOptionalKey( "t",   "threads",  "Number of threads",                    CArgDescriptions::eInteger);
        argdesc->SetConstraint(  "t",   new CArgAllow_Integers(1, 256));
        SetupArgDescriptions(argdesc.release());
    }
    void ParseArgs()
    {
        const CArgs& args = GetArgs();
        m_IniFile = args[ "ini" ].AsString();
        string logfile;

        filebuf fb;
        fb.open(m_IniFile.c_str(), ios::in | ios::binary);
        CNcbiIstream is( &fb);
        CNcbiRegistry Registry( is, 0);
        fb.close();

        if (!Registry.Empty() ) {
            IdLogUtil::CAppLog::SetLogFile(Registry.GetString("COMMON", "LOGFILE", ""));
            IdLogUtil::CAppLog::SetLogLevel(Registry.GetInt("COMMON", "LOGLEVEL", DFLT_LOG_LEVEL));
        }
        if (args["o"])
            IdLogUtil::CAppLog::SetLogFile(args["o"].AsString());
        if (args["l"])
            IdLogUtil::CAppLog::SetLogLevel(args["l"].AsInteger());

        if (args["H"])
            m_HostPort = args["H"].AsString();
        if (args["la"])
            m_BioId = args["la"].AsString();
        if (args["gb"])
            m_BlobId = args["gb"].AsString();
        if (args["fa"])
            m_LookupFileRemote = args["fa"].AsString();
        if (args["fb"])
            m_BlobIdFile = args["fb"].AsString();
        if (args["t"])
            m_NumThreads = args["t"].AsInteger();

        if (m_HostPort.empty())
            EAccVerException::raise("Host is not specified, use -H command line argument");
    }

    virtual int Run(void)
    {
        int rv = 0;
        try {
            ParseArgs();
            m_Queue = make_shared<CPSG_Queue>(m_HostPort);

            if (!m_BioId.empty()) {
                ProcessId<CPSG_Request_Biodata>(m_BioId);
            }
            else if (!m_BlobId.empty()) {
                ProcessId<CPSG_Request_Blob>(m_BlobId);
            }
            else if (!m_LookupFileRemote.empty()) {
                ProcessFile<CPSG_Request_Biodata>(m_LookupFileRemote);
            }
            else if (!m_BlobIdFile.empty()) {
                ProcessFile<CPSG_Request_Blob>(m_BlobIdFile);
            }
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
        return rv;
    }
};

void CAccVerCacheApp::ProcessReply(shared_ptr<CPSG_Reply> reply)
{
    assert(reply);

    for (;;) {
        auto reply_item = reply->GetNextItem(CDeadline::eInfinite);

        switch (reply_item->GetType()) {
        case CPSG_ReplyItem::eEndOfReply:
            return;

        case CPSG_ReplyItem::eBlob:
            PrintBlob(reply_item->CastTo<CPSG_Blob>());
            break;

        case CPSG_ReplyItem::eBlobInfo:
            // TODO
            break;

        case CPSG_ReplyItem::eBioseqInfo:
            // TODO
            break;
        }
    }
}

template <class TRequest>
void CAccVerCacheApp::ProcessId(const string& id)
{
    assert(m_Queue);

    auto request = make_shared<TRequest>(id);
    m_Queue->SendRequest(request, CDeadline::eInfinite);
    ProcessReply(m_Queue->GetNextReply(CDeadline::eInfinite));
}

template <class TRequest>
void CAccVerCacheApp::ProcessFile(const string& filename)
{
    assert(m_Queue);

    ifstream infile(filename);
    vector<string> ids;

    if (m_NumThreads == 0 || m_NumThreads > 1000)
        EAccVerException::raise("Invalid number of threads");

    if (!infile) {
        EAccVerException::raise("Error on reading bio IDs");
    }

    while (!infile.eof()) {
        string line;
        getline(infile, line);
        if (line.empty()) continue;

        auto delim = line.find('|');
        ids.emplace_back(string(line, 0, delim));
    }

    vector<thread> threads(m_NumThreads);
    auto impl = [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            auto request = make_shared<TRequest>(ids[i]);
            m_Queue->SendRequest(request, CDeadline::eInfinite);

            auto wait_ms = (kNanoSecondsPerSecond / kMilliSecondsPerSecond) * begin % 100;

            if (auto reply = m_Queue->GetNextReply(CDeadline(0, wait_ms))) {
                ProcessReply(reply);
            }
        }

        const auto kWaitForReplySeconds = 3;

        while (auto reply = m_Queue->GetNextReply(kWaitForReplySeconds)) {
            ProcessReply(reply);
            auto wait_ms = chrono::milliseconds(end % 100);
            this_thread::sleep_for(wait_ms);
        }
    };

    size_t ids_per_thread = ids.size() / m_NumThreads;

    for (size_t i = 0; i < m_NumThreads; ++i) {
        size_t begin = i * ids_per_thread;
        size_t end = i == m_NumThreads - 1 ? ids.size() :begin + ids_per_thread;
        threads[i] = thread(impl, begin, end);
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

void CAccVerCacheApp::PrintErrors(EPSG_Status status, const CPSG_ReplyItem* item)
{
    cout << static_cast<int>(status) << "\n";

    for (;;) {
        auto message = item->GetNextMessage();
        if (message.empty()) break;
        cout << message << "\n";
    }
}

void CAccVerCacheApp::PrintBlob(const CPSG_Blob* blob)
{
    assert(blob);
    lock_guard<mutex> lock(m_CoutMutex);

    auto status = blob->GetStatus(CDeadline::eInfinite);

    if (status != EPSG_Status::eSuccess) {
        cout << "ERROR: Failed to retrieve blob '" << blob->GetId().Get() << "': ";
        PrintErrors(status, blob);
        return;
    }

    ostringstream os;
    os << blob->GetStream().rdbuf();
    hash<string> blob_hash;
    cout << blob->GetId().Get() << "|" << os.str().size() << "|" <<blob_hash(os.str()) << endl;
}

/////////////////////////////////////////////////////////////////////////////
//  main

int main(int argc, const char* argv[])
{
    srand(time(NULL));

    IdLogUtil::CAppLog::SetLogLevelFile(0);
    IdLogUtil::CAppLog::SetLogLevel(0);
    return CAccVerCacheApp().AppMain(argc, argv);
}
