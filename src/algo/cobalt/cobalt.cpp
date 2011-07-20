static char const rcsid[] = "$Id$";

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

File name: cobalt.cpp

Author: Jason Papadopoulos

Contents: Implementation of CMultiAligner class

******************************************************************************/


#include <ncbi_pch.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/phy_tree/dist_methods.hpp>
#include <algo/cobalt/cobalt.hpp>

/// @file cobalt.cpp
/// Implementation of the CMultiAligner class

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)


CMultiAligner::CMultiAligner(void) 
    : m_Options(new CMultiAlignerOptions(CMultiAlignerOptions::kDefaultMode
                                         | CMultiAlignerOptions::fNoRpsBlast))
{
    x_Init();
    x_InitParams();
    x_InitAligner();
}


CMultiAligner::CMultiAligner(const string& rps_db)
    : m_Options(new CMultiAlignerOptions(CMultiAlignerOptions::kDefaultMode))
{
    x_Init();
    x_InitParams();
    x_InitAligner();
}


CMultiAligner::CMultiAligner(const CConstRef<CMultiAlignerOptions>& options)
    : m_Options(options)
{
    x_Init();
    x_InitParams();
    x_InitAligner();
}


void CMultiAligner::x_InitParams(void)
{
    _ASSERT(!m_Options.Empty());

    m_ClustAlnMethod = m_Options->GetUseQueryClusters() 
        ? m_Options->GetInClustAlnMethod() : CMultiAlignerOptions::eNone;

    int score = m_Options->GetUserConstraintsScore();
    m_UserHits.PurgeAllHits();
    ITERATE(CMultiAlignerOptions::TConstraints, it,
            m_Options->GetUserConstraints()) {

        m_UserHits.AddToHitList(new CHit(it->seq1_index, it->seq2_index,
                                         TRange(it->seq1_start, it->seq1_stop),
                                         TRange(it->seq2_start, it->seq2_stop),
                                         score, CEditScript()));

    }

    //Note: Patterns are kept in m_Options
}


void CMultiAligner::x_InitAligner(void)
{
    x_SetScoreMatrix(m_Options->GetScoreMatrixName().c_str());
    m_Aligner.SetWg(m_Options->GetGapOpenPenalty());
    m_Aligner.SetWs(m_Options->GetGapExtendPenalty());
    m_Aligner.SetStartWg(m_Options->GetEndGapOpenPenalty());
    m_Aligner.SetStartWs(m_Options->GetEndGapExtendPenalty());
    m_Aligner.SetEndWg(m_Options->GetEndGapOpenPenalty());
    m_Aligner.SetEndWs(m_Options->GetEndGapExtendPenalty());
}


bool CMultiAligner::x_ValidateQueries(void) const
{
    ITERATE (vector<CSequence>, it, m_QueryData) {
        const unsigned char* sequence = it->GetSequence();
        for (int i=0;i < it->GetLength();i++) {
            if (sequence[i] == CSequence::kGapChar) {
                NCBI_THROW(CMultiAlignerException, eInvalidInput, "Gaps in "
                           "input sequences are not allowed");
            }
        }
    }

    return true;
}

bool CMultiAligner::x_ValidateUserHits(void)
{
    for (int i = 0; i < m_UserHits.Size(); i++) {
        CHit* hit = m_UserHits.GetHit(i);
        if (hit->m_SeqIndex1 < 0 || hit->m_SeqIndex2 < 0 ||
            hit->m_SeqIndex1 >= (int)m_QueryData.size() ||
            hit->m_SeqIndex2 >= (int)m_QueryData.size()) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput,
                        "Sequence specified by constraint is out of range");
        }
        int from1 = hit->m_SeqRange1.GetFrom();
        int from2 = hit->m_SeqRange2.GetFrom();
        int to1 = hit->m_SeqRange1.GetTo();
        int to2 = hit->m_SeqRange2.GetTo();
        int index1 = hit->m_SeqIndex1;
        int index2 = hit->m_SeqIndex2;

        if (from1 > to1 || from2 > to2) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput,
                        "Range specified by constraint is invalid");
        }
        if (from1 >= m_QueryData[index1].GetLength() ||
            to1 >= m_QueryData[index1].GetLength() ||
            from2 >= m_QueryData[index2].GetLength() ||
            to2 >= m_QueryData[index2].GetLength()) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput,
                        "Constraint is out of range");
        }
    }

    return true;
}


void 
CMultiAligner::SetQueries(const vector< CRef<objects::CSeq_loc> >& queries,
                          CRef<objects::CScope> scope)
{
    if (queries.size() < 2) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Aligner requires at least two input sequences");
    }

    m_Scope = scope;

    m_tQueries.resize(queries.size());
    copy(queries.begin(), queries.end(), m_tQueries.begin());

    m_QueryData.clear();
    ITERATE(vector< CRef<objects::CSeq_loc> >, itr, m_tQueries) {
        m_QueryData.push_back(CSequence(**itr, *m_Scope));
    }

    x_ValidateQueries();
    x_ValidateUserHits();
    Reset();
}


void 
CMultiAligner::SetQueries(const vector< CRef<objects::CBioseq> >& queries)
{
    if (queries.size() < 2) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Aligner requires at least two input sequences");
    }

    CRef<objects::CObjectManager> objmgr
        = objects::CObjectManager::GetInstance();

    m_Scope.Reset(new objects::CScope(*objmgr));
    m_Scope->AddDefaults();

    vector<objects::CBioseq_Handle> bioseq_handles;
    ITERATE(vector< CRef<objects::CBioseq> >, it, queries) {
        bioseq_handles.push_back(m_Scope->AddBioseq(**it));
    }

    m_tQueries.clear();
    ITERATE(vector<objects::CBioseq_Handle>, it, bioseq_handles) {
        CRef<objects::CSeq_loc> 
            seq_loc(new objects::CSeq_loc(objects::CSeq_loc::e_Whole));
                
        try {
            seq_loc->SetId(*it->GetSeqId());
        }
        catch (objects::CObjMgrException e) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput,
                       (string)"Missing seq-id in bioseq. " + e.GetMsg());
        }
        m_tQueries.push_back(seq_loc);
    }

    m_QueryData.clear();
    ITERATE(vector< CRef<objects::CSeq_loc> >, itr, m_tQueries) {
        m_QueryData.push_back(CSequence(**itr, *m_Scope));
    }

    x_ValidateQueries();
    x_ValidateUserHits();
    Reset();
}


