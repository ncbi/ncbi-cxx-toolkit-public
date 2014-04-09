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
 * Author: Christiam Camacho
 *
 */

/** @file blastdbcmd.cpp
 * Command line tool to examine the contents of BLAST databases. This is the
 * successor to fastacmd from the C toolkit
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/version.hpp>
#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <objtools/blast/blastdb_format/seq_writer.hpp>
#include <objtools/blast/blastdb_format/blastdb_formatter.hpp>
#include <objtools/blast/blastdb_format/blastdb_seqid.hpp>

#include <algo/blast/blastinput/blast_input.hpp>
#include "../blast/blast_app_util.hpp"
#include <iomanip>


#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
#endif

/// The application class
class CBlastDBCmdApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CBlastDBCmdApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();
    
    /// Handle to BLAST database
    CRef<CSeqDBExpert> m_BlastDb;
    /// Is the database protein
    bool m_DbIsProtein;
    /// Sequence range, non-empty if provided as command line argument
    TSeqRange m_SeqRange;
    /// Strand to retrieve
    ENa_strand m_Strand;
    /// output is FASTA
    bool m_FASTA;
    /// output is ASN.1 defline
    bool m_Asn1DeflineOutput;
    /// should we find duplicate entries? 
    bool m_GetDuplicates;
    /// should we output target sequence only?
    bool m_TargetOnly;

    /// Initializes the application's data members
    void x_InitApplicationData();

    /// Prints the BLAST database information (e.g.: handles -info command line
    /// option)
    void x_PrintBlastDatabaseInformation();

    /// Processes all requests except printing the BLAST database information
    /// @return 0 on success; 1 if some sequences were not retrieved
    int x_ProcessSearchRequest();

    /// Vector of sequence identifiers in a BLAST database
    typedef vector< CRef<CBlastDBSeqId> > TQueries;

    /// Extract the queries for the BLAST database from the command line
    /// options
    /// @param queries queries to retrieve [in|out]
    /// @return 0 on sucess; 1 if some queries were not processed
    int x_GetQueries(TQueries& queries) const;

    /// Add a query ID for processing
    /// @param retval the return value where the queries will be added [in|out]
    /// @param entry the user's query [in]
    /// @return 0 on sucess; 1 if some seqid is not translated
    int x_AddSeqId(CBlastDBCmdApp::TQueries& retval, const string& entry) const;

    /// Add an OID for processing
    /// @param retval the return value where the queries will be added [in|out]
    /// @param entry the user's query [in]
    void x_AddOid(CBlastDBCmdApp::TQueries& retval, const int oid, bool check=false) const;

    /// Process batch entry with range, strand and filter id
    /// @param args program input args
    /// @param seq_fmt sequence formatter object
    /// @return 0 on sucess; 1 if some queries were not processed
    int x_ProcessBatchEntry(const CArgs& args, CSeqFormatter & seq_fmt);
};

int
CBlastDBCmdApp::x_AddSeqId(CBlastDBCmdApp::TQueries& retval, 
                           const string& entry) const
{
    // Process get dups
    if (m_GetDuplicates) {
        vector<int> oids;
        m_BlastDb->AccessionToOids(entry, oids);
        ITERATE(vector<int>, oid, oids) {
            x_AddOid(retval, *oid, true);
        }
        return 0;
    } 

    // FASTA / target_only just need one id
    if (m_TargetOnly) {
        retval.push_back(CRef<CBlastDBSeqId>(new CBlastDBSeqId(entry)));
        return 0;
    } 

    // Default: find oid first and add all pertinent
    vector<int> oids;
    m_BlastDb->AccessionToOids(entry, oids);
    if (!oids.empty()) {
        x_AddOid(retval, oids[0], true);
        return 0;
    } 

    ERR_POST(Error << entry << ": OID not found");
    return 1;
}

void 
CBlastDBCmdApp::x_AddOid(CBlastDBCmdApp::TQueries& retval,
                         const int oid,
                         bool check) const
{
    // check to see if this oid has been excluded
    if (check) {
        list< CRef<CSeq_id> > filtered_ids = m_BlastDb->GetSeqIDs(oid);
        if (filtered_ids.empty()) {
            return;
        } 
    } 

    // FASTA output just need one id
    if (m_FASTA || m_Asn1DeflineOutput) {
        CRef<CBlastDBSeqId> blastdb_seqid(new CBlastDBSeqId());
        blastdb_seqid->SetOID(oid);
        retval.push_back(blastdb_seqid);
        return;
    } 

    // Not a NR database, add oid instead
    vector<int> gis;
    m_BlastDb->GetGis(oid, gis);
    if (gis.empty()) {
        CRef<CBlastDBSeqId> blastdb_seqid(new CBlastDBSeqId());
        blastdb_seqid->SetOID(oid);
        retval.push_back(blastdb_seqid);
        return;
    }

    // Default:  add all possible ids
    ITERATE(vector<int>, gi, gis) {
        retval.push_back(CRef<CBlastDBSeqId>
                         (new CBlastDBSeqId(NStr::IntToString(*gi))));
    }
}

int
CBlastDBCmdApp::x_GetQueries(CBlastDBCmdApp::TQueries& retval) const
{
    int err_found = 0;
    const CArgs& args = GetArgs();

    retval.clear();

    _ASSERT(m_BlastDb.NotEmpty());

    if (args["pig"].HasValue()) {
        retval.reserve(1);
        retval.push_back(CRef<CBlastDBSeqId>
                         (new CBlastDBSeqId(args["pig"].AsInteger())));

    } else if (args["entry"].HasValue()) {

        static const string kDelim(",");
        const string& entry = args["entry"].AsString();

        if (entry.find(kDelim[0]) != string::npos) {
            vector<string> tokens;
            NStr::Tokenize(entry, kDelim, tokens);
            ITERATE(vector<string>, itr, tokens) {
                err_found += x_AddSeqId(retval, *itr);
            }
        } else if (entry == "all") {
            for (int i = 0; m_BlastDb->CheckOrFindOID(i); i++) {
                x_AddOid(retval, i);
            }
        } else {
            err_found += x_AddSeqId(retval, entry);
        }

    } else {
        NCBI_THROW(CInputException, eInvalidInput, 
                   "Must specify query type: one of 'entry', 'entry_batch', or 'pig'");
    }

    if (retval.empty()) {
        NCBI_THROW(CInputException, eInvalidInput,
                   "Entry not found in BLAST database");
    }

    return (err_found) ? 1 : 0;
}

void
CBlastDBCmdApp::x_InitApplicationData()
{
    const CArgs& args = GetArgs();

    CSeqDB::ESeqType seqtype = ParseMoleculeTypeString(args[kArgDbType].AsString());
    m_BlastDb.Reset(new CSeqDBExpert(args[kArgDb].AsString(), seqtype));

    m_DbIsProtein = static_cast<bool>(m_BlastDb->GetSequenceType() == CSeqDB::eProtein);

    m_SeqRange = TSeqRange::GetEmpty();
    if (args["range"].HasValue()) {
        m_SeqRange = ParseSequenceRangeOpenEnd(args["range"].AsString());
    }

    m_Strand = eNa_strand_unknown;
    if (args["strand"].HasValue() && !m_DbIsProtein) {
        if (args["strand"].AsString() == "plus") {
            m_Strand = eNa_strand_plus;
        } else if (args["strand"].AsString() == "minus") {
            m_Strand = eNa_strand_minus;
        } else {
            abort();    // both strands not supported
        }
    } 

    m_GetDuplicates = args["get_dups"];

    m_TargetOnly = args["target_only"];
}

void
CBlastDBCmdApp::x_PrintBlastDatabaseInformation()
{
    _ASSERT(m_BlastDb.NotEmpty());
    static const NStr::TNumToStringFlags kFlags = NStr::fWithCommas;
    const string kLetters = m_DbIsProtein ? "residues" : "bases";
    const CArgs& args = GetArgs();

    CNcbiOstream& out = args["out"].AsOutputFile();

    // Print basic database information
    out << "Database: " << m_BlastDb->GetTitle() << endl
        << "\t" << NStr::IntToString(m_BlastDb->GetNumSeqs(), kFlags) 
        << " sequences; "
        << NStr::UInt8ToString(m_BlastDb->GetExactTotalLength(), kFlags)
        << " total " << kLetters << endl << endl
        << "Date: " << m_BlastDb->GetDate() 
        << "\tLongest sequence: " 
        << NStr::IntToString(m_BlastDb->GetMaxLength(), kFlags) << " " 
        << kLetters << endl;

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    // Print filtering algorithms supported
    out << m_BlastDb->GetAvailableMaskAlgorithmDescriptions();
#endif

    // Print volume names
    vector<string> volumes;
    m_BlastDb->FindVolumePaths(volumes,false);
    out << endl << "Volumes:" << endl;
    ITERATE(vector<string>, file_name, volumes) {
        out << "\t" << *file_name << endl;
    }
}

int
CBlastDBCmdApp::x_ProcessSearchRequest()
{
    const CArgs& args = GetArgs();
    CNcbiOstream& out = args["out"].AsOutputFile();

    CSeqFormatterConfig conf;
    conf.m_LineWidth = args["line_length"].AsInteger();
    conf.m_SeqRange = m_SeqRange;
    conf.m_Strand = m_Strand;
    conf.m_TargetOnly = m_TargetOnly;
    conf.m_UseCtrlA = args["ctrl_a"];
    if (args["mask_sequence_with"].HasValue()) {
    	 conf.m_FiltAlgoId = -1;
    	 conf.m_FiltAlgoId = NStr::StringToInt(args["mask_sequence_with"].AsString(), NStr::fConvErr_NoThrow);
    	 if(errno)
    		 conf.m_FiltAlgoId = m_BlastDb->GetMaskAlgorithmId(args["mask_sequence_with"].AsString());
    }

    string outfmt;
    if (args["outfmt"].HasValue()) {
        outfmt = args["outfmt"].AsString();
        m_FASTA = false;
        m_Asn1DeflineOutput = false;

        if (outfmt.find("%f") != string::npos && 
            outfmt.find("%d") != string::npos) {
            NCBI_THROW(CInputException, eInvalidInput, 
                "The %d and %f output format options cannot be specified together.");
        }

        // If "%f" is found within outfmt, discard everything else
        if (outfmt.find("%f") != string::npos) {
            outfmt = "%f";
            m_FASTA = true;
        }
        // If "%d" is found within outfmt, discard everything else
        if (outfmt.find("%d") != string::npos) {
            outfmt = "%d";
            m_Asn1DeflineOutput = true;
        }
        if (outfmt.find("%m") != string::npos) {
            int algo_id = 0;
            size_t i = outfmt.find("%m") + 2;
            bool found = false;
            while (i < outfmt.size() 
                && outfmt[i] >= '0' && outfmt[i] <= '9') {
                algo_id = algo_id * 10 + (outfmt[i] - '0');
                outfmt.erase(i, 1);
                found = true;
            }
            if (!found) {
                NCBI_THROW(CInputException, eInvalidInput, 
                    "The option '-outfmt %m' is not followed by a masking algo ID.");
            }
            conf.m_FmtAlgoId = algo_id;
        }
    }

    bool errors_found = false;
    CSeqFormatter seq_fmt(outfmt, *m_BlastDb, out, conf);

    /* Special case: full db dump when no range and mask data is specified */
    if (m_FASTA && 
        args["entry"].HasValue() && args["entry"].AsString() == "all" &&
        ! args["mask_sequence_with"].HasValue() &&
        ! args["range"].HasValue()) {

        try {
            seq_fmt.DumpAll(*m_BlastDb, conf);
        } catch (const CException& e) {
            ERR_POST(Error << e.GetMsg());
            errors_found = true;
        } catch (...) {
            ERR_POST(Error << "Failed to retrieve requested item");
            errors_found = true;
        }
        return errors_found ? 1 : 0;
    }

    if (args["entry_batch"].HasValue()) {
       	return x_ProcessBatchEntry(args, seq_fmt);
    }

    TQueries queries;
	errors_found = (x_GetQueries(queries) > 0 ? true : false);
    _ASSERT( !queries.empty() );

    NON_CONST_ITERATE(TQueries, itr, queries) {
        try { 
            seq_fmt.Write(**itr); 
        } catch (const CException& e) {
            ERR_POST(Error << e.GetMsg());
            errors_found = true;
        } catch (...) {
            ERR_POST(Error << "Failed to retrieve requested item");
            errors_found = true;
        }
    }
    return errors_found ? 1 : 0;
}

