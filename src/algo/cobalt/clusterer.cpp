/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: clusterer.cpp

Author: Greg Boratyn

Contents: Implementation of CClusterer class

******************************************************************************/


#include <ncbi_pch.hpp>
#include <algo/cobalt/clusterer.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(cobalt);

void CClusterer::SetDistMatrix(const TDistMatrix& dmat)
{
    m_DistMatrix.reset(new TDistMatrix());
    m_DistMatrix->Resize(dmat.GetRows(), dmat.GetCols());
    copy(dmat.begin(), dmat.end(), m_DistMatrix->begin());
}

void CClusterer::SetDistMatrix(auto_ptr<TDistMatrix>& dmat)
{
    m_DistMatrix = dmat;
}

// Finds maximum distance between given element and element in given cluster
static double s_FindMaxDistFromElem(int elem, 
                                    const CClusterer::TSingleCluster& cluster, 
                                    const CClusterer::TDistMatrix& dmat) {

    _ASSERT(cluster.size() > 0);
    _ASSERT(elem < (int)dmat.GetRows());

    double result = 0.0;
    ITERATE(CClusterer::TSingleCluster, elem_it, cluster) {
        if (dmat(elem, *elem_it) > result) {

            _ASSERT(*elem_it < (int)dmat.GetRows());

            result = dmat(elem, *elem_it);
        }
    }

    return result;
}

// Finds maximum distance between any pair of elements from given clusters
static double s_FindClusterDist(const CClusterer::TSingleCluster& cluster1,
                                const CClusterer::TSingleCluster& cluster2,
                                const CClusterer::TDistMatrix& dmat) {

    _ASSERT(cluster1.size() > 0 && cluster2.size() > 0);

    double result = 0.0;
    ITERATE(CClusterer::TSingleCluster, elem, cluster1) {

        _ASSERT(*elem < (int)dmat.GetRows());

        double dist = s_FindMaxDistFromElem(*elem, cluster2, dmat);
        if (dist > result) {
            result = dist;
        }
    }

    return result;
}