void CMultiAligner::SetQueries(const blast::TSeqLocVector& queries)
{
    if (queries.size() < 2) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Aligner requires at least two input sequences");
    }

    m_Scope = queries[0].scope;

    m_tQueries.resize(queries.size());
    for (size_t i=0;i < queries.size();i++) {
        m_tQueries[i].Reset(new objects::CSeq_loc());
        try {
            m_tQueries[i]->Assign(*queries[i].seqloc);
        }
        catch (...) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput, "Bad SSeqLoc");
        }
        if (i > 0) {
            m_Scope->AddScope(*queries[i].scope);
        }
    }

    m_QueryData.clear();
    ITERATE(vector< CRef<objects::CSeq_loc> >, itr, m_tQueries) {
        m_QueryData.push_back(CSequence(**itr, *m_Scope));
    }

    x_ValidateQueries();
    x_ValidateUserHits();
    Reset();
}

CRef<objects::CBioTreeContainer> CMultiAligner::GetTreeContainer(void) const
{
    if (!m_Tree.GetTree()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "No tree to return");
    }

    CRef<objects::CBioTreeContainer> btc
         = MakeBioTreeContainer(m_Tree.GetTree());

    return btc;
}

CMultiAligner::FInterruptFn CMultiAligner::SetInterruptCallback(
                                             CMultiAligner::FInterruptFn fnptr,
                                             void* user_data)
{
    FInterruptFn prev_fun = m_Interrupt;
    m_Interrupt = fnptr;
    m_ProgressMonitor.user_data = user_data;
    return prev_fun;
}

void
CMultiAligner::x_SetScoreMatrix(const char *matrix_name)
{
    if (strcmp(matrix_name, "BLOSUM62") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Blosum62);
    else if (strcmp(matrix_name, "BLOSUM45") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Blosum45);
    else if (strcmp(matrix_name, "BLOSUM80") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Blosum80);
    else if (strcmp(matrix_name, "PAM30") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Pam30);
    else if (strcmp(matrix_name, "PAM70") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Pam70);
    else if (strcmp(matrix_name, "PAM250") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Pam250);
    else
        NCBI_THROW(CMultiAlignerException, eInvalidScoreMatrix,
                   "Unsupported score matrix");
}


void
CMultiAligner::Reset()
{
    m_Results.clear();
    m_DomainHits.PurgeAllHits();
    m_LocalHits.PurgeAllHits();
    m_PatternHits.PurgeAllHits();
    m_CombinedHits.PurgeAllHits();
}


void 
CMultiAligner::x_ComputeTree(void)
{
    // Convert the current collection of pairwise
    // hits into a distance matrix. This is the only
    // nonlinear operation that involves the scores of
    // alignments, so the raw scores should be converted
    // into bit scores before becoming distances.
    //
    // To do that, we need Karlin parameters for the 
    // score matrix and gap penalties chosen. Since both
    // of those can change independently, calculate the
    // Karlin parameters right now

    m_ProgressMonitor.stage = eTreeComputation;

    Blast_KarlinBlk karlin_blk;
    const Int4 kGapOpen = 11;
    const Int4 kGapExtend = 1;
    if (Blast_KarlinBlkGappedLoadFromTables(&karlin_blk, kGapOpen, kGapExtend,
                             m_Options->GetScoreMatrixName().c_str()) != 0) {

        NCBI_THROW(blast::CBlastException, eInvalidArgument,
                     "Cannot generate Karlin block");
    }

    CDistances distances(m_QueryData,
                         m_CombinedHits,
                         m_Aligner.GetMatrix(),
                         karlin_blk);

    CDistMethods::TMatrix dmat;

    if (m_ClustAlnMethod == CMultiAlignerOptions::eMulti) {
        const CDistMethods::TMatrix& bigmat = distances.GetMatrix();

        const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();
        dmat.Resize(clusters.size(), clusters.size(), 0.0);
        for (size_t i=0;i < clusters.size() - 1;i++) {
            for (size_t j=i+1;j < clusters.size();j++) {
                dmat(i, j) = bigmat(clusters[i].GetPrototype(),
                                    clusters[j].GetPrototype());
                dmat(j, i) = dmat(i, j);
            }
        }
    }
    else {
        dmat = distances.GetMatrix();
    }


    //--------------------------------
    if (m_Options->GetVerbose()) {
        const CDistMethods::TMatrix& matrix = dmat;
        printf("distance matrix:\n");
        printf("    ");
        for (int i = matrix.GetCols() - 1; i > 0; i--)
            printf("%5d ", i);
        printf("\n");
    
        for (int i = 0; i < (int)matrix.GetRows() - 1; i++) {
            printf("%2d: ", i);
            for (int j = matrix.GetCols() - 1; j > i; j--) {
                printf("%5.3f ", matrix(i, j));
            }
            printf("\n");
        }
        printf("\n\n");
    }
    //--------------------------------

    // build the guide tree associated with the matrix
    if (m_Options->GetTreeMethod() == CMultiAlignerOptions::eClusters) {

        CClusterer clusterer(dmat);
        // max in-cluster distance ensures one cluster hence one tree
        clusterer.ComputeClusters(DBL_MAX, CClusterer::eCompleteLinkage, true,
                                  1.0);
        _ASSERT(clusterer.GetClusters().size() == 1);
        m_Tree.SetTree(clusterer.ReleaseTree());
    }
    else {
        m_Tree.ComputeTree(dmat,
               m_Options->GetTreeMethod() == CMultiAlignerOptions::eFastME);
    }

    //--------------------------------
    if (m_Options->GetVerbose()) {
        CTree::PrintTree(m_Tree.GetTree());
    }
    //--------------------------------

    // check for interrupt
    if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
        NCBI_THROW(CMultiAlignerException, eInterrupt,
                   "Alignment interrupted");
    }
}


