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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   check biosource and structured comment descriptors against biosample database
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiutil.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objectio.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
#include <connect/ncbi_http_session.hpp>

// Objects includes
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/PCRPrimerName.hpp>
#include <objects/seqfeat/PCRPrimerSeq.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/submit/Contact_info.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>
#include <objects/seqtable/SeqTable_column_info.hpp>
#include <util/line_reader.hpp>
#include <util/compress/stream_util.hpp>
#include <util/format_guess.hpp>

#include <objects/seqset/Bioseq_set.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#ifdef HAVE_NCBI_VDB
#  include <sra/data_loaders/wgs/wgsloader.hpp>
#endif
#include <misc/jsonwrapp/jsonwrapp.hpp>
#include <misc/xmlwrapp/xmlwrapp.hpp>


#include <misc/biosample_util/biosample_util.hpp>
#include <misc/biosample_util/struc_table_column.hpp>

#if __has_include(<multicall/multicall.hpp>)
    #include <multicall/multicall.hpp>
#else
    #define biosample_chk_app_main main
#endif

using namespace ncbi;
using namespace objects;
using namespace xml;

const char * BIOSAMPLE_CHK_APP_VER = "1.0";

/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CBiosampleHandler
{
public:
    CBiosampleHandler() :
        m_ReportStream(0),
        m_UseDevServer(false)
        {}

    virtual ~CBiosampleHandler() {}

    virtual void ProcessBioseq(CBioseq_Handle bh) {}
    virtual bool NeedsReportStream() { return false; }
    virtual void AddSummary() {}

    void SetReportStream(CNcbiOstream* stream) { m_ReportStream = stream; }

protected:
    CNcbiOstream* m_ReportStream;
    bool m_UseDevServer;
};


class CBiosampleStatusReport : public CBiosampleHandler
{
public:
    CBiosampleStatusReport() : CBiosampleHandler() {}
    virtual ~CBiosampleStatusReport() {}
    virtual void ProcessBioseq(CBioseq_Handle bh);
    virtual bool NeedsReportStream() { return true; }
    virtual void AddSummary();

protected:
    biosample_util::TStatuses m_Status;
};


void CBiosampleStatusReport::ProcessBioseq(CBioseq_Handle bsh)
{
    vector<string> ids = biosample_util::GetBiosampleIDs(bsh);
    if (ids.empty()) {
        return;
    }

    for (const auto &it : ids) {
        if (m_Status.find(it) == m_Status.end()) {
            biosample_util::TStatus new_pair(it, biosample_util::eStatus_Unknown);
            m_Status.insert(new_pair);
        }
    }
}

void CBiosampleStatusReport::AddSummary()
{
    if (m_Status.empty()) {
        *m_ReportStream << "No BioSample IDs found" << endl;
    } else {
        biosample_util::GetBiosampleStatus(m_Status, m_UseDevServer);
        biosample_util::TStatuses::iterator it = m_Status.begin();
        while (it != m_Status.end()) {
            *m_ReportStream << it->first << "\t" << biosample_util::GetBiosampleStatusName(it->second) << endl;
            ++it;
        }
    }
    m_Status.clear();
}


class CBiosampleChkApp : public CReadClassMemberHook, public CNcbiApplication
{
public:
    CBiosampleChkApp(void);

    virtual void Init(void);
    virtual int  Run (void);

    void ReadClassMember(CObjectIStream& in,
        const CObjectInfo::CMemberIterator& member);

private:

    void Setup(const CArgs& args);

    unique_ptr<CObjectIStream> OpenFile(const CArgs& args);
    unique_ptr<CObjectIStream> OpenFile(const string &fname);
    void SaveFile(const string &fname, bool useBinaryOutputFormat);

    void GetBioseqDiffs(CBioseq_Handle bh);
    void PushToRecord(CBioseq_Handle bh);

    void ProcessBioseqForUpdate(CBioseq_Handle bh);
    void ProcessBioseqHandle(CBioseq_Handle bh);
    void ProcessSeqEntry(CRef<CSeq_entry> se);
    void ProcessSeqEntry(void);
    void ProcessSet(void);
    void ProcessSeqSubmit(void);
    void ProcessAsnInput (void);
    void ProcessList (const string& fname);
    void ProcessFileList (const string& fname);
    int ProcessOneDirectory(const string& dir_name, const string& file_suffix, const string& file_mask, bool recurse);
    void ProcessOneFile(string fname);
    void ProcessReleaseFile(const CArgs& args);
    CRef<CSeq_entry> ReadSeqEntry(void);
    CRef<CBioseq_set> ReadBioseqSet(void);

    void CreateBiosampleUpdateWebService(biosample_util::TBiosampleFieldDiffList& diffs, bool del_okay);
    void PrintResults(biosample_util::TBiosampleFieldDiffList& diffs);
    void PrintDiffs(biosample_util::TBiosampleFieldDiffList& diffs);
    void PrintTable(CRef<CSeq_table> table);

    CRef<CScope> BuildScope(void);

    // for mode 3, biosample_push
    void UpdateBioSource (CBioseq_Handle bh, const CBioSource& src);
    vector<CRef<CSeqdesc> > GetBiosampleDescriptors(string fname);
    vector<CRef<CSeqdesc> > GetBiosampleDescriptorsFromSeqSubmit();
    vector<CRef<CSeqdesc> > GetBiosampleDescriptorsFromSeqEntry();
    vector<CRef<CSeqdesc> > GetBiosampleDescriptorsFromSeqEntry(const CSeq_entry& se);

