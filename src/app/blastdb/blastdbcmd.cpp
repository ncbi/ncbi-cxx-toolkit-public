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
#include <objtools/readers/seqdb/seqdbexpert.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
#endif

class CBlastDBCmdApp : public CNcbiApplication
{
public:
    CBlastDBCmdApp() {
        SetVersion(blast::Version);
    }
private:
    virtual void Init();
    virtual int Run();
    
    CRef<CSeqDBExpert> m_BlastDb;
    bool m_DbIsProtein;

    void x_InitializeBlastDbHandle();
    void x_PrintBlastDatabaseInformation();
    void x_ProcessSearchRequest() const;
};

/// Class to constrain the values of an argument to those greater than or equal
/// to the value specified in the constructor
class CArgAllowValuesGreaterThanOrEqual : public CArgAllow
{
public:
    /// Constructor taking an integer
    CArgAllowValuesGreaterThanOrEqual(int min) : m_MinValue(min) {}
    /// Constructor taking a double
    CArgAllowValuesGreaterThanOrEqual(double min) : m_MinValue(min) {}

protected:
    /// Overloaded method from CArgAllow
    virtual bool Verify(const string& value) const {
        return NStr::StringToDouble(value) >= m_MinValue;
    }

    /// Overloaded method from CArgAllow
    virtual string GetUsage(void) const {
        return ">=" + NStr::DoubleToString(m_MinValue);
    }
    
private:
    double m_MinValue;  /**< Minimum value for this object */
};

void
CBlastDBCmdApp::x_InitializeBlastDbHandle()
{
    const CArgs& args = GetArgs();
    CSeqDB::ESeqType seqtype;
    const string& dbtype = args["dbtype"].AsString();
    if (dbtype == "guess") {
        seqtype = CSeqDB::eUnknown;
    } else if (dbtype == "prot") {
        seqtype = CSeqDB::eProtein;
    } else if (dbtype == "nucl") {
        seqtype = CSeqDB::eNucleotide;
    } else {
        _ASSERT("Unknown molecule for BLAST DB" != 0);
    }
    m_BlastDb.Reset(new CSeqDBExpert(args["db"].AsString(), seqtype));
    m_DbIsProtein = 
        static_cast<bool>(m_BlastDb->GetSequenceType() == CSeqDB::eProtein);
}

// FIXME: this should be moved as a member function of SBlastDbMaskData
static string
s_EMaskingAlgorithmToString(SBlastDbMaskData::EMaskingAlgorithm algorithm)
{
    string retval;
    switch (algorithm) {
    case SBlastDbMaskData::eNotSet: 
        retval.assign("Not set"); 
        break;
    case SBlastDbMaskData::eDUST:
        retval.assign("DUST");
        break;
    case SBlastDbMaskData::eSEG:
        retval.assign("SEG");
        break;
    case SBlastDbMaskData::eRepeat:
        retval.assign("Repeats");
        break;
    case SBlastDbMaskData::eOther:
        retval.assign("Other");
        break;
    default:
        _ASSERT("Unknown masking algorithm" != 0);
    }
    return retval;
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

    // Print filtering algorithms supported
    vector<int> algorithms;
    m_BlastDb->GetAvailableMaskAlgorithms(algorithms);
    if ( !algorithms.empty() ) {
        out << endl
            << "Available filtering algorithms applied to database sequences:"
            << endl;
        int counter = 0;
        ITERATE(vector<int>, algo_id, algorithms) {
            CSeqDB::TMaskingAlgorithm algo;
            string algo_opts;
            m_BlastDb->GetMaskAlgorithmDetails(*algo_id, algo, algo_opts);
            out << "\t" << ++counter << ". " 
                << s_EMaskingAlgorithmToString(algo) << " (options: '" 
                << algo_opts << "')" << endl;
        }
    }

    // Print volume names
    vector<string> volumes;
    m_BlastDb->FindVolumePaths(volumes);
    out << endl << "Volumes:" << endl;
    ITERATE(vector<string>, file_name, volumes) {
        out << "\t" << *file_name << endl;
    }
}

void
CBlastDBCmdApp::x_ProcessSearchRequest() const
{
    //const CArgs& args = GetArgs();
    throw runtime_error("Unimplemented");
}

void CBlastDBCmdApp::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideDryRun);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                  "BLAST database client, version " + blast::Version.Print());

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
                             "Input file with batch input", 
                             CArgDescriptions::eInputFile);
    arg_desc->SetDependency("entry_batch", CArgDescriptions::eExcludes,
                            "entry");

    arg_desc->AddOptionalKey("pig", "PIG", "PIG to retrieve", 
                             CArgDescriptions::eInteger);
    arg_desc->SetConstraint("pig", new CArgAllowValuesGreaterThanOrEqual(0));
    arg_desc->SetDependency("pig", CArgDescriptions::eExcludes, "entry");
    arg_desc->SetDependency("pig", CArgDescriptions::eExcludes, "entry_batch");

    arg_desc->AddFlag("info", "Print BLAST database information", true);
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
            "Output format, available formats are:\n"
            "\t'fasta', 'taxid', 'gi', 'accession', or 'custom'\n"
            "\twhere custom is a quoted string containing the following "
            "directives:\n"
            "\t\t%s means sequence id (identifier + title)\n"
            "\t\t%a means accession\n"
            "\t\t%g means gi\n"
            "\t\t%t means sequence title\n"
            "\t\t%f means sequence data in FASTA\n"
            "\t\t%l means sequence length\n"
            "\t\t%T means taxid\n"
            "\t\t%P means PIG\n",
            CArgDescriptions::eString, "fasta");

    arg_desc->AddDefaultKey("line_length", "number",
                            "Line length for output",
                            CArgDescriptions::eInteger, "80");
    arg_desc->SetConstraint("line_length", 
                            new CArgAllowValuesGreaterThanOrEqual(1));

    arg_desc->AddDefaultKey("ctrl_a", "value",
                            "Use Ctrl-A's as non-redundant defline separator",
                            CArgDescriptions::eBoolean, "false");

    arg_desc->AddDefaultKey("target_only", "value",
                            "Definition line should contain target gi only",
                            CArgDescriptions::eBoolean, "false");
    
    arg_desc->AddDefaultKey("get_dups", "value",
                            "Retrieve duplicate accessions",
                            CArgDescriptions::eBoolean, "false");

    SetupArgDescriptions(arg_desc.release());
}

int CBlastDBCmdApp::Run(void)
{
    int status = 0;
    const CArgs& args = GetArgs();

    try {
        x_InitializeBlastDbHandle();

        if (args["info"]) {
            x_PrintBlastDatabaseInformation();
        } else {
            x_ProcessSearchRequest();
        }

    } catch (const CException& exptn) {
        cerr << exptn.what() << endl;
        status = exptn.GetErrCode();
    } catch (const exception& e) {
        cerr << e.what() << endl;
        status = -1;
    } catch (...) {
        cerr << "Unknown exception" << endl;
        status = -1;
    }
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastDBCmdApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
