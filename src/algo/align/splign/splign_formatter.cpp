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

#include <algo/align/nw/align_exception.hpp>
#include <algo/align/splign/splign_formatter.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Score_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Splice_site.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>

#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

#include <algorithm>

#include "splign_util.hpp"
#include "messages.hpp"

#include <algorithm>

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


string CSplignFormatter::AsExonTable(
    const CSplign::TResults* results, int flags) const
{
    if(results == 0) {
        results = &m_splign_results;
    }

    CNcbiOstrstream oss;
    oss.precision(3);

    const bool print_exon_scores ((flags & eTF_NoExonScores)? false: true);
    const bool use_fasta_style_ids (flags & eTF_UseFastaStyleIds);
    

    const string querystr (use_fasta_style_ids?
                           m_QueryId->AsFastaString():
                           m_QueryId->GetSeqIdString(true));
    const string subjstr (use_fasta_style_ids?
                          m_SubjId->AsFastaString():
                          m_SubjId->GetSeqIdString(true));

    ITERATE(CSplign::TResults, ii, *results) {

        for(size_t i (0), seg_dim (ii->m_Segments.size()); i < seg_dim; ++i) {
            
            const CSplign::TSegment& seg (ii->m_Segments[i]);
            
            oss << (ii->m_QueryStrand? '+': '-')
                << ii->m_Id << '\t' 
                << querystr << '\t' 
                << subjstr << '\t';
            if(seg.m_exon) {
                oss << seg.m_idty << '\t';
            }
            else {
                oss << "-\t";
            }
            
            oss << seg.m_len << '\t'
                << seg.m_box[0] + 1 << '\t' << seg.m_box[1] + 1 << '\t';
            
            if(seg.m_exon) {
                oss << seg.m_box[2] + 1 << '\t' 
                    << seg.m_box[3] + 1 << '\t';
            }
            else {
                oss << "-\t-\t";
            }
            
            if(seg.m_exon) {
                
                oss << seg.m_annot << '\t';
                oss << CAlignShadow::s_RunLengthEncode(seg.m_details);
                if(print_exon_scores) {
                    oss << '\t' << seg.m_score;
                }
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
                if(print_exon_scores) {
                    oss << "\t-";
                }
            }
            oss << endl;
        }

        // print poly-A/T, if any
        const bool polya_present (ii->m_PolyA > 0 && ii->m_PolyA < ii->m_QueryLen);

        if(polya_present) {

            size_t polya_len;
            size_t start, stop;
            char c1, c2;
            if(ii->m_QueryStrand) {
                polya_len = ii->m_QueryLen - ii->m_PolyA;
                c1 = '+';
                c2 = 'A';
                start = 1 + ii->m_PolyA;
                stop  = ii->m_QueryLen;
            }
            else {
                polya_len = 1 + ii->m_PolyA;
                c1 = '-';
                c2 = 'T';
                start = polya_len;
                stop  = 1;
            }

            oss << c1 << ii->m_Id << '\t' << querystr << '\t' << subjstr 
                << "\t-\t"  << polya_len << '\t';
            oss << start << '\t' << stop 
                << "\t-\t-\t<poly-" << c2 << ">\t-";
            if(print_exon_scores) {
                oss << "\t-";
            }
            oss << endl;
        }
    }
    
    return CNcbiOstrstreamToString(oss);
}


void MakeLeftHeader(size_t x, string* ps) 
{
    string & s (*ps);
    const string strx (NStr::SizetToString(x));
    copy(strx.begin(), strx.end(), s.begin() + 9 - strx.size());
}


string CSplignFormatter::AsAlignmentText(
    CRef<objects::CScope> scope,
    const CSplign::TResults* results,
    size_t line_width,
    int segnum) const

