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
 * Note:  On MS-Windows Cygwin should be installed and added to PATH before
 *        run this test program, because we use 'ls' command.
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

#include <common/test_assert.h> // This header must go last

#if NCBI_COMPILER_MSVC && (_MSC_VER >= 1400)
 /* Microsoft does not want to use POSIX name, not to accept POSIX compliance */
#  define sys_strdup  _strdup
#else
#  define sys_strdup   strdup
#endif /*NCBI_COMPILER_MSVC && _MSC_VER>=1400*/


USING_NCBI_SCOPE;


#define TEST_RESULT_C        99   // Test exit code for current test
#define TEST_RESULT_NO_ARGS  98   // Test exit code for current test (no arguments)
#define TEST_RESULT_P         0   // Test exit code for test from PATH

// Array of arguments to test argument quoting in SpawnL/SpawnV
const char* s_QuoteArgsTest[] =
{
    "",             // reserved:  Will be overwritten with application name
    "SpawnV_Quote", // reserved:  Test name  
    "",
    " ",
    "\\",
    " \\",
    "\\ ",
    "dir\\",
    "dir\\path",
    "d i r\\p a t h",
    "d i r\\p a t h\\",
    "a b",
    "\"a",          // "a
    "a\"b",         // a"b
    "\"a b\"",      // "a b"
    "a\\\\b",       // a\\b
    "a\\\\\\b",     // a\\\b
    "a\\\"b",       // a\"b
    "a\\\\\"b",     // a\\"b
    "a\\\\\\\"b",   // a\\\"b
    "a\\\\\\\\\"b", // a\\\\"b
    "\"",           // "
    "\"\"",         // ""
    "\"\"\"",       // """
    "\"\"\"\"",     // """"
    "!{} \t\r\n[|&;<>()$`'*?#~=%",
    NULL
};


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
    unique_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("test_ncbiexec", "Test program for portable exec functions");
    d->AddOptionalPositional
        ("run",
         "Run tests. This argument is required for a master test process.",
         CArgDescriptions::eString);
    SetupArgDescriptions(d.release());
}


int CTest::Run(void)
{
    TExitCode code;
    TProcessHandle handle;

    string app = GetArguments().GetProgramName();
    LOG_POST("Application path: " << app);

    // Initialization of variables and structures

    const char* app_c  = app.c_str();
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
#elif defined(NCBI_OS_DARWIN)
        string path_setting("DYLD_LIBRARY_PATH=");
        const string& path = GetEnvironment().Get("DYLD_LIBRARY_PATH");
#else
        string path_setting("LD_LIBRARY_PATH=");
        const string& path = GetEnvironment().Get("LD_LIBRARY_PATH");
#endif
        if (path.size()) {
            path_setting += path;
            my_env[0] = sys_strdup(path_setting.c_str());
        }
    }}

    const char* args_c[3];   // Arguments for SpawnV[E]
    args_c[1] = "SpawnV[E]";
    args_c[2] = NULL;

    const char* args_p[3];   // Arguments for SpawnVP[E]
    args_p[1] = app_pp;
    args_p[2] = NULL;


    // ResolvePath() test

    assert(CExec::IsExecutable(app_c));
    string res_path;
    res_path = CExec::ResolvePath(app_c);
    LOG_POST("Resolve path: " << app_c << " -> " << res_path);
    assert(!res_path.empty());
    res_path = CExec::ResolvePath(app_p);
    LOG_POST("Resolve path: " << app_p << " -> " << res_path);
    assert(!res_path.empty());
#if defined(NCBI_OS_MSWIN)
    res_path = CExec::ResolvePath("winver.exe");
    LOG_POST("Resolve path: " << "winver.exe" << " -> " << res_path);
    assert(!res_path.empty());
#endif


    // System

#if !defined(NCBI_OS_CYGWIN)
    // This test doesn't work on GCC/Cygwin
    assert(CExec::System(0) > 0);