// Complete Linkage clustering
void CClusterer::ComputeClusters(double max_diam)
{
    m_Clusters.clear();

    size_t num_elements = m_DistMatrix->GetRows();

    // If there is exactly one element to cluster
    if (num_elements == 1) {
        m_Clusters.resize(1);
        m_Clusters[0].AddElement(0);

        return;
    }

    // If there are exactly two elements
    if (num_elements == 2) {
        if ((*m_DistMatrix)(0, 1) < max_diam) {
            m_Clusters.resize(1);
            m_Clusters[0].AddElement(0);
            m_Clusters[0].AddElement(1);
        }
        else {
            m_Clusters.resize(2);
            m_Clusters[0].AddElement(0);
            m_Clusters[1].AddElement(1);
        }

        return;
    }

    // If there are at least 3 elements to cluster

    // Checking whether the data is in one cluster
    double max_dist = 0.0;
    for (size_t i=0;i < num_elements - 1;i++) {
        for (size_t j=i+1;j < num_elements;j++) {
            if ((*m_DistMatrix)(i, j) > max_dist) {
                max_dist = (*m_DistMatrix)(i, j);
            }
        }
    }
    // If yes, create the cluster and return
    if (max_dist <= max_diam) {
        m_Clusters.resize(1);
        for (int i=0;i < (int)num_elements;i++) {
            m_Clusters[0].AddElement(i);
        }
        return;
    }


    // Getting to this point means that there are at least two clusters
    // and max distance is larger than max_diam

    // Creating working copy of dist matrix
    TDistMatrix dmat(*m_DistMatrix);
    // Denotes whether an entry in dist matrix and clusters is used
    vector<bool> used_entry(num_elements, true);

    // Creating initial one-element clusters 
    TClusters clusters(num_elements);
    for (size_t i=0;i < num_elements;i++) {
        clusters[i].AddElement((int)i);
    }

    // Computing clusters
    // Exits the loop once minimum distance is larger than max_diam
    // It was checked above that such distance exists
    while (true) {

        // Find first used dist matrix entries
	size_t min_i = 0;
	size_t min_j;
	do {
	    while (!used_entry[min_i] && min_i < num_elements) {
		min_i++;
	    }
	
	    min_j = min_i + 1;
	    while (!used_entry[min_j] && min_j < num_elements) {
		min_j++;
	    }

	    if (min_j >= num_elements) {
		min_i++;
	    }
	} while (min_j >= num_elements && min_i < num_elements);

	// A distance larger than max_diam exists in the dist matrix,
	// then there always should be at least two used entires in dist matrix
	_ASSERT(min_i < num_elements && min_j < num_elements);

        // Find smallest distance entry
        for (size_t i=0;i < num_elements - 1;i++) {
            for (size_t j=i+1;j < num_elements;j++) {
                
                if (!used_entry[i] || !used_entry[j]) {
                    continue;
                }

                if (dmat(i, j) < dmat(min_i, min_j)) {
                    min_i = i;
                    min_j = j;
                }            
            }
        }

        _ASSERT(used_entry[min_i] && used_entry[min_j]);

        // Check whether the smallest distance is larger than max_diam
        if (dmat(min_i, min_j) > max_diam) {
            break;
        }

        _ASSERT(clusters[min_i].size() > 0 && clusters[min_j].size() > 0);
        _ASSERT(min_i < num_elements && min_j < num_elements);

        // Joining clusters
        TSingleCluster& extended_cluster = clusters[min_i];
        TSingleCluster& included_cluster = clusters[min_j];
        used_entry[min_j] = false;

        ITERATE(TSingleCluster, elem, included_cluster) {
            extended_cluster.AddElement(*elem);
        }
        
        // Updating distance matrix
        // Distance between clusters is the largest pairwise distance
        for (size_t i=0;i < min_i && min_i > 0;i++) {
            if (!used_entry[i]) {
                continue;
            }

            double dist = s_FindClusterDist(clusters[i], included_cluster, 
                                            *m_DistMatrix);
            if (dist > dmat(i, min_i)) {
                dmat(i, min_i) = dist;
            }
        }
            for (size_t j=min_i+1;j < num_elements;j++) {
            if (!used_entry[j]) {
                continue;
            }

            double dist = s_FindClusterDist(clusters[j], included_cluster,
                                            *m_DistMatrix);
            if (dist > dmat(min_i, j)) {
                dmat(min_i, j) = dist;
            }

        }
        clusters[min_j].clear(); //needed only for debuging
    }

    // Putting result in class attribute
    for (size_t i=0;i < num_elements;i++) {
        if (used_entry[i]) {

            _ASSERT(clusters[i].size() > 0);

            m_Clusters.push_back(TSingleCluster());
            TClusters::iterator it = m_Clusters.end();
            --it;
            ITERATE(TSingleCluster, elem, clusters[i]) {
                it->AddElement(*elem);
            }
        }
    }
}

void CClusterer::SetPrototypes(void) {

    NON_CONST_ITERATE(TClusters, cluster, m_Clusters) {
        cluster->SetPrototype(cluster->FindCenterElement(*m_DistMatrix));
    }
}


//------------------------------------------------------------

int CClusterer::CSingleCluster::FindCenterElement(const TDistMatrix& 
                                                  dmatrix) const
{
    _ASSERT(m_Elements.size() > 0);

    if (m_Elements.size() == 1) {
        return m_Elements[0];
    }

    vector<double> sum_distance;
    sum_distance.resize(m_Elements.size());
    for (size_t i=0;i < m_Elements.size(); i++) {
        double dist = 0.0;
        for (size_t j=0;j < m_Elements.size();j++) {
            if (i == j) {
                continue;
            }

            dist += dmatrix(m_Elements[i], m_Elements[j]);
        }
        sum_distance[i] = dist;
    }

    size_t min_index = 0;
    for (size_t i=1;i < sum_distance.size();i++) {
        if (sum_distance[i] < sum_distance[min_index]) {
            min_index = i;
        }
    }

    return m_Elements[min_index];
}


