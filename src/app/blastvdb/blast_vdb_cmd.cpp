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
 * Author: Amelia Fong
 *
** @file blast_vdb_cmd.cpp
 * Command line tool to get vdb info.
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objmgr/util/sequence.hpp>
#include <internal/blast/vdb/vdb2blast_util.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/vdb/vdbalias.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(blast);

static const NStr::TNumToStringFlags kFlags = NStr::fWithCommas;

/// The application class
class CBlastVdbCmdApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CBlastVdbCmdApp() {}
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();
    
    /// Initializes the application's data members
    void x_InitApplicationData();

    /// Get vdb util
    CRef<CVDBBlastUtil> x_GetVDBBlastUtil(bool isCSRA);

    /// Prints the BLAST database information (e.g.: handles -info command line
    /// option)
    int x_PrintBlastDatabaseInformation();

    /// Scan dbs in 2na format
    /// @return 0 on success; 1 if some sequences were not retrieved
    int x_ScanCompressedDatabase();

    /// Scan dbs in 4na format
    /// @return 0 on success; 1 if some sequences were not retrieved
    int x_ScanUncompressedDatabase();

    /// Processes all requests except printing the BLAST database information
    /// @return 0 on success; 1 if some sequences were not retrieved
    int x_ProcessSearchRequest();

    /// Print vdb paths
    int x_PrintVDBPaths(bool recursive);

    /// Resolve vdb paths
    void x_GetFullPaths();

    /// Retrieve the queries from the command line arguments
    vector<string> x_GetQueries();

    string x_FormatRuntime(const CStopWatch& sw) const;

    // Store all db names
    string m_allDbs;
    string m_origDbs;
    bool m_isRef;
};

/** Class to extract FASTA (as returned by the blast_sra library) from SRA
 * data.
 *
 * Inspired by the CSeqFormatter class 
 */
class CVdbFastaExtractor {
public:
    CVdbFastaExtractor(CRef<CVDBBlastUtil> sraobj, CNcbiOstream& out,
		       	   	   TSeqPos line_width = 80 )
        : m_VdbBlastDB(sraobj), m_Out(out), m_LineWidth(line_width) {}

    void Write(CRef<CSeq_id> seqid) {
        CRef<CBioseq> bioseq = m_VdbBlastDB->CreateBioseqFromVDBSeqId(seqid);
        if (bioseq.Empty()) {
            throw runtime_error("Failed to find Bioseq for '" + seqid->AsFastaString() + "'");
        }
        CFastaOstream fasta(m_Out);
        fasta.SetWidth(m_LineWidth);
        fasta.SetAllFlags(CFastaOstream::fKeepGTSigns|CFastaOstream::fNoExpensiveOps);
        fasta.Write(*bioseq);
    }

    void DumpAll() {
        CFastaOstream fasta(m_Out);
        fasta.SetWidth(m_LineWidth);
        fasta.SetAllFlags(CFastaOstream::fKeepGTSigns|CFastaOstream::fNoExpensiveOps);
        BlastSeqSrc* seqsrc = m_VdbBlastDB->GetSRASeqSrc();
        BlastSeqSrcGetSeqArg seq_arg = { '\0' };
        BlastSeqSrcIterator * itr = BlastSeqSrcIteratorNewEx(1);
   	    while ((seq_arg.oid = BlastSeqSrcIteratorNext(seqsrc, itr)) !=
        	          BLAST_SEQSRC_EOF)
        {
   	    	 if (seq_arg.oid == BLAST_SEQSRC_ERROR) {
          			cerr << "Iterator returns BLAST_SEQSRC_ERROR" << endl;
           			return;
             }
   	    	 CRef<CBioseq> bioseq = m_VdbBlastDB->CreateBioseqFromOid(seq_arg.oid);
             if (bioseq.Empty())
             {
        	  		cerr << "Empty Bioseq"  << endl;
        	    	return;
             }
             fasta.Write(*bioseq);
        }
    }
private:
    CRef<CVDBBlastUtil> m_VdbBlastDB;
    CNcbiOstream& m_Out;
    TSeqPos m_LineWidth;

};

