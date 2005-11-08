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

File name: hit.hpp

Author: Jason Papadopoulos

Contents: Interface for CHit class

******************************************************************************/

/// @file hit.hpp
/// Interface for CHit class, used to encapsulate
/// operations involving pairwise alignments.
/// <pre>
/// Given an alignment with its traceback, this class can
/// compute subsets of the alignment from subsets of the traceback.
/// This can sometimes be tricky; for example, given an alignment
/// described by
///
///                       11111
/// traceback   012345678901234
///
/// query   10  AAAAA---AAAAAAA 21
/// subject 50  AAAAAAAAA--AAAA 62
///
/// the traceback is in the range 0 to 14, the query range is 10 to 21,
/// and the subject range is 50 to 62. For a subject range of [50,54] the
/// query range is [10,14]. However, a subject range of [50,56] still has
/// a query range of [10,14]. Some other examples:
///
///  start with        of        compute        of
///  -------------   -------    -------------   -----
///  query range     [12,15]    subject range   [52,58]
///  query range     [15,16]    subject range   [58,59]
///  subject range   [55,56]    query range     [15,14] <- inverted!
///
/// In general, query ranges and subject ranges are assumed to exclude 
/// gaps at either end of the range. For scoring purposes, the traceback 
/// range is what's specified, as this is unambiguous.
/// </pre>


#ifndef _ALGO_COBALT_HIT_HPP_
#define _ALGO_COBALT_HIT_HPP_

#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/gapinfo.h>
#include <algo/align/nw/nw_aligner.hpp>

#include <algo/cobalt/base.hpp>
#include <algo/cobalt/seq.hpp>
#include <algo/cobalt/traceback.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

class CHit {

public:
   static const int kMinHitSize = 2;
   typedef vector<CHit *> TSubHit;

   int m_SeqIndex1;
   int m_SeqIndex2;

   int m_Score;
   double m_BitScore;
   double m_HitRate;

   TRange m_SeqRange1;
   TRange m_SeqRange2;

   CHit(int seq1_index, int seq2_index) 
          : m_SeqIndex1(seq1_index), m_SeqIndex2(seq2_index), 
            m_Score(0), m_BitScore(0.0), m_HitRate(1.0),
            m_SeqRange1(0,0), m_SeqRange2(0,0) {}

   CHit(int seq1_index, int seq2_index, BlastHSP *hsp)
          : m_SeqIndex1(seq1_index), m_SeqIndex2(seq2_index), 
            m_Score(hsp->score), m_BitScore(0.0), m_HitRate(1.0),
            m_SeqRange1(hsp->query.offset, hsp->query.end - 1),
            m_SeqRange2(hsp->subject.offset, hsp->subject.end - 1),
            m_EditScript(hsp->gap_info) { VerifyHit(); }

   CHit(int seq1_index, int seq2_index,
        TRange seq_range1, TRange seq_range2,
        int score, CEditScript edit_script)
       : m_SeqIndex1(seq1_index), m_SeqIndex2(seq2_index),
         m_Score(score), m_BitScore(0.0), m_HitRate(1.0),
         m_SeqRange1(seq_range1), m_SeqRange2(seq_range2),
         m_EditScript(edit_script) { VerifyHit(); }

   ~CHit() 
   {
       for (int i = 0; i < (int)m_SubHit.size(); i++)
            delete m_SubHit[i];
   }

   void InsertSubHit(CHit *hit) { m_SubHit.push_back(hit); }
   TSubHit& GetSubHit() { return m_SubHit; }
   CEditScript& GetEditScript() { return m_EditScript; }
   bool HasSubHits() { return !(m_SubHit.empty()); }
   void AddUpSubHits();
   CHit * Copy();

    void GetRangeFromSeq2(TRange seq_range2,
                          TRange& seq_range1,
                          TRange& new_seq_range2,
                          TRange& traceback_range);
    
    void GetRangeFromSeq1(TRange seq_range1,
                          TRange& new_seq_range1,
                          TRange& seq_range2,
                          TRange& traceback_range);

    void VerifyHit();

    void ResolveSubHitConflicts(CSequence& seq1,
                                int **seq2_pssm,
                                CNWAligner::TScore gap_open,
                                CNWAligner::TScore gap_extend);

private:
   CEditScript m_EditScript;
   vector<CHit *> m_SubHit;
};


END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // _ALGO_COBALT_HIT_HPP_

/*--------------------------------------------------------------------
  $Log$
  Revision 1.2  2005/11/08 17:42:17  papadopo
  Rearrange includes to be self-sufficient

  Revision 1.1  2005/11/07 18:15:52  papadopo
  Initial revision

--------------------------------------------------------------------*/
