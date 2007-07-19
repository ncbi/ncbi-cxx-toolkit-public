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
*  Author: Boris Kiryutin
*
* =========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <corelib/ncbiutil.hpp>

#include "PSeq.hpp"
#include "NSeq.hpp"
#include "AliSeqAlign.hpp"
#include "Ali.hpp"

BEGIN_NCBI_SCOPE
using namespace objects;

CAliToSeq_align::CAliToSeq_align(CScope& scope, const CPosAli& ali) :
    m_scope(scope), m_ali(ali)
{
    m_IsFull = m_ali.m_output_options.IsPassThrough();
}

CRef<CSeq_id> CAliToSeq_align::StringToSeq_id(const string& id)
{
    CRef<CSeq_id> ps;
    try {
        CBioseq::TId ids;
        CSeq_id::ParseFastaIds(ids, id);
        ps = FindBestChoice(ids, CSeq_id::Score); 
        //if(!ps) ps = new CSeq_id(id);
        if(!ps) ps = new CSeq_id(CSeq_id::e_Local, id);
    } catch(CException) {
        ps.Reset (new CSeq_id(CSeq_id::e_Local, id)); 
    }
    return ps;
}


string CAliToSeq_align::Seq_idToString(CRef<CSeq_id> seqid)
{
  return seqid->GetSeqIdString(true);
}

CRef<CProduct_pos> CAliToSeq_align::NultriposToProduct_pos(int nultripos)
{
    CRef<CProduct_pos> pos(new CProduct_pos);
    pos->SetProtpos().SetFrame(nultripos%3 + 1);
    pos->SetProtpos().SetAmin(nultripos/3);
    return pos;
}

void CAliToSeq_align::SetExonBioStart(CRef<CSpliced_exon> exon, int nulpos, int nultripos) const
{ 
    if(IsForward(m_ali.m_genomic.GetStrand()))
        exon->SetGenomic_start(m_ali.NucPosOut(nulpos));
    else
        exon->SetGenomic_end(m_ali.NucPosOut(nulpos));
    exon->SetProduct_start(*NultriposToProduct_pos(nultripos));
}

void CAliToSeq_align::SetExonBioEnd(CRef<CSpliced_exon> exon, int nulpos, int nultripos) const
{ 
    if(IsForward(m_ali.m_genomic.GetStrand()))
        exon->SetGenomic_end(m_ali.NucPosOut(nulpos));
    else
        exon->SetGenomic_start(m_ali.NucPosOut(nulpos));
    exon->SetProduct_end(*NultriposToProduct_pos(nultripos));
}

