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
 * Authors: Dmitri Dmitrienko, Rafael Sadyrov
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

#include <objtools/pubseq_gateway/impl/rpc/UtilException.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//////////////////////////////////

class CPsgCliException: public EException {
public:
    CPsgCliException(const char * const msg, const int code) noexcept : EException(msg, code) {}
    [[noreturn]] static inline void raise(const char* msg, int code = 0) {
        throw CPsgCliException{msg, code};
    }
    [[noreturn]] static inline void raise(const std::string& msg, int code = 0) {
        raise(msg.c_str(), code);
    }
};

class CPsgCliApp: public CNcbiApplication
{
private:
    unsigned int m_NumThreads;
    string m_HostPort;
    string m_BioId;
    string m_ResolveId;
    string m_BlobId;
    string m_LookupFileRemote;
    string m_ResolveIdFile;
    string m_BlobIdFile;
    char m_Delimiter;
    mutex m_CoutMutex;
    mutex m_CerrMutex;
    shared_ptr<CPSG_Queue> m_Queue;

    template <class TRequest>
    void ProcessId(shared_ptr<TRequest> id);

    template <class TRequest>
    void ProcessFile(const string& filename);

    void ProcessReply(shared_ptr<CPSG_Reply> reply);
    void PrintErrors(EPSG_Status status, const CPSG_ReplyItem* item);
    void PrintBlobData(const CPSG_BlobData*);
    void PrintBlobInfo(const CPSG_BlobInfo*);
    void PrintBioseqInfo(const CPSG_BioseqInfo*);

public:
    CPsgCliApp() :
        m_NumThreads(1),
        m_Delimiter('|')
    {}
    virtual void Init()
    {
        unique_ptr<CArgDescriptions> argdesc(new CArgDescriptions());
        argdesc->SetUsageContext(GetArguments().GetProgramBasename(), "psg_cli -- Application to maintain Accession.Version Cache");
        argdesc->AddOptionalKey( "H",   "host",     "Host[:port] for remote lookups",       CArgDescriptions::eString);
        argdesc->AddOptionalKey( "la",  "bio_id",   "Individual bio ID lookup and retrieval", CArgDescriptions::eString);
        argdesc->AddOptionalKey( "rv",  "bio_id",   "Individual bio ID resolve",            CArgDescriptions::eString);
        argdesc->AddOptionalKey( "gb",  "blob_id",  "Individual blob retrieval",            CArgDescriptions::eString);
        argdesc->AddOptionalKey( "fa",  "bio_id_file", "Lookup Bio IDs from a file",        CArgDescriptions::eString);
        argdesc->AddOptionalKey( "fv",  "resolve_id_file", "Resolve Bio IDs from a file",   CArgDescriptions::eString);
        argdesc->AddOptionalKey( "fb",  "blob_id_file", "Retrieval blobs from a file",      CArgDescriptions::eString);
        argdesc->AddOptionalKey( "t",   "threads",  "Number of threads",                    CArgDescriptions::eInteger);
        argdesc->SetConstraint(  "t",   new CArgAllow_Integers(1, 256));
        SetupArgDescriptions(argdesc.release());
    }
    void ParseArgs()
    {
        const CArgs& args = GetArgs();

        if (args["H"])
            m_HostPort = args["H"].AsString();
        if (args["la"])
            m_BioId = args["la"].AsString();
        if (args["rv"])
            m_ResolveId = args["rv"].AsString();
        if (args["gb"])
            m_BlobId = args["gb"].AsString();
        if (args["fa"])
            m_LookupFileRemote = args["fa"].AsString();
        if (args["fv"])
            m_ResolveIdFile = args["fv"].AsString();
        if (args["fb"])
            m_BlobIdFile = args["fb"].AsString();
        if (args["t"])
            m_NumThreads = args["t"].AsInteger();

        if (m_HostPort.empty())
            CPsgCliException::raise("Host is not specified, use -H command line argument");
    }

