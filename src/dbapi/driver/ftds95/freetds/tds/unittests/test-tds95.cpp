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
* Author:  Aaron Ucko
*
* File Description:
*   Wrapper for upstream FreeTDS tests that connect to servers.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbicfg.h>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiexec.hpp>
#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE

static const char* kServers[] = { "MSDEV1", "MSDEV4", "DBAPI_DEV3", NULL };

class CRunTestApplication : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
};

void CRunTestApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Wrapper for upstream FreeTDS tests");

    arg_desc->AddDefaultKey
        ("-ms-ver", "TDSVersion",
         "TDS protocol version to use for MS SQL servers",
         CArgDescriptions::eString, "7.3");
    arg_desc->AddOptionalKey
        ("-syb-ver", "TDSVersion",
         "Explicit TDS protocol version to use for Sybase servers",
         CArgDescriptions::eString);
    arg_desc->AddFlag
        ("-no-auto",
         "Skip in automated runs due to use of non-temporary objects.");

    arg_desc->AddPositional
        ("test-command", "Command to run", CArgDescriptions::eString);
    arg_desc->AddExtra
        (0, kMax_UInt, "Arguments for command", CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}

int CRunTestApplication::Run(void)
{
    const CArgs& args = GetArgs();
    CNcbiEnvironment& env = SetEnvironment();

    if (args["-no-auto"]) {
        bool is_automated;
        env.Get("NCBI_AUTOMATED_BUILD", &is_automated);
        if (is_automated) {
            NcbiCout << "NCBI_UNITTEST_SKIPPED\n";
            return 0;
        }
    }

    CDir sybase_dir(NCBI_GetDefaultSybasePath());
    if (sybase_dir.Exists()  ||  env.Get("SYBASE").empty()) {
        env.Set("SYBASE", sybase_dir.GetPath());
    }
    string cmd     = args["test-command"].AsString(),
           cmdline = env.Get("CHECK_EXEC");
    if ( !cmdline.empty() ) {
#ifdef NCBI_OS_MSWIN
        if (NStr::EndsWith(cmdline, ".sh")) {
            cmdline = "sh " + cmdline;
        }
#endif
        cmdline += ' ';
    }
    cmdline += CExec::QuoteArg(cmd);
    for (size_t n = 1;  n <= args.GetNExtra();  ++n) {
        cmdline += ' ' + CExec::QuoteArg(args[n].AsString());
    }
    _TRACE("PATH: " << env.Get("PATH"));
    _TRACE("Command: " << cmdline);

    if (NStr::StartsWith(cmd, "db95_")  ||  NStr::StartsWith(cmd, "odbc95_")) {
        string prefix = cmd.substr(0, cmd.find('_') + 1),
               base   = cmd.substr(prefix.size());
        if (CFile(base + ".sql").Exists()  ||  CFile(base + ".in").Exists()) {
            vector<string> dirs(1, "."), masks, found;
            masks.push_back(base + ".??*");
            masks.push_back(base + "_*.??*");
            FindFiles(found, dirs.begin(), dirs.end(), masks, fFF_File);
            ITERATE (vector<string>, it, found) {
                CFile f(*it);
                f.Copy(prefix + f.GetName(),
                       CFile::fCF_Overwrite | CFile::fCF_PreserveAll);
            }
        }
    }

    if (CFile("odbc.ini").Exists()) {
        env.Set("ODBCINI",    "odbc.ini");
        env.Set("SYSODBCINI", "odbc.ini");
    }

    int status = 0;
    bool succeeded_anywhere = false;
    CNcbiOstrstream failures;

    for (const char* const* s = kServers;  *s != NULL;  ++s) {
        if (NStr::StartsWith(*s, "MS")) {
            env.Set("TDSVER", args["-ms-ver"].AsString());
        } else if (args["-syb-ver"]) {
            env.Set("TDSVER", args["-syb-ver"].AsString());
        } else {
            env.Unset("TDSVER");
        }
        {{
            string tdspwdfile = string("login-info-") + *s + ".txt";
            CNcbiOfstream ofs(tdspwdfile.c_str());
            ofs << "SRV=" << *s
                << "\nUID=DBAPI_test\nPWD=allowed\nDB=DBAPI_Sample\n";
            env.Set("TDSPWDFILE", tdspwdfile);
        }}
        NcbiCout << *s << "\n--------------------" << endl;
        int this_status = CExec::System(cmdline.c_str());
        if (this_status == 0) {
            NcbiCout << "... SUCCEEDED on " << *s << '\n';
            succeeded_anywhere = true;
        } else if (this_status == 77) {
            NcbiCout << "... SKIPPED on " << *s << '\n';
        } else {
            status = this_status;
            NcbiCout << "... FAILED on " << *s << ", with status " << status
                     << '\n';
            failures << ' ' << *s << " (" << status << ')';
        }
        NcbiCout << endl;
    }

    if (status != 0) {
        NcbiCout << "FAILED:" << (string)CNcbiOstrstreamToString(failures)
                 << '\n';
    } else if ( !succeeded_anywhere ) {
        NcbiCout << "NCBI_UNITTEST_SKIPPED\n";
    }
    return status;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, char** argv)
{
    return CRunTestApplication().AppMain(argc, argv);
}