void CMultiAligner::x_Run(void)
{
    if ((int)m_tQueries.size() >= kClusterNodeId) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   (string)"Number of queries exceeds maximum of "
                   + NStr::IntToString(kClusterNodeId - 1));
    }

    bool is_cluster_found = false;
    vector<TPhyTreeNode*> cluster_trees;

    switch (m_ClustAlnMethod) {
    case CMultiAlignerOptions::eNone:

        break;

    case CMultiAlignerOptions::eToPrototype:
        if ((is_cluster_found = x_FindQueryClusters())) {
            x_AlignInClusters();

            // No multiple alignment is done for one cluster
            if (m_Clusterer.GetClusters().size() == 1) {
                m_tQueries.swap(m_AllQueries);
                return;
            }
        }
        break;

    case CMultiAlignerOptions::eMulti:
        if ((is_cluster_found = x_FindQueryClusters())) {
            x_ComputeClusterTrees(cluster_trees);
            x_FindLocalInClusterHits(cluster_trees);
        }
        break;

    default:
        NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                   "Invalid clustering option");
    }

    blast::TSeqLocVector blast_queries;
    vector<int> indices;
    x_CreateBlastQueries(blast_queries, indices);
    x_FindDomainHits(blast_queries, indices);
    x_FindLocalHits(blast_queries, indices);

    vector<const CSequence*> pattern_queries;
    x_CreatePatternQueries(pattern_queries, indices);
    x_FindPatternHits(pattern_queries, indices);
    x_FindConsistentHitSubset();

    switch (m_ClustAlnMethod) {
    case CMultiAlignerOptions::eNone:
        x_ComputeTree();
        x_BuildAlignment();
        break;

    case CMultiAlignerOptions::eToPrototype:
        x_ComputeTree();
        x_BuildAlignment();
        if (is_cluster_found) {
             x_MultiAlignClusters();
        }
        break;

    case CMultiAlignerOptions::eMulti:
        if (m_Clusterer.GetClusters().size() == 1) {
            // node id >= kClusterNodeId denotes root of cluster tree
            cluster_trees[0]->GetValue().SetId(kClusterNodeId);
            m_Tree.SetTree(cluster_trees[0]);
        }
        else {
            x_ComputeTree();
            x_BuildFullTree(cluster_trees);
        }
        x_BuildAlignment();
        break;

    default:
        NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                   "Invalid clustering option");
    }

}

CMultiAligner::TStatus CMultiAligner::Run()
{
    EStatus status = eSuccess;

    try {
        x_Run();
    }
    catch (CMultiAlignerException e) {
        CMultiAlignerException::EErrCode err_code
            = (CMultiAlignerException::EErrCode)e.GetErrCode();

        switch (err_code) {
        case CMultiAlignerException::eInvalidScoreMatrix:
        case CMultiAlignerException::eInvalidOptions:
            status = eOptionsError;
            break;

        case CMultiAlignerException::eInvalidInput:
            status = eQueriesError;
            break;

        case CMultiAlignerException::eInterrupt:
            status = eInterrupt;
            break;

        default:
            status = eInternalError;
        }

        m_Messages.push_back(e.GetMsg());
    }
    catch (blast::CBlastException e) {
        blast::CBlastException::EErrCode err_code
            = (blast::CBlastException::EErrCode)e.GetErrCode();

        status = (err_code == blast::CBlastException::eInvalidArgument 
                  ? eDatabaseError : eInternalError);

        m_Messages.push_back(e.GetMsg());
    }
    catch (CException e) {
        status = eInternalError;
        m_Messages.push_back(e.GetMsg());
    }
    catch (std::exception e) {
        status = eInternalError;
        m_Messages.push_back((string)e.what());
    }
    catch (...) {
        status = eInternalError;
    }

    if (status == eSuccess && IsMessage()) {
        status = eWarnings;
    }

    return (TStatus)status;
}


