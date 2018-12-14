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
* Author:  Aleksey Grichenko
*
* File Description:
*   C++ object manager performance test.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <common/test_data_path.h>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/random_gen.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <dbapi/driver/drivers.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <sstream>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//


class CPerfTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

    bool TestId(void);

private:
    typedef set<CSeq_id_Handle> TIds;

    void x_ParseDump(CNcbiIstream& in, CNcbiOstream& prim_out, CNcbiOstream& sec_out);

    void x_LoadIds(CNcbiIstream& in, TIds& ids, bool filter);

    CSeq_id_Handle x_NextId(void) {
        CFastMutexGuard guard(m_IdsMutex);
        CSeq_id_Handle ret;
        if (m_NextId != m_Ids.end()) {
            ret = *m_NextId;
            ++m_NextId;
        }
        return move(ret);
    }

    size_t m_MaxIds = (size_t)(-1);
    bool m_GetInfo = true;
    bool m_GetData = true;
    bool m_PrintInfo = false;
    bool m_PrintBlobId = false;
    bool m_PrintData = false;

    CFastMutex m_IdsMutex;
    CFastMutex m_FailMutex;
    CRef<CScope> m_Scope;
    TIds m_Ids;
    TIds::const_iterator m_NextId;
    TIds m_IgnoreIds; // Both input ignored ids and failed ids.
    int m_Failed = 0;
};


class CTestThread : public CThread
{
public:
    CTestThread(CPerfTestApp& app) : m_App(app) {}

protected:
    void* Main(void) override;

private:
    CPerfTestApp& m_App;
};


class CAtomicOut : public stringstream
{
public:
    ~CAtomicOut() {
        CFastMutexGuard guard(sm_Mutex);
        cout << str();
    }

private:
    static CFastMutex sm_Mutex;
};


CFastMutex CAtomicOut::sm_Mutex;


void* CTestThread::Main(void)
{
    while (m_App.TestId()) {}
    return nullptr;
}


