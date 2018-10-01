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

#include <objtools/pubseq_gateway/client/psg_client.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//////////////////////////////////

class CPsgCliException: public CException
{
public:
    enum EErrCode {
        eInvalidNumberOfThreads,
        eHostMissing,
        eFileReadingFailure
    };

    const char* GetErrCodeString(void) const override;

    NCBI_EXCEPTION_DEFAULT(CPsgCliException, CException);
};

inline const char* CPsgCliException::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
        case eInvalidNumberOfThreads: return "eInvalidNumberOfThreads";
        case eHostMissing:            return "eHostMissing";
        case eFileReadingFailure:     return "eFileReadingFailure";
        default:                      return CException::GetErrCodeString();
    }
}

struct SAtomicCout : stringstream
{
    ~SAtomicCout() { cout << str(); }
};

class CPsgCliApp: public CNcbiApplication
{
private:
    using TFactory = function<shared_ptr<CPSG_Request>(const string&)>;

    unsigned int m_NumThreads;
    string m_HostPort;
    string m_BioId;
    string m_ResolveId;
    string m_BlobId;
    string m_LookupFileRemote;
    string m_ResolveIdFile;
    string m_BlobIdFile;
    shared_ptr<CPSG_Queue> m_Queue;

    void ProcessId(const string& id, TFactory factory);
    void ProcessFile(const string& filename, TFactory factory);

    void ProcessReply(shared_ptr<CPSG_Reply> reply);

    template <class TReply>
    void PrintErrors(SAtomicCout& atomic_cout, EPSG_Status status, shared_ptr<TReply> item);

    void PrintBlobData(shared_ptr<CPSG_BlobData>);
    void PrintBlobInfo(shared_ptr<CPSG_BlobInfo>);
    void PrintBioseqInfo(shared_ptr<CPSG_BioseqInfo>);

    static shared_ptr<CPSG_Request> CreateBioRequest(const string& id)
    {
        auto bio_id = CPSG_BioId(id, CSeq_id_Base::e_Gi);
        return make_shared<CPSG_Request_Biodata>(move(bio_id));
    }

    static shared_ptr<CPSG_Request> CreateResolveRequest(const string& id)
    {
        auto bio_id = CPSG_BioId(id, CSeq_id_Base::e_Gi);
        auto request = make_shared<CPSG_Request_Resolve>(move(bio_id));
        request->IncludeInfo(CPSG_Request_Resolve::fAllInfo);
        return request;
    }