bool
CMultiAligner::x_FindQueryClusters()
{
    m_ProgressMonitor.stage = eQueryClustering;

    // Compute k-mer counts and distances between query sequences
    vector<TKmerCounts> kmer_counts;
    TKMethods::SetParams(m_Options->GetKmerLength(),
                         m_Options->GetKmerAlphabet());
    TKMethods::ComputeCounts(m_tQueries, *m_Scope, kmer_counts);

    // TO DO: Remove distance matrix, currently it is needed for finding
    // cluster representatives

    // distance matrix is need for fining cluster representatives
    auto_ptr<CClusterer::TDistMatrix> dmat
        = TKMethods::ComputeDistMatrix(kmer_counts,
                                       m_Options->GetKmerDistMeasure());

    // find sequences with user constraints
    set<int> constr_q;
    for (int i=0;i < m_UserHits.Size();i++) {
        CHit* hit = m_UserHits.GetHit(i);
        constr_q.insert(hit->m_SeqIndex1);
        constr_q.insert(hit->m_SeqIndex2);
    }

    // find a set of graph edges between sequences (will be used for clustering)
    CRef<CLinks> links(new CLinks(kmer_counts.size()));
    for (int i=0;i < (int)dmat->GetCols() - 1;i++) {

        // do no create links for sequences with user constraints as
        // they must not be clustered together with other sequences
        if (!constr_q.empty() && constr_q.find(i) != constr_q.end()) {
            continue;
        }

        for (int j=i+1;j < (int)dmat->GetCols();j++) {

            if (!constr_q.empty() && constr_q.find(j) != constr_q.end()) {
                continue;
            }

            if ((*dmat)(i, j) < m_Options->GetMaxInClusterDist()) {
                links->AddLink(i, j, (*dmat)(i, j));
            }
        }
    }
    links->Sort();

    // Set distances between queries that appear in user constraints and all
    // other queries to maximum, so that they form one-element clusters
    const double kMaxDistance = 1.5;

    // set distances to maximum
    ITERATE(set<int>, it, constr_q) {
        for (int i=0;i < (int)dmat->GetRows();i++) {
            if (i != *it) {
                (*dmat)(i, *it) = kMaxDistance;
                (*dmat)(*it, i) = kMaxDistance;
            }
        }
    }

    //-------------------------------------------------------
    if (m_Options->GetVerbose()) {
        printf("K-mer counts distance matrix:\n");
        printf("    ");
        for (size_t i=dmat->GetCols() - 1;i > 0;i--) {
            printf("%6d", (int)i);
        }
        printf("\n");
        for (size_t i=0;i < dmat->GetRows() - 1;i++) {
            printf("%3d:", (int)i);
            for (size_t j=dmat->GetCols() - 1;j > i;j--) {
                printf("%6.3f", (*dmat)(i, j));
            }
            printf("\n");
        }
        printf("\n");
    }
    //-------------------------------------------------------

    // Compute query clusters
    m_Clusterer.SetLinks(links);
    m_Clusterer.SetClustMethod(CClusterer::eClique);
    m_Clusterer.SetMakeTrees(
               m_Options->GetTreeMethod() == CMultiAlignerOptions::eClusters);
    m_Clusterer.Run();

    // save distance matrix in clusterer
    m_Clusterer.SetDistMatrix(dmat);

    // links are not needed any more
    links.Reset();

    const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();

    // If there are only single-element clusters
    // COBALT will be run without clustering informartion
    if (clusters.size() == m_QueryData.size()) {
        //-----------------------------------------------------------------
        if (m_Options->GetVerbose()) {
            printf("\nNumber of queries in clusters: 0 (0%%)\n");
            printf("Number of domain searches reduced by: 0 (0%%)\n\n");
            printf("Only single-element clusters were found."
                   " No clustering information will be used.\n");
        }
        //-----------------------------------------------------------------

        m_Clusterer.Reset();
        m_ClustAlnMethod = CMultiAlignerOptions::eNone;

        return false;
    }

    if (clusters.size() == 1) {
        m_Messages.push_back("All queries form only one cluster. No domain"
                             " information was used for generating constraints."
                             " Decreasing maximum in-cluster distance or"
                             " turning off query clustering option"
                             " may improve results.");
    }

    // Select cluster prototypes
    NON_CONST_ITERATE(CClusterer::TClusters, it, m_Clusterer.SetClusters()) {

        // For one-element clusters same element
        if (it->size() == 1) {
            it->SetPrototype(*it->begin());

        } else if (it->size() == 2) {

            // For two-element clusters - the longer sequence
            int len1 = m_QueryData[(*it)[0]].GetLength();
            int len2 = m_QueryData[(*it)[1]].GetLength();
            int prot = (len1 > len2) ? (*it)[0] : (*it)[1];
            it->SetPrototype(prot);

        } else {

            // For more than two elements - cluster center
            it->SetPrototype(it->FindCenterElement(m_Clusterer.GetDistMatrix()));
        }
    }

    // Rearrenging input sequences to consider cluster information
    vector< CRef<objects::CSeq_loc> > cluster_prototypes;
    if (m_ClustAlnMethod == CMultiAlignerOptions::eToPrototype) {
        ITERATE(CClusterer::TClusters, cluster_it, m_Clusterer.GetClusters()) {
            cluster_prototypes.push_back(m_tQueries[cluster_it->GetPrototype()]);
            m_AllQueryData.push_back(m_QueryData[cluster_it->GetPrototype()]);
        }

        m_tQueries.swap(cluster_prototypes);
        cluster_prototypes.swap(m_AllQueries);
        m_QueryData.swap(m_AllQueryData);
    }

    //-------------------------------------------------------
    if (m_Options->GetVerbose()) {
        const vector<CSequence>& q =
            (m_ClustAlnMethod == CMultiAlignerOptions::eToPrototype)
            ? m_AllQueryData : m_QueryData;
        printf("Query clusters:\n");
        int cluster_idx = 0;
        int num_in_clusters = 0;
        ITERATE(CClusterer::TClusters, it_cl, clusters) {
            printf("Cluster %3d: ", cluster_idx++);
            printf("(prototype: %3d) ", it_cl->GetPrototype());
            
            ITERATE(CClusterer::TSingleCluster, it_el, *it_cl) {
                printf("%d (%d), ", *it_el, q[*it_el].GetLength());
            }
            printf("\n");
            if (it_cl->size() > 1) {
                num_in_clusters += it_cl->size();
            }
        }
     
        int gain = m_QueryData.size() - clusters.size();
        printf("\nNumber of queries in clusters: %d (%.0f%%)\n", 
               num_in_clusters,
               (double)num_in_clusters / m_QueryData.size() * 100.0);
        printf("Number of domain searches reduced by: %d (%.0f%%)\n\n", gain, 
               (double) gain / m_QueryData.size() * 100.0);

        const CClusterer::TDistMatrix& d = m_Clusterer.GetDistMatrix();
        printf("Distances in clusters:\n");
        for (size_t cluster_idx=0;cluster_idx < clusters.size();
             cluster_idx++) {

            const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
            if (cluster.size() == 1) {
                continue;
            }

            printf("Cluster %d:\n", (int)cluster_idx);
            if (cluster.size() == 2) {
                printf("   %6.3f\n\n", d(cluster[0], cluster[1]));
                continue;
            }

            printf("    ");
            for (size_t i= cluster.size() - 1;i > 0;i--) {
                printf("%6d", (int)cluster[i]);
            }
            printf("\n");
            for (size_t i=0;i < cluster.size() - 1;i++) {
                printf("%3d:", (int)cluster[i]);
                for (size_t j=cluster.size() - 1;j > i;j--) {
                    printf("%6.3f", d(cluster[i], cluster[j]));
                }
                printf("\n");
            }
            printf("\n\n");
        }

        printf("Sequences that belong to different clusters with distance"
               " smaller than threshold (exludes prototypes):\n");
        ITERATE(CClusterer::TClusters, it, clusters) {
            if (it->size() == 1) {
                continue;
            }
            ITERATE(CClusterer::TSingleCluster, elem, *it) {
                ITERATE(CClusterer::TClusters, cl, clusters) {
                    if (it == cl) {
                        continue;
                    }

                    ITERATE(CClusterer::TSingleCluster, el, *cl) {

                        if (*el == cl->GetPrototype()) {
                            continue;
                        }

                        if (d(*elem, *el) < m_Options->GetMaxInClusterDist()) {
                            printf("%3d, %3d: %f\n", *elem, *el, d(*elem, *el));
                        }
                    }
                }
            }
        }
        printf("\n\n");
        
        
    }
    //-------------------------------------------------------

    // Distance matrix is not needed any more, release memory
    if (m_ClustAlnMethod == CMultiAlignerOptions::eToPrototype) {

        m_Clusterer.PurgeDistMatrix();
    }

    return true;
}

