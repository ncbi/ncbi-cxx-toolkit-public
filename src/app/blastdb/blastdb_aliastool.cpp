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

/** @file blastdb_aliastool.cpp
 * Command line tool to create BLAST database aliases and associated files. 
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/version.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objtools/blast/seqdb_writer/writedb.hpp>
#include <objtools/blast/seqdb_writer/writedb_error.hpp>

#include <algo/blast/blastinput/blast_input.hpp>
#include "../blast/blast_app_util.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
#endif

/// The main application class
class CBlastDBAliasApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CBlastDBAliasApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    /// Converts gi files from binary to text format
    /// @param input Input stream with text file [in]
    /// @param output Output stream where converted binary gi list will be
    /// written [out]
    /// @return 0 on success
    int ConvertGiFile(CNcbiIstream& input, CNcbiOstream& output,
                      const string* input_fname = NULL,
                      const string* output_fname = NULL) const;
    /// Invokes function to create an alias file with the arguments provided on
    /// the command line
    void CreateAliasFile() const;

    /// Documentation for this program
    static const char * const DOCUMENTATION;

    /// Describes the modes of operation of this application
    enum EOperationMode {
        eCreateAlias,       ///< Create alias files
        eConvertGiFile      ///< Convert gi files from text to binary format
    };

    /// Determine what mode of operation is being used
    EOperationMode x_GetOperationMode() const {
        EOperationMode retval = eCreateAlias;
        if (GetArgs()["gi_file_in"].HasValue()) {
            retval = eConvertGiFile;
        }
        return retval;
    }
};

const char * const CBlastDBAliasApp::DOCUMENTATION = "\n\n"
"This application has three modes of operation:\n\n"
"1) Gi file conversion:\n"
"   Converts a text file containing GIs (one per line) to a more efficient\n"
"   binary format. This can be provided as an argument to the -gilist option\n"
"   of the BLAST search command line binaries or to the -gilist option of\n"
"   this program to create an alias file for a BLAST database (see below).\n\n"
"2) Alias file creation (restricting with GI List):\n"
"   Creates an alias for a BLAST database and a GI list which restricts this\n"
"   database. This is useful if one often searches a subset of a database\n"
"   (e.g., based on organism or a curated list). The alias file makes the\n"
"   search appear as if one were searching a regular BLAST database rather\n"
"   than the subset of one.\n\n"
"3) Alias file creation (aggregating BLAST databases):\n"
"   Creates an alias for multiple BLAST databases. All databases must be of\n"
"   the same molecule type (no validation is done). The relevant options are\n" 
"   -dblist and -num_volumes.\n";

static const string kOutput("out");

void CBlastDBAliasApp::Init()
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                  "Application to create BLAST database aliases, version " 
                  + CBlastVersion().Print() + DOCUMENTATION);

    string dflt("Default = input file name provided to -gi_file_in argument");
    dflt += " with the .bgl extension";

    const char* exclusions[]  = { kArgDb.c_str(), kArgDbType.c_str(), kArgDbTitle.c_str(), 
		kArgGiList.c_str(), kArgOutput.c_str(), "dblist", "num_volumes" };
    arg_desc->SetCurrentGroup("GI file conversion options");
    arg_desc->AddOptionalKey("gi_file_in", "input_file",
                     "Text file to convert, should contain one GI per line",
                     CArgDescriptions::eInputFile);
    for (size_t i = 0; i < sizeof(exclusions)/sizeof(*exclusions); i++) {
        arg_desc->SetDependency("gi_file_in", CArgDescriptions::eExcludes,
                                string(exclusions[i]));
    }
    arg_desc->AddOptionalKey("gi_file_out", "output_file",
                     "File name of converted GI file\n" + dflt,
                     CArgDescriptions::eOutputFile,
                     CArgDescriptions::fPreOpen | CArgDescriptions::fBinary);
    arg_desc->SetDependency("gi_file_out", CArgDescriptions::eRequires,
                            "gi_file_in");
    for (size_t i = 0; i < sizeof(exclusions)/sizeof(*exclusions); i++) {
        arg_desc->SetDependency("gi_file_out", CArgDescriptions::eExcludes,
                                string(exclusions[i]));
    }

    arg_desc->SetCurrentGroup("Alias file creation options");
    arg_desc->AddOptionalKey(kArgDb, "dbname", "BLAST database name", 
                             CArgDescriptions::eString);
    arg_desc->SetDependency(kArgDb, CArgDescriptions::eRequires, kArgGiList);
    arg_desc->SetDependency(kArgDb, CArgDescriptions::eRequires, kOutput);

    arg_desc->AddDefaultKey(kArgDbType, "molecule_type",
                            "Molecule type stored in BLAST database",
                            CArgDescriptions::eString, "prot");
    arg_desc->SetConstraint(kArgDbType, &(*new CArgAllow_Strings,
                                        "nucl", "prot"));

    arg_desc->AddOptionalKey(kArgDbTitle, "database_title",
                     "Title for BLAST database\n"
                     "Default = name of BLAST database provided to -db"
                     " argument with the -gifile argument appended to it",
                     CArgDescriptions::eString);
    arg_desc->SetDependency(kArgDbTitle, CArgDescriptions::eRequires, kOutput);

    arg_desc->AddOptionalKey(kArgGiList, "input_file", 
                             "Text or binary gi file to restrict the BLAST "
                             "database provided in -db argument\n"
                             "If text format is provided, it will be converted "
                             "to binary",
                             CArgDescriptions::eInputFile);
    arg_desc->SetDependency(kArgGiList, CArgDescriptions::eRequires, kOutput);

    arg_desc->AddOptionalKey(kOutput, "database_name",
                             "Name of BLAST database alias to be created",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("dblist", "database_names", 
                             "A space separated list of BLAST database names to"
                             " aggregate",
                             CArgDescriptions::eString);
    arg_desc->SetDependency("dblist", CArgDescriptions::eExcludes, kArgDb);
    arg_desc->SetDependency("dblist", CArgDescriptions::eExcludes, "num_volumes");
    arg_desc->SetDependency("dblist", CArgDescriptions::eRequires, kOutput);
    arg_desc->SetDependency("dblist", CArgDescriptions::eRequires, kArgDbType);
    arg_desc->SetDependency("dblist", CArgDescriptions::eRequires, kArgDbTitle);

    CNcbiOstrstream msg;
    msg << "Number of volumes to aggregate, in which case the "
        << "basename for the database is extracted from the "
        << kOutput << " option";
    arg_desc->AddOptionalKey("num_volumes", "positive_integer", 
                             CNcbiOstrstreamToString(msg),
                             CArgDescriptions::eInteger);
    arg_desc->SetDependency("num_volumes", CArgDescriptions::eExcludes, kArgDb);
    arg_desc->SetDependency("num_volumes", CArgDescriptions::eExcludes, kArgGiList);
    arg_desc->SetDependency("num_volumes", CArgDescriptions::eExcludes, "dblist");
    arg_desc->SetDependency("num_volumes", CArgDescriptions::eRequires, kOutput);
    arg_desc->SetDependency("num_volumes", CArgDescriptions::eRequires, kArgDbType);
    arg_desc->SetDependency("num_volumes", CArgDescriptions::eRequires, kArgDbTitle);
    arg_desc->SetConstraint("num_volumes", new CArgAllowValuesBetween(0, 100));

    SetupArgDescriptions(arg_desc.release());
}

int 
CBlastDBAliasApp::ConvertGiFile(CNcbiIstream& input,
                                CNcbiOstream& output,
                                const string* input_fname /* = NULL */,
                                const string* output_fname /* = NULL */) const
{
    CBinaryListBuilder builder(CBinaryListBuilder::eGi);

    unsigned int line_ctr = 0;
    while (input) {
        string line;
        NcbiGetlineEOL(input, line);
        line_ctr++;
        if ( !line.empty() ) {
            if (NStr::StartsWith(line, "#")) continue;
            try { builder.AppendId(NStr::StringToInt8(line)); }
            catch (const CStringException& e) {
                ERR_POST(Warning << "error in line " << line_ctr 
                         << ": " << e.GetMsg());
            }
        }
    }

    builder.Write(output);
    if (input_fname && output_fname) {
        LOG_POST("Converted " << builder.Size() << " GIs from " << *input_fname
                 << " to binary format in " << *output_fname);
    } else {
        LOG_POST("Converted " << builder.Size() << " GIs into binary GI file");
    }
    return 0;
}

