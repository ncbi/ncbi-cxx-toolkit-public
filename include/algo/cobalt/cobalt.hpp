#ifndef ALGO_COBALT___COBALT__HPP
#define ALGO_COBALT___COBALT__HPP

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

File name: cobalt.hpp

Author: Jason Papadopoulos

Contents: Interface for CMultiAligner

******************************************************************************/

#include <util/math/matrix.hpp>
#include <corelib/ncbifile.hpp>
#include <algo/align/nw/nw_pssm_aligner.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/biotree/BioTreeContainer.hpp>
#include <objects/blast/Blast4_archive.hpp>

#include <algo/cobalt/base.hpp>
#include <algo/cobalt/resfreq.hpp>
#include <algo/cobalt/seq.hpp>
#include <algo/cobalt/hit.hpp>
#include <algo/cobalt/hitlist.hpp>
#include <algo/cobalt/dist.hpp>
#include <algo/cobalt/tree.hpp>
#include <algo/cobalt/exception.hpp>
#include <algo/cobalt/kmercounts.hpp>
#include <algo/cobalt/clusterer.hpp>
#include <algo/cobalt/options.hpp>

/// @file cobalt.hpp
/// Interface for CMultiAligner

class CMultiAlignerTest;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Simultaneously align multiple protein sequences
class NCBI_COBALT_EXPORT CMultiAligner : public CObject
{
public:
    /// Version information
    static const int kMajorVersion = 2;
    static const int kMinorVersion = 0;
    static const int kPatchVersion = 1;

    /// Return status
    enum EStatus {
        eSuccess = 0,   ///< Alignment successfully completed
        eWarnings = 1,  ///< Alignment completed with warnings
        eOptionsError,  ///< Error related to options occured
        eQueriesError,  ///< Error related to query sequences occured
        eDatabaseError, ///< Error related to RPS database occured
        eInternalError, ///< Unexpected error occured
        eInterrupt,     ///< Alignment interruped through callback function
        eLimitsExceeded ///< Processing limits exceeded
    };

    typedef int TStatus;

    enum EAlignmentStage {
        eBegin,
        eQueryClustering,
        eDomainHitsSearch,
        eLocalHitsSearch,
        ePatternHitsSearch,
        eTreeComputation,
        eProgressiveAlignment,
        eIterativeAlignment
    };

    /// Structure for reporting alignment progress
    struct SProgress {
        EAlignmentStage stage;
        void* user_data;

        SProgress(void) : stage(eBegin), user_data(NULL) {}
    };

    /// Prototype for function pointer to dertermine whether alignment should
    /// proceed of be interrupted. If this function returns true, all
    /// processing stops
    typedef bool(*FInterruptFn)(SProgress* progress);

    typedef CSparseKmerCounts TKmerCounts;
    typedef TKmerMethods<TKmerCounts> TKMethods;

public:

    //------------------ Constructors ----------------------------

    /// Create mutli aligner with default options
    ///
    CMultiAligner(void);


    /// Create multi aligner with selected RPS data base and default options
    /// @param rps_db RPS data base path [in]
    ///
    CMultiAligner(const string& rps_db);


    /// Create mutli aligner with given options
    /// @param options Parameters [in]
    ///
    CMultiAligner(const CConstRef<CMultiAlignerOptions>& options);


    //------------------ Sequences to align --------------------------

    /// Set query sequences.
    /// This automatically clears out the intermediate state
    /// of the last alignment.
    /// @param queries List of query sequences or seq-ids [in]
    /// @param scope Scope object [in]
    ///
    void SetQueries(const vector< CRef<objects::CSeq_loc> >& queries,
                    CRef<objects::CScope> scope);

    /// Set query sequences.
    /// This automatically clears out the intermediate state
    /// of the last alignment.
    /// @param queries List of query sequences [in]
    ///
    void SetQueries(const vector< CRef<objects::CBioseq> >& queries);