void CMultiAligner::x_AlignInClusters(void)
{
    const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();
    m_ClusterGapPositions.clear();
    m_ClusterGapPositions.resize(clusters.size());

    m_Aligner.SetWg(m_Options->GetGapOpenPenalty());
    m_Aligner.SetStartWg(m_Options->GetEndGapOpenPenalty());
    m_Aligner.SetEndWg(m_Options->GetEndGapOpenPenalty());
    m_Aligner.SetWs(m_Options->GetGapExtendPenalty());
    m_Aligner.SetStartWs(m_Options->GetEndGapExtendPenalty());
    m_Aligner.SetEndWs(m_Options->GetEndGapExtendPenalty());

    // For each cluster
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {

        const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
        CSequence& cluster_prot = m_AllQueryData[cluster.GetPrototype()];

        // One-element clusters contain only prototype sequence hence
        // nothing to align
        if (clusters[cluster_idx].size() > 1) {

            // Iterating over cluster elements
            ITERATE(CClusterer::TSingleCluster, seq_idx, cluster) {
                ASSERT((size_t)*seq_idx < m_AllQueryData.size());

                bool is_gap_in_prototype = false;

                // Skipping prototype sequence
                if (*seq_idx == cluster.GetPrototype()) {
                    continue;
                }
                CSequence& cluster_seq = m_AllQueryData[*seq_idx];
                
                // Aligning cluster sequence to cluster prototype
                m_Aligner.SetSequences((const char*)cluster_seq.GetSequence(),
                                       cluster_seq.GetLength(), 
                                       (const char*)cluster_prot.GetSequence(), 
                                       cluster_prot.GetLength());

                m_Aligner.SetEndSpaceFree(false, false, false, false);

                // If there is a large length disparity between the two
                // sequences, reduce or eliminate gap penalties.
                int len1 = cluster_seq.GetLength();
                int len2 = cluster_prot.GetLength();
                if (len1 > 1.2 * len2 || len2 > 1.2 * len1) {
                    m_Aligner.SetStartWs(m_Options->GetEndGapExtendPenalty()/2);
                    m_Aligner.SetEndWs(m_Options->GetEndGapExtendPenalty()/2); 
                }

                // Run aligner
                m_Aligner.Run();

                // Reset gap penalties
                m_Aligner.SetWg(m_Options->GetGapOpenPenalty());
                m_Aligner.SetStartWg(m_Options->GetEndGapOpenPenalty());
                m_Aligner.SetEndWg(m_Options->GetEndGapOpenPenalty());
                m_Aligner.SetWs(m_Options->GetGapExtendPenalty());
                m_Aligner.SetStartWs(m_Options->GetEndGapExtendPenalty());
                m_Aligner.SetEndWs(m_Options->GetEndGapExtendPenalty());

                CNWAligner::TTranscript t = m_Aligner.GetTranscript(false);
                cluster_seq.PropagateGaps(t, CNWAligner::eTS_Insert);

                // Saving gap positions with respect to non-gap letters only
                for (size_t j=0;j < t.size();j++) {
                    if (t[j] == CNWAligner::eTS_Delete) {
                        is_gap_in_prototype = true;
                        break;
                    }
                }

                if (!is_gap_in_prototype) {
                    continue;
                }

                cluster_prot.PropagateGaps(t, CNWAligner::eTS_Delete);

                // If gaps are added to cluster prototype they also need to
                // be added to sequences that were already aligned
                CClusterer::TSingleCluster::const_iterator it;
                for (it=clusters[cluster_idx].begin();it != seq_idx;++it) {
                    if (*it == cluster.GetPrototype()) {
                        continue;
                    }

                    m_AllQueryData[*it].PropagateGaps(t, 
                                                      CNWAligner::eTS_Delete);
                }
            }

            Uint4 pos = 0;
            for (Uint4 i=0;i < (Uint4)cluster_prot.GetLength();i++) {
                if (cluster_prot.GetLetter(i) == CSequence::kGapChar) {
                    m_ClusterGapPositions[cluster_idx].push_back(pos);
                }
                else {
                    pos++;
                }
            }

            //------------------------------------------------------------
            if (m_Options->GetVerbose()) {
                printf("Aligning in cluster %d:\n", (int)cluster_idx);
                ITERATE(CClusterer::TSingleCluster, elem, cluster) {
                    const CSequence& seq = m_AllQueryData[*elem];
                    printf("%3d: ", *elem);
                    for (int i=0;i < seq.GetLength();i++) {
                        printf("%c", seq.GetPrintableLetter(i));
                    }
                    printf("\n");
                }
                printf("\n\n");
            }
            //------------------------------------------------------------
        }

        // check for interrupt
        if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
            NCBI_THROW(CMultiAlignerException, eInterrupt,
                       "Alignement Interrupted");
        }
    }
    //--------------------------------------------------------------------
    if (m_Options->GetVerbose()) {
        for (size_t i=0;i < m_ClusterGapPositions.size();i++) {
            if (m_ClusterGapPositions[i].empty()) {
                continue;
            }

            printf("Gaps in cluster %d: ", (int)i);
            size_t j;
            for (j=0;j < m_ClusterGapPositions[i].size() - 1;j++) {
                printf("%3d, ", m_ClusterGapPositions[i][j]);
            }
            printf("%3d\n", m_ClusterGapPositions[i][j]);
        }
        printf("\n\n");
    }
    //--------------------------------------------------------------------

    // If there is only one cluster no multiple alignment is done
    if (clusters.size() == 1) {
        m_AllQueryData.swap(m_Results);
    }

}

// Initiate regular column of multiple alignment
void CMultiAligner::x_InitColumn(vector<CMultiAligner::SColumn>::iterator& it,
                         size_t len)
{
    it->insert = false;
    it->letters.resize(len);
    for (size_t i=0;i < len;i++) {
        it->letters[i] = -1;
    }
    it->number = 1;
    it->cluster = -1;
}


// Initiate a range from in-cluster alignment for insertion into multiple
// alignment
void CMultiAligner::x_InitInsertColumn(
                                vector<CMultiAligner::SColumn>::iterator& it,
                                size_t len, int num, int cluster)
{
    it->insert = true;
    it->letters.resize(len);
    for (size_t i=0;i < len;i++) {
        it->letters[i] = -1;
    }
    it->number = num;
    it->cluster = cluster;
}

