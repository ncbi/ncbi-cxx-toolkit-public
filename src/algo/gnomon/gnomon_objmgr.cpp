/*  $Id$
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
 * Authors:  Mike DiCuccio
 *
 * File Description: gnomon library parts requiring object manager support
 * to allow apps not using these to be linked w/o object manager libs
 *
 */

#include <ncbi_pch.hpp>
#include <algo/gnomon/gnomon.hpp>
#include <algo/gnomon/chainer.hpp>
#include <algo/gnomon/gnomon_exception.hpp>
#include <algo/gnomon/id_handler.hpp>
#include "hmm.hpp"
#include "hmm_inlines.hpp"

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/general/general__.hpp>
#include <objmgr/object_manager.hpp>

#include <stdio.h>

#include <functional>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)
USING_SCOPE(ncbi::objects);

namespace {
int GetCompartmentNum(const CSeq_align& sa)
{
    if (sa.CanGetExt()) {
        ITERATE(CSeq_align::TExt, e, sa.GetExt()) {
            const CUser_object& uo = **e;
            if (uo.CanGetData()) {
                ITERATE(CUser_object::TData, field, uo.GetData()) {
                    if ((*field)->CanGetLabel() && (*field)->GetLabel().IsStr() && (*field)->GetLabel().GetStr()=="CompartmentId") {
                        return (*field)->GetData().GetInt();
                    }
                }
            }
        }
    }
    return 0;
}
}

void debug()
{
}

Int8 GetModelId(const CSeq_align& seq_align)
{
    return seq_align.GetId().back()->GetId();
}