    /// Set query sequences.
    /// This automatically clears out the intermediate state
    /// of the last alignment.
    /// @param queries List of query sequences [in]
    ///
    void SetQueries(const blast::TSeqLocVector& queries);

    /// Set input alignments
    /// @param msa1 The first input alignment [in]
    /// @param msa2 The second input alignment [in]
    /// @param representatives1 List of sequence indices in msa1
    /// to be used for computing constraints [in]
    /// @param representatives2 List of sequence indices in msa2
    /// to be used for computing constraints [in]
    /// @param scope Scope [in]
    ///
    void SetInputMSAs(const objects::CSeq_align& msa1,
                      const objects::CSeq_align& msa2,
                      const set<int>& representatives1,
                      const set<int>& representatives2,
                      CRef<objects::CScope> scope);

    /// Set pre-computed domain hits using BLAST archive format
    /// @param archive BLAST archive format [in]
    ///
    /// Queries from the archive are matched to COBALT queries by Seq_ids.
    /// The two sets of queries do not need to be the same. Domain hits for
    /// the archive queries that do match to any of the COBALT queries are
    /// ignored. COBALT will do RPS-BLAST search for all of its sequences that
    /// were not matched to the archive queries. It is the responsibility of
    /// user to ensure that the pre-computed hits and COBALT refer to the same
    /// domain database.
    void SetDomainHits(const objects::CBlast4_archive& archive);

    /// Get query sequences
    /// @return List of seq-ids and locations [in]
    ///
    const vector< CRef<objects::CSeq_loc> >& GetQueries(void) const
    {return m_tQueries;}

    /// Get scope
    /// @return Scope
    ///
    CRef<objects::CScope> GetScope(void) {return m_Scope;}
    

    //------------------ Options --------------------------

    /// Get mutli aligner parameters
    /// @return Options
    ///
    CConstRef<CMultiAlignerOptions> GetOptions(void) const {return m_Options;}


    // ---------------- Running the aligner -----------------------

    /// Align the current set of input sequences (reset any existing
    /// alignment information).
    ///
    /// This function handles the generation of all internal state in the
    /// correct order. It is sufficient for 'black box' applications that
    /// only want a final answer without tweaking internal state.
    /// x_Run() is called for computing alignment.
    /// @return Computation status: success (0), warnings (1), error (>1)
    ///
    TStatus Run(void);

    /// Clear out the state left by the previous alignment operation
    ///
    void Reset(void);


    // ---------------- Results -----------------------

    /// Retrieve the current aligned results in Seq-align format.
    /// The Seq-align is of global type, with a single denseg
    /// @return The results
    ///
    CRef<objects::CSeq_align> GetResults(void) const;

    /// Retrieve a selection of the current aligned results, 
    /// in Seq-align format. The Seq-align is of global type, 
    /// with a single denseg. Columns that have gaps in all the
    /// selected sequences are removed
    /// @param indices List of ordinal IDs of sequences that the
    ///                Seq-align will contain. Indices may appear
    ///                in any order and may be repeated
    /// @return The results
    ///
    CRef<objects::CSeq_align> GetResults(vector<int>& indices) const;

    /// Retrieve the current aligned results in CSequence format. Included 
    /// for backward compatibility with previous API.
    /// @return The results, on CSequence for each input sequence
    ///
    const vector<CSequence>& GetSeqResults(void) const {return m_Results;}

    /// Get ree used guide in progressive alignment
    /// @return Tree
    ///
    const TPhyTreeNode* GetTree(void) const {return m_Tree.GetTree();}

    /// Get serializable tree used as guide in progressive alignment
    /// @return Tree
    ///
    CRef<objects::CBioTreeContainer> GetTreeContainer(void) const;

    /// Get clusters of query sequences
    /// @return Query clusters
    ///
    const CClusterer::TClusters& GetQueryClusters(void) const
    {return m_Clusterer.GetClusters();}


    // ---------------- Interrupt ---------------------------

