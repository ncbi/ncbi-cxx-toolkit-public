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
#include <algo/blast/api/sseqloc.hpp>
#include <algo/align/nw/nw_pssm_aligner.hpp>
#include <objects/seqalign/Seq_align.hpp>

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

/// @file cobalt.hpp
/// Interface for CMultiAligner

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Simultaneously align multiple protein sequences
class NCBI_COBALT_EXPORT CMultiAligner : public CObject
{
public:

    typedef CSparseKmerCounts TKmerCounts;
    typedef TKmerMethods<TKmerCounts> TKMethods;


public:

    /// Default gap open penalty
    static const CNWAligner::TScore kDefaultGapOpen = -11;

    /// Default gap extension penalty
    static const CNWAligner::TScore kDefaultGapExtend = -1;

    /// Constructor; note that the actual sequences are 
    /// loaded via SetQueries()
    /// @param matrix_name The score matrix to use; limited to
    ///                  the same list of matrices as blast [in]
    /// @param gap_open Penalty for starting an internal gap [in]
    /// @param gap_extend Penalty for extend an internal gap; a gap
    ///                   of length 1 has (open + extension) penalty [in]
    /// @param end_gap_open Penalty for starting a gap at the beginning 
    ///                   or end of a sequence [in]
    /// @param end_gap_extend Penalty for extending a gap at the 
    ///                   beginning or end of a sequence [in]
    /// @param iterate True if conserved columns should be detected and
    ///                the alignment recomputed if any are found [in]
    /// @param blastp_evalue When running blast on the input sequences,
    ///                   keep hits that are this significant or better
    ///                   (i.e. have e-value this much or lower) [in]
    /// @param conserved_cutoff When looking for conserved columns, consider
    ///                   a column conserved if it manages a score
    ///                   of at least this much [in]
    /// @param filler_resfreq_boost When assigning residue frequencies to 
    ///                   portions of sequences not covered by domain
    ///                   hits, upweight the frequency associated with the
    ///                   actual residue at a given position by this much.
    ///                   Values range from 0 to 1; 0 implies using only
    ///                   background frequencies, 1 uses no background
    ///                   frequencies at all [in]
    /// @param pseudocount An additional factor that downweights the value
    ///                    of actual sequence data when computing column-
    ///                    wise scores (higher = more downweighting) [in]
    CMultiAligner(const char *matrix_name = "BLOSUM62",
                  CNWAligner::TScore gap_open = kDefaultGapOpen,
                  CNWAligner::TScore gap_extend = kDefaultGapExtend,
                  CNWAligner::TScore end_gap_open = kDefaultGapOpen,
                  CNWAligner::TScore end_gap_extend = kDefaultGapExtend,
                  bool iterate = false,
                  double blastp_evalue = 0.01,
                  double conserved_cutoff = 0.67,
                  double filler_resfreq_boost = 1.0,
                  double pseudocount = 2.0
                  );

    /// Destructor
    ~CMultiAligner();

    // ----------------- Getters -----------------------------


    /// Retrieve the current aligned results in Seq-align format.
    /// The Seq-align is of global type, with a single denseg
    /// @return The results
    ///
    CRef<objects::CSeq_align> GetSeqalignResults();

    /// Retrieve a selection of the current aligned results, 
    /// in Seq-align format. The Seq-align is of global type, 
    /// with a single denseg. Columns that have gaps in all the
    /// selected sequences are removed
    /// @param indices List of ordinal IDs of sequences that the
    ///                Seq-align will contain. Indices may appear
    ///                in any order and may be repeated
    /// @return The results
    ///
    CRef<objects::CSeq_align> GetSeqalignResults(vector<int>& indices);

    /// Retrieve a selection of an input Seq_align,
    /// in Seq-align format. The output Seq-align is of global type, 
    /// with a single denseg. Columns that have gaps in all the
    /// selected sequences are removed
    /// @param indices List of ordinal IDs of sequences that the
    ///                Seq-align will contain. Indices may appear
    ///                in any order and may be repeated
    /// @return The results
    ///

    static CRef<objects::CSeq_align> GetSeqalignResults(
                                  objects::CSeq_align& align, 
                                  vector<int>& indices);

    /// Retrieve the current aligned results in CSequence format.
    /// @return The results, on CSequence for each input sequence
    ///
    const vector<CSequence>& GetResults() const { return m_Results; }

    /// Retrieve the unaligned input sequences
    /// @return The input sequences
    ///
    const blast::TSeqLocVector& GetSeqLocs() const { return m_tQueries; }

