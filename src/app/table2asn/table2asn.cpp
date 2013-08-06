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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko
 *
 * File Description:
 *   validator
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
#include <objects/submit/Seq_submit.hpp>
#include <objects/seq/Seq_descr.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#if 0
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#endif

#include "multireader.hpp"

#include <util/line_reader.hpp>

#include <common/test_assert.h>  /* This header must go last */

using namespace ncbi;
using namespace objects;
//using namespace validator;

const char * TBL2ASN_APP_VER = "10.0";

/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CTbl2AsnApp : public CNcbiApplication
{
public:
    CTbl2AsnApp(void);

    virtual void Init(void);
    virtual int  Run (void);

private:

	CMultiReader m_reader;

    void Setup(const CArgs& args);

    void ProcessOneFile(const CTable2AsnContext& context, const string& pathname);
	void ProcessOneFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result);
	bool ProcessOneDirectory(const CTable2AsnContext& context, const CDir& directory, const CMask& mask, bool recurse);
	void ProcessSecretFiles(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result);
	void ProcessTBLFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result);
	void ProcessSRCFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result);
	void ProcessQVLFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result);
	void ProcessDSCFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result);
	void ProcessCMTFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result);
	void ProcessCMTFileByCols(const CTable2AsnContext& context, ILineReader& pathname, CRef<CSerialObject>& result);
	void ProcessCMTFileByRows(const CTable2AsnContext& context, ILineReader& pathname, CRef<CSerialObject>& result);
	void ProcessPEPFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result);
	void ProcessRNAFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result);
	void ProcessPRTFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result);

#if 0
    CRef<CSeq_entry> ReadSeqEntry(void);
    CRef<CSeq_feat> ReadSeqFeat(void);
    CRef<CBioSource> ReadBioSource(void);
    CRef<CPubdesc> ReadPubdesc(void);
#endif

    CRef<CScope> BuildScope(void);

    CRef<CObjectManager> m_ObjMgr;
    //auto_ptr<CObjectIStream> m_In;
    //unsigned int m_Options;
    //bool m_Continue;
    //bool m_OnlyAnnots;

    //size_t m_Level;
    //size_t m_Reported;
    //size_t m_ReportLevel;

    //bool m_DoCleanup;
    //CCleanup m_Cleanup;

    //EDiagSev m_LowCutoff;
    //EDiagSev m_HighCutoff;

    //CNcbiOstream* m_OutputStream;
    //CNcbiOstream* m_LogStream;
};


CTbl2AsnApp::CTbl2AsnApp(void) :
    m_ObjMgr(0)
    //, m_In(0), m_Options(0), m_Continue(false), m_OnlyAnnots(false)
    //m_Level(0), m_Reported(0), m_OutputStream(0), m_LogStream(0)
{
}