    /// Set a function callback to be invoked by multi aligner to allow
    /// interrupting alignment in progress
    /// @param fnptr Pointer to callback function [in]
    /// @param user_data user data to be attached to progress structure [in]
    /// @return Previously set callback function
    ///
    FInterruptFn SetInterruptCallback(FInterruptFn fnptr,
                                      void* user_data = NULL);


    // -------------- Errors/Warnings -----------------------

    /// Get Error/Warning messages
    ///
    /// Errors are reported by exceptions, hence the messages will be mostly
    /// warnings.
    /// @return Messages
    ///
    const vector<string>& GetMessages(void) const {return m_Messages;}

    /// Check whether there are any error/warning messages
    /// @return True if there are messages, false otherwise
    ///
    bool IsMessage(void) const {return !m_Messages.empty();}


    ////////////////////////////////////////////////////////////

public:

    /// Default gap open penalty
    static const CNWAligner::TScore kDefaultGapOpen = -11;

    /// Default gap extension penalty
    static const CNWAligner::TScore kDefaultGapExtend = -1;



protected:

    /// Validate query sequences. Check for gaps in sequences. Throws if
    /// queries do not pass validation.
    /// @return True if validation passed
    ///
    bool x_ValidateQueries(void) const;

    /// Validate input alignments. Throws if alignments do not pass validation.
    /// @param True if validation passed
    //
    bool x_ValidateInputMSAs(void) const;

    /// Validate user constraints with queries. Throws if constraints do not
    /// pass validation.
    /// @return True if validation passed
    ///
    bool x_ValidateUserHits(void);

    /// Create query set for RPS Blast and Blastp searches along with indices
    /// in multiple alignment queries array.
    ///
    /// Searches for conserved regions can be performed for a subset of
    /// multiple alignment queries (typically to reduce computation time).
    /// @param queries Queries for RPS Blast and Blastp searches [out]
    /// @param indices Indexes of each query in m_tQueries [out]
    ///
    void x_CreateBlastQueries(blast::TSeqLocVector& queries,
                              vector<int>& indices);

    /// Create query set for PROSITE pattern search along with indices
    /// in multiple alignment queries array.
    ///
    /// Searches for conserved regions can be performed for a subset of
    /// multiple alignment queries (typically to reduce computation time).
    /// @param queries Queries for PROSITE patterns searches [out]
    /// @param indices Indexes of each query in m_tQueries [out]
    ///
    void x_CreatePatternQueries(vector<const CSequence*>& queries,
                                vector<int>& indices);

    /// Run RPS blast on seletced input sequences and postprocess the
    /// results. Intended for applications that want fine-grained control of
    /// the alignment process.
    /// @param queries Queries for RPS Blast search [in]
    /// @param indices Index of each RPS Blast query in the array of input
    /// sequences [in]
    ///
    void x_FindDomainHits(blast::TSeqLocVector& queries,
                          const vector<int>& indices);

    /// Run blast on selected input sequences and postprocess the results.
    /// Intended for applications that want fine-grained control of the 
    /// alignment process
    /// @param queries Queries for Blast alignment [in]
    /// @param indices Index of each Blast query in the array of input
    /// sequences [in]
    ///
    void x_FindLocalHits(const blast::TSeqLocVector& queries,
                         const vector<int>& indices);

    /// Run blast on sequences from each cluster subtree. Each tree is
    /// traversed. Blast is run for pairs of most similar sequences such that
    /// each belongs to different subtree. Indented for use with query
    /// clustering for faster in-cluster pair-wise alignments.
    /// @param cluster_trees List of query cluster trees [in]
    ///
    void x_FindLocalInClusterHits(const vector<TPhyTreeNode*>& cluster_trees);

    /// Find PROSITE pattern hits on selected input sequences.
    /// Intended for applications that want fine-grained control of the 
    /// alignment process
    /// @param queries Queries for PROSITE patter search [in]
    /// @param indices Index of each PROSITE pattern search query in the
    /// array of input sequences [in]
    ///
    void x_FindPatternHits(const vector<const CSequence*>& queries,
                           const vector<int>& indices);