#endif
    string cmd = app + " System";
    assert(CExec::System(cmd.c_str()) == TEST_RESULT_C);
    cmd = string(app_p) + " " + app_pp;
    assert(CExec::System(cmd.c_str()) == TEST_RESULT_P);

    // Spawn()
    // eWait only -- all other modes are similar to Spawn*() methods tested below
    {{
        vector<string> env;
        {
            auto p = my_env;
            while (*p) {
                env.push_back(*p);
                ++p;
            }
        }
        cout << "Spawn - no args - no env:" << endl;
        {
            code = CExec::Spawn(app_c, CExec::eWait).GetExitCode();;
            assert(code == TEST_RESULT_NO_ARGS);
            code = CExec::Spawn(app_p, CExec::eWait, CExec::fPath).GetExitCode();;
            assert(code == TEST_RESULT_P);
        }
        cout << "Spawn - args - no env:" << endl;
        {
            code = CExec::Spawn(app_c, { "Spawn_A_eWait" }, CExec::eWait).GetExitCode();;
            assert(code == TEST_RESULT_C);
            code = CExec::Spawn(app_p, { app_pp }, CExec::eWait, CExec::fPath).GetExitCode();;
            assert(code == TEST_RESULT_P);
        }
        cout << "Spawn - args - env:" << endl;
        {
            code = CExec::Spawn(app_c, { "Spawn_E_eWait" }, env, CExec::eWait).GetExitCode();;
            assert(code == TEST_RESULT_C);
        }
    }}


    // Spawn - no arguments
    // eWait only -- all other modes are similar and tested below
    cout << "eWait no arguments" << endl;
    {{
        cout << "SpawnL(1):" << endl;
        code = CExec::SpawnL(CExec::eWait, app_c).GetExitCode();
        assert(code == TEST_RESULT_NO_ARGS);
        cout << "SpawnL(2):" << endl;
        code = CExec::SpawnL(CExec::eWait, app_c, NULL).GetExitCode();
        assert(code == TEST_RESULT_NO_ARGS);

        cout << "SpawnLP(1):" << endl;
        code = CExec::SpawnLP(CExec::eWait, app_p).GetExitCode();
        assert(code == TEST_RESULT_P);
        cout << "SpawnLP(2):" << endl;
        code = CExec::SpawnLP(CExec::eWait, app_p, NULL).GetExitCode();
        assert(code == TEST_RESULT_P);

        cout << "SpawnLE:" << endl;
        code = CExec::SpawnLE(CExec::eWait, app_c, NULL, my_env).GetExitCode();
        assert(code == TEST_RESULT_NO_ARGS);

        cout << "SpawnLPE:" << endl;
        code = CExec::SpawnLPE(CExec::eWait, app_c, NULL, my_env).GetExitCode();
        assert(code == TEST_RESULT_NO_ARGS);

        cout << "SpawnV(1):" << endl;
        code = CExec::SpawnV(CExec::eWait, app_c).GetExitCode();
        assert(code == TEST_RESULT_NO_ARGS);
        cout << "SpawnV(2):" << endl;
        code = CExec::SpawnV(CExec::eWait, app_c, NULL).GetExitCode();
        assert(code == TEST_RESULT_NO_ARGS);

        cout << "SpawnVP(1):" << endl;
        code = CExec::SpawnVP(CExec::eWait, app_p).GetExitCode();
        assert(code == TEST_RESULT_P);
        cout << "SpawnVP(2):" << endl;
        code = CExec::SpawnVP(CExec::eWait, app_p, NULL).GetExitCode();
        assert(code == TEST_RESULT_P);

        cout << "SpawnVE:" << endl;
        code = CExec::SpawnVE(CExec::eWait, app_c, NULL, my_env).GetExitCode();
        assert(code == TEST_RESULT_NO_ARGS);

        cout << "SpawnVPE:" << endl;
        code = CExec::SpawnVPE(CExec::eWait, app_c, NULL, my_env).GetExitCode();
        assert(code == TEST_RESULT_NO_ARGS);
    }}

    // Spawn with eWait - with arguments
    cout << "eWait tests" << endl;
    {{
        code = CExec::SpawnL(CExec::eWait, app_c, "SpawnL_eWait", NULL).GetExitCode();
        assert(code == TEST_RESULT_C);
        code = CExec::SpawnLP(CExec::eWait, app_p, app_pp, NULL).GetExitCode();
        assert(code == TEST_RESULT_P);
        code = CExec::SpawnLE(CExec::eWait, app_c, "SpawnLE_eWait", NULL, my_env).GetExitCode();
        assert(code == TEST_RESULT_C);
        code = CExec::SpawnLPE(CExec::eWait, app_c, "SpawnLPE_eWait", NULL, my_env).GetExitCode();
        assert(code == TEST_RESULT_C);
        code = CExec::SpawnV(CExec::eWait, app_c, args_c).GetExitCode();
        assert(code == TEST_RESULT_C);
        code = CExec::SpawnVP(CExec::eWait, app_p, args_p).GetExitCode();
        assert(code == TEST_RESULT_P);
        code = CExec::SpawnVE(CExec::eWait, app_c, args_c, my_env).GetExitCode();
        assert(code == TEST_RESULT_C);
        code = CExec::SpawnVPE(CExec::eWait, app_c, args_c, my_env).GetExitCode();
        assert(code == TEST_RESULT_C);
    }}

    // Spawn with eNoWait, waiting self
    cout << "eNoWait tests" << endl;
    {{
        handle = CExec::SpawnL(CExec::eNoWait, app_c, "SpawnL_eNoWait", NULL).GetProcessHandle();
        assert(CExec::Wait(handle) == TEST_RESULT_C);
        handle = CExec::SpawnLP(CExec::eNoWait, app_p, app_pp, NULL).GetProcessHandle();
        assert(CExec::Wait(handle) == TEST_RESULT_P);
        handle = CExec::SpawnLE(CExec::eNoWait, app_c, "SpawnLE_eNoWait", NULL, my_env).GetProcessHandle();
        assert(CExec::Wait(handle) == TEST_RESULT_C);
        handle = CExec::SpawnLPE(CExec::eNoWait, app_c, "SpawnLPE_eNoWait", NULL, my_env).GetProcessHandle();
        assert(CExec::Wait(handle) == TEST_RESULT_C);
    }}

    // Spawn with eDetach
    cout << "eDetach tests" << endl;
    {{
        CExec::SpawnL(CExec::eDetach, app_c, "SpawnL_eDetach", NULL);
        CExec::SpawnLP(CExec::eDetach, app_p, app_pp, NULL);
        CExec::SpawnLE(CExec::eDetach, app_c, "SpawnLE_eDetach", NULL, my_env);
        CExec::SpawnLPE(CExec::eDetach, app_c, "SpawnLPE_eDetach", NULL, my_env);
        CExec::SpawnV(CExec::eDetach, app_c, args_c);
        CExec::SpawnVP(CExec::eDetach, app_p, args_p);
        CExec::SpawnVE(CExec::eDetach, app_c, args_c, my_env);
        CExec::SpawnVPE(CExec::eDetach, app_c, args_c, my_env);
    }}

    // Spawn with eWaitGroup
    {{
        code = CExec::SpawnL(CExec::eWaitGroup, app_c, "SpawnL_eWaitGroup", NULL).GetExitCode();
        assert(code == TEST_RESULT_C);
    }}

    // Spawn with eNoWaitGroup, waiting self
    {{
        handle = CExec::SpawnL(CExec::eNoWaitGroup, app_c, "SpawnL_eNoWaitGroup", NULL).GetProcessHandle();
        assert(CExec::Wait(handle) == TEST_RESULT_C);
    }}

    // Argument quoting test
    {{
        code = CExec::SpawnL(CExec::eWait, app_c,
                             "SpawnL_Quote",
                             s_QuoteArgsTest[2],
                             s_QuoteArgsTest[3],
                             s_QuoteArgsTest[4],
                             s_QuoteArgsTest[5],
                             s_QuoteArgsTest[6],
                             s_QuoteArgsTest[7],
                             s_QuoteArgsTest[8],
                             s_QuoteArgsTest[9],
                             s_QuoteArgsTest[10],
                             s_QuoteArgsTest[11],
                             s_QuoteArgsTest[12],
                             s_QuoteArgsTest[13],
                             s_QuoteArgsTest[14],
                             s_QuoteArgsTest[15],
                             s_QuoteArgsTest[16],
                             s_QuoteArgsTest[17],
                             s_QuoteArgsTest[18],
                             s_QuoteArgsTest[19],
                             s_QuoteArgsTest[20],
                             s_QuoteArgsTest[21],
                             s_QuoteArgsTest[22],
                             s_QuoteArgsTest[23],
                             s_QuoteArgsTest[24],
                             s_QuoteArgsTest[25],
                             NULL).GetExitCode();
        assert(code == TEST_RESULT_C);
        code = CExec::SpawnV(CExec::eWait, app_c, s_QuoteArgsTest).GetExitCode();
        assert(code == TEST_RESULT_C);
    }}

    // Spawn with eOverlay
    {{
        code = CExec::SpawnL(CExec::eOverlay, app_c, "SpawnL_eOverlay", NULL).GetExitCode();
        assert(code == TEST_RESULT_C);
    }}

    // At success code below never been executed
    LOG_POST("\nTEST execution fails!\n");

    return 77;
}


