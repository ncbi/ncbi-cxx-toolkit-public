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
 * Author:  Anatoliy Kuznetov
 *
 * File Description:
 *   TEST for:  NCBI C++ core tree related API
 *
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_tree.hpp>
#include <algorithm>

#include <test/test_assert.h>  /* This header must go last */


// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;



/////////////////////////////////
// Test application
//

class CTestApplication : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestApplication::Init(void)
{
    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);
}

static void s_TEST_Tree()
{
    typedef CTreeNWay<int>  TTree;
    
    TTree* tr = new TTree(0);
    
    tr->AddNode(10);
    tr->AddNode(11);

    {{
    unsigned int cnt;
    TTree::TNodeList_CI it = tr->SubNodeBegin();
    TTree::TNodeList_CI it_end = tr->SubNodeEnd();
    
    for (cnt = 0; it != it_end; ++it, ++cnt) {
        const TTree* t = *it;
        int v = t->GetValue();
        assert(v == 10 || v == 11);
    }
    assert(cnt == 2);
    }}
    
    {{
    TTree* tr2 = new TTree(*tr);
    unsigned int cnt;
    TTree::TNodeList_CI it = tr2->SubNodeBegin();
    TTree::TNodeList_CI it_end = tr2->SubNodeEnd();
    
    for (cnt = 0; it != it_end; ++it, ++cnt) {
        const TTree* t = *it;
        int v = t->GetValue();
        assert(v == 10 || v == 11);
    }
    assert(cnt == 2);
    delete tr2;
    }}
    
    
    {{
    TTree::TNodeList_I it = tr->SubNodeBegin();
    TTree::TNodeList_I it_end = tr->SubNodeEnd();
    
    for (; it != it_end; ++it) {
        TTree* t = *it;
        int v = t->GetValue();
        if (v == 10)
        {
            tr->RemoveNode(t);
            break;
        }
    }
    }}


    {{
    unsigned int cnt;
    TTree::TNodeList_CI it = tr->SubNodeBegin();
    TTree::TNodeList_CI it_end = tr->SubNodeEnd();
    
    for (cnt = 0; it != it_end; ++it, ++cnt) {
        const TTree* t = *it;
        int v = t->GetValue();
        assert(v == 11);
    }
    assert(cnt == 1);
    }}
    
    delete tr;
}


int CTestApplication::Run(void)
{


    //        CExceptionReporterStream reporter(cerr);
    //        CExceptionReporter::SetDefault(&reporter);
    //        CExceptionReporter::EnableDefault(false);
    //        CExceptionReporter::EnableDefault(true);
    //        CExceptionReporter::SetDefault(0);
    
    SetupDiag(eDS_ToStdout);
    /*
      
    CExceptionReporter::EnableDefault(true);
    cerr << endl;
    NCBI_REPORT_EXCEPTION(
    "****** default reporter (stream) ******",e);

    CExceptionReporter::SetDefault(0);
    cerr << endl;
    NCBI_REPORT_EXCEPTION(
    "****** default reporter (diag) ******",e);
    */
    
    s_TEST_Tree();    

    return 0;
}

  
/////////////////////////////////
// APPLICATION OBJECT and MAIN
//

int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    CTestApplication theTestApplication;
    return theTestApplication.AppMain(argc, argv, 0 /*envp*/, eDS_ToMemory);
}


/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/01/09 19:00:40  kuznets
 * Added test for tree
 *
 *
 * ==========================================================================
 */