    /// Find consistent subset of pair-wise hits that can be used as
    /// alignment constraints
    ///
    void x_FindConsistentHitSubset(void);

    /// Given the current list of domain and local hits, generate a 
    /// phylogenetic tree that clusters the current input sequences.
    /// Intended for applications that want fine-grained control of the 
    /// alignment process
    ///
    void x_ComputeTree();

    /// Given the current domain, local, pattern and user hits, along with
    /// the current tree, compute a multiple alignment of the input sequences.
    /// Intended for applications that want fine-grained control of the 
    /// alignment process
    ///
    void x_BuildAlignment();

    /// Find clusters of similar queries, select cluster representative 
    /// sequences, and prepare input to multiple alignement composed of 
    /// only representatives
    /// @return True if at least one cluster was found, false otherwise
    ///
    bool x_FindQueryClusters();

    /// Pair-wise align each cluster sequence to cluster representative
    ///
    void x_AlignInClusters();

    /// Compute profile residue frequencies for clusters. Frequencies are not
    /// normalized.
    ///
    void x_MakeClusterResidueFrequencies();

    /// Combine pair-wise in-cluster alignements with multiple alignments of
    /// cluster prototypes
    ///
    void x_MultiAlignClusters();

    /// Compute independent phylogenetic trees each cluster.
    /// @param Array of tree roots. Each array element corresponds to
    /// the cluster. One element clusters yield NULL tree. [out]
    ///
    void x_ComputeClusterTrees(vector<TPhyTreeNode*>& trees);

    /// Replace leaves in the alignment guide tree of clusters with cluster
    /// trees.
    /// @param cluster_trees List of phylogenetic trees computed for each
    /// cluster of input sequences [in]
    /// @param cluster_leaves List of pointers to leaves of the alignment guide
    /// tree computed for clusters [in]
    ///
    void x_AttachClusterTrees(const vector<TPhyTreeNode*>& cluster_trees,
                              const vector<TPhyTreeNode*>& cluster_leaves);

    /// Combine alignment guide tree computed for clusters with guide trees
    /// computed for each cluster. 
    ///
    /// Leaves of the guide tree computed for cluster prototypes are replaced
    /// with clusters trees. Cluster trees are rescaled so that distance from
    /// root to cluster prototype in cluster tree is the same as length of
    /// the leaf edge in the alignment guide tree (for cluster prototypes).
    /// @param cluster_trees List of cluster trees [in]
    ///
    void x_BuildFullTree(const vector<TPhyTreeNode*>& cluster_trees);

    /// Align the current set of input sequences (reset any existing
    /// alignment information).
    ///
    /// This function handles the generation of all internal state in the
    /// correct order. It is sufficient for 'black box' applications that
    /// only want a final answer without tweaking internal state.
    ///
    virtual void x_Run(void);

    /// Align multiple sequence alignments
    ///
    void x_AlignMSAs(void);

protected:

    typedef struct SSegmentLoc {
        int seq_index;
        TRange range;
    
        SSegmentLoc() {}
        SSegmentLoc(int s, TRange r)
            : seq_index(s), range(r) {}
        SSegmentLoc(int s, TOffset from, TOffset to)
            : seq_index(s), range(from, to) {}
    
        TOffset GetFrom() const { return range.GetFrom(); }
        TOffset GetTo() const { return range.GetTo(); }
    } SSegmentLoc;

    
private:
    static const int kRpsScaleFactor = 100;
    
    typedef struct SGraphNode {
        CHit *hit;                  ///< the alignment represented by this node
        int list_pos;
        struct SGraphNode *path_next;///< the optimal path node belongs to
        double best_score;           ///< the score of that optimal path
                                     ///  (assumes this node is the first entry
                                     ///  in the path)
    
        SGraphNode() {}
    
        SGraphNode(CHit *h, int list_pos) 
            : hit(h), list_pos(list_pos), 
              path_next(0), best_score(0.0) {}
    
    } SGraphNode;

