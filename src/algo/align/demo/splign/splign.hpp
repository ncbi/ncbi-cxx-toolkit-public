#ifndef ALGO_DEMO_SPLIGN_SPLIGN__HPP
#define ALGO_DEMO_SPLIGN_SPLIGN__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin
*
* File Description:
*   CSplign class definition
*
*/

#include <corelib/ncbistd.hpp>
#include <algo/align/nw_spliced_aligner.hpp>

BEGIN_NCBI_SCOPE

class CSplign
{
public:

  CSplign(CSplicedAligner* aligner);

  // setters
  void SetSequences(const char* seq1, size_t len1,
		    const char* seq2, size_t len2);
  void SetPattern(const vector<size_t>& pattern);
  void SetMinExonIdentity(double idty);
  void EnforceEndGapsDetection(bool enforce);

  // a segment can represent an exon or an unaligning piece of mRna (a gap)
  struct SSegment {
    bool   m_exon; // false if gap
    double m_idty;
    size_t m_len;
    size_t m_box [4];
    string m_annot; // short description like AG<exon>GT
    string m_details; // transcript for exons, sequence for gaps

    void ImproveFromLeft(const CNWAligner* aligner,
			 const char* seq1, const char* seq2);
    void ImproveFromRight(const CNWAligner* aligner,
			  const char* seq1, const char* seq2);
  };

  const vector<SSegment>* Run(void);

protected:

  // active ingridient :-)
  CSplicedAligner*    m_aligner;

  // sequence data
  const char*         m_Seq1;
  size_t              m_SeqLen1;
  const char*         m_Seq2;
  size_t              m_SeqLen2;

  // alignment pattern
  vector<size_t>      m_pattern;

  // min exon idty - anything less would be marked as gap
  double              m_minidty;

  // mandatory end gap detection flag
  bool                m_endgaps;

  // output
  vector<SSegment>    m_out;

  // alignment map
  struct SAlnMapElem {
    size_t m_box [4];
    int    m_pattern_start, m_pattern_end;
  };
  vector<SAlnMapElem> m_alnmap;

  // gap evaluation
  double x_EvalMinExonIdty(size_t q0, size_t q1, size_t s0, size_t s1);
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
 * ===========================================================================
 */

#endif
