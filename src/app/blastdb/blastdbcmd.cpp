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
#include <objtools/blast_format/seq_writer.hpp>
#include <objtools/blast_format/blastdb_seqid.hpp>
#include "blastdb_aux.hpp"

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
    void x_GetQueries(TQueries& queries) const;

    /// Process a single query and add it to the return value
    /// @param retval the return value where the queries will be added [in|out]
    /// @param entry the user's query [in]
    void x_ProcessSingleQuery(CBlastDBCmdApp::TQueries& retval, 
                              const string& entry) const;
};

void
CBlastDBCmdApp::x_ProcessSingleQuery(CBlastDBCmdApp::TQueries& retval, 
                                     const string& entry) const
{
    const CArgs& args = GetArgs();
    const bool kGetDuplicates = args["get_dups"];
    const bool kTargetGi = args["target_only"];

    CRef<CBlastDBSeqId> blastdb_seqid;
    if (kGetDuplicates) {
        _ASSERT(kTargetGi == false);
        vector<int> oids;
        m_BlastDb->AccessionToOids(entry, oids);
        ITERATE(vector<int>, oid, oids) {
            blastdb_seqid.Reset(new CBlastDBSeqId());
            blastdb_seqid->SetOID(*oid);
            retval.push_back(blastdb_seqid);
        }
    } else {
        blastdb_seqid.Reset(new CBlastDBSeqId(entry));
        if (kTargetGi && !blastdb_seqid->IsGi()) {
            ERR_POST(Warning << "Skipping " << blastdb_seqid->AsString() 
                     << " as it is not a GI and target_only is being used");
            return;
        }
        retval.push_back(blastdb_seqid);
    }
}

void
CBlastDBCmdApp::x_GetQueries(CBlastDBCmdApp::TQueries& retval) const
{
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
            retval.reserve(tokens.size());
            ITERATE(vector<string>, itr, tokens) {
                x_ProcessSingleQuery(retval, *itr);
            }
        } else if (entry == "all") {
            // dump all OIDs in this database
            CRef<CBlastDBSeqId> blastdb_seqid;
            for (int i = 0; m_BlastDb->CheckOrFindOID(i); i++) {
                blastdb_seqid.Reset(new CBlastDBSeqId());
                blastdb_seqid->SetOID(i);
                retval.push_back(blastdb_seqid);
            }
        } else {
            retval.reserve(1);
            x_ProcessSingleQuery(retval, entry);
        }

    } else if (args["entry_batch"].HasValue()) {

        CNcbiIstream& input = args["entry_batch"].AsInputFile();
        retval.reserve(256); // arbitrary value
        while (input) {
            string line;
            NcbiGetlineEOL(input, line);
            if ( !line.empty() ) {
                x_ProcessSingleQuery(retval, line);
            }
        }

    } else {
        NCBI_THROW(CInputException, eInvalidInput, "Must specify query type");
    }
    if (retval.empty()) {
        NCBI_THROW(CInputException, eInvalidInput, 
                   "No valid entries to search");
    }
}

