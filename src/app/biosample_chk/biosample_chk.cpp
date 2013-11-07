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
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Affil.hpp>
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

#include "util.hpp"
#include "src_table_column.hpp"
#include "struc_table_column.hpp"

#include <common/test_assert.h>  /* This header must go last */


using namespace ncbi;
using namespace objects;

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
    void AddBioseqToTable(CBioseq_Handle bh);
    void PushToRecord(CBioseq_Handle bh);

    void ProcessBioseqHandle(CBioseq_Handle bh);
    void ProcessSeqEntry(CRef<CSeq_entry> se);
    void ProcessSeqEntry(void);
    void ProcessSeqSubmit(void);
    void ProcessInput (void);
    void ProcessList (const string& fname);
    void ProcessFileList (const string& fname);
    void ProcessOneDirectory(const string& dir_name, bool recurse);
    void ProcessOneFile(string fname);
    void ProcessReleaseFile(const CArgs& args);
    CRef<CSeq_entry> ReadSeqEntry(void);

    void PrintResults(TBiosampleFieldDiffList& diffs);
    void PrintDiffs(TBiosampleFieldDiffList& diffs);
    void PrintTable(CRef<CSeq_table> table);
    CRef<CScope> BuildScope(void);
	bool x_IsReportableStructuredComment(const CSeqdesc& desc);

    // for mode 3, biosample_push
    void UpdateBioSource (CBioseq_Handle bh, const CBioSource& src);
    void x_ClearCoordinatedSubSources(CBioSource& src);
    CRef<CSeq_entry> ProcessSeqEntry(const string& fname);
    CRef<CSeq_submit> ProcessSeqSubmit(const string& fname);
    void ProcessInput (const string& fname);
    vector<CRef<CSeqdesc> > GetBiosampleDescriptors(string fname);
    vector<CRef<CSeqdesc> > GetBiosampleDescriptorsFromSeqSubmit();
    vector<CRef<CSeqdesc> > GetBiosampleDescriptorsFromSeqEntry();
    vector<CRef<CSeqdesc> > GetBiosampleDescriptorsFromSeqEntry(const CSeq_entry& se);

    CRef<CObjectManager> m_ObjMgr;
    auto_ptr<CObjectIStream> m_In;
    bool m_Continue;

    size_t m_Level;

    CNcbiOstream* m_ReportStream;
    CNcbiOstream* m_LogStream;

    enum E_Mode {
        e_report_diffs = 1,     // Default - report diffs between biosources on records with biosample accessions
                                // and biosample data
        e_generate_biosample,
        e_push
    };

    enum E_ListType {
        e_none = 0,
        e_accessions,
        e_files
    };

    int m_Mode;
    int m_ListType;
	string m_StructuredCommentPrefix;

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
    m_Level(0), m_ReportStream(0), m_LogStream(0), m_Mode(e_report_diffs),
    m_StructuredCommentPrefix(""), m_Processed(0), m_Unprocessed(0)
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

    arg_desc->AddDefaultKey("a", "a", 
                            "ASN.1 Type (a Automatic, z Any, e Seq-entry, b Bioseq, s Bioseq-set, m Seq-submit, t Batch Bioseq-set, u Batch Seq-submit) or accession list (l)",
                            CArgDescriptions::eString,
                            "a");

    arg_desc->AddFlag("b", "Input is in binary format");
    arg_desc->AddFlag("c", "Batch File is Compressed");

    arg_desc->AddOptionalKey(
        "L", "OutFile", "Log File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey(
        "m", "mode", "Mode (1 report diffs, 2 generate biosample table, 3 push source info from one file (-i) to others (-p))",
        CArgDescriptions::eInteger, "1");
    CArgAllow* constraint = new CArgAllow_Integers(e_report_diffs, e_push);
    arg_desc->SetConstraint("m", constraint);

	arg_desc->AddOptionalKey(
		"P", "Prefix", "StructuredCommentPrefix", CArgDescriptions::eString);


    // Program description
    string prog_description = "BioSample Checker\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());

}


void CBiosampleChkApp::ProcessInput (void)
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


void CBiosampleChkApp::ProcessInput (const string& fname)
{
    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.

	string header = m_In->ReadFileHeader();

    if (header == "Seq-submit" ) {  // Seq-submit
        ProcessSeqSubmit(fname + ".out");
    } else if ( header == "Seq-entry" ) {           // Seq-entry
        ProcessSeqEntry(fname + ".out");
    } else {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
    }
}


