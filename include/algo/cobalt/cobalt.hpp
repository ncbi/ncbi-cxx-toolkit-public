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

#ifndef _ALGO_COBALT_COBALT_HPP_
#define _ALGO_COBALT_COBALT_HPP_

#include <util/math/matrix.hpp>
#include <corelib/ncbifile.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/core/blast_stat.h>
#include <algo/align/nw/nw_pssm_aligner.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <algo/cobalt/base.hpp>
#include <algo/cobalt/seq.hpp>
#include <algo/cobalt/hit.hpp>
#include <algo/cobalt/hitlist.hpp>
#include <algo/cobalt/dist.hpp>
#include <algo/cobalt/tree.hpp>

/// @file cobalt.hpp
/// Interface for CMultiAligner

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

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


typedef struct SGraphNode {
    CHit *hit;                     ///< the alignment represented by this node
    int list_pos;
    struct SGraphNode *path_next;  ///< the optimal path node belongs to
    double best_score;             ///< the score of that optimal path
                                   ///  (assumes this node is the first entry
                                   ///  in the path)

    SGraphNode() {}

    SGraphNode(CHit *h, int list_pos) 
        : hit(h), list_pos(list_pos), 
          path_next(0), best_score(0.0) {}

} SGraphNode;



class CProfileData {
public:
    typedef enum {
        eGetResFreqs,
        eGetPssm
    } EMapChoice;

    CProfileData() {}
    ~CProfileData() { Clear(); }

    Int4 * GetSeqOffsets() const { return m_SeqOffsets; }
    Int4 ** GetPssm() const { return m_PssmRows; }
    double ** GetResFreqs() const { return m_ResFreqRows; }

    void Load(EMapChoice choice, 
              string dbname,
              string resfreq_file = "");
    void Clear();

private:
    CMemoryFile *m_ResFreqMmap;
    CMemoryFile *m_PssmMmap;
    Int4 *m_SeqOffsets;
    Int4 **m_PssmRows;
    double **m_ResFreqRows;
};

class CMultiAligner : public CObject
{
public:
    static const CNWAligner::TScore kDefaultGapOpen = -11;
    static const CNWAligner::TScore kDefaultGapExtend = -1;
    static const double kDefaultEvalue = 0.01;

    CMultiAligner(const char *matrix_name = "BLOSUM62",
                  CNWAligner::TScore gap_open = kDefaultGapOpen,
                  CNWAligner::TScore gap_extend = kDefaultGapExtend,
                  CNWAligner::TScore end_gap_open = kDefaultGapOpen,
                  CNWAligner::TScore end_gap_extend = kDefaultGapExtend,
                  double blastp_evalue = kDefaultEvalue,
                  double conserved_cutoff = 2.0,
                  double filler_res_boost = 1.0
                  );

    ~CMultiAligner();

    CRef<objects::CSeq_align> GetSeqalignResults() const;
    const vector<CSequence>& GetResults() const { return m_Results; }
    const blast::TSeqLocVector& GetSeqLocs() const { return m_tQueries; }
    const CHitList& GetDomainHits() const { return m_DomainHits; }
    const CHitList& GetLocalHits() const { return m_LocalHits; }
    const CHitList& GetCombinedHits() const { return m_CombinedHits; }
    const CHitList& GetPatternHits() const { return m_PatternHits; }
    const CHitList& GetUserHits() const { return m_UserHits; }
    const TPhyTreeNode *GetTree() const { return m_Tree.GetTree(); }
    CNWAligner::TScore GetGapOpen() const { return m_GapOpen; }
    CNWAligner::TScore GetGapExtend() const { return m_GapExtend; }
    CNWAligner::TScore GetEndGapOpen() const { return m_EndGapOpen; }
    CNWAligner::TScore GetEndGapExtend() const { return m_EndGapExtend; }



    void SetQueries(const blast::TSeqLocVector& queries);