    CRef<CObjectManager> m_ObjMgr;
    unique_ptr<CObjectIStream> m_In;
    bool m_Continue;

    size_t m_Level;

    CNcbiOstream* m_ReportStream;
    bool m_NeedReportHeader;
    CNcbiOfstream* m_AsnOut;
    CNcbiOstream* m_LogStream;

    enum E_Mode {
        e_report_diffs = 1,     // Default - report diffs between biosources on records with biosample accessions
                                // and biosample data
        e_generate_biosample,
        e_push,
        e_take_from_biosample,         // update with qualifiers from BioSample, stop if conflict
        e_take_from_biosample_force,   // update with qualifiers from BioSample, no stop on conflict
        e_report_status,               // make table with list of BioSample IDs and statuses
        e_update_with,                 // use web API for update (with delete)
        e_update_no                    // use web API for update (no delete)
    };

    enum E_ListType {
        e_none = 0,
        e_accessions,
        e_files
    };

    int m_Mode;
    int m_ReturnCode;
    int m_ListType;
    string m_StructuredCommentPrefix;
    bool m_CompareStructuredComments;
    bool m_UseDevServer;
    bool m_FirstSeqOnly;
    string m_IDPrefix;
    string m_HUPDate;
    string m_BioSampleAccession;
    string m_BioProjectAccession;
    string m_Owner;
    string m_Comment;

    string m_BioSampleWebAPIKey;

    size_t m_Processed;
    size_t m_Unprocessed;

    biosample_util::TBiosampleFieldDiffList m_Diffs;
    CRef<CSeq_table> m_Table;
    vector<CRef<CSeqdesc> > m_Descriptors;

    CBiosampleHandler * m_Handler;

    biosample_util::TBioSamples m_cache;
};


CBiosampleChkApp::CBiosampleChkApp(void) :
    m_ObjMgr(0), m_Continue(false),
    m_Level(0), m_ReportStream(0), m_NeedReportHeader(true), m_AsnOut(0),
    m_LogStream(0), m_Mode(e_report_diffs), m_ReturnCode(0),
    m_StructuredCommentPrefix(""), m_CompareStructuredComments(true),
    m_FirstSeqOnly(false), m_IDPrefix(""), m_HUPDate(""),
    m_BioSampleAccession(""), m_BioProjectAccession(""),
    m_Owner(""), m_Comment(""),
    m_Processed(0), m_Unprocessed(0), m_Handler(NULL)
{
    SetVersion(CVersionInfo(1, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY));
}