void
CBlastDBAliasApp::CreateAliasFile() const
{
    const CArgs& args = GetArgs();
    string title;
    if (args[kArgDbTitle].HasValue()) {
        title = args[kArgDbTitle].AsString();
    } else if (args[kArgDb].HasValue()) {
        _ASSERT(args[kArgGiList].HasValue());
        title = args[kArgDb].AsString() + " limited by " + 
            args[kArgGiList].AsString();
    }
    const CWriteDB::ESeqType seq_type = 
        args[kArgDbType].AsString() == "prot"
        ? CWriteDB::eProtein
        : CWriteDB::eNucleotide;

    string gilist = args[kArgGiList] ? args[kArgGiList].AsString() : kEmptyStr;
    if ( !gilist.empty() ) {
        if ( !CFile(gilist).Exists() ) {
            NCBI_THROW(CSeqDBException, eFileErr, gilist + " not found");
        }
        if ( !SeqDB_IsBinaryGiList(gilist) ) {
            const char mol_type = args[kArgDbType].AsString()[0];
            _ASSERT(mol_type == 'p' || mol_type == 'n');
            CNcbiOstrstream oss;
            oss << args[kOutput].AsString() << "." << mol_type << ".gil";
            gilist.assign(CNcbiOstrstreamToString(oss));
            const string& ifname = args[kArgGiList].AsString();
            ifstream input(ifname.c_str());
            ofstream output(gilist.c_str());
            ConvertGiFile(input, output, &ifname, &gilist);
        }
    }

    if (args["dblist"].HasValue()) {
        const string dblist = args["dblist"].AsString();
        vector<string> dbs2aggregate;
        NStr::Tokenize(dblist, " ", dbs2aggregate);
        CWriteDB_CreateAliasFile(args[kOutput].AsString(), dbs2aggregate,
                                 seq_type, gilist, title);
    } else if (args["num_volumes"].HasValue()) {
        const unsigned int num_vols = 
            static_cast<unsigned int>(args["num_volumes"].AsInteger());
        CWriteDB_CreateAliasFile(args[kOutput].AsString(), num_vols, seq_type,
                                 title);
    } else {
        CWriteDB_CreateAliasFile(args[kOutput].AsString(),
                                 args[kArgDb].AsString(),
                                 seq_type, gilist,
                                 title);
    }
}

int CBlastDBAliasApp::Run(void)
{
    const CArgs& args = GetArgs();
    int status = 0;

    try {

        if (x_GetOperationMode() == eConvertGiFile) {
            CNcbiIstream& input = args["gi_file_in"].AsInputFile();
            CNcbiOstream& output = args["gi_file_out"].AsOutputFile();
            status = ConvertGiFile(input, output);
        } else {
            CreateAliasFile();
        }

    } CATCH_ALL(status)
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastDBAliasApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