CAlignModel::CAlignModel(const CSeq_align& seq_align) :
    CGeneModel(seq_align.GetSegs().GetSpliced().GetGenomic_strand()==eNa_strand_minus?eMinus:ePlus, GetModelId(seq_align), emRNA)
{
#ifdef _DEBUG   
    debug();
#endif

    const CSpliced_seg& sps = seq_align.GetSegs().GetSpliced();

    bool is_protein = false;
    if(sps.CanGetProduct_type() && sps.GetProduct_type()==CSpliced_seg::eProduct_type_protein) {
        SetType(eProt);
        is_protein = true;
    }

    bool is_product_reversed = false;
    if(sps.CanGetProduct_strand() && sps.GetProduct_strand()==eNa_strand_minus) {
        Status() |= CGeneModel::eReversed;
        is_product_reversed = true;
    }

    SetTargetId(sps.GetProduct_id());

    int product_len = sps.CanGetProduct_length()?sps.GetProduct_length():0;
    if (is_protein)
        product_len *=3;
    int prod_prev = -1;
    bool prev_3_prime_splice = false;

    int target_len = product_len;

    vector<TSignedSeqRange> transcript_exons;
    TInDels indels;

    if(!sps.CanGetGenomic_id())
        NCBI_THROW(CGnomonException, eGenericError, "CSpliced_seg must have genomic id");

    const CSeq_id& genomic_id = sps.GetGenomic_id();

    bool ggap_model= false;

    ITERATE(CSpliced_seg::TExons, e_it, sps.GetExons()) {
        const CSpliced_exon& exon = **e_it;
        int prod_cur_start = exon.GetProduct_start().AsSeqPos();
        int prod_cur_end = exon.GetProduct_end().AsSeqPos();
        if (is_product_reversed) {
            int tmp = prod_cur_start;
            prod_cur_start = product_len - prod_cur_end -1;
            prod_cur_end = product_len - tmp -1;
        }
        int nuc_cur_start = exon.GetGenomic_start();
        int nuc_cur_end = exon.GetGenomic_end();

        //        bool cur_5_prime_splice = exon.CanGetAcceptor_before_exon() && exon.GetAcceptor_before_exon().CanGetBases() && exon.GetAcceptor_before_exon().GetBases().size()==2;
        bool cur_5_prime_splice = exon.CanGetAcceptor_before_exon() && exon.GetAcceptor_before_exon().CanGetBases();
        if (prod_prev+1 != prod_cur_start || !prev_3_prime_splice || !cur_5_prime_splice)
            AddHole();
        //        prev_3_prime_splice = exon.CanGetDonor_after_exon() && exon.GetDonor_after_exon().CanGetBases() && exon.GetDonor_after_exon().GetBases().size()==2;
        prev_3_prime_splice = exon.CanGetDonor_after_exon() && exon.GetDonor_after_exon().CanGetBases();

        string fs, ss;
        if(exon.CanGetAcceptor_before_exon() && exon.GetAcceptor_before_exon().CanGetBases()) {
            fs = exon.GetAcceptor_before_exon().GetBases();
        }
        if(exon.CanGetDonor_after_exon() && exon.GetDonor_after_exon().CanGetBases()) {
            ss = exon.GetDonor_after_exon().GetBases();
        }
        if(Strand() == eMinus) {
            swap(fs,ss);
        }

        double eident = 0;
        if(exon.CanGetScores() && exon.GetScores().CanGet()) {
            CScore_set::Tdata scores = exon.GetScores().Get();
            ITERATE(CScore_set::Tdata, it, scores) {
                if((*it)->CanGetId() && (*it)->GetId().IsStr()) {
                    if((*it)->GetId().GetStr() == "idty") {
                        eident = (*it)->GetValue().GetReal();
                        break;
                    }
                }
            }
        }

        if(!exon.CanGetGenomic_id() || !(exon.GetGenomic_id() < genomic_id || genomic_id < exon.GetGenomic_id())) {  // normal exon
            AddNormalExon(TSignedSeqRange(nuc_cur_start,nuc_cur_end), fs, ss, eident, Strand() == eMinus);
        } else {  // genomic gap
            if(!exon.CanGetGenomic_strand())
                NCBI_THROW(CGnomonException, eGenericError, "CSpliced_exon for gap filling must have genomic strand");
            CConstRef<objects::CSeq_id> fill_id(&exon.GetGenomic_id());
            CScope scope(*CObjectManager::GetInstance());
            scope.AddDefaults();

            string transcript = GetDNASequence(fill_id,scope);
            string fill_seq = transcript.substr(nuc_cur_start, nuc_cur_end-nuc_cur_start+1);

            CInDelInfo::SSource fill_src;
            fill_src.m_acc = CIdHandler::ToString(*fill_id);
            fill_src.m_strand = ePlus;
            fill_src.m_range = TSignedSeqRange(nuc_cur_start, nuc_cur_end);

            if(exon.GetGenomic_strand() == eNa_strand_minus) {
                fill_src.m_strand = eMinus;
                ReverseComplement(fill_seq.begin(),fill_seq.end());
            }

            AddGgapExon(eident, fill_seq, fill_src, Strand() == eMinus);
            ggap_model = true;
        }
        transcript_exons.push_back(TSignedSeqRange(exon.GetProduct_start().AsSeqPos(), exon.GetProduct_end().AsSeqPos()));

        _ASSERT(transcript_exons.back().NotEmpty());

        int pos = 0;
        int prod_pos = prod_cur_start;
        ITERATE(CSpliced_exon::TParts, p_it, exon.GetParts()) {
            const CSpliced_exon_chunk& chunk = **p_it;
            if (chunk.IsProduct_ins()) {
                CInDelInfo fs(Strand()==ePlus?nuc_cur_start+pos:nuc_cur_end-pos+1,chunk.GetProduct_ins(),false);
                if (Strand()==ePlus)
                    indels.push_back(fs);
                else
                    indels.insert(indels.begin(), fs);
                prod_pos += chunk.GetProduct_ins();
            } else if (chunk.IsGenomic_ins()) {
                const int genomic_ins = chunk.GetGenomic_ins();
                if (Strand()==ePlus)
                    indels.push_back(CInDelInfo(nuc_cur_start+pos,genomic_ins));
                else
                    indels.insert(indels.begin(), CInDelInfo(nuc_cur_end-pos-genomic_ins+1,genomic_ins));
                pos += genomic_ins;
            } else if (chunk.IsMatch()) {
                pos += chunk.GetMatch();
                prod_pos += chunk.GetMatch();
            } else if (chunk.IsMismatch()) {
                pos += chunk.GetMismatch();
                prod_pos += chunk.GetMismatch();
            } else { // if (chunk.IsDiag())
                pos += chunk.GetDiag();
                prod_pos += chunk.GetDiag();
            }
        }

        prod_prev = prod_cur_end;
    }

    sort(transcript_exons.begin(),transcript_exons.end());
    bool minusstrand = Strand() == eMinus;
    EStrand orientation = (is_product_reversed == minusstrand) ? ePlus : eMinus;
    if(orientation == eMinus)
       reverse(transcript_exons.begin(),transcript_exons.end());

    if (is_protein) {
        _ASSERT(orientation == Strand());
        if (sps.CanGetModifiers()) {
            ITERATE(CSpliced_seg::TModifiers, m, sps.GetModifiers()) {
                if ((*m)->IsStop_codon_found()) {
                    target_len += 3;
                    if (Strand() == ePlus) {
                        ExtendRight( 3 );
                        _ASSERT((transcript_exons.back().GetTo()+1)%3 == 0);
                        transcript_exons.back().SetTo(transcript_exons.back().GetTo()+3);
                    } else {
                        ExtendLeft( 3 );
                        _ASSERT((transcript_exons.front().GetTo()+1)%3 == 0);
                        transcript_exons.front().SetTo(transcript_exons.front().GetTo()+3);
                    }
                }
            }
        }
    }

    m_alignmap = CAlignMap(Exons(), transcript_exons, indels, orientation, target_len );
    FrameShifts() = m_alignmap.GetInDels(true);

    TSignedSeqRange newlimits = m_alignmap.ShrinkToRealPoints(Limits(),is_protein);
    if(newlimits != Limits()) {
        Clip(newlimits,CAlignModel::eRemoveExons);    
    }   

    for (CGeneModel::TExons::const_iterator piece_begin = Exons().begin(); piece_begin != Exons().end(); ++piece_begin) {
        _ASSERT( !piece_begin->m_fsplice );

        if(piece_begin->Limits().Empty()) {   // ggap
            _ASSERT(piece_begin->m_ssplice);
            ++piece_begin;
            _ASSERT(piece_begin->Limits().NotEmpty());
        }
        
        CGeneModel::TExons::const_iterator piece_end;
        for (piece_end = piece_begin; piece_end != Exons().end() && piece_end->m_ssplice; ++piece_end) ;
        _ASSERT( piece_end != Exons().end() );

        CGeneModel::TExons::const_iterator piece_end_g = piece_end;
        if(piece_end_g->Limits().Empty()) {   // ggap
            _ASSERT(piece_end_g->m_fsplice);
            --piece_end_g;
            _ASSERT(piece_end_g->Limits().NotEmpty());
        }
        
        TSignedSeqRange piece_range(piece_begin->GetFrom(),piece_end_g->GetTo());
            
        piece_range = m_alignmap.ShrinkToRealPoints(piece_range, is_protein); // finds first projectable interval (on codon boundaries  for proteins)   

        TSignedSeqRange pr;
        while(pr != piece_range) {
            pr = piece_range;
            ITERATE(TInDels, i, indels) { // here we check that no indels touch our interval from outside   
                if((i->IsDeletion() && i->Loc() == pr.GetFrom()) || (i->IsInsertion() && i->Loc()+i->Len() == pr.GetFrom()))
                    pr.SetFrom(pr.GetFrom()+1);                
                else if(i->Loc() == pr.GetTo()+1)
                    pr.SetTo(pr.GetTo()-1);
            }
            if(pr != piece_range)
                piece_range = m_alignmap.ShrinkToRealPoints(pr, is_protein);
        }

        _ASSERT(piece_range.NotEmpty());
        _ASSERT(piece_range.IntersectingWith(piece_begin->Limits()) && piece_range.IntersectingWith(piece_end_g->Limits()));

        if(piece_range.GetFrom() != piece_begin->GetFrom() || piece_range.GetTo() != piece_end_g->GetTo()) {
            _ASSERT(!ggap_model);
            Clip(piece_range, CGeneModel::eDontRemoveExons); 
        }
            
        piece_begin = piece_end;
    }

    if (is_protein) {
        TSignedSeqRange reading_frame = Limits();
        TSignedSeqRange start, stop;
        if (sps.CanGetModifiers()) {
            ITERATE(CSpliced_seg::TModifiers, m, sps.GetModifiers()) {
                TSignedSeqRange rf = m_alignmap.MapRangeOrigToEdited(reading_frame, true);
                if ((*m)->IsStart_codon_found()) {
                    start = m_alignmap.MapRangeEditedToOrig(TSignedSeqRange(rf.GetFrom(),rf.GetFrom()+2),false);
                    reading_frame = m_alignmap.MapRangeEditedToOrig(TSignedSeqRange(rf.GetFrom()+3,rf.GetTo()),true);
                } else if ((*m)->IsStop_codon_found()) {
                    stop = m_alignmap.MapRangeEditedToOrig(TSignedSeqRange(rf.GetTo()-2,rf.GetTo()),false);
                    reading_frame = m_alignmap.MapRangeEditedToOrig(TSignedSeqRange(rf.GetFrom(),rf.GetTo()-3),true);
                }
            }
        }

        CCDSInfo cds_info;
        cds_info.SetReadingFrame( reading_frame, true);
        if (start.NotEmpty()) {
            //            cds_info.SetStart(start, sps.GetExons().front()->GetProduct_start().AsSeqPos() == 0 && sps.GetExons().front()->GetParts().front()->IsMatch());
            cds_info.SetStart(start, false);
        }
        if (stop.NotEmpty()) {
            //            cds_info.SetStop(stop, sps.GetExons().back()->GetProduct_end().AsSeqPos() == product_len-1 );
            cds_info.SetStop(stop, false);
        }
        SetCdsInfo(cds_info);
    }

    if (sps.IsSetPoly_a()) {
        Status() |= CGeneModel::ePolyA;
    }

    if(seq_align.CanGetExt()) {
        int count = 0;
        ITERATE(CSeq_align::TExt, i, seq_align.GetExt()) {
            if((*i)->IsSetType() && (*i)->GetType().IsStr() && (*i)->GetType().GetStr() == "RNASeq-Counts") {
                ITERATE(CUser_object::TData, j, (*i)->GetData()) {
                    if(*j)
                        count += (*j)->GetData().GetInt();
                }
            }
        }
        if(count > 0)
            SetWeight(count);
    }

    if(seq_align.CanGetScore()) {
        const CSeq_align::TScore& score = seq_align.GetScore();
        ITERATE(CSeq_align::TScore, it, score) {
            if((*it)->CanGetId() && (*it)->GetId().IsStr()) {
                string scr = (*it)->GetId().GetStr();
                if((scr == "N of matches") || (scr == "num_ident") || (scr == "matches")) {
                    double ident = (*it)->GetValue().GetInt()*100;
                    ident /= seq_align.GetAlignLength();
                    SetIdent(ident);
                } else if(scr == "rank" && (*it)->GetValue().GetInt() == 1) {
                    Status() |= CGeneModel::eBestPlacement;
                } else if(scr == "ambiguous_orientation") {
                    Status() |= CGeneModel::eUnknownOrientation;
                } else if(scr == "count") {
                    SetWeight((*it)->GetValue().GetInt());
                }
            }
        }
    }
}



