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
* Author:  Chris Lanczycki
*
* File Description:  $Source$
*
*
*
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include "cuSimpleClusterer.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)



CSimpleClusterer::CSimpleClusterer(TDist clusteringThreshold) : CDistBasedClusterer(clusteringThreshold) {
    InitializeClusterer(0);
}

CSimpleClusterer::CSimpleClusterer(TSlc_DM distances, unsigned int dim, TDist clusteringThreshold) : CDistBasedClusterer(clusteringThreshold) {
    InitializeClusterer(dim);
    m_distances = distances;
}

CSimpleClusterer::CSimpleClusterer(const TDist** distances, unsigned int dim, TDist clusteringThreshold) : CDistBasedClusterer(clusteringThreshold) {
    InitializeClusterer(dim);
    CopyDistances(distances);
}

CSimpleClusterer::~CSimpleClusterer() {
    if (m_ownDistances && m_distances) {
        delete [] m_distances;
    }
}

void CSimpleClusterer::InitializeClusterer(unsigned int dim) {
    m_cuTree = NULL;
    m_ownDistances = false;
    m_distances = NULL;
    m_dim = dim;
    CBaseClusterer::InitializeClusters(m_dim);
}

void CSimpleClusterer::CopyDistances(const TDist** distances) {

    unsigned int k = 0;
    if (m_dim == 0) return;

    m_distances = new TDist[m_dim*(m_dim + 1)/2];
    if (m_distances) {
        m_ownDistances = true;
        for (unsigned int i = 0; i < m_dim; ++i) {
//            k = i*m_dim;
            for (unsigned int j = i; j < m_dim; ++j, ++k) {
                m_distances[k] = distances[i][j];
            }
        }
    }
}

void CSimpleClusterer::InitializeTree() {

    if (m_cuTree) delete m_cuTree;
    m_cuTree = new CUTree();

    CSimpleSLCAlgorithm slcAlg;
    slcAlg.ComputeTree(m_cuTree, m_distances, m_dim);

    if (!m_cuTree) {
        return;
    }

}

//  implementation of virtual function
bool CSimpleClusterer::IsValid() const {
    return (m_distances && m_dim > 0 && m_cuTree);
}

//  implementation of virtual function
unsigned int CSimpleClusterer::MakeClusters(const TDist** distances, unsigned int dim) {

    CopyDistances(distances);
    return MakeClusters();
}


//  implementation of virtual function
unsigned int CSimpleClusterer::MakeClusters() {

    unsigned int result = 0;

    InitializeTree();
    if (!IsValid()) return result;

    TId rowId;
    TCluster newCluster;
    TClusterId currentCluster = 0;
    TDist dtmp = 0;
    CUTree::post_order_iterator treeIt, treeItEnd;

    //  In the underlying tree we're looking at branch lengths, not distances between
    //  the actual leaf nodes, which is what m_clusteringThreshold represents.
    //  The SLC tree is constructed by placing new nodes halfway between existing nodes,
    //  so operationally we will look for branches in the tree cut at 1/2 of the true
    //  clustering threshold specified.
    TDist clusteringThreshold = m_clusteringThreshold/2.0;
    ERR_POST(ncbi::Info << "Clustering threshold (distance units) = " << m_clusteringThreshold << ncbi::Endl());

    if (m_cuTree) {
        treeIt = m_cuTree->begin_post();
        treeItEnd = m_cuTree->end_post();
//        InitializeClusters();  // done in ctor
        while (treeIt != treeItEnd) {
            if (treeIt.number_of_children() == 0) { // a leaf
                rowId = treeIt->id;
                _ASSERT(m_idToClusterMap.count(rowId) == 0);  //  debugging test
                m_idToClusterMap[rowId] = currentCluster;

                //  add leaf to current cluster
//                TItem rowItem(rowId);
//                m_clusters[currentCluster].insert(TClusterVT(rowId,rowItem));
                m_clusters[currentCluster].insert(rowId);

                //  advance to next cluster id if threshold cuts this branch of the tree
                if (clusteringThreshold < treeIt->distance) {
                    ++currentCluster;
                    m_clusters.push_back(newCluster);
                }
                dtmp = treeIt->distance;
            } else {  // an internal node
                //  advance to next cluster id if threshold cuts this internal node from the tree
                if (dtmp <= clusteringThreshold && clusteringThreshold < dtmp + treeIt->distance) {
                    ++currentCluster;
                    m_clusters.push_back(newCluster);
                }
                dtmp += treeIt->distance;
		    }
            ++treeIt;
	    }
        result = m_clusters.size();
    }

    return result;
}

/*
void CSimpleClusterer::SetRootednessForTree(TreeAlgorithm::Rootedness rootedness) {
    m_rootedness = rootedness;
}

TreeAlgorithm::Rootedness CSimpleClusterer::GetRootednessForTree() const {
    return m_rootedness;
}
*/


END_SCOPE(cd_utils)
END_NCBI_SCOPE