int CBlastDBCmdApp::x_ProcessBatchEntry(const CArgs& args, CSeqFormatter & seq_fmt)
{
    CNcbiIstream& input = args["entry_batch"].AsInputFile();
    bool err_found = false;
    while (input) {
        string line;
        NcbiGetlineEOL(input, line);
        if ( !line.empty() ) {
        	vector<string> tmp;
        	NStr::Tokenize(line, " \t", tmp, NStr::fSplit_MergeDelims);
        	if(tmp.empty())
        		continue;

        	TQueries queries;
        	if(x_AddSeqId(queries, tmp[0]) > 0 )
        			err_found = true;

            if(queries.empty())
            	continue;

           	TSeqRange seq_range(TSeqRange::GetEmpty());
            ENa_strand seq_strand = eNa_strand_plus;
           	int seq_algo_id = -1;

           	for(unsigned int i=1; i < tmp.size(); i++) {

           		if(tmp[i].find('-')!= string::npos)
            	{
            		try {
            			seq_range = ParseSequenceRangeOpenEnd(tmp[i]);
            		} catch (...) {
            			seq_range = TSeqRange::GetEmpty();
            		}
            	}
            	else if (!m_DbIsProtein && NStr::EqualNocase(tmp[i].c_str(), "minus")) {
            		seq_strand = eNa_strand_minus;
            	}
            	else {
            		seq_algo_id = NStr::StringToNonNegativeInt(tmp[i]);
            	}
            }

           	seq_fmt.SetConfig(seq_range, seq_strand, seq_algo_id);
            NON_CONST_ITERATE(TQueries, itr, queries) {
            	try {
            		seq_fmt.Write(**itr);
            	} catch (const CException& e) {
                     ERR_POST(Error << e.GetMsg());
                     err_found = true;
            	} catch (...) {
                  	ERR_POST(Error << "Failed to retrieve requested item");
                   	err_found = true;
            	}
            }
        }
    }
    return err_found ? 1:0;
}

