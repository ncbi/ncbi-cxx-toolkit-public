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

static void s_CheckDistMatrix(const CClusterer::TDistMatrix& dmat)
{
    if (dmat.GetRows() != dmat.GetCols()) {
        NCBI_THROW(CClustererException, eInvalidOptions,
                   "Distance matrix is not square");
    }
}

CClusterer::CClusterer()
{
    x_Init();
}

CClusterer::CClusterer(const CClusterer::TDistMatrix& dmat)
    : m_DistMatrix(new TDistMatrix(dmat))
{
    s_CheckDistMatrix(*m_DistMatrix);
    x_Init();
}

CClusterer::CClusterer(auto_ptr<CClusterer::TDistMatrix>& dmat)
    : m_DistMatrix(dmat)
{
    s_CheckDistMatrix(*m_DistMatrix);
    x_Init();
}

CClusterer::CClusterer(CRef<CLinks> links) : m_Links(links)
{
    x_Init();
}

static void s_PurgeTrees(vector<TPhyTreeNode*>& trees)
{
    NON_CONST_ITERATE(vector<TPhyTreeNode*>, it, trees) {
        delete *it;
        *it = NULL;
    }
    trees.clear();
}

CClusterer::~CClusterer()
{
    s_PurgeTrees(m_Trees);
}

void CClusterer::x_Init(void)
{
    m_MaxDiameter = 0.8;
    m_LinkMethod = eClique;
    m_MakeTrees = false;
}

void CClusterer::SetDistMatrix(const TDistMatrix& dmat)
{
    s_CheckDistMatrix(dmat);

    m_DistMatrix.reset(new TDistMatrix());
    m_DistMatrix->Resize(dmat.GetRows(), dmat.GetCols());
    copy(dmat.begin(), dmat.end(), m_DistMatrix->begin());
}

void CClusterer::SetDistMatrix(auto_ptr<TDistMatrix>& dmat)
{
    s_CheckDistMatrix(*dmat);

    m_DistMatrix = dmat;
}

void CClusterer::SetLinks(CRef<CLinks> links)
{
    m_Links = links;
}

const CClusterer::TDistMatrix& CClusterer::GetDistMatrix(void) const
{
    if (!m_DistMatrix.get()) {
        NCBI_THROW(CClustererException, eNoDistMatrix,
                   "Distance matrix not assigned");
    }

    return *m_DistMatrix;
}


