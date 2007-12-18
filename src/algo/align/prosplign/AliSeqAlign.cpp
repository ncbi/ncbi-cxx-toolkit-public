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

#include <algo/align/prosplign/prosplign_exception.hpp>
#include "PSeq.hpp"
#include "NSeq.hpp"
#include "AliSeqAlign.hpp"
#include "Ali.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)
using namespace objects;

CAliToSeq_align::CAliToSeq_align(const CAli& ali, CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic) :
    m_ali(ali), m_scope(scope), m_protein(protein), m_genomic(genomic)
{
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

CRef<CProduct_pos> NultriposToProduct_pos(int nultripos)
{
    CRef<CProduct_pos> pos(new CProduct_pos);
    pos->SetProtpos().SetFrame(nultripos%3 + 1);
    pos->SetProtpos().SetAmin(nultripos/3);
    return pos;
}

void CAliToSeq_align::SetExonBioStart(CRef<CSpliced_exon> exon, int nulpos, int nultripos) const
{ 
    if(IsForward(m_genomic.GetStrand()))
        exon->SetGenomic_start(NucPosOut(nulpos));
    else
        exon->SetGenomic_end(NucPosOut(nulpos));
    exon->SetProduct_start(*NultriposToProduct_pos(nultripos));
}

void CAliToSeq_align::SetExonBioEnd(CRef<CSpliced_exon> exon, int nulpos, int nultripos) const
{ 
    if(IsForward(m_genomic.GetStrand()))
        exon->SetGenomic_end(NucPosOut(nulpos));
    else
        exon->SetGenomic_start(NucPosOut(nulpos));
    exon->SetProduct_end(*NultriposToProduct_pos(nultripos));
}

CRef<CSeq_align> CAliToSeq_align::MakeSeq_align(const CPSeq& cpseq, const CNSeq& cnseq) const
{
    //strand setup
    ENa_strand nstrand = m_genomic.GetStrand();
    //Seq-align
    CRef<CSeq_align> topl(new CSeq_align);
    topl->SetType(CSeq_align::eType_global);
    topl->SetDim(2);
    //Seq-align bounds
    CRef<CSeq_loc> ploc(new CSeq_loc);
    topl->SetBounds().push_back(ploc);

    CRef<CSeq_id> prot_seqid(new CSeq_id);
    prot_seqid->Assign(m_protein);
    ploc->SetWhole(*prot_seqid);

    CRef<CSeq_loc> nloc(new CSeq_loc);
    nloc->Assign(m_genomic);
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
    genomic_seqid->Assign(*m_genomic.GetId());
    sps.SetGenomic_id(*genomic_seqid);
    sps.SetGenomic_strand(nstrand);

    sps.SetExons();
    {
    //EXONS
        CRef<CSpliced_exon> exon;
        CRef<CSpliced_exon_chunk> chunk;

        int nulpos = 0, nultripos = 0;
        int alipos = 0;
        char s[] = "GT"; 
        

        vector<CAliPiece>::const_iterator pit = m_ali.m_ps.begin();
        vector<CAliPiece>::const_iterator pit_end = m_ali.m_ps.end();

        for(; pit != pit_end; ++pit) {
            if (pit->m_type == eVP)
                nultripos += pit->m_len;
            else if (pit->m_type == eHP || pit->m_type == eSP)
                nulpos += pit->m_len;
            else
                break;
        }

        if (pit == pit_end)
            NCBI_THROW(CProSplignException, eGenericError, "Just insertions in alignment");

        _ASSERT(pit->m_type == eMP);

        while(--pit_end != pit && pit_end->m_type != eMP)
            ;

        _ASSERT(pit_end->m_type == eMP);
        ++pit_end;

        exon = new CSpliced_exon;
        SetExonBioStart(exon, nulpos, nultripos);
        exon->SetPartial(nultripos > 0);

        for(; pit != pit_end; ++pit) {
            if(pit->m_type == eSP) {
                SetExonBioEnd(exon, nulpos-1, nultripos-1);
                s[0] = cnseq.Upper(nulpos);
                s[1] = cnseq.Upper(nulpos+1);
                exon->SetSplice_3_prime().SetBases(s);
                sps.SetExons().push_back(exon);

                exon = new CSpliced_exon;
                nulpos += pit->m_len;
                SetExonBioStart(exon, nulpos, nultripos);
                s[0] = cnseq.Upper(nulpos-2);
                s[1] = cnseq.Upper(nulpos-1);
                exon->SetSplice_5_prime().SetBases(s);

            } else {
                if (exon.NotNull()) {
                    chunk.Reset(new CSpliced_exon_chunk);
                    exon->SetParts().push_back(chunk);
                }
                    
                switch(pit->m_type) {
                case eVP : 
                    if (chunk.NotNull()) {
                        chunk->SetProduct_ins(pit->m_len);
                        chunk.Reset();
                    }
                    nultripos += pit->m_len;
                    break;
                case eHP :
                    if (chunk.NotNull()) {
                        chunk->SetGenomic_ins(pit->m_len);
                        chunk.Reset();
                    }
                    nulpos += pit->m_len;
                    break;
                case eMP :
                    _ASSERT(chunk.NotNull());
                    chunk->SetDiag(pit->m_len);
                    chunk.Reset();
                    nultripos += pit->m_len;
                    nulpos += pit->m_len;
                    break;
                default :
                    _ASSERT(false); //shouldn't get here
                    break;
                }
            }
            alipos += pit->m_len;
        }

        _ASSERT (exon.NotNull());
        SetExonBioEnd(exon, nulpos-1, nultripos-1);
        exon->SetPartial(nultripos < int(cpseq.seq.size())*3);
        sps.SetExons().push_back(exon);
    }
    sps.SetProduct_type(CSpliced_seg::eProduct_type_protein);
    sps.SetProduct_length(cpseq.seq.size());

    return topl;
}

int CAliToSeq_align::NucPosOut(int pos) const
{
    if(IsForward(m_genomic.GetStrand()))
        return pos + m_genomic.GetStart(eExtreme_Positional);
    else
        return m_genomic.GetStop(eExtreme_Positional) - pos;
}

END_SCOPE(prosplign)
END_NCBI_SCOPE
