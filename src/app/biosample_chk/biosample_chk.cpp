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
#include <objects/seqfeat/PCRReactionSet.hpp>
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
#include <objects/seqset/Seq_entry.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>
#include <objects/seqtable/SeqTable_column_info.hpp>
#include <util/line_reader.hpp>

#include <objects/seqset/Bioseq_set.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <misc/xmlwrapp/xmlwrapp.hpp>


#include "util.hpp"
#include "src_table_column.hpp"
#include "struc_table_column.hpp"

#include <common/test_assert.h>  /* This header must go last */


using namespace ncbi;
using namespace objects;
using namespace xml;

const char * BIOSAMPLE_CHK_APP_VER = "1.0";

/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CBiosampleChkApp : public CNcbiApplication, CReadClassMemberHook
{
public:
    CBiosampleChkApp(void);

    virtual void Init(void);
    virtual int  Run (void);

    void ReadClassMember(CObjectIStream& in,
        const CObjectInfo::CMemberIterator& member);

private:

    void Setup(const CArgs& args);

    CObjectIStream* OpenFile(const CArgs& args);
    CObjectIStream* OpenFile(string fname, const CArgs& args);

    void GetBioseqDiffs(CBioseq_Handle bh);
    bool DoDiffsContainConflicts();
    void AddBioseqToTable(CBioseq_Handle bh, bool with_id = false);
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
    void ProcessOneDirectory(const string& dir_name, bool recurse);
    void ProcessOneFile(string fname);
    void ProcessReleaseFile(const CArgs& args);
    CRef<CSeq_entry> ReadSeqEntry(void);
    CRef<CBioseq_set> ReadBioseqSet(void);

    void PrintResults(TBiosampleFieldDiffList& diffs);
    void PrintDiffs(TBiosampleFieldDiffList& diffs);
    void PrintTable(CRef<CSeq_table> table);
    void PrintBioseqXML(CBioseq_Handle bh);

    CRef<CScope> BuildScope(void);
	bool x_IsReportableStructuredComment(const CSeqdesc& desc);

    bool x_ResolveSuppliedBioSampleAccession(vector<string>& biosample_ids);

    // for mode 3, biosample_push
    void UpdateBioSource (CBioseq_Handle bh, const CBioSource& src);
    void x_ClearCoordinatedSubSources(CBioSource& src);
    vector<CRef<CSeqdesc> > GetBiosampleDescriptors(string fname);
    vector<CRef<CSeqdesc> > GetBiosampleDescriptorsFromSeqSubmit();
    vector<CRef<CSeqdesc> > GetBiosampleDescriptorsFromSeqEntry();
    vector<CRef<CSeqdesc> > GetBiosampleDescriptorsFromSeqEntry(const CSeq_entry& se);

    CRef<CObjectManager> m_ObjMgr;
    auto_ptr<CObjectIStream> m_In;
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
        e_take_from_biosample,        // update with qualifiers from BioSample, stop if conflict
        e_take_from_biosample_force   // update with qualifiers from BioSample, no stop on conflict
    };

    enum E_ListType {
        e_none = 0,
        e_accessions,
        e_files
    };

    int m_Mode;
    int m_ListType;
	string m_StructuredCommentPrefix;
    bool m_CompareStructuredComments;
    bool m_UseDevServer;
    bool m_FirstSeqOnly;
    string m_IDPrefix;
    string m_HUPDate;
    string m_BioSampleAccession;
    string m_BioProjectAccession;

    size_t m_Processed;
    size_t m_Unprocessed;

    TSrcTableColumnList m_SrcReportFields;
	TStructuredCommentTableColumnList m_StructuredCommentReportFields;


    TBiosampleFieldDiffList m_Diffs;
    CRef<CSeq_table> m_Table;
    vector<CRef<CSeqdesc> > m_Descriptors;
};


CBiosampleChkApp::CBiosampleChkApp(void) :
    m_ObjMgr(0), m_In(0), m_Continue(false),
    m_Level(0), m_ReportStream(0), m_NeedReportHeader(true), m_AsnOut(0), 
    m_LogStream(0), m_Mode(e_report_diffs),
    m_StructuredCommentPrefix(""), m_CompareStructuredComments(true), 
    m_FirstSeqOnly(false), m_IDPrefix(""), m_HUPDate(""),
    m_BioSampleAccession(""), m_BioProjectAccession(""),
    m_Processed(0), m_Unprocessed(0)
{
    m_SrcReportFields.clear();
	m_StructuredCommentReportFields.clear();
}


