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
 * Author: Irena Zaretskaya
 *
 */

/** @file blastdbcmd_spec.cpp
 * Command line tool to examine the contents of BLAST databases. This is the
 * successor to fastacmd from the C toolkit
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/version.hpp>
#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
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
};





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
                            CArgDescriptions::eString, "nucl");
    arg_desc->SetConstraint(kArgDbType, &(*new CArgAllow_Strings,
                                        "nucl", "prot"));
   
    arg_desc->SetCurrentGroup("Output configuration options");
    arg_desc->AddDefaultKey(kArgOutput, "output_file", "Output file name",
                            CArgDescriptions::eOutputFile, "-");
                            
    SetupArgDescriptions(arg_desc.release());
}

int CBlastDBCmdApp::Run(void)
{
    int status = 0;
    const CArgs& args = GetArgs();
    
    try {

        string dbtype = args[kArgDbType].AsString();
        string dbLocation = SeqDB_ResolveDbPathNoExtension(args[kArgDb].AsString(),dbtype.at(0));
        CNcbiOstream& out = args["out"].AsOutputFile();
        if(dbLocation.empty()) {
            status = 1;
        }
        out << dbLocation << NcbiEndl;               	


    } CATCH_ALL(status)

    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastDBCmdApp().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