void CBiosampleChkApp::Init(void)
{
    // Prepare command line descriptions

    // Create
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddOptionalKey
        ("p", "Directory", "Path to ASN.1 Files",
        CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey
        ("i", "InFile", "Single Input File",
        CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey(
        "o", "OutFile", "Single Output File",
        CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey(
        "f", "Filter", "Substring Filter",
        CArgDescriptions::eOutputFile);
    arg_desc->AddDefaultKey
        ("x", "String", "File Selection Substring", CArgDescriptions::eString, ".sqn");
    arg_desc->AddFlag("u", "Recurse");
    arg_desc->AddFlag("d", "Use development Biosample server");

    arg_desc->AddDefaultKey("a", "a",
                            "ASN.1 Type (a Automatic, z Any, e Seq-entry, b Bioseq, s Bioseq-set, m Seq-submit, t Batch Bioseq-set, u Batch Seq-submit) or accession list (l)",
                            CArgDescriptions::eString,
                            "a");

    arg_desc->AddFlag("b", "Output binary ASN.1");
    //arg_desc->AddFlag("c", "Batch File is Compressed");
    arg_desc->AddFlag("M", "Process only first sequence in file (master)");
    arg_desc->AddOptionalKey("R", "BioSampleIDPrefix", "BioSample ID Prefix", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("HUP", "HUPDate", "Hold Until Publish Date", CArgDescriptions::eString);

    arg_desc->AddOptionalKey(
        "L", "OutFile", "Log File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey(
        "m", "mode", "Mode:\n"
        "\t1 create update file\n"
        "\t2 generate file for creating new biosample entries\n"
        "\t3 push source info from one file (-i) to others (-p)\n"
        "\t4 update with source qualifiers from BioSample unless conflict\n"
        "\t5 update with source qualifiers from BioSample (continue with conflict))\n"
        "\t6 report transaction status\n"
        "\t7 use web API for update (with delete)\n"
        "\t8 use web API for update (no delete)\n",
        CArgDescriptions::eInteger, "1");
    CArgAllow* constraint = new CArgAllow_Integers(e_report_diffs, e_update_no);
    arg_desc->SetConstraint("m", constraint);

    arg_desc->AddOptionalKey(
        "P", "Prefix", "StructuredCommentPrefix", CArgDescriptions::eString);

    arg_desc->AddOptionalKey(
        "biosample", "BioSampleAccession", "BioSample Accession to use for sequences in record. Report error if sequences contain a reference to a different BioSample accession.", CArgDescriptions::eString);
    arg_desc->AddOptionalKey(
        "bioproject", "BioProjectAccession", "BioProject Accession to use for sequences in record. Report error if sequences contain a reference to a different BioProject accession.", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("comment", "BioSampleComment", "Comment to use for creating new BioSample xml", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("apikey_file", "BioSampleWebAPIKey", "File containing Web API Key needed to update BioSample database", CArgDescriptions::eString);

    // Program description
    string prog_description = "BioSample Checker\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());

}


void CBiosampleChkApp::ProcessAsnInput (void)
{
    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    string header = m_In->ReadFileHeader();

    bool unhandled = false;
    try {
        if (header == "Seq-submit" ) {  // Seq-submit
            ProcessSeqSubmit();
        } else if ( header == "Seq-entry" ) {           // Seq-entry
            ProcessSeqEntry();
        } else if (header == "Bioseq-set" ) {  // Bioseq-set
            ProcessSet();
        } else {
            unhandled = true;
        }
    } catch (CException& e) {
        if (NStr::StartsWith(e.GetMsg(), "duplicate Bioseq id")) {
            *m_LogStream << e.GetMsg();
            exit(4);
        } else {
            throw e;
        }
    }
    if (unhandled) {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
    }

}


void CBiosampleChkApp::ProcessList (const string& fname)
{
    // Process file with list of accessions

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*objmgr);
#ifdef HAVE_NCBI_VDB
    CWGSDataLoader::RegisterInObjectManager(*objmgr);
#endif
    CScope scope(*objmgr);
    scope.AddDefaults();

    CRef<ILineReader> lr = ILineReader::New (fname);
    while ( !lr->AtEOF() ) {
        CTempString line = *++*lr;
        if (!NStr::IsBlank(line)) {
            try {
                CRef<CSeq_id> id(new CSeq_id(line));
                if (id) {
                    CBioseq_Handle bsh = scope.GetBioseqHandle(*id);
                    if (bsh) {
                        ProcessBioseqHandle(bsh);
                    } else {
                        *m_LogStream << "Unable to fetch Bioseq for " << line << endl;
                        string label = "";
                        id->GetLabel(&label);
                        *m_LogStream << "  (interpreted as " << label << ")" << endl;
                        m_Unprocessed++;
                    }
                }
            } catch (CException& e) {
                *m_LogStream << e.GetMsg() << endl;
                m_Unprocessed++;
            }
        }
    }

}


void CBiosampleChkApp::ProcessFileList (const string& fname)
{
    // Process file with list of files

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*objmgr);
    CScope scope(*objmgr);
    scope.AddDefaults();

    m_ListType = e_none;
    CRef<ILineReader> lr = ILineReader::New (fname);
    while ( !lr->AtEOF() ) {
        CTempString line = *++*lr;
        if (!NStr::IsBlank(line)) {
            ProcessOneFile(line);
        }
    }
    m_ListType = e_files;
}


void CBiosampleChkApp::ProcessOneFile(string fname)
{
    const CArgs& args = GetArgs();

    bool need_to_close_report = false;
    bool need_to_close_asn = false;

    if (!m_ReportStream &&
        (m_Mode == e_report_diffs || m_Mode == e_update_with || m_Mode == e_update_no || m_Mode == e_take_from_biosample || m_Mode == e_report_status ||
         (m_Handler != NULL && m_Handler->NeedsReportStream()))) {
        string path = fname;
        size_t pos = NStr::Find(path, ".", NStr::eCase, NStr::eReverseSearch);
        if (pos != string::npos) {
            path = path.substr(0, pos);
        }
        path = path + ".val";
        m_Table.Reset(new CSeq_table());
        m_Table->SetNum_rows(0);
        m_ReportStream = new CNcbiOfstream(path.c_str());
        if (!m_ReportStream)
        {
            NCBI_THROW(CException, eUnknown, "Unable to open " + path);
        }
        need_to_close_report = true;
        m_NeedReportHeader = true;
        if (m_Handler && m_Handler->NeedsReportStream()) {
            m_Handler->SetReportStream(m_ReportStream);
        }
    }
    if (!m_AsnOut && (m_Mode == e_push || m_Mode == e_take_from_biosample || m_Mode == e_take_from_biosample_force)) {
        string path = fname;
        size_t pos = NStr::Find(path, ".", NStr::eCase, NStr::eReverseSearch);
        if (pos != string::npos) {
            path = path.substr(0, pos);
        }
        path = path + ".out";
        SaveFile(path, args["b"]);
        need_to_close_asn = true;
    }

    m_Diffs.clear();
    switch (m_ListType) {
        case e_accessions:
            ProcessList (fname);
            break;
        case e_files:
            ProcessFileList (fname);
            break;
        case e_none:
            m_In = OpenFile(fname);
            if (m_In.get() == nullptr) {
                NCBI_THROW(CException, eUnknown, "Unable to open " + fname);
            }
            if (!m_In->InGoodState()) {
                NCBI_THROW(CException, eUnknown, "Unable to open " + fname);
            }
            ProcessAsnInput();
            break;
    }

    if (m_Mode == e_report_diffs) {
        PrintResults(m_Diffs);
    }
    if (m_Mode == e_update_with) {
        CreateBiosampleUpdateWebService(m_Diffs, true);
    } else if (m_Mode == e_update_no) {
        CreateBiosampleUpdateWebService(m_Diffs, false);
    }
    if (m_Handler != NULL) {
        m_Handler->AddSummary();
    }

    // TODO! Must free diffs
    m_Diffs.clear();

    if (need_to_close_report) {
        if (m_Mode == e_take_from_biosample) {
            PrintTable(m_Table);
            m_Table->Reset();
            m_Table = new CSeq_table();
            m_Table->SetNum_rows(0);
        }
        m_ReportStream->flush();
        m_ReportStream = 0;
    }
    if (need_to_close_asn) {
        m_AsnOut->flush();
        m_AsnOut->close();
        m_AsnOut = 0;
    }
}


vector<CRef<CSeqdesc> > CBiosampleChkApp::GetBiosampleDescriptorsFromSeqEntry(void)
{
    // Get seq-entry to process
    CRef<CSeq_entry> se(ReadSeqEntry());

    return GetBiosampleDescriptorsFromSeqEntry(*se);
}


vector<CRef<CSeqdesc> > CBiosampleChkApp::GetBiosampleDescriptorsFromSeqEntry(const CSeq_entry& se)
{
    vector<CRef<CSeqdesc> > descriptors;

    CRef<CScope> scope = BuildScope();
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(se);
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    if (bi) {
        CSeqdesc_CI src_desc_ci(*bi, CSeqdesc::e_Source);
        if (src_desc_ci) {
            CRef<CSeqdesc> src_desc(new CSeqdesc());
            src_desc->Assign(*src_desc_ci);
            descriptors.push_back(src_desc);
        }
    }

    return descriptors;
}


vector<CRef<CSeqdesc> > CBiosampleChkApp::GetBiosampleDescriptorsFromSeqSubmit()
{
    vector<CRef<CSeqdesc> > descriptors;
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to process
    m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);

    // Validae Seq-submit
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys() && ! ss->GetData().GetEntrys().empty()) {
        descriptors = GetBiosampleDescriptorsFromSeqEntry(**(ss->GetData().GetEntrys().begin()));
    }
    return descriptors;
}


vector<CRef<CSeqdesc> > CBiosampleChkApp::GetBiosampleDescriptors(string fname)
{
    m_In = OpenFile(fname);

    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.

    string header = m_In->ReadFileHeader();

    vector<CRef<CSeqdesc> > descriptors;
    if (header == "Seq-submit" ) {  // Seq-submit
        descriptors = GetBiosampleDescriptorsFromSeqSubmit();
    } else if ( header == "Seq-entry" ) {           // Seq-entry
        descriptors = GetBiosampleDescriptorsFromSeqEntry();

    } else {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
    }
    return descriptors;
}


int CBiosampleChkApp::ProcessOneDirectory(const string& dir_name, const string& file_suffix, const string& file_mask, bool recurse)
{
    int num_of_files = 0;

    CDir dir(dir_name);
    CDir::TEntries files (dir.GetEntries(file_mask, CDir::eFile));
    for (const auto &ii : files) {
        string fname = ii->GetName();
        if (ii->IsFile() &&
            (!file_suffix.empty() || NStr::Find (fname, file_suffix) != string::npos)) {
            ++num_of_files;
            string fname = CDirEntry::MakePath(dir_name, ii->GetName());
            ProcessOneFile (fname);
        }
    }
    if (recurse) {
        CDir::TEntries subdirs (dir.GetEntries("", CDir::eDir));
        for (const auto &ii : subdirs) {
            string subdir = ii->GetName();
            if (ii->IsDir() && !NStr::Equal(subdir, ".") && !NStr::Equal(subdir, "..")) {
                string subname = CDirEntry::MakePath(dir_name, ii->GetName());
                num_of_files += ProcessOneDirectory (subname, file_suffix, file_mask, recurse);
            }
        }
    }
    if (!num_of_files)
    {
        NCBI_THROW(CException, eUnknown, "No input '" + file_mask + "' files found in directory '" + dir_name + "'");
    }
    return num_of_files;
}


int CBiosampleChkApp::Run(void)
{
    const CArgs& args = GetArgs();
    Setup(args);

    m_Mode = args["m"].AsInteger();
    m_FirstSeqOnly = args["M"].AsBoolean();
    m_IDPrefix = args["R"] ? args["R"].AsString() : "";
    m_HUPDate = args["HUP"] ? args["HUP"].AsString() : "";
    m_BioSampleAccession = args["biosample"] ? args["biosample"].AsString() : "";
    m_BioProjectAccession = args["bioproject"] ? args["bioproject"].AsString() : "";
    m_Comment = args["comment"] ? args["comment"].AsString() : "";

    string apikey_file = args["apikey_file"] ? args["apikey_file"].AsString() : "";
    if (!apikey_file.empty()) {
        ifstream is(apikey_file.c_str());
        is >> m_BioSampleWebAPIKey;
    }

    if (m_Mode == e_report_status) {
        m_Handler = new CBiosampleStatusReport();
    }

    if (args["o"]) {
        if (m_Mode == e_report_diffs || m_Mode == e_generate_biosample
            //|| m_Mode == e_take_from_biosample
            || (m_Handler != NULL && m_Handler->NeedsReportStream())) {
            m_ReportStream = &(args["o"].AsOutputFile());
            if (!m_ReportStream)
            {
                NCBI_THROW(CException, eUnknown, "Unable to open " + args["o"].AsString());
            }
            if (m_Handler) {
                m_Handler->SetReportStream(m_ReportStream);
            }
            if (m_Mode == e_take_from_biosample) {
                m_Table.Reset(new CSeq_table());
                m_Table->SetNum_rows(0);
            }
        } else {
            SaveFile(args["o"].AsString(), args["b"]);
        }
    } else if (m_Mode == e_update_with || m_Mode == e_update_no) {
            m_ReportStream = &NcbiCout;
            if (!m_ReportStream)
            {
                NCBI_THROW(CException, eUnknown, "Unable to open " + args["o"].AsString());
            }
            if (m_Handler) {
                m_Handler->SetReportStream(m_ReportStream);
            }
            if (m_Mode == e_take_from_biosample) {
                m_Table.Reset(new CSeq_table());
                m_Table->SetNum_rows(0);
            }
    }

    m_LogStream = args["L"] ? &(args["L"].AsOutputFile()) : &NcbiCout;
    m_StructuredCommentPrefix = args["P"] ? args["P"].AsString() : "";
    if (!NStr::IsBlank(m_StructuredCommentPrefix) && !NStr::StartsWith(m_StructuredCommentPrefix, "##")) {
        m_StructuredCommentPrefix = "##" + m_StructuredCommentPrefix;
    }

    m_UseDevServer = args["d"].AsBoolean();

    if (!NStr::IsBlank(m_StructuredCommentPrefix) && m_Mode != e_generate_biosample) {
        // error
        *m_LogStream << "Structured comment prefix is only appropriate for generating a biosample table." << endl;
        return 1;
    }

    if (m_Mode == e_report_diffs) {
        m_CompareStructuredComments = false;
    }

    // Process file based on its content
    // Unless otherwise specified we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    if (NStr::Equal(args["a"].AsString(), "l")) {
        m_ListType = e_accessions;
    } else if (NStr::Equal(args["a"].AsString(), "f")) {
        m_ListType = e_files;
    } else {
        m_ListType = e_none;
    }

    string dir_name    = (args["p"]) ? args["p"].AsString() : "";
    string file_suffix = (args["f"]) ? args["f"].AsString() : "";
    string file_mask   = (args["x"]) ? args["x"].AsString() : ".sqn";
    file_mask = "*" + file_mask;
    bool dir_recurse   = args["u"];
    if (m_Mode == e_report_status && !NStr::IsBlank(m_BioSampleAccession)) {
        biosample_util::EStatus status = biosample_util::GetBiosampleStatus(m_BioSampleAccession, m_UseDevServer);
        if (m_ReportStream) {
            *m_ReportStream << m_BioSampleAccession << "\t" << biosample_util::GetBiosampleStatusName(status) << endl;
        } else {
            NcbiCout << m_BioSampleAccession << "\t" << biosample_util::GetBiosampleStatusName(status) << endl;
        }
    } else if ( m_Mode == e_push) {
        if (m_ListType != e_none) {
            // error
            *m_LogStream << "List type (-a l or -a f) is not appropriate for push mode." << endl;
            return 1;
        } else if (!args["p"] || !args["i"]) {
            // error
            *m_LogStream << "Both directory containing contigs (-p) and master file (-i) are required for push mode." << endl;
            return 1;
        } else {
            m_Descriptors = GetBiosampleDescriptors(args["i"].AsString());
            ProcessOneDirectory (dir_name, file_suffix, file_mask, dir_recurse);
        }
    } else if ( args["p"] ) {
        ProcessOneDirectory (dir_name, file_suffix, file_mask, dir_recurse);
        if (m_Mode == e_take_from_biosample) {
            if (m_Table && m_Table->GetNum_rows() > 0) {
                PrintTable(m_Table);
            }
        }
    } else {
        if (args["i"]) {
            ProcessOneFile (args["i"].AsString());
            if (m_Mode == e_take_from_biosample) {
                if (m_Table && m_Table->GetNum_rows() > 0) {
                    PrintTable(m_Table);
                }
            }
        }
    }

    if (m_Unprocessed > 0) {
        if (m_Mode != e_report_diffs) {
            *m_LogStream << m_Unprocessed << " results failed" << endl;
        }
        return 1;
    } else {
        return m_ReturnCode;
    }
}


CRef<CScope> CBiosampleChkApp::BuildScope (void)
{
    CRef<CScope> scope(new CScope (*m_ObjMgr));
    scope->AddDefaults();

    return scope;
}


void CBiosampleChkApp::ReadClassMember
(CObjectIStream& in,
 const CObjectInfo::CMemberIterator& member)
{
    m_Level++;

    if ( m_Level == 1 ) {
        size_t n = 0;
        // Read each element separately to a local TSeqEntry,
        // process it somehow, and... not store it in the container.
        for ( CIStreamContainerIterator i(in, member); i; ++i ) {
            try {
                // Get seq-entry to process
                CRef<CSeq_entry> se(new CSeq_entry);
                i >> *se;

                CStopWatch sw(CStopWatch::eStart);

                m_Diffs.clear();
                ProcessSeqEntry(se);
                PrintResults(m_Diffs);
                // TODO! Must free diffs
                m_Diffs.clear();

                if (m_ReportStream) {
                    *m_ReportStream << "Elapsed = " << sw.Elapsed() << endl;
                }
                n++;
            } catch (std::exception e) {
                if ( !m_Continue ) {
                    throw;
                }
                // should we issue some sort of warning?
            }
        }
    } else {
        in.ReadClassMember(member);
    }

    m_Level--;
}


void CBiosampleChkApp::ProcessReleaseFile
(const CArgs& args)
{
    CRef<CBioseq_set> seqset(new CBioseq_set);

    // Register the Seq-entry hook
    CObjectTypeInfo set_type = CType<CBioseq_set>();
    set_type.FindMember("seq-set").SetLocalReadHook(*m_In, this);

    // Read the CBioseq_set, it will call the hook object each time we
    // encounter a Seq-entry
    *m_In >> *seqset;
}


CRef<CSeq_entry> CBiosampleChkApp::ReadSeqEntry(void)
{
    CRef<CSeq_entry> se(new CSeq_entry);
    m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);

    return se;
}


