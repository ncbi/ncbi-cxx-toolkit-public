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
* Revision 6.1  2001/07/02 16:43:43  ivanov
* Initialization
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbi_system.hpp>


USING_NCBI_SCOPE;


/////////////////////////////////
// Memory limits
//

static void Test_MemLimit(void)
{
    LOG_POST("\nHeap limit test\n");

    const size_t kHeapLimit = 100000;

    if ( !Ncbi_SetHeapLimit(kHeapLimit) ) {
        ERR_POST("Error set heap limit");
        return;
    }
    
    for (size_t i = 0;  i <= kHeapLimit/10; i++) {
        int *pi = new int[10];
        if ( !pi ) {
            ERR_POST("Not memory!");
        }
    }
}


/////////////////////////////////
// CPU time limits
//

static void Test_CpuLimit(void)
{
    LOG_POST("\nCPU time limit test\n");

    if ( !Ncbi_SetCpuTimeLimit(2) ) {
        ERR_POST("Error set CPU time limit");
        return;
    }
    while (1);
}

/////////////////////////////////
// Test application
//

class CTestApplication : public CNcbiApplication
{
public:
    virtual ~CTestApplication(void);
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
    SetDiagStream(&NcbiCout);

    bool none_param = false;
    for ( size_t i = 1;  i < GetArguments().Size();  ++i ) {
        if ( GetArguments()[i] == "-mem" )
            Test_MemLimit();
        else if ( GetArguments()[i] == "-cpu" )
            Test_CpuLimit();
        else {
            none_param = true;
        }
    }
    if ( none_param )
        LOG_POST("None parameters (-mem, -cpu). Test not running.");
    else {
        // test normal exit with booked handler
        Ncbi_SetHeapLimit(100000);
        LOG_POST("\nTEST execution completed successfully!\n");
        return 0;
    }
    return 0;
}

CTestApplication::~CTestApplication()
{
    SetDiagStream(0);
    _ASSERT( IsDiagStream(0) );
}


  
///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

static CTestApplication theTestApplication;


int main(int argc, const char* argv[])
{
    // Execute main application function
    return theTestApplication.AppMain(argc, argv);
}