string s_GetCSRADBs(const string & db_list, string & not_csra_list) {
	vector<string> dbs;
	string csra_list = kEmptyStr;
	not_csra_list = kEmptyStr;
	NStr::Split(db_list, " ", dbs);
	for(unsigned int i=0; i < dbs.size(); i++) {
		if(CVDBBlastUtil::IsCSRA(dbs[i])) {
			csra_list += dbs[i] + " ";
		}
		else {
			not_csra_list += dbs[i] + " ";
		}
	}
	return csra_list;
}

vector<string>
CBlastVdbCmdApp::x_GetQueries()
{
    const CArgs& args = GetArgs();
    vector<string> retval;

    if (args["entry"].HasValue()) {

        static const string kDelim(",");
        const string& entry = args["entry"].AsString();

        if (entry.find(kDelim[0]) != string::npos) {
            vector<string> tokens;
            NStr::Split(entry, kDelim, tokens);
            retval.swap(tokens);
        } else {
            retval.push_back(entry);
        }

    } else if (args["entry_batch"].HasValue()) {

        CNcbiIstream& input = args["entry_batch"].AsInputFile();
        retval.reserve(256); // arbitrary value
        while (input) {
            string line;
            NcbiGetlineEOL(input, line);
            if ( !line.empty() ) {
                retval.push_back(line);
            }
        }
    } else {
        NCBI_THROW(CInputException, eInvalidInput, 
                   "Must specify query type: one of 'entry', or 'entry_batch'");
    }

    if (retval.empty()) {
        NCBI_THROW(CInputException, eInvalidInput,
                   "Entry not found in BLAST database");
    }

    return retval;
}

int
CBlastVdbCmdApp::x_ProcessSearchRequest()
{
    const CArgs& args = GetArgs();
    CNcbiOstream& out = args["out"].AsOutputFile();

    bool errors_found = false;

    /* Special case: full db dump */
    if (args["entry"].HasValue() && args["entry"].AsString() == "all") {
        try {
        	CRef<CVDBBlastUtil> util = x_GetVDBBlastUtil(m_isRef);
        	CVdbFastaExtractor seq_fmt(util, out,  args["line_length"].AsInteger());
            seq_fmt.DumpAll();
        } catch (const CException& e) {
            ERR_POST(Error << e.GetMsg());
            errors_found = true;
        } catch (...) {
            ERR_POST(Error << "Failed to retrieve requested item");
            errors_found = true;
        }
        return errors_found ? 1 : 0;
    }

    vector<string> queries = x_GetQueries();
    _ASSERT( !queries.empty() );

    CRef<CVDBBlastUtil> util = x_GetVDBBlastUtil(false);
    CVdbFastaExtractor seq_fmt(util, out, args["line_length"].AsInteger());

   	CRef<CVDBBlastUtil> util_csra = x_GetVDBBlastUtil(true);
    CVdbFastaExtractor * seq_fmt_csra = NULL;
   	if(util_csra.NotEmpty()) {
    	seq_fmt_csra = new CVdbFastaExtractor(util_csra, out, args["line_length"].AsInteger());
    }

    NON_CONST_ITERATE(vector<string>, itr, queries) {
        try { 
        	CRef<CSeq_id> seq_id (new CSeq_id(*itr));
        	switch (CVDBBlastUtil::VDBIdType(*seq_id)) {
        	case CVDBBlastUtil::eSRAId:
        	case CVDBBlastUtil::eWGSId:
        			seq_fmt.Write(seq_id);
        	break;
        	case CVDBBlastUtil::eCSRALocalRefId:
        	case CVDBBlastUtil::eCSRARefId:
        	{
        		if(seq_fmt_csra == NULL) {
           			NCBI_THROW(CInputException, eInvalidInput, *itr + ": CSRA ref seq id for non CSRA db");
           		}
           		seq_fmt_csra->Write(seq_id);
           	}
        	break;
        	default :
       			NCBI_THROW(CInputException, eInvalidInput, *itr + " is not a valid SRA, CSRA ref or WGS id");
       		break;
        	}

        } catch (const CException& e) {
            ERR_POST(Error << e.GetMsg());
            errors_found = true;
        } catch (...) {
            ERR_POST(Error << "Failed to retrieve requested item");
            errors_found = true;
        }

    }
    if(seq_fmt_csra != NULL) {
       	delete seq_fmt_csra;
    }
    return errors_found ? 1 : 0;
}

