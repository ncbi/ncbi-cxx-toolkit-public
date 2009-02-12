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


USING_NCBI_SCOPE;
USING_SCOPE(cobalt);

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

