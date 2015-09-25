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
* Author:  Greg Boratyn
*
* File Description:
*   Unit tests for CClusterer
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <algo/cobalt/clusterer.hpp>

#include <math.h>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(cobalt);

BOOST_AUTO_TEST_SUITE(clusterer)

BOOST_AUTO_TEST_CASE(TestSingleCluster)
{
    // Empty cluster
    CClusterer::CSingleCluster cluster;
    BOOST_CHECK_EQUAL((int)cluster.size(), 0);
    BOOST_CHECK(cluster.GetPrototype() < 0);

    // Index out of range throws
    BOOST_CHECK_THROW(cluster[0], CClustererException);

    // Finding center element of an empty cluster throws exception
    BOOST_CHECK_THROW(
           cluster.FindCenterElement(CClusterer::TDistMatrix(2, 2, 0.0)),
           CClustererException);
}

BOOST_AUTO_TEST_CASE(TestClusterer)
{
    // Empty clusterer is created with no clusters and distance matrix
    CClusterer clusterer;
    BOOST_CHECK_EQUAL((int)clusterer.GetClusters().size(), 0);
    BOOST_CHECK_THROW(clusterer.GetDistMatrix(), CClustererException);

    // One element distance matrix yields one one-element cluster
    CClusterer::TDistMatrix dmat(1, 1, 0.0);
    clusterer.SetDistMatrix(dmat);
    clusterer.ComputeClusters(1.0);
    BOOST_CHECK_EQUAL((int)clusterer.GetClusters().size(), 1);
    BOOST_CHECK_EQUAL((int)clusterer.GetSingleCluster(0).size(), 1);

    // For two elements...
    dmat.Resize(2, 2, 0.0);
    dmat(0, 1) = 0.1;
    dmat(1, 0) = dmat(0, 1);
    clusterer.SetDistMatrix(dmat);

    // ... larger max distance yields one cluster
    clusterer.ComputeClusters(1.0);
    BOOST_CHECK_EQUAL((int)clusterer.GetClusters().size(), 1);

    // ... smaller max distance yields two clusters
    clusterer.ComputeClusters(0.01);
    BOOST_CHECK_EQUAL((int)clusterer.GetClusters().size(), 2);

    // Accessing cluster by index out of range yields exception
    BOOST_CHECK_THROW(clusterer.GetSingleCluster(3), CClustererException);

    // The settings below produce more than 1 cluster
    dmat.Resize(4, 4, 0.0);
    dmat(0, 1) = dmat(1, 0) = 0.1;
    dmat(0, 2) = dmat(2, 0) = 0.3;
    dmat(0, 3) = dmat(3, 0) = 0.2;
    dmat(1, 2) = dmat(2, 1) = 0.5;
    dmat(1, 3) = dmat(3, 1) = 0.2;
    dmat(2, 3) = dmat(3, 2) = 0.1;
    clusterer.SetDistMatrix(dmat);
    clusterer.ComputeClusters(0.4);
    BOOST_CHECK((int)clusterer.GetClusters().size() > 1);

    // Cluster distance matrix is square with size equal to number of cluster
    // elements
    clusterer.GetClusterDistMatrix(0, dmat);
    BOOST_CHECK_EQUAL(dmat.GetRows(), dmat.GetCols());
    BOOST_CHECK_EQUAL(dmat.GetRows(), clusterer.GetSingleCluster(0).size());
    clusterer.GetClusterDistMatrix(1, dmat);
    BOOST_CHECK_EQUAL(dmat.GetRows(), dmat.GetCols());
    BOOST_CHECK_EQUAL(dmat.GetRows(), clusterer.GetSingleCluster(1).size());

    // Incompatible cluster elements and distance matrix cause exception
    clusterer.SetClusters()[0].AddElement(10);
    BOOST_CHECK_THROW(clusterer.GetClusterDistMatrix(0, dmat),
                      CClustererException);

    // Supplying non-square distance matrix yields exception
    dmat.Resize(2, 3, 0.0);
    BOOST_CHECK_THROW(clusterer.SetDistMatrix(dmat), CClustererException);

    // Accessing non-existing distance matrix yields exception
    clusterer.PurgeDistMatrix();
    BOOST_CHECK_THROW(clusterer.GetDistMatrix(), CClustererException);
}