    class compare_sseg_db_idx;
    friend class compare_sseg_db_idx;
    
    class compare_sseg_db_idx {
    public:
        bool operator()(const SSegmentLoc& a, const SSegmentLoc& b) const {
            return a.seq_index < b.seq_index;
        }
    };


    /// Column in an alignment used for combining result from multiple
    /// alignment and pair-wise in-cluster alignments
    typedef struct SColumn {
        /// Is the column an insertion from in-cluster alignment
        bool insert;
        
        /// Indices of letters in this column
        /// for regular column: index of a letter in input sequence 
        /// (n-th letter)
        /// for inserted column: index of a letter in in-cluster alignment
        vector<int> letters;

        /// number of letters in range (inserted column only)
        int number;

        /// cluster index (inserted column only)
        int cluster;

        SColumn(void) : insert(false), number(1), cluster(-1) {}
        
    } SColumn;



private:
    /// Initiate parameters using m_Options
    void x_InitParams(void);

    /// Initiate PSSM aligner parameters
    void x_InitAligner(void);

    /// Set the score matrix the aligner will use. NOTE that at present
    /// any hits between sequences will always be scored using BLOSUM62;
    /// the matrix chosen here is only used when forming the complete
    /// alignment
    /// @param matrix_name The score matrix to use; limited to
    /// the same list of matrices as blast [in]
    void x_SetScoreMatrix(const char *matrix_name);

    /// Initiate class attributes that are not alignment parameters
    void x_Init(void)
    {m_Interrupt = NULL;}

    void x_LoadBlockBoundaries(string blockfile,
                      vector<SSegmentLoc>& blocklist);
    void x_FindRPSHits(blast::TSeqLocVector& queries,
                       const vector<int>& indices, CHitList& rps_hits);

    void x_RealignBlocks(CHitList& rps_hits,
                         vector<SSegmentLoc>& blocklist,
                         CProfileData& profile_data);
    void x_AssignRPSResFreqs(CHitList& rps_hits,
                             CProfileData& profile_data);
    void x_AssignDefaultResFreqs();

    void x_AddNewSegment(vector< CRef<objects::CSeq_loc> >& loc_list, 
                         const CRef<objects::CSeq_loc>& query, 
                         TOffset from, TOffset to, 
                         vector<SSegmentLoc>& seg_list,
                         int query_index);

    void x_MakeFillerBlocks(const vector<int>& indices,
                            vector< CRef<objects::CSeq_loc> >& filler_locs,
                            vector<SSegmentLoc>& filler_segs);

    void x_AlignFillerBlocks(const blast::TSeqLocVector& queries,
                             const vector<int>& indices,
                             vector< CRef<objects::CSeq_loc> >& filler_locs, 
                             vector<SSegmentLoc>& filler_segs);

    void x_FindAlignmentSubsets();
    SGraphNode * x_FindBestPath(vector<SGraphNode>& nodes);

    void x_BuildAlignmentIterative(vector<CTree::STreeEdge>& edges,
                                   double cluster_cutoff);
    void x_FindConservedColumns(vector<CSequence>& new_alignment,
                                CHitList& conserved);
    void x_AlignProgressive(const TPhyTreeNode *tree,
                            vector<CSequence>& query_data,
                            CNcbiMatrix<CHitList>& pair_info,
                            int iteration, bool is_cluster);
    double x_RealignSequences(const TPhyTreeNode *input_cluster,
                              vector<CSequence>& alignment,
                              CNcbiMatrix<CHitList>& pair_info,
                              double score,
                              int iteration);
    void x_AlignProfileProfile(vector<CTree::STreeLeaf>& node_list1,
                               vector<CTree::STreeLeaf>& node_list2,
                               vector<CSequence>& alignment,
                               CNcbiMatrix<CHitList>& pair_info,
                               int iteration);
    void x_AlignProfileProfileUsingHit(vector<CTree::STreeLeaf>& node_list1,
                                       vector<CTree::STreeLeaf>& node_list2,
                                       vector<CSequence>& alignment,
                                       CNcbiMatrix<CHitList>& pair_info,
                                       int iteration);
    void x_FindConstraints(vector<size_t>& constraint,
                           vector<CSequence>& alignment,
                           vector<CTree::STreeLeaf>& node_list1,
                           vector<CTree::STreeLeaf>& node_list2,
                           CNcbiMatrix<CHitList>& pair_info,
                           int iteration);
    void x_FindInClusterConstraints(auto_ptr<CHit>& hit,
                                    vector<CSequence>& alignment,
                                    vector<CTree::STreeLeaf>& node_list1,
                                    vector<CTree::STreeLeaf>& node_list2,
                                    CNcbiMatrix<CHitList>& pair_info,
                                    vector< pair<int, int> >& gaps1,
                                    vector< pair<int, int> >& gaps2) const;
    double x_GetScoreOneCol(vector<CSequence>& align, 
                            int col);
    double x_GetScore(vector<CSequence>& align);