    /// Retrieve the current list of domain hits. Each CHit
    /// corresponds to one domain hit; sequence 1 of the hit
    /// has the index of the input protein sequence (0...num_inputs - 1), 
    /// and sequence 2 gives the OID of the database sequence to which 
    /// the hit refers. Each domain hit contains block alignments
    /// as subhits, each of which means a conserved region
    /// @return The list of hits 
    ///
    const CHitList& GetDomainHits() const { return m_DomainHits; }

    /// Retrieve the current list of local hits. Each CHit
    /// represents a pairwise alignment between two input sequences
    /// @return The list of hits 
    ///
    const CHitList& GetLocalHits() const { return m_LocalHits; }

    /// Retrieve the current list of combined domain and local hits. 
    /// Each CHit represents a pairwise alignment between two input 
    /// sequences. If the CHit came from a domain hit that was matched up
    /// between two sequences, the matched up block alignments will be
    /// present as subhits
    /// @return The list of hits 
    ///
    const CHitList& GetCombinedHits() const { return m_CombinedHits; }

    /// Retrieve the current list of pattern hits. Each CHit
    /// represents a pairwise alignment between two input sequences,
    /// corresponding to a match to the same PROSITE pattern
    /// @return The list of hits 
    ///
    const CHitList& GetPatternHits() const { return m_PatternHits; }

    /// Retrieve the current list of user-defined constraints. Each CHit
    /// represents a pairwise alignment between two input sequences,
    /// @return The list of constraints 
    ///
    const CHitList& GetUserHits() const { return m_UserHits; }

    /// Retrieve the current internally generated phylogenetic tree.
    /// Tree leaves each contain the index of the query sequence at that
    /// leaf. The tree is strictly binary, and the tree root is not
    /// associated with any sequence
    /// @return The tree
    ///
    const TPhyTreeNode *GetTree() const { return m_Tree.GetTree(); }

    /// Retrieve the current open penalty for internal gaps
    /// @return The penalty
    ///
    CNWAligner::TScore GetGapOpen() const { return m_GapOpen; }

    /// Retrieve the current extend penalty for internal gaps
    /// @return The penalty
    ///
    CNWAligner::TScore GetGapExtend() const { return m_GapExtend; }

    /// Retrieve the current open penalty for start / end gaps
    /// @return The penalty
    ///
    CNWAligner::TScore GetEndGapOpen() const { return m_EndGapOpen; }

    /// Retrieve the current extend penalty for start / end gaps
    /// @return The penalty
    ///
    CNWAligner::TScore GetEndGapExtend() const { return m_EndGapExtend; }

    // ---------------------- Setters -----------------------------


    /// Load a new set of input sequences into the aligner.
    /// This automatically clears out the intermediate state
    /// of the last alignment
    /// @param queries The list of new query sequences
    ///
    void SetQueries(const blast::TSeqLocVector& queries);

    /// Load information needed for determining domain hits
    /// @param dbname Name of RPS BLAST database [in]
    /// @param blockfile Name of file containing the offset ranges of
    ///                  conserved blocks in each RPS database sequence [in]
    /// @param freqfile Name of file containing the residue frequencies
    ///                  associated with the sequences of the RPS database [in]
    /// @param rps_evalue When running RPS blast on the input sequences,
    ///                   keep hits that are this significant or better
    ///                   (i.e. have e-value this much or lower) [in]
    /// @param domain_resfreq_boost When assigning residue frequencies to 
    ///                   portions of sequences covered by domain
    ///                   hits, upweight the frequency associated with the
    ///                   actual residue at a given position by this much.
    ///                   Values range from 0 to 1; 0 implies using only
    ///                   the domain residue frequencies, 1 uses no domain
    ///                   frequencies at all (essentially ignoring the
    ///                   residue frequencies of domain hits) [in]
    ///
    void SetDomainInfo(const string& dbname, 
                       const string& blockfile,
                       const string& freqfile,
                       double rps_evalue = 0.01,
                       double domain_resfreq_boost = 0.5)
    { 
        m_RPSdb = dbname; 
        m_Blockfile = blockfile; 
        m_Freqfile = freqfile;
        m_RPSEvalue = rps_evalue;
        m_DomainResFreqBoost = domain_resfreq_boost;
    }

    /// Load information needed for determining PROSITE pattern hits
    /// @param patternfile Name of file containing the list of PROSITE
    ///                    regular expressions that will be applied to
    ///                    all of the input sequences [in]
    ///
    void SetPatternInfo(const string& patternfile) 
    { 
        m_PatternFile = patternfile; 
    }

    /// Set the score matrix the aligner will use. NOTE that at present
    /// any hits between sequences will always be scored using BLOSUM62;
    /// the matrix chosen here is only used when forming the complete
    /// alignment
    /// @param matrix_name The score matrix to use; limited to
    ///                  the same list of matrices as blast [in]
    ///
    void SetScoreMatrix(const char *matrix_name);

