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
 * File Description:
 *   Portable DLL handling test
 * Note:
 *   This test use simple DLL (coremake/test/test_dll). It must be compiled 
 *   and copied to project's directory before run the test.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbidll.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


/////////////////////////////////
// 
// Common CDll test
//

static void s_TEST_SimpleDll(void)
{
    CDll dll("./","test_dll", CDll::eLoadLater);

    // Load DLL
    dll.Load();

    // DLL variable definition
    int*    DllVar_Counter;
    // DLL functions definition    
    int     (* Dll_Inc) (int) = NULL;
    int     (* Dll_Add) (int, int) = NULL;
    string* (* Dll_StrRepeat) (const string&, unsigned int) = NULL;

    // Get addresses from DLL
    DllVar_Counter = dll.GetEntryPoint_Data("DllVar_Counter", &DllVar_Counter);
    if ( !DllVar_Counter ) {
        ERR_POST(Fatal << "Error get address of variable DllVar_Counter.");
    }
    Dll_Inc = dll.GetEntryPoint_Func("Dll_Inc", &Dll_Inc );
    if ( !Dll_Inc ) {
        ERR_POST(Fatal << "Error get address of function Dll_Inc().");
    }
    Dll_Add = dll.GetEntryPoint_Func("Dll_Add", &Dll_Add );
    if ( !Dll_Add ) {
        ERR_POST(Fatal << "Error get address of function Dll_Add().");
    }
    Dll_StrRepeat = dll.GetEntryPoint_Func("Dll_StrRepeat", &Dll_StrRepeat );
    if ( !Dll_StrRepeat ) {
        ERR_POST(Fatal << "Error get address of function Dll_StrRepeat().");
    }

    // Call loaded function

    assert( *DllVar_Counter == 0  );
    assert( Dll_Inc(3)      == 3  );
    assert( *DllVar_Counter == 3  );
    assert( Dll_Inc(100)    == 103);
    assert( *DllVar_Counter == 103);
    *DllVar_Counter = 1;
    assert( Dll_Inc(0)      == 1  );

    assert( Dll_Add(3,4)    == 7  );
    assert( Dll_Add(-2,-1)  == -3 );
    
    string* str = Dll_StrRepeat("ab",2);
    assert( *str == "abab");
    delete str;

    str = Dll_StrRepeat("a",4);  
    assert( *str == "aaaa");
    delete str;

    // Unload used dll
    dll.Unload();
}


/////////////////////////////////
// 
//  Load windows systems DLL
//

static void s_TEST_WinSystemDll(void)
{
#if defined NCBI_OS_MSWIN

    CDll dll_user32("USER32", CDll::eLoadLater);
    CDll dll_userenv("userenv.dll", CDll::eLoadNow, CDll::eAutoUnload);

    // Load DLL
    dll_user32.Load();

    // DLL functions definition    
    BOOL (STDMETHODCALLTYPE FAR * dllMessageBeep) 
            (UINT type) = NULL;
    BOOL (STDMETHODCALLTYPE FAR * dllGetProfilesDirectory) 
            (LPTSTR  lpProfilesDir, LPDWORD lpcchSize) = NULL;

    // This is other variant of functions definition
    //
    // typedef BOOL (STDMETHODCALLTYPE FAR * LPFNMESSAGEBEEP) (
    //     UINT uType
    // );
    // LPFNMESSAGEBEEP  dllMessageBeep = NULL;
    //
    // typedef BOOL (STDMETHODCALLTYPE FAR * LPFNGETPROFILESDIR) (
    //     LPTSTR  lpProfilesDir,
    //     LPDWORD lpcchSize
    // );
    // LPFNGETUSERPROFILESDIR  dllGetProfilesDirectory = NULL;

    dllMessageBeep = dll_user32.GetEntryPoint_Func("MessageBeep", &dllMessageBeep );
    if ( !dllMessageBeep ) {
        ERR_POST(Fatal << "Error get address of function MessageBeep().");
    }
    // Call loaded function
    dllMessageBeep(-1);

    #ifdef UNICODE
    dll_userenv.GetEntryPoint_Func("GetProfilesDirectoryW", &dllGetProfilesDirectory);
    #else
    dll_userenv.GetEntryPoint_Func("GetProfilesDirectoryA", &dllGetProfilesDirectory);
    #endif
    if ( !dllGetProfilesDirectory ) {
        ERR_POST(Fatal <<
                 "Error get address of function GetUserProfileDirectory().");
    }
    // Call loaded function
    TCHAR szProfilePath[1024];
    DWORD cchPath = 1024;
    if ( dllGetProfilesDirectory(szProfilePath, &cchPath) ) {
        cout << "Profile dir: " << szProfilePath << endl;
    } else {
        ERR_POST(Fatal << "GetProfilesDirectory() failed");
    }

    // Unload USER32.DLL (our work copy)
    dll_user32.Unload();
    // USERENV.DLL will be unloaded in the destructor
    // dll_userenv.Unload();
#endif
}


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
    d->SetUsageContext("test_dll", "DLL accessory class");
    SetupArgDescriptions(d.release());
}


int CTest::Run(void)
{
    cout << "Run test" << endl << endl;

    // Common test
    s_TEST_SimpleDll();
    // Windows only extended test
    s_TEST_WinSystemDll();

    cout << endl;
    cout << "TEST execution completed successfully!" << endl << endl;
    return 0;
}


///////////////////////////////////
// MAIN
//

int main(int argc, const char* argv[])
{
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