///////////////////////////////////
// MAIN
//

int main(int argc, const char* argv[], const char* /*envp*/[])
{
    // Exec without parameters?
    if (argc == 1) {
        cout << "Exec: no arguments" << endl;
        return TEST_RESULT_NO_ARGS;
    }

    // Exec with parameters

    assert(argc > 1);
    assert(argv[1] && *argv[1]);
    std::string_view argv1(argv[1]);

    if (argv1 != "run"sv) {
        // Child process:

        cout << endl << "Exec: " << argv1 << endl;

        if (argv1.find("Quote") != NPOS) {
            // Check arguments
            const size_t n = sizeof(s_QuoteArgsTest) / sizeof(s_QuoteArgsTest[0]);
            assert(argc == (n - 1));
            for (int i = 2; i < argc; i++) {
                //cout << i << " = '" << argv[i] << "'" << endl;
                //cout << i << " = '" << s_QuoteArgsTest[i] << "'" << endl;
                //cout << i << " = '" << NStr::PrintableString(argv[i]) << "'" << endl;
                //cout << i << " = '" << NStr::PrintableString(s_QuoteArgsTest[i]) << "'" << endl;
                cout.flush();
                assert(NStr::CompareCase(s_QuoteArgsTest[i], argv[i]) == 0);
            }
        } else {
            assert(argc == 2);
        }

        // View environment
        /*
        const char** env_var = &envp[0];
        while (*env_var) {
            cout << *env_var << endl;
            env_var++;
        }
        */
        // Check environment for Spawn*E() tests
        if (argv1.find("E_e") != NPOS) {
            char* ptr = getenv("TEST_NCBI_EXEC");
            if (!ptr || !*ptr) {
                cout << "Environment variable TEST_NCBI_EXEC not found" << endl;
                cout.flush();
                _exit(88);
            } else {
                cout << "TEST_NCBI_EXEC=" << ptr << endl;
            }
        }
        cout.flush();
        _exit(TEST_RESULT_C);
    }

    // Execute main application / master process
    return CTest().AppMain(argc, argv);
}
