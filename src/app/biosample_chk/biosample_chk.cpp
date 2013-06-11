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
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>
#include <objects/seqtable/SeqTable_column_info.hpp>

#include <objects/seqset/Bioseq_set.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
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

	void ProcessBioseqHandle(CBioseq_Handle bh, CRef<CSeq_table> table);
	void ProcessSeqEntry(const CSeq_entry& se, CRef<CSeq_table> table);
    void ProcessSeqEntry(CRef<CSeq_table> table);
    void ProcessSeqSubmit(CRef<CSeq_table> table);
    void ProcessInput (CRef<CSeq_table> table);
	void ProcessOneFile(string fname, CRef<CSeq_table> table);
    void ProcessOneDirectory(string dir_name, bool recurse, CRef<CSeq_table> table);

    TBiosampleFieldDiffList ProcessBioseqHandle(CBioseq_Handle bh);
    TBiosampleFieldDiffList ProcessSeqEntry(const CSeq_entry& se);
    TBiosampleFieldDiffList ProcessSeqEntry(void);
    TBiosampleFieldDiffList ProcessSeqSubmit(void);
    TBiosampleFieldDiffList ProcessInput (void);
    void ProcessOneDirectory(string dir_name, bool recurse);
    void ProcessOneFile(string fname);
    void ProcessReleaseFile(const CArgs& args);
    CRef<CSeq_entry> ReadSeqEntry(void);

    void PrintResults(TBiosampleFieldDiffList& diffs);
    void PrintDiffs(TBiosampleFieldDiffList& diffs);
    void PrintTable(CRef<CSeq_table> table);
    CRef<CScope> BuildScope(void);


    CRef<CObjectManager> m_ObjMgr;
    auto_ptr<CObjectIStream> m_In;
    bool m_Continue;

    size_t m_Level;

    CNcbiOstream* m_ReportStream;
    CNcbiOstream* m_LogStream;

    enum E_Mode {
        e_report_diffs = 1,     // Default - report diffs between biosources on records with biosample accessions
                                // and biosample data
        e_generate_biosample
    };

    int m_Mode;
    TSrcTableColumnList m_SrcReportFields;
	TStructuredCommentTableColumnList m_StructuredCommentReportFields;
};


CBiosampleChkApp::CBiosampleChkApp(void) :
    m_ObjMgr(0), m_In(0), m_Continue(false),
    m_Level(0), m_ReportStream(0), m_LogStream(0), m_Mode(e_report_diffs)
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
                            "ASN.1 Type (a Automatic, z Any, e Seq-entry, b Bioseq, s Bioseq-set, m Seq-submit, t Batch Bioseq-set, u Batch Seq-submit",
                            CArgDescriptions::eString,
                            "a");

    arg_desc->AddFlag("b", "Input is in binary format");
    arg_desc->AddFlag("c", "Batch File is Compressed");

    arg_desc->AddOptionalKey(
        "L", "OutFile", "Log File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey(
        "m", "mode", "Mode (1 report diffs, 2 generate biosample table)",
        CArgDescriptions::eInteger, "1");
    CArgAllow* constraint = new CArgAllow_Integers(e_report_diffs, e_generate_biosample);
    arg_desc->SetConstraint("m", constraint);


    // Program description
    string prog_description = "BioSample Checker\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());

}


TBiosampleFieldDiffList CBiosampleChkApp::ProcessInput (void)
{
    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    TBiosampleFieldDiffList diffs;
    string header = m_In->ReadFileHeader();

    if (header == "Seq-submit" ) {  // Seq-submit
        diffs = ProcessSeqSubmit();
    } else if ( header == "Seq-entry" ) {           // Seq-entry
        diffs = ProcessSeqEntry();
    } else {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
    }
    return diffs;
}


void CBiosampleChkApp::ProcessInput (CRef<CSeq_table> table)
{
    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.

	string header = m_In->ReadFileHeader();

    if (header == "Seq-submit" ) {  // Seq-submit
        ProcessSeqSubmit(table);
    } else if ( header == "Seq-entry" ) {           // Seq-entry
        ProcessSeqEntry(table);
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
    }
    m_In.reset(OpenFile(fname, args));

    TBiosampleFieldDiffList diffs = ProcessInput ();

    PrintResults(diffs);

    // TODO! Must free diffs
    
    if (need_to_close) {
        m_ReportStream = 0;
    }
}


void CBiosampleChkApp::ProcessOneFile(string fname, CRef<CSeq_table> table)
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
		table.Reset(new CSeq_table());
		table->SetNum_rows(0);
    }
    m_In.reset(OpenFile(fname, args));

    ProcessInput (table);

    // TODO! Must free diffs
    
    if (need_to_close) {
		PrintTable(table);
        m_ReportStream = 0;
    }
}


void CBiosampleChkApp::ProcessOneDirectory(string dir_name, bool recurse)
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