    void SetDomainInfo(const string& dbname, 
                       const string& blockfile,
                       const string& freqfile,
                       double rps_evalue = kDefaultEvalue,
                       double domain_resfreq_boost = 0.5)
    { 
        m_RPSdb = dbname; 
        m_Blockfile = blockfile; 
        m_Freqfile = freqfile;
        m_RPSEvalue = rps_evalue;
        m_DomainResFreqBoost = domain_resfreq_boost;
    }

    void SetPatternInfo(const string& patternfile) { m_PatternFile = patternfile; }
    void SetScoreMatrix(const char *matrix_name);
    void SetUserHits(CHitList& hits) { m_UserHits += hits; }
    void SetBlastpEvalue(double evalue) { m_BlastpEvalue = evalue; }
    void SetConservedCutoff(double cutoff) { m_ConservedCutoff = cutoff; }
    void SetGapOpen(CNWAligner::TScore score) { m_GapOpen = score; }
    void SetGapExtend(CNWAligner::TScore score) { m_GapExtend = score; }
    void SetEndGapOpen(CNWAligner::TScore score) { m_EndGapOpen = score; }
    void SetEndGapExtend(CNWAligner::TScore score) { m_EndGapExtend = score; }
    void SetVerbose(bool verbose) { m_Verbose = verbose; }

    void FindDomainHits();
    void FindLocalHits();
    void FindPatternHits();
    void ComputeTree();
    void BuildAlignment();
    void Run();
    void Reset();

private:
    static const int kRpsScaleFactor = 100;

    blast::TSeqLocVector m_tQueries;
    vector<CSequence> m_QueryData;
    vector<CSequence> m_Results;

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

    CTree m_Tree;
    CPSSMAligner m_Aligner;
    double m_ConservedCutoff;
    CNWAligner::TScore m_GapOpen;
    CNWAligner::TScore m_GapExtend;
    CNWAligner::TScore m_EndGapOpen;
    CNWAligner::TScore m_EndGapExtend;
    Blast_KarlinBlk *m_KarlinBlk;

    bool m_Verbose;

    void x_FindRPSHits(CHitList& rps_hits);
    void x_RealignBlocks(CHitList& rps_hits,
                         vector<SSegmentLoc>& blocklist,
                         CProfileData& profile_data);
    void x_AssignRPSResFreqs(CHitList& rps_hits,
                             CProfileData& profile_data);

    void x_AssignDefaultResFreqs();
    void x_MakeFillerBlocks(blast::TSeqLocVector& filler_locs,
                            vector<SSegmentLoc>& filler_segs);
    void x_AlignFillerBlocks(blast::TSeqLocVector& filler_locs, 
                             vector<SSegmentLoc>& filler_segs);

    void x_FindConsistentHitSubset();
    void x_AssignHitRate();
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
    int x_RealignSequences(const TPhyTreeNode *input_cluster,
                           vector<CSequence>& alignment,
                           CNcbiMatrix<CHitList>& pair_info,
                           int sp_score,
                           int iteration);
    void x_AlignProfileProfile(vector<CTree::STreeLeaf>& node_list1,
                               vector<CTree::STreeLeaf>& node_list2,
                               vector<CSequence>& alignment,
                               CNcbiMatrix<CHitList>& pair_info,
                               int iteration);
    void x_FindConstraints(vector<size_t>& constraint,
                           vector<CSequence>& alignment,
                           vector<CTree::STreeLeaf>& node_list1,
                           int freq1_size,
                           vector<CTree::STreeLeaf>& node_list2,
                           int freq2_size,
                           CNcbiMatrix<CHitList>& pair_info,
                           int iteration);
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif /* _ALGO_COBALT_COBALT_HPP_ */

/*--------------------------------------------------------------------
  $Log$
  Revision 1.2  2005/11/08 17:41:34  papadopo
  1. rearrange includes to be self-sufficient
  2. do not automatically use blast namespace

  Revision 1.1  2005/11/07 18:15:52  papadopo
  Initial revision

--------------------------------------------------------------------*/