int
CBlastVdbCmdApp::x_ScanCompressedDatabase()
{
    CStopWatch sw;
    sw.Start();
    CRef<CVDBBlastUtil>  util=x_GetVDBBlastUtil(m_isRef);
    if(util.Empty()) {
		NCBI_THROW(CInputException, eInvalidInput, "Cannot scan ref seq");
    }

    BlastSeqSrc* seqsrc = util->GetSRASeqSrc();
    Uint8 bases_scanned = 0;
    char base;

    BlastSeqSrcGetSeqArg seq_arg = { '\0' };
    	BlastSeqSrcIterator * itr =
    			BlastSeqSrcIteratorNewEx(1);

    	while ( (seq_arg.oid = BlastSeqSrcIteratorNext(seqsrc, itr)) !=
            BLAST_SEQSRC_EOF)
    	{
    		if (seq_arg.oid == BLAST_SEQSRC_ERROR) {
    			cerr << "Error while iterating over BLAST VDB database (2na)" << endl;
    			return 1;
    		}

    		seq_arg.encoding = eBlastEncodingNcbi2na;
    		seq_arg.reset_ranges = TRUE;
    		if (BlastSeqSrcGetSequence(seqsrc, &seq_arg) < 0) {
    			continue;
                }
    		for (int i = 0; i < seq_arg.seq->length/4; i++) {
    			base = seq_arg.seq->sequence[i];
    		}
    		bases_scanned +=seq_arg.seq->length;
    		BlastSeqSrcReleaseSequence(seqsrc, &seq_arg);
    	}
    sw.Stop();
    cout << "PERF: "<< setiosflags(ios::fixed) << setprecision(2)
            << bases_scanned / sw.Elapsed() << " bases/second" << endl;
    cout << "PERF: "<< setiosflags(ios::fixed) << setprecision(2)
    << "Scanned " << NStr::ULongToString(bases_scanned, kFlags) <<  " bases in "<<  x_FormatRuntime(sw) << endl;

    // Silence set but not used warning
    base= base?1:0;

    return 0;
}

int
CBlastVdbCmdApp::x_ScanUncompressedDatabase()
{
    CStopWatch sw;
    sw.Start();
    CRef<CVDBBlastUtil>  util=x_GetVDBBlastUtil(m_isRef);
    if(util.Empty()) {
    	NCBI_THROW(CInputException, eInvalidInput, "Cannot scan ref seq");
    }
    BlastSeqSrc* seqsrc = util->GetSRASeqSrc();
    Uint8 bases_scanned = 0;
    const Uint8 num_seqs = BlastSeqSrcGetNumSeqs(seqsrc);
    BlastSeqSrcGetSeqArg seq_arg = { '\0' };
    seq_arg.encoding = eBlastEncodingNucleotide;
    seq_arg.reset_ranges = TRUE;
    Uint8 err_counter = 0;
    Uint8 counter = 0;
    char base;

    //Break after 1001 (arbitrary) consecutive errors to prevent infinite loop
    //Note that some oids may correspond to nothing and occasional error is ok
   	while ( (counter < num_seqs) && (err_counter <= 1000))
   	{
   		seq_arg.oid = counter;
    	if (BlastSeqSrcGetSequence(seqsrc, &seq_arg) < 0) {
    		err_counter ++;
    		continue;
    	}

    	for (int i = 0; i < seq_arg.seq->length; i++) {
    		base = seq_arg.seq->sequence[i];
    	}
    	bases_scanned +=seq_arg.seq->length;
    	err_counter = 0;
    	counter ++;
    	BlastSeqSrcReleaseSequence(seqsrc, &seq_arg);
    }

   	if((counter != num_seqs) || (err_counter > 1000)){
   		cerr << "Error while scanning uncompressed (4na) dbs" << endl;
   		return 1;
   	}

    sw.Stop();
    cout << "PERF: "<< setiosflags(ios::fixed) << setprecision(2)
            << bases_scanned / sw.Elapsed() << " bases/second" << endl;
    cout << "PERF: "<< setiosflags(ios::fixed) << setprecision(2)
    << "Scanned " << NStr::ULongToString(bases_scanned, kFlags) <<  " bases in "<<  x_FormatRuntime(sw) << endl;

    // Silence set but not used warning
    base= base?1:0;

    return 0;
}