string CGeneModel::GetCdsDnaSequence (const CResidueVec& contig_sequence) const
{
    if(ReadingFrame().Empty())
        return kEmptyStr;

    CAlignMap amap = CGeneModel::GetAlignMap();
    CCDSInfo cds_info = GetCdsInfo();
    if(cds_info.IsMappedToGenome())
        cds_info = cds_info.MapFromOrigToEdited(amap);
    int cds_len = cds_info.Cds().GetLength();
    int cds_start = cds_info.Cds().GetFrom()-TranscriptLimits().GetFrom();

    CResidueVec mrna;
    amap.EditedSequence(contig_sequence,mrna);

    string cds_seq(cds_len,'A');
    copy(mrna.begin()+cds_start, mrna.begin()+cds_start+cds_len, cds_seq.begin());

    if(Status() & eReversed)
        ReverseComplement(cds_seq.begin(),cds_seq.end());

    return cds_seq;
}

string CGeneModel::GetProtein (const CResidueVec& contig_sequence) const
{
    string cds_seq = GetCdsDnaSequence(contig_sequence);
    string prot_seq;

    objects::CSeqTranslator::Translate(cds_seq, prot_seq, objects::CSeqTranslator::fIs5PrimePartial);

    return prot_seq;
}

string CGeneModel::GetProtein (const CResidueVec& contig_sequence, const CGenetic_code* gencode) const
{
    string cds_seq = GetCdsDnaSequence(contig_sequence);
    string prot_seq;

    objects::CSeqTranslator::Translate(cds_seq, prot_seq, objects::CSeqTranslator::fDefault, gencode );
    if (prot_seq[0] == '-') {
        string first_triplet = cds_seq.substr(0, 3);
        string first_aa;
        objects::CSeqTranslator::Translate(first_triplet, first_aa, objects::CSeqTranslator::fIs5PrimePartial, gencode );
        prot_seq = first_aa+prot_seq.substr(1);
    }
    return prot_seq;
}