///Read distance matrix from file
///@param filename Filename [in]
///@param dmat Distance matrix [out]
static void s_ReadDistMatrix(const string& filename,
                             CClusterer::TDistMatrix& dmat)
{
    CNcbiIfstream istr(filename.c_str());
    BOOST_REQUIRE(istr);
    vector<double> dists;
    while (!istr.eof()) {
        double val = DBL_MAX;
        istr >> val;

        if (val != DBL_MAX) {
            dists.push_back(val);
        }
    }

    int num_elements = (int)sqrt((double)dists.size());

    // distances must form a square matrix
    BOOST_REQUIRE_EQUAL(num_elements * num_elements, (int)dists.size());
    
    dmat.Resize(num_elements, num_elements, 0.0);
    for (int i=0;i < num_elements;i++) {
        for (int j=0;j < num_elements;j++) {
            dmat(i, j) = dists[i*num_elements + j];
        }
    }
}


// Traverse the tree and check if elements such that elems[elem] == false
// appear in the tree and others do not
static void s_TestTree(vector<bool>& elems, const TPhyTreeNode* node)
{
    if (node->IsLeaf()) {

        int id = node->GetValue().GetId();
        BOOST_REQUIRE(id < (int)elems.size());

        // each element appears in the tree exactly once
        BOOST_REQUIRE(!elems[id]);
        elems[id] = true;
    }
    else {

        TPhyTreeNode::TNodeList_CI it = node->SubNodeBegin();
        for (; it != node->SubNodeEnd();it++) {
            s_TestTree(elems, *it);
        }
    }
}


// Check if all cluster elements appear in the tree
static void s_TestClusterTree(const CClusterer::TSingleCluster& cluster,
                              const TPhyTreeNode* tree)
{
    BOOST_REQUIRE(tree);
    BOOST_REQUIRE(cluster.size() > 0);

    // find max element
    int max_elem = cluster[0];
    for (int i=1;i < (int)cluster.size();i++) {
        if (cluster[i] > max_elem) {
            max_elem = cluster[i];
        }
    }

    // s_TestTree fails if e such that elems[e] == true is found or e such
    // that elems[e] == false is not found
    vector<bool> elems(max_elem + 1, true);
    ITERATE (CClusterer::TSingleCluster, elem, cluster) {
        elems[*elem] = false;
    }

    s_TestTree(elems, tree);

    // make sure all elements were found
    ITERATE (vector<bool>, it, elems) {
        BOOST_CHECK(*it);
    }
}