void CMultiAligner::x_MultiAlignClusters(void)
{
    const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();

    int seq_length = m_Results[0].GetLength();
    size_t num_seqs = m_AllQueryData.size();

    vector<int> letter_inds(clusters.size());
    vector<SColumn> columns(seq_length);

    // Represent multiple alignment as list of columns of positions in input
    // sequences (n-th letter)
    int col = 0;
    NON_CONST_ITERATE(vector<SColumn>, it, columns) {
        x_InitColumn(it, num_seqs);
        for (size_t j=0;j < clusters.size();j++) {
            if (m_Results[j].GetLetter(col) != CSequence::kGapChar) {
                it->letters[clusters[j].GetPrototype()] = letter_inds[j]++;
            }
        }
        col++;
    }

    // Insert in-cluster ranges to columns
    size_t new_length = seq_length;

    // for each cluster
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {

        // for each gap position
        for (size_t i=0;i < m_ClusterGapPositions[cluster_idx].size();i++) {

            // get letter before which the gap needs to be placed
            size_t letter = m_ClusterGapPositions[cluster_idx][i];
            size_t offset = i;
            int num = 1;

            // combine all gaps before the same letter as one range
            while (i < m_ClusterGapPositions[cluster_idx].size() - 1
                   && m_ClusterGapPositions[cluster_idx][i + 1] == letter) {
                i++;
                num++;
            }
            
            // find that column that contains this letter
            vector<SColumn>::iterator it = columns.begin();
            size_t prototype_idx =  clusters[cluster_idx].GetPrototype();
            while (it != columns.end() 
                 && (it->insert || it->letters[prototype_idx] < (int)letter)) {
                ++it;
            }

            // insert the range in all cluster sequences
            it = columns.insert(it, SColumn());
            x_InitInsertColumn(it, num_seqs, num, cluster_idx);
            ITERATE(CClusterer::TSingleCluster, elem, clusters[cluster_idx]) {

                // for insert ranges leter index is absolute index in 
                // in-cluster alignment
                it->letters[*elem] = letter + offset;
            }

            // extend the length of the sequences by added ranges
            new_length += num;
            
            // check for interrupt
            if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
                NCBI_THROW(CMultiAlignerException, eInterrupt,
                           "Alignment interrupted");
            }
        }
    }


    // Convert columns to array of CSequence
    vector<CSequence> results(m_AllQueryData.size());

    // Initialize all sequences to gaps
    NON_CONST_ITERATE(vector<CSequence>, it, results) {
        it->Reset(new_length);
    }


    // offsets caused by in-cluster gaps in cluster prototypes
    vector<int> gap_offsets(clusters.size());
    col = 0;

    // for each column
    ITERATE(vector<SColumn>, it, columns) {
        if (!it->insert) {

            // for regular column
            // for each cluster
            for (size_t i=0;i < clusters.size();i++) {
                
                // find letter index in input sequnece (n-th letter)
                size_t prototype_idx = clusters[i].GetPrototype();
                int letter = it->letters[prototype_idx];
                
                // if gap in this position, do nothing
                if (letter < 0) {
                    continue;
                }

                // find correct location by skipping gaps in in-cluster
                // alingment
                // NOTE: This index juggling could be simplified if we
                // kept an array of unmodified input sequences
                while (m_AllQueryData[prototype_idx].GetLetter(letter 
                       + gap_offsets[i]) == CSequence::kGapChar) {
                    gap_offsets[i]++;
                }

                // insert letter in all cluster sequences
                ITERATE(CClusterer::TSingleCluster, elem, clusters[i]) {
                    results[*elem].SetLetter(col,
                     m_AllQueryData[*elem].GetLetter(letter + gap_offsets[i]));
                }
            }
        }
        else {
            
            // for insetr columns
            // for each cluster sequence copy letters from in-cluster alignment
            ITERATE(CClusterer::TSingleCluster, elem, clusters[it->cluster]) {
                for (int i=0; i < it->number;i++) {
                    results[*elem].SetLetter(col + i,
                      m_AllQueryData[*elem].GetLetter(it->letters[*elem] + i));
                }
            }

        }
        col += it->number;

        // check for interrupt
        if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
            NCBI_THROW(CMultiAlignerException, eInterrupt,
                       "Alignment interrupted");
        }
    }


    //----------------------------------------------------------------------
    if (m_Options->GetVerbose()) {
        printf("Cluster prototypes:\n");
        ITERATE(CClusterer::TClusters, it, clusters) {
            const CSequence& seq = results[it->GetPrototype()];
            for (int i=0;i < seq.GetLength();i++) {
                printf("%c", (char)seq.GetPrintableLetter(i));
            }
            printf("\n");
        }
        printf("\n\n");

        printf("Individual clusters:\n");
        for (int i=0;i < (int)clusters.size();i++) {
            if (clusters[i].size() > 1) {
                printf("Cluster %d:\n", i);
                ITERATE(CClusterer::TSingleCluster, elem, clusters[i]) {
                    const CSequence& seq = results[*elem];
                    for (int j=0;j < seq.GetLength();j++) {
                        printf("%c", (char)seq.GetPrintableLetter(j));
                    }
                    printf("\n");
                }
                printf("\n");
            }
        }
        printf("\n\n");


        printf("All queries:\n");
        ITERATE(vector<CSequence>, it, results) {
            const CSequence& seq = *it;
            for (int i=0;i < seq.GetLength();i++) {
                printf("%c", (char)seq.GetPrintableLetter(i));
            }
            printf("\n");
        }
        printf("\n\n");
    }
    //----------------------------------------------------------------------

    m_Results.swap(results);
    m_tQueries.swap(m_AllQueries);
}


// Frequencies are not normalized
void CMultiAligner::x_MakeClusterResidueFrequencies(void) 
{
    // Iterate over all clusters
    const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {

        const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
        _ASSERT(cluster.size() >= 1);
        
        // Skip one-element clusters
        if (cluster.size() == 1) {
            continue;
        }

        CSequence& prototype = m_QueryData[cluster_idx];
        CSequence::TFreqMatrix& freqs = prototype.GetFreqs();
        int size = prototype.GetLength();

        // For all cluster elements
        ITERATE(CClusterer::TSingleCluster, elem, cluster) {

            // Values from the prototype are already in the matrix
            if (*elem == cluster.GetPrototype()) {
                continue;
            }

            // Add frequencies from current sequence
            CSequence::TFreqMatrix& matrix = m_AllQueryData[*elem].GetFreqs();
            _ASSERT(matrix.GetRows() == freqs.GetRows() 
                    + m_ClusterGapPositions[cluster_idx].size());
            Uint4 gap_idx = 0, offset = 0;
            for (Uint4 i=0;i < (Uint4)size;i++) {
                while (gap_idx < m_ClusterGapPositions[cluster_idx].size()
                  && i == m_ClusterGapPositions[cluster_idx][gap_idx]) {
                    offset++;
                    gap_idx++;
                }
                for (int k=0;k < kAlphabetSize;k++) {
                    freqs(i, k) += matrix(i + offset, k);
                }
            }
            _ASSERT(offset == m_ClusterGapPositions[cluster_idx].size()
                    || m_ClusterGapPositions[cluster_idx][gap_idx] 
                    == (Uint4)prototype.GetLength());
        }

        // check for interrupt
        if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
            NCBI_THROW(CMultiAlignerException, eInterrupt,
                       "Alignment interrupted");
        }

    }

}

void CMultiAligner::x_CreateBlastQueries(blast::TSeqLocVector& queries,
                                        vector<int>& indices)
{
    queries.clear();
    indices.clear();

    switch (m_ClustAlnMethod) {

    case CMultiAlignerOptions::eNone:
    case CMultiAlignerOptions::eToPrototype:
        ITERATE(vector< CRef<objects::CSeq_loc> >, it, m_tQueries) {
            blast::SSeqLoc sl(**it, *m_Scope);
            queries.push_back(sl);
        }
        indices.resize(m_tQueries.size());
        for (int i=0;i < (int)m_tQueries.size();i++) {
            indices[i] = i;
        }
        break;

    case CMultiAlignerOptions::eMulti:
        ITERATE(CClusterer::TClusters, it, m_Clusterer.GetClusters()) {
            int index = it->GetPrototype();
            blast::SSeqLoc sl(*m_tQueries[index], *m_Scope);
            queries.push_back(sl);
            indices.push_back(index);
        }
        break;

    default:
        NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                   "Invalid in-cluster alignment method");
    }

}


