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

#include <algo/align/splign/splign_formatter.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/general/Object_id.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CSplignFormatter::CSplignFormatter(const CSplign::TResults& results):
    m_splign_results(results)
{
    x_Init();
}


CSplignFormatter::CSplignFormatter (const CSplign& splign):
    m_splign_results(splign.GetResult())
{
    x_Init();
}


void CSplignFormatter::x_Init(void)
{
    const char kSeqId_not_set [] = "lcl|ID_not_set";
    CConstRef<CSeq_id> seqid_not_set (new CSeq_id (kSeqId_not_set));
    m_QueryId = m_SubjId = seqid_not_set;
}


void CSplignFormatter::SetSeqIds(CConstRef<objects::CSeq_id> id1,
                                 CConstRef<objects::CSeq_id> id2)
{
    m_QueryId = id1;
    m_SubjId = id2;
}


string CSplignFormatter::AsText(const CSplign::TResults* results) const
{
    if(results == 0) {
        results = &m_splign_results;
    }

    CNcbiOstrstream oss;
    oss.precision(3);
    
    ITERATE(CSplign::TResults, ii, *results) {
        
        for(size_t i = 0, seg_dim = ii->m_segments.size(); i < seg_dim; ++i) {
            
            const CSplign::SSegment& seg = ii->m_segments[i];
            
            oss << (ii->m_QueryStrand? '+': '-')
                << ii->m_id << '\t' 
                << m_QueryId->GetSeqIdString(true) << '\t' 
                << m_SubjId->GetSeqIdString(true) << '\t';
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
                oss << CNWAligner::s_RunLengthEncode(seg.m_details);
#ifdef GENOME_PIPELINE
                oss << '\t' << seg.m_score;
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
        results = &(m_splign_results);
    }

    CRef<CSeq_align_set> rv (new CSeq_align_set);
    CSeq_align_set::Tdata& data = rv->Set();


    ITERATE(CSplign::TResults, ii, *results) {
    
        vector<size_t> boxes;
        vector<string> transcripts;
        vector<CNWAligner::TScore> scores;

        for(size_t i = 0, seg_dim = ii->m_segments.size(); i < seg_dim; ++i) {

            const CSplign::SSegment& seg = ii->m_segments[i];

            if(seg.m_exon) {
                
                copy(seg.m_box, seg.m_box + 4, back_inserter(boxes));
                transcripts.push_back(seg.m_details);
                scores.push_back(seg.m_score);
            }
        }
       
        CRef<CSeq_align> sa (x_Compartment2SeqAlign(boxes,transcripts,scores));
        data.push_back(sa);
    }
    
    return rv;
}


double CalcIdentity(const string& transcript)
{
    Uint4 matches = 0;
    ITERATE(string, ii, transcript) { 
        if(*ii == 'M') {
            ++matches;  // std::count() not supported by some compilers
        }
    }
    return double(matches) / transcript.size();
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
  
    // create seq-align-set
    CRef< CSeq_align::C_Segs > segs (new CSeq_align::C_Segs);
    CSeq_align_set& sas = segs->SetDisc();
    list<CRef<CSeq_align> >& sas_data = sas.Set();
    
    for(size_t i = 0; i < num_exons; ++i) {

      CRef<CSeq_align> sa (new CSeq_align);
      sa->Reset();
      sa->SetDim(2);
      sa->SetType(CSeq_align::eType_global);
      
      // add dynprog score
      CRef<CScore> score (new CScore);
      CRef<CObject_id> id (new CObject_id);
      id->SetStr("splign");
      score->SetId(*id);
      CRef< CScore::C_Value > val (new CScore::C_Value);
      val->SetReal(scores[i]);
      score->SetValue(*val);
      CSeq_align::TScore& scorelist = sa->SetScore();
      scorelist.push_back(score);

      // add percent identity
      CRef<CScore> idty (new CScore);
      CRef<CObject_id> id_idty (new CObject_id);
      id_idty->SetStr("idty");
      idty->SetId(*id_idty);
      CRef< CScore::C_Value > val_idty (new CScore::C_Value);
      val_idty->SetReal(CalcIdentity(transcripts[i]));
      idty->SetValue(*val_idty);
      scorelist.push_back(idty);

      CRef<CSeq_align::C_Segs> segs (new CSeq_align::C_Segs);
      CDense_seg& ds = segs->SetDenseg();

      const size_t* box = &(*(boxes.begin() + i*4));
      const size_t query_start = box[0];
      ENa_strand query_strand = box[0] <= box[1]? eNa_strand_plus:
          eNa_strand_minus;
      const size_t subj_start = box[2];
      ENa_strand subj_strand = box[2] <= box[3]? eNa_strand_plus:
          eNa_strand_minus;
      ds.FromTranscript(query_start, query_strand, subj_start, subj_strand,
                        transcripts[i]);
      // don't include strands when both are positive
      if(query_strand == eNa_strand_plus && subj_strand == eNa_strand_plus) {
          ds.ResetStrands();
      }

      vector< CRef< CSeq_id > > &ids = ds.SetIds();

      CRef<CSeq_id> id_query (new CSeq_id());
      id_query->Assign(*m_QueryId);
      ids.push_back(id_query);

      CRef<CSeq_id> id_subj (new CSeq_id());
      id_subj->Assign(*m_SubjId);
      ids.push_back(id_subj);

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
 * Revision 1.17  2005/06/20 17:49:26  kapustin
 * Strip strand info when both strands are positive
 *
 * Revision 1.16  2005/05/24 19:36:27  kapustin
 * RLE() -> CNWAligner::s_RunLengthEncode()
 *
 * Revision 1.15  2005/01/07 20:56:41  kapustin
 * Specify percent identity score when in ASN mode
 *
 * Revision 1.14  2005/01/04 15:48:41  kapustin
 * Move SetSeqIds() implementation to the cpp file
 *
 * Revision 1.13  2005/01/03 22:47:35  kapustin
 * Implement seq-ids with CSeq_id instead of generic strings
 *
 * Revision 1.12  2004/12/09 22:37:16  kapustin
 * Do not assume query plus strand when converting to seq-align
 *
 * Revision 1.11  2004/11/29 14:37:16  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be 
 * specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two 
 * additional parameters to specify starting coordinates.
 *
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
