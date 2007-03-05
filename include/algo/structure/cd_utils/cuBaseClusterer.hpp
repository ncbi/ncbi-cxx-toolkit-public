#ifndef CU_BASECLUSTERER__HPP
#define CU_BASECLUSTERER__HPP

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

#include <vector>
#include <map>
#include <set>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT CBaseClusterer {

public:

    typedef unsigned int TId;
    static const TId INVALID_ID = kMax_Int;

    typedef unsigned int TClusterId;  // cluster ids are zero-based
    static const TClusterId INVALID_CLUSTER_ID;

    // typedefs for individual clusters
    typedef set< TId > TCluster;
    typedef TCluster::iterator TClusterIt;
    typedef TCluster::const_iterator TClusterCit;
    typedef TCluster::value_type TClusterVT;

    // typedefs for the collection of all clusters
    typedef vector< TCluster > TClusters;
    typedef TClusters::iterator TClustersIt;
    typedef TClusters::const_iterator TClustersCit;

    // typedefs for inverse mapping of an item's id to its cluster's id;
    // for convenience and to avoid continual searching through the clusters.
    typedef map< TId, TClusterId > TIdToCluster;
    typedef TIdToCluster::iterator TIdToClusterIt;
    typedef TIdToCluster::const_iterator TIdToClusterCit;
    typedef TIdToCluster::value_type TIdToClusterVT;

protected:
    //  Do not allow instances to be created.
    CBaseClusterer() {};

public:
    virtual ~CBaseClusterer() {};

    //  Run a subclass-specific clustering algorithm on a set of TId's.
    virtual unsigned int MakeClusters() = 0;

    //  Return a string for the indicated id.  By default, simply return it's id.
    virtual string IdToString(const TId& id) const;

    //  Provide a class-specific way to tell if the clusterer is in a usable state.
    virtual bool IsValid() const = 0;

    //  Return the total number of items in all clusters.
    unsigned int NumItems() const;

    bool  HasClusters() const;
    bool  IsValidClusterId(TClusterId clusterId) const;
    const TCluster& GetCluster(TClusterId clusterId) const;
    bool GetCluster(TClusterId clusterId, TCluster*& cluster);
    //  if 'item' is invalid, returns INVALID_CLUSTER_ID
    TClusterId GetClusterForId(TId itemId, TCluster*& cluster);

    TClusters& SetClusters();
    const TClusters& GetClusters() const;
    unsigned int NumClusters() const;
    void  ResetClusters(unsigned int dim = 0);

protected:

    void InitializeClusters(unsigned int dim = 0);

    TClusters       m_clusters;
    TIdToCluster    m_idToClusterMap;
};


///////////////////////////////////////////
//  Subclass for distance-based clustering.
///////////////////////////////////////////

class NCBI_CDUTILS_EXPORT CDistBasedClusterer : public CBaseClusterer {

public:

    typedef double TDist;

    //  ** Some classes will only need to override one of the two forms below **

    //  This version is to support a simpler API for derived classes that already have an
    //  internal representation of the distance matrix, and just need to know the clustering threshold.
    //  Default implementation is to return 0; only override in derived classes where needed.
    virtual unsigned int MakeClusters() {
        return 0;
    }

    //  Cluster the square array of distances into groups segmented by the clusteringThreshold.
    //  Return the number of clusters (must be in [1, dim], where dim is dimension of the array),
    //  or zero as an error indicator.

    //  This version is to support derived classes that do not store an internal representation
    //  of the distance matrix.
    //  Default implementation is to return 0; only override in derived classes where needed.
    virtual unsigned int MakeClusters(const TDist** distances, unsigned int dim) {
        return 0;
    }

public:
    virtual ~CDistBasedClusterer() {};

    //  Provide a class-specific way to tell if the clusterer is in a usable state.
    virtual bool IsValid() const = 0;

    void  SetClusteringThreshold(TDist threshold);
    TDist GetClusteringThreshold() const;

protected:

    TDist           m_clusteringThreshold;

    //  Do not allow instances to be created.
    CDistBasedClusterer(TDist clusteringThreshold = 0) : m_clusteringThreshold(clusteringThreshold) {};

};

inline
bool CBaseClusterer::HasClusters() const {
    return (m_clusters.size() > 0);
}

inline
bool  CBaseClusterer::IsValidClusterId(TClusterId clusterId) const {
    return (clusterId < m_clusters.size());
}

inline
CBaseClusterer::TClusters& CBaseClusterer::SetClusters() {
    return m_clusters;
}

inline
const CBaseClusterer::TClusters& CBaseClusterer::GetClusters() const {
    return m_clusters;
}

inline
unsigned int CBaseClusterer::NumClusters() const {
    return m_clusters.size();
}

inline
void CDistBasedClusterer::SetClusteringThreshold(CDistBasedClusterer::TDist threshold) {
    m_clusteringThreshold = threshold;
}


inline
CDistBasedClusterer::TDist CDistBasedClusterer::GetClusteringThreshold() const {
    return m_clusteringThreshold;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif  /* CU_SEQCLUSTERER__HPP */