CRef<CBioseq_set> CBiosampleChkApp::ReadBioseqSet(void)
{
    CRef<CBioseq_set> set(new CBioseq_set());
    m_In->Read(ObjectInfo(*set), CObjectIStream::eNoFileHeader);

    return set;
}


void CBiosampleChkApp::PrintTable(CRef<CSeq_table> table)
{
    if (table->GetNum_rows() == 0) {
        // do nothing
        return;
    }

    for (const auto &it : table->GetColumns()) {
        *m_ReportStream << it->GetHeader().GetTitle() << "\t";
    }
    *m_ReportStream << endl;
    for (size_t row = 0; row < (size_t)table->GetNum_rows(); row++) {
        for (const auto &it : table->GetColumns()) {
            if (row < it->GetData().GetString().size()) {
                *m_ReportStream << it->GetData().GetString()[row] << "\t";
            } else {
                *m_ReportStream << "\t";
            }
        }
        *m_ReportStream << endl;
    }
}


void CBiosampleChkApp::PrintDiffs(biosample_util::TBiosampleFieldDiffList & diffs)
{
    if (diffs.empty()) {
        if (m_Processed == 0) {
            *m_ReportStream << "No results processed" << endl;
        } else {
            *m_ReportStream << "No differences found" << endl;
        }
    } else {
        if (m_NeedReportHeader) {
            biosample_util::CBiosampleFieldDiff::PrintHeader(*m_ReportStream, false);
            m_NeedReportHeader = false;
        }

        for (const auto &it : diffs) {
            it->Print(*m_ReportStream, false);
        }
    }
    if (m_Unprocessed > 0) {
        *m_ReportStream << m_Unprocessed << " results failed" << endl;
    }
}