{
    if(results == 0) {
        results = &m_splign_results;
    }
    
    const size_t extra_chars = 5;
    
    const string kNotSet ("id_not_set");
    const string querystr (m_QueryId.IsNull()? kNotSet:
                           m_QueryId->GetSeqIdString(true));
    const string subjstr (m_SubjId.IsNull()? kNotSet:
                          m_SubjId->GetSeqIdString(true));

    // query seq-vector
    CBioseq_Handle bh_query (scope->GetBioseqHandle(*m_QueryId));
    CSeqVector sv_query (bh_query.GetSeqVector(CBioseq_Handle::eCoding_Iupac));
    string query_sequence_sense;
    sv_query.GetSeqData(sv_query.begin(), sv_query.end(), query_sequence_sense);

    // subject seq-vector
    CBioseq_Handle bh_subj (scope->GetBioseqHandle(*m_SubjId));
    CSeqVector sv_subj (bh_subj.GetSeqVector(CBioseq_Handle::eCoding_Iupac));

    CNcbiOstrstream oss;
    oss.precision(3);
    const string kTenner (10, ' '); // heading spaces

    ITERATE(CSplign::TResults, ii, *results) {
            
        if(ii->m_Status != CSplign::SAlignedCompartment::eStatus_Ok) {
            continue;
        }
        
        const bool qstrand = ii->m_QueryStrand;
        const bool sstrand = ii->m_SubjStrand;
        const char qc = qstrand? '+': '-';
        const char sc = sstrand? '+': '-';

        oss << endl << '>' << qc << ii->m_Id << '\t' 
            << querystr << '(' << qc << ")\t" 
            << subjstr << '(' << sc << ')' << endl;
                
        size_t exons_total = 0;
        ITERATE(CSplign::TSegments, jj, ii->m_Segments) {
            if(jj->m_exon) {
                ++exons_total;
            }
        }

        string query_sequence;
        if(qstrand) {
            query_sequence = query_sequence_sense;
        }
        else {
            query_sequence.resize(query_sequence_sense.size());
            transform(query_sequence_sense.rbegin(), query_sequence_sense.rend(),
                      query_sequence.begin(),SCompliment());
        }

        string cds_sequence, query_protein;
        if(ii->m_Cds_start < ii->m_Cds_stop) {

            cds_sequence.resize(ii->m_Cds_stop - ii->m_Cds_start + 1);
            copy(query_sequence.begin() + ii->m_Cds_start,
                 query_sequence.begin() + ii->m_Cds_stop + 1,
                 cds_sequence.begin());
            CSeqTranslator::Translate(cds_sequence, query_protein);
        }

        size_t exon_count = 0, seg_count = 0;
        int query_aa_idx (0);
        int qframe(ii->m_Cds_start < ii->m_Cds_stop? -2: -3);
        ITERATE(CSplign::TSegments, jj, ii->m_Segments) {
            
            const CSplign::TSegment & s (*jj);
            if(s.m_exon) {

                size_t qbeg = s.m_box[0];
                size_t qfin = s.m_box[1];
                size_t sbeg = s.m_box[2];
                size_t sfin = s.m_box[3];

                if(exon_count > 0) {
                    if(sstrand) {
                        sbeg -= extra_chars;
                    }
                    else {
                        sbeg += extra_chars;
                    }
                }

                if(exon_count + 1 < exons_total) {
                    if(sstrand) {
                        sfin += extra_chars;
                    }
                    else {
                        sfin -= extra_chars;
                    }
                }

                size_t s0, s1;
                if(sbeg < sfin) {
                    s0 = sbeg;
                    s1 = sfin;
                }
                else {
                    s0 = sfin;
                    s1 = sbeg;
                }

                size_t q0, q1;
                if(qbeg < qfin) {
                    q0 = qbeg;
                    q1 = qfin;
                }
                else {
                    q0 = qfin;
                    q1 = qbeg;
                }

                // Load seq data
                string str;
                sv_subj.GetSeqData(sv_subj.begin() + TSeqPos(s0),
                                   sv_subj.begin() + TSeqPos(s1 + 1), str);
                vector<char> subj (str.size());
                if(sstrand) {
                    copy(str.begin(), str.end(), subj.begin());
                }
                else {
                    reverse(str.begin(), str.end());
                    transform(str.begin(), str.end(), 
                              subj.begin(), SCompliment());
                }
                
                if(!qstrand) {
                    const size_t Q0 (q0);
                    q0 = query_sequence.size() - q1 - 1;
                    q1 = query_sequence.size() - Q0 - 1;
                }

                vector<char> query (q1 - q0 + 1);
                copy(query_sequence.begin() + q0, query_sequence.begin() + q1 + 1,
                     query.begin());
                
                const bool do_print (segnum == -1 || int(seg_count) == segnum);
                if(do_print) {

                    oss << endl << " Exon " << (exon_count + 1) << " ("
                        << (1 + s.m_box[0]) << '-' << (1 + s.m_box[1]) << ','
                        << (1 + s.m_box[2]) << '-' << (1 + s.m_box[3]) << ") "
                        << "Len = " << s.m_len << ' '
                        << "Identity = " << s.m_idty << endl;
                }
                
                string l0 (kTenner);
                string l1 (kTenner);
                string l2 (kTenner);
                string l3 (kTenner);
                
                MakeLeftHeader(qbeg + 1, &l1);
                MakeLeftHeader(sbeg + 1, &l3);

                string trans;
                if(exon_count > 0) {
                    trans.assign(extra_chars, '#');
                }
                trans.append(s.m_details);
                if(exon_count + 1 < exons_total) {
                    trans.append(extra_chars, '#');
                }

                size_t lines = 0;
                for(size_t t = 0, td = trans.size(), iq = 0, is = 0; t < td; ++t) {
                    
                    char c = trans[t], c1, c2, c3, c0, c4;

                    if(qframe == -2 && q0 + iq == ii->m_Cds_start) {
                        qframe = -1;
                    }

                    if(qframe >= 0 && q0 + iq >= ii->m_Cds_stop) {
                        qframe = -3;
                    }
                    
                    switch(c) {
                 
                    case '#':
                        c1 = '.';
                        c2 = ' ';
                        c3 = subj[is++];
                        break;
       
                    case 'M':
                        c1 = query[iq++];
                        c2 = '|';
                        c3 = subj[is++];
                        break;

                    case 'R':
                        c1 = query[iq++];
                        c2 = ' ';
                        c3 = subj[is++];
                        break;
                        
                    case 'I':
                        c1 = '-';
                        c2 = ' ';
                        c3 = subj[is++];
                        break;
                        
                    case 'D':
                        c1 = query[iq++];
                        c2 = ' ';
                        c3 = '-';
                        break;
                        
                    default:
                        NCBI_THROW(CAlgoAlignException,
                                   eInternal,
                                   g_msg_UnknownTranscriptSymbol + c);
                    }

                    c0 = c4 = ' ';
                    if(qframe >= -1 && (c == 'M' || c == 'R' || c == 'D')) {
                        qframe = (qframe + 1) % 3;
                    }
                    if(c != '#' && c != 'I' && qframe == 1) {
                        c0 = query_protein[query_aa_idx++];
                    }

                    l0.push_back(c0);
                    l1.push_back(c1);
                    l2.push_back(c2);
                    l3.push_back(c3);
                    
                    if(l1.size() == 10 + line_width) {

                        if(do_print) {
                            oss << l0 << endl << l1 << endl 
                                << l2 << endl << l3 << endl << endl;
                        }

                        ++lines;
                        l0 = l1 = l2 = l3 = kTenner;
                        size_t q0, s0;
                        if(qstrand) {
                            q0 = qbeg + iq;
                        }
                        else {
                            q0 = qbeg - iq;
                        }

                        if(sstrand) {
                            s0 = sbeg + is;
                        }
                        else {
                            s0 = sbeg - is;
                        }
                
                        MakeLeftHeader(q0 + 1, &l1);
                        MakeLeftHeader(s0 + 1, &l3);
                    }
                }

                if(l1.size() > 10) {

                    if(do_print) {
                        oss << l0 << endl << l1 << endl << l2 << endl << l3 << endl;
                    }

                    l0 = l1 = l2 = l3 = kTenner;
                }

                ++exon_count;
            }
            else {
                if(qframe >= 0) qframe = -3; // disable further translation
            }

            ++seg_count;
        }
    }
    
    return CNcbiOstrstreamToString(oss);
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
    const vector<float>&  scores ) const
{
    const size_t num_exons (boxes.size() / 4);

    CRef<CSeq_align> sa (new CSeq_align);

    sa->Reset();

    // this is a discontinuous alignment
    sa->SetType(CSeq_align::eType_disc);
    sa->SetDim(2);
  
    // create seq-align-set
    CSeq_align_set& sas = sa->SetSegs().SetDisc();
    list<CRef<CSeq_align> >& sas_data = sas.Set();
    
    for(size_t i = 0; i < num_exons; ++i) {

      CRef<CSeq_align> sa (new CSeq_align);
      sa->Reset();
      sa->SetDim(2);
      sa->SetType(CSeq_align::eType_global);
      
      // add dynprog score
      CRef<CScore> score (new CScore);
      score->SetId().SetStr("splign");
      score->SetValue().SetReal(scores[i]);
      CSeq_align::TScore& scorelist = sa->SetScore();
      scorelist.push_back(score);

      // add percent identity
      CRef<CScore> idty (new CScore);
      idty->SetId().SetStr("idty");
      idty->SetValue().SetReal(CalcIdentity(transcripts[i]));
      scorelist.push_back(idty);

      CDense_seg& ds = sa->SetSegs().SetDenseg();

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

      sas_data.push_back(sa);
    }
    
    return sa;
}