void CBiosampleChkApp::ProcessOneDirectory(string dir_name, bool recurse, CRef<CSeq_table> table)
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
            ProcessOneFile (fname, table);
        }
    }
    if (recurse) {
        CDir::TEntries subdirs (dir.GetEntries("", CDir::eDir));
        ITERATE(CDir::TEntries, ii, subdirs) {
            string subdir = (*ii)->GetName();
            if ((*ii)->IsDir() && !NStr::Equal(subdir, ".") && !NStr::Equal(subdir, "..")) {
                string subname = CDirEntry::MakePath(dir_name, (*ii)->GetName());
                ProcessOneDirectory (subname, recurse, table);
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

    m_Mode = args["m"].AsInteger();
    m_SrcReportFields.clear();
	m_StructuredCommentReportFields.clear();

    // Process file based on its content
    // Unless otherwise specified we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.

    if ( args["p"] ) {
 	    if (m_Mode == e_generate_biosample) {
			CRef<CSeq_table> table(new CSeq_table());
			table->SetNum_rows(0);
			ProcessOneDirectory (args["p"].AsString(), args["u"], table);
			if (table->GetNum_rows() > 0) {
			    PrintTable(table);
			}
		} else {
            ProcessOneDirectory (args["p"].AsString(), args["u"]);
		}
    } else {
        if (args["i"]) {
			if (m_Mode == e_generate_biosample) {
				CRef<CSeq_table> table(new CSeq_table());
				table->SetNum_rows(0);
				ProcessOneFile (args["i"].AsString(), table);
				if (table->GetNum_rows() > 0) {
					PrintTable(table);
				}
			} else {
                ProcessOneFile (args["i"].AsString());
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
                
                TBiosampleFieldDiffList diffs = ProcessSeqEntry(*se);
                PrintResults(diffs);
                // TODO! Must free diffs

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


bool s_CompareBiosampleFieldDiffs (CBiosampleFieldDiff * f1, CBiosampleFieldDiff * f2)
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
        *m_ReportStream << "No differences found" << endl;
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
}


void CBiosampleChkApp::PrintResults(TBiosampleFieldDiffList & diffs)
{
    PrintDiffs(diffs);
}


const string kSequenceID = "Sequence ID";

// This function is for generating a table of biosample values for a bioseq
// that does not currently have a biosample ID
void CBiosampleChkApp::ProcessBioseqHandle(CBioseq_Handle bh, CRef<CSeq_table> table)
{
    vector<string> biosample_ids = GetBiosampleIDs(bh);
    if (biosample_ids.size() > 0) {
		// do not collect if already has biosample ID
		return;
	}
	vector<string> bioproject_ids = GetBioProjectIDs(bh);

    CSeqdesc_CI src_desc_ci(bh, CSeqdesc::e_Source);
	CSeqdesc_CI comm_desc_ci(bh, CSeqdesc::e_User);
	while (comm_desc_ci && !NStr::Equal(comm_desc_ci->GetUser().GetType().GetStr(), "StructuredComment")) {
		++comm_desc_ci;
	}
    if (!src_desc_ci && !comm_desc_ci && bioproject_ids.size() == 0) {
        return;
    }

    string sequence_id;
    bh.GetId().front().GetSeqId()->GetLabel(&sequence_id);
	size_t row = table->GetNum_rows();
	AddValueToTable (table, kSequenceID, sequence_id, row);

	if (bioproject_ids.size() > 0) {
		string val = bioproject_ids[0];
		for (size_t i = 1; i < bioproject_ids.size(); i++) {
			val += ";";
			val += bioproject_ids[i];
		}
		AddValueToTable(table, "BioProject", val, row);
	}

	if (src_desc_ci) {
		const CBioSource& src = src_desc_ci->GetSource();
	    TSrcTableColumnList src_fields = GetSourceFields(src);
		ITERATE(TSrcTableColumnList, it, src_fields) {
			AddValueToTable(table, (*it)->GetLabel(), (*it)->GetFromBioSource(src), row);
		}
	}
	if (comm_desc_ci) {
		const CUser_object& usr = comm_desc_ci->GetUser();
		TStructuredCommentTableColumnList comm_fields = GetStructuredCommentFields(usr);
	    ITERATE(TStructuredCommentTableColumnList, it, comm_fields) {
			AddValueToTable(table, (*it)->GetLabel(), (*it)->GetFromComment(usr), row);
		}
	}
	int num_rows = (int)row  + 1;
	table->SetNum_rows(num_rows);
}


TBiosampleFieldDiffList CBiosampleChkApp::ProcessBioseqHandle(CBioseq_Handle bh)
{
    TBiosampleFieldDiffList diffs;

    CSeqdesc_CI src_desc_ci(bh, CSeqdesc::e_Source);
	CSeqdesc_CI comm_desc_ci(bh, CSeqdesc::e_User);
	while (comm_desc_ci && !NStr::Equal(comm_desc_ci->GetUser().GetType().GetStr(), "StructuredComment")) {
		++comm_desc_ci;
	}
    if (!src_desc_ci && !comm_desc_ci) {
        return diffs;
    }
    
    vector<string> biosample_ids = GetBiosampleIDs(bh);

    if (m_Mode == e_generate_biosample && biosample_ids.size() > 0) {
        // for generate mode, do not collect if already has biosample ID
        return diffs;
    } else if (m_Mode == e_report_diffs && biosample_ids.size() == 0) {
        // for report mode, do not report if no biosample ID
        return diffs;
    }

    string sequence_id;
    bh.GetId().front().GetSeqId()->GetLabel(&sequence_id);

    switch (m_Mode) {
        case e_report_diffs:
            ITERATE(vector<string>, id, biosample_ids) {
                CRef<CSeq_descr> descr = GetBiosampleData(*id);
                if (descr) {
                    ITERATE(CSeq_descr::Tdata, it, descr->Get()) {
                        if ((*it)->IsSource()) {
							if (src_desc_ci) {
                                TBiosampleFieldDiffList these_diffs = GetFieldDiffs(sequence_id, *id, src_desc_ci->GetSource(), (*it)->GetSource());
							    diffs.insert(diffs.end(), these_diffs.begin(), these_diffs.end());
							}
                        } else if ((*it)->IsUser()) {
							if (comm_desc_ci) {
                                TBiosampleFieldDiffList these_diffs = GetFieldDiffs(sequence_id, *id, comm_desc_ci->GetUser(), (*it)->GetUser());
                                diffs.insert(diffs.end(), these_diffs.begin(), these_diffs.end());
							}
						}
                    }
                }
            }
            break;
        case e_generate_biosample:
			if (src_desc_ci) {
				ITERATE(TSrcTableColumnList, field, m_SrcReportFields) {
					string src_val = (*field)->GetFromBioSource(src_desc_ci->GetSource());
					CRef<CBiosampleFieldDiff> one_diff(new CBiosampleFieldDiff(sequence_id, "", (*field)->GetLabel(), src_val, ""));
					diffs.push_back(one_diff);
				}
			}
			if (comm_desc_ci) {
				ITERATE(TStructuredCommentTableColumnList, field, m_StructuredCommentReportFields) {
					string src_val = (*field)->GetFromComment(comm_desc_ci->GetUser());
					CRef<CBiosampleFieldDiff> one_diff(new CBiosampleFieldDiff(sequence_id, "", (*field)->GetLabel(), src_val, ""));
					diffs.push_back(one_diff);
				}
			}

            break;
    }

    return diffs;
}


TBiosampleFieldDiffList CBiosampleChkApp::ProcessSeqEntry(const CSeq_entry& se)
{
    TBiosampleFieldDiffList diffs;

    CRef<CScope> scope = BuildScope();
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(se);
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        TBiosampleFieldDiffList these_diffs = ProcessBioseqHandle(*bi);
        diffs.insert(diffs.end(), these_diffs.begin(), these_diffs.end());
        ++bi;
    }

    return diffs;
}


void CBiosampleChkApp::ProcessSeqEntry(const CSeq_entry& se, CRef<CSeq_table> table)
{
    CRef<CScope> scope = BuildScope();
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(se);
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
		ProcessBioseqHandle(*bi, table);
		++bi;
	}
}


void CBiosampleChkApp::ProcessSeqEntry(CRef<CSeq_table> table)
{
    // Get seq-entry to process
    CRef<CSeq_entry> se(ReadSeqEntry());

    return ProcessSeqEntry(*se, table);
}


TBiosampleFieldDiffList CBiosampleChkApp::ProcessSeqEntry(void)
{
    // Get seq-entry to process
    CRef<CSeq_entry> se(ReadSeqEntry());

    return ProcessSeqEntry(*se);
}


void CBiosampleChkApp::ProcessSeqSubmit(CRef<CSeq_table> table)
{
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to process
    m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);

    // Validae Seq-submit
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        ITERATE(CSeq_submit::TData::TEntrys, se, ss->GetData().GetEntrys()) {
            ProcessSeqEntry(**se, table);
        }
    }
}


TBiosampleFieldDiffList CBiosampleChkApp::ProcessSeqSubmit(void)
{
    TBiosampleFieldDiffList diffs;
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to process
    m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);

    // Validae Seq-submit
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        ITERATE(CSeq_submit::TData::TEntrys, se, ss->GetData().GetEntrys()) {
            TBiosampleFieldDiffList these_diffs = ProcessSeqEntry(**se);
            diffs.insert(diffs.end(), these_diffs.begin(), these_diffs.end());
        }
    }

    return diffs;
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