void CBiosampleChkApp::PrintResults(biosample_util::TBiosampleFieldDiffList & diffs)
{
    PrintDiffs(diffs);
}


void CBiosampleChkApp::CreateBiosampleUpdateWebService(biosample_util::TBiosampleFieldDiffList & diffs, bool del_okay)
{
    if (diffs.empty()) {
        return;
    }

    vector< CRef<biosample_util::CBiosampleFieldDiff> > add_item;
    vector< CRef<biosample_util::CBiosampleFieldDiff> > change_item;
    vector< CRef<biosample_util::CBiosampleFieldDiff> > delete_item;
    vector< CRef<biosample_util::CBiosampleFieldDiff> > change_organism;

    set<string> ids;

    for (const auto &it : diffs) {
        string id = it->GetBioSample();
        string smp = it->GetSampleVal();
        string src = it->GetSrcVal();
        string fld = it->GetFieldName();
        bool blank_smp = NStr::IsBlank(smp);
        bool blank_src = NStr::IsBlank(src);
        if (blank_smp && blank_src) {
            continue;
        }
        if (smp == src) {
            continue;
        }
        ids.insert(id);
        if (fld == "Organism Name") {
            change_organism.push_back(it);
        } else if (blank_smp) {
            add_item.push_back(it);
        } else if (blank_src) {
            if (del_okay) {
                delete_item.push_back(it);
            }
        } else {
            change_item.push_back(it);
        }
    }

    CJson_Document req;
    CJson_Object top_obj = req.SetObject();
    CJson_Array biosample_array = top_obj.insert_array("update");

    CJson_Object options_obj = top_obj.insert_object("options");
    options_obj.insert("attribute_synonyms", "true");

    for (auto& id : ids) {
        CJson_Object obj1 = biosample_array.push_back_object();
        obj1.insert("samples", id);

        if (! add_item.empty()) {
            CJson_Object add_obj = obj1.insert_object("add");
            CJson_Array add_arr = add_obj.insert_array("attribute");
            for (auto& itm : add_item) {
                CJson_Object obj2 = add_arr.push_back_object();
                obj2.insert("name", itm->GetFieldName());
                obj2.insert("new_value", itm->GetSrcVal());
            }
        }

        if (! delete_item.empty()) {
            CJson_Object del_obj = obj1.insert_object("delete");
            CJson_Array del_arr = del_obj.insert_array("attribute");
            for (auto& itm : delete_item) {
                CJson_Object obj2 = del_arr.push_back_object();
                obj2.insert("name", itm->GetFieldName());
                obj2.insert("old_value", itm->GetSampleVal());
            }
        }

        if (! change_item.empty() || ! change_organism.empty()) {
            CJson_Object chg_obj = obj1.insert_object("change");
            if (! change_organism.empty()) {
                CJson_Object chg_org = chg_obj.insert_object("organism");
                for (auto& itm : change_organism) {
                    chg_org.insert("new_value", itm->GetSrcVal());
                }
            }
            if (! change_item.empty()) {
                CJson_Array chg_arr = chg_obj.insert_array("attribute");
                for (auto& itm : change_item) {
                    string fld = itm->GetFieldName();
                    if (fld == "Tax ID") {
                        continue;
                    }
                    CJson_Object obj2 = chg_arr.push_back_object();
                    obj2.insert("name", fld);
                    obj2.insert("old_value", itm->GetSampleVal());
                    obj2.insert("new_value", itm->GetSrcVal());
                }
            }
        }
    }

    if ( ids.size() > 1 ) {
        *m_LogStream << "ERROR: More than one BioSample ID is not supported by -m 7." << endl;
        exit(6);
    }

    string sData = req.ToString();

    NcbiCout << sData << endl;

    // BioSample update
    string sUrl = "https://api-int.ncbi.nlm.nih.gov/biosample/update/";
    if (m_UseDevServer) {
        sUrl = "https://dev-api-int.ncbi.nlm.nih.gov/biosample/update/";
    }
    string sContentType = "application/json; charset=utf-8";

    CUrl curl(sUrl);
    CHttpHeaders headers;
    headers.SetValue("NCBI-BioSample-Authorization", m_BioSampleWebAPIKey);
    CHttpResponse response = g_HttpPost(curl, headers, sData, sContentType);

    if (response.GetStatusCode() != 200) {
        NcbiStreamCopy(cout, response.ErrorStream());
        cout << endl;
    } else {
        NcbiStreamCopy(cout, response.ContentStream());
        cout << endl;
    }
}