void CMultiAligner::x_CreatePatternQueries(vector<const CSequence*>& queries,
                                           vector<int>& indices)
{

    switch (m_ClustAlnMethod) {

    case CMultiAlignerOptions::eNone:
    case CMultiAlignerOptions::eToPrototype:
        queries.resize(m_QueryData.size());
        indices.resize(m_QueryData.size());
        for (size_t i=0;i < m_QueryData.size();i++) {
            queries[i] = &m_QueryData[i];
            indices[i] = i;
        }
        break;

    case CMultiAlignerOptions::eMulti:
        {
            const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();
            queries.resize(clusters.size());
            indices.resize(clusters.size());
            for (size_t i=0;i < clusters.size();i++) {
                int index = clusters[i].GetPrototype();
                queries[i] = &m_QueryData[index];
                indices[i] = index;
            }
        }
        break;

    default:
        NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                   "Invalid in-cluster alignment method");
    }

}

/// Create phylogenetic tree for two sequences. This will be root and two
/// children.
/// @param ids Indices of the sequences in the array of queries [in]
/// @param distance Distance between the sequences [in]
/// @return Root of computed phylogenetic tree
static TPhyTreeNode* s_MakeTwoLeafTree(const CClusterer::CSingleCluster& ids,
                                       double distance)
{
    _ASSERT(ids.size() == 2);

    TPhyTreeNode *root = new TPhyTreeNode();
    root->GetValue().SetDist(0.0);
    double node_dist = distance / 2.0;

    // so that edges can be scaled later
    if (node_dist <= 0.0) {
        node_dist = 1.0;
    }

    TPhyTreeNode* node = new TPhyTreeNode();
    node->GetValue().SetId(ids[0]);
    // Label is set so that serialized tree can be used in external programs
    node->GetValue().SetLabel(NStr::IntToString(ids[0]));
    node->GetValue().SetDist(node_dist);
    root->AddNode(node);

    node = new TPhyTreeNode();
    node->GetValue().SetId(ids[1]);
    node->GetValue().SetLabel(NStr::IntToString(ids[1]));
    node->GetValue().SetDist(node_dist);
    root->AddNode(node);

    return root;
}

/// Change ids of leaf nodes in a given tree to desired values (recursive).
/// Function assumes that current leaf ids are 0,...,number of lefs. Each id i
/// will be changed to i-th element of the given array.
/// @param node Tree root [in|out]
/// @param ids List of desired ids [in]
static void s_SetLeafIds(TPhyTreeNode* node,
                       const CClusterer::CSingleCluster& ids)
{
    if (node->IsLeaf()) {
        _ASSERT(node->GetValue().GetId() < (int)ids.size());

        int id = ids[node->GetValue().GetId()];
        node->GetValue().SetId(id);
        
        // Labels are used to identify sequence in serialized tree.
        // They are set so that the tree can be used by external programs
        node->GetValue().SetLabel(NStr::IntToString(id));

        return;
    }
    
    TPhyTreeNode::TNodeList_CI child(node->SubNodeBegin());
    while (child != node->SubNodeEnd()) {

        s_SetLeafIds(*child, ids);
        child++;
    }
}

void CMultiAligner::x_ComputeClusterTrees(vector<TPhyTreeNode*>& trees)
{
    const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();

    if (m_Options->GetTreeMethod() == CMultiAlignerOptions::eClusters) {
        m_Clusterer.ReleaseTrees(trees);
        _ASSERT(trees.size() == clusters.size());
        
        // Trees for one-element clusters are not needed
        // Tree root == NULL indicates one-elemet cluster
        for (size_t i=0;i < trees.size();i++) {
            _ASSERT(trees[i]);

            if (clusters[i].size() == 1) {
                delete trees[i];
                trees[i] = NULL;
            }
        }        
    }
    else {

        trees.resize(clusters.size());
        for (int clust_idx=0;clust_idx < (int)clusters.size();clust_idx++) {
            const CClusterer::CSingleCluster& cluster = clusters[clust_idx];

            // Tree root == NULL indicates one-elemet cluster
            if (cluster.size() == 1) {
                trees[clust_idx] = NULL;
                continue;
            }

            if (cluster.size() == 2) {
                trees[clust_idx] = s_MakeTwoLeafTree(cluster,
                       (m_Clusterer.GetDistMatrix())(cluster[0], cluster[1]));
                
                continue;
            }

            CClusterer::TDistMatrix mat;
            m_Clusterer.GetClusterDistMatrix(clust_idx, mat);
            CTree single_tree(mat,
                 m_Options->GetTreeMethod() == CMultiAlignerOptions::eFastME);
            TPhyTreeNode* root = single_tree.ReleaseTree();

            // Set node id's that correspod to cluster sequences
            s_SetLeafIds(root, cluster);

            trees[clust_idx] = root;
        }
    }

    //----------------------------------------------------------------
    if (m_Options->GetVerbose()) {
        for (size_t i=0;i < trees.size();i++) {
            if (trees[i]) {
                printf("Tree for cluster %d:\n", (int)i);
                CTree::PrintTree(trees[i]);
                printf("\n");
            }
        }
    }
    //----------------------------------------------------------------
}


/// Compute length of the edge or distance from root for each leaf (recursive).
/// @param tree Tree root [in]
/// @param dist_from_root Current distance from root, used for recurence [in]
/// @param leaf_dists Leaf distances, vector must be allocated [out]
/// @param leaf_nodes Pointers to leaf nodes. Need to be initialized to NULLs
/// [out]
/// @param last_edge_only If true, length of last edge is returned for each
///        leaf, distance from root otherwise [in]
static void s_FindLeafDistances(TPhyTreeNode* tree, double dist_from_root,
                                vector<double>& leaf_dists,
                                vector<TPhyTreeNode*>& leaf_nodes,
                                bool last_edge_only = false)
{
    _ASSERT(!tree->GetParent() || tree->GetValue().IsSetDist());

    if (tree->IsLeaf()) {

        int id = tree->GetValue().GetId();
        double dist = tree->GetValue().GetDist();
        if (!last_edge_only) {
            dist += dist_from_root;
        }
        
        _ASSERT(id < (int)leaf_dists.size());
        leaf_dists[id] = dist;


        _ASSERT(id < (int)leaf_nodes.size() && !leaf_nodes[id]);
        leaf_nodes[id] = tree;

        return;
    }

    double dist;
    if (tree->GetParent() && tree->GetValue().IsSetDist() && !last_edge_only) {
        dist = tree->GetValue().GetDist();
    }
    else {
        dist = 0.0;
    }

    TPhyTreeNode::TNodeList_CI it = tree->SubNodeBegin();
    while (it != tree->SubNodeEnd()) {
        s_FindLeafDistances(*it, dist_from_root + dist, leaf_dists, leaf_nodes,
                            last_edge_only);
        it++;
    }
}

