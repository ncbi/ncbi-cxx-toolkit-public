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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiexec.hpp>

#include <test/test_assert.h>  // This header must go last

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
    // Initialization of variables and structures

    const string app = GetArguments().GetProgramName();
    char* app_c = strdup(app.c_str());
    assert( app_c != 0 );

    int pid = 0;

    //char *app_p  = "echo";
    //char *app_pp = "This message is from child process";
    char *app_p  = "ls";
    char *app_pp = "-l";

    char *my_env[] =   // Environment for Spawn*E
    {
        "THIS=environment will be",
        "PASSED=to new process by",
        "the EXEC=functions",
        "TEST_NCBI_EXEC=yes",
        0
    };

    char *args_c[3];   // Arguments for SpawnV[E]
    args_c[1] = "SpawnV[E]";
    args_c[2] = 0;

    char *args_p[3];   // Arguments for SpawnVP[E]
    args_p[1] = app_pp;
    args_p[2] = 0;

    // System
        
    assert( CExec::System(0) > 0 );
    string cmd = app + " System";
    assert( CExec::System(cmd.c_str()) == TEST_RESULT_C );
    cmd = string(app_p) + " " + app_pp;
    assert( CExec::System(cmd.c_str()) == TEST_RESULT_P );
    
    // Spawn with eWait

    assert( CExec::SpawnL  (CExec::eWait, app_c, "SpawnL_eWait", 0) 
            == TEST_RESULT_C );
    assert( CExec::SpawnLP (CExec::eWait, app_p, app_pp, 0) 
            == TEST_RESULT_P );
    assert( CExec::SpawnLE (CExec::eWait, app_c, "SpawnLE_eWait", 0, my_env) 
            == TEST_RESULT_C );
    assert( CExec::SpawnLPE(CExec::eWait, app_c, "SpawnLPE_eWait", 0, my_env)
            == TEST_RESULT_C );
    assert( CExec::SpawnV  (CExec::eWait, app_c, args_c)
            == TEST_RESULT_C );
    assert( CExec::SpawnVP (CExec::eWait, app_p, args_p)
            == TEST_RESULT_P );
    assert( CExec::SpawnVE (CExec::eWait, app_c, args_c, my_env)
            == TEST_RESULT_C );
    assert( CExec::SpawnVPE(CExec::eWait, app_c, args_c, my_env)
            == TEST_RESULT_C );

    // Spawn with eNoWait, waiting self

    pid = CExec::SpawnL(CExec::eNoWait, app_c, "SpawnL_eNoWait", 0);
    assert( pid != -1 );
    assert( CExec::Wait(pid) == TEST_RESULT_C );

    pid = CExec::SpawnLP(CExec::eNoWait, app_p, app_pp, 0);
    assert( pid != -1 );
    assert( CExec::Wait(pid) == TEST_RESULT_P );

    pid = CExec::SpawnLE(CExec::eNoWait, app_c, "SpawnLE_eNoWait", 0, my_env);
    assert( pid != -1 );
    assert( CExec::Wait(pid) == TEST_RESULT_C );

    pid = CExec::SpawnLPE(CExec::eNoWait, app_c, "SpawnLPE_eNoWait", 0, my_env);
    assert( pid != -1 );
    assert( CExec::Wait(pid) == TEST_RESULT_C );

    // Spawn with eDetach

    CExec::SpawnL  (CExec::eDetach, app_c, "SpawnL_eDetach", 0 );
    CExec::SpawnLP (CExec::eDetach, app_p, app_pp, 0);
    CExec::SpawnLE (CExec::eDetach, app_c, "SpawnLE_eDetach", 0, my_env);
    CExec::SpawnLPE(CExec::eDetach, app_c, "SpawnLPE_eDetach", 0, my_env);
    CExec::SpawnV  (CExec::eDetach, app_c, args_c);
    CExec::SpawnVP (CExec::eDetach, app_p, args_p);
    CExec::SpawnVE (CExec::eDetach, app_c, args_c, my_env);
    CExec::SpawnVPE(CExec::eDetach, app_c, args_c, my_env);

    // Spawn with eOverlay

    assert( CExec::SpawnL  (CExec::eOverlay, app_c, "SpawnL_eOverlay", 0)
            == TEST_RESULT_C );

    // At success code below never been executed
    cout << endl << "TEST execution fails!" << endl << endl;

    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
     // Test exec run
    if ( argc > 1) {
        cout << "Exec: " << argv[1] << endl;
        // Check environment
        if (strstr(argv[1],"E_e")) {
           char* ptr = getenv("TEST_NCBI_EXEC");
           if (!ptr) {
               exit(1);
           }
        }
        exit(TEST_RESULT_C);
    }
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.1  2002/05/30 16:29:15  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