    /// Set the list of user-specified constraints between the sequences
    /// to be aligned
    /// @param hits the list of user-specified constraints
    ///
    void SetUserHits(CHitList& hits);

    /// Set the expect value to use for local hits. When running blast 
    /// on the input sequences, keep hits that are this significant or better
    ///                   (i.e. have e-value this much or lower)
    /// @param evalue The expect value to use
    ///
    void SetBlastpEvalue(double evalue) { m_BlastpEvalue = evalue; }

    /// Set the cutoff for conserved columns. When looking for conserved 
    /// columns, consider a column conserved if it manages a per-residue
    /// sum of pairs score of at least this much
    /// @param cutoff The cutoff value
    ///
    void SetConservedCutoff(double cutoff) { m_ConservedCutoff = cutoff; }

    /// Set the open penalty for internal gaps
    /// @param score The penalty
    ///
    void SetGapOpen(CNWAligner::TScore score) { m_GapOpen = score; }

    /// Set the extend penalty for internal gaps
    /// @param score The penalty
    ///
    void SetGapExtend(CNWAligner::TScore score) { m_GapExtend = score; }

    /// Set the open penalty for initial/terminal gaps
    /// @param score The penalty
    ///
    void SetEndGapOpen(CNWAligner::TScore score) { m_EndGapOpen = score; }

    /// Set the extend penalty for initial/terminal gaps
    /// @param score The penalty
    ///
    void SetEndGapExtend(CNWAligner::TScore score) { m_EndGapExtend = score; }

    /// Turn the output of internal aligner state on/off
    /// @param verbose true to turn on verbose logging, false to turn it off
    ///
    void SetVerbose(bool verbose) { m_Verbose = verbose; }

    /// Set the pseudocount factor to use
    /// @param pseudocount The factor
    ///
    void SetPseudocount(double pseudocount) { m_Pseudocount = pseudocount; }

    /// Turn iteration on or off
    /// @param iterate True if iteration is allowed
    ///
    void SetIterate(bool iterate) { m_Iterate = iterate; }

    /// Use the FastME algorithm to generate the tree
    /// (default is to use neighbor-joining)
    /// @param fastme True if FastME is used
    ///
    void SetFastmeTree(bool fastme) { m_FastMeTree = fastme; }

    /// Set query clustering parameters
    /// @param use_clusters Should query clustering be used
    /// @param kmer_len K-mer length for calculation of distances between 
    /// queries
    /// @param alph Selection of regular or compressed alphabet for calculation
    /// of distances between queries
    /// @param dist Measure of distance between queries
    /// @param max_dist Maximum cluster diameter
    ///
    void SetQueryClustersInfo(bool use_clusters, 
                    unsigned kmer_len = 4,
                    TKMethods::ECompressedAlphabet alph 
                    = TKMethods::eRegular,
                    TKMethods::EDistMeasures dist 
                    = TKMethods::eFractionCommonKmersGlobal,
                    double max_dist = 0.1)
    {
        m_UseClusters = use_clusters;
        m_KmerLength = kmer_len;
        m_ClustDistMeasure = dist;
        m_MaxClusterDist = max_dist;
        m_KmerAlphabet = alph;
    }

    // ---------------- Running the aligner -----------------------

    /// Clear out the state left by the previous alignment operation
    ///
    void Reset();

    /// Align the current set of input sequences (reset any existing
    /// alignment information). This function handles the generation
    /// of all internal state in the correct order. It is sufficient 
    /// for 'black box' applications that only want a final
    /// answer without tweaking internal state
    ///
    void Run();

    /// Run RPS blast on the input sequences and postprocess the results.
    /// Intended for applications that want fine-grained control of the 
    /// alignment process
    ///
    void FindDomainHits();

    /// Run blast on the input sequences and postprocess the results.
    /// Intended for applications that want fine-grained control of the 
    /// alignment process
    ///
    void FindLocalHits();

    /// Find PROSITE pattern hits on the input sequences.
    /// Intended for applications that want fine-grained control of the 
    /// alignment process
    ///
    void FindPatternHits();

    /// Given the current list of domain and local hits, generate a 
    /// phylogenetic tree that clusters the current input sequences.
    /// Intended for applications that want fine-grained control of the 
    /// alignment process
    ///
    void ComputeTree();

    /// Given the current domain, local, pattern and user hits, along with
    /// the current tree, compute a multiple alignment of the input sequences.
    /// Intended for applications that want fine-grained control of the 
    /// alignment process
    ///
    void BuildAlignment();


    // ---------------- Clustering queries -----------------------

