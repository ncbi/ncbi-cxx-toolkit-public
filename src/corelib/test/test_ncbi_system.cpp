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
* Author:  Vladimir Ivanov
*
* File Description:
*      System functions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 6.4  2001/11/08 21:10:59  ivanov
* Added test for GetCPUNumber()
*
* Revision 6.3  2001/07/23 15:24:06  ivanov
* Fixed bug in Get/Set times in DB-format (1 day difference)
*
* Revision 6.2  2001/07/02 21:33:09  vakatov
* Fixed against SIGXCPU during the signal handling.
* Increase the amount of reserved memory for the memory limit handler
* to 10K (to fix for the 64-bit WorkShop compiler).
* Use standard C++ arg.processing (ncbiargs) in the test suite.
* Cleaned up the code. Get rid of the "Ncbi_" prefix.
*
* Revision 6.1  2001/07/02 16:43:43  ivanov
* Initialization
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>

#include <memory>


USING_NCBI_SCOPE;


/////////////////////////////////
// General tests
//

static void Test_General(void)
{
    LOG_POST("\nGeneral tests\n");

    // Number of processors
    cout << "Number of processors: " << GetCPUNumber() << endl;
}


/////////////////////////////////
// User defined dump print handler
//

int s_PrintParameter = 0;

static void PrintHandler (ELimitsExitCode code, size_t limit, CTime& time, 
                   TLimitsPrintParameter param) 
{
    cout << "Type          : " << 
        (code == eLEC_Memory ? "Memory limit" : "CPU limit") << endl;
    cout << "Limit value   : " << limit << endl;
    cout << "Set time      : " << time.AsString() << endl;
    cout << "Our parameter : " << (param ? *(int*)param : 0) << endl;
}


/////////////////////////////////
// Memory limits
//

static void Test_MemLimit(void)
{
    LOG_POST("\nHeap limit test\n");

    const size_t kHeapLimit = 100000;

    _VERIFY( SetHeapLimit(kHeapLimit, PrintHandler, &s_PrintParameter) );
    
    for (size_t i = 0;  i <= kHeapLimit/10;  i++) {
        s_PrintParameter++;
        int* pi = new int[10];
        _ASSERT(pi);
    }
}


/////////////////////////////////
// CPU time limits
//

static void Test_CpuLimit(void)
{
    LOG_POST("\nCPU time limit test\n");

    _VERIFY( SetCpuTimeLimit(2) );

    for (;;) {
        continue;
    }
}


/////////////////////////////////
// Test application
//

class CTestApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CTestApplication::Init(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test some system-specific functions");

    // Describe the expected command-line arguments
    arg_desc->AddPositional
        ("feature",
         "Platform-specific feature to test",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("feature", &(*new CArgAllow_Strings, "mem", "cpu"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTestApplication::Run(void)
{
    // General tests
    Test_General();

    // Specific tests
    CArgs args = GetArgs();

    if (args["feature"].AsString() == "mem") {
        Test_MemLimit();
    }
    else if (args["feature"].AsString() == "cpu") {
        Test_CpuLimit();
    }
    else {
        _TROUBLE;
    }

    return 0;
}



  
///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

static CTestApplication theTestApplication;


int main(int argc, const char* argv[])
{
    // Execute main application function
    return theTestApplication.AppMain(argc, argv, 0, eDS_Default, 0);
}
