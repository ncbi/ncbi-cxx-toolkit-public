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

#include <common/test_assert.h>  /* This header must go last */


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

// static string s_IntToStr(int i)
// {
//     return NStr::IntToString(i);
// }



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
    delete sr;
    sr = 0;

    TTree* ur = new TTree(0);
    ur->AddNode(10);
    ur->AddNode(11);
    ur->AddNode(20);
    ur->AddNode(21);
    delete ur;
    ur = 0;
    

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

    delete str;
    str = tr5 = tr4 = tr6;  // just to eliminate "unused var" warning
}

struct IdValue
{
    int id;
    
    IdValue() : id(0) {}
    IdValue(int v) : id(v) {}

    operator int() const { return id; }
    int GetId() const { return id; }
};

// static string s_IdValueToStr(const IdValue& idv)
// {
//     return NStr::IntToString(idv.id);
// }


static void s_TEST_CountNodes()
{
    typedef CTreeNode<int>  TTree;

    // 0 - 10 - 20
    //        - 21 - 30
    //             - 31 - 40
    //                  - 41
    //                  - 42
    //   - 11 - 22
    //        - 23
    //        - 24
    //        - 25

    std::auto_ptr<TTree>    root( new TTree(0) );

    TTree* node10 = root->AddNode(10);
    node10->AddNode(20);

    TTree* node21 = node10->AddNode(21);
    node21->AddNode(30);

    TTree* node31 = node21->AddNode(31);
    node31->AddNode(40);
    node31->AddNode(41);
    node31->AddNode(42);

    TTree* node11 = root->AddNode(11);
    node11->AddNode(22);
    node11->AddNode(23);
    node11->AddNode(24);
    node11->AddNode(25);

    assert( root->CountNodes() == 2 );

    assert( root->CountNodes(0) == 1 );
    assert( root->CountNodes(0, TTree::fOnlyLeafs) == 0 );
    assert( root->CountNodes(0, TTree::fOnlyLeafs | TTree::fCumulative) == 0 );
    assert( root->CountNodes(0, TTree::fCumulative) == 1 );

    assert( root->CountNodes(1) == 2);
    assert( root->CountNodes(1, TTree::fOnlyLeafs) == 0 );
    assert( root->CountNodes(1, TTree::fOnlyLeafs | TTree::fCumulative) == 0 );
    assert( root->CountNodes(1, TTree::fCumulative) == 3 );

    assert( root->CountNodes(2) == 6 );
    assert( root->CountNodes(2, TTree::fOnlyLeafs) == 5 );
    assert( root->CountNodes(2, TTree::fOnlyLeafs | TTree::fCumulative) == 5 );
    assert( root->CountNodes(2, TTree::fCumulative) == 9 );

    assert( root->CountNodes(3) == 2 );
    assert( root->CountNodes(3, TTree::fOnlyLeafs) == 1 );
    assert( root->CountNodes(3, TTree::fOnlyLeafs | TTree::fCumulative) == 6 );
    assert( root->CountNodes(3, TTree::fCumulative) == 11 );

    assert( root->CountNodes(4) == 3 );
    assert( root->CountNodes(4, TTree::fOnlyLeafs) == 3 );
    assert( root->CountNodes(4, TTree::fOnlyLeafs | TTree::fCumulative) == 9 );
    assert( root->CountNodes(4, TTree::fCumulative) == 14 );

    assert( root->CountNodes(100) == 0 );
    return;
}


int CTestApplication::Run(void)
{


    //        CExceptionReporterStream reporter(cerr);
    //        CExceptionReporter::SetDefault(&reporter);
    //        CExceptionReporter::EnableDefault(false);
    //        CExceptionReporter::EnableDefault(true);
    //        CExceptionReporter::SetDefault(0);
    
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
    s_TEST_CountNodes();

    return 0;
}

  
/////////////////////////////////
// MAIN
//

int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv);
}