void CBiosampleChkApp::ProcessOneFile(string fname)
{
    const CArgs& args = GetArgs();

    bool need_to_close = false;

    if (!m_ReportStream) {
        string path = fname;
        size_t pos = NStr::Find(path, ".", 0, string::npos, NStr::eLast);
        if (pos != string::npos) {
            path = path.substr(0, pos);
        }
        path = path + ".val";

        m_ReportStream = new CNcbiOfstream(path.c_str());
        need_to_close = true;
        m_Table.Reset(new CSeq_table());
		m_Table->SetNum_rows(0);
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
            if (m_Mode == e_push) {
                ProcessInput(fname);
            } else {
                ProcessInput ();
            }
            break;        
    }

    if (m_Mode == e_report_diffs) {
        PrintResults(m_Diffs);
    }

    // TODO! Must free diffs
    m_Diffs.clear();
    
    if (need_to_close) {
        if (m_Mode == e_generate_biosample) {
            PrintTable(m_Table);
            m_Table->Reset();
            m_Table = new CSeq_table();
            m_Table->SetNum_rows(0);
        }
        m_ReportStream = 0;
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

    if (args["o"]) {
        m_ReportStream = &(args["o"].AsOutputFile());
    }
            
    m_LogStream = args["L"] ? &(args["L"].AsOutputFile()) : &NcbiCout;
	m_StructuredCommentPrefix = args["P"] ? args["P"].AsString() : "";
	if (!NStr::IsBlank(m_StructuredCommentPrefix) && !NStr::StartsWith(m_StructuredCommentPrefix, "##")) {
		m_StructuredCommentPrefix = "##" + m_StructuredCommentPrefix;
	}

    m_Mode = args["m"].AsInteger();
    m_SrcReportFields.clear();
	m_StructuredCommentReportFields.clear();

    if (!NStr::IsBlank(m_StructuredCommentPrefix) && m_Mode != e_generate_biosample) {
        // error
        *m_LogStream << "Structured comment prefix is only appropriate for generating a biosample table." << endl;
        return 0;
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
            return 0;
        } else if (!args["p"] || !args["i"]) {
            // error
            *m_LogStream << "Both directory containing contigs (-p) and master file (-i) are required for push mode." << endl;
            return 0;
        } else {
            m_Descriptors = GetBiosampleDescriptors(args["i"].AsString());
            ProcessOneDirectory (args["p"].AsString(), args["u"]);
        }
    } else if ( args["p"] ) {
 	    if (m_Mode == e_generate_biosample) {
			m_Table = new CSeq_table();
			m_Table->SetNum_rows(0);
        }
		ProcessOneDirectory (args["p"].AsString(), args["u"]);
        if (m_Mode == e_generate_biosample) {
			if (m_Table->GetNum_rows() > 0) {
			    PrintTable(m_Table);
			}
		}
    } else {
        if (args["i"]) {
			if (m_Mode == e_generate_biosample) {
				m_Table = new CSeq_table();
				m_Table->SetNum_rows(0);
            }
		    ProcessOneFile (args["i"].AsString());
            if (m_Mode == e_generate_biosample) {
				if (m_Table->GetNum_rows() > 0) {
					PrintTable(m_Table);
				}
			}
        }
    }

    return 1;
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
            } catch (exception e) {
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
	for (size_t row = 0; row < table->GetNum_rows(); row++) {
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
    } else if (diffs.size() == 1) {
        diffs.front()->Print(*m_ReportStream);
    } else {
        sort(diffs.begin(), diffs.end(), s_CompareBiosampleFieldDiffs);

        TBiosampleFieldDiffList::iterator f_prev = diffs.begin();
        TBiosampleFieldDiffList::iterator f_next = f_prev;
        f_next++;
        while (f_next != diffs.end()) {
            if ((*f_prev)->CompareAllButSequenceID(**f_next) == 0) {
                string old_id = (*f_prev)->GetSequenceId();
                string new_id = (*f_next)->GetSequenceId();
                old_id += "," + new_id;
                (*f_prev)->SetSequenceId(old_id);
                 f_next = diffs.erase(f_next);
            } else {
                ++f_prev;
                ++f_next;
            }
        }
    
        f_prev = diffs.begin();
        f_next = f_prev;
        f_next++;
        (*f_prev)->Print(*m_ReportStream);
        while (f_next != diffs.end()) {
            (*f_next)->Print(*m_ReportStream, **f_prev);
            f_next++;
            f_prev++;
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
static const string kStructuredCommentSuffix = "StructuredCommentSufix";


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
                && !NStr::StartsWith(prefix, "##Assembly-Data", NStr::eNocase)) {
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


const string kSequenceID = "Sequence ID";
const string kAffilInst = "Institution";
const string kAffilDept = "Department";

// This function is for generating a table of biosample values for a bioseq
// that does not currently have a biosample ID
void CBiosampleChkApp::AddBioseqToTable(CBioseq_Handle bh)
{
    vector<string> biosample_ids = GetBiosampleIDs(bh);
    if (biosample_ids.size() > 0) {
		// do not collect if already has biosample ID
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

    string sequence_id;
    bh.GetId().front().GetSeqId()->GetLabel(&sequence_id);
	size_t row = m_Table->GetNum_rows();
	AddValueToTable (m_Table, kSequenceID, sequence_id, row);

	if (bioproject_ids.size() > 0) {
		string val = bioproject_ids[0];
		for (size_t i = 1; i < bioproject_ids.size(); i++) {
			val += ";";
			val += bioproject_ids[i];
		}
		AddValueToTable(m_Table, "BioProject", val, row);
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
	int num_rows = (int)row  + 1;
	m_Table->SetNum_rows(num_rows);
}


void CBiosampleChkApp::GetBioseqDiffs(CBioseq_Handle bh)
{
    CSeqdesc_CI src_desc_ci(bh, CSeqdesc::e_Source);
	CSeqdesc_CI comm_desc_ci(bh, CSeqdesc::e_User);
	while (comm_desc_ci && !NStr::Equal(comm_desc_ci->GetUser().GetType().GetStr(), "StructuredComment")) {
		++comm_desc_ci;
	}
    if (!src_desc_ci && !comm_desc_ci) {
        return;
    }

    vector<string> biosample_ids = GetBiosampleIDs(bh);

    if (biosample_ids.size() == 0) {
        // for report mode, do not report if no biosample ID
        return;
    }

    string sequence_id;
    bh.GetId().front().GetSeqId()->GetLabel(&sequence_id);

    ITERATE(vector<string>, id, biosample_ids) {
        CRef<CSeq_descr> descr = GetBiosampleData(*id);
        if (descr) {
            ITERATE(CSeq_descr::Tdata, it, descr->Get()) {
                if ((*it)->IsSource()) {
					if (src_desc_ci) {
                        TBiosampleFieldDiffList these_diffs = GetFieldDiffs(sequence_id, *id, src_desc_ci->GetSource(), (*it)->GetSource());
						m_Diffs.insert(m_Diffs.end(), these_diffs.begin(), these_diffs.end());
					}
                } else if ((*it)->IsUser()) {
					if (comm_desc_ci) {
                        TBiosampleFieldDiffList these_diffs = GetFieldDiffs(sequence_id, *id, comm_desc_ci->GetUser(), (*it)->GetUser());
                        m_Diffs.insert(m_Diffs.end(), these_diffs.begin(), these_diffs.end());
					}
				}
            }
            m_Processed++;
        } else {
            m_Unprocessed++;
        }
    }
}


void CBiosampleChkApp::PushToRecord(CBioseq_Handle bh)
{
    ITERATE(vector<CRef<CSeqdesc> >, it, m_Descriptors) {
        if ((*it)->IsSource()) {
            UpdateBioSource(bh, (*it)->GetSource());
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
            AddBioseqToTable(bh);
            break;
        case e_push:
            PushToRecord(bh);
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
        ++bi;
    }
    scope->RemoveTopLevelSeqEntry(seh);
}


void CBiosampleChkApp::ProcessSeqEntry(void)
{
    // Get seq-entry to process
    CRef<CSeq_entry> se(ReadSeqEntry());

    ProcessSeqEntry(se);
}


CRef<CSeq_entry> CBiosampleChkApp::ProcessSeqEntry(const string& fname)
{
    // Get seq-entry to process
    CRef<CSeq_entry> se(ReadSeqEntry());

    ProcessSeqEntry(se);

    ios::openmode mode = ios::out;
    CNcbiOfstream os(fname.c_str(), mode);
    if (!os)
    {
        NCBI_THROW(CException, eUnknown, "Unable to open " + fname);
    }
    os << MSerial_AsnText;
    os << *se;

    return se;
}


void CBiosampleChkApp::ProcessSeqSubmit(void)
{
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to process
    m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);

    // Validae Seq-submit
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        ITERATE(CSeq_submit::TData::TEntrys, se, ss->GetData().GetEntrys()) {
            ProcessSeqEntry(*se);
        }
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


CRef<CSeq_submit> CBiosampleChkApp::ProcessSeqSubmit(const string& fname)
{
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to process
    m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);

    // Update Seq-submit
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        ITERATE(CSeq_submit::TData::TEntrys, se, ss->GetData().GetEntrys()) {
            ProcessSeqEntry(*se);
        }
    }
    ios::openmode mode = ios::out;
    CNcbiOfstream os(fname.c_str(), mode);
    if (!os)
    {
        NCBI_THROW(CException, eUnknown, "Unable to open " + fname);
    }
    os << MSerial_AsnText;
    os << *ss;

    return ss;
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