string
CBlastVdbCmdApp::x_FormatRuntime(const CStopWatch& sw) const
{
    const string& outfmt = GetArgs()["outfmt"].AsString();
    string retval;
    if (outfmt == "smart") {
        retval = sw.AsSmartString();
    } else if (outfmt == "seconds") {
        retval = NStr::IntToString((int)sw.Elapsed());
    } else {
        abort();
    }
    return retval;
}

void
CBlastVdbCmdApp::x_InitApplicationData()
{
    const CArgs& args = GetArgs();
    string strAllRuns;
    if (args["db"]) {
        strAllRuns = args["db"].AsString();

    } else {
        CNcbiIstream& in = args["dbs_file"].AsInputFile();
        string line;
        while (NcbiGetline(in, line, "\n")) {
            if (line.empty()) {
                continue;
            }
            strAllRuns += line + " ";
        }
    }
    list<string> tmp;
    NStr::Split(strAllRuns, "\n\t ", tmp, NStr::fSplit_Tokenize);
    m_origDbs = NStr::Join(tmp, " ");
    if (args["ref"]) {
    	m_isRef = true;
    }
    else {
    	m_isRef = false;
    }
}

void
CBlastVdbCmdApp::x_GetFullPaths()
{
	vector<string> vdbs;
	vector<string> vdb_alias;
	vector<string> db_alias;
	CVDBAliasUtil::FindVDBPaths(m_origDbs, false, vdbs, &db_alias, &vdb_alias, true, true);

	m_allDbs = NStr::Join(vdbs, " ");
}

CRef<CVDBBlastUtil>
CBlastVdbCmdApp::x_GetVDBBlastUtil(bool isCSRA)
{
	CRef<CVDBBlastUtil>  util;
	if (isCSRA) {
		string not_csra_list = kEmptyStr;
		string csra_list = s_GetCSRADBs(m_allDbs, not_csra_list);
		if(csra_list == kEmptyStr) {
			return util;
		}

		CStopWatch sw;
		sw.Start();
		util.Reset(new CVDBBlastUtil(csra_list, true, true));
		sw.Stop();
		cout << "PERF: blast_vdb library csra initialization: " << x_FormatRuntime(sw) << endl;
	}
	else {
		CStopWatch sw;
    	sw.Start();
    	util.Reset(new CVDBBlastUtil(m_allDbs, true, false));
    	sw.Stop();
    	cout << "PERF: blast_vdb library initialization: " << x_FormatRuntime(sw) << endl;
	}
    return util;
}

void s_PrintStr(const string & str, unsigned int line_width, CNcbiOstream & out)
{
	list<string> print_str;
	NStr::Wrap(str, line_width, print_str);
	ITERATE(list<string>, itr, print_str) {
		out << *itr << endl;
	}
}

