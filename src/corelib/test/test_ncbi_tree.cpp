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

#include <ncbi_pch.hpp>
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

ETreeTraverseCode TestFunctor1(CTreeNode<int>& tr, int delta)
{
    cout << tr.GetValue() << " :" << delta << endl;
    return eTreeTraverse;
}

ETreeTraverseCode TestFunctor2(CTreeNode<int>& tr)
{
  cout << tr.GetValue() << endl;
  return eTreeTraverse;
}

bool TestFunctor3(int a, int b) 
{
  return (a == b);
}

static string s_IntToStr(int i)
{
    return NStr::IntToString(i);
}



static void s_TEST_Tree()
{
    typedef CTreeNode<int>  TTree;
    
    TTree* tr = new TTree(0);
    TTree* tr10 = tr->AddNode(10);
    tr->AddNode(11);
    tr10->AddNode(20);
    tr10->AddNode(21);
   
    TTree* sr = new TTree(0);
    sr->AddNode(10);
    sr->AddNode(11);

    TTree* ur = new TTree(0);
    ur->AddNode(10);
    ur->AddNode(11);
    ur->AddNode(20);
    ur->AddNode(21);
    

//    TreePrint(cout, *tr, (IntConvType) NStr::IntToString);
//    TreeReRoot(*tr10);
//    TreePrint(cout, *tr10, (IntConvType) NStr::IntToString);

    cout << "Testing Breadth First Traversal" << endl;
    TreeBreadthFirstTraverse(*tr, TestFunctor2);
    cout << endl;

    cout << "Testing Depth First Traversal" << endl;
    TreeDepthFirstTraverse(*tr, TestFunctor1);
    cout << endl;

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

    TreeDepthFirstTraverse(*tr, TestFunctor1);
    cout << endl;

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

    TTree* str = tr = new TTree(0);
    
    //
    // 0 - 2 
    //       - 4
    //   - 3 
    //       - 5
    //       - 6
    //

    TTree* tr4 = tr->AddNode(2)->AddNode(4);
    tr = tr->AddNode(3);
    TTree* tr5 = tr->AddNode(5);
    TTree* tr6 = tr->AddNode(6);

    cout << "Test Tree: " << endl;

    TreeDepthFirstTraverse(*str, TestFunctor1);
    cout << endl;

    delete tr;
}

struct IdValue
{
    int id;
    
    IdValue() : id(0) {}
    IdValue(int v) : id(v) {}

    operator int() const { return id; }
    int GetId() const { return id; }
};

static string s_IdValueToStr(const IdValue& idv)
{
    return NStr::IntToString(idv.id);
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
 * Revision 1.20  2004/07/21 21:18:57  ucko
 * s_TEST_Tree: delete tr rather than tr5 at the end, now that the call
 * to TreeReRoot has been removed.
 *
 * Revision 1.19  2004/07/21 18:19:09  kuznets
 * Removed util dependent test cases
 *
 * Revision 1.18  2004/06/21 16:49:38  ckenny
 * fixed warnings in compilation
 *
 * Revision 1.17  2004/06/15 13:02:59  ckenny
 * + test case for tree operations
 *
 * Revision 1.16  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.15  2004/04/22 13:53:15  kuznets
 * + test for minimal set
 *
 * Revision 1.14  2004/04/21 16:43:08  kuznets
 * + test cases for AND, OR (tree node lists)
 *
 * Revision 1.13  2004/04/21 12:58:32  kuznets
 * + s_TEST_IdTreeOperations
 *
 * Revision 1.12  2004/04/19 16:01:30  kuznets
 * + #include <algo/tree/tree_algo.hpp>
 *
 * Revision 1.11  2004/04/09 14:40:33  kuznets
 * TreeReRoot tested
 *
 * Revision 1.10  2004/04/08 21:46:35  ucko
 * Use a more foolproof method of supplying NStr::IntToString to TreePrint.
 *
 * Revision 1.9  2004/04/08 18:35:50  kuznets
 * + tree print test
 *
 * Revision 1.8  2004/04/08 11:48:22  kuznets
 * + TreeTraceToRoot test
 *
 * Revision 1.7  2004/01/14 17:38:27  kuznets
 * Reflecting changed in ncbi_tree
 *
 * Revision 1.6  2004/01/14 17:03:28  kuznets
 * + test case for PairTreeTraceNode
 *
 * Revision 1.5  2004/01/14 15:26:24  kuznets
 * Added test case for PairTreeTraceNode
 *
 * Revision 1.4  2004/01/14 14:20:08  kuznets
 * + test for for depth first traversal
 *
 * Revision 1.3  2004/01/12 20:09:41  kuznets
 * Renamed CTreeNWay to CTreeNode
 *
 * Revision 1.2  2004/01/12 15:02:48  kuznets
 * + test for CTreePairNode
 *
 * Revision 1.1  2004/01/09 19:00:40  kuznets
 * Added test for tree
 *
 *
 * ==========================================================================
 */