void CTbl2AsnApp::Init(void)
{
	
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Prepare command line descriptions, inherit them from tbl2asn legacy application

    arg_desc->AddOptionalKey
        ("p", "Directory", "Path to ASN.1 Files",
        CArgDescriptions::eInputFile);

	arg_desc->AddOptionalKey
        ("r", "Directory", "Path to results",
        CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey
        ("i", "InFile", "Single Input File",
        CArgDescriptions::eInputFile);

	arg_desc->AddOptionalKey(
        "o", "OutFile", "Single Output File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey
        ("x", "String", "Suffix", CArgDescriptions::eString, ".fsa");

    arg_desc->AddFlag("E", "Recurse");  // done
    arg_desc->AddOptionalKey            // done
        ("t", "InFile", "Template File",
        CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey
        ("a", "String", "File Type\n\
  a Any\n\
  r20u Runs of 20+ Ns are gaps, 100 Ns are unknown length\n\
  r20k Runs of 20+ Ns are gaps, 100 Ns are known length\n\
  r10u Runs of 10+ Ns are gaps, 100 Ns are unknown length\n\
  r10k Runs of 10+ Ns are gaps, 100 Ns are known length\n\
  s FASTA Set (s Batch, s1 Pop, s2 Phy, s3 Mut, s4 Eco, s9 Small-genome)\n\
  d FASTA Delta, di FASTA Delta with Implicit Gaps\n\
  l FASTA+Gap Alignment (l Batch, l1 Pop, l2 Phy, l3 Mut, l4 Eco, l9 Small-genome)\n\
  z FASTA with Gap Lines\n\
  e PHRAP/ACE\n\
  b ASN.1 for -M flag", CArgDescriptions::eString, "a");

    arg_desc->AddFlag("s", "Read FASTAs as Set");
    arg_desc->AddFlag("g", "Genomic Product Set");
    arg_desc->AddFlag("J", "Delayed Genomic Product Set	");
    arg_desc->AddDefaultKey
        ("F", "String", "Feature ID Links\n\
  o By Overlap\n\
  p By Product", CArgDescriptions::eString, "o");

    arg_desc->AddOptionalKey
        ("A", "String", "Accession", CArgDescriptions::eString);
    arg_desc->AddOptionalKey
        ("C", "String", "Genome Center Tag", CArgDescriptions::eString);
    arg_desc->AddOptionalKey
        ("n", "String", "Organism Name", CArgDescriptions::eString); // done
    arg_desc->AddOptionalKey
        ("j", "String", "Source Qualifiers", CArgDescriptions::eString);
    arg_desc->AddOptionalKey
        ("y", "String", "Comment", CArgDescriptions::eString); // done
    arg_desc->AddOptionalKey
        ("Y", "InFile", "Comment File", CArgDescriptions::eInputFile); // done
    arg_desc->AddOptionalKey
        ("D", "InFile", "Descriptors File", CArgDescriptions::eInputFile); // done
    arg_desc->AddOptionalKey
        ("f", "InFile", "Single Table File", CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey
        ("k", "String", "CDS Flags (combine any of the following letters)\n\
  c Annotate Longest ORF\n\
  r Allow Runon ORFs\n\
  m Allow Alternative Starts\n\
  k Set Conflict on Mismatch", CArgDescriptions::eString);

    arg_desc->AddOptionalKey
        ("V", "String", "Verification (combine any of the following letters)\n\
  v Validate with Normal Stringency\n\
  r Validate without Country Check\n\
  c BarCode Validation\n\
  b Generate GenBank Flatfile\n\
  g Generate Gene Report\n\
  t Validate with TSA Check", CArgDescriptions::eString);

    arg_desc->AddFlag("q", "Seq ID from File Name");
    arg_desc->AddFlag("u", "GenProdSet to NucProtSet");
    arg_desc->AddFlag("I", "General ID to Note");

    arg_desc->AddOptionalKey("G", "String", "Alignment Gap Flags (comma separated fields, e.g., p,-,-,-,?,. )\n\
  n Nucleotide or p Protein,\n\
  Begin, Middle, End Gap Characters,\n\
  Missing Characters, Match Characters", CArgDescriptions::eString);

    arg_desc->AddFlag("R", "Remote Sequence Record Fetching from ID");
    arg_desc->AddFlag("S", "Smart Feature Annotation");

	arg_desc->AddOptionalKey("Q", "String", "mRNA Title Policy\n\
  s Special mRNA Titles\n\
  r RefSeq mRNA Titles", CArgDescriptions::eString);

    arg_desc->AddFlag("U", "Remove Unnecessary Gene Xref");
    arg_desc->AddFlag("L", "Force Local protein_id/transcript_id");
    arg_desc->AddFlag("T", "Remote Taxonomy Lookup");
    arg_desc->AddFlag("P", "Remote Publication Lookup");
    arg_desc->AddFlag("W", "Log Progress");
    arg_desc->AddFlag("K", "Save Bioseq-set");

	arg_desc->AddOptionalKey("H", "String", "Hold Until Publish\n\
  y Hold for One Year\n\
  mm/dd/yyyy", CArgDescriptions::eString); // done

	arg_desc->AddOptionalKey(
        "Z", "OutFile", "Discrepancy Report Output File", CArgDescriptions::eOutputFile);

	arg_desc->AddOptionalKey("c", "String", "Cleanup (combine any of the following letters)\n\
  d Correct Collection Dates (assume month first)\n\
  D Correct Collection Dates (assume day first)\n\
  b Append note to coding regions that overlap other coding regions with similar product names and do not contain 'ABC'\n\
  x Extend partial ends of features by one or two nucleotides to abut gaps or sequence ends\n\
  p Add exception to non-extendable partials\n\
  s Add exception to short introns\n\
  f Fix product names", CArgDescriptions::eString);

	arg_desc->AddOptionalKey("z", "OutFile", "Cleanup Log File", CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("X",  "String", "Extra Flags (combine any of the following letters)\n\
  A Automatic definition line generator\n\
  C Apply comments in .cmt files to all sequences\n\
  E Treat like eukaryota in the Discrepancy Report", CArgDescriptions::eString);

	arg_desc->AddDefaultKey("N", "Integer", "Project Version Number", CArgDescriptions::eInteger, "0");

	arg_desc->AddOptionalKey("w", "InFile", "Single Structured Comment File (overrides the use of -X C)", CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("M", "String", "Master Genome Flags\n\
  n Normal\n\
  b Big Sequence\n\
  p Power Option\n\
  t TSA", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("l", "String", "Add type of evidence used to assert linkage across assembly gaps (only for TSA records). Must be one of the following:\n\
  paired-ends\n\
  align-genus\n\
  align-xgenus\n\
  align-trnscpt\n\
  within-clone\n\
  clone-contig\n\
  map\n\
  strobe", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("m", "String", "Lineage to use for Discrepancy Report tests", CArgDescriptions::eString);

    // Program description
    string prog_description = "Converts files of various formats to ASN.1\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());
}


int CTbl2AsnApp::Run(void)
{
	CTable2AsnContext context;
    const CArgs& args = GetArgs();

    Setup(args);

    //m_LogStream = args["L"] ? &(args["L"].AsOutputFile()) : &NcbiCout;

	/*

    // note - the C Toolkit uses 0 for SEV_NONE, but the C++ Toolkit uses 0 for SEV_INFO
    // adjust here to make the inputs to table2asn match tbl2asn expectations
    //m_ReportLevel = args["R"].AsInteger() - 1;
    //m_LowCutoff = static_cast<EDiagSev>(args["Q"].AsInteger() - 1);
    //m_HighCutoff = static_cast<EDiagSev>(args["P"].AsInteger() - 1);

    //m_DoCleanup = args["cleanup"] && args["cleanup"].AsBoolean();

    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    m_Reported = 0;
	*/

	if (args["n"])
		context.nOrganismName = args["n"].AsString();

	if (args["y"])
		context.yComment = args["y"].AsString();
	else
	if (args["Y"])
	{
		CNcbiIfstream comments(args["Y"].AsString().c_str());
		comments >> context.yComment;
	}

	context.gGenomicProductSet = args["g"];
	context.sHandleAsSet = args["s"];

	if (args["t"])
	{
		//context.tTemplateFile = args["t"].AsString();

		m_reader.LoadTemplate(context, args["t"].AsString(), context.m_other_template, context.m_submit_template);
	}
	if (args["D"])
	{
		m_reader.LoadDescriptors(context, args["D"].AsString(), context.m_descriptors);
	}

	if (args["H"])
	{
        try 
		{	   
		   CTime time(args["H"].AsString(), "M/D/Y" );
		   context.HoldUntilPublish.SetToTime(time, CDate::ePrecision_day);
        } 
		catch( const CException & ) 
		{
			int years = NStr::StringToInt(args["H"].AsString());
			CTime time; 
			time.SetCurrent();
			time.SetYear(time.Year()+years);
			context.HoldUntilPublish.SetToTime(time, CDate::ePrecision_day);
//                    // couldn't parse date
//                    x_HandleBadModValue(*mod);
        }
	}

	context.NProjectVersionNumber = args["N"].AsInteger();

	// Designate where do we output files: local folder, specified folder or a specific single output file
	if (args["o"])
	{
		context.m_output = &args["o"].AsOutputFile();
	}
	else
	{
		if (args["r"])
		{
			context.rResultsDirectory = args["r"].AsString();
		}
		else
		{
			context.rResultsDirectory = ".";
		}
		context.rResultsDirectory = CDir::AddTrailingPathSeparator(context.rResultsDirectory);

		CDir outputdir(context.rResultsDirectory);
		if (!outputdir.Exists())
			outputdir.Create();
	}

	// Designate where do we get input: single file or a folder or folder structure
    if ( args["p"] ) 
	{

		CDir directory(args["p"].AsString());
		if (directory.Exists())
		{
			CMaskFileName masks;
			masks.Add("*" +args["x"].AsString());

			ProcessOneDirectory (context, directory, masks, args["E"].AsBoolean());
		}
    } else {
        if (args["i"]) 
		{
			ProcessOneFile (context, args["i"].AsString());
        }
    }

	/*
    if (m_Reported > 0) {
        return 1;
    } else {
        return 0;
    }
	*/

	//m_reader.Run(args);

	return 0;
}


CRef<CScope> CTbl2AsnApp::BuildScope (void)
{
    CRef<CScope> scope(new CScope (*m_ObjMgr));
    scope->AddDefaults();

    return scope;
}


#if 0 
CRef<CSeq_entry> CTbl2AsnApp::ReadSeqEntry(void)
{
    CRef<CSeq_entry> se(new CSeq_entry);
    m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);

    return se;
}
#endif

void CTbl2AsnApp::ProcessOneFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result)
{
	m_reader.Process(context, *this);

	result = m_reader.LoadFile(context, pathname);

	if (context.m_descriptors.NotNull())
	  m_reader.ApplyDescriptors(*result, *context.m_descriptors);
	ProcessSecretFiles(context, pathname, result);
	m_reader.ApplyAdditionalProperties(context, result);
}

string GenerateOutputStream(const CTable2AsnContext& context, const string& pathname)
{
	string dir;
	string outputfile;
	string base;
	CDirEntry::SplitPath(pathname, &dir, &base, 0);

	outputfile = context.rResultsDirectory.empty() ? dir : context.rResultsDirectory;
	outputfile += base;
	outputfile += ".asn";

	cout << "Opening:" << pathname << ":" << outputfile << endl;

	return outputfile.c_str();
}

void CTbl2AsnApp::ProcessOneFile(const CTable2AsnContext& context, const string& pathname)
{
	CNcbiOstream* output = 0;
	auto_ptr<CNcbiOfstream> local_output(0);
	CFile local_file;
	try
	{
		if (context.m_output == 0)
		{
			local_file = GenerateOutputStream(context, pathname);
			local_output.reset(new CNcbiOfstream(local_file.GetPath().c_str()));
			output = local_output.get();
		}
		else
		{
			output = context.m_output;
		}

		CRef<CSerialObject> obj;
		ProcessOneFile(context, pathname, obj);
		m_reader.WriteObject(*obj, *output);
	}
	catch(...)
	{
		// if something goes wrong - remove the partial output to avoid confuse
		if (local_output.get())
		{
			local_file.Remove();
		}
		throw;
	}
}

bool CTbl2AsnApp::ProcessOneDirectory(const CTable2AsnContext& context, const CDir& directory, const CMask& mask, bool recurse)
{
	//cout << "Entering directory " << path << endl;
	CDir::TEntries* e = directory.GetEntriesPtr("*", CDir::fCreateObjects | CDir::fIgnoreRecursive);
	auto_ptr<CDir::TEntries> entries(e);

	for (CDir::TEntries::const_iterator it = e->begin(); it != e->end(); it++)
	{
		// first process files and then recursivelly access other folders
		if (!(*it)->IsDir())
		{
		   if (mask.Match((*it)->GetPath()))
		      ProcessOneFile(context, (*it)->GetPath());
		}
		else
		if (recurse)
		{
			ProcessOneDirectory(context, **it, mask, recurse);
		}
	}

	return true;
}

void CTbl2AsnApp::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    // Create object manager
    m_ObjMgr = CObjectManager::GetInstance();
	if ( args["r"] ) {
        // Create GenBank data loader and register it with the OM.
        // The last argument "eDefault" informs the OM that the loader must
        // be included in scopes during the CScope::AddDefaults() call.
        //CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
    }
}

/*
tbl    5-column Feature Table 
.src   tab-delimited table with source qualifiers
.qvl   PHRAP/PHRED/consed quality scores
.dsc  One or more descriptors in ASN.1 format
.cmt  Tab-delimited file for structured comment
.pep  Replacement proteins for coding regions on this sequence, use to mark conflicts
.rna   Replacement mRNA sequences for RNA editing
.prt    Proteins for suggest intervals
*/
void CTbl2AsnApp::ProcessSecretFiles(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result)
{
	string dir;
	string base;
	string ext;
	CDirEntry::SplitPath(pathname, &dir, &base, &ext);

	string name = dir + base;

	ProcessTBLFile(context, name + ".tbl", result);
	ProcessSRCFile(context, name + ".src", result);
	ProcessQVLFile(context, name + ".qvl", result);
	ProcessDSCFile(context, name + ".dsc", result);
	ProcessCMTFile(context, name + ".cmt", result);
	ProcessPEPFile(context, name + ".pep", result);
	ProcessRNAFile(context, name + ".rna", result);
	ProcessPRTFile(context, name + ".prt", result);
}

void CTbl2AsnApp::ProcessTBLFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result)
{
	CFile file(pathname);
	if (!file.Exists()) return;
}

void CTbl2AsnApp::ProcessSRCFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result)
{
	CFile file(pathname);
	if (!file.Exists()) return;
}

void CTbl2AsnApp::ProcessQVLFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result)
{
	CFile file(pathname);
	if (!file.Exists()) return;
}

void CTbl2AsnApp::ProcessDSCFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result)
{
	CFile file(pathname);
	if (!file.Exists()) return;

	CRef<CSeq_descr> descr;
	m_reader.LoadDescriptors(context, pathname, descr);
	CMultiReader::ApplyDescriptors(*result, *descr);
}

void CTbl2AsnApp::ProcessCMTFileByCols(const CTable2AsnContext& context, ILineReader& reader, CRef<CSerialObject>& result)
{
}

void CTbl2AsnApp::ProcessCMTFileByRows(const CTable2AsnContext& context, ILineReader& reader, CRef<CSerialObject>& result)
{
	CUser_object* obj = 0;
	while (!reader.AtEOF())
	{
		reader.ReadLine();
		if (!reader.AtEOF())
		{
			string current = reader.GetCurrentLine();
			if (!current.empty())
			{
				int index = current.find('\t');
				if (index != string::npos )
				{
					string commentname = current.substr(0, index);
					current.erase(current.begin(), current.begin()+index+1);
					obj = m_reader.AddStructuredComment(obj, commentname, current, *result);
				}
			}
		}
	}
}

void CTbl2AsnApp::ProcessCMTFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result)
{
	CFile file(pathname);
	if (!file.Exists()) return;

	CRef<ILineReader> reader(ILineReader::New(pathname));

	//if (context.wInFile
	ProcessCMTFileByRows(context, *reader, result);
	//else
	//ProcessCMTFileByCols(context, *reader, result);
}

void CTbl2AsnApp::ProcessPEPFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result)
{
	CFile file(pathname);
	if (!file.Exists()) return;

	// CPropsLine
}

void CTbl2AsnApp::ProcessRNAFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result)
{
	CFile file(pathname);
	if (!file.Exists()) return;
}

void CTbl2AsnApp::ProcessPRTFile(const CTable2AsnContext& context, const string& pathname, CRef<CSerialObject>& result)
{
	CFile file(pathname);
	if (!file.Exists()) return;
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CTbl2AsnApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