void CBlastDBCmdApp::Init()
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                  "BLAST database client, version " + CBlastVersion().Print());

    arg_desc->SetCurrentGroup("BLAST database options");
    arg_desc->AddDefaultKey(kArgDb, "dbname", "BLAST database name", 
                            CArgDescriptions::eString, "nr");

    arg_desc->AddDefaultKey(kArgDbType, "molecule_type",
                            "Molecule type stored in BLAST database",
                            CArgDescriptions::eString, "guess");
    arg_desc->SetConstraint(kArgDbType, &(*new CArgAllow_Strings,
                                        "nucl", "prot", "guess"));

    arg_desc->SetCurrentGroup("Retrieval options");
    arg_desc->AddOptionalKey("entry", "sequence_identifier",
                     "Comma-delimited search string(s) of sequence identifiers"
                     ":\n\te.g.: 555, AC147927, 'gnl|dbname|tag', or 'all' "
                     "to select all\n\tsequences in the database",
                     CArgDescriptions::eString);

    arg_desc->AddOptionalKey("entry_batch", "input_file", 
                 "Input file for batch processing (Format: one entry per line, seq id \n"
		 "followed by optional space-delimited specifier(s) [range|strand|mask_algo_id]",
                 CArgDescriptions::eInputFile);
    arg_desc->SetDependency("entry_batch", CArgDescriptions::eExcludes, "entry");
    arg_desc->SetDependency("entry_batch", CArgDescriptions::eExcludes, "range");
    arg_desc->SetDependency("entry_batch", CArgDescriptions::eExcludes, "strand");
    arg_desc->SetDependency("entry_batch", CArgDescriptions::eExcludes, "mask_sequence_with");

    arg_desc->AddOptionalKey("pig", "PIG", "PIG to retrieve", 
                             CArgDescriptions::eInteger);
    arg_desc->SetConstraint("pig", new CArgAllowValuesGreaterThanOrEqual(0));
    arg_desc->SetDependency("pig", CArgDescriptions::eExcludes, "entry");
    arg_desc->SetDependency("pig", CArgDescriptions::eExcludes, "entry_batch");
    arg_desc->SetDependency("pig", CArgDescriptions::eExcludes, "target_only");

    arg_desc->AddFlag("info", "Print BLAST database information", true);
    // All other options to this program should be here
    const char* exclusions[]  = { "entry", "entry_batch", "outfmt", "strand",
        "target_only", "ctrl_a", "get_dups", "pig", "range",
        "mask_sequence", "list", "remove_redundant_dbs", "recursive",
        "list_outfmt" };
    for (size_t i = 0; i < sizeof(exclusions)/sizeof(*exclusions); i++) {
        arg_desc->SetDependency("info", CArgDescriptions::eExcludes,
                                string(exclusions[i]));
    }

    arg_desc->SetCurrentGroup("Sequence retrieval configuration options");
    arg_desc->AddOptionalKey("range", "numbers",
                         "Range of sequence to extract in 1-based offsets "
                         "(Format: start-stop, for start to end of sequence use start - )",
                         CArgDescriptions::eString);

    arg_desc->AddDefaultKey("strand", "strand",
                            "Strand of nucleotide sequence to extract",
                            CArgDescriptions::eString, "plus");
    arg_desc->SetConstraint("strand", &(*new CArgAllow_Strings, "minus",
                                        "plus"));

    arg_desc->AddOptionalKey("mask_sequence_with", "mask_algo_id",
                             "Produce lower-case masked FASTA using the "
                             "algorithm ID specified", 
                             CArgDescriptions::eString);

    arg_desc->SetCurrentGroup("Output configuration options");
    arg_desc->AddDefaultKey(kArgOutput, "output_file", "Output file name", 
                            CArgDescriptions::eOutputFile, "-");

    // The format specifiers below should be handled in
    // CSeqFormatter::x_Builder
    arg_desc->AddDefaultKey("outfmt", "format",
            "Output format, where the available format specifiers are:\n"
            "\t\t%f means sequence in FASTA format\n"
            "\t\t%s means sequence data (without defline)\n"
            "\t\t%a means accession\n"
            "\t\t%g means gi\n"
            "\t\t%o means ordinal id (OID)\n"
            "\t\t%i means sequence id\n"
            "\t\t%t means sequence title\n"
            "\t\t%l means sequence length\n"
            "\t\t%h means sequence hash value\n"
            "\t\t%T means taxid\n"
            "\t\t%e means membership integer\n"
            "\t\t%L means common taxonomic name\n"
            "\t\t%S means scientific name\n"
#if _BLAST_DEBUG
            "\t\t%B means BLAST name\n"     /* Is this useful outside NCBI? */
            "\t\t%n means a list of links integers separated by ';'\n"
#endif /* _BLAST_DEBUG */
            "\t\t%K means taxonomic super kingdom\n"
            "\t\t%P means PIG\n"
#if _BLAST_DEBUG
            "\t\t%d means defline in text ASN.1 format\n"
            "\t\t%b means Bioseq in text ASN.1 format\n"
#endif /* _BLAST_DEBUG */
            "\t\t%m means sequence masking data.\n"
            "\t\t   Masking data will be displayed as a series of 'N-M' values\n"
            "\t\t   separated by ';' or the word 'none' if none are available.\n"
#if _BLAST_DEBUG
            "\tIf '%f' or '%d' are specified, all other format specifiers are ignored.\n"
            "\tFor every format except '%f' and '%d', each line of output will "
#else
            "\tIf '%f' is specified, all other format specifiers are ignored.\n"
            "\tFor every format except '%f', each line of output will "
#endif /* _BLAST_DEBUG */
            "correspond\n\tto a sequence.\n",
            CArgDescriptions::eString, "%f");

    //arg_desc->AddDefaultKey("target_only", "value",
    //                        "Definition line should contain target gi only",
    //                        CArgDescriptions::eBoolean, "false");
    arg_desc->AddFlag("target_only", 
                      "Definition line should contain target entry only", true);
    
    //arg_desc->AddDefaultKey("get_dups", "value",
    //                        "Retrieve duplicate accessions",
    //                        CArgDescriptions::eBoolean, "false");
    arg_desc->AddFlag("get_dups", "Retrieve duplicate accessions", true);
    arg_desc->SetDependency("get_dups", CArgDescriptions::eExcludes, 
                            "target_only");

    arg_desc->SetCurrentGroup("Output configuration options for FASTA format");
    arg_desc->AddDefaultKey("line_length", "number", "Line length for output",
                        CArgDescriptions::eInteger, 
                        NStr::IntToString(CSeqFormatterConfig().m_LineWidth));
    arg_desc->SetConstraint("line_length", 
                            new CArgAllowValuesGreaterThanOrEqual(1));

    arg_desc->AddFlag("ctrl_a", 
                      "Use Ctrl-A as the non-redundant defline separator",true);

    const char* exclusions_discovery[]  = { "entry", "entry_batch", "outfmt",
        "strand", "target_only", "ctrl_a", "get_dups", "pig", "range", kArgDb.c_str(),
        "info", "mask_sequence", "line_length" };
    arg_desc->SetCurrentGroup("BLAST database configuration and discovery options");
    arg_desc->AddFlag("show_blastdb_search_path", 
                      "Displays the default BLAST database search paths", true);
    arg_desc->AddOptionalKey("list", "directory",
                             "List BLAST databases in the specified directory",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("remove_redundant_dbs", 
                      "Remove the databases that are referenced by another "
                      "alias file in the directory in question", true);
    arg_desc->AddFlag("recursive", 
                      "Recursively traverse the directory structure to list "
                      "available BLAST databases", true);
    arg_desc->AddDefaultKey("list_outfmt", "format",
            "Output format for the list option, where the available format specifiers are:\n"
            "\t\t%f means the BLAST database absolute file name path\n"
            "\t\t%p means the BLAST database molecule type\n"
            "\t\t%t means the BLAST database title\n"
            "\t\t%d means the date of last update of the BLAST database\n"
            "\t\t%l means the number of bases/residues in the BLAST database\n"
            "\t\t%n means the number of sequences in the BLAST database\n"
            "\t\t%U means the number of bytes used by the BLAST database\n"
            "\tFor every format each line of output will "
            "correspond to a BLAST database.\n",
            CArgDescriptions::eString, "%f %p");
    for (size_t i = 0; i <
         sizeof(exclusions_discovery)/sizeof(*exclusions_discovery); i++) {
        arg_desc->SetDependency("list", CArgDescriptions::eExcludes,
                                string(exclusions_discovery[i]));
        arg_desc->SetDependency("recursive", CArgDescriptions::eExcludes,
                                string(exclusions_discovery[i]));
        arg_desc->SetDependency("remove_redundant_dbs", CArgDescriptions::eExcludes,
                                string(exclusions_discovery[i]));
        arg_desc->SetDependency("list_outfmt", CArgDescriptions::eExcludes,
                                string(exclusions_discovery[i]));
        arg_desc->SetDependency("show_blastdb_search_path", CArgDescriptions::eExcludes,
                                string(exclusions_discovery[i]));
    }
    arg_desc->SetDependency("show_blastdb_search_path", CArgDescriptions::eExcludes,
                            "list");
    arg_desc->SetDependency("show_blastdb_search_path", CArgDescriptions::eExcludes,
                            "recursive");
    arg_desc->SetDependency("show_blastdb_search_path", CArgDescriptions::eExcludes,
                            "list_outfmt");
    arg_desc->SetDependency("show_blastdb_search_path", CArgDescriptions::eExcludes,
                            "remove_redundant_dbs");

    SetupArgDescriptions(arg_desc.release());
}