void CBiosampleChkApp::GetBioseqDiffs(CBioseq_Handle bh)
{
    vector<string> unprocessed_ids;
    biosample_util::TBiosampleFieldDiffList new_diffs =
                  biosample_util::GetBioseqDiffs(bh,
                                       m_BioSampleAccession,
                                       m_Processed,
                                       unprocessed_ids,
                                       m_UseDevServer,
                                       m_CompareStructuredComments,
                                       m_StructuredCommentPrefix,
                                       &m_cache);
    if (! new_diffs.empty()) {
        m_Diffs.insert(m_Diffs.end(), new_diffs.begin(), new_diffs.end());
        for (const auto &id : unprocessed_ids) {
            *m_LogStream << "Failed to retrieve BioSample data for  " << id << endl;
        }
        m_Unprocessed += unprocessed_ids.size();
    }
}


void CBiosampleChkApp::PushToRecord(CBioseq_Handle bh)
{
    for (const auto &it : m_Descriptors) {
        if (it->IsSource()) {
            UpdateBioSource(bh, it->GetSource());
        }
    }
}


void CBiosampleChkApp::ProcessBioseqForUpdate(CBioseq_Handle bh)
{
    vector<string> biosample_ids = biosample_util::GetBiosampleIDs(bh);

    if (!biosample_util::ResolveSuppliedBioSampleAccession(m_BioSampleAccession, biosample_ids)) {
        // error
        string label = biosample_util::GetBestBioseqLabel(bh);
        *m_LogStream << label << " has conflicting BioSample Accession " << biosample_ids[0] << endl;
        return;
    }

    if (biosample_ids.empty()) {
        // for report mode, do not report if no biosample ID
        return;
    }

    for (const auto &id : biosample_ids) {
        CRef<CSeq_descr> descr = biosample_util::GetBiosampleData(id, m_UseDevServer, &m_cache);
        if (descr) {
            m_Descriptors.clear();
            copy(descr->Set().begin(), descr->Set().end(),
                back_inserter(m_Descriptors));
            PushToRecord(bh);
            m_Descriptors.clear();
        }
    }

}


void CBiosampleChkApp::ProcessBioseqHandle(CBioseq_Handle bh)
{
    switch (m_Mode) {
        case e_report_diffs:
            GetBioseqDiffs(bh);
            break;
        case e_generate_biosample:
            try {
                biosample_util::PrintBioseqXML(
                          bh,
                          m_IDPrefix,
                          m_ReportStream,
                          m_BioProjectAccession,
                          m_Owner,
                          m_HUPDate,
                          m_Comment,
                          m_FirstSeqOnly,
                          m_CompareStructuredComments,
                          m_StructuredCommentPrefix);
            } catch (CException& e) {
                *m_LogStream << e.GetMsg() << endl;
            }
            break;
        case e_push:
            PushToRecord(bh);
            break;
        case e_take_from_biosample:
            m_Diffs.clear();
            GetBioseqDiffs(bh);
            if (biosample_util::DoDiffsContainConflicts(m_Diffs, m_LogStream)) {
                m_ReturnCode = 1;
                string sequence_id = biosample_util::GetBestBioseqLabel(bh);
                *m_LogStream << "Conflicts found for  " << sequence_id << endl;
                try {
                    biosample_util::AddBioseqToTable(
                                  bh, *m_Table,
                                  true,
                                  m_CompareStructuredComments,
                                  m_StructuredCommentPrefix);
                } catch (CException& e) {
                    *m_LogStream << e.GetMsg() << endl;
                }
            } else {
                ProcessBioseqForUpdate(bh);
            }
            break;
        case e_take_from_biosample_force:
            ProcessBioseqForUpdate(bh);
            break;
        case e_update_with:
        case e_update_no:
            GetBioseqDiffs(bh);
            break;
        default:
            if (m_Handler != NULL) {
                m_Handler->ProcessBioseq(bh);
            }
            break;
    }

}


void CBiosampleChkApp::ProcessSeqEntry(CRef<CSeq_entry> se)
{
    CRef<CScope> scope = BuildScope();
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*se);
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        ProcessBioseqHandle(*bi);
        if (m_FirstSeqOnly) {
            break;
        }
        ++bi;
    }
    scope->RemoveTopLevelSeqEntry(seh);
}


void CBiosampleChkApp::ProcessSeqEntry(void)
{
    // Get seq-entry to process
    CRef<CSeq_entry> se(ReadSeqEntry());

    ProcessSeqEntry(se);

    // write out copy after processing, if requested
    if (m_AsnOut) {
        *m_AsnOut << *se;
    }
}


void CBiosampleChkApp::ProcessSet(void)
{
    // Get Bioseq-set to process
    CRef<CBioseq_set> set(ReadBioseqSet());
    if (set && set->IsSetSeq_set()) {
        for (const auto &se : set->GetSeq_set()) {
            ProcessSeqEntry(se);
        }
    }

    // write out copy after processing, if requested
    if (m_AsnOut) {
        *m_AsnOut << *set;
    }
}