/// Find distance from root for selected node (recursive).
/// @param node Tree root [in]
/// @param id Node id for selected node [in]
/// @param dist_from_root Current distance from root, for recurrence [in]
/// @return Distance from root for node with given id, or -1.0 if node not
/// found
static double s_FindNodeDistance(const TPhyTreeNode* node, int id,
                                 double dist_from_root)
{
    _ASSERT(!node->GetParent() || node->GetValue().IsSetDist());

    if (node->GetValue().GetId() == id) {
        return dist_from_root + node->GetValue().GetDist();
    }

    if (node->IsLeaf()) {
        return -1.0;
    }

    double dist;
    if (!node->GetParent()) {
        dist = 0.0;
    }
    else {
        _ASSERT(node->GetValue().IsSetDist());
        dist = node->GetValue().GetDist();
    }

    TPhyTreeNode::TNodeList_CI it = node->SubNodeBegin();

    double result = -1.0;
    while (it != node->SubNodeEnd() && result <= -1.0) {
        result = s_FindNodeDistance(*it, id, dist_from_root + dist);
        it++;
    }

    return result;
}

/// Scale all tree edges by given factor (recursive).
/// @param node Tree root [in|out]
/// @param scale Scaling factor [in]
static void s_ScaleTreeEdges(TPhyTreeNode* node, double scale)
{
    _ASSERT(!node->GetParent() || node->GetValue().IsSetDist());

    node->GetValue().SetDist(node->GetValue().GetDist() * scale);

    if (node->IsLeaf()) {
        return;
    }

    TPhyTreeNode::TNodeList_I it = node->SubNodeBegin();
    while (it != node->SubNodeEnd()) {
        s_ScaleTreeEdges(*it, scale);
        it++;
    }
}

/// Rescale tree so that node with given id has desired distance from root
/// @param tree Tree root [in|out]
/// @param id Id of node that is to have desired distance from root [in]
/// @param dist Desired distance from root for desired node [in]
static void s_RescaleTree(TPhyTreeNode* tree, int id, double dist)
{
    // Find current distance from root
    double curr_dist = s_FindNodeDistance(tree, id, 0.0);

    _ASSERT(dist > 0.0);

    // Find scale
    double scale;
    if (curr_dist > 0.0) {
        scale = dist / curr_dist;
    }
    else {
        scale = dist;
    }

    // Scale all edges of the tree
    s_ScaleTreeEdges(tree, scale);
}

void CMultiAligner::x_AttachClusterTrees(
                                   const vector<TPhyTreeNode*>& cluster_trees,
                                   const vector<TPhyTreeNode*>& cluster_leaves)
{
    ITERATE(vector<TPhyTreeNode*>, it, cluster_leaves) {
        // For each leaf here
        TPhyTreeNode* node = *it;

        _ASSERT(node && node->IsLeaf());
        _ASSERT(node->GetValue().IsSetDist());

        // find query cluster it represents and get cluster subtree
        int cluster_id = node->GetValue().GetId();
        TPhyTreeNode* subtree = cluster_trees[cluster_id];

        // NULL pointer indicates one-element cluster
        // there is no subtree to attach, but node id must be changed from
        // cluster id to sequence id
        if (!subtree) {
            const CClusterer::CSingleCluster& cluster
                = m_Clusterer.GetClusters()[cluster_id];
            int seq_id = cluster[0];

            node->GetValue().SetId(seq_id);
            node->GetValue().SetLabel(NStr::IntToString(seq_id));

            continue;
        }

        // id >= kClusterNodeId denotes root of cluster subtree
        node->GetValue().SetId(kClusterNodeId + cluster_id);
        node->GetValue().SetLabel("");

        // Detach subtree children and attach them to the leaf node.
        // This prevents problems in recursion
        vector<TPhyTreeNode*> children;
        TPhyTreeNode::TNodeList_I child(subtree->SubNodeBegin());
        while (child != subtree->SubNodeEnd()) {
            children.push_back(*child);
            child++;
        }
        ITERATE(vector<TPhyTreeNode*>, it, children) {
            subtree->DetachNode(*it);
            node->AddNode(*it);
        }
        delete subtree;
        subtree = NULL;

        // node replaces root of the subtree
        node->GetValue().SetDist(0.0);

    }
}

void CMultiAligner::x_BuildFullTree(const vector<TPhyTreeNode*>& cluster_trees)
{
    _ASSERT(m_Tree.GetTree());

    const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();
    _ASSERT(cluster_trees.size() == clusters.size());

    // Find leaf nodes and lengths of leaf edges in the tree of cluster
    // prototypes
    vector<double> cluster_dists(clusters.size(), 0.0);
    vector<TPhyTreeNode*> cluster_leaves(clusters.size(), NULL);
    s_FindLeafDistances(m_Tree.GetTree(), 0.0, cluster_dists, cluster_leaves,
                      true);
    //------------------------------------------------------------
    if (m_Options->GetVerbose()) {
        vector<TPhyTreeNode*> dummy_vect(clusters.size(), NULL);
        vector<double>d(cluster_dists.size());
        s_FindLeafDistances(m_Tree.GetTree(), 0.0, d, dummy_vect, false);
        for (size_t i=0;i < d.size();i++) {
            printf("%d:%f ", (int)i, d[i]);
        }
        printf("\n");
    }
    //------------------------------------------------------------

    // For each cluster tree
    for (size_t i=0;i < cluster_trees.size();i++) {

        // skip one-element clusters
        if (!cluster_trees[i]) {
            continue;
        }

        const CClusterer::CSingleCluster& cluster = clusters[i];

        // if the length of leaf edge is non-positive, set it to a small value
        if (cluster_dists[i] <= 0.0) {
            cluster_dists[i] = 1e-5;
        }

        // rescale cluster tree so that distance from root to cluster
        // representative is the same as leaf edge in the tree of cluster
        // prototypes
        s_RescaleTree(cluster_trees[i], cluster.GetPrototype(),
                      cluster_dists[i]);
    }

    // Attach cluster trees to the guide tree
    x_AttachClusterTrees(cluster_trees, cluster_leaves);

    //------------------------------------------------------------
    if (m_Options->GetVerbose()) {
        vector<TPhyTreeNode*> dummy_vect(m_QueryData.size(), NULL);
        cluster_dists.resize(m_QueryData.size(), 0.0);
        s_FindLeafDistances(m_Tree.GetTree(), 0.0, cluster_dists, dummy_vect,
                          false);
        for (size_t i=0;i < cluster_dists.size();i++) {
            printf("%d:%f ", (int)i, cluster_dists[i]);
        }
        printf("\n");
    }

    if (m_Options->GetVerbose()) {
        printf("Full tree:\n");
        CTree::PrintTree(m_Tree.GetTree());
        printf("\n");
    }
    //------------------------------------------------------------


}


END_SCOPE(cobalt)
END_NCBI_SCOPE