/// Check clusters
/// @param dmat Distance matrix [in]
/// @param clusters Clusters to examine [in]
/// @param trees Cluster trees to examine [in] 
/// @param ref_filename Name of filename containing reference clusters data [in]
static void s_TestClustersAndTrees(int num_elems,
                                   CClusterer& clusterer,
                                   const string& ref_filename = "")
{
    BOOST_REQUIRE(clusterer.GetClusters().size() > 0);

    vector<bool> check_elems(num_elems, false);

    const CClusterer::TClusters& clusters = clusterer.GetClusters();
    const vector<TPhyTreeNode*>& trees = clusterer.GetTrees();
    
    // check whether each element belongs to exactly one cluster
    int num_elements = 0;
    ITERATE (CClusterer::TClusters, cluster, clusters) {
        ITERATE (CClusterer::TSingleCluster, elem, *cluster) {

            BOOST_REQUIRE(*elem < num_elems);

            // each element appear only once in all clusters
            BOOST_REQUIRE(!check_elems[*elem]);
            check_elems[*elem] = true;

            num_elements++;
        }
    }

    // make sure all elements were found in clusters
    ITERATE (vector<bool>, it, check_elems) {
        BOOST_CHECK(*it);
    }

    // Check cluster trees
    // each cluster must have its tree
    BOOST_REQUIRE_EQUAL(clusters.size(), trees.size());

    // check each tree
    for (size_t i=0;i < clusters.size();i++) {
        s_TestClusterTree(clusters[i], trees[i]);
    }

    // Compare clusters and their elements to reference
    if (!ref_filename.empty()) {

        // read reference clusters from file
        // format: number of clusters, cluster sizes, elements of cluster 0,
        // elements of cluster1, ...
        CNcbiIfstream ref_istr(ref_filename.c_str());
        BOOST_REQUIRE(ref_istr);
        vector<int> ref_clust_elems;
        int ref_num_clusters = 0;
        int ref_num_elems = 0;

        // ingnore comment lines
        // comments are allowed in the beginning of the reference file
        char* buff = new char[256];
        while (ref_istr.peek() == (int)'#') {
            ref_istr.getline(buff, 256);
        }
        delete [] buff;

        // read number of clusters
        ref_istr >> ref_num_clusters;
        // ... and compare with computed clusters
        BOOST_REQUIRE_EQUAL((int)clusters.size(), ref_num_clusters);

        // read cluster sizes and compare with computed cluster sizes
        int ind = 0;
        for (int i=0;i < ref_num_clusters;i++) {

            BOOST_REQUIRE(!ref_istr.eof());

            int ref_size = 0;
            ref_istr >> ref_size;
            BOOST_REQUIRE_EQUAL((int)clusters[ind++].size(), ref_size);

            ref_num_elems += ref_size;
        }

        // read cluster elements
        bool zero_found = false;
        for (int i=0;i < ref_num_elems;i++) {

            BOOST_REQUIRE(!ref_istr.eof());

            int ref_elem;
            ref_istr >> ref_elem;

            ref_clust_elems.push_back(ref_elem);
            

            // there can be only one zero
            BOOST_REQUIRE((!zero_found && ref_elem >= 0)
                          || (zero_found && ref_elem > 0));

            if (ref_elem == 0) {
                zero_found = true;
            }
        }

        // compare computed cluster elements with refernece
        ind = 0;
        ITERATE(CClusterer::TClusters, cluster, clusters) {
            ITERATE(CClusterer::TSingleCluster, elem, *cluster) {

                BOOST_REQUIRE_EQUAL(*elem, ref_clust_elems[ind++]);
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(TestNoLinks)
{
    CClusterer clusterer;
    clusterer.SetMakeTrees(true);

    const int num_elements = 5;
    CRef<CLinks> links(new CLinks(num_elements));
    clusterer.SetLinks(links);
    clusterer.Run();

    s_TestClustersAndTrees(num_elements, clusterer);
}

BOOST_AUTO_TEST_CASE(TestOneElement)
{
    CClusterer clusterer;
    clusterer.SetMakeTrees(true);    

    // one element
    const int num_elements = 1;

    CRef<CLinks> links(new CLinks(num_elements));
    // one element, hence no links
    clusterer.SetLinks(links);
    clusterer.Run();

    s_TestClustersAndTrees(num_elements, clusterer);
}


BOOST_AUTO_TEST_CASE(TestTwoElementsOneCluster)
{
    CClusterer clusterer;
    clusterer.SetMakeTrees(true);

    // two elements
    const int num_elements = 2;

    CRef<CLinks> links(new CLinks(num_elements));
    // link exists, hence the elements will be joined
    links->AddLink(0, 1, 0.0);
    clusterer.SetLinks(links);
    clusterer.Run();

    s_TestClustersAndTrees(num_elements, clusterer);
}


BOOST_AUTO_TEST_CASE(TestTwoElementsTwoClusters)
{
    CClusterer clusterer;
    clusterer.SetMakeTrees(true);

    // two elements
    const int num_elements = 2;

    CRef<CLinks> links(new CLinks(num_elements));
    // no link between elements, hence two clusters
    clusterer.SetLinks(links);
    clusterer.Run();

    s_TestClustersAndTrees(num_elements, clusterer);
}

BOOST_AUTO_TEST_CASE(TestMoreElements)
{
    // Check clusters and trees and compare clusters with reference files

    CClusterer clusterer;
    clusterer.SetMakeTrees(true);

    CClusterer::TDistMatrix dmat;
    s_ReadDistMatrix("data/dist_matrix.txt", dmat);    

    BOOST_REQUIRE_EQUAL(dmat.GetCols(), dmat.GetRows());

    // two elements
    const int num_elements = dmat.GetCols();

    // maximum cluster diameter
    const double max_distance = 0.8;

    CRef<CLinks> links(new CLinks(num_elements));
    for (size_t i=0;i < dmat.GetCols() - 1;i++) {
        for (size_t j=i+1;j < dmat.GetCols();j++) {
            if (dmat(i, j) < max_distance) {
                links->AddLink(i, j, dmat(i, j));
            }
        }
    }
    clusterer.SetLinks(links);
    clusterer.Run();

    s_TestClustersAndTrees(num_elements, clusterer, "data/ref_clusters.txt");
}

// Test clustering using pre-computed clusters as another input
BOOST_AUTO_TEST_CASE(TestPrecomputedClusters)
{
    // create two pre-computed clusters with elements (0, 1, 2), (3)
    CClusterer::TClusters pre_clusters(2);
    pre_clusters[0].AddElement(0);
    pre_clusters[0].AddElement(1);
    pre_clusters[0].AddElement(2);
    pre_clusters[1].AddElement(3);
    
    // create links for clustering
    CRef<CLinks> links(new CLinks(6));
    links->AddLink(0, 4, 0.5);
    links->AddLink(1, 4, 1.5);
    links->AddLink(4, 2, 2.0);
    
    // set pre-computed clusters
    CClusterer clusterer(links);
    ITERATE (CClusterer::TClusters, it, pre_clusters) {
        clusterer.SetClusters().push_back(*it);
    }

    // run clustering
    clusterer.Run();
    CClusterer::TClusters& clusters = clusterer.SetClusters();
    BOOST_REQUIRE_EQUAL(clusters.size(), 3u);

    // make sure that all elements are present once and assigned to correct
    // clusters
    vector<bool> presence(links->GetNumElements(), false);
    NON_CONST_ITERATE(CClusterer::TClusters, clust, clusters) {
        int elem = *clust->begin();
        if (elem == 5 || elem == 3) {
            BOOST_REQUIRE_EQUAL(clust->size(), 1u);
            BOOST_REQUIRE(!presence[elem]);
            presence[elem] = true;
        }
        else {
            BOOST_REQUIRE_EQUAL(clust->size(), 4u);
            ITERATE (CClusterer::TSingleCluster, it, *clust) {
                BOOST_REQUIRE(!presence[*it]);
                presence[*it] = true;
            }
        }
    }

    ITERATE (vector<bool>, it, presence) {
        BOOST_REQUIRE(*it);
    }
}

// Test incremental clustering: first cluster a set of links, then cluster
// new set of links using the clusters as a staring point
BOOST_AUTO_TEST_CASE(TestIncremental)
{
    const int kNumElements = 10;

    // create links
    CRef<CLinks> links(new CLinks(kNumElements));
    links->AddLink(0, 1, 0.0);
    links->AddLink(5, 6, 0.0);

    // run first clustering
    auto_ptr<CClusterer> clusterer(new CClusterer(links));
    clusterer->SetReportSingletons(false);
    clusterer->Run();

    // save clusters from the first round
    CClusterer::TClusters clusters;
    clusters.swap(clusterer->SetClusters());

    // create new set of links
    links.Reset(new CLinks(kNumElements));
    links->AddLink(0, 2, 0.0);
    links->AddLink(1, 2, 0.0);
    links->AddLink(5, 8, 0.0);

    // reset clusterer, set new links, pre-computed clusters and run clustering
    clusterer.reset(new CClusterer(links));
    clusterer->SetClusters().swap(clusters);
    clusterer->Run();

    CClusterer::TClusters& result = clusterer->SetClusters();

    // these should be sizes of resulting clusters, indexed by the smalles
    // element of each cluster
    vector<size_t> sizes(kNumElements, 0);
    sizes[0] = 3;
    sizes[3] = 1;
    sizes[4] = 1;
    sizes[5] = 2;
    sizes[7] = 1;
    sizes[8] = 1;
    sizes[9] = 1;
    
    // the sets must correspond to clusters
    vector< set<int> > reference(kNumElements);
    reference[0].insert(0);
    reference[0].insert(1);
    reference[0].insert(2);

    reference[3].insert(3);
    reference[4].insert(4);

    reference[5].insert(5);
    reference[5].insert(6);

    reference[7].insert(7);
    reference[8].insert(8);
    reference[9].insert(9);

    // check the size and elements of the resulting clusters
    NON_CONST_ITERATE (CClusterer::TClusters, cluster, result) {
        sort(cluster->begin(), cluster->end());
        int cl_index = *cluster->begin();
        BOOST_REQUIRE_EQUAL(cluster->size(), sizes[cl_index]);
        ITERATE (CClusterer::TSingleCluster, elem, *cluster) {
            set<int>::iterator it = reference[cl_index].find(*elem);
            BOOST_REQUIRE(it != reference[cl_index].end());
        }
    }
}


BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