void CBiosampleChkApp::ProcessSeqSubmit(void)
{
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to process
    m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);

    m_Owner = "";
    // get owner from Seq-submit to use if no pub is found
    if (ss->IsSetSub()) {
        if (ss->GetSub().IsSetCit()
            && ss->GetSub().GetCit().IsSetAuthors()
            && ss->GetSub().GetCit().GetAuthors().IsSetAffil()) {
            m_Owner = biosample_util::OwnerFromAffil(ss->GetSub().GetCit().GetAuthors().GetAffil());
        } else if (ss->GetSub().IsSetContact() && ss->GetSub().GetContact().IsSetContact()
            && ss->GetSub().GetContact().GetContact().IsSetAffil()) {
            m_Owner = biosample_util::OwnerFromAffil(ss->GetSub().GetContact().GetContact().GetAffil());
        }
    }

    // Process Seq-submit
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        for (const auto &se : ss->GetData().GetEntrys()) {
            ProcessSeqEntry(se);
        }
    }
    // write out copy after processing, if requested
    if (m_AsnOut) {
        *m_AsnOut << *ss;
    }
}

static bool s_IsEmptyBioSource(const CSeqdesc& src)
{
    return !src.GetSource().IsSetSubtype() && !src.GetSource().IsSetGenome() && !src.GetSource().IsSetOrigin() &&
        (!src.GetSource().IsSetOrg() || (!src.GetSource().IsSetOrgname() && !src.GetSource().IsSetTaxname() && !src.GetSource().IsSetDivision()));
}

void CBiosampleChkApp::UpdateBioSource (CBioseq_Handle bh, const CBioSource& src)
{
    CSeqdesc_CI src_desc_ci(bh, CSeqdesc::e_Source);

    CBioseq_EditHandle beh = bh.GetEditHandle();
    // Removes empty BioSources
    for (; src_desc_ci;) {

        if (s_IsEmptyBioSource(*src_desc_ci)) {
            const CSeqdesc& cur_descr = *src_desc_ci;
            ++src_desc_ci;
            beh.RemoveSeqdesc(cur_descr);
        }
        else {
            break;
        }
    }

    if (!src_desc_ci) {
        CRef<CSeqdesc> new_desc(new CSeqdesc());
        new_desc->SetSource().Assign(src);
        CBioseq_set_Handle parent = bh.GetParentBioseq_set();

        if (parent && parent.IsSetClass() && parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
            CBioseq_set_EditHandle bseh = parent.GetEditHandle();
            bseh.AddSeqdesc(*new_desc);
        } else {
            beh.AddSeqdesc(*new_desc);
        }
    } else {

        const CBioSource& bs = src_desc_ci->GetSource();
        CBioSource* old_src = const_cast<CBioSource *> (&bs);
        old_src->UpdateWithBioSample(src, true, true);

        // Removes the rest of empty BioSources
        for (++src_desc_ci; src_desc_ci;) {

            if (s_IsEmptyBioSource(*src_desc_ci)) {
                const CSeqdesc& cur_descr = *src_desc_ci;
                ++src_desc_ci;
                beh.RemoveSeqdesc(cur_descr);
            }
            else {
                ++src_desc_ci;
            }
        }
    }
}


void CBiosampleChkApp::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    // Create object manager
    m_ObjMgr = CObjectManager::GetInstance();
}


unique_ptr<CObjectIStream> CBiosampleChkApp::OpenFile(const CArgs& args)
{
    string fname = args["i"].AsString();
    return CBiosampleChkApp::OpenFile(fname);
}

unique_ptr<CObjectIStream> CBiosampleChkApp::OpenFile(const string &fname)
{
    ESerialDataFormat format = eSerial_AsnText;

    unique_ptr<CNcbiIstream> hold_stream(new CNcbiIfstream (fname.c_str(), ios::binary));
    CNcbiIstream* InputStream = hold_stream.get();

    CFormatGuess::EFormat formatGuess = CFormatGuess::Format(*InputStream);

    CCompressStream::EMethod method;
    switch (formatGuess)
    {
        case CFormatGuess::eGZip:  method = CCompressStream::eGZipFile;  break;
        case CFormatGuess::eBZip2: method = CCompressStream::eBZip2;     break;
        case CFormatGuess::eLzo:   method = CCompressStream::eLZO;       break;
        default:                   method = CCompressStream::eNone;      break;
    }
    if (method != CCompressStream::eNone)
    {
        CDecompressIStream* decompress(new CDecompressIStream(*InputStream, method, CCompressStream::fDefault, eTakeOwnership));
        hold_stream.release();
        hold_stream.reset(decompress);
        InputStream = hold_stream.get();
        formatGuess = CFormatGuess::Format(*InputStream);
    }

    unique_ptr<CObjectIStream> objectStream;
    switch (formatGuess)
    {
        case CFormatGuess::eBinaryASN:
            format = eSerial_AsnBinary;
        case CFormatGuess::eTextASN:
            format = eSerial_AsnText;
            objectStream.reset(CObjectIStream::Open(format, *InputStream, eTakeOwnership));
            hold_stream.release();
            break;
        default:
            break;
    }
    return objectStream;
}

void CBiosampleChkApp::SaveFile(const string &fname, bool useBinaryOutputFormat)
{
    ios::openmode mode = ios::out;
    m_AsnOut = new CNcbiOfstream(fname.c_str(), mode);
    if (!m_AsnOut)
    {
        NCBI_THROW(CException, eUnknown, "Unable to open " + fname);
    }
    if ( useBinaryOutputFormat ) {
        *m_AsnOut << MSerial_AsnBinary;
    } else {
        *m_AsnOut << MSerial_AsnText;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int biosample_chk_app_main(int argc, const char* argv[])
{
    return CBiosampleChkApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