    virtual int Run(void)
    {
        int rv = 0;
        try {
            ParseArgs();
            m_Queue = make_shared<CPSG_Queue>(m_HostPort);

            if (!m_BioId.empty()) {
                auto id = CPSG_BioId(m_BioId, CSeq_id_Base::e_Gi);
                auto request = make_shared<CPSG_Request_Biodata>(move(id));
                ProcessId<CPSG_Request_Biodata>(request);
            }
            else if (!m_ResolveId.empty()) {
                auto id = CPSG_BioId(m_ResolveId, CSeq_id_Base::e_Gi);
                auto request = make_shared<CPSG_Request_Resolve>(move(id));
                request->IncludeInfo(CPSG_Request_Resolve::fAllInfo);
                ProcessId<CPSG_Request_Resolve>(request);
            }
            else if (!m_BlobId.empty()) {
                auto request = make_shared<CPSG_Request_Blob>(m_BlobId);
                ProcessId<CPSG_Request_Blob>(request);
            }
            else if (!m_LookupFileRemote.empty()) {
                ProcessFile<CPSG_Request_Biodata>(m_LookupFileRemote);
            }
            else if (!m_ResolveIdFile.empty()) {
                ProcessFile<CPSG_Request_Resolve>(m_ResolveIdFile);
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

void CPsgCliApp::ProcessReply(shared_ptr<CPSG_Reply> reply)
{
    assert(reply);

    for (;;) {
        auto reply_item = reply->GetNextItem(CDeadline::eInfinite);

        switch (reply_item->GetType()) {
        case CPSG_ReplyItem::eEndOfReply:
            return;

        case CPSG_ReplyItem::eBlobData:
            PrintBlobData(reply_item->CastTo<CPSG_BlobData>());
            break;

        case CPSG_ReplyItem::eBlobInfo:
            PrintBlobInfo(reply_item->CastTo<CPSG_BlobInfo>());
            break;

        case CPSG_ReplyItem::eBioseqInfo:
            PrintBioseqInfo(reply_item->CastTo<CPSG_BioseqInfo>());
            break;
        }
    }
}

template <class TRequest>
void CPsgCliApp::ProcessId(shared_ptr<TRequest> request)
{
    assert(m_Queue);

    m_Queue->SendRequest(request, CDeadline::eInfinite);
    ProcessReply(m_Queue->GetNextReply(CDeadline::eInfinite));
}

template <class TRequest>
void CPsgCliApp::ProcessFile(const string& filename)
{
    assert(m_Queue);

    ifstream infile(filename);
    vector<string> ids;

    if (m_NumThreads == 0 || m_NumThreads > 1000)
        CPsgCliException::raise("Invalid number of threads");

    if (!infile) {
        CPsgCliException::raise("Error on reading bio IDs");
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

void CPsgCliApp::PrintErrors(EPSG_Status status, const CPSG_ReplyItem* item)
{
    cout << static_cast<int>(status) << "\n";

    for (;;) {
        auto message = item->GetNextMessage();
        if (message.empty()) break;
        cout << message << "\n";
    }
}

void CPsgCliApp::PrintBlobData(const CPSG_BlobData* blob_data)
{
    assert(blob_data);
    lock_guard<mutex> lock(m_CoutMutex);

    auto status = blob_data->GetStatus(CDeadline::eInfinite);

    if (status != EPSG_Status::eSuccess) {
        cout << "ERROR: Failed to retrieve blob data'" << blob_data->GetId().Get() << "': ";
        PrintErrors(status, blob_data);
        return;
    }

    ostringstream os;
    os << blob_data->GetStream().rdbuf();
    hash<string> blob_hash;
    cout << "BlobData. ";
    cout << "Id: " << blob_data->GetId().Get() << ";";
    cout << "Size: " << os.str().size() << ";";
    cout << "Hash: " << blob_hash(os.str()) << endl;
}

void CPsgCliApp::PrintBlobInfo(const CPSG_BlobInfo* blob_info)
{
    assert(blob_info);
    lock_guard<mutex> lock(m_CoutMutex);

    auto status = blob_info->GetStatus(CDeadline::eInfinite);

    if (status != EPSG_Status::eSuccess) {
        cout << "ERROR: Failed to retrieve blob_info '" << blob_info->GetId().Get() << "': ";
        PrintErrors(status, blob_info);
        return;
    }

    cout << "BlobInfo. ";
    cout << "GetId: " << blob_info->GetId().Get() << ";";
    cout << "GetCompression: " << blob_info->GetCompression() << ";";
    cout << "GetFormat: " << blob_info->GetFormat() << ";";
    cout << "GetVersion: " << blob_info->GetVersion() << ";";
    cout << "GetStorageSize: " << blob_info->GetStorageSize() << ";";
    cout << "GetSize: " << blob_info->GetSize() << ";";
    cout << "IsDead: " << blob_info->IsDead() << ";";
    cout << "IsSuppressed: " << blob_info->IsSuppressed() << ";";
    cout << "IsWithdrawn: " << blob_info->IsWithdrawn() << ";";
    cout << "GetHupReleaseDate: " << blob_info->GetHupReleaseDate() << ";";
    cout << "GetOwner: " << blob_info->GetOwner() << ";";
    cout << "GetOriginalLoadDate: " << blob_info->GetOriginalLoadDate() << ";";
    cout << "GetClass: " << blob_info->GetClass() << ";";
    cout << "GetDivision: " << blob_info->GetDivision() << ";";
    cout << "GetUsername: " << blob_info->GetUsername() << ";";
    cout << "GetSplitInfoBlobId(eSplitShell): " << blob_info->GetSplitInfoBlobId(CPSG_BlobInfo::eSplitShell).Get() << ";";
    cout << "GetSplitInfoBlobId(eSplitInfo): " << blob_info->GetSplitInfoBlobId(CPSG_BlobInfo::eSplitInfo).Get() << ";";

    for (int i = 1; ; ++i) {
        auto blob_id = blob_info->GetChunkBlobId(i).Get();
        if (blob_id.empty()) break;
        cout << "GetChunkBlobId(" << i << "): " << blob_id << ";";
    }

    cout << endl;
}

void CPsgCliApp::PrintBioseqInfo(const CPSG_BioseqInfo* bioseq_info)
{
    assert(bioseq_info);
    lock_guard<mutex> lock(m_CoutMutex);

    auto status = bioseq_info->GetStatus(CDeadline::eInfinite);

    if (status != EPSG_Status::eSuccess) {
        cout << "ERROR: Failed to retrieve bioseq_info '" << bioseq_info->GetCanonicalId().Get() << "': ";
        PrintErrors(status, bioseq_info);
        return;
    }

    const auto& id = bioseq_info->GetCanonicalId();
    cout << "BioseqInfo. ";
    cout << "CanonicalId: " << id.Get() << "-" << id.GetType() << ";";

    cout << "OtherIds:";
    for (auto& other_id : bioseq_info->GetOtherIds()) cout << " " << other_id.Get() << "-" << other_id.GetType();
    cout << ";";

    cout << "MoleculeType: " << bioseq_info->GetMoleculeType() << ";";
    cout << "Length: " << bioseq_info->GetLength() << ";";
    cout << "State: " << bioseq_info->GetState() << ";";
    cout << "BlobId: " << bioseq_info->GetBlobId().Get() << ";";
    cout << "TaxId: " << bioseq_info->GetTaxId() << ";";
    cout << "Hash: " << bioseq_info->GetHash() << ";";
    cout << "GetDateChanged: " << bioseq_info->GetDateChanged() << ";";
    cout << "IncludedData: " << bioseq_info->IncludedInfo() << endl;
};

/////////////////////////////////////////////////////////////////////////////
//  main

int main(int argc, const char* argv[])
{
    srand(time(NULL));

    return CPsgCliApp().AppMain(argc, argv);
}