void CBiosampleChkApp::Init(void)
{
    // Prepare command line descriptions

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

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

    arg_desc->AddFlag("b", "Input is in binary format");
    arg_desc->AddFlag("c", "Batch File is Compressed");
    arg_desc->AddFlag("M", "Process only first sequence in file (master)");
    arg_desc->AddOptionalKey("R", "BioSampleIDPrefix", "BioSample ID Prefix", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("HUP", "HUPDate", "Hold Until Publish Date", CArgDescriptions::eString);

    arg_desc->AddOptionalKey(
        "L", "OutFile", "Log File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey(
        "m", "mode", "Mode:\n\t1 create update file\n\t2 generate file for creating new biosample entries\n\t3 push source info from one file (-i) to others (-p)\n\t4 update with source qualifiers from BioSample unless conflict\n\t5 update with source qualifiers from BioSample (continue with conflict))",
        CArgDescriptions::eInteger, "1");
    CArgAllow* constraint = new CArgAllow_Integers(e_report_diffs, e_take_from_biosample_force);
    arg_desc->SetConstraint("m", constraint);
    
	arg_desc->AddOptionalKey(
		"P", "Prefix", "StructuredCommentPrefix", CArgDescriptions::eString);

    arg_desc->AddOptionalKey(
        "biosample", "BioSampleAccession", "BioSample Accession to use for sequences in record. Report error if sequences contain a reference to a different BioSample accession.", CArgDescriptions::eString);
    arg_desc->AddOptionalKey(
        "bioproject", "BioProjectAccession", "BioProject Accession to use for sequences in record. Report error if sequences contain a reference to a different BioProject accession.", CArgDescriptions::eString);


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

    if (header == "Seq-submit" ) {  // Seq-submit
        ProcessSeqSubmit();
    } else if ( header == "Seq-entry" ) {           // Seq-entry
        ProcessSeqEntry();
    } else if (header == "Bioseq-set" ) {  // Bioseq-set
        ProcessSet();
    } else {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
    }
}


void CBiosampleChkApp::ProcessList (const string& fname)
{
    // Process file with list of accessions

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*objmgr);
    CWGSDataLoader::RegisterInObjectManager(*objmgr);
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

    if (!m_ReportStream && (m_Mode == e_report_diffs || m_Mode == e_take_from_biosample)) {
        string path = fname;
        size_t pos = NStr::Find(path, ".", 0, string::npos, NStr::eLast);
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
    }
    if (!m_AsnOut && (m_Mode == e_push || m_Mode == e_take_from_biosample || m_Mode == e_take_from_biosample_force)) {
        string path = fname;
        size_t pos = NStr::Find(path, ".", 0, string::npos, NStr::eLast);
        if (pos != string::npos) {
            path = path.substr(0, pos);
        }
        path = path + ".out";
        ios::openmode mode = ios::out;
        m_AsnOut = new CNcbiOfstream(path.c_str(), mode);
        if (!m_AsnOut)
        {
            NCBI_THROW(CException, eUnknown, "Unable to open " + path);
        }
        *m_AsnOut << MSerial_AsnText;
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
            m_In.reset(OpenFile(fname, args));
            if (!m_In->InGoodState()) {
                NCBI_THROW(CException, eUnknown, "Unable to open " + fname);
            }
            ProcessAsnInput();
            break;        
    }

    if (m_Mode == e_report_diffs) {
        PrintResults(m_Diffs);
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
    if (ss->GetData().IsEntrys() && ss->GetData().GetEntrys().size() > 0) {
        descriptors = GetBiosampleDescriptorsFromSeqEntry(**(ss->GetData().GetEntrys().begin()));
    }
    return descriptors;
}


vector<CRef<CSeqdesc> > CBiosampleChkApp::GetBiosampleDescriptors(string fname)
{
    const CArgs& args = GetArgs();

    m_In.reset(OpenFile(fname, args));

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


void CBiosampleChkApp::ProcessOneDirectory(const string& dir_name, bool recurse)
{
    const CArgs& args = GetArgs();

    CDir dir(dir_name);

    string suffix = ".sqn";
    if (args["x"]) {
        suffix = args["x"].AsString();
    }
    string mask = "*" + suffix;

    CDir::TEntries files (dir.GetEntries(mask, CDir::eFile));
    ITERATE(CDir::TEntries, ii, files) {
        string fname = (*ii)->GetName();
        if ((*ii)->IsFile() &&
            (!args["f"] || NStr::Find (fname, args["f"].AsString()) != string::npos)) {
            string fname = CDirEntry::MakePath(dir_name, (*ii)->GetName());
            ProcessOneFile (fname);
        }
    }
    if (recurse) {
        CDir::TEntries subdirs (dir.GetEntries("", CDir::eDir));
        ITERATE(CDir::TEntries, ii, subdirs) {
            string subdir = (*ii)->GetName();
            if ((*ii)->IsDir() && !NStr::Equal(subdir, ".") && !NStr::Equal(subdir, "..")) {
                string subname = CDirEntry::MakePath(dir_name, (*ii)->GetName());
                ProcessOneDirectory (subname, recurse);
            }
        }
    }
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

    if (args["o"]) {
        if (m_Mode == e_report_diffs || m_Mode == e_generate_biosample || m_Mode == e_take_from_biosample) {
            m_ReportStream = &(args["o"].AsOutputFile());
            if (!m_ReportStream)
            {
                NCBI_THROW(CException, eUnknown, "Unable to open " + args["o"].AsString());
            }
            if (m_Mode == e_take_from_biosample) {
                m_Table.Reset(new CSeq_table());
                m_Table->SetNum_rows(0);
            }
        } else {
            ios::openmode mode = ios::out;
            m_AsnOut = new CNcbiOfstream(args["o"].AsString().c_str(), mode);
            if (!m_AsnOut)
            {
                NCBI_THROW(CException, eUnknown, "Unable to open " + args["o"].AsString());
            }
            *m_AsnOut << MSerial_AsnText;
        }
    }
            
    m_LogStream = args["L"] ? &(args["L"].AsOutputFile()) : &NcbiCout;
	m_StructuredCommentPrefix = args["P"] ? args["P"].AsString() : "";
	if (!NStr::IsBlank(m_StructuredCommentPrefix) && !NStr::StartsWith(m_StructuredCommentPrefix, "##")) {
		m_StructuredCommentPrefix = "##" + m_StructuredCommentPrefix;
	}

    m_UseDevServer = args["d"].AsBoolean();

    m_SrcReportFields.clear();
	m_StructuredCommentReportFields.clear();

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

    if ( m_Mode == e_push) {
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
            ProcessOneDirectory (args["p"].AsString(), args["u"]);
        }
    } else if ( args["p"] ) {
		ProcessOneDirectory (args["p"].AsString(), args["u"]);
        if (m_Mode == e_take_from_biosample) {
			if (m_Table->GetNum_rows() > 0) {
			    PrintTable(m_Table);
			}
		}
    } else {
        if (args["i"]) {
		    ProcessOneFile (args["i"].AsString());
            if (m_Mode == e_take_from_biosample) {
				if (m_Table->GetNum_rows() > 0) {
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
        return 0;
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


bool s_CompareBiosampleFieldDiffs (CRef<CBiosampleFieldDiff> f1, CRef<CBiosampleFieldDiff> f2)
{ 
    if (!f1) {
        return true;
    } else if (!f2) {
        return false;
    } 
    int cmp = f1->Compare(*f2);
    if (cmp < 0) {
        return true;
    } else {
        return false;
    }        
}


void CBiosampleChkApp::PrintTable(CRef<CSeq_table> table)
{
    if (table->GetNum_rows() == 0) {
        // do nothing
        return;
    }

	ITERATE(CSeq_table::TColumns, it, table->GetColumns()) {
		*m_ReportStream << (*it)->GetHeader().GetTitle() << "\t";
	}
	*m_ReportStream << endl;
	for (size_t row = 0; row < (size_t)table->GetNum_rows(); row++) {
		ITERATE(CSeq_table::TColumns, it, table->GetColumns()) {
			if (row < (*it)->GetData().GetString().size()) {
				*m_ReportStream << (*it)->GetData().GetString()[row] << "\t";
			} else {
				*m_ReportStream << "\t";
			}
		}
		*m_ReportStream << endl;
	}
}


void CBiosampleChkApp::PrintDiffs(TBiosampleFieldDiffList & diffs)
{
    if (diffs.size() == 0) {
        if (m_Processed == 0) {
            *m_ReportStream << "No results processed" << endl;
        } else {
            *m_ReportStream << "No differences found" << endl;
        }
    } else {
        if (m_NeedReportHeader) {
            CBiosampleFieldDiff::PrintHeader(*m_ReportStream, false);
            m_NeedReportHeader = false;
        }

        ITERATE(TBiosampleFieldDiffList, it, diffs) {
            (*it)->Print(*m_ReportStream, false);
        }
    }
    if (m_Unprocessed > 0) {
        *m_ReportStream << m_Unprocessed << " results failed" << endl;
    }
}


void CBiosampleChkApp::PrintResults(TBiosampleFieldDiffList & diffs)
{
    PrintDiffs(diffs);
}


static const string kStructuredCommentPrefix = "StructuredCommentPrefix";
static const string kStructuredCommentSuffix = "StructuredCommentSuffix";


static string s_GetStructuredCommentPrefix(const CUser_object& usr)
{
	if (!usr.IsSetType() || !usr.GetType().IsStr()
		|| !NStr::Equal(usr.GetType().GetStr(), "StructuredComment")){
		return "";
    }
    string prefix = "";
	try {
		const CUser_field& field = usr.GetField(kStructuredCommentPrefix);
		if (field.IsSetData() && field.GetData().IsStr()) {
			prefix = field.GetData().GetStr();		
		}
	} catch (...) {
        // no prefix
	}
    return prefix;
}


bool CBiosampleChkApp::x_IsReportableStructuredComment(const CSeqdesc& desc)
{
    if (!desc.IsUser()) {
        return false;
    }

	bool rval = false;

    const CUser_object& user = desc.GetUser();

	if (!user.IsSetType() || !user.GetType().IsStr()
		|| !NStr::Equal(user.GetType().GetStr(), "StructuredComment")){
		rval = false;
	} else {
        string prefix = s_GetStructuredCommentPrefix(user);
        if (NStr::IsBlank (m_StructuredCommentPrefix)) {
            if (!NStr::StartsWith(prefix, "##Genome-Assembly-Data", NStr::eNocase)
                && !NStr::StartsWith(prefix, "##Assembly-Data", NStr::eNocase)
                && !NStr::StartsWith(prefix, "##Genome-Annotation-Data", NStr::eNocase)) {
		        rval = true;
            }
        } else if (NStr::StartsWith(prefix, m_StructuredCommentPrefix)) {
			rval = true;
		}
	}
	return rval;
}


static bool s_IsCitSub (const CSeqdesc& desc)
{
    if (!desc.IsPub() || !desc.GetPub().IsSetPub()) {
        return false;
    }
    ITERATE(CPubdesc::TPub::Tdata, it, desc.GetPub().GetPub().Get()) {
        if ((*it)->IsSub()) {
            return true;
        }
    }

    return false;
}


string GetBestBioseqLabel(CBioseq_Handle bsh)
{
    string label = "";
 
    CConstRef<CSeq_id> id(NULL);
    vector<CRef <CSeq_id> > id_list;
    ITERATE(CBioseq_Handle::TId, it, bsh.GetId()) {
        CConstRef<CSeq_id> ir = (*it).GetSeqId();
        if (ir->IsGenbank()) {
            id = ir;
        }
        CRef<CSeq_id> ic(const_cast<CSeq_id *>(ir.GetPointer()));
        id_list.push_back(ic);
    }
    if (!id) {
        id = FindBestChoice(id_list, CSeq_id::BestRank);
    }
    if (id) {
        id->GetLabel(&label);
    }

    return label;
}


const string kSequenceID = "Sequence ID";
const string kAffilInst = "Institution";
const string kAffilDept = "Department";
const string kBioProject = "BioProject";

// This function is for generating a table of biosample values for a bioseq
// that does not currently have a biosample ID
void CBiosampleChkApp::AddBioseqToTable(CBioseq_Handle bh, bool with_id)
{
    vector<string> biosample_ids = GetBiosampleIDs(bh);
    if (biosample_ids.size() > 0 && !with_id) {
		// do not collect if already has biosample ID
        string label = GetBestBioseqLabel(bh);
        *m_LogStream << label << " already has Biosample ID " << biosample_ids[0] << endl;
		return;
	}
	vector<string> bioproject_ids = GetBioProjectIDs(bh);

    CSeqdesc_CI src_desc_ci(bh, CSeqdesc::e_Source);
	CSeqdesc_CI comm_desc_ci(bh, CSeqdesc::e_User);
	while (comm_desc_ci && !x_IsReportableStructuredComment(*comm_desc_ci)) {
		++comm_desc_ci;
	}

    CSeqdesc_CI pub_desc_ci(bh, CSeqdesc::e_Pub);
    while (pub_desc_ci && !s_IsCitSub(*pub_desc_ci)) {
        ++pub_desc_ci;
    }

    if (!src_desc_ci && !comm_desc_ci && bioproject_ids.size() == 0 && !pub_desc_ci) {
        return;
    }

    string sequence_id = GetBestBioseqLabel(bh);
	size_t row = m_Table->GetNum_rows();
	AddValueToTable (m_Table, kSequenceID, sequence_id, row);

	if (bioproject_ids.size() > 0) {
		string val = bioproject_ids[0];
		for (size_t i = 1; i < bioproject_ids.size(); i++) {
			val += ";";
			val += bioproject_ids[i];
		}
		AddValueToTable(m_Table, kBioProject, val, row);
	}

    if (pub_desc_ci) {
        ITERATE(CPubdesc::TPub::Tdata, it, pub_desc_ci->GetPub().GetPub().Get()) {
            if ((*it)->IsSub() && (*it)->GetSub().IsSetAuthors() && (*it)->GetSub().GetAuthors().IsSetAffil()) {
                const CAffil& affil = (*it)->GetSub().GetAuthors().GetAffil();
                if (affil.IsStd()) {
                    if (affil.GetStd().IsSetAffil()) {
                        AddValueToTable(m_Table, kAffilInst, affil.GetStd().GetAffil(), row);
                    }
                    if (affil.GetStd().IsSetDiv()) {
                        AddValueToTable(m_Table, kAffilDept, affil.GetStd().GetDiv(), row);
                    }
                } else if (affil.IsStr()) {
                    AddValueToTable(m_Table, kAffilInst, affil.GetStr(), row);
                }
                break;
            }
        }
    }

	if (src_desc_ci) {
		const CBioSource& src = src_desc_ci->GetSource();
	    TSrcTableColumnList src_fields = GetSourceFields(src);
		ITERATE(TSrcTableColumnList, it, src_fields) {
			AddValueToTable(m_Table, (*it)->GetLabel(), (*it)->GetFromBioSource(src), row);
		}
	}
    
    if (m_CompareStructuredComments) {
	    while (comm_desc_ci) {
		    const CUser_object& usr = comm_desc_ci->GetUser();
		    TStructuredCommentTableColumnList comm_fields = GetStructuredCommentFields(usr);
	        ITERATE(TStructuredCommentTableColumnList, it, comm_fields) {
			    string label = (*it)->GetLabel();
                AddValueToTable(m_Table, (*it)->GetLabel(), (*it)->GetFromComment(usr), row);
		    }
		    ++comm_desc_ci;
		    while (comm_desc_ci && !x_IsReportableStructuredComment(*comm_desc_ci)) {
			    ++comm_desc_ci;
		    }
	    }
    }
    if (with_id && biosample_ids.size() > 0) {
        AddValueToTable(m_Table, "BioSample ID", biosample_ids[0], row);
    }
	int num_rows = (int)row  + 1;
	m_Table->SetNum_rows(num_rows);
}


void AddContact(node::iterator& organization, CConstRef<CAuth_list> auth_list)
{
    string email = "";
    string street = "";
    string city = "";
    string sub = "";
    string country = "";
    string first = "";
    string last = "";
    bool add_address = false;

    CConstRef<CAffil> affil(NULL);
    if (auth_list && auth_list->IsSetAffil()) {
        affil = &(auth_list->GetAffil());
    }

    if (affil && affil->IsStd()) {
        const CAffil::TStd& std = affil->GetStd();
        string email = "";
        if (std.IsSetEmail()) {
            email = std.GetEmail();
        } 
        if (std.IsSetStreet() && !NStr::IsBlank(std.GetStreet())
            && std.IsSetCity() && !NStr::IsBlank(std.GetCity())
            && std.IsSetSub() && !NStr::IsBlank(std.GetSub())
            && std.IsSetCountry() && !NStr::IsBlank(std.GetCountry())) {
            street = std.GetStreet();
            city = std.GetCity();
            sub = std.GetSub();
            country = std.GetCountry();
            add_address = true;
        }
    }

    if (auth_list && auth_list->IsSetNames() && auth_list->GetNames().IsStd()
        && auth_list->GetNames().GetStd().size() 
        && auth_list->GetNames().GetStd().front()->IsSetName()
        && auth_list->GetNames().GetStd().front()->GetName().IsName()) {
        const CName_std& nstd = auth_list->GetNames().GetStd().front()->GetName().GetName();
        string first = "";
        string last = "";
        if (nstd.IsSetFirst()) {
            first = nstd.GetFirst();
        }
        if (nstd.IsSetLast()) {
            last = nstd.GetLast();
        }
    }

    if (NStr::IsBlank(email) || NStr::IsBlank(first) || NStr::IsBlank(last)) {
        // just don't add contact if no email address or name
        return;
    }
    node::iterator contact = organization->insert(node("Contact"));
    contact->get_attributes().insert("email", email.c_str());
    if (add_address) {
        node::iterator address = contact->insert(node("Address"));
        address->insert(node("Street", street.c_str()));
        address->insert(node("City", city.c_str()));
        address->insert(node("Sub", sub.c_str()));
        address->insert(node("Country", country.c_str()));
    }

    node::iterator name = contact->insert(node("Name"));
    name->insert(node("First", first.c_str()));
    name->insert(node("Last", last.c_str()));

}


void AddStructuredCommentToAttributes(node& sample_attrs, const CUser_object& usr)
{
	TStructuredCommentTableColumnList comm_fields = GetStructuredCommentFields(usr);
	ITERATE(TStructuredCommentTableColumnList, it, comm_fields) {
		string label = (*it)->GetLabel();
        if (NStr::EqualNocase(label, kStructuredCommentPrefix)
            || NStr::EqualNocase(label, kStructuredCommentSuffix)) {
            continue;
        }
        string val = (*it)->GetFromComment(usr);

        node::iterator a = sample_attrs.begin();
        bool found = false;
        while (a != sample_attrs.end() && !found) {
            if (NStr::Equal(a->get_name(), "Attribute")) {
                attributes::const_iterator at = a->get_attributes().begin();
                bool name_match = false;
                while (at != a->get_attributes().end() && !name_match) {
                    if (NStr::Equal(at->get_name(), "attribute_name")
                        && AttributeNamesAreEquivalent(at->get_value(), label)) {
                        name_match = true;                        
                    } 
                    ++at;
                }
                if (name_match) {
                    if (NStr::Equal(a->get_content(), val)) {
                        found = true;
                    }
                }
            }
            ++a;
        }
        if (!found) {
            sample_attrs.insert(node("Attribute", val.c_str()))
                ->get_attributes().insert("attribute_name", label.c_str());
        }
	}

}


// This function is for generating a table of biosample values for a bioseq
// that does not currently have a biosample ID
void CBiosampleChkApp::PrintBioseqXML(CBioseq_Handle bh)
{
    vector<string> biosample_ids = GetBiosampleIDs(bh);
    if (biosample_ids.size() > 0) {
		// do not collect if already has biosample ID
        string label = GetBestBioseqLabel(bh);
        *m_LogStream << label << " already has BioSample ID " << biosample_ids[0] << endl;
		return;
	}
	vector<string> bioproject_ids = GetBioProjectIDs(bh);
    if (bioproject_ids.size() > 0) {
        if (!NStr::IsBlank(m_BioProjectAccession)) {
            bool found = false;
            ITERATE(vector<string>, it, bioproject_ids) {
                if (NStr::EqualNocase(*it, m_BioProjectAccession)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                // error
                string label = GetBestBioseqLabel(bh);
                *m_LogStream << label << " has conflicting BioProject ID " << bioproject_ids[0] << endl;
		        return;
	        }
            bioproject_ids.clear();
            bioproject_ids.push_back(m_BioProjectAccession);
        }
    } else if (!NStr::IsBlank(m_BioProjectAccession)) {
        bioproject_ids.push_back(m_BioProjectAccession);
    }

    CSeqdesc_CI src_desc_ci(bh, CSeqdesc::e_Source);
	CSeqdesc_CI comm_desc_ci(bh, CSeqdesc::e_User);
	while (comm_desc_ci && !x_IsReportableStructuredComment(*comm_desc_ci)) {
		++comm_desc_ci;
	}

    CConstRef<CAuth_list> auth_list(NULL);
    CSeqdesc_CI pub_desc_ci(bh, CSeqdesc::e_Pub);
    while (pub_desc_ci){
        if (pub_desc_ci->GetPub().IsSetPub()) {
            ITERATE(CPubdesc::TPub::Tdata, it, pub_desc_ci->GetPub().GetPub().Get()) {
                if ((*it)->IsSub() && (*it)->GetSub().IsSetAuthors()) {
                    auth_list = &((*it)->GetSub().GetAuthors());
                    break;
                } else if ((*it)->IsGen() && (*it)->GetGen().IsSetAuthors()) {
                    auth_list = &((*it)->GetGen().GetAuthors());
                }
            }
        }
        ++pub_desc_ci;
    }

    if (!src_desc_ci && !comm_desc_ci && bioproject_ids.size() == 0) {
        string label = GetBestBioseqLabel(bh);
        *m_LogStream << label << " has no BioSample information" << endl;
        return;
    }

    string sequence_id = GetBestBioseqLabel(bh);

    CTime tNow(CTime::eCurrent);
    document doc("Submission");
    doc.set_encoding("UTF-8");
    doc.set_is_standalone(true);

    node & root = doc.get_root_node();

    node::iterator description = root.insert(node("Description"));
    string title = "Auto generated from GenBank Accession " + sequence_id;
    description->insert(node("Comment", title.c_str()));

    node::iterator node_iter = description->insert(node("Submitter"));

    CConstRef<CAffil> affil(NULL);
    if (auth_list && auth_list->IsSetAffil()) {
        affil = &(auth_list->GetAffil());
    }

    // Contact info
    node::iterator organization = description->insert(node("Organization"));
    {
        attributes & attrs = organization->get_attributes();
        attrs.insert("role", "owner");
        attrs.insert("type", "institute");
    }
    // same info for sample structure
    node owner("Owner");

    list<string> sbm_info;
    if (affil) {
        if (affil->IsStd()) {
            if (affil->GetStd().IsSetAffil()) {
                sbm_info.push_back(affil->GetStd().GetAffil());
            }
            if (affil->GetStd().IsSetDiv() 
                && (!affil->GetStd().IsSetAffil() 
                    || !NStr::EqualNocase(affil->GetStd().GetDiv(), affil->GetStd().GetAffil()))) {
                sbm_info.push_back(affil->GetStd().GetDiv());
            }
        } else if (affil->IsStr()) {
            sbm_info.push_back(affil->GetStr());
        }
    }

        
    organization->insert(node("Name", NStr::Join(sbm_info, ", ").c_str()));
    owner.insert(node("Name", NStr::Join(sbm_info, ", ").c_str()));

    AddContact(organization, auth_list);

    if (!NStr::IsBlank(m_HUPDate)) {
        node hup("Hold");
        hup.get_attributes().insert("release_date", m_HUPDate.c_str());
        description->insert(hup);
    }

    node_iter = root.insert(node("Action"))->insert(node("AddData"));
    node_iter->get_attributes().insert("target_db", "BioSample");

    // BioSample-specific xml
    node::iterator data = node_iter->insert(node("Data"));
    data->get_attributes().insert("content_type", "XML");

    node::iterator sample = data->insert(node("XmlContent"))->insert(node("BioSample"));
    sample->get_attributes().insert("schema_version", "2.0");    
    xml::ns ourns("xsi", "http://www.w3.org/2001/XMLSchema-instance");
    sample->add_namespace_definition(ourns, node::type_replace_if_exists);
    sample->get_attributes().insert("xsi:noNamespaceSchemaLocation", "http://www.ncbi.nlm.nih.gov/viewvc/v1/trunk/submit/public-docs/biosample/biosample.xsd?view=co");
    
    node::iterator ids = sample->insert(node("SampleId"));
    string sample_id = sequence_id;
    if (!NStr::IsBlank(m_IDPrefix)) {
        if (m_FirstSeqOnly) {
            sample_id = m_IDPrefix;
        } else {
            sample_id = m_IDPrefix + ":" + sequence_id;
        }
    }
    node_iter = ids->insert(node("SPUID", sample_id.c_str()));
    node_iter->get_attributes().insert( "spuid_namespace", "GenBank");

    node::iterator descr = sample->insert(node("Descriptor"));

    node::iterator organism = sample->insert(node("Organism"));

    // add unique bioproject links from series
    if (bioproject_ids.size() > 0) {
        node links("BioProject");
        ITERATE(vector<string>, it, bioproject_ids) {
            if (! it->empty()) {
                node_iter = links.insert(node("PrimaryId", it->c_str()));
                node_iter->get_attributes().insert("db", "BioProject");
            }
        }
        sample->insert(links);
    }

    sample->insert(node("Package", "Generic.1.0"));

    string tax_id = "";
    string taxname = "";
    node sample_attrs("Attributes");
	if (src_desc_ci) {
		const CBioSource& src = src_desc_ci->GetSource();
	    TSrcTableColumnList src_fields = GetSourceFields(src);
		ITERATE(TSrcTableColumnList, it, src_fields) {
            if (NStr::EqualNocase((*it)->GetLabel(), kTaxId)) {
                tax_id = (*it)->GetFromBioSource(src);
            } else if (NStr::EqualNocase((*it)->GetLabel(), kOrganismName)) {
                taxname = (*it)->GetFromBioSource(src);
            } else {
                string attribute_name = (*it)->GetLabel();
                string val = (*it)->GetFromBioSource(src);
                sample_attrs.insert(node("Attribute", val.c_str()))
                    ->get_attributes().insert("attribute_name", attribute_name.c_str());
            }
		}
    }

    organism->insert(node("OrganismName", taxname.c_str()));
    
    if (m_CompareStructuredComments) {
	    while (comm_desc_ci) {
		    const CUser_object& usr = comm_desc_ci->GetUser();
            AddStructuredCommentToAttributes(sample_attrs, usr);
		    ++comm_desc_ci;
		    while (comm_desc_ci && !x_IsReportableStructuredComment(*comm_desc_ci)) {
			    ++comm_desc_ci;
		    }
	    }
    }
    sample->insert(sample_attrs);

    // write XML to file
    if (m_ReportStream) {
        *m_ReportStream << doc << endl;
    } else {
        string path = sequence_id;
        NStr::ReplaceInPlace(path, "|", "_");
        NStr::ReplaceInPlace(path, ".", "_");
        NStr::ReplaceInPlace(path, ":", "_");

        path = path + ".xml";
        CNcbiOstream* xml_out = new CNcbiOfstream(path.c_str());
        if (!xml_out)
        {
            NCBI_THROW(CException, eUnknown, "Unable to open " + path);
        }
        *xml_out << doc << endl;
    }
}


bool CBiosampleChkApp::x_ResolveSuppliedBioSampleAccession(vector<string>& biosample_ids)
{
    if (!NStr::IsBlank(m_BioSampleAccession)) {
        if (biosample_ids.size() == 0) {
            // use supplied BioSample accession
            biosample_ids.push_back(m_BioSampleAccession);
        } else {
            // make sure supplied BioSample accession is listed
            bool found = false;
            ITERATE(vector<string>, it, biosample_ids) {
                if (NStr::EqualNocase(*it, m_BioSampleAccession)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
	        }
            biosample_ids.clear();
            biosample_ids.push_back(m_BioSampleAccession);
        }
    }
    return true;
}


void CBiosampleChkApp::GetBioseqDiffs(CBioseq_Handle bh)
{
    CSeqdesc_CI src_desc_ci(bh, CSeqdesc::e_Source);
	CSeqdesc_CI comm_desc_ci(bh, CSeqdesc::e_User);
    vector<string> user_labels;
    vector<CConstRef<CUser_object> > user_objs;    
	while (comm_desc_ci) {
        if (x_IsReportableStructuredComment(*comm_desc_ci)) {
            const CUser_object& user = comm_desc_ci->GetUser();
            string prefix = s_GetStructuredCommentPrefix(user);
            CConstRef<CUser_object> obj(&user);
            user_labels.push_back(prefix);
            user_objs.push_back(obj);
        }
		++comm_desc_ci;
	}
    if (!src_desc_ci && user_labels.size() == 0) {
        return;
    }

    vector<string> biosample_ids = GetBiosampleIDs(bh);

    if (!x_ResolveSuppliedBioSampleAccession(biosample_ids)) {
        // error
        string label = GetBestBioseqLabel(bh);
        *m_LogStream << label << " has conflicting BioSample Accession " << biosample_ids[0] << endl;
        return;
    }

    if (biosample_ids.size() == 0) {
        // for report mode, do not report if no biosample ID
        return;
    }

    string sequence_id = GetBestBioseqLabel(bh);

    ITERATE(vector<string>, id, biosample_ids) {
        CRef<CSeq_descr> descr = GetBiosampleData(*id, m_UseDevServer);
        if (descr) {
            ITERATE(CSeq_descr::Tdata, it, descr->Get()) {
                if ((*it)->IsSource()) {
					if (src_desc_ci) {
                        TBiosampleFieldDiffList these_diffs = GetFieldDiffs(sequence_id, *id, src_desc_ci->GetSource(), (*it)->GetSource());
						m_Diffs.insert(m_Diffs.end(), these_diffs.begin(), these_diffs.end());
					}
                } else if ((*it)->IsUser() && x_IsReportableStructuredComment(**it)) {
                    if (m_CompareStructuredComments) {
                        CConstRef<CUser_object> sample(&(*it)->GetUser());
                        string this_prefix = s_GetStructuredCommentPrefix((*it)->GetUser());
                        bool found = false;
                        vector<string>::iterator sit = user_labels.begin();
                        vector<CConstRef<CUser_object> >::iterator uit = user_objs.begin();
                        while (sit != user_labels.end() && uit != user_objs.end()) {
                            if (NStr::EqualNocase(*sit, this_prefix)) {
                                TBiosampleFieldDiffList these_diffs = GetFieldDiffs(sequence_id, *id, *uit, sample);
                                m_Diffs.insert(m_Diffs.end(), these_diffs.begin(), these_diffs.end());
                                found = true;
                            }
                            ++sit;
                            ++uit;
                        }
				        if (!found) {
                            TBiosampleFieldDiffList these_diffs = GetFieldDiffs(sequence_id, *id, CConstRef<CUser_object>(NULL), sample);
                            m_Diffs.insert(m_Diffs.end(), these_diffs.begin(), these_diffs.end());                           
                        }
                    }
				}
            }
            m_Processed++;
        } else {
            m_Unprocessed++;
            *m_LogStream << "Failed to retrieve BioSample data for  " << *id << endl;
        }
    }
}


bool CBiosampleChkApp::DoDiffsContainConflicts()
{
    if (m_Diffs.empty()) {
        return false;;
    }

    bool rval = false;
    bool printed_header = false;

    ITERATE (TBiosampleFieldDiffList, it, m_Diffs) {
        if (!NStr::IsBlank((*it)->GetSrcVal())) {
            if (!printed_header) {
                *m_LogStream << "Conflict found for " << (*it)->GetSequenceId() << " for " << (*it)->GetBioSample() << endl;
                printed_header = true;
            }
            *m_LogStream << "\t" << (*it)->GetFieldName() << ": BioSource contains \"" << (*it)->GetSrcVal() << "\", BioSample contains \"" << (*it)->GetSampleVal() << "\"" << endl;
            rval = true;
        }
    }
    return rval;
}


void CBiosampleChkApp::PushToRecord(CBioseq_Handle bh)
{
    ITERATE(vector<CRef<CSeqdesc> >, it, m_Descriptors) {
        if ((*it)->IsSource()) {
            UpdateBioSource(bh, (*it)->GetSource());
        }
    }
}


void CBiosampleChkApp::ProcessBioseqForUpdate(CBioseq_Handle bh)
{
    vector<string> biosample_ids = GetBiosampleIDs(bh);

    if (!x_ResolveSuppliedBioSampleAccession(biosample_ids)) {
        // error
        string label = GetBestBioseqLabel(bh);
        *m_LogStream << label << " has conflicting BioSample Accession " << biosample_ids[0] << endl;
        return;
    }

    if (biosample_ids.size() == 0) {
        // for report mode, do not report if no biosample ID
        return;
    }

    ITERATE(vector<string>, id, biosample_ids) {
        CRef<CSeq_descr> descr = GetBiosampleData(*id, m_UseDevServer);
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
            PrintBioseqXML(bh);
            break;
        case e_push:
            PushToRecord(bh);
            break;
        case e_take_from_biosample:
            m_Diffs.clear();
            GetBioseqDiffs(bh);
            if (DoDiffsContainConflicts()) {
                string sequence_id = GetBestBioseqLabel(bh);
                *m_LogStream << "Conflicts found for  " << sequence_id << endl;
                AddBioseqToTable(bh, true);
            } else {
                ProcessBioseqForUpdate(bh);
            }
            break;
        case e_take_from_biosample_force:
            ProcessBioseqForUpdate(bh);
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
        ITERATE(CBioseq_set::TSeq_set, se, set->GetSeq_set()) {
            ProcessSeqEntry(*se);
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

    // Process Seq-submit
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        ITERATE(CSeq_submit::TData::TEntrys, se, ss->GetData().GetEntrys()) {
            ProcessSeqEntry(*se);
        }
    }
    // write out copy after processing, if requested
    if (m_AsnOut) {
        *m_AsnOut << *ss;
    }

}


static bool s_MustCopy (int subtype)
{
    if (CSubSource::IsDiscouraged(subtype)) {
        return false;
    } else if (subtype == CSubSource::eSubtype_chromosome
               || subtype == CSubSource::eSubtype_map
               || subtype == CSubSource::eSubtype_plasmid_name
               || subtype == CSubSource::eSubtype_other) {
        return false;
    } else {
        return true;
    }
}


void CBiosampleChkApp::x_ClearCoordinatedSubSources(CBioSource& src)
{
    if (!src.IsSetSubtype()) {
        return;
    }
    CBioSource::TSubtype::iterator it = src.SetSubtype().begin();
    while (it != src.SetSubtype().end()) {
        if (s_MustCopy((*it)->GetSubtype())) {
            it = src.SetSubtype().erase(it);
        } else {
            ++it;
        }
    }
}


void CBiosampleChkApp::UpdateBioSource (CBioseq_Handle bh, const CBioSource& src)
{
    CSeqdesc_CI src_desc_ci(bh, CSeqdesc::e_Source);

    if (!src_desc_ci) {
        CRef<CSeqdesc> new_desc(new CSeqdesc());
        new_desc->SetSource().Assign(src);
        CBioseq_set_Handle parent = bh.GetParentBioseq_set();
        if (parent && parent.IsSetClass() && parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
            CBioseq_set_EditHandle beh = parent.GetEditHandle();
            beh.AddSeqdesc(*new_desc);
        } else {
            CBioseq_EditHandle beh = bh.GetEditHandle();
            beh.AddSeqdesc(*new_desc);
        }
    } else {
        const CBioSource& bs = src_desc_ci->GetSource();
        CBioSource* old_src = const_cast<CBioSource *> (&bs);

        if (src.IsSetOrg()) {
            old_src->SetOrg().Assign(src.GetOrg());
        } else {
            old_src->ResetOrg();
        }
        if (src.IsSetPcr_primers()) {
            old_src->SetPcr_primers().Assign(src.GetPcr_primers());
        } else {
            old_src->ResetPcr_primers();
        }       
        if (src.IsSetOrigin()) {
            old_src->SetOrigin(src.GetOrigin());
        } else {
            old_src->ResetOrigin();
        }

        // optionals
        if (src.IsSetGenome()) {
            old_src->SetGenome(src.GetGenome());
        }

        // only values that must stay the same are removed from the existing source
        x_ClearCoordinatedSubSources(*old_src);

        if (src.IsSetSubtype()) {
            ITERATE(CBioSource::TSubtype, it, src.GetSubtype()) {
                if (s_MustCopy((*it)->GetSubtype())) {
                    CRef<CSubSource> s(new CSubSource());
                    s->Assign(**it);
                    old_src->SetSubtype().push_back(s);
                } else {
                    // if the master has a value, the contig's value must be updated,
                    // but if the master had no value, the contig's value would have
                    // been allowed to remain
                    bool found = false;
                    NON_CONST_ITERATE (CBioSource::TSubtype, sit, old_src->SetSubtype()) {
                        if ((*it)->GetSubtype() == (*sit)->GetSubtype()) {
                            found = true;
                            (*sit)->SetName((*it)->GetName());
                            break;
                        }
                    }
                    if (!found) {
                        CRef<CSubSource> s(new CSubSource());
                        s->Assign(**it);
                        old_src->SetSubtype().push_back(s);                        
                    }
                }
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


CObjectIStream* CBiosampleChkApp::OpenFile
(const CArgs& args)
{
    // file name
    string fname = args["i"].AsString();

    // file format 
    ESerialDataFormat format = eSerial_AsnText;
    if ( args["b"] ) {
        format = eSerial_AsnBinary;
    }

    return CObjectIStream::Open(fname, format);
}


CObjectIStream* CBiosampleChkApp::OpenFile
(string fname, const CArgs& args)
{
    // file format 
    ESerialDataFormat format = eSerial_AsnText;
    if ( args["b"] ) {
        format = eSerial_AsnBinary;
    }

    return CObjectIStream::Open(fname, format);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CBiosampleChkApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