    CRef<objects::CSeq_align> x_GetSeqalign(const vector<CSequence>& align,
                                            vector<int>& indices) const;

    static void x_InitColumn(vector<SColumn>::iterator& it, size_t len);

    static void x_InitInsertColumn(vector<SColumn>::iterator& it, size_t len,
                                   int num, int cluster);

    void x_AddRpsFreqsToCluster(const CClusterer::CSingleCluster& cluster,
                                vector<CSequence>& query_data,
                                const vector<TRange>& gaps);

    auto_ptr< vector<int> > x_AlignClusterQueries(const TPhyTreeNode* node);

    void x_ComputeProfileRangeAlignment(
                                vector<CTree::STreeLeaf>& node_list1,
                                vector<CTree::STreeLeaf>& node_list2,
                                vector<CSequence>& alignment,
                                vector<size_t>& constraints,
                                TRange range1, TRange range2,
                                int full_prof_len1, int full_prof_len2,
                                bool left_margin,
                                CNWAligner::TTranscript& t);



protected:

    // Input sequences

    CConstRef<CMultiAlignerOptions> m_Options;

    vector< CRef<objects::CSeq_loc> > m_tQueries;
    CRef<objects::CScope> m_Scope;

    vector<CSequence> m_QueryData;

    /// Input alignment
    vector<CSequence> m_InMSA1;
    /// Input alignment
    vector<CSequence> m_InMSA2;

    /// Indices of sequence representatives in input alignment 1
    vector<int> m_Msa1Repr; 
    /// Indices of sequence representatives in input alignment 2
    vector<int> m_Msa2Repr;

    vector<CSequence> m_Results;

    // Server classes

    CPSSMAligner m_Aligner;
    CTree m_Tree;
    CClusterer m_Clusterer;

    // Intermediate results

    CHitList m_DomainHits;
    CHitList m_LocalHits;
    CHitList m_CombinedHits;
    CHitList m_PatternHits;
    CHitList m_LocalInClusterHits;
    CHitList m_UserHits;

    /// Marks sequences with pre-computed domain hits
    vector<bool> m_IsDomainSearched;

    vector< vector<Uint4> > m_ClusterGapPositions;
    vector< CRef<objects::CSeq_loc> > m_AllQueries;
    vector<CSequence> m_AllQueryData;

    // Ranges of conserved domains in query sequences identified by RPS Blast
    vector< vector<TRange> > m_RPSLocs;


    // Running and monitoring

    FInterruptFn m_Interrupt;
    SProgress m_ProgressMonitor;

    vector<string> m_Messages;

    // Query clustering

    // Modified by x_FindQueryClusters() if no clusters found
    CMultiAlignerOptions::EInClustAlnMethod m_ClustAlnMethod;

    // Minimum tree node id for root of cluster subtree
    static const int kClusterNodeId = 16000;

    friend class ::CMultiAlignerTest;
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif /* ALGO_COBALT___COBALT__HPP */