void CPerfTestApp::Init(void)
{
    CONNECT_Init(&GetConfig());

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey("ids", "IdsFile",
        "file with seq-ids to load (fasta) or '_prim'|'_sec' to use NCBI_TEST_DATA",
        CArgDescriptions::eInputFile,
        "_prim");
    // Seq-ids filtering.
    arg_desc->AddOptionalKey("ignore", "IgnoreIdsFile",
        "file with seq-ids to ignore (fasta)",
        CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("passed", "PassedIds",
        "output file for saving successfully loaded seq-ids",
        CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("failed", "FailedIds",
        "output file for saving failed seq-ids",
        CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("max_ids", "MaxIds",
        "limit number of ids to MaxIds",
        CArgDescriptions::eInteger);
    arg_desc->AddDefaultKey("count", "RepeatCount",
        "repeat test RepeatCount times",
        CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("threads", "Threads",
        "number of threads to run",
        CArgDescriptions::eInteger, "0");

    // Data loader: genbank vs psg
    arg_desc->AddDefaultKey("loader", "DataLoader",
        "data loader and reader to use",
        CArgDescriptions::eString,
        "gb");
    arg_desc->SetConstraint("loader", &(*new CArgAllow_Strings,
        "gb", "psg"));

    // For genbank use id1/id2 vs pubseqos/pubseqos2
    arg_desc->AddFlag("pubseqos", "use pubseqos (with genbank only)");

    // Split vs unsplit (id1/id2, pubseqos/pubseqos2, psg tse option).
    arg_desc->AddFlag("no_split", "get only unsplit data");

    arg_desc->AddFlag("skip_info", "do not get sequence info");
    arg_desc->AddFlag("skip_data", "do not get complete TSE");
    arg_desc->AddFlag("print_info", "print sequence info");
    arg_desc->AddFlag("print_blob_id", "print blob-id");
    arg_desc->AddFlag("print_data", "print TSE");

    // Special mode - parse cassandra dump file, save ids in fasta format.
    arg_desc->AddOptionalKey("dump", "DumpFile",
        "cassandra si2csi table dump file",
        CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("prim", "PrimaryIdsFile",
        "file to save primary ids to",
        CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("sec", "SecondaryIdsFile",
        "file to save secondary ids to",
        CArgDescriptions::eOutputFile);
    arg_desc->SetDependency("dump", CArgDescriptions::eRequires, "prim");
    arg_desc->SetDependency("dump", CArgDescriptions::eRequires, "sec");

    string prog_description = "C++ object manager performance test\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}

int CPerfTestApp::Run(void)
{
    const CArgs& args = GetArgs();

    if (args["max_ids"]) m_MaxIds = (size_t)args["max_ids"].AsInteger();

    if (args["dump"]) {
        x_ParseDump(args["dump"].AsInputFile(), args["prim"].AsOutputFile(), args["sec"].AsOutputFile());
        return 0;
    }

    int repeat_count = args["count"].AsInteger();
    if (repeat_count <= 0) repeat_count = 1;
    int thread_count = args["threads"].AsInteger();
    if (thread_count < 0) thread_count = 0;
    m_GetInfo = !args["skip_info"];
    m_GetData = !args["skip_data"];
    m_PrintInfo = args["print_info"];
    m_PrintBlobId = args["print_blob_id"];
    m_PrintData = args["print_data"];

    CRef<CObjectManager> om = CObjectManager::GetInstance();
    m_Scope.Reset(new CScope(*om));

    string loader = args["loader"].AsString();
    if (loader == "gb") {
        string reader;
        if (args["pubseqos"]) {
#ifdef HAVE_PUBSEQ_OS
            DBAPI_RegisterDriver_FTDS();
            if (args["no_split"]) {
                reader = "pubseqos";
                GenBankReaders_Register_Pubseq();
            }
            else {
                reader = "pubseqos2";
                GenBankReaders_Register_Pubseq2();
            }
#else
            ERR_POST("PubseqOS reader not supported");
            return 0;
#endif
        }
        else {
            if (args["no_split"]) {
                GenBankReaders_Register_Id1();
                reader = "id1";
            }
            else {
                GenBankReaders_Register_Id2();
                reader = "id2";
            }
        }
        CGBDataLoader::RegisterInObjectManager(*om, reader);
        m_Scope->AddDefaults();
    }
    else if (loader == "psg") {
        GetRWConfig().Set("psg_loader", "no_split", args["no_split"] ? "t" : "f");
        string loader_name = om->RegisterDataLoader(0, loader)->GetName();
        m_Scope->AddDataLoader(loader_name);
    }

    cout << loader;
    if (loader == "gb") cout << (args["pubseqos"] ? " / pubseqos" : " / id");
    cout << (args["no_split"] ? " / no_split" : " / split");
    cout << " / " << args["ids"].AsString();
    if (m_MaxIds != (size_t)(-1)) cout << " / " << m_MaxIds << " ids";
    if (repeat_count > 1) cout << " / " << repeat_count << " repeats";
    if (thread_count > 0) cout << " / " << thread_count << (thread_count > 1 ? " threads" : " thread");
    cout << endl;

    // Ignored ids file may be missing.
    if (args["ignore"]) {
        x_LoadIds(args["ignore"].AsInputFile(), m_IgnoreIds, false);
    }
    string ids_file = args["ids"].AsString();
    if (!ids_file.empty() && ids_file[0] == '_') {
        string path = CFile::MakePath(
            CFile::MakePath(NCBI_GetTestDataPath(), "pubseq_gateway"),
            string("ids") + ids_file);
        if (CDirEntry(path).Exists()) {
            CNcbiIfstream in(path.c_str());
            x_LoadIds(in, m_Ids, true);
        }
        else {
            ERR_POST("Input file does not exist: " << path);
            return 0;
        }
    }
    else {
        x_LoadIds(args["ids"].AsInputFile(), m_Ids, true);
    }
    m_NextId = m_Ids.begin();
    cout << "Loaded " << m_Ids.size() << " seq-ids" << endl;

    CStopWatch sw;
    sw.Start();
    for (int i = 0; i < repeat_count; ++i) {
        CStopWatch sw2;
        sw2.Start();

        if (thread_count == 0) {
            while (TestId()) {}
        }
        else {
            vector<CRef<CThread>> threads;
            for (int i = 0; i < thread_count; ++i) {
                CRef<CThread> thr(new CTestThread(*this));
                threads.push_back(thr);
                thr->Run();
            }
            for (int i = 0; i < thread_count; ++i) {
                threads[i]->Join(nullptr);
            }
        }

        if (repeat_count > 1) {
            cout << "Cycle " << i << " finished in " << sw2.AsSmartString() << endl;
        }
    }
    cout << "Processed " << m_Ids.size();
    if (repeat_count > 1) cout << "x" << repeat_count;
    cout << " ids in " << sw.AsSmartString() << endl;
    if (m_Failed > 0) cout << "Failed to load " << m_Failed << " ids" << endl;

    if (args["failed"]) {
        CNcbiOstream& failed_out = args["failed"].AsOutputFile();
        ITERATE(TIds, it, m_IgnoreIds) {
            failed_out << it->GetSeqId()->AsFastaString() << endl;
        }
    }

    if (args["passed"]) {
        CNcbiOstream& passed_out = args["passed"].AsOutputFile();
        ITERATE(TIds, it, m_Ids) {
            if (m_IgnoreIds.find(*it) != m_IgnoreIds.end()) continue;
            passed_out << it->GetSeqId()->AsFastaString() << endl;
        }
    }

    cout << endl;
    return 0;
}


void CPerfTestApp::x_ParseDump(CNcbiIstream& in, CNcbiOstream& prim_out, CNcbiOstream& sec_out)
{
    size_t count = 0;
    while (in.good() && !in.eof()) {
        string line;
        getline(in, line);
        NStr::TruncateSpacesInPlace(line);
        if (line.empty()) continue;
        if (line.find('"') != NPOS) {
            // Quoted values (containing comma) are not yet supported.
            ERR_POST(Warning << "Seq-ids containing commas are not yet supported: " << NStr::PrintableString(line));
            continue;
        }
        vector<string> parts;
        NStr::Split(line, ",", parts);
        if (parts.size() != 5) {
            ERR_POST(Warning << "Bad id line format: " << NStr::PrintableString(line));
            continue;
        }
        CSeq_id::E_Choice id_type = CSeq_id::E_Choice(NStr::StringToNumeric<int>(parts[1]));
        CSeq_id id(CSeq_id::eFasta_AsTypeAndContent, id_type, parts[0]);
        sec_out << id.AsFastaString() << endl;

        id_type = CSeq_id::E_Choice(NStr::StringToNumeric<int>(parts[3]));
        id.Set(id_type, parts[2], kEmptyStr, NStr::StringToNumeric<int>(parts[4]));
        prim_out << id.AsFastaString() << endl;
        if (++count >= m_MaxIds) break;
    }
}


void CPerfTestApp::x_LoadIds(CNcbiIstream& in, TIds& ids, bool filter)
{
    CStopWatch sw;
    sw.Start();

    while (in.good() && !in.eof()) {
        string line;
        getline(in, line);
        NStr::TruncateSpacesInPlace(line);
        if (line.empty()) continue;
        CSeq_id id(line);
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
        if (filter && m_IgnoreIds.find(idh) != m_IgnoreIds.end()) continue;
        ids.insert(idh);
        if (filter && ids.size() >= m_MaxIds) break;
    }
}


bool CPerfTestApp::TestId()
{
    CSeq_id_Handle idh = x_NextId();
    if (!idh) return false;
    CAtomicOut atomic_out;
    if (m_PrintInfo || m_PrintData) {
        atomic_out << "ID: " << idh.AsString() << endl;
    }
    try {
        if (m_GetInfo) {
            CScope::TIds ids = m_Scope->GetIds(idh);
            if (ids.empty()) {
                atomic_out << "Failed to get synonyms for " << idh.AsString() << endl;
                CFastMutexGuard guard(m_FailMutex);
                ++m_Failed;
                m_IgnoreIds.insert(idh);
                return true;
            }
            CSeq_inst::TMol moltype = m_Scope->GetSequenceType(idh);
            TSeqPos len = m_Scope->GetSequenceLength(idh);
            int taxid = m_Scope->GetTaxId(idh);
            int hash = m_Scope->GetSequenceHash(idh);
            vector<string> synonyms;
            ITERATE(CScope::TIds, it, ids) {
                synonyms.push_back(it->AsString());
            }
            sort(synonyms.begin(), synonyms.end());
            if (m_PrintInfo) {
                atomic_out << "  Synonyms:";
                ITERATE(vector<string>, it, synonyms) {
                    atomic_out << " " << *it;
                }
                atomic_out << endl;
                atomic_out << "  mol-type=" << moltype << "; len=" << len << "; taxid=" << taxid << "; hash=" << hash << endl;
            }
        }
        if (m_GetData) {
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(idh);
            if (!bh) {
                atomic_out << "Failed to load bioseq " << idh.AsString() << endl;
                CFastMutexGuard guard(m_FailMutex);
                ++m_Failed;
                m_IgnoreIds.insert(idh);
                return true;
            }
            if (m_PrintBlobId) {
                atomic_out << "  Blob-id: " << bh.GetTSE_Handle().GetBlobId() << endl;
            }
            CConstRef<CSeq_entry> entry = bh.GetTopLevelEntry().GetCompleteSeq_entry();
            if (m_PrintData) {
                atomic_out << MSerial_AsnText << *entry;
            }
        }
    }
    catch (CException& ex) {
        ERR_POST("Error testing seq-id " << idh.AsString() << " : " << ex);
        CFastMutexGuard guard(m_FailMutex);
        ++m_Failed;
        m_IgnoreIds.insert(idh);
    }
    if (m_PrintInfo || m_PrintData) {
        atomic_out << endl;
    }
    return true;
}


END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CPerfTestApp().AppMain(argc, argv);
}
