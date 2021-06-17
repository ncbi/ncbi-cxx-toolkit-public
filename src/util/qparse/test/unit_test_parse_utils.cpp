/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*   This software/database is a "United States Government Work" under the
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
* Author: Andrea Asztalos
*
* File Description:
*   Unit test for parse utility functions
*
*/

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <util/qparse/parse_utils.hpp>

USING_NCBI_SCOPE;

static int s_GetTreeHeight(const CQueryParseTree::TNode& node)
{
    if (node.IsLeaf()) return 0;

    CQueryParseTree::TNode::TNodeList_CI iter = node.SubNodeBegin();
    vector<int> heights;
    for ( ; iter != node.SubNodeEnd(); ++iter) {
        CQueryParseTree::TNode& sub_node = **iter;
        heights.push_back(s_GetTreeHeight(sub_node));
    }

    auto it = max_element(begin(heights), end(heights));
    return *it + 1;
}

static int s_CountNodesOfGivenType(const CQueryParseTree::TNode& node, CQueryParseNode::EType type)
{
    if (node.IsLeaf()) {
        return (node->GetType() == type) ? 1 : 0;
    }

    CQueryParseTree::TNode::TNodeList_CI iter = node.SubNodeBegin();
    int count = 0;
    for (; iter != node.SubNodeEnd(); ++iter) {
        CQueryParseTree::TNode& sub_node = **iter;
        count += s_CountNodesOfGivenType(sub_node, type);
    }

    return (node->GetType() == type) ? count +1 : count;
}


BOOST_AUTO_TEST_CASE(Test_FlattenParseTree1)
{
    CQueryParseTree qtree;
    const char* q = "a AND b AND c AND d";

    //NcbiCout << "---------------------------------------------------" << endl;
    //NcbiCout << "Query:" << "'" << q << "'" << endl << endl;
    qtree.Parse(q);
    //qtree.Print(NcbiCout);
    
    BOOST_CHECK_EQUAL(s_GetTreeHeight(*qtree.GetQueryTree()), 3);
    BOOST_CHECK_EQUAL(s_CountNodesOfGivenType(*qtree.GetQueryTree(), CQueryParseNode::eAnd), 3);
    
    Flatten_ParseTree(*qtree.GetQueryTree());
    //qtree.Print(NcbiCout);

    BOOST_CHECK_EQUAL(s_GetTreeHeight(*qtree.GetQueryTree()), 1);
    BOOST_CHECK_EQUAL(s_CountNodesOfGivenType(*qtree.GetQueryTree(), CQueryParseNode::eAnd), 1);
}

BOOST_AUTO_TEST_CASE(Test_FlattenParseTree2)
{
    CQueryParseTree qtree;
    const char* q = "a AND (b AND (c AND d))";

    //NcbiCout << "---------------------------------------------------" << endl;
    //NcbiCout << "Query:" << "'" << q << "'" << endl << endl;
    qtree.Parse(q);
    //qtree.Print(NcbiCout);

    BOOST_CHECK_EQUAL(s_GetTreeHeight(*qtree.GetQueryTree()), 3);
    BOOST_CHECK_EQUAL(s_CountNodesOfGivenType(*qtree.GetQueryTree(), CQueryParseNode::eAnd), 3);
    
    Flatten_ParseTree(*qtree.GetQueryTree());
    //qtree.Print(NcbiCout);

    BOOST_CHECK_EQUAL(s_GetTreeHeight(*qtree.GetQueryTree()), 1);
    BOOST_CHECK_EQUAL(s_CountNodesOfGivenType(*qtree.GetQueryTree(), CQueryParseNode::eAnd), 1);
}

BOOST_AUTO_TEST_CASE(Test_FlattenParseTree3)
{
    CQueryParseTree qtree;
    const char* q = "NOT ((a AND NOT b) OR "
        "(c AND NOT d) OR "
        "(e AND NOT f) OR "
        "(g AND NOT h)) AND "
        "(a OR c OR e OR g)";
        

    //NcbiCout << "---------------------------------------------------" << endl;
    //NcbiCout << "Query:" << "'" << q << "'" << endl << endl;
    qtree.Parse(q);
    //qtree.Print(NcbiCout);

    BOOST_CHECK_EQUAL(s_GetTreeHeight(*qtree.GetQueryTree()), 7);
    BOOST_CHECK_EQUAL(s_CountNodesOfGivenType(*qtree.GetQueryTree(), CQueryParseNode::eAnd), 5);
    BOOST_CHECK_EQUAL(s_CountNodesOfGivenType(*qtree.GetQueryTree(), CQueryParseNode::eOr), 6);
    
    Flatten_ParseTree(*qtree.GetQueryTree());
    //qtree.Print(NcbiCout);

    BOOST_CHECK_EQUAL(s_GetTreeHeight(*qtree.GetQueryTree()), 5);
    BOOST_CHECK_EQUAL(s_CountNodesOfGivenType(*qtree.GetQueryTree(), CQueryParseNode::eAnd), 5);
    BOOST_CHECK_EQUAL(s_CountNodesOfGivenType(*qtree.GetQueryTree(), CQueryParseNode::eOr), 2);
}