/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Eyal Mozes
*
* =========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/alnmgr/alntext.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);

const char CProteinAlignText::GAP_CHAR='-'; // used in dna and protein text
const char CProteinAlignText::SPACE_CHAR=' '; // translation and protein
const char CProteinAlignText::INTRON_CHAR='.'; // protein
const char CProteinAlignText::INTRON_OR_GAP[] =
    {CProteinAlignText::INTRON_CHAR, CProteinAlignText::GAP_CHAR,0};

// used in match text
const char CProteinAlignText::BAD_PIECE_CHAR='X';
const char CProteinAlignText::MISMATCH_CHAR=' ';
const char CProteinAlignText::BAD_OR_MISMATCH[] =
    {CProteinAlignText::BAD_PIECE_CHAR,CProteinAlignText::MISMATCH_CHAR,0};
const char CProteinAlignText::MATCH_CHAR='|';
const char CProteinAlignText::POSIT_CHAR='+';


void CProteinAlignText::AddSpliceText(CSeqVector_CI& genomic_ci, int& nuc_prev, char match)
{
    AddDNAText(genomic_ci,nuc_prev,2);
    m_translation.append((SIZE_TYPE)2,SPACE_CHAR);
    m_match.append((SIZE_TYPE)2,match);
    m_protein.append((SIZE_TYPE)2,INTRON_CHAR);
}

void CProteinAlignText::AddDNAText(CSeqVector_CI& genomic_ci, int& nuc_prev, size_t len)
{
    string buf;
    genomic_ci.GetSeqData(buf,len);
    nuc_prev +=len;
    m_dna.append(buf);
}

void CProteinAlignText::AddProtText(CSeqVector_CI& protein_ci, int& prot_prev, size_t len)
{
    m_protein.reserve(m_protein.size()+len);

    size_t phase = (prot_prev+1)%3;

    if (phase!=0) {
        size_t prev_not_intron_pos = m_protein.find_last_not_of(INTRON_OR_GAP,m_protein.size()-1);
        char aa = m_protein[prev_not_intron_pos];
        _ASSERT( aa != SPACE_CHAR );
        SIZE_TYPE added_len = min(3-phase,len);
        if (prev_not_intron_pos == m_protein.size()-1 && phase+added_len==3 && (phase==1 || m_protein[prev_not_intron_pos-1]==aa)) {
            m_protein.append(added_len,SPACE_CHAR);
            m_protein[m_protein.size()-3] = SPACE_CHAR;
            m_protein[m_protein.size()-2] = toupper(aa);
        } else {
            m_protein.append(added_len,aa);
        }
        len -= added_len;
        prot_prev += added_len;
    }

    if (len > 0) {
        string buf;
        protein_ci.GetSeqData(buf,(len+2)/3);
        const char* p = buf.c_str();

        while (len >= 3) {
            m_protein.push_back(SPACE_CHAR);
            m_protein.push_back(*p++);
            m_protein.push_back(SPACE_CHAR);
            len -=3;
            prot_prev += 3;
        }
        if (len > 0) {
            m_protein.append(len,tolower(*p));
        }
        prot_prev += len;
    }
}

// translate last len bases in m_dna
// plus spliced codon in prev exon if at the start of exon
void CProteinAlignText::TranslateDNA(int phase, size_t len, bool is_insertion)
{
    _ASSERT( m_translation.size()+len ==m_dna.size() );
    _ASSERT( phase==0 || m_dna.size()>0 );

    m_translation.reserve(m_translation.size()+len);
    size_t start_pos = m_dna.size()-len;
    const char INTRON[] = {INTRON_CHAR,0};
    if (phase != 0) {
        size_t prev_exon_pos = 0;
        if (phase+len >=3 &&
            ((prev_exon_pos=m_protein.find_last_not_of(is_insertion?INTRON:INTRON_OR_GAP,start_pos-1))!=start_pos-1 ||
             m_dna[start_pos]==GAP_CHAR) &&
            m_match[prev_exon_pos]!=BAD_PIECE_CHAR) {
            string codon = m_dna.substr(prev_exon_pos-phase+1,phase)+m_dna.substr(start_pos,3-phase);
            char aa = (codon[0]!=GAP_CHAR && codon[1]!=GAP_CHAR) ? TranslateTriplet(*m_trans_table, codon) : SPACE_CHAR;
            for( size_t i = prev_exon_pos-phase+1; i<=prev_exon_pos;++i) {
                m_translation[i] = tolower(aa);
                m_match[i] = MatchChar(i);
            }
            m_translation.append((SIZE_TYPE)(3-phase),m_dna[start_pos]!=GAP_CHAR?tolower(aa):SPACE_CHAR);
        } else {
            m_translation.append(min(len,(SIZE_TYPE)(3-phase)),SPACE_CHAR);
        }
        start_pos += min(len,(SIZE_TYPE)(3-phase));
   }

    if (m_dna[start_pos]!=GAP_CHAR) {
        char aa[] = "   ";
        for ( ; start_pos+3 <= m_dna.size(); start_pos += 3) {
            aa[1] = TranslateTriplet(*m_trans_table, m_dna.substr(start_pos,3));
            m_translation += aa;
        }
    }

    if (start_pos < m_dna.size()) {
        m_translation.append(m_dna.size()-start_pos,SPACE_CHAR);
    }

    _ASSERT( m_translation.size()==m_dna.size() );
}

char CProteinAlignText::MatchChar(size_t i)
{
    char m = SPACE_CHAR;
    if (m_translation[i] != SPACE_CHAR && m_protein[i] != SPACE_CHAR) {
        if (m_translation[i] == m_protein[i]) {
            m = MATCH_CHAR;
        } else if(m_matrix.s[(int)toupper(m_protein[i])]
                            [(int)toupper(m_translation[i])] > 0)
        {
            m = POSIT_CHAR;
        }
    }
    return m;
}

void CProteinAlignText::MatchText(size_t len, bool is_match)
{
    _ASSERT( m_translation.size() == m_protein.size() );
    _ASSERT( m_translation.size() == m_match.size()+len );

    m_match.reserve(m_match.size()+len);

    for (size_t i = m_translation.size()-len; i < m_translation.size(); ++i) {
        m_match.push_back((is_match && islower(m_protein[i]))?MATCH_CHAR:MatchChar(i));
    }
}

char CProteinAlignText::TranslateTriplet(const CTrans_table& table,
                                         const string& triplet)
{
    return table.GetCodonResidue(
        table.SetCodonState(triplet[0], triplet[1], triplet[2]));
}

void CProteinAlignText::AddHoleText(
                                 bool prev_3_prime_splice, bool cur_5_prime_splice,
                                 CSeqVector_CI& genomic_ci, CSeqVector_CI& protein_ci,
                                 int& nuc_prev, int& prot_prev,
                                 int nuc_cur_start, int prot_cur_start)
{
    _ASSERT( m_dna.size() == m_translation.size() );
    _ASSERT( m_match.size() == m_protein.size() );
    _ASSERT( m_dna.size() == m_protein.size() );

    int prot_hole_len = prot_cur_start - prot_prev -1;
    int nuc_hole_len = nuc_cur_start - nuc_prev -1;

    bool can_show_splices = prot_hole_len < nuc_hole_len -4;
    if (can_show_splices && prev_3_prime_splice) {
        AddSpliceText(genomic_ci,nuc_prev, BAD_PIECE_CHAR);
        nuc_hole_len = nuc_cur_start - nuc_prev -1;
    }
    if (can_show_splices && cur_5_prime_splice) {
        nuc_cur_start -= 2;
        nuc_hole_len = nuc_cur_start - nuc_prev -1;
    }

    SIZE_TYPE hole_len = max(prot_hole_len,nuc_hole_len);
    _ASSERT( prot_hole_len>0 || nuc_hole_len>0 );
    int left_gap = 0;
    
    left_gap = (prot_hole_len-nuc_hole_len)/2;
    if (left_gap>0)
        m_dna.append((SIZE_TYPE)left_gap,GAP_CHAR);
    if (nuc_hole_len>0)
        AddDNAText(genomic_ci,nuc_prev,nuc_hole_len);
    if (prot_hole_len>nuc_hole_len)
        m_dna.append((SIZE_TYPE)(prot_hole_len-nuc_hole_len-left_gap),GAP_CHAR);
    
    m_translation.append(hole_len,SPACE_CHAR);
    m_match.append(hole_len,BAD_PIECE_CHAR);
    
    left_gap = (nuc_hole_len-prot_hole_len)/2;
    if (left_gap>0)
        m_protein.append((SIZE_TYPE)left_gap,GAP_CHAR);
    if (prot_hole_len>0)
        AddProtText(protein_ci,prot_prev,prot_hole_len);
    if (prot_hole_len<nuc_hole_len)
        m_protein.append((SIZE_TYPE)(nuc_hole_len-prot_hole_len-left_gap),
                         GAP_CHAR);
    
    if (can_show_splices && cur_5_prime_splice) {
        AddSpliceText(genomic_ci,nuc_prev, BAD_PIECE_CHAR);
    }
    _ASSERT( m_dna.size() == m_translation.size() );
    _ASSERT( m_match.size() == m_protein.size() );
    _ASSERT( m_dna.size() == m_protein.size() );
}

CProteinAlignText::~CProteinAlignText()
{
}

CProteinAlignText::CProteinAlignText(objects::CScope& scope, const objects::CSeq_align& seqalign,
                               const string& matrix_name)
{
    const CSpliced_seg& sps = seqalign.GetSegs().GetSpliced();

    ENa_strand strand = sps.GetGenomic_strand();

    const CSeq_id& protid = sps.GetProduct_id();
    int prot_len = sps.GetProduct_length()*3;
    CSeqVector protein_seqvec(scope.GetBioseqHandle(protid), CBioseq_Handle::eCoding_Iupac);
    CSeqVector_CI protein_ci(protein_seqvec);

    CRef<CSeq_loc> genomic_seqloc = GetGenomicBounds(scope, seqalign);
    CSeqVector genomic_seqvec(*genomic_seqloc, scope, CBioseq_Handle::eCoding_Iupac);
    CSeqVector_CI genomic_ci(genomic_seqvec);

    int gcode = 1;
    try {
        const CSeq_id* sid = genomic_seqloc->GetId();
        CBioseq_Handle hp = scope.GetBioseqHandle(*sid);
        gcode = sequence::GetOrg_ref(hp).GetGcode();
    } catch (...) {}

    m_trans_table = &CGen_code_table::GetTransTable(gcode);

    const SNCBIPackedScoreMatrix* packed_mtx =
        NCBISM_GetStandardMatrix(matrix_name.c_str());
    if (packed_mtx == NULL)
        NCBI_THROW(CException, eUnknown, "unknown scoring matrix: "+matrix_name);
    NCBISM_Unpack(packed_mtx, &m_matrix);

    int nuc_from = genomic_seqloc->GetTotalRange().GetFrom();
    int nuc_to = genomic_seqloc->GetTotalRange().GetTo();
    int nuc_prev = -1;
    int prot_prev = -1;
    bool prev_3_prime_splice = false;
    int prev_genomic_ins = 0;
    ITERATE(CSpliced_seg::TExons, e_it, sps.GetExons()) {
        const CSpliced_exon& exon = **e_it;
        int prot_cur_start = exon.GetProduct_start().AsSeqPos();
#ifdef _DEBUG
        int prot_cur_end = exon.GetProduct_end().AsSeqPos();
#endif
        int nuc_cur_start = exon.GetGenomic_start();
        int nuc_cur_end = exon.GetGenomic_end();
        if (strand==eNa_strand_plus) {
            nuc_cur_start -= nuc_from;
            nuc_cur_end -= nuc_from;
        } else {
            swap(nuc_cur_start,nuc_cur_end);
            nuc_cur_start = nuc_to - nuc_cur_start;
            nuc_cur_end = nuc_to - nuc_cur_end;
        }
        bool cur_5_prime_splice = exon.CanGetAcceptor_before_exon() && exon.GetAcceptor_before_exon().CanGetBases() && exon.GetAcceptor_before_exon().GetBases().size()==2;
        bool hole_before =
            prot_prev+1 != prot_cur_start || !( (prev_3_prime_splice && cur_5_prime_splice) || (prot_cur_start==0 && nuc_cur_start==0) );

        if (hole_before) {
            AddHoleText(prev_3_prime_splice, cur_5_prime_splice,
                        genomic_ci, protein_ci,
                        nuc_prev, prot_prev,
                        nuc_cur_start, prot_cur_start);
            prev_genomic_ins = 0;
        } else { //intron
            SIZE_TYPE intron_len = nuc_cur_start - nuc_prev -1;
            AddDNAText(genomic_ci, nuc_prev, intron_len);
            m_translation.append(intron_len,SPACE_CHAR);
            m_match.append(intron_len,MISMATCH_CHAR);
            m_protein.append(intron_len,INTRON_CHAR);
        }

        _ASSERT( m_dna.size() == m_translation.size() );
        _ASSERT( m_match.size() == m_protein.size() );
        _ASSERT( m_dna.size() == m_protein.size() );
        
        prev_3_prime_splice = exon.CanGetDonor_after_exon() && exon.GetDonor_after_exon().CanGetBases() && exon.GetDonor_after_exon().GetBases().size()==2;

        ITERATE(CSpliced_exon::TParts, p_it, exon.GetParts()) {
            const CSpliced_exon_chunk& chunk = **p_it;
            if (!chunk.IsGenomic_ins())
                prev_genomic_ins = 0;
            if (chunk.IsDiag() || chunk.IsMatch() || chunk.IsMismatch()) {
                int len = 0;
                if (chunk.IsDiag()) {
                    len = chunk.GetDiag();
                } else if (chunk.IsMatch()) {
                    len = chunk.GetMatch();
                } else if (chunk.IsMismatch()) {
                    len = chunk.GetMismatch();
                }
                AddDNAText(genomic_ci,nuc_prev,len);
                TranslateDNA((prot_prev+1)%3,len,false);
                AddProtText(protein_ci,prot_prev,len);
                if (chunk.IsMismatch()) {
                    m_match.append(len,MISMATCH_CHAR);
                } else
                    MatchText(len, chunk.IsMatch());
            } else if (chunk.IsProduct_ins()) {
                SIZE_TYPE len = chunk.GetProduct_ins();
                m_dna.append(len,GAP_CHAR);
                TranslateDNA((prot_prev+1)%3,len,false);
                m_match.append(len,MISMATCH_CHAR);
                AddProtText(protein_ci,prot_prev,len);
            } else if (chunk.IsGenomic_ins()) {
                SIZE_TYPE len = chunk.GetGenomic_ins();
                AddDNAText(genomic_ci,nuc_prev,len);
                if (0<=prot_prev && prot_prev<prot_len-1 && (prot_prev+1)%3==0)
                    TranslateDNA(prev_genomic_ins,len,true);
                else
                    m_translation.append(len,SPACE_CHAR);
                prev_genomic_ins = (prev_genomic_ins+len)%3;
                m_match.append(len,MISMATCH_CHAR);
                m_protein.append(len,GAP_CHAR);
            }
            _ASSERT(prot_prev <= prot_cur_end);
        }
        _ASSERT(prot_prev == prot_cur_end);
        _ASSERT(nuc_prev == nuc_cur_end);

        _ASSERT( m_dna.size() == m_translation.size() );
        _ASSERT( m_match.size() == m_protein.size() );
        _ASSERT( m_dna.size() == m_protein.size() );
    }

    int nuc_cur_start = nuc_to - nuc_from +1;
    int prot_cur_start = prot_len;
    if (prot_prev+1 != prot_cur_start || nuc_prev+1 != nuc_cur_start) {
        bool cur_5_prime_splice = false;
        AddHoleText(prev_3_prime_splice, cur_5_prime_splice,
                    genomic_ci, protein_ci,
                    nuc_prev, prot_prev,
                    nuc_cur_start, prot_cur_start);
    }
}

CRef<CSeq_loc> CProteinAlignText::GetGenomicBounds(CScope& scope,
                                                   const CSeq_align& seqalign)
{
    CRef<CSeq_loc> genomic(new CSeq_loc);

    const CSpliced_seg& sps = seqalign.GetSegs().GetSpliced();
    const CSeq_id& nucid = sps.GetGenomic_id();

    if (seqalign.CanGetBounds()) {
        ITERATE(CSeq_align::TBounds, b,seqalign.GetBounds()) {
            if ((*b)->GetId() != NULL && (*b)->GetId()->Match(nucid)) {

                TSeqPos len = sequence::GetLength(nucid, &scope);

                genomic->Assign(**b);
                if (genomic->IsWhole()) {
                    // change to Interval, because Whole doesn't allow strand change - it's always unknown.
                    genomic->SetInt().SetFrom(0);
                    genomic->SetInt().SetTo(len-1);
                }
                genomic->SetStrand(sps.GetGenomic_strand());

                if (genomic->GetStop(eExtreme_Positional) >= len) {
                    genomic->SetInt().SetFrom(genomic->GetStart(eExtreme_Positional));
                    genomic->SetInt().SetTo(len-1);
                }

                return genomic;
            }
        }
    }

    if (sps.GetExons().empty()) {
        genomic->SetNull();
    } else {
        genomic->SetPacked_int().AddInterval(nucid,sps.GetExons().front()->GetGenomic_start(),sps.GetExons().front()->GetGenomic_end(),sps.GetGenomic_strand());
        genomic->SetPacked_int().AddInterval(nucid,sps.GetExons().back()->GetGenomic_start(),sps.GetExons().back()->GetGenomic_end(),sps.GetGenomic_strand());

        genomic = sequence::Seq_loc_Merge(*genomic, CSeq_loc::fMerge_SingleRange, NULL);
    }

    return genomic;
}

END_NCBI_SCOPE