int
CBlastVdbCmdApp::x_PrintBlastDatabaseInformation()
{
    const string kLetters("bases");
    const CArgs& args = GetArgs();
    CNcbiOstream& out = args["out"].AsOutputFile();
    unsigned int line_width = args["line_length"].AsInteger();
    Uint8 num_seqs(0), length(0), max_seq_length(0), av_seq_length(0);
    Uint8 ref_num_seqs(0), ref_length(0);
    string not_csra_dbs = kEmptyStr;
    string csra_dbs = s_GetCSRADBs(m_allDbs, not_csra_dbs);
    CStopWatch sw;
    sw.Start();
    CVDBBlastUtil::GetVDBStats(m_allDbs, num_seqs, length, max_seq_length, av_seq_length);
    if(csra_dbs != kEmptyStr) {
    	CVDBBlastUtil::GetVDBStats(csra_dbs, ref_num_seqs, ref_length, true);
    }
    sw.Stop();

    // Print basic database information
    out << "Database(s): ";
    if(m_origDbs.size() > line_width) {
    	out << endl;
    	s_PrintStr(m_origDbs, line_width, out);
    }
    else {
    	out << m_origDbs << endl;
    }

    out << "Database(s) Full Path: ";
    if(m_allDbs.size() > line_width) {
    	out << endl;
    	s_PrintStr(m_allDbs, line_width, out);
    }
    else {
    	out << m_allDbs << endl;
    }
    out << "\t" << NStr::ULongToString(num_seqs, kFlags) << " sequences; ";
    out << NStr::ULongToString(length, kFlags)  << " total " << kLetters << " (unclipped)" << endl;
    out << "\tLongest sequence: "  << NStr::ULongToString(max_seq_length, kFlags) << " " << kLetters << endl;
    out << "\tAverage sequence: "  << NStr::ULongToString(av_seq_length, kFlags) << " " << kLetters << endl;

    if(csra_dbs != kEmptyStr) {
    	if(not_csra_dbs != kEmptyStr) {
    		out << "CSRA Database(s): ";
    		if(csra_dbs.size() > line_width) {
    		   	out << endl;
    		   	s_PrintStr(csra_dbs, line_width, out);
    		}
    		else {
    		   	out << csra_dbs << endl;
    		}
    	}
    	out << "\t" << NStr::ULongToString(ref_num_seqs, kFlags) << " ref sequences; ";
    	out << NStr::ULongToString(ref_length, kFlags)  << " total ref " << kLetters << endl;
    }

    cout << "PERF: Get all BLASTDB metadata: " << x_FormatRuntime(sw) << endl;
    return 0;
}

int
CBlastVdbCmdApp::x_PrintVDBPaths(bool recursive)
{
    const CArgs& args = GetArgs();
    CNcbiOstream& out = args["out"].AsOutputFile();
    vector<string> vdbs;
    vector<string> vdb_alias;
    vector<string> db_alias;

    CStopWatch sw;
    sw.Start();
    CVDBAliasUtil::FindVDBPaths(m_origDbs, false, vdbs, &db_alias, &vdb_alias, recursive, true);
    sw.Stop();

    // Print basic database information
    out << "VDB(s): ";
    if(vdbs.empty()) {
    	out << "None" << endl;
    }
    else {
    	out << endl;
    	ITERATE(vector<string>, itr, vdbs)
    		out << *itr << endl;
   	}

    if(recursive) {
    	out << "VDB Alias File(s): ";
    	if(vdb_alias.empty()) {
       		out << "None" << endl;
    	}
    	else {
       		out << endl;
       		ITERATE(vector<string>, itr, vdb_alias)
     		out << *itr << endl;
    	}

    	out << "Blats DB Alias File(s): ";
    	if(db_alias.empty()) {
       		out << "None" << endl;
    	}
    	else {
       		out << endl;
       		ITERATE(vector<string>, itr, db_alias)
       			out << *itr << endl;
    	}
    }
    cout << "PERF: Get Paths : " << x_FormatRuntime(sw) << endl;
    return 0;
}