//
// helper function - convert a vector<TSignedSeqRange> into a compact CSeq_loc
//
namespace {

CRef<CSeq_loc> s_ExonDataToLoc(const vector<TSignedSeqRange>& vec,
                               ENa_strand strand, const CSeq_id& id)
{
    CRef<CSeq_loc> loc(new CSeq_loc());

    CPacked_seqint::Tdata data;
    ITERATE (vector<TSignedSeqRange>, iter, vec) {
        CRef<CSeq_interval> ival(new CSeq_interval);
        ival->SetFrom(iter->GetFrom());
        ival->SetTo(iter->GetTo());
        ival->SetStrand(strand);
        ival->SetId().Assign(id);

        data.push_back(ival);
    }

    if (data.size() == 1) {
        loc->SetInt(*data.front());
    } else {
        loc->SetPacked_int().Set().swap(data);
    }

    return loc;
}

} //end unnamed namespace

CRef<CSeq_annot> CGnomonEngine::GetAnnot(const CSeq_id& id)
{
    TGeneModelList genes = GetGenes();

    CRef<objects::CSeq_annot> annot(new CSeq_annot());

    annot->SetNameDesc("Gnomon gene scan output");

    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();

    unsigned int counter = 0;
    string locus_tag_base("GNOMON_");
    ITERATE (TGeneModelList, it, genes) {
        const CGeneModel& igene = *it;
        int strand = igene.Strand();
        TSignedSeqRange cds_limits = igene.RealCdsLimits();

        vector<TSignedSeqRange> mrna_vec;
        copy(igene.Exons().begin(), igene.Exons().end(), back_inserter(mrna_vec));
        vector<TSignedSeqRange> cds_vec;

        for (size_t j = 0;  j < mrna_vec.size();  ++j) {
            TSignedSeqRange intersect(mrna_vec[j] & cds_limits);
            if (!intersect.Empty()) {
                cds_vec.push_back(intersect);
            }
        }

        // stop-codon removal from cds
        if (igene.HasStop()) {
            if (strand == ePlus) {
                _ASSERT(cds_vec.back().GetLength()>=3);
                cds_vec.back().SetTo(cds_vec.back().GetTo() - 3);
            } else {
                _ASSERT(cds_vec.front().GetLength()>=3);
                cds_vec.front().SetFrom(cds_vec.front().GetFrom() + 3);
            }
        }

        //
        // create our mRNA
        CRef<CSeq_feat> feat_mrna;
        if (mrna_vec.size()) {
            feat_mrna = new CSeq_feat();
            feat_mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
            feat_mrna->SetLocation
                (*s_ExonDataToLoc(mrna_vec,
                 (strand == ePlus ? eNa_strand_plus : eNa_strand_minus), id));
        } else {
            continue;
        }

        //
        // create the accompanying CDS
        CRef<CSeq_feat> feat_cds;
        if (!cds_vec.empty()) {
            feat_cds = new CSeq_feat();
            feat_cds->SetData().SetCdregion().SetFrame(CCdregion::TFrame(1+(strand == ePlus?(igene.ReadingFrame().GetFrom()-cds_limits.GetFrom())%3:(cds_limits.GetTo()-igene.ReadingFrame().GetTo())%3)));

            feat_cds->SetLocation
                (*s_ExonDataToLoc(cds_vec,
                 (strand == ePlus ? eNa_strand_plus : eNa_strand_minus), id));
        }

        //
        // create a dummy gene feature as well
        CRef<CSeq_feat> feat_gene(new CSeq_feat());

        char buf[32];
        sprintf(buf, "%04u", ++counter);
        string name(locus_tag_base);
        name += buf;
        feat_gene->SetData().SetGene().SetLocus_tag(name);
        feat_gene->SetLocation().SetInt()
            .SetFrom(feat_mrna->GetLocation().GetTotalRange().GetFrom());
        feat_gene->SetLocation().SetInt()
            .SetTo(feat_mrna->GetLocation().GetTotalRange().GetTo());
        feat_gene->SetLocation().SetInt().SetStrand
            (strand == ePlus ? eNa_strand_plus : eNa_strand_minus);

        feat_gene->SetLocation().SetId
            (sequence::GetId(feat_mrna->GetLocation(), 0));

        ftable.push_back(feat_gene);
        ftable.push_back(feat_mrna);
        if (feat_cds) {
            ftable.push_back(feat_cds);
        }
    }
    return annot;
}


