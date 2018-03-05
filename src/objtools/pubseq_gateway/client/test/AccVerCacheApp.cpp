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
    string m_LookupRemote;
    string m_LookupFileRemote;
    char m_Delimiter;
    mutex m_CoutMutex;
    mutex m_CerrMutex;

    void RemoteLookup();
    void RemoteLookupFile();
    void PrintBlob(CPSG_Blob& blob);
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
        argdesc->AddOptionalKey( "ra",  "rlookup",  "Lookup individual accession.version remotely",  CArgDescriptions::eString);
        argdesc->AddOptionalKey( "fa",  "falookup", "Lookup accession.version from a file",  CArgDescriptions::eString);
        argdesc->AddOptionalKey( "t",   "threads",  "Number of threads",                    CArgDescriptions::eInteger);
        argdesc->SetConstraint(  "t",   new CArgAllow_Integers(1, 256));
        argdesc->AddFlag(        "s",               "Save retrieved blobs into files");
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
        if (args["ra"])
            m_LookupRemote = args["ra"].AsString();
        if (args["fa"])
            m_LookupFileRemote = args["fa"].AsString();
        if (args["t"])
            m_NumThreads = args["t"].AsInteger();
    }

    virtual int Run(void)
    {
        int rv = 0;
        try {
            ParseArgs();
            if (!m_LookupRemote.empty()) {
                RemoteLookup();
            }
            else if (!m_LookupFileRemote.empty()) {
                RemoteLookupFile();
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

void CAccVerCacheApp::RemoteLookup()
{
    auto blob_id = CPSG_BioIdResolutionQueue::Resolve(m_HostPort, m_LookupRemote);
    auto blob = CPSG_BlobRetrievalQueue::Retrieve(m_HostPort, blob_id);
    PrintBlob(blob);
}

void CAccVerCacheApp::RemoteLookupFile()
{

    ifstream infile(m_LookupFileRemote);
    TPSG_BioIds all_bio_ids;


    if (m_HostPort.empty())
        EAccVerException::raise("Host is not specified, use -H command line argument");
    if (m_NumThreads == 0 || m_NumThreads > 1000)
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

            auto delim = line.find('|');
            all_bio_ids.emplace_back(string(line, 0, delim));
            ++line_count;
        }

        {{
            CPSG_BioIdResolutionQueue res_queue(m_HostPort);
            CPSG_BlobRetrievalQueue ret_queue(m_HostPort);
            vector<unique_ptr<thread, function<void(thread*)>>> threads;
            threads.resize(m_NumThreads);
            size_t start_index = 0, next_index, i = 0;


            for (auto & it : threads) {
                i++;
                next_index = ((uint64_t)line_count * i) / m_NumThreads;
                it = unique_ptr<thread, function<void(thread*)>>(new thread(
                    [&res_queue, &ret_queue, start_index, next_index, line_count, &all_bio_ids, this]() {
                        size_t max = next_index > line_count ? line_count : next_index;

                        {
                            {
                                TPSG_BioIds bio_ids;
                                TPSG_BlobIds blob_ids;

                                const unsigned int MAX_SRC_AT_ONCE = 1024;
                                const unsigned int PUSH_TIMEOUT_MS = 15000;
                                const unsigned int POP_TIMEOUT_MS = 15000;
                                size_t res_pushed = 0, res_popped = 0;
                                size_t ret_pushed = 0, ret_popped = 0;

                                auto res_push = [&](const CDeadline& deadline) {
                                    size_t cnt = bio_ids.size();
                                    res_queue.Resolve(&bio_ids, deadline);
                                    res_pushed += cnt - bio_ids.size();
                                };

                                auto res_pop = [&](const CDeadline& deadline = CDeadline(0)) {
                                    try {
                                        TPSG_BlobIds result = res_queue.GetBlobIds(deadline);
                                        res_popped += result.size();
                                        blob_ids.insert(blob_ids.end(), result.begin(), result.end());
                                    }
                                    catch (const std::runtime_error& e) {
                                        ERRLOG0((e.what()));
                                    }
                                };

                                auto ret_push = [&](const CDeadline& deadline = CDeadline(0)) {
                                    try {
                                        size_t cnt = blob_ids.size();
                                        ret_queue.Retrieve(&blob_ids, deadline);
                                        ret_pushed += cnt - blob_ids.size();
                                    }
                                    catch (const std::runtime_error& e) {
                                        ERRLOG0((e.what()));
                                    }
                                };

                                auto ret_pop = [&](const CDeadline& deadline = CDeadline(0)) {
                                    try {
                                        TPSG_Blobs result = ret_queue.GetBlobs(deadline);
                                        ret_popped += result.size();

                                        for (auto& blob : result) {
                                            PrintBlob(blob);
                                        }
                                    }
                                    catch (const std::runtime_error& e) {
                                        ERRLOG0((e.what()));
                                    }
                                };

                                for (size_t i = start_index; i < max; ++i) {
                                    try {
                                        auto & it_data = all_bio_ids[i];
                                        LOG3(("Adding [%lu](%s)", i, it_data.GetId().c_str()));

                                        bio_ids.emplace_back(std::move(it_data));

                                        if (bio_ids.size() >= MAX_SRC_AT_ONCE) {
                                            res_push(PUSH_TIMEOUT_MS);
                                            res_pop(0);
                                            ret_push(0);
                                            ret_pop(0);
                                        }
                                    }
                                    catch (const std::runtime_error& e) {
                                        ERRLOG0((e.what()));
                                    }
                                }

                                while (bio_ids.size() > 0) {
                                    res_push(PUSH_TIMEOUT_MS);
                                    res_pop(0);
                                    ret_push(0);
                                    ret_pop(0);
                                }

                                while (!res_queue.IsEmpty()) {
                                    res_pop(POP_TIMEOUT_MS);
                                    ret_push(0);
                                    ret_pop(0);
                                }

                                while (blob_ids.size() > 0) {
                                    ret_push(PUSH_TIMEOUT_MS);
                                    ret_pop(0);
                                }

                                while (!ret_queue.IsEmpty()) {
                                    ret_pop(POP_TIMEOUT_MS);
                                }

                                LOG3(("thread finished: start: %lu, max: %lu, count: %lu, res_pushed: %lu, res_popped: %lu, ret_pushed: %lu, ret_popped: %lu", start_index, max, max - start_index, res_pushed, res_popped, ret_pushed, ret_popped));
                            }
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
}

void CAccVerCacheApp::PrintBlob(CPSG_Blob& blob)
{
    const auto& blob_id = blob.GetBlobId();
    const auto& name = blob_id.GetBioId().GetId();
    lock_guard<mutex> lock(m_CoutMutex);

    if (blob_id.GetStatus() != CPSG_BlobId::eSuccess) {
        cout << name << "||Failed to resolve: " << blob_id.GetMessage() << std::endl;
        return;
    }

    const auto& blob_info = blob_id.GetBlobInfo();
    const string blob_info_date_queued = blob_info.last_modified ? CTime(blob_info.last_modified).AsString() : "";

    cout
        << name << "||"
        << blob_info.gi << "|"
        << blob_info.seq_length << "|"
        << blob_info.sat << "|"
        << blob_info.sat_key << "|"
        << blob_info.tax_id << "|"
        << blob_info_date_queued << "|"
        << blob_info.state << "|";

    if (blob.GetStatus() != CPSG_Blob::eSuccess) {
        cout << "Failed to retrieve: " << blob.GetMessage() << std::endl;
        return;
    }

    auto& is = blob.GetStream();
    ostringstream os;
    os << is.rdbuf();
    hash<string> blob_hash;
    cout << os.str().size() << "|" <<blob_hash(os.str()) << endl;
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