    static shared_ptr<CPSG_Request> CreateBlobRequest(const string& id)
    {
        return make_shared<CPSG_Request_Blob>(id);
    }

public:
    CPsgCliApp() :
        m_NumThreads(2)
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
            NCBI_THROW(CPsgCliException, eHostMissing, "Host is not specified, use -H command line argument");
    }

    virtual int Run(void)
    {
        int rv = 0;
        try {
            ParseArgs();
            m_Queue = make_shared<CPSG_Queue>(m_HostPort);

            if (!m_BioId.empty()) {
                ProcessId(m_BioId, CreateBioRequest);
            }
            else if (!m_ResolveId.empty()) {
                ProcessId(m_ResolveId, CreateResolveRequest);
            }
            else if (!m_BlobId.empty()) {
                ProcessId(m_BlobId, CreateBlobRequest);
            }
            else if (!m_LookupFileRemote.empty()) {
                ProcessFile(m_LookupFileRemote, CreateBioRequest);
            }
            else if (!m_ResolveIdFile.empty()) {
                ProcessFile(m_ResolveIdFile, CreateResolveRequest);
            }
            else if (!m_BlobIdFile.empty()) {
                ProcessFile(m_BlobIdFile, CreateBlobRequest);
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

    auto status = reply->GetStatus(CDeadline::eInfinite);

    if (status != EPSG_Status::eSuccess) {
        SAtomicCout atomic_cout;
        atomic_cout << "Reply. ERROR: ";
        PrintErrors(atomic_cout, status, reply);
        return;
    }

    for (;;) {
        auto reply_item = reply->GetNextItem(CDeadline::eInfinite);

        switch (reply_item->GetType()) {
        case CPSG_ReplyItem::eEndOfReply:
            return;

        case CPSG_ReplyItem::eBlobData:
            PrintBlobData(static_pointer_cast<CPSG_BlobData>(reply_item));
            break;

        case CPSG_ReplyItem::eBlobInfo:
            PrintBlobInfo(static_pointer_cast<CPSG_BlobInfo>(reply_item));
            break;

        case CPSG_ReplyItem::eBioseqInfo:
            PrintBioseqInfo(static_pointer_cast<CPSG_BioseqInfo>(reply_item));
            break;
        }
    }
}

void CPsgCliApp::ProcessId(const string& id, TFactory factory)
{
    assert(m_Queue);

    auto request = factory(id);
    m_Queue->SendRequest(request, CDeadline::eInfinite);
    ProcessReply(m_Queue->GetNextReply(CDeadline::eInfinite));
}

void CPsgCliApp::ProcessFile(const string& filename, TFactory factory)
{
    assert(m_Queue);

    ifstream infile(filename);
    vector<string> ids;

    if (m_NumThreads == 0 || m_NumThreads > 1000)
        NCBI_THROW(CPsgCliException, eInvalidNumberOfThreads, "Invalid number of threads");

    if (!infile) {
        NCBI_THROW(CPsgCliException, eFileReadingFailure, "Error on reading bio IDs");
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
            auto request = factory(ids[i]);
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

template <class TReply>
void CPsgCliApp::PrintErrors(SAtomicCout& atomic_cout, EPSG_Status status, shared_ptr<TReply> item)
{
    atomic_cout << static_cast<int>(status);

    for (;;) {
        auto message = item->GetNextMessage();

        if (message.empty()) {
            break;
        } else {
            atomic_cout << "|";
        }

        atomic_cout << "'" << message << "'";
    }

    atomic_cout << endl;
}

void CPsgCliApp::PrintBlobData(shared_ptr<CPSG_BlobData> blob_data)
{
    assert(blob_data);
    SAtomicCout atomic_cout;
    atomic_cout << "BlobData. ";

    auto status = blob_data->GetStatus(CDeadline::eInfinite);

    if (status != EPSG_Status::eSuccess) {
        atomic_cout << "ERROR: Failed to retrieve blob data'" << blob_data->GetId().Get() << "': ";
        PrintErrors(atomic_cout, status, blob_data);
        return;
    }

    ostringstream os;
    os << blob_data->GetStream().rdbuf();
    hash<string> blob_hash;
    atomic_cout << "Id: " << blob_data->GetId().Get() << ";";
    atomic_cout << "Size: " << os.str().size() << ";";
    atomic_cout << "Hash: " << blob_hash(os.str()) << endl;
}

void CPsgCliApp::PrintBlobInfo(shared_ptr<CPSG_BlobInfo> blob_info)
{
    assert(blob_info);
    SAtomicCout atomic_cout;
    atomic_cout << "BlobInfo. ";

    auto status = blob_info->GetStatus(CDeadline::eInfinite);

    if (status != EPSG_Status::eSuccess) {
        atomic_cout << "ERROR: Failed to retrieve blob_info '" << blob_info->GetId().Get() << "': ";
        PrintErrors(atomic_cout, status, blob_info);
        return;
    }

    atomic_cout << "GetId: " << blob_info->GetId().Get() << ";";
    atomic_cout << "GetCompression: " << blob_info->GetCompression() << ";";
    atomic_cout << "GetFormat: " << blob_info->GetFormat() << ";";
    atomic_cout << "GetVersion: " << blob_info->GetVersion() << ";";
    atomic_cout << "GetStorageSize: " << blob_info->GetStorageSize() << ";";
    atomic_cout << "GetSize: " << blob_info->GetSize() << ";";
    atomic_cout << "IsDead: " << blob_info->IsDead() << ";";
    atomic_cout << "IsSuppressed: " << blob_info->IsSuppressed() << ";";
    atomic_cout << "IsWithdrawn: " << blob_info->IsWithdrawn() << ";";
    atomic_cout << "GetHupReleaseDate: " << blob_info->GetHupReleaseDate() << ";";
    atomic_cout << "GetOwner: " << blob_info->GetOwner() << ";";
    atomic_cout << "GetOriginalLoadDate: " << blob_info->GetOriginalLoadDate() << ";";
    atomic_cout << "GetClass: " << blob_info->GetClass() << ";";
    atomic_cout << "GetDivision: " << blob_info->GetDivision() << ";";
    atomic_cout << "GetUsername: " << blob_info->GetUsername() << ";";
    atomic_cout << "GetSplitInfoBlobId: " << blob_info->GetSplitInfoBlobId().Get() << ";";

    for (int i = 1; ; ++i) {
        auto blob_id = blob_info->GetChunkBlobId(i).Get();
        if (blob_id.empty()) break;
        atomic_cout << "GetChunkBlobId(" << i << "): " << blob_id << ";";
    }

    atomic_cout << endl;
}

void CPsgCliApp::PrintBioseqInfo(shared_ptr<CPSG_BioseqInfo> bioseq_info)
{
    assert(bioseq_info);
    SAtomicCout atomic_cout;
    atomic_cout << "BioseqInfo. ";

    auto status = bioseq_info->GetStatus(CDeadline::eInfinite);

    if (status != EPSG_Status::eSuccess) {
        auto request = bioseq_info->GetReply()->GetRequest();
        string id;
        if (auto request_biodata = dynamic_cast<const CPSG_Request_Biodata*>(request.get())) {
            id = request_biodata->GetBioId().Get();
        } else if (auto request_resolve = dynamic_cast<const CPSG_Request_Resolve*>(request.get())) {
            id = request_resolve->GetBioId().Get();
        } else {
            id = "UNKNOWN_REQUEST";
        }

        atomic_cout << "ERROR: Failed to retrieve bioseq_info '" << id << "': ";
        PrintErrors(atomic_cout, status, bioseq_info);
        return;
    }

    const auto& id = bioseq_info->GetCanonicalId();
    atomic_cout << "CanonicalId: " << id.Get() << "-" << id.GetType() << ";";

    atomic_cout << "OtherIds:";
    for (auto& other_id : bioseq_info->GetOtherIds()) atomic_cout << " " << other_id.Get() << "-" << other_id.GetType();
    atomic_cout << ";";

    atomic_cout << "MoleculeType: " << bioseq_info->GetMoleculeType() << ";";
    atomic_cout << "Length: " << bioseq_info->GetLength() << ";";
    atomic_cout << "State: " << bioseq_info->GetState() << ";";
    atomic_cout << "BlobId: " << bioseq_info->GetBlobId().Get() << ";";
    atomic_cout << "TaxId: " << bioseq_info->GetTaxId() << ";";
    atomic_cout << "Hash: " << bioseq_info->GetHash() << ";";
    atomic_cout << "GetDateChanged: " << bioseq_info->GetDateChanged() << ";";
    atomic_cout << "IncludedData: " << bioseq_info->IncludedInfo() << endl;
};

/////////////////////////////////////////////////////////////////////////////
//  main

int main(int argc, const char* argv[])
{
    srand(time(NULL));

    return CPsgCliApp().AppMain(argc, argv);
}