    /// Find clusters of similar queries, select cluster representative 
    /// sequences, and prepare input to multiple alignement composed of 
    /// only representatives
    /// @return True if at least one cluster was found, false otherwise
    ///
    bool FindQueryClusters();

    /// Pair-wise align each cluster sequence to cluster representative
    ///
    void AlignInClusters();

    /// Compute profile residue frequencies for clusters. Frequencies are not
    /// normalized.
    ///
    void MakeClusterResidueFrequencies();

    /// Combine pair-wise in-cluster alignements with multiple alignments of
    /// cluster prototypes
    ///
    void MultiAlignClusters();



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
    
    class compare_sseg_db_idx {
    public:
        bool operator()(const SSegmentLoc& a, const SSegmentLoc& b) const {
            return a.seq_index < b.seq_index;
        }
    };

public:
    /// Column in an alignment used for combining result from multiple
    /// alignment and pair-wise in-cluster alignments
    typedef struct SColumn {
        /// Is the column an insertion from in-cluster alignment
        bool insert;
        
        /// Indeces of letters in this column
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
    blast::TSeqLocVector m_tQueries;
    vector<CSequence> m_QueryData;
    vector<CSequence> m_Results;

    
    bool m_UseClusters;
    TKMethods::ECompressedAlphabet m_KmerAlphabet;

    unsigned int m_KmerLength;
    double m_MaxClusterDist;
    TKMethods::EDistMeasures m_ClustDistMeasure;
    vector<CSequence> m_AllQueryData;
    blast::TSeqLocVector m_AllQueries;
    CClusterer m_Clusterer;
    vector< vector<Uint4> > m_ClusterGapPositions;

    CHitList m_DomainHits;
    string m_RPSdb;
    string m_Blockfile;
    string m_Freqfile;
    double m_RPSEvalue;
    double m_DomainResFreqBoost;

    CHitList m_LocalHits;
    double m_BlastpEvalue;
    double m_LocalResFreqBoost;

    CHitList m_CombinedHits;

    CHitList m_PatternHits;
    string m_PatternFile;

    CHitList m_UserHits;

    const char *m_MatrixName;

    CTree m_Tree;
    CPSSMAligner m_Aligner;
    double m_ConservedCutoff;
    double m_Pseudocount;
    CNWAligner::TScore m_GapOpen;
    CNWAligner::TScore m_GapExtend;
    CNWAligner::TScore m_EndGapOpen;
    CNWAligner::TScore m_EndGapExtend;

    bool m_Verbose;
    bool m_Iterate;
    bool m_FastMeTree;

    void x_LoadBlockBoundaries(string blockfile,
                      vector<SSegmentLoc>& blocklist);
    void x_FindRPSHits(CHitList& rps_hits);

    void x_RealignBlocks(CHitList& rps_hits,
                         vector<SSegmentLoc>& blocklist,
                         CProfileData& profile_data);
    void x_AssignRPSResFreqs(CHitList& rps_hits,
                             CProfileData& profile_data);
    void x_AssignDefaultResFreqs();

    void x_AddNewSegment(blast::TSeqLocVector& loc_list, 
                         blast::SSeqLoc& query, 
                         TOffset from, 
                         TOffset to, 
                         vector<SSegmentLoc>& seg_list,
                         int query_index);
    void x_MakeFillerBlocks(blast::TSeqLocVector& filler_locs,
                            vector<SSegmentLoc>& filler_segs);
    void x_AlignFillerBlocks(blast::TSeqLocVector& filler_locs, 
                             vector<SSegmentLoc>& filler_segs);

    void x_FindConsistentHitSubset();
    void x_FindAlignmentSubsets();
    SGraphNode * x_FindBestPath(vector<SGraphNode>& nodes);

    void x_BuildAlignmentIterative(vector<CTree::STreeEdge>& edges,
                                   double cluster_cutoff);
    void x_FindConservedColumns(vector<CSequence>& new_alignment,
                                CHitList& conserved);
    void x_AlignProgressive(const TPhyTreeNode *tree,
                            vector<CSequence>& query_data,
                            CNcbiMatrix<CHitList>& pair_info,
                            int iteration);
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
    void x_FindConstraints(vector<size_t>& constraint,
                           vector<CSequence>& alignment,
                           vector<CTree::STreeLeaf>& node_list1,
                           vector<CTree::STreeLeaf>& node_list2,
                           CNcbiMatrix<CHitList>& pair_info,
                           int iteration);
    double x_GetScoreOneCol(vector<CSequence>& align, 
                            int col);
    double x_GetScore(vector<CSequence>& align);

    CRef<objects::CSeq_align> x_GetSeqalign(vector<CSequence>& align,
                                            vector<int>& indices);
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif /* ALGO_COBALT___COBALT__HPP */