int CBlastDBCmdApp::Run(void)
{
    int status = 0;
    const CArgs& args = GetArgs();

    // Silences warning in CSeq_id for CSeq_id::fParse_PartialOK
    SetDiagFilter(eDiagFilter_Post, "!(1306.10)");

    try {
        CNcbiOstream& out = args["out"].AsOutputFile();
        if (args["show_blastdb_search_path"]) {
            out << CSeqDB::GenerateSearchPath() << NcbiEndl;
            return status;
        } else if (args["list"]) {
            const string& blastdb_dir = args["list"].AsString();
            const bool recurse = args["recursive"];
            const bool remove_redundant_dbs = args["remove_redundant_dbs"];
            const string dbtype = args[kArgDbType] 
                ? args[kArgDbType].AsString() 
                : "guess";
            const string& kOutFmt = args["list_outfmt"].AsString();
            const vector<SSeqDBInitInfo> dbs = 
                FindBlastDBs(blastdb_dir, dbtype, recurse, true,
                             remove_redundant_dbs);
            CBlastDbFormatter blastdb_fmt(kOutFmt);
            ITERATE(vector<SSeqDBInitInfo>, db, dbs) {
                out << blastdb_fmt.Write(*db) << NcbiEndl;
            }
            return status;
        }
        
        x_InitApplicationData();

        if (args["info"]) {
            x_PrintBlastDatabaseInformation();
        } else {
            status = x_ProcessSearchRequest();
        }

    } CATCH_ALL(status)
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastDBCmdApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