//
//
// This function deduces the frame from 5p coordinate of the CDS which should be on the 5p codon boundary
//
// 
double CCodingPropensity::GetScore(CConstRef<CHMMParameters> hmm_params, const CSeq_loc& cds, CScope& scope, int *const gccontent, double *const startscore)
{
    *gccontent = 0;
    const CSeq_id* seq_id = cds.GetId();
    if (seq_id == NULL)
	NCBI_THROW(CGnomonException, eGenericError, "CCodingPropensity: CDS has multiple ids or no id at all");
    
    // Need to know GC content in order to load correct models.
    // Compute on the whole transcript, not just CDS.
    CBioseq_Handle xcript_hand = scope.GetBioseqHandle(*seq_id);
    CSeqVector xcript_vec = xcript_hand.GetSeqVector();
    xcript_vec.SetIupacCoding();
    unsigned int gc_count = 0;
    CSeqVector_CI xcript_iter(xcript_vec);
    for( ;  xcript_iter;  ++xcript_iter) {
        if (*xcript_iter == 'G' || *xcript_iter == 'C') {
            ++gc_count;
        }
    }
    *gccontent = static_cast<unsigned int>(100.0 * gc_count / xcript_vec.size() + 0.5);
	
    const CMC3_CodingRegion<5>&   cdr = dynamic_cast<const CMC3_CodingRegion<5>&>(hmm_params->GetParameter(CMC3_CodingRegion<5>::class_id(), *gccontent));
    const CMC_NonCodingRegion<5>& ncdr = dynamic_cast<const CMC_NonCodingRegion<5>&>(hmm_params->GetParameter(CMC_NonCodingRegion<5>::class_id(), *gccontent));

    // Represent coding sequence as enum Nucleotides
    CSeqVector vec(cds, scope);
    vec.SetIupacCoding();
    CEResidueVec seq;
    seq.reserve(vec.size());
    CSeqVector_CI iter(vec);
    for( ;  iter;  ++iter) {
	seq.push_back(fromACGT(*iter));
    }

    // Sum coding and non-coding scores across coding sequence.
    // Don't include stop codon!
    double score = 0;
    for (unsigned int i = 5;  i < seq.size() - 3;  ++i)
        score += cdr.Score(seq, i, i % 3) - ncdr.Score(seq, i);

    if(startscore) {
        //
        // start evalustion needs 17 bases total (11 before ATG and 3 after)
        // if there is not enough sequence it will be substituted by Ns which will degrade the score
        //

        const CWMM_Start& start = dynamic_cast<const CWMM_Start&>(hmm_params->GetParameter(CWMM_Start::class_id(), *gccontent));

        int totallen = xcript_vec.size();
        int left, right;
        int extraNs5p = 0;
        int extrabases = start.Left()+2;            // (11) extrabases left of ATG needed for start (6) and noncoding (5) evaluation
        if(cds.GetStrand() == eNa_strand_plus) {
            int startposition = cds.GetTotalRange().GetFrom();
            if(startposition < extrabases) {
                left = 0;
                extraNs5p = extrabases-startposition;
            } else {
                left = startposition-extrabases;
            }
            right = min(startposition+2+start.Right(),totallen-1);
        } else {
            int startposition = cds.GetTotalRange().GetTo();
            if(startposition+extrabases >= totallen) {
                right = totallen-1;
                extraNs5p = startposition+extrabases-totallen+1;
            } else {
                right = startposition+extrabases;
            }
            left = max(0,startposition-2-start.Right());
        }


        CRef<CSeq_loc> startarea(new CSeq_loc());
        CSeq_interval& d = startarea->SetInt();
        d.SetStrand(cds.GetStrand());
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*seq_id);
        d.SetId(*id);
        d.SetFrom(left);
        d.SetTo(right);
        
        CSeqVector sttvec(*startarea, scope);
        sttvec.SetIupacCoding();
        CEResidueVec sttseq;
        sttseq.resize(extraNs5p, enN);                         // fill with Ns if necessery
        for(unsigned int i = 0; i < sttvec.size(); ++i) {
            sttseq.push_back(fromACGT(sttvec[i]));
        }
        sttseq.resize(5+start.Left()+start.Right(), enN);      // add Ns if necessery
        
        *startscore = start.Score(sttseq, extrabases+2);
        if(*startscore != BadScore()) {
            for(unsigned int i = 5; i < sttseq.size(); ++i) {
                *startscore -= ncdr.Score(sttseq, i);
            }
        }
    }

    return score;
}

END_SCOPE(gnomon)
END_NCBI_SCOPE
