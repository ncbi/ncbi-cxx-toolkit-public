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

#include <ncbi_pch.hpp>
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


string CSplignFormatter::AsText(const CSplign::TResults* results) const
{
    if(results == 0) {
        results = &(m_splign->GetResult());
    }

    CNcbiOstrstream oss;
    oss.precision(3);
    
    ITERATE(CSplign::TResults, ii, *results) {
        
        for(size_t i = 0, seg_dim = ii->m_segments.size(); i < seg_dim; ++i) {
            
            const CSplign::SSegment& seg = ii->m_segments[i];
            
            oss << (ii->m_QueryStrand? '+': '-')
                << ii->m_id << '\t' << m_QueryId << '\t' << m_SubjId << '\t';
            if(seg.m_exon) {
                oss << seg.m_idty << '\t';
            }
            else {
                oss << "-\t";
            }
            
            oss << seg.m_len << '\t'
                << seg.m_box[0] + 1 << '\t' << seg.m_box[1] + 1 << '\t';
      
            if(seg.m_exon) {
                oss << seg.m_box[2] + 1 << '\t' << seg.m_box[3] + 1 << '\t';
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


CRef<CSeq_align_set> CSplignFormatter::AsSeqAlignSet(const CSplign::TResults*
                                                     results) const
{
    if(results == 0) {
        results = &(m_splign->GetResult());
    }

    CRef<CSeq_align_set> rv (new CSeq_align_set);
    CSeq_align_set::Tdata& data = rv->Set();


    ITERATE(vector<CSplign::SAlignedCompartment>, ii, *results) {
    
        vector<size_t> boxes;
        vector<string> transcripts;
        vector<CNWAligner::TScore> scores;

        for(size_t i = 0, seg_dim = ii->m_segments.size(); i < seg_dim; ++i) {

            const CSplign::SSegment& seg = ii->m_segments[i];

            if(seg.m_exon) {
                
                copy(seg.m_box, seg.m_box + 4, back_inserter(boxes));
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
    const vector<CNWAligner::TScore>&    scores ) const
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
      val->SetReal(scores[i]);
      score->SetValue(*val);
      CSeq_align::TScore& scorelist = sa->SetScore();
      scorelist.push_back(score);

      CRef<CSeq_align::C_Segs> segs (new CSeq_align::C_Segs);
      CDense_seg& ds = segs->SetDenseg();

      const size_t* box = &(*(boxes.begin() + i*4));
      const size_t query_start = box[0];
      ENa_strand query_strand = eNa_strand_plus;
      const size_t subj_start = box[2];
      ENa_strand subj_strand = box[2] <= box[3]? eNa_strand_plus:
          eNa_strand_minus;
      ds.FromTranscript(query_start, query_strand, subj_start, subj_strand,
                        transcripts[i]);
      vector< CRef< CSeq_id > > &ids = ds.SetIds();
      ids.push_back(id1);
      ids.push_back(id2);

      sa->SetSegs(*segs);
      sas_data.push_back(sa);
    }
    
    sa->SetSegs(*segs);

    return sa;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.10  2004/11/04 17:55:46  kapustin
 * Use CDense_seg::FromTrancsript() to format dense-segs
 *
 * Revision 1.9  2004/08/12 20:07:26  kapustin
 * Fix score type in x_Compartment2SeqAlign
 *
 * Revision 1.8  2004/06/21 17:46:18  kapustin
 * Add result param to AsText and AsSeqAlignSet with zero default
 *
 * Revision 1.7  2004/06/03 19:29:07  kapustin
 * Minor fixes
 *
 * Revision 1.6  2004/05/24 16:13:57  gorelenk
 * Added PCH ncbi_pch.hpp
 *
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