void
CBlastDBCmdApp::x_InitApplicationData()
{
    const CArgs& args = GetArgs();
    CSeqDB::ESeqType seqtype = ParseTypeString(args["dbtype"].AsString());;
    m_BlastDb.Reset(new CSeqDBExpert(args["db"].AsString(), seqtype));
    m_DbIsProtein = 
        static_cast<bool>(m_BlastDb->GetSequenceType() == CSeqDB::eProtein);

    m_SeqRange = TSeqRange::GetEmpty();
    if (args["range"].HasValue()) {
        m_SeqRange = ParseSequenceRange(args["range"].AsString());
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
        << NStr::UInt8ToString(m_BlastDb->GetTotalLength(), kFlags)
        << " total " << kLetters << endl << endl
        << "Date: " << m_BlastDb->GetDate() 
        << "\tLongest sequence: " 
        << NStr::IntToString(m_BlastDb->GetMaxLength(), kFlags) << " " 
        << kLetters << endl;

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    // Print filtering algorithms supported
    vector<int> algorithms;
    m_BlastDb->GetAvailableMaskAlgorithms(algorithms);
    if ( !algorithms.empty() ) {
        out << endl
            << "Available filtering algorithms applied to database sequences:"
            << endl << endl;
        
        out << setw(14) << left << "Algorithm ID" 
            << setw(20) << left << "Algorithm name"
            << setw(40) << left << "Algorithm options" << endl;
        ITERATE(vector<int>, algo_id, algorithms) {
            objects::EBlast_filter_program algo;
            string algo_opts, algo_name;
            m_BlastDb->GetMaskAlgorithmDetails(*algo_id, algo, algo_name,
                                               algo_opts);
            out << "    " << setw(10) << left << (*algo_id) 
                << setw(20) << left << algo_name
                << setw(40) << left << algo_opts << endl;
        }
    }
#endif

    // Print volume names
    vector<string> volumes;
    m_BlastDb->FindVolumePaths(volumes);
    out << endl << "Volumes:" << endl;
    ITERATE(vector<string>, file_name, volumes) {
        out << "\t" << *file_name << endl;
    }
}

/// Auxiliary function to pretty-format a CBlastDBSeqId
/// @param id the id to format [in]
/// @param blastdb BLAST database handle [in]
static string s_FormatCBlastDBSeqID(CRef<CBlastDBSeqId> id, CSeqDB& blastdb)
{
    static const string kInvalid(NStr::IntToString(CBlastDBSeqId::kInvalid));
    _ASSERT(id.NotEmpty());
    string retval = id->AsString();
    if (NStr::StartsWith(retval, "OID")) {
        // First try the GI...
        retval = CGiExtractor().Extract(*id, blastdb);
        if (retval != kInvalid) {
            return "GI " + retval;
        }

        // ... then the accession...
        retval = CAccessionExtractor().Extract(*id, blastdb);
        if (retval != kInvalid && !isalnum(retval[0])) {
            // identify as accession if not invalid or just a number (bound to
            // occur when BLAST database wasn't created with parsing of
            // Seq-ids)
            return "Accession " + retval;
        }

        // ... and finally the sequence title.
        retval = CTitleExtractor().Extract(*id, blastdb);
        if (retval != kInvalid) {
            return "Sequence " + retval;
        }
    }
    return retval;
}

int
CBlastDBCmdApp::x_ProcessSearchRequest()
{
    TQueries queries;
    x_GetQueries(queries);
    _ASSERT( !queries.empty() );

    const CArgs& args = GetArgs();
    CNcbiOstream& out = args["out"].AsOutputFile();
    const string kOutFmt = args["outfmt"].AsString();
    CSeqFormatterConfig conf;
    conf.m_LineWidth = args["line_length"].AsInteger();
    conf.m_SeqRange = m_SeqRange;
    conf.m_Strand = m_Strand;
    conf.m_TargetOnly = args["target_only"];
    conf.m_UseCtrlA = args["ctrl_a"];

    bool errors_found = false;
    CSeqFormatter seq_fmt(kOutFmt, *m_BlastDb, out, conf);
    NON_CONST_ITERATE(TQueries, itr, queries) {
        try { 
            seq_fmt.Write(**itr); 
        } catch (const CException& e) {
            ERR_POST(Error << s_FormatCBlastDBSeqID(*itr, *m_BlastDb) 
                     << ": " << e.GetMsg());
            errors_found = true;
        } catch (...) {
            ERR_POST(Error << "Failed to retrieve " << 
                     s_FormatCBlastDBSeqID(*itr, *m_BlastDb));
            errors_found = true;
        }
    }
    return errors_found ? 1 : 0;
}

void CBlastDBCmdApp::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                  "BLAST database client, version " + CBlastVersion().Print());

    arg_desc->SetCurrentGroup("BLAST database options");
    arg_desc->AddDefaultKey("db", "dbname", "BLAST database name", 
                            CArgDescriptions::eString, "nr");

    arg_desc->AddDefaultKey("dbtype", "molecule_type",
                            "Molecule type stored in BLAST database",
                            CArgDescriptions::eString, "guess");
    arg_desc->SetConstraint("dbtype", &(*new CArgAllow_Strings,
                                        "nucl", "prot", "guess"));

    arg_desc->SetCurrentGroup("Retrieval options");
    arg_desc->AddOptionalKey("entry", "sequence_identifier",
                     "Comma-delimited search string(s) of sequence identifiers"
                     ":\n\te.g.: 555, AC147927, 'gnl|dbname|tag', or 'all' "
                     "to select all\n\tsequences in the database",
                     CArgDescriptions::eString);

    arg_desc->AddOptionalKey("entry_batch", "input_file", 
                 "Input file for batch processing (Format: one entry per line)",
                 CArgDescriptions::eInputFile);
    arg_desc->SetDependency("entry_batch", CArgDescriptions::eExcludes,
                            "entry");

    arg_desc->AddOptionalKey("pig", "PIG", "PIG to retrieve", 
                             CArgDescriptions::eInteger);
    arg_desc->SetConstraint("pig", new CArgAllowValuesGreaterThanOrEqual(0));
    arg_desc->SetDependency("pig", CArgDescriptions::eExcludes, "entry");
    arg_desc->SetDependency("pig", CArgDescriptions::eExcludes, "entry_batch");
    arg_desc->SetDependency("pig", CArgDescriptions::eExcludes, "target_only");

    arg_desc->AddFlag("info", "Print BLAST database information", true);
    // All other options to this program should be here
    const char* exclusions[]  = { "entry", "entry_batch", "outfmt", "strand",
        "target_only", "ctrl_a", "get_dups", "pig", "range" };
    for (size_t i = 0; i < sizeof(exclusions)/sizeof(*exclusions); i++) {
        arg_desc->SetDependency("info", CArgDescriptions::eExcludes,
                                string(exclusions[i]));
    }

    arg_desc->SetCurrentGroup("Sequence retrieval configuration options");
    arg_desc->AddOptionalKey("range", "numbers",
                         "Range of sequence to extract (Format: start-stop)",
                         CArgDescriptions::eString);

    arg_desc->AddDefaultKey("strand", "strand",
                            "Strand of nucleotide sequence to extract",
                            CArgDescriptions::eString, "plus");
    arg_desc->SetConstraint("strand", &(*new CArgAllow_Strings, "minus",
                                        "plus"));

    arg_desc->SetCurrentGroup("Output configuration options");
    arg_desc->AddDefaultKey("out", "output_file", "Output file name", 
                            CArgDescriptions::eOutputFile, "-");

    arg_desc->AddDefaultKey("outfmt", "format",
            "Output format, where the available format specifiers are:\n"
            "\t\t%f means sequence in FASTA format\n"
            "\t\t%s means sequence data (without defline)\n"
            "\t\t%a means accession\n"
            "\t\t%g means gi\n"
            "\t\t%o means ordinal id (OID)\n"
            "\t\t%t means sequence title\n"
            "\t\t%l means sequence length\n"
            "\t\t%T means taxid\n"
            "\t\t%L means common taxonomic name\n"
            "\t\t%S means scientific name\n"
            "\t\t%P means PIG\n"
            "\tFor every format except '%f', each line of output will "
            "correspond to\n\ta sequence.\n",
            CArgDescriptions::eString, "%f");

    //arg_desc->AddDefaultKey("target_only", "value",
    //                        "Definition line should contain target gi only",
    //                        CArgDescriptions::eBoolean, "false");
    arg_desc->AddFlag("target_only", 
                      "Definition line should contain target GI only", true);
    
    //arg_desc->AddDefaultKey("get_dups", "value",
    //                        "Retrieve duplicate accessions",
    //                        CArgDescriptions::eBoolean, "false");
    arg_desc->AddFlag("get_dups", "Retrieve duplicate accessions", true);
    arg_desc->SetDependency("get_dups", CArgDescriptions::eExcludes, 
                            "target_only");

    arg_desc->SetCurrentGroup("Output configuration options for FASTA format");
    arg_desc->AddDefaultKey("line_length", "number",
                            "Line length for output",
                            CArgDescriptions::eInteger, "80");
    arg_desc->SetConstraint("line_length", 
                            new CArgAllowValuesGreaterThanOrEqual(1));

    arg_desc->AddFlag("ctrl_a", 
                      "Use Ctrl-A as the non-redundant defline separator",true);

    SetupArgDescriptions(arg_desc.release());
}

int CBlastDBCmdApp::Run(void)
{
    int status = 0;
    const CArgs& args = GetArgs();

    try {
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