void CBlastVdbCmdApp::Init()
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "BLAST-VDB Cmd");

    // SRA-related parameters
    arg_desc->SetCurrentGroup("VDB 'BLASTDB' options");
    arg_desc->AddKey("db", "VDB_ACCESSIONS",
                     "List of whitespace-separated VDB accessions",
                     CArgDescriptions::eString);
    arg_desc->AddKey("dbs_file", "Input_File_with_VDB_ACCESSIONS",
                     "File with a newline delimited list of VDB Run accessions",
                     CArgDescriptions::eInputFile);
    arg_desc->SetDependency("db", CArgDescriptions::eExcludes, "dbs_file");
    arg_desc->AddDefaultKey("outfmt", "output_format", 
                            "Runtime output format (e.g.: smart, seconds)",
                            CArgDescriptions::eString, "smart");
    arg_desc->SetConstraint("outfmt", &(*new CArgAllow_Strings, "smart", "seconds"));

    arg_desc->SetCurrentGroup("Retrieval options");
    arg_desc->AddOptionalKey("entry", "sequence_identifier",
                             "Comma-delimited search string(s) of sequence identifiers"
                             ":\n\te.g.: 'gnl|SRR|SRR066117.18823.2', or 'all' "
                             "to select all\n\tsequences in the database",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("entry_batch", "input_file", 
                             "Input file for batch processing (Format: one entry per line)",
                             CArgDescriptions::eInputFile);
    arg_desc->SetDependency("entry_batch", CArgDescriptions::eExcludes, "entry");
    arg_desc->AddDefaultKey("line_length", "number", "Line length for output",
                            CArgDescriptions::eInteger,
                            NStr::IntToString(80));
    arg_desc->SetConstraint("line_length", 
                            new CArgAllowValuesGreaterThanOrEqual(1));

    const char* exclusions[]  = { "entry", "entry_batch"};
    for (size_t i = 0; i < sizeof(exclusions)/sizeof(*exclusions); i++) {
        arg_desc->SetDependency(exclusions[i], CArgDescriptions::eExcludes, "info");
    }

    arg_desc->AddFlag("info", "Print VDB information", true);
    arg_desc->AddFlag("scan_uncompressed",
                      "Do a full database scan of clipped uncompressed sequence data", true);
    arg_desc->AddFlag("scan_compressed",
                      "Do a full database scan of clipped compressed sequence data", true);
    arg_desc->AddFlag("ref",
                      "Scan or dump reference seqs", true);

    arg_desc->SetDependency("scan_compressed", CArgDescriptions::eExcludes,
                            "scan_uncompressed");
    arg_desc->SetDependency("scan_compressed", CArgDescriptions::eExcludes, "info");
    arg_desc->SetDependency("scan_uncompressed", CArgDescriptions::eExcludes, "info");
    arg_desc->SetDependency("ref", CArgDescriptions::eExcludes, "info");
    arg_desc->SetDependency("ref", CArgDescriptions::eExcludes, "entry_batch");

    arg_desc->AddFlag("paths", "Get top level paths", true);
    arg_desc->AddFlag("paths_all", "Get all vdb and alias paths", true);
    const char* exclude_paths[]  = { "scan_uncompressed", "scan_compressed", "info", "entry", "entry_batch"};
    for (size_t i = 0; i < sizeof(exclude_paths)/sizeof(*exclude_paths); i++) {
        arg_desc->SetDependency("paths", CArgDescriptions::eExcludes, exclude_paths[i]);
        arg_desc->SetDependency("paths_all", CArgDescriptions::eExcludes, exclude_paths[i]);
    }
    arg_desc->SetCurrentGroup("Output configuration options");
    arg_desc->AddDefaultKey("out", "output_file", "Output file name", 
                            CArgDescriptions::eOutputFile, "-");

    SetupArgDescriptions(arg_desc.release());
}

int CBlastVdbCmdApp::Run(void)
{
    int status = 0;
    const CArgs& args = GetArgs();

   	x_InitApplicationData();
    try {
       	if(args["paths"].HasValue()) {
        		status = x_PrintVDBPaths(false);
        		return status;
        }
       	if(args["paths_all"].HasValue()) {
        		status = x_PrintVDBPaths(true);
        		return status;
        }

       	x_GetFullPaths();
        if (args["info"]) {
            status = x_PrintBlastDatabaseInformation();
        }
        else {
        	if (args["entry"].HasValue() || args["entry_batch"].HasValue()) {
            	status = x_ProcessSearchRequest();
        	} else if(args["scan_compressed"].HasValue()){
            	status = x_ScanCompressedDatabase();
        	} else if(args["scan_uncompressed"].HasValue()) {
        		status = x_ScanUncompressedDatabase();
        	}
        }
    } catch (const CException& e) {
        LOG_POST(Error << "VDB Blast error: " << e.GetMsg());          \
        status = 1;
    } catch (const exception& e) {
        status = 1;
    } catch (...) {
        cerr << "Unknown exception!" << endl;
        status = 1;
    }
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastVdbCmdApp().AppMain(argc, argv, 0, eDS_Default, "");
}
#endif /* SKIP_DOXYGEN_PROCESSING */
