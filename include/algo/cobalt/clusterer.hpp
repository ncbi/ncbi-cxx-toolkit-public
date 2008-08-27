#ifndef ALGO_COBALT___CLUSTER_METHODS__HPP
#define ALGO_COBALT___CLUSTER_METHODS__HPP

/* $Id$
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

File name: clusterer.hpp

Author: Greg Boratyn

Contents: Interface for CClusterer class

******************************************************************************/


#include <corelib/ncbidbg.hpp>
#include <util/math/matrix.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Interface for CClusterer class used for clustering any type of data based
/// on distance matrix. The class operates on ideces in the distance matrix.
class CClusterer
{
public:

    typedef CNcbiMatrix<double> TDistMatrix;

    /// Single cluster
    class CSingleCluster 
    {
    public:
    typedef vector<int>::const_iterator const_iterator;
    typedef vector<int>::iterator iterator;

    public:
    /// Create empty cluster
    ///
    CSingleCluster(void) : m_Prototype(-1) {}

    /// Add element to the cluster
    /// @param el Index of an element
    ///
    void AddElement(int el) {m_Elements.push_back(el);}

    /// Removes all elements from the cluster
    ///
    void clear(void) {m_Elements.clear();}

    /// Set cluster prototype
    /// @param el Index of element to be used as cluster prototype
    ///
    /// Prototype does not have to belong to the cluster
    void SetPrototype(int el) {m_Prototype = el;}

    /// Get cluster prototype
    /// @return Index of element used as cluster prototype
    ///
    int GetPrototype(void) const {return m_Prototype;}

    /// Get cluster size
    /// @return Number of elements in the cluster
    ///
    size_t size(void) const {return m_Elements.size();}

    /// Find element that is closest to the center of the cluster
    /// @param dmatrix Full distance matrix used to cluster data
    /// @return Index of element that has smallest distance to all other
    /// elements
    ///
    int FindCenterElement(const TDistMatrix& dmatrix) const;

    /// Get element
    /// @param index Element index
    /// @return Element
    ///
    int operator[](size_t index) const
    {_ASSERT(index < m_Elements.size()); return m_Elements[index];}

    // Iterators

    const_iterator begin(void) const {return m_Elements.begin();}
    const_iterator end(void) const {return m_Elements.end();}
    iterator begin(void) {return m_Elements.begin();}
    iterator end(void) {return m_Elements.end();}

    protected:
    int m_Prototype;          ///< Index of cluster representative element
    vector<int> m_Elements;   ///< List of indeces of cluster elements
    };

    typedef CSingleCluster TSingleCluster;
    typedef vector<TSingleCluster> TClusters;

public:

    /// Create empty clusterer
    ///
    CClusterer(void) {}

    /// Create clusterer
    /// @param dmat Distance matrix
    ///
    CClusterer(const TDistMatrix& dmat) : m_DistMatrix(new TDistMatrix(dmat)) {}

    /// Create clusterer
    /// @param dmat Pointer to distance matrix 
    ///
    CClusterer(auto_ptr<TDistMatrix>& dmat) : m_DistMatrix(dmat) {}

    /// Set new distance matrix
    /// @param dmat Distance matrix
    ///
    void SetDistMatrix(const TDistMatrix& dmat);

    /// Set new distance matrix without copying
    /// @param dmat Distance matrix
    ///
    void SetDistMatrix(auto_ptr<TDistMatrix>& dmat);

    /// Get distance matrix
    /// @return Distance matrix
    ///
    const TDistMatrix& GetDistMatrix(void) const 
    {_ASSERT(m_DistMatrix.get()); return *m_DistMatrix;}

    /// Compute clusters
    /// @param max_dim Maximum distance between two elements in a cluster
    ///
    void ComputeClusters(double max_diam);

    //    void ComputeClustersEx(double max_diam);

    /// Get list of elements of a specified cluster
    /// @param index Cluster index
    /// @return list of element indeces that belong to the cluster
    ///
    const TSingleCluster& GetSingleCluster(size_t index) const
    {_ASSERT(index < m_Clusters.size()); return m_Clusters[index];}

    /// Get clusters
    /// @return list of clusters
    ///
    TClusters& GetClusters(void) {return m_Clusters;}

    /// Set prototypes for all clusters as center elements
    ///
    void SetPrototypes(void);

    /// Delete distance matrix
    ///
    void PurgeDistMatrix(void) {m_DistMatrix.reset();}

protected:
    auto_ptr<TDistMatrix> m_DistMatrix;
    TClusters m_Clusters;
};


END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif /* ALGO_COBALT___CLUSTER_METHODS__HPP */