CRef<CSeq_align> CAliToSeq_align::MakeSeq_align(void) const
{
    //strand setup
    ENa_strand nstrand = m_ali.m_genomic.GetStrand();
    //Seq-align
    CRef<CSeq_align> topl(new CSeq_align);
    if(m_IsFull) topl->SetType(CSeq_align::eType_global);
    else topl->SetType(CSeq_align::eType_disc);
    topl->SetDim(2);
    //Seq-align bounds
    CRef<CSeq_loc> ploc(new CSeq_loc);
    topl->SetBounds().push_back(ploc);

    CRef<CSeq_id> prot_seqid(new CSeq_id);
    prot_seqid->Assign(m_ali.m_protein);
    ploc->SetWhole(*prot_seqid);

    CRef<CSeq_loc> nloc(new CSeq_loc);
    nloc->Assign(m_ali.m_genomic);
    topl->SetBounds().push_back(nloc);

    //compartment id
//     CRef<CUser_object> uo(new CUser_object);
//     topl->SetExt().push_back(uo);
//     uo->SetType().SetStr("info");
//     CRef<CUser_field> uf(new CUser_field);
//     uo->SetData().push_back(uf);
//     uf->SetLabel().SetStr("CompartmentId");
//     uf->SetData().SetInt(m_compart_id);

    //Splised-seg
    CSpliced_seg& sps = topl->SetSegs().SetSpliced();
    sps.SetProduct_id(*prot_seqid);


    CRef<CSeq_id> genomic_seqid(new CSeq_id);
    genomic_seqid->Assign(*m_ali.m_genomic.GetId());
    sps.SetGenomic_id(*genomic_seqid);
    if(nstrand == eNa_strand_minus) sps.SetGenomic_strand(nstrand);

    if (!m_ali.m_pcs.empty()) {
    //EXONS
        CRef<CSpliced_exon> exon;
        CRef<CSpliced_exon_chunk> chunk;

        int nulpos = 0, nultripos = 0;
        int alipos = 0;
        EAliPieceType prev_type = eSP;
        char s[] = "GT"; 
        
        list<CNPiece>::const_iterator good_piece_it = m_ali.m_pcs.begin();
        if (good_piece_it->beg == 0) {
            exon = new CSpliced_exon;
            SetExonBioStart(exon, nulpos, nultripos);
        }
            
        for(vector<CAliPiece>::const_iterator pit = m_ali.m_ps.begin(); pit != m_ali.m_ps.end(); ++pit) {
            if(pit->m_type == eSP) {
                if(prev_type != eSP) {
                    if (exon.NotNull()) {
                        SetExonBioEnd(exon, nulpos-1, nultripos-1);
                        s[0] = m_ali.cnseq->Upper(nulpos);
                        s[1] = m_ali.cnseq->Upper(nulpos+1);
                        exon->SetSplice_3_prime().SetBases(s);
                        sps.SetExons().push_back(exon);
                        exon = new CSpliced_exon;
                    } else
                        exon.Reset();
                }
                nulpos += pit->m_len;
                if (exon.NotNull()) {
                    SetExonBioStart(exon, nulpos, nultripos);
                    s[0] = m_ali.cnseq->Upper(nulpos-2);
                    s[1] = m_ali.cnseq->Upper(nulpos-1);
                    exon->SetSplice_5_prime().SetBases(s);
                }
            } else {
                if (exon.NotNull()) {
                    chunk.Reset(new CSpliced_exon_chunk);
                    exon->SetParts().push_back(chunk);
                }
                    
                switch(pit->m_type) {
                case eVP : 
                    if (chunk.NotNull())
                        chunk->SetProduct_ins(pit->m_len);
                    nultripos += pit->m_len;
                    break;
                case eHP :
                    if (chunk.NotNull())
                        chunk->SetGenomic_ins(pit->m_len);
                    nulpos += pit->m_len;
                    break;
                case eMP : {
                    int remaining_len = pit->m_len;
                    int tmp_alipos = alipos;
                    while (remaining_len > 0 &&
                           good_piece_it != m_ali.m_pcs.end() &&
                           tmp_alipos <= good_piece_it->end && good_piece_it->beg <= tmp_alipos+remaining_len) {

                        if (tmp_alipos <= good_piece_it->beg) {
                            int bad_len = good_piece_it->beg - tmp_alipos;
                            nultripos += bad_len;
                            nulpos += bad_len;
                            remaining_len -= bad_len;
                            tmp_alipos += bad_len;
                            exon = new CSpliced_exon;
                            SetExonBioStart(exon, nulpos, nultripos);
                            chunk = new CSpliced_exon_chunk;
                            exon->SetParts().push_back(chunk);
                        }
                        int chunk_len = min(remaining_len,good_piece_it->end - tmp_alipos+1);
                        chunk->SetDiag(chunk_len);
                        nultripos += chunk_len;
                        nulpos += chunk_len;
                        remaining_len -= chunk_len;
                        tmp_alipos += chunk_len;

                        if (remaining_len > 0) {
                            SetExonBioEnd(exon, nulpos-1, nultripos-1);
                            sps.SetExons().push_back(exon);
                            exon.Reset();
                            chunk.Reset();
                        }
                        if (tmp_alipos > good_piece_it->end) {
                            ++good_piece_it;
                        }
                    }
                    
                    nultripos += remaining_len;
                    nulpos += remaining_len;
                }
                    break;
                default :
                    break;
                }
            }
            prev_type = pit->m_type;
            alipos += pit->m_len;
        }

        /*
          HERE WE NEED TO HANDLE THE CASE WHERE AN EXON HAS EMPTY
          PRODUCT PART OF EMPTY GENOMIC PART
        */
    }

    sps.SetProduct_type(CSpliced_seg::eProduct_type_protein);
    sps.SetProduct_length(m_ali.cpseq->seq.size());
    //start, stop
    if(m_ali.HasStartOnNuc() && m_ali.cpseq->HasStart()) {
        CRef<CSpliced_seg_modifier> modi(new CSpliced_seg_modifier);
        modi->SetStart_codon_found(true);
        sps.SetModifiers().push_back(modi);
    }
    if(m_ali.HasStopOnNuc()) {
        CRef<CSpliced_seg_modifier> modi(new CSpliced_seg_modifier);
        modi->SetStop_codon_found(true);
        sps.SetModifiers().push_back(modi);
    }

    return topl;
}

END_NCBI_SCOPE
