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
* Authors:  Aleksey Grichenko
*
* File Description:
*   Test object manager with LDS2 data loader
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serial.hpp>

#include <util/compress/compress.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/lds2/lds2_db.hpp>
#include <objtools/lds2/lds2.hpp>
#include <objtools/data_loaders/lds2/lds2_dataloader.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CLDS2TestApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);

    // subdir and src_name are relative to m_SrcDir
    void x_ConvertDir(const string& subdir);
    void x_ConvertFile(const string& rel_name);

    void x_TestDatabase(const string& id);
    void x_InitStressTest(void);
    void x_RunStressTest(void);

    string                      m_DbFile;
    string                      m_SrcDir;
    string                      m_DataDir;
    string                      m_FmtName;
    ESerialDataFormat           m_Fmt;
    bool                        m_RunStress;
    bool                        m_GZip;
    CRef<CLDS2_Manager>         m_Mgr;
};


void CLDS2TestApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "LDS2 test program");

    arg_desc->AddDefaultKey("src_dir", "SrcDir",
        "Directory with the source text ASN.1 data files.",
        CArgDescriptions::eString, "lds2_data/asn");

    arg_desc->AddOptionalKey("db", "DbFile",
        "LDS2 database file name.",
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("format", "DataFormat",
        "Data format to use. If other than asn, the source data will be " \
        "converted and saved to the sub-directory named by the format.",
        CArgDescriptions::eString);
    arg_desc->SetConstraint("format", 
        &(*new CArgAllow_Strings, "asnb", "xml", "fasta"),
        CArgDescriptions::eConstraint);

    arg_desc->AddOptionalKey("id", "SeqId",
        "Seq-id to use in the database test.",
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("group_aligns", "group_size",
        "Group standalone seq-aligns into blobs",
        CArgDescriptions::eInteger);

    arg_desc->AddFlag("stress", "Run stress test.");

    arg_desc->AddFlag("gzip", "Use gzip compression for data.");

    arg_desc->AddFlag("readonly", "Test an existing database in read-only mode.");

    SetupArgDescriptions(arg_desc.release());
}


void CLDS2TestApplication::x_ConvertDir(const string& subdir)
{
    CDir src(CDirEntry::ConcatPath(m_SrcDir, subdir));
    CDir::TEntries subs = src.GetEntries("*", CDir::fIgnoreRecursive);
    ITERATE(CDir::TEntries, it, subs) {
        const CDirEntry& sub = **it;
        string rel = CDirEntry::CreateRelativePath(m_SrcDir, sub.GetPath());
        if ( sub.IsDir() ) {
            x_ConvertDir(rel);
        }
        else if ( sub.IsFile() ) {
            x_ConvertFile(rel);
        }
    }
}


void CLDS2TestApplication::x_ConvertFile(const string& rel_name)
{
    CFile src(CDirEntry::ConcatPath(m_SrcDir, rel_name));

    CFile tmp(CDirEntry::ConcatPath(m_DataDir, rel_name));
    CFile dst(CDirEntry::MakePath(tmp.GetDir(), tmp.GetBase(), m_FmtName));
    CDir(tmp.GetDir()).CreatePath();

    cout << "Converting " << src.GetName() << " to " << dst.GetName() << endl;

    CNcbiIfstream fin(src.GetPath().c_str(), ios::binary | ios::in);
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, fin));

    CNcbiOfstream fout(dst.GetPath().c_str(), ios::binary | ios::out);
    auto_ptr<CObjectOStream> out;
    if ( m_GZip ) {
        auto_ptr<CCompressionOStream> zout(new CCompressionOStream(fout,
            new CZipStreamCompressor(CZipCompression::eLevel_Default,
            CZipCompression::fGZip)));
        out.reset(CObjectOStream::Open(m_Fmt, *zout.release(),
            eTakeOwnership));
    }
    else {
        out.reset(CObjectOStream::Open(m_Fmt, fout));
    }
    CObjectStreamCopier cp(*in, *out);

    while ( in->HaveMoreData() ) {
        try {
            string type_name = in->ReadFileHeader();
            if (type_name == "Seq-entry") {
                cp.Copy(CType<CSeq_entry>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else if (type_name == "Bioseq") {
                cp.Copy(CType<CBioseq>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else if (type_name == "Bioseq-set") {
                cp.Copy(CType<CBioseq_set>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else if (type_name == "Seq-annot") {
                cp.Copy(CType<CSeq_annot>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else if (type_name == "Seq-align") {
                cp.Copy(CType<CSeq_align>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else if (type_name == "Seq-align-set") {
                cp.Copy(CType<CSeq_align_set>().GetTypeInfo(),
                    CObjectStreamCopier::eNoFileHeader);
            }
            else {
                ERR_POST("Unknown object in the source data: " << type_name);
                break;
            }
        }
        catch (CEofException) {
            break;
        }
    }
}


void CLDS2TestApplication::x_TestDatabase(const string& id)
{
    CSeq_id seq_id(id);
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(seq_id);
    cout << "Testing LDS2 database, seq-id: " << idh.AsString() << endl;

    string sep = "";
    CLDS2_Database::TBlobSet blobs;

    // If the manager does not exist, run in read-only mode.
    CRef<CLDS2_Database> db(
        m_Mgr ? m_Mgr->GetDatabase() : new CLDS2_Database(m_DbFile, CLDS2_Database::eRead));
    SLDS2_Blob blob = db->GetBlobInfo(idh);
    cout << "Main blob id: " << blob.id;
    if (blob.id > 0) {
        SLDS2_File file = db->GetFileInfo(blob.file_id);
        cout << " in " << file.name << " @ " << blob.file_pos << endl;
        cout << "LDS2 synonyms for the bioseq: ";
        CLDS2_Database::TLdsIdSet ids;
        db->GetSynonyms(idh, ids);
        ITERATE(CLDS2_Database::TLdsIdSet, it, ids) {
            cout << sep << *it;
            sep = ", ";
        }
        cout << endl;
    }
    else {
        cout << " (unknown seq-id or multiple bioseqs found)" << endl;
        cout << "All blobs with bioseq: ";
        db->GetBioseqBlobs(idh, blobs);
        sep = "";
        ITERATE(CLDS2_Database::TBlobSet, it, blobs) {
            cout << sep << it->id;
            sep = ", ";
        }
        cout << endl;
    }

    blobs.clear();
    cout << "Blobs with internal annots: ";
    db->GetAnnotBlobs(idh, CLDS2_Database::fAnnot_Internal, blobs);
    sep = "";
    ITERATE(CLDS2_Database::TBlobSet, it, blobs) {
        cout << sep << it->id;
        sep = ", ";
    }
    cout << endl;

    blobs.clear();
    cout << "Blobs with external annots: ";
    db->GetAnnotBlobs(idh, CLDS2_Database::fAnnot_External, blobs);
    sep = "";
    ITERATE(CLDS2_Database::TBlobSet, it, blobs) {
        cout << sep << it->id;
        sep = ", ";
    }
    cout << endl;

    blobs.clear();
    cout << "Blobs with any annots for: ";
    db->GetAnnotBlobs(idh, CLDS2_Database::fAnnot_All, blobs);
    sep = "";
    ITERATE(CLDS2_Database::TBlobSet, it, blobs) {
        cout << sep << it->id;
        sep = ", ";
    }
    cout << endl;
}


const TIntId kStressTestFiles = 50;
const TIntId kStressTestEntriesPerFile = 20;
const TIntId kStressTestEntries = kStressTestFiles*kStressTestEntriesPerFile;

void CLDS2TestApplication::x_InitStressTest(void)
{
    cout << "Initializing stress test data..." << endl;
    // Create data files
    CSeq_entry e;
    {{
        string src = "./lds2_data/base_entry.asn";
        CNcbiIfstream fin(src.c_str());
        fin >> MSerial_AsnText >> e;
    }}

    m_DataDir = "./lds2_data/stress";
    CDir(m_DataDir).CreatePath();
    if ( m_FmtName.empty() ) {
        m_FmtName = "asn";
    }

    // File index starts with 1 so that there's no gi 0 in the data.
    for (int f = 1; f < kStressTestFiles; f++) {
        string fname = CDirEntry::ConcatPath(m_DataDir,
            "data" + NStr::Int8ToString(f) + "." + m_FmtName);
        CNcbiOfstream fout(fname.c_str(), ios::binary | ios::out);
        auto_ptr<CNcbiOstream> zout;
        CNcbiOstream* out_stream = &fout;
        if ( m_GZip ) {
            zout.reset(new CCompressionOStream(fout,
                new CZipStreamCompressor(CZipCompression::eLevel_Default,
                CZipCompression::fGZip)));
            out_stream = zout.get();
        }
        auto_ptr<CObjectOStream> out;
        auto_ptr<CFastaOstream> fasta_out;
        if (m_FmtName == "fasta") {
            fasta_out.reset(new CFastaOstream(*out_stream));
        }
        else {
            out.reset(CObjectOStream::Open(m_Fmt, *out_stream));
        }

        for (TIntId idx = 0; idx < kStressTestEntriesPerFile; idx++) {
            TGi gi = GI_FROM(TIntId, idx + f*kStressTestEntriesPerFile);
            CSeq_id& id = *e.SetSeq().SetId().front();
            id.SetGi(gi);
            CSeq_feat& feat = *e.SetSeq().SetAnnot().front()->SetData().SetFtable().front();
            feat.SetLocation().SetWhole().SetGi(gi);
            feat.SetProduct().SetWhole().SetGi(gi+GI_FROM(TIntId, 1));
            if ( fasta_out.get() ) {
                fasta_out->Write(e);
            }
            else {
                out->Write(&e, e.GetThisTypeInfo());
            }
        }
    }
}


class CLDS2_TestThread : public CThread
{
public:
    CLDS2_TestThread(int id, int step);

    virtual void* Main(void);

private:
    int     m_Id;
    int     m_Step;
    CScope  m_Scope;
};


CLDS2_TestThread::CLDS2_TestThread(int id, int step)
    : m_Id(id),
      m_Step(step),
      m_Scope(*CObjectManager::GetInstance())
{
    m_Scope.AddDefaults();
}


void* CLDS2_TestThread::Main(void)
{
    TIntId gi = kStressTestEntriesPerFile + m_Id;
    CSeq_id seq_id;
    for (; gi < kStressTestEntries; gi += m_Step) {
        seq_id.SetGi(gi);
        CSeq_id_Handle id = CSeq_id_Handle::GetHandle(seq_id);
        CBioseq_Handle h = m_Scope.GetBioseqHandle(id);
        if ( !h ) {
            ERR_POST(Error << "Failed to fetch gi " << gi);
            _ASSERT(h);
        }

        SAnnotSelector sel;
        sel.SetSearchUnresolved()
            .SetResolveAll();
        CSeq_loc loc;
        loc.SetWhole().Assign(seq_id);
        CFeat_CI fit(m_Scope, loc, sel);
        int fcount = 0;
        for (; fit; ++fit) {
            fcount++;
        }

        sel.SetByProduct(true);
        CFeat_CI fitp(m_Scope, loc, sel);
        fcount = 0;
        for (; fitp; ++fitp) {
            fcount++;
        }
    }
    return 0;
}


void CLDS2TestApplication::x_RunStressTest(void)
{
    cout << "Running stress test" << endl;
    CStopWatch sw(CStopWatch::eStart);
    vector< CRef<CThread> > threads;
    int step = 5; // Number of threads = step for each thread
    for (int i = 0; i < step; i++) {
        CRef<CThread> thr(new CLDS2_TestThread(i, step));
        threads.push_back(thr);
        thr->Run(CThread::fRunAllowST);
    }
    NON_CONST_ITERATE(vector< CRef<CThread> >, it, threads) {
        (*it)->Join();
    }
    double elapsed = sw.Elapsed();
    cout << "Finished stress test in " << elapsed << " sec (" <<
        elapsed/(kStressTestEntries - kStressTestEntriesPerFile) <<
        " per bioseq)" << endl;
}


int CLDS2TestApplication::Run(void)
{
    const CArgs& args(GetArgs());

    m_SrcDir = CDirEntry::CreateAbsolutePath(args["src_dir"].AsString());
    m_DataDir = m_SrcDir;
    if ( args["db"] ) {
        m_DbFile = args["db"].AsString();
    }
    else {
        m_DbFile = CDirEntry::CreateAbsolutePath(
            CDirEntry::ConcatPath(m_SrcDir, "../lds2.db"));
    }

    m_Fmt = eSerial_AsnText;
    m_RunStress = args["stress"];
    m_GZip = args["gzip"];

    if (args["format"]  ||  m_GZip) {
        if ( args["format"] ) {
            m_FmtName = args["format"].AsString();
            if (m_FmtName == "asnb") {
                m_Fmt = eSerial_AsnBinary;
            }
            else if (m_FmtName == "xml") {
                m_Fmt = eSerial_Xml;
            }
            else if (m_FmtName == "fasta") {
                // Only stress test supports fasta format
                if ( !m_RunStress ) {
                    return 0;
                }
            }
        }
        else {
            m_FmtName = "asn";
        }

        string dir_fmt = m_FmtName;
        if ( m_GZip ) {
            dir_fmt += ".gz";
            m_FmtName += ".gz";
        }

        if ( !m_RunStress ) {
            m_DataDir = CDirEntry::NormalizePath(
                CDirEntry::ConcatPath(
                CDirEntry::ConcatPath(m_SrcDir, ".."), dir_fmt));
            x_ConvertDir("");
        }
    }

    if ( m_RunStress ) {
        x_InitStressTest();
    }

    cout << "Converting and indexing data..." << endl;
    m_Mgr.Reset(new CLDS2_Manager(m_DbFile));
    if ( args["group_aligns"] ) {
        m_Mgr->SetSeqAlignGroupSize(args["group_aligns"].AsInteger());
    }
    m_Mgr->ResetData(); // Re-create the database
    m_Mgr->SetGBReleaseMode(CLDS2_Manager::eGB_Guess);

    CStopWatch sw(CStopWatch::eStart);
    m_Mgr->AddDataDir(m_DataDir);
    m_Mgr->UpdateData();
    cout << "Data indexing done in " << sw.Elapsed() << " sec" << endl;

    if (m_DbFile != ":memory:") {
        m_Mgr.Reset();
    }
    else if ( args["readonly"] ) {
        // Reopen database in read-only mode.
        m_Mgr->GetDatabase()->Open(CLDS2_Database::eRead);
    }

    if ( args["id"] ) {
        x_TestDatabase(args["id"].AsString());
    }

    // Run object manager tests
    CRef<CObjectManager> objmgr(CObjectManager::GetInstance());
    if ( m_Mgr ) {
        CLDS2_DataLoader::RegisterInObjectManager(*objmgr,
            *m_Mgr->GetDatabase(),
            -1, CObjectManager::eDefault);
    }
    else {
        CLDS2_DataLoader::RegisterInObjectManager(*objmgr,
            m_DbFile,
            -1, CObjectManager::eDefault);
    }

    if ( m_RunStress ) {
        x_RunStressTest();
    }
    else if ( args["id"] ) {
        CSeq_id seq_id(args["id"].AsString());
        CScope scope(*objmgr);
        scope.AddDefaults();
        //scope.AddDataLoader(CLDS2_DataLoader::GetLoaderNameFromArgs(m_DbFile));

        CBioseq_Handle h = scope.GetBioseqHandle(seq_id);
        if ( !h ) {
            cout << "Seq-id " << seq_id.AsFastaString() <<
                " not found in the database" << endl;
            return 0;
        }

        SAnnotSelector sel;
        sel.SetSearchUnresolved()
            .SetResolveAll();
        CSeq_loc loc;
        loc.SetWhole().Assign(seq_id);
        CFeat_CI fit(scope, loc, sel);
        int fcount = 0;
        for (; fit; ++fit) {
            fcount++;
        }
        cout << "Features with location on " << seq_id.AsFastaString() <<
            ": " << fcount << endl;

        sel.SetByProduct(true);
        CFeat_CI fitp(scope, loc, sel);
        fcount = 0;
        for (; fitp; ++fitp) {
            fcount++;
        }
        cout << "Features with product on " << seq_id.AsFastaString() <<
            ": " << fcount << endl;

        sel.SetByProduct(false);
        CAlign_CI ait(scope, loc, sel);
        int acount = 0;
        for (; ait; ++ait) {
            acount++;
        }
        cout << "Alignments for " << seq_id.AsFastaString() <<
            ": " << acount << endl;
    }

    if (m_RunStress  ||  m_GZip  ||  args["format"]) {
        // Remove converted data if any
        CDir(m_DataDir).Remove();
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    CLDS2TestApplication app;
    return app.AppMain(argc, argv, 0, eDS_Default);
}
