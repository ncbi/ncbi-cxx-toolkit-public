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
* File Description:
*   Virtual base classes to perform clustering.  The first is completely an API.
*   The second is for distance-based clustering using an algorithm defined in the class.
*   Clusters are constructed in terms of the indices of the underlying distance matrix.
*   The implementing & calling classes are responsible for consistent interpretation
*   of that index.
*
*/

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuBaseClusterer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const CBaseClusterer::TClusterId CBaseClusterer::INVALID_CLUSTER_ID = 1234567890;

//  default implementation of virtual function
string CBaseClusterer::IdToString(const TId& id) const {
    return "ID " + NStr::IntToString(id);
}

const CBaseClusterer::TCluster& CBaseClusterer::GetCluster(TClusterId clusterId) const {
    return m_clusters[clusterId];
}

bool CBaseClusterer::GetCluster(TClusterId clusterId, TCluster*& cluster) {
    bool result = false;
    cluster = NULL;
    if (clusterId != INVALID_CLUSTER_ID && clusterId < m_clusters.size()) {
        cluster = &(m_clusters[clusterId]);
        result = true;
    }
    return result;
}

CBaseClusterer::TClusterId CBaseClusterer::GetClusterForId(TId itemId, TCluster*& cluster) {
    TClusterId result = INVALID_CLUSTER_ID;
    TIdToClusterCit itemIt = m_idToClusterMap.find(itemId);
    cluster = NULL;
    if (itemIt != m_idToClusterMap.end()) {
        result = itemIt->second;
        if (result != INVALID_CLUSTER_ID) {
            cluster = &(m_clusters[result]);
        }
    }
    return result;
}

void CBaseClusterer::InitializeClusters(unsigned int dim) {
    if (dim <=0) dim = 1;     //  start off w/ at least one cluster
    m_idToClusterMap.clear();
    m_clusters.clear();
    m_clusters.resize(dim);

}

unsigned int CBaseClusterer::NumItems() const {
    unsigned int nItems = 0;
    for (unsigned int i = 0; i < m_clusters.size(); ++i) {
        nItems += m_clusters[i].size();
    }
    return nItems;
}

void CBaseClusterer::ResetClusters(unsigned int dim) {
    if (dim > 0) {
        InitializeClusters(dim);
    }
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
