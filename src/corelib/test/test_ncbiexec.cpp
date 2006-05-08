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
 * Author: Vladimir Ivanov
 *
 * File Description:   Test program for portable exec functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiexec.hpp>

#if defined(HAVE_UNISTD_H)
#  include <unistd.h>         // for _exit()
#endif

#include <test/test_assert.h> // This header must go last


USING_NCBI_SCOPE;


#define TEST_RESULT_C    99   // Test exit code for current test
#define TEST_RESULT_P     0   // Test exit code for test from PATH


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTest::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("test_files",
                       "test file's accessory functions");
    SetupArgDescriptions(d.release());
}


int CTest::Run(void)
{
    string app = GetArguments().GetProgramName();
    cout << "Application path: " << app << endl;

    // Initialization of variables and structures

    char* app_c = strdup(app.c_str());
    assert( app_c != 0 );

    const char* app_p  = "ls";
    const char* app_pp = "..";

    const char* my_env[] =   // Environment for Spawn*E
    {
        "THIS=environment will be",
        "PASSED=to new process by the",
        "EXEC=functions",
        "TEST_NCBI_EXEC=yes",
        NULL
    };

    {{
#if defined(NCBI_OS_CYGWIN)
        string path_setting("PATH=");
        const string& path = GetEnvironment().Get("PATH");
#else
        string path_setting("LD_LIBRARY_PATH=");
        const string& path = GetEnvironment().Get("LD_LIBRARY_PATH");
#endif
        if (path.size()) {
            path_setting += path;
            my_env[0] = strdup(path_setting.c_str());
        }
    }}

    const char* args_c[3];   // Arguments for SpawnV[E]
    args_c[1] = "SpawnV[E]";
    args_c[2] = NULL;

    const char* args_p[3];   // Arguments for SpawnVP[E]
    args_p[1] = app_pp;
    args_p[2] = NULL;

    // System

#if !defined(NCBI_OS_CYGWIN)
    // This test doesn't work on GCC/Cygwin
    assert( CExec::System(0) > 0 );
#endif
    string cmd = app + " System";
    assert( CExec::System(cmd.c_str()) == TEST_RESULT_C );
    cmd = string(app_p) + " " + app_pp;
    assert( CExec::System(cmd.c_str()) == TEST_RESULT_P );
    
    // Spawn with eWait

    TExitCode code;

    code = CExec::SpawnL  (CExec::eWait, app_c, "SpawnL_eWait",
                           NULL).GetExitCode(); 
    cout << "Exit code: " << code << endl;
    assert( code == TEST_RESULT_C );

    code = CExec::SpawnLP (CExec::eWait, app_p, app_pp,
                           NULL).GetExitCode();
    cout << "Exit code: " << code << endl;
    assert( code == TEST_RESULT_P );

    code = CExec::SpawnLE (CExec::eWait, app_c, "SpawnLE_eWait",
                           NULL, my_env).GetExitCode(); 
    cout << "Exit code: " << code << endl;
    assert( code == TEST_RESULT_C );

    code = CExec::SpawnLPE(CExec::eWait, app_c, "SpawnLPE_eWait",
                           NULL, my_env).GetExitCode();
    cout << "Exit code: " << code << endl;
    assert( code == TEST_RESULT_C );

    code = CExec::SpawnV  (CExec::eWait, app_c, args_c).GetExitCode();
    cout << "Exit code: " << code << endl;
    assert( code == TEST_RESULT_C );

    code = CExec::SpawnVP (CExec::eWait, app_p, args_p).GetExitCode();
    cout << "Exit code: " << code << endl;
    assert( code == TEST_RESULT_P );

    code = CExec::SpawnVE (CExec::eWait, app_c, args_c, my_env).GetExitCode();
    cout << "Exit code: " << code << endl;
    assert( code == TEST_RESULT_C );

    code = CExec::SpawnVPE(CExec::eWait, app_c, args_c, my_env).GetExitCode();
    cout << "Exit code: " << code << endl;
    assert( code == TEST_RESULT_C );

    // Spawn with eNoWait, waiting self

    TProcessHandle handle;

    handle = CExec::SpawnL  (CExec::eNoWait, app_c, "SpawnL_eNoWait",
                             NULL).GetProcessHandle();
    assert(CExec::Wait(handle) == TEST_RESULT_C );
    handle = CExec::SpawnLP (CExec::eNoWait, app_p, app_pp,
                             NULL).GetProcessHandle();
    assert(CExec::Wait(handle) == TEST_RESULT_P);
    handle = CExec::SpawnLE (CExec::eNoWait, app_c, "SpawnLE_eNoWait",
                             NULL, my_env).GetProcessHandle();
    assert(CExec::Wait(handle) == TEST_RESULT_C);
    handle = CExec::SpawnLPE(CExec::eNoWait, app_c, "SpawnLPE_eNoWait",
                             NULL, my_env).GetProcessHandle();
    assert(CExec::Wait(handle) == TEST_RESULT_C);

    // Spawn with eDetach

    CExec::SpawnL  (CExec::eDetach, app_c, "SpawnL_eDetach", NULL);
    CExec::SpawnLP (CExec::eDetach, app_p, app_pp, NULL);
    CExec::SpawnLE (CExec::eDetach, app_c, "SpawnLE_eDetach", NULL, my_env);
    CExec::SpawnLPE(CExec::eDetach, app_c, "SpawnLPE_eDetach",NULL, my_env);
    CExec::SpawnV  (CExec::eDetach, app_c, args_c);
    CExec::SpawnVP (CExec::eDetach, app_p, args_p);
    CExec::SpawnVE (CExec::eDetach, app_c, args_c, my_env);
    CExec::SpawnVPE(CExec::eDetach, app_c, args_c, my_env);

    // Spawn with eOverlay

    code = CExec::SpawnL(CExec::eOverlay, app_c, "SpawnL_eOverlay",
                         NULL).GetExitCode();
    assert( code == TEST_RESULT_C );

    // At success code below never been executed
    cout << endl << "TEST execution fails!" << endl << endl;

    return 77;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[], const char* envp[])
{
    // Exec from test?
    if ( argc > 1) {
        assert(argv[1] && *argv[1]);
        cout << endl << "Exec: " << argv[1] << endl;
        // View environment
        const char** env_var = &envp[0];
        while (*env_var) {
            cout << *env_var << endl;
            env_var++;
        }
        // Check environment
        if ( strstr(argv[1],"E_e")) {
            char* ptr = getenv("TEST_NCBI_EXEC");
            if (!ptr || !*ptr) {
                cout << "Environment variable TEST_NCBI_EXEC not found" <<endl;
                cout.flush();
                _exit(88);
            } else {
                cout << "TEST_NCBI_EXEC=" << ptr << endl;
            }
        }
        _exit(TEST_RESULT_C);
    }
    cout << "Start tests:" << endl << endl;

    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.22  2006/05/08 13:58:56  ivanov
 * Changed accordingly to last changes in CExec class.
 * Use GetExitCode() and GetProcessHandle() function for all Spawn* calls.
 *
 * Revision 6.21  2006/03/16 12:45:37  ivanov
 * Fixed to work on GCC/Cygwin
 *
 * Revision 6.20  2004/10/13 14:56:02  ivanov
 * Do not make a ls for current directory, run it for parent dir '..'
 * -- this fixed a problem with ls on Cygwin.
 *
 * Revision 6.19  2004/08/18 16:00:50  ivanov
 * Use NULL instead 0 where necessary to avoid problems with 64bit platforms
 *
 * Revision 6.18  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.17  2003/09/16 19:09:10  ivanov
 * Removed the code for location a current executable file using PATH env
 * variable. Use renewed GetProgramName() instead.
 *
 * Revision 6.16  2003/07/16 13:38:06  ucko
 * Make sure to preserve LD_LIBRARY_PATH if it was set in the original
 * environment, since we may need it to find libxncbi.so.
 *
 * Revision 6.15  2003/04/08 20:32:40  ivanov
 * Get rid of an unused variable
 *
 * Revision 6.14  2003/03/12 16:05:52  ivanov
 * MS Windows: Add extention to the executable file if running without it
 *
 * Revision 6.13  2003/03/10 20:49:06  ivanov
 * A PATH environment variable is used to find current file to execute
 *
 * Revision 6.12  2002/08/14 17:06:20  ucko
 * Reinstate calls to _exit, but be sure to include <unistd.h> first for
 * the prototype.
 *
 * Revision 6.11  2002/08/14 14:33:29  ivanov
 * Changed allcalls _exit() to exit() back -- non crossplatform function
 *
 * Revision 6.10  2002/08/13 14:09:48  ivanov
 * Changed exit() to exit() in the child's branch of the test
 *
 * Revision 6.9  2002/07/26 15:36:53  ivanov
 * Changed exit code in the Run()
 *
 * Revision 6.8  2002/07/25 15:53:06  ivanov
 * Changed exit code for failed test
 *
 * Revision 6.7  2002/07/23 20:34:22  ivanov
 * Added more info to debug output
 *
 * Revision 6.6  2002/07/23 13:30:30  ivanov
 * Added output of environment for each test
 *
 * Revision 6.5  2002/07/17 15:14:21  ivanov
 * Minor changes in the test exec branch
 *
 * Revision 6.4  2002/07/15 15:20:37  ivanov
 * Added extended error dignostic
 *
 * Revision 6.3  2002/06/29 06:45:50  vakatov
 * Get rid of some compilation warnings
 *
 * Revision 6.2  2002/06/25 20:17:58  ivanov
 * Changed default exit code from 0 to 1
 *
 * Revision 6.1  2002/05/30 16:29:15  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
