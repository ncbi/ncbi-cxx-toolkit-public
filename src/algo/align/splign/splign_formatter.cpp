/* $Id$
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
 * Author:  Yuri Kapustin
 *
 * ===========================================================================
 *
 */

#include "splign_util.hpp"

#include <algo/align/splign_formatter.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CSplignFormatter::CSplignFormatter (const CSplign& splign):
    m_splign(&splign)
{
}


string CSplignFormatter::AsText(void) const
{

  CNcbiOstrstream oss;
  oss.precision(3);
  const vector<CSplign::SAlignedCompartment>& acs = m_splign->GetResult();

  ITERATE(vector<CSplign::SAlignedCompartment>, ii, acs) {
    
    for(size_t i = 0, seg_dim = ii->m_segments.size(); i < seg_dim; ++i) {

      const CSplign::SSegment& seg = ii->m_segments[i];

      oss << ii->m_id << '\t' << ii->m_query << '\t' << ii->m_subj << '\t';
      if(seg.m_exon) {
        oss << seg.m_idty << '\t';
      }
      else {
        oss << "-\t";
      }
      
      oss << seg.m_len << '\t';
      
      if(ii->m_QueryStrand) {
        oss << ii->m_qmin + seg.m_box[0] + 1 << '\t'
            << ii->m_qmin + seg.m_box[1] + 1 << '\t';
      }
      else {
        oss << ii->m_mrnasize - seg.m_box[0] << '\t'
            << ii->m_mrnasize - seg.m_box[1] << '\t';
      }
      
      if(seg.m_exon) {
        
        if(ii->m_SubjStrand) {
          oss << ii->m_smin + seg.m_box[2] + 1 << '\t' 
              << ii->m_smin + seg.m_box[3] + 1 << '\t';
        }
        else {
          oss << ii->m_smax - seg.m_box[2] + 1 << '\t' 
              << ii->m_smax - seg.m_box[3] + 1 << '\t';
        }
      }
      else {
        oss << "-\t-\t";
      }
      
      if(seg.m_exon) {
        
        oss << seg.m_annot << '\t';
        oss << RLE(seg.m_details);
#ifdef GENOME_PIPELINE
        oss << '\t' << ScoreByTranscript(*(m_splign->GetAligner()), seg.m_details.c_str());
#endif
      }
      else {
        if(i == 0) {
          oss << "<L-Gap>\t";
        }
        else if(i == seg_dim - 1) {
          oss << "<R-Gap>\t";
        }
        else {
          oss << "<M-Gap>\t";
        }
        oss << '-';
#ifdef GENOME_PIPELINE
        oss << "\t-";
#endif
      }
      oss << endl;
    }

  }

  return CNcbiOstrstreamToString(oss);
}


vector<CRef<CSeq_annot> > CSplignFormatter::AsSeqAnnotVector(void) const
{
    const vector<CSplign::SAlignedCompartment>& results = m_splign->GetResult();
    vector<CRef<CSeq_annot> > rv (results.size());

    for(size_t i = 0, n = rv.size(); i < n; ++i) {

        rv[i].Reset(new CSeq_annot);
        CSeq_annot::TData& seq_annot_data = rv[i]->SetData();
        CSeq_annot::TData::TAlign* seq_align_list = &(seq_annot_data.SetAlign());
        
        vector<size_t> boxes;
        vector<string> transcripts;
        vector<CNWAligner::TScore> scores;

        const CSplign::SAlignedCompartment& ac = results[i];
    }
    return rv;
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.3  2004/04/23 14:37:44  kapustin
 * *** empty log message ***
 *
 *
 * ===========================================================================
 */