const CClusterer::TSingleCluster&
CClusterer::GetSingleCluster(size_t index) const
{
    if (index >= m_Clusters.size()) {
        NCBI_THROW(CClustererException, eClusterIndexOutOfRange,
                   "Cluster index out of range");
    }

    return m_Clusters[index];
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
static double s_FindDistAsMax(const CClusterer::TSingleCluster& cluster1,
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

/// Find sum of distances between given element and cluster elements
static double s_FindSumDistFromElem(int elem, 
                                    const CClusterer::TSingleCluster& cluster, 
                                    const CClusterer::TDistMatrix& dmat) {

    _ASSERT(cluster.size() > 0);
    _ASSERT(elem < (int)dmat.GetRows());

    double result = 0.0;
    ITERATE(CClusterer::TSingleCluster, elem_it, cluster) {
            _ASSERT(*elem_it < (int)dmat.GetRows());

            result += dmat(elem, *elem_it);
    }

    return result;
}

/// Find mean distance between cluster elements
static double s_FindDistAsMean(const CClusterer::TSingleCluster& cluster1,
                               const CClusterer::TSingleCluster& cluster2,
                               const CClusterer::TDistMatrix& dmat)
{
    _ASSERT(cluster1.size() > 0 && cluster2.size() > 0);

    double result = 0.0;
    ITERATE(CClusterer::TSingleCluster, elem, cluster1) {

        _ASSERT(*elem < (int)dmat.GetRows());

        result += s_FindSumDistFromElem(*elem, cluster2, dmat);
    }
    result /= (double)(cluster1.size() * cluster2.size());

    return result;    
}


/// Find mean
static double s_FindMean(const vector<double>& vals)
{
    double result = 0.0;
    if (!vals.empty()) {
        ITERATE(vector<double>, it, vals) {
            result += *it;
        }
        result /= (double)vals.size();
    }
    return result;
}

/// Find distance between two clusters 
/// (cluster and included_cluster + extended_cluster) using given dist method.
/// current_dist is the current distance between cluster and extended_cluster.
static double s_FindDist(const CClusterer::CSingleCluster& cluster,
                         const CClusterer::CSingleCluster& included_cluster,
                         const CClusterer::CSingleCluster& extended_cluster,
                         const CClusterer::TDistMatrix& dmat,
                         double current_dist,
                         CClusterer::EDistMethod dist_method)
{
    double result = -1.0;
    if (dist_method == CClusterer::eCompleteLinkage) {
        double dist = s_FindDistAsMax(cluster, included_cluster, dmat);
        
        result = dist > current_dist ? dist : current_dist;
    
    }
    else {
        
        result = s_FindDistAsMean(cluster, extended_cluster, dmat);
    }
    return result;
}

// Create tree leaf (node representing element) with given id
TPhyTreeNode* s_CreateTreeLeaf(int id)
{
    TPhyTreeNode* node = new TPhyTreeNode();
    node->GetValue().SetId(id);

    // This is needed so that serialized tree can be interpreted
    // by external applications
    node->GetValue().SetLabel(NStr::IntToString(id));

    return node;
}

// Complete Linkage clustering with dendrograms
void CClusterer::ComputeClusters(double max_diam,
                                 CClusterer::EDistMethod dist_method,
                                 bool do_trees,
                                 double infinity)
{
    if (dist_method != eCompleteLinkage && dist_method != eAverageLinkage) {
        NCBI_THROW(CClustererException, eInvalidOptions, "Unrecognised cluster"
                   " distance method");
    }

    if (!m_DistMatrix.get()) {
        NCBI_THROW(CClustererException, eInvalidInput, "Distance matrix not"
                   " set");
    }

    m_Clusters.clear();
    m_Trees.clear();

    size_t num_elements = m_DistMatrix->GetRows();

    // If there is exactly one element to cluster
    if (num_elements == 1) {
        m_Clusters.resize(1);
        m_Clusters[0].AddElement(0);

        if (do_trees) {
            m_Trees.push_back(s_CreateTreeLeaf(0));
        }

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

        if (do_trees) {
            TPhyTreeNode* node0 = s_CreateTreeLeaf(0);
            TPhyTreeNode* node1 = s_CreateTreeLeaf(1);

            if ((*m_DistMatrix)(0, 1) < max_diam) {
                // one cluster case
                double dist = (*m_DistMatrix)(0, 1) / 2.0;
                node0->GetValue().SetDist(dist);
                node1->GetValue().SetDist(dist);

                TPhyTreeNode* root = new TPhyTreeNode();
                root->AddNode(node0);
                root->AddNode(node1);

                m_Trees.push_back(root);

            }
            else {
                // two clusters case
                m_Trees.push_back(node0);
                m_Trees.push_back(node1);
            }
        }

        return;
    }

    // If there are at least 3 elements to cluster

    // Checking whether the data is in one cluster
    // skip if tree is requested
    if (!do_trees) {
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
    }


    // Getting to this point means that there are at least two clusters
    // and max distance is larger than max_diam
    // or tree was requested

    // Creating working copy of dist matrix
    TDistMatrix dmat(*m_DistMatrix);
    // Denotes whether an entry in dist matrix and clusters is used
    vector<bool> used_entry(num_elements, true);

    // Creating initial one-element clusters 
    TClusters clusters(num_elements);
    for (size_t i=0;i < num_elements;i++) {
        clusters[i].AddElement((int)i);
    }

    // Create leaf nodes for dendrogram
    vector<TPhyTreeNode*> nodes(num_elements);
    if (do_trees) {
        for (size_t i=0;i < nodes.size();i++) {
            nodes[i] = s_CreateTreeLeaf(i);
        }
    }
    vector< vector<double> > dists_to_root(num_elements);

    // Computing clusters
    // Exits the loop once minimum distance is larger than max_diam
    // It was checked above that such distance exists
    int num_clusters = num_elements;
    while (num_clusters > 1) {

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
        _ASSERT(do_trees || (min_i < num_elements && min_j < num_elements));

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

        // If distance between two one-element clusters exceeds
        // infinity, they should not start a cluster.
        // Try attaching one of the elements to a cluster with at least two
        // elements.
        if (infinity >= 0.0 && dmat(min_i, min_j) >= infinity
            && clusters[min_i].size() == 1 && clusters[min_j].size() == 1) {

            // find first used entry
            size_t new_min_j = 0;
            while ((!used_entry[new_min_j] || new_min_j == min_j
                    || new_min_j == min_i 
                    || clusters[new_min_j].size() == 1)
                   && new_min_j < num_elements) {

                new_min_j++;
            }
            if (new_min_j < num_elements) {

                // find new smallest distance
                for (size_t j=new_min_j+1;j < num_elements;j++) {
                    if (!used_entry[j] || clusters[j].size() == 1
                        || j == min_j || j == min_i) {
                        continue;
                    }
                    
                    if (dmat(min_i, j) < dmat(min_i, new_min_j)) {
                        new_min_j = j;
                    }

                }
                // if the new pair cannot be found keep the old min_j
                if (new_min_j < num_elements) {
                    min_j = new_min_j;
                    if (min_i > min_j) {
                        swap(min_i, min_j);
                    }
                }
            }
            // TO DO: Generate a warning if new min_j is not found
        }


        _ASSERT(clusters[min_i].size() > 0 && clusters[min_j].size() > 0);
        _ASSERT(min_i < num_elements && min_j < num_elements);

        // Joining tree nodes
        // must be done before joining clusters
        if (do_trees) {
            TPhyTreeNode* new_root = new TPhyTreeNode();
            
            _ASSERT(nodes[min_i]);
            _ASSERT(nodes[min_j]);

            // left and right are meaningless, only to make code readble
            const int left = min_i;
            const int right = min_j;

            new_root->AddNode(nodes[left]);
            new_root->AddNode(nodes[right]);

            // set sub node distances
            double dist = s_FindDistAsMean(clusters[left], clusters[right],
                                           *m_DistMatrix);

            // find average distances too root in left and right subtrees 
            double mean_dist_to_root_left = s_FindMean(dists_to_root[left]);
            double mean_dist_to_root_right = s_FindMean(dists_to_root[right]);
            
            // set edge length between new root and subtrees
            double left_edge_length = dist - mean_dist_to_root_left;
            double right_edge_length = dist - mean_dist_to_root_right;
            left_edge_length = left_edge_length > 0.0 ? left_edge_length : 0.0;
            right_edge_length 
                = right_edge_length > 0.0 ? right_edge_length : 0.0;

            nodes[left]->GetValue().SetDist(left_edge_length);
            nodes[right]->GetValue().SetDist(right_edge_length);
            
            // compute distances from leaves to new root
            if (dists_to_root[left].empty()) {
                dists_to_root[left].push_back(dist);
            }
            else {
                NON_CONST_ITERATE(vector<double>, it, dists_to_root[left]) {
                    *it += left_edge_length;
                }
            }

            if (dists_to_root[right].empty()) {
                dists_to_root[right].push_back(dist);
            }
            else {
                NON_CONST_ITERATE(vector<double>, it, dists_to_root[right]) {
                    *it += right_edge_length;
                }
            }

            // merge cluster related information
            // Note: the extended and included cluster must correspond to
            // Joining clusters procedure below
            nodes[min_i] = new_root;
            nodes[min_j] = NULL;
            ITERATE(vector<double>, it, dists_to_root[min_j]) {
                dists_to_root[min_i].push_back(*it);
            }
            dists_to_root[min_j].clear();
        }

        // Joining clusters
        TSingleCluster& extended_cluster = clusters[min_i];
        TSingleCluster& included_cluster = clusters[min_j];
        used_entry[min_j] = false;
        num_clusters--;
        _ASSERT(!do_trees || !nodes[min_j]);

        ITERATE(TSingleCluster, elem, included_cluster) {
            extended_cluster.AddElement(*elem);
        }
        
        // Updating distance matrix
        for (size_t i=0;i < min_i && min_i > 0;i++) {
            if (!used_entry[i]) {
                continue;
            }

            dmat(i, min_i) = s_FindDist(clusters[i], included_cluster,
                                        extended_cluster, *m_DistMatrix,
                                        dmat(i, min_i), dist_method);

            dmat(min_i, i) = dmat(i, min_i);

        }
        for (size_t j=min_i+1;j < num_elements;j++) {
            if (!used_entry[j]) {
                continue;
            }

            dmat(min_i, j) = s_FindDist(clusters[j], included_cluster,
                                        extended_cluster, *m_DistMatrix,
                                        dmat(min_i, j), dist_method);

            dmat(j, min_i) = dmat(min_i, j);

        }
        clusters[min_j].clear();
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

            if (do_trees) {
                _ASSERT(nodes[i]);

                m_Trees.push_back((TPhyTreeNode*)nodes[i]);
            }
        }
        else {

            _ASSERT(clusters[i].size() == 0);

            if (do_trees) {
                _ASSERT(!nodes[i]);
                _ASSERT(dists_to_root[i].empty());
            }
        }
    }
}

void CClusterer::ComputeClustersFromLinks(void)
{
    if (m_Links.Empty()) {
        NCBI_THROW(CClustererException, eInvalidOptions, "Distance links not"
                   " set");
    }

    // initialize table of cluster ids for elements to unassigned
    m_ClusterId.resize(m_Links->GetNumElements(), -1);

    // sort links
    if (!m_Links->IsSorted()) {
        m_Links->Sort();
    }

    // for each link
    CLinks::SLink_CI it = m_Links->begin();
    for (;it != m_Links->end();++it) {

        _ASSERT(it->first != it->second);

        // if none of elements belongs to a cluster
        if (m_ClusterId[it->first] < 0 && m_ClusterId[it->second] < 0) {

            // join these two elements
            x_JoinElements(*it);
            continue;
        }

        // if cluster ids differ
        if (m_ClusterId[it->first] != m_ClusterId[it->second]) {

            // if one of elements does not belong to any cluster
            if (m_ClusterId[it->first] < 0 || m_ClusterId[it->second] < 0) {
                int elem;
                int cluster_id;
            
                if (m_ClusterId[it->first] < 0) {
                    elem = it->first;
                    cluster_id = m_ClusterId[it->second];
                }
                else {
                    elem = it->second;
                    cluster_id = m_ClusterId[it->first];
                }

                // check if element can be added to the cluster the other
                // element belongs
                double distance;
                if (x_CanAddElem(cluster_id, elem, distance)) {
                    // add element
                    x_JoinClustElem(cluster_id, elem, distance);
                }
                
            }
            // if both elements belong to clusters
            else {
                // if these clusters can be joined
                double distance;
                if (x_CanJoinClusters(m_ClusterId[it->first],
                                      m_ClusterId[it->second],
                                      distance)) {
                    
                    // join clusters
                    x_JoinClusters(m_ClusterId[it->first],
                                   m_ClusterId[it->second],
                                   distance);
                }
            }
        }
        // if both elements already belong to the same cluster
        else {
            // update value of cluster diameter
            int cluster_id = m_ClusterId[it->first];
            if (m_Clusters[cluster_id].GetMaxDistance() < it->weight) {
                m_Clusters[cluster_id].SetMaxDistance(it->weight);
            }
        }
    }

    // find elements that were not assigned to any clusters and create
    // one-element clusters for them
    for (int i=0;i < (int)m_ClusterId.size();i++) {
        if (m_ClusterId[i] < 0) {
            x_CreateCluster(i);
        }
    }

    // TO DO: m_Clusters needs to be chenched to vector of pointers or list
    // so that the operations below are more efficient

    // remove empty clusters from the back of the list of clusters
    // the empty clusters appear in the list when clusters are joined
    while (m_Clusters.back().size() == 0) {
        m_Clusters.pop_back();
    }

    // remove empty clusters from the list of clusters by moving the cluster
    // from the back of the list to the position of the empty one
    for (int i=0;i < (int)m_Clusters.size();i++) {
        if (m_Clusters[i].size() == 0) {
            const TSingleCluster& cluster = m_Clusters.back();
            _ASSERT(cluster.size() > 0);
            ITERATE (vector<int>, it, cluster) {
                m_Clusters[i].AddElement(*it);
                m_ClusterId[*it] = i;
                m_Clusters[i].m_Tree = cluster.m_Tree;
                _ASSERT(m_Clusters[i].m_Tree);
            }
            m_Clusters.pop_back();

            // remove empty clusters from the back of the list of clusters
            // the empty clusters appear in the list when clusters are joined
            while (m_Clusters.back().size() == 0) {
                m_Clusters.pop_back();
            }
        }        
    }

    if (m_MakeTrees) {
        m_Trees.reserve(m_Clusters.size());
        ITERATE (vector<CSingleCluster>, it, m_Clusters) {
            _ASSERT(it->m_Tree);
            m_Trees.push_back(it->m_Tree);
        }
    }
}


void CClusterer::x_JoinElements(const CLinks::SLink& link)
{
    int cluster_id;

    // if the list of unused entries in the cluster list is empty
    if (m_UnusedEntries.empty()) {

        // add a new cluster to the list
        m_Clusters.push_back(CSingleCluster());        
        CSingleCluster& cluster = m_Clusters.back();
        cluster.AddElement(link.first);
        cluster.AddElement(link.second);
        cluster.SetMaxDistance(link.weight);
        cluster_id = (int)m_Clusters.size() - 1;        
    }
    else {

        // if there are any empty clusters in the list (created by joining two
        // clusters)
        
        // use the empty cluster enrty in the list for joining the elements
        cluster_id = m_UnusedEntries.front();
        m_UnusedEntries.pop_front();
        _ASSERT(m_Clusters[cluster_id].size() == 0);
        CSingleCluster& cluster = m_Clusters[cluster_id];
        cluster.AddElement(link.first);
        cluster.AddElement(link.second);
        cluster.SetMaxDistance(link.weight);
    }

    // set cluster id for elements
    m_ClusterId[link.first] = cluster_id;
    m_ClusterId[link.second] = cluster_id;

    if (!m_MakeTrees) {
        return;
    }

    // join tree nodes
    TPhyTreeNode* root = new TPhyTreeNode();
    TPhyTreeNode* left = s_CreateTreeLeaf(link.first);
    TPhyTreeNode* right = s_CreateTreeLeaf(link.second);
    root->AddNode(left);
    root->AddNode(right);
    double dist = link.weight / 2.0;
    left->GetValue().SetDist(dist);
    right->GetValue().SetDist(dist);

    // set cluster tree and average leaf distance to root
    _ASSERT(!m_Clusters[cluster_id].m_Tree);
    m_Clusters[cluster_id].m_Tree = root;
    m_Clusters[cluster_id].m_DistToRoot.push_back(dist);
    m_Clusters[cluster_id].m_DistToRoot.push_back(dist);
}

void CClusterer::x_CreateCluster(int elem)
{
    int cluster_id;

    // if the list of unused entries in the cluster list is empty
    if (m_UnusedEntries.empty()) {

        // add a new cluster to the list
        m_Clusters.push_back(CSingleCluster());
        CSingleCluster& cluster = m_Clusters.back();
        cluster.AddElement(elem);
        cluster_id = m_Clusters.size() - 1;
    }
    else {

        // if there are any empty clusters in the list (created by joining two
        // clusters)
        
        // use the empty cluster enrty in the list for joining the elements
        cluster_id = m_UnusedEntries.front();
        m_UnusedEntries.pop_front();
        CSingleCluster& cluster = m_Clusters[cluster_id];
        cluster.AddElement(elem);
    }

    // set cluster id for the element
    m_ClusterId[elem] = cluster_id;

    if (m_MakeTrees) {
        _ASSERT(!m_Clusters[cluster_id].m_Tree);
        m_Clusters[cluster_id].m_Tree = s_CreateTreeLeaf(elem);
    }    
}

void CClusterer::x_JoinClustElem(int cluster_id, int elem, double distance)
{
    m_Clusters[cluster_id].AddElement(elem);
    m_ClusterId[elem] = cluster_id;

    if (!m_MakeTrees) {
        return;
    }

    _ASSERT(m_Clusters[cluster_id].m_Tree);
    
    // attach the new node to cluster tree
    TPhyTreeNode* root = new TPhyTreeNode();
    TPhyTreeNode* old_root = m_Clusters[cluster_id].m_Tree;
    TPhyTreeNode* node = s_CreateTreeLeaf(elem);

    root->AddNode(old_root);
    root->AddNode(node);

    // find average leaf distance to root
    double sum_dist = 0.0;
    ITERATE (vector<double>, it, m_Clusters[cluster_id].m_DistToRoot) {
        sum_dist += *it;
    }
    double ave_dist_to_root
        = sum_dist / (double)m_Clusters[cluster_id].m_DistToRoot.size();

    // set edge lengths for subtrees
    double d = (distance - ave_dist_to_root) / 2.0;

    old_root->GetValue().SetDist(d > 0.0 ? d : 0.0);
    node->GetValue().SetDist(d > 0.0 ? d : 0.0);
    
    // set new root for cluster
    m_Clusters[cluster_id].m_Tree = root;

    // update leaf distances to root
    NON_CONST_ITERATE (vector<double>, it, m_Clusters[cluster_id].m_DistToRoot) {
        *it += d;
    }
    m_Clusters[cluster_id].m_DistToRoot.push_back(d);
}


bool CClusterer::x_CanAddElem(int cluster_id, int elem, double& dist) const
{
    // for the link method, clusters and elements can be joined as long
    // as there exists at least one link between elements
    if (m_LinkMethod == eDist) {
        return true;
    }

    // for the clique method there must be a link between all pairs of elements
    if (m_MakeTrees) {
        vector<int> el(1, elem);
        return m_Links->IsLink(m_Clusters[cluster_id].GetElements(), el, dist);
    }
    else {

        ITERATE (vector<int>, it, m_Clusters[cluster_id]) {
            if (!m_Links->IsLink(*it, elem)) {
                return false;
            }
        }

        return true;
    }
}


void CClusterer::x_JoinClusters(int cluster1_id, int cluster2_id, double dist)
{
    CSingleCluster& cluster1 = m_Clusters[cluster1_id];
    CSingleCluster& cluster2 = m_Clusters[cluster2_id];

    if (m_MakeTrees) {

        _ASSERT(cluster1.m_Tree);
        _ASSERT(cluster2.m_Tree);

        // join cluster trees
        TPhyTreeNode* root = new TPhyTreeNode();
        TPhyTreeNode* left = cluster1.m_Tree;
        TPhyTreeNode* right = cluster2.m_Tree;

        root->AddNode(left);
        root->AddNode(right);

        // find edge lengths below new root
        double left_dist_to_root, right_dist_to_root;
        double sum = 0.0;
        ITERATE (vector<double>, it, cluster1.m_DistToRoot) {
            sum += *it;
        }
        left_dist_to_root = sum / (double)cluster1.size();

        sum = 0.0;
        ITERATE (vector<double>, it, cluster2.m_DistToRoot) {
            sum += *it;
        }
        right_dist_to_root = sum / (double)cluster2.size();

        double left_dist = dist - left_dist_to_root;
        double right_dist = dist - right_dist_to_root;

        // set lengths lengths for new edges
        left->GetValue().SetDist(left_dist > 0.0 ? left_dist : 0.0);
        right->GetValue().SetDist(right_dist > 0.0 ? right_dist : 0.0);
    
        // update tree root, the clusters are joint into cluster1 see below
        cluster1.m_Tree = root;

        // update leaf distances to root
        NON_CONST_ITERATE (vector<double>, it, cluster1.m_DistToRoot) {
            *it += left_dist;
        }

        size_t num_elements
            = cluster1.m_DistToRoot.size() + cluster2.m_DistToRoot.size();

        if (cluster1.m_DistToRoot.capacity() < num_elements) {

            cluster1.m_DistToRoot.reserve(num_elements
                                          + (int)(0.3 * num_elements));
        }
        ITERATE (vector<double>, it, cluster2.m_DistToRoot) {
            cluster1.m_DistToRoot.push_back(*it + right_dist);
        }
    }

    // add elements of cluster2 to cluster1
    ITERATE (vector<int>, it, cluster2) {
        cluster1.AddElement(*it);
        m_ClusterId[*it] = cluster1_id;
    }

    // remove all elements and detouch tree of cluster 2
    cluster2.clear();
    cluster2.m_Tree = NULL;
    _ASSERT(!cluster2.m_Tree);

    // mark cluster2 as unused
    m_UnusedEntries.push_back(cluster2_id);
}


bool CClusterer::x_CanJoinClusters(int cluster1_id, int cluster2_id,
                                   double& dist) const
{
    // for the link method, clusters can be joined as long
    // as there exists at least one link between elements
    if (m_LinkMethod == eDist) {
        return true;
    }
    
    // for the clique method there must be a link between all pairs of elements
    if (m_MakeTrees) {
        return m_Links->IsLink(m_Clusters[cluster1_id].GetElements(),
                               m_Clusters[cluster2_id].GetElements(),
                               dist);
    }
    else {

        ITERATE (vector<int>, it1, m_Clusters[cluster1_id]) {
            ITERATE (vector<int>, it2, m_Clusters[cluster2_id]) {
                if (!m_Links->IsLink(*it1, *it2)) {
                    return false;
                }
            }
        }

        return true;
    }
}

int CClusterer::GetClusterId(int elem) const
{
    if (elem < 0 || elem >= m_ClusterId.size()) {
        NCBI_THROW(CClustererException, eInvalidInput, "Element index out of "
                   "range");
    }

    return m_ClusterId[elem];
}


void CClusterer::GetTrees(vector<TPhyTreeNode*>& trees) const
{
    trees.clear();
    ITERATE(vector<TPhyTreeNode*>, it, m_Trees) {
        trees.push_back(*it);
    }
}


void CClusterer::ReleaseTrees(vector<TPhyTreeNode*>& trees)
{
    trees.clear();
    ITERATE(vector<TPhyTreeNode*>, it, m_Trees) {
        trees.push_back(*it);
    }
    m_Trees.clear();
}

#ifdef NCBI_COMPILER_WORKSHOP
// In some configurations, cobalt.o winds up with an otherwise
// undefined reference to a slightly mismangled name!  The compiler's
// own README recommends this workaround for such incidents.
#pragma weak "__1cEncbiGcobaltKCClustererMReleaseTrees6MrnDstdGvector4Cpn0AJCTreeNode4n0AMCPhyNodeData_n0AVCDefaultNodeKeyGetter4n0E_____n0DJallocator4Cp4_____v_" = "__1cEncbiGcobaltKCClustererMReleaseTrees6MrnDstdGvector4Cpn0AJCTreeNode4n0AMCPhyNodeData_n0AVCDefaultNodeKeyGetter4n0E_____n0DJallocator4C5_____v_"
#endif


const TPhyTreeNode* CClusterer::GetTree(int index) const
{
    if (index < 0 || index >= (int)m_Trees.size()) {
        NCBI_THROW(CClustererException, eClusterIndexOutOfRange,
                   "Tree index out of range");
    }

    return m_Trees[index];
}


TPhyTreeNode* CClusterer::ReleaseTree(int index)
{
    if (index < 0 || index >= (int)m_Trees.size()) {
        NCBI_THROW(CClustererException, eClusterIndexOutOfRange,
                   "Tree index out of range");
    }

    TPhyTreeNode* result = m_Trees[index];
    m_Trees[index] = NULL;

    return result;
}


void CClusterer::SetPrototypes(void) {

    NON_CONST_ITERATE(TClusters, cluster, m_Clusters) {
        cluster->SetPrototype(cluster->FindCenterElement(*m_DistMatrix));
    }
}


void CClusterer::GetClusterDistMatrix(int index, TDistMatrix& mat) const
{
    if (index >= (int)m_Clusters.size()) {
        NCBI_THROW(CClustererException, eClusterIndexOutOfRange,
                   "Cluster index out of range");
    }

    const CSingleCluster& cluster = m_Clusters[index];

    mat.Resize(cluster.size(), cluster.size(), 0.0);
    for (size_t i=0;i < cluster.size() - 1;i++) {
        for (size_t j=i+1;j < cluster.size();j++) {

            if (cluster[i] >= (int)m_DistMatrix->GetRows()
                || cluster[j] >= (int)m_DistMatrix->GetRows()) {
                NCBI_THROW(CClustererException, eElementOutOfRange,
                           "Distance matrix size is smaller than number of"
                           " elements");
            }

            mat(i, j) = mat(j, i) = (*m_DistMatrix)(cluster[i], cluster[j]);
        }
    }
}

void CClusterer::Reset(void)
{
    s_PurgeTrees(m_Trees);
    m_Clusters.clear();
    PurgeDistMatrix();
    m_Links.Reset();
}


void CClusterer::Run(void)
{
    if (!m_Links.Empty()) {
        ComputeClustersFromLinks();
    }
    else if (m_DistMatrix.get()) {
        ComputeClusters(m_MaxDiameter);
    }
    else {
        NCBI_THROW(CClustererException, eInvalidInput, "Either distance matrix"
                   " or distance links must be set");
    }
}

//------------------------------------------------------------

int CClusterer::CSingleCluster::operator[](size_t index) const
{
    if (index >= m_Elements.size()) {
        NCBI_THROW(CClustererException, eElemIndexOutOfRange,
                   "Cluster element index out of range");
    }

    return m_Elements[index];
}


int CClusterer::CSingleCluster::FindCenterElement(const TDistMatrix& 
                                                  dmatrix) const
{
    if (m_Elements.empty()) {
        NCBI_THROW(CClustererException, eInvalidOptions, "Cluster is empty");
    }


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