bool PIsSpace(char c) {
    return isspace(c);
}



CRef<CSpliced_exon_chunk> CreateSplicedExonChunk(char cur, size_t count)
{
    CRef<CSpliced_exon_chunk> chunk (new CSpliced_exon_chunk);
    switch(cur) {
    case 'M': chunk->SetMatch()       = count; break;
    case 'R': chunk->SetMismatch()    = count; break;
    case 'I': chunk->SetGenomic_ins() = count; break;
    case 'D': chunk->SetProduct_ins() = count; break;
    default:
        NCBI_THROW(CAlgoAlignException,
                   eInternal,
                   string(g_msg_UnknownTranscriptSymbol) + cur);
    }
    return chunk;
}


CRef<CSeq_align_set> CSplignFormatter::AsSeqAlignSet(
   const CSplign::TResults * results, 
   int flag)
const
{
    const bool spliced_seg  (flag & 0x0001);
    const bool with_parts   (flag & 0x0002);
    const bool no_version   (flag & eAF_NoVersion);

    if(results == 0) {
        results = &(m_splign_results);
    }

    CRef<CSeq_align_set> rv (new CSeq_align_set);
    CSeq_align_set::Tdata& data (rv->Set());

    ITERATE(CSplign::TResults, ii, *results) {
    
        if(ii->m_Status != CSplign::SAlignedCompartment::eStatus_Ok) continue;

        if(spliced_seg) {

            CRef<CSeq_align> sa (new CSeq_align);
            sa->SetType(CSeq_align::eType_global);
            sa->SetDim(2);

            CSpliced_seg& sseg (sa->SetSegs().SetSpliced());

            sseg.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
            CSeq_id * pseqid_query (const_cast<CSeq_id*>(
                                    m_QueryId.GetNonNullPointer()));
            sseg.SetProduct_id(*pseqid_query);

            CSeq_id * pseqid_subj (const_cast<CSeq_id*>(
                                   m_SubjId.GetNonNullPointer()));
            sseg.SetGenomic_id(*pseqid_subj);
        
            sseg.SetProduct_strand((*ii).m_QueryStrand? eNa_strand_plus:
                                   eNa_strand_minus);
            sseg.SetGenomic_strand((*ii).m_SubjStrand? eNa_strand_plus:
                                   eNa_strand_minus);

            if((*ii).m_QueryLen > 0) {
                sseg.SetProduct_length((*ii).m_QueryLen);
            }

            if((*ii).m_PolyA > 0 && (*ii).m_PolyA < (*ii).m_QueryLen) {
                sseg.SetPoly_a((*ii).m_PolyA);
            }

            CSpliced_seg::TExons & exons (sseg.SetExons());
            for(size_t i (0), seg_dim ((*ii).m_Segments.size()); i < seg_dim; ++i) {

                const CSplign::TSegment & seg ((*ii).m_Segments[i]);
                if(seg.m_exon) {

                    CRef<CSpliced_exon> exon (new CSpliced_exon);

                    TSeqPos qmin, qmax, smin, smax;
                    if(seg.m_box[0] <= seg.m_box[1]) {
                        qmin = seg.m_box[0];
                        qmax = seg.m_box[1];
                    }
                    else {
                        qmax = seg.m_box[0];
                        qmin = seg.m_box[1];
                    }

                    if(seg.m_box[2] <= seg.m_box[3]) {
                        smin = seg.m_box[2];
                        smax = seg.m_box[3];
                    }
                    else {
                        smax = seg.m_box[2];
                        smin = seg.m_box[3];
                    }

                    exon->SetProduct_start().SetNucpos(qmin);
                    exon->SetProduct_end().SetNucpos(qmax);

                    exon->SetGenomic_start(smin);
                    exon->SetGenomic_end(smax);

                    CSpliced_exon::TScores::Tdata & scores (exon->SetScores().Set());

                    {{
                        // add dynprog score
                        CRef<CScore> score (new CScore);
                        score->SetId().SetStr("splign");
                        score->SetValue().SetReal(seg.m_score);
                        scores.push_back(score);
                    }}

                    {{
                        // add percent identity
                        CRef<CScore> score (new CScore);
                        score->SetId().SetStr("idty");
                        score->SetValue().SetReal(seg.m_idty);
                        scores.push_back(score);
                    }}

                    if( i>0 && !(*ii).m_Segments[i-1].m_exon) {// 5` partial
                        exon->SetPartial(true);
                    }

                    const size_t adim (seg.m_annot.size());
                    if( i>0 && (*ii).m_Segments[i-1].m_exon) {
                        // add acceptor residues if available   
                        if(adim > 2 && seg.m_annot[2] == '<') {
                            string acc;
                            acc.push_back(seg.m_annot[0]);
                            acc.push_back(seg.m_annot[1]);
                            exon->SetAcceptor_before_exon().SetBases(acc);
                        }
                    }

                    if(i+1<seg_dim && !(*ii).m_Segments[i+1].m_exon) {//3` partial
                        exon->SetPartial(true);
                    }

                    if(i+1<seg_dim && (*ii).m_Segments[i+1].m_exon) {
                        // add donor residues if available
                        if(adim > 2 && seg.m_annot[adim - 3] == '>') {
                            string dnr;
                            dnr.push_back(seg.m_annot[adim - 2]);
                            dnr.push_back(seg.m_annot[adim - 1]);
                            exon->SetDonor_after_exon().SetBases(dnr);
                        }
                    }

                    if(with_parts) {

                        // add parts
                        CSpliced_exon::TParts & parts (exon->SetParts());
                        if(seg.m_details.size() == 0) {
                            NCBI_THROW(CAlgoAlignException,
                                       eInternal,
                                       "Alignment details not available");
                        }

                        char cur (seg.m_details[0]);
                        size_t count (0);
                        ITERATE(string, ii, seg.m_details) {
                            
                            if(cur != *ii) {

                                parts.push_back(CreateSplicedExonChunk(cur, count));
                                count = 1;
                                cur = *ii;
                            }
                            else {
                                ++count;
                            }
                        }
                        parts.push_back(CreateSplicedExonChunk(cur, count));
                    }

                    exons.push_back(exon);
                }
            }
            
            if(!no_version) {

                CSeq_align::TExt& ext (sa->SetExt());
                CRef<CUser_object> uo (new CUser_object);
                ext.push_back(uo);

                uo->SetType().SetStr("origin");
                CRef<CUser_field> uf (new CUser_field);
                uo->SetData().push_back(uf);

                CRef<CObject_id> oid (new CObject_id);
                oid->SetStr("algo");
                uf->SetLabel(*oid);
                string verstr (CSplign::s_GetVersion().Print("Splign"));
                verstr.erase(remove_if(verstr.begin(), verstr.end(), PIsSpace), 
                             verstr.end());
                uf->SetData().SetStr(CUtf8::AsUTF8(verstr, eEncoding_UTF8));
            }

            data.push_back(sa);
        }
        else {

            // format into a dense-seg

            vector<size_t> boxes;
            vector<string> transcripts;
            vector<float>  scores;

            for(size_t i (0), seg_dim (ii->m_Segments.size()); i < seg_dim; ++i) {
                const CSplign::TSegment& seg (ii->m_Segments[i]);
                if(seg.m_exon) {
                    copy(seg.m_box, seg.m_box + 4, back_inserter(boxes));
                    transcripts.push_back(seg.m_details);
                    scores.push_back(seg.m_score);
                }
            }
       
            CRef<CSeq_align> sa (x_Compartment2SeqAlign(boxes,transcripts,scores));
            data.push_back(sa);
        }
    }

    return rv;
}


END_NCBI_SCOPE
