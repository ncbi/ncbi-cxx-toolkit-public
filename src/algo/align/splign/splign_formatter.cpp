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
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <algo/align/splign/splign_formatter.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Dense_seg.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CSplignFormatter::CSplignFormatter (const CSplign& splign):
    m_splign(&splign)
{
    static const char kErrMsg[] = "ID_not_set";
    m_QueryId = m_SubjId = kErrMsg;
}


string CSplignFormatter::AsText(void) const
{

  CNcbiOstrstream oss;
  oss.precision(3);
  const vector<CSplign::SAlignedCompartment>& acs = m_splign->GetResult();

  ITERATE(vector<CSplign::SAlignedCompartment>, ii, acs) {
    
    for(size_t i = 0, seg_dim = ii->m_segments.size(); i < seg_dim; ++i) {

      const CSplign::SSegment& seg = ii->m_segments[i];

      oss << ii->m_id << '\t' << m_QueryId << '\t' << m_SubjId << '\t';
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
        oss << '\t' << ScoreByTranscript(*(m_splign->GetAligner()),
                                         seg.m_details.c_str());
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


CRef<CSeq_align_set> CSplignFormatter::AsSeqAlignSet(void) const
{
    const vector<CSplign::SAlignedCompartment>& acs = m_splign->GetResult();
    CRef<CSeq_align_set> rv (new CSeq_align_set);
    CSeq_align_set::Tdata& data = rv->Set();


    ITERATE(vector<CSplign::SAlignedCompartment>, ii, acs) {
    
        vector<size_t> boxes;
        vector<string> transcripts;
        vector<CNWAligner::TScore> scores;

        for(size_t i = 0, seg_dim = ii->m_segments.size(); i < seg_dim; ++i) {

            const CSplign::SSegment& seg = ii->m_segments[i];

            if(seg.m_exon) {
                
                if(ii->m_QueryStrand) {
                    boxes.push_back(ii->m_qmin + seg.m_box[0]);
                    boxes.push_back(ii->m_qmin + seg.m_box[1]);
                }
                else {
                    boxes.push_back(ii->m_mrnasize - seg.m_box[0] - 1);
                    boxes.push_back(ii->m_mrnasize - seg.m_box[1] - 1);
                }

                if(ii->m_SubjStrand) {
                    boxes.push_back(ii->m_smin + seg.m_box[2]);
                    boxes.push_back(ii->m_smin + seg.m_box[3]);
                }
                else {
                    boxes.push_back(ii->m_smax - seg.m_box[2]); 
                    boxes.push_back(ii->m_smax - seg.m_box[3]);
                }

                transcripts.push_back(seg.m_details);
                scores.push_back(ScoreByTranscript(*(m_splign->GetAligner()),
                                                   seg.m_details.c_str()));
            }
        }
       
        CRef<CSeq_align> sa (x_Compartment2SeqAlign(boxes,transcripts,scores));
        data.push_back(sa);
    }
    
    return rv;
}


CRef<CSeq_align> CSplignFormatter::x_Compartment2SeqAlign (
    const vector<size_t>& boxes,
    const vector<string>& transcripts,
    const vector<int>&    scores ) const
{
    const size_t num_exons = boxes.size() / 4;

    CRef<CSeq_align> sa (new CSeq_align);

    sa->Reset();

    // this is a discontinuous alignment
    sa->SetType(CSeq_align::eType_disc);
    sa->SetDim(2);
  
    // seq-ids
    CRef<CSeq_id> id1 ( new CSeq_id (m_QueryId) );
    if(id1->Which()==CSeq_id::e_not_set) {
        id1.Reset(new CSeq_id(CSeq_id::e_Local, m_QueryId, kEmptyStr));
    }
    
    CRef<CSeq_id> id2 ( new CSeq_id (m_SubjId) );
    if(id2->Which()==CSeq_id::e_not_set) {
        id2.Reset(new CSeq_id(CSeq_id::e_Local, m_SubjId, kEmptyStr));
    }

    // create seq-align-set
    CRef< CSeq_align::C_Segs > segs (new CSeq_align::C_Segs);
    CSeq_align_set& sas = segs->SetDisc();
    list<CRef<CSeq_align> >& sas_data = sas.Set();
    
    for(size_t i = 0; i < num_exons; ++i) {
      CRef<CSeq_align> sa (new CSeq_align);
      sa->Reset();
      sa->SetDim(2);
      sa->SetType(CSeq_align::eType_global);
      CRef<CScore> score (new CScore);
      CRef<CObject_id> id (new CObject_id);
      id->SetStr("splign");
      score->SetId(*id);
      CRef< CScore::C_Value > val (new CScore::C_Value);
      val->SetInt(scores[i]);
      score->SetValue(*val);
      CSeq_align::TScore& scorelist = sa->SetScore();
      scorelist.push_back(score);

      CRef<CSeq_align::C_Segs> segs (new CSeq_align::C_Segs);
      CDense_seg& ds = segs->SetDenseg();
      ds.SetNumseg(0);
      ds.SetDim(2);

      vector< CRef< CSeq_id > > &ids = ds.SetIds();
      ids.push_back(id1);
      ids.push_back(id2);

      const size_t* box = &(*(boxes.begin() + i*4));
      x_Exon2DS(box, transcripts[i], &ds);
      sa->SetSegs(*segs);
      sas_data.push_back(sa);
    }
    
    sa->SetSegs(*segs);

    return sa;
}


void CSplignFormatter::x_Exon2DS(const size_t* box, const string& trans,
                                 CDense_seg* pds ) const
{
    bool strand = box[2] <= box[3];

    CDense_seg& ds = *pds;
    vector< TSignedSeqPos > &starts  = ds.SetStarts();
    vector< TSeqPos >       &lens    = ds.SetLens();
    vector< ENa_strand >    &strands = ds.SetStrands();
    
    // iterate through the transcript
    size_t seg_count = 0;

    size_t start1 = 0, pos1 = 0; // relative to exon start in mrna
    size_t start2 = 0, pos2 = 0; // and genomic
    size_t seg_len = 0;
	
    string::const_iterator ib = trans.begin(), ie = trans.end(), ii = ib;
    unsigned char seg_type;
    char c = *ii++;
    if(c == 'M' || c == 'R') {
      seg_type = 0; // matches and mismatches
      ++pos1;
      ++pos2;
    }
    else if (c == 'I') {
      seg_type = 1;  // inserts
      ++pos2;
    }
    else {
      seg_type = 2;  // dels
      ++pos1;
    }
    
    while(ii < ie) {
      c = *ii;
      if(isalpha(c)) {
	if(seg_type == 0 && (c == 'M' || c == 'R')) {
	  ++pos1;
	  ++pos2;
	  ++ii;
	}
	else {
	  // close current seg
	  starts.push_back((seg_type == 1)? -1: TSignedSeqPos(box[0]+start1));
	  starts.push_back((seg_type == 2)? -1:
                           TSignedSeqPos(box[2]+(strand?start2:(1-pos2))));
	  strands.push_back(eNa_strand_plus);
	  strands.push_back(strand? eNa_strand_plus: eNa_strand_minus);
	  switch(seg_type) {
	  case 0: seg_len = pos1 - start1; break;
	  case 1: seg_len = pos2 - start2; break;
	  case 2: seg_len = pos1 - start1; break;
	  }
	  lens.push_back(seg_len);
	  ++seg_count;
	  
	  // start a new seg
	  start1 = pos1;
	  start2 = pos2;

	  if(c == 'M' || c == 'R') {
	    seg_type = 0; // matches and mismatches
	    ++pos1;
	    ++pos2;
	  }
	  else if (c == 'I') {
	    seg_type = 1;  // inserts
	    ++pos2;
	  }
	  else {
	    seg_type = 2;  // dels
	    ++pos1;
	  }

	  ++ii;
	}
      }
      else {
	size_t len = 0;
	while(ii < ie && ('0' <= *ii && *ii <= '9')) {
	  len = 10*len + *ii - '0';
	  ++ii;
	}
	--len;
	switch(seg_type) {
	case 0: pos1 += len; pos2 += len; break;
	case 1: pos2 += len; break;
	case 2: pos1 += len; break;
	}
      }
    }
    
    starts.push_back( (seg_type == 1)? -1: TSignedSeqPos(box[0] + start1) );
    starts.push_back( (seg_type == 2)? -1:
		      TSignedSeqPos(box[2] + (strand? start2: (1 - pos2))) );
    strands.push_back(eNa_strand_plus);
    strands.push_back(strand? eNa_strand_plus: eNa_strand_minus);	
    switch(seg_type) {
    case 0: seg_len = pos1 - start1; break;
    case 1: seg_len = pos2 - start2; break;
    case 2: seg_len = pos1 - start1; break;
    }
    lens.push_back(seg_len);
    ++seg_count;

    ds.SetNumseg(seg_count);
}




END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.5  2004/05/04 15:23:45  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.4  2004/04/30 15:00:47  kapustin
 * Support ASN formatting
 *
 * Revision 1.3  2004/04/23 14:37:44  kapustin
 * *** empty log message ***
 *
 *
 * ===========================================================================
 */
