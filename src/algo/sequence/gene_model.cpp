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
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/sequence/gene_model.hpp>
#include <corelib/ncbitime.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/alnmgr/score_builder_base.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_seg_modifier.hpp>
#include <objects/seqalign/Splice_site.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Object_id.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <util/range_coll.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include "feature_generator.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(sequence);

//////////////////////////
void CGeneModel::CreateGeneModelFromAlign(const CSeq_align& align_in,
                                          CScope& scope,
                                          CSeq_annot& annot,
                                          CBioseq_set& seqs,
                                          TGeneModelCreateFlags flags,
                                          TSeqPos allowed_unaligned)
{
    CFeatureGenerator generator(scope);
    generator.SetFlags(flags | CFeatureGenerator::fGenerateLocalIds);
    generator.SetAllowedUnaligned(allowed_unaligned);

    CConstRef<CSeq_align> clean_align = generator.CleanAlignment(align_in);
    generator.ConvertAlignToAnnot(*clean_align, annot, seqs);
}

void CGeneModel::CreateGeneModelsFromAligns(const list< CRef<objects::CSeq_align> > &aligns,
                                       CScope& scope,
                                       CSeq_annot& annot,
                                       CBioseq_set& seqs,
                                       TGeneModelCreateFlags flags,
                                       TSeqPos allowed_unaligned)
{
    CFeatureGenerator generator(scope);
    generator.SetFlags(flags | CFeatureGenerator::fGenerateLocalIds);
    generator.SetAllowedUnaligned(allowed_unaligned);

    generator.ConvertAlignToAnnot(aligns, annot, seqs);
}

void CGeneModel::SetFeatureExceptions(CSeq_feat& feat,
                                      CScope& scope,
                                      const CSeq_align* align)
{
    CFeatureGenerator generator(scope);
    generator.SetFeatureExceptions(feat, align);
}

const char* k_except_text_for_gap_filled_gnomon_model = "annotated by transcript or proteomic data";

void CGeneModel::SetPartialFlags(CScope& scope,
                                 CRef<CSeq_feat> gene_feat,
                                 CRef<CSeq_feat> mrna_feat,
                                 CRef<CSeq_feat> cds_feat)
{
    CFeatureGenerator generator(scope);
    generator.SetPartialFlags(gene_feat, mrna_feat, cds_feat);
}

void CGeneModel::RecomputePartialFlags(CScope& scope,
                                       CSeq_annot& annot)
{
    CFeatureGenerator generator(scope);
    generator.RecomputePartialFlags(annot);
}


///
/// Return the mol-info object for a given sequence
///
static const CMolInfo* s_GetMolInfo(const CBioseq_Handle& handle)
{
    if (handle) {
        CSeqdesc_CI desc_iter(handle, CSeqdesc::e_Molinfo);
        for ( ;  desc_iter;  ++desc_iter) {
            return &desc_iter->GetMolinfo();
        }
    }

    return NULL;
}

/////////////////////////////////////

CFeatureGenerator::SImplementation::SImplementation(CScope& scope)
    : m_scope(&scope)
    , m_flags(fDefaults)
    , m_min_intron(kDefaultMinIntron)
    , m_allowed_unaligned(kDefaultAllowedUnaligned)
    , m_is_gnomon(false)
    , m_is_best_refseq(false)
{
}

CFeatureGenerator::SImplementation::~SImplementation()
{
}

CFeatureGenerator::CFeatureGenerator(CRef<CScope> scope)
    : m_impl(new SImplementation(*scope))
{
}

CFeatureGenerator::CFeatureGenerator(CScope& scope)
    : m_impl(new SImplementation(scope))
{
}

CFeatureGenerator::~CFeatureGenerator()
{
}

void CFeatureGenerator::SetFlags(TFeatureGeneratorFlags flags)
{
    m_impl->m_flags = flags;
}

CFeatureGenerator::TFeatureGeneratorFlags CFeatureGenerator::GetFlags() const
{
    return m_impl->m_flags;
}

void CFeatureGenerator::SetMinIntron(TSeqPos value)
{
    m_impl->m_min_intron = value;
}

void CFeatureGenerator::SetAllowedUnaligned(TSeqPos value)
{
    m_impl->m_allowed_unaligned = value;
}

CConstRef<CSeq_align>
CFeatureGenerator::CleanAlignment(const CSeq_align& align_in)
{
    return m_impl->CleanAlignment(align_in);
}

CRef<CSeq_feat> CFeatureGenerator::ConvertAlignToAnnot(const CSeq_align& align,
                                            CSeq_annot& annot,
                                            CBioseq_set& seqs,
                                            int gene_id,
                                            const CSeq_feat* cdregion)
{
    return m_impl->ConvertAlignToAnnot(align, annot, seqs, gene_id, cdregion, false);
}

void CFeatureGenerator::ConvertAlignToAnnot(
                         const list< CRef<CSeq_align> > &aligns,
                         CSeq_annot &annot,
                         CBioseq_set &seqs)
{
    m_impl->ConvertAlignToAnnot(aligns, annot, seqs);
}

void CFeatureGenerator::ConvertLocToAnnot(
        const objects::CSeq_loc &loc,
        objects::CSeq_annot& annot,
        objects::CBioseq_set& seqs,
        CCdregion::EFrame frame,
        CRef<objects::CSeq_id> prot_id,
        CRef<objects::CSeq_id> rna_id)
{
    CBioseq_Handle bsh = m_impl->m_scope->GetBioseqHandle(*loc.GetId());
    if (!bsh) {
        NCBI_THROW(CException, eUnknown,
            "Can't find genomic sequence " + loc.GetId()->AsFastaString());
    }

    const CMolInfo *mol_info = s_GetMolInfo(bsh);
    if (!mol_info || !mol_info->CanGetBiomol() ||
        mol_info->GetBiomol() != CMolInfo::eBiomol_genomic)
    {
        NCBI_THROW(CException, eUnknown,
            "Not a genomic sequence: " + loc.GetId()->AsFastaString());
    }

    const COrg_ref &org = sequence::GetOrg_ref(bsh);

    TFeatureGeneratorFlags old_flags = GetFlags();
    TFeatureGeneratorFlags flags = old_flags;

    /// Temporarily change flags to make sure the needed bioseqs are generated,
    /// and that the input ids are used
    flags &= ~fGenerateLocalIds;
    SetFlags(flags);

    static CAtomicCounter counter;
    size_t new_id_num = counter.Add(1);
    CTime time(CTime::eCurrent);
    if (!rna_id) {
        string str("lcl|MRNA_");
        str += time.AsString("YMD");
        str += "_";
        str += NStr::NumericToString(new_id_num);
        rna_id.Reset(new CSeq_id(str));
    }
    if (!prot_id) {
        string str("lcl|PROT_");
        str += time.AsString("YMD");
        str += "_";
        str += NStr::NumericToString(new_id_num);
        prot_id.Reset(new CSeq_id(str));
    }

    CSeq_align fake_align;
    fake_align.SetType(CSeq_align::eType_not_set);
    fake_align.SetDim(2);
    fake_align.SetSegs().SetSpliced().SetProduct_id().Assign(*rna_id);
    fake_align.SetSegs().SetSpliced().SetGenomic_id().Assign(*loc.GetId());
    fake_align.SetSegs().SetSpliced().SetProduct_strand(eNa_strand_plus);
    fake_align.SetSegs().SetSpliced().SetGenomic_strand(loc.GetStrand());
    fake_align.SetSegs().SetSpliced().SetProduct_type(
        CSpliced_seg::eProduct_type_transcript);

    TSeqPos product_pos = 0;
    ITERATE (CSeq_loc, loc_it, loc) {
        CRef<CSpliced_exon> exon(new CSpliced_exon);
        exon->SetProduct_start().SetNucpos(product_pos);
        product_pos += loc_it.GetRange().GetLength();
        exon->SetProduct_end().SetNucpos(product_pos-1);
        exon->SetGenomic_start(loc_it.GetRange().GetFrom());
        exon->SetGenomic_end(loc_it.GetRange().GetTo());
        CRef<CSpliced_exon_chunk> match(new CSpliced_exon_chunk);
        match->SetMatch(loc_it.GetRange().GetLength());
        exon->SetParts().push_back(match);
        fake_align.SetSegs().SetSpliced().SetExons().push_back(exon);
    }
    fake_align.SetSegs().SetSpliced().SetProduct_length(product_pos);

    CSeq_feat cdregion;
    cdregion.SetData().SetCdregion().SetFrame(frame);
    if (frame != CCdregion::eFrame_one &&
        !loc.IsPartialStart(eExtreme_Biological))
    {
        NCBI_THROW(CException, eUnknown,
            "Non-standard frame specified with 5'-complete location");
    }

    CSeq_loc cdregion_loc(*rna_id, 0, product_pos-1, eNa_strand_plus);
    if (loc.IsPartialStart(eExtreme_Biological)) {
        cdregion_loc.SetPartialStart(true, eExtreme_Biological);
    }
    if (loc.IsPartialStop(eExtreme_Biological)) {
        cdregion_loc.SetPartialStop(true, eExtreme_Biological);
    } else if (flags & fCreateCdregion) {
        /// location is 3'-complete; verify we have a whole number of codons,
        /// taking frame into account
        switch (frame) {
        case CCdregion::eFrame_two:
            product_pos -= 1;
            break;

        case CCdregion::eFrame_three:
            product_pos -= 2;
            break;
        default:
            break;
        }

        if (product_pos % 3) {
            NCBI_THROW(CException, eUnknown,
                "Non-whole number of codons with 3'-complete location");
        }
    }
    if (org.IsSetGcode()) {
        CRef<CGenetic_code::C_E> code(new CGenetic_code::C_E);
        code->SetId(org.GetGcode());
        cdregion.SetData().SetCdregion().SetCode().Set().push_back(code);
    }
    cdregion.SetLocation().Assign(cdregion_loc);
    cdregion.SetProduct().SetWhole(*prot_id);

    m_impl->ConvertAlignToAnnot(fake_align, annot, seqs, 0, &cdregion, false);

    /// Restore old flags
    SetFlags(old_flags);
}

void CFeatureGenerator::SetFeatureExceptions(CSeq_feat& feat,
                                             const CSeq_align* align)
{
    m_impl->SetFeatureExceptions(feat, align);
}


void CFeatureGenerator::SetPartialFlags(CRef<CSeq_feat> gene_feat,
                                        CRef<CSeq_feat> mrna_feat,
                                        CRef<CSeq_feat> cds_feat)
{
    m_impl->SetPartialFlags(gene_feat, mrna_feat, cds_feat);
}

void CFeatureGenerator::RecomputePartialFlags(CSeq_annot& annot)
{
    m_impl->RecomputePartialFlags(annot);
}


CFeatureGenerator::SImplementation::SMapper::SMapper(const CSeq_align& aln, CScope& scope,
                                                     TSeqPos allowed_unaligned,
                                                     CSeq_loc_Mapper::TMapOptions opts)
    : m_aln(aln), m_scope(scope), m_genomic_row(-1)
    , m_allowed_unaligned(allowed_unaligned), m_opts(opts)
{
    if(aln.GetSegs().IsSpliced()) {
        //row 1 is always genomic in spliced-segs
        m_genomic_row = 1;
    } else {
        //otherwise, find exactly one genomic row
        CSeq_align::TDim num_rows = aln.CheckNumRows();
        if (num_rows != 2) {
            /// make sure we only have two rows. anything else
            /// represents a mixed-strand case or more than two
            /// sequences
            NCBI_THROW(CException, eUnknown,
                       "CreateGeneModelFromAlign(): "
                       "failed to create consistent alignment");
        }
        for (CSeq_align::TDim i = 0;  i < num_rows;  ++i) {
            const CSeq_id& id = aln.GetSeq_id(i);
            CBioseq_Handle handle = scope.GetBioseqHandle(id);
            if(!handle) {
                continue;
            }
            const CMolInfo* info =  sequence::GetMolInfo(handle);
            if (info && info->IsSetBiomol()
                && info->GetBiomol() == CMolInfo::eBiomol_genomic)
                {
                    if(m_genomic_row < 0) {
                        m_genomic_row = i;
                    } else {
                        NCBI_THROW(CException, eUnknown,
                                   "CreateGeneModelFromAlign(): "
                                   "More than one genomic row in alignment");
                    }
                }
        }
        if (m_genomic_row < 0) {
            NCBI_THROW(CException, eUnknown,
                       "CreateGeneModelFromAlign(): "
                       "No genomic sequence found in alignment");
        }
    }
}

const CSeq_loc& CFeatureGenerator::SImplementation::SMapper::GetRnaLoc()
{
    if(rna_loc.IsNull()) {
        if(m_aln.GetSegs().IsSpliced()) {
            rna_loc = x_GetLocFromSplicedExons(m_aln);
        } else {
            const CSeq_id& id = m_aln.GetSeq_id(GetRnaRow());
            CBioseq_Handle handle = m_scope.GetBioseqHandle(id);
            CRef<CSeq_loc> range_loc =
                handle.GetRangeSeq_loc(0, 0, eNa_strand_plus); //0-0 meanns whole range
            //todo: truncate the range loc not to include polyA, or
            //else the remapped loc will be erroneously partial
            //not a huge issue as it only applies to seg alignments only.
            rna_loc = x_Mapper()->Map(*range_loc);
        }
    }
    return *rna_loc;
}

CSeq_align::TDim CFeatureGenerator::SImplementation::SMapper::GetGenomicRow() const
{
    return m_genomic_row;
}

CSeq_align::TDim CFeatureGenerator::SImplementation::SMapper::GetRnaRow() const
{
    //we checked that alignment contains exactly 2 rows
    return GetGenomicRow() == 0 ? 1 : 0;
}

CRef<CSeq_loc> CFeatureGenerator::SImplementation::SMapper::Map(const CSeq_loc& loc)
{
    CRef<CSeq_loc> mapped_loc  = x_Mapper()->Map(loc);
    return mapped_loc;
}

void CFeatureGenerator::SImplementation::SMapper::IncludeSourceLocs(bool b)
{
    x_Mapper()->IncludeSourceLocs(b);
}

void CFeatureGenerator::SImplementation::SMapper::SetMergeNone()
{
    x_Mapper()->SetMergeNone();
}

CRef<CSeq_loc> CFeatureGenerator::SImplementation::SMapper::x_GetLocFromSplicedExons(const CSeq_align& aln) const
{
    CRef<CSeq_loc> loc(new CSeq_loc);
    CConstRef<CSpliced_exon> prev_exon;
    CRef<CSeq_interval> prev_int;
    
    const CSpliced_seg& spliced_seg = aln.GetSegs().GetSpliced();
    TSeqPos genomic_size = m_scope.GetSequenceLength(spliced_seg.GetGenomic_id());
    ITERATE(CSpliced_seg::TExons, it, spliced_seg.GetExons()) {
        const CSpliced_exon& exon = **it;
        CRef<CSeq_interval> genomic_int(new CSeq_interval);
        
        genomic_int->SetId().Assign(aln.GetSeq_id(1));
        genomic_int->SetFrom(exon.GetGenomic_start());
        genomic_int->SetTo(exon.GetGenomic_end());
        genomic_int->SetStrand(
                               exon.IsSetGenomic_strand() ? exon.GetGenomic_strand()
                               : spliced_seg.IsSetGenomic_strand() ? spliced_seg.GetGenomic_strand()
                               : eNa_strand_plus);
        
        // check for gaps between exons
        if(!prev_exon.IsNull() && 
           !(prev_exon->GetProduct_end().GetNucpos() + 1 == exon.GetProduct_start().GetNucpos() &&
             ((genomic_int->GetStrand()!=eNa_strand_minus && prev_exon->GetGenomic_end()==genomic_size-1 && exon.GetGenomic_start()==0) ||
              (genomic_int->GetStrand()==eNa_strand_minus && exon.GetGenomic_end()==genomic_size-1 && prev_exon->GetGenomic_start()==0))
             )) {

            bool donor_set = prev_exon->IsSetDonor_after_exon();
            bool acceptor_set = exon.IsSetAcceptor_before_exon();
            
            if(!(donor_set && acceptor_set) || prev_exon->GetProduct_end().GetNucpos() + 1 != exon.GetProduct_start().GetNucpos()) {
                // gap between exons on rna. But which exon is partial?
                // if have non-strict consensus splice site - blame it
                // for partialness. If can't disambiguate on this - set
                // both.
                bool donor_ok =
                    (donor_set  &&
                     prev_exon->GetDonor_after_exon().GetBases() == "GT");
                bool acceptor_ok =
                    (acceptor_set  &&
                     exon.GetAcceptor_before_exon().GetBases() == "AG");
                if(donor_ok || !acceptor_ok) {
                    genomic_int->SetPartialStart(true, eExtreme_Biological);
                }
                if(acceptor_ok || !donor_ok) {
                    prev_int->SetPartialStop(true, eExtreme_Biological);
                }
            }
        }

        loc->SetPacked_int().Set().push_back(genomic_int);

        prev_exon = *it;
        prev_int = genomic_int;
    }
    
    // set terminal partialness
    if(m_aln.GetSeqStart(0) > m_allowed_unaligned) {
        loc->SetPartialStart(true, eExtreme_Biological);
    }
    
    TSeqPos product_len = aln.GetSegs().GetSpliced().GetProduct_length();
    TSeqPos polya_pos = aln.GetSegs().GetSpliced().CanGetPoly_a() ? aln.GetSegs().GetSpliced().GetPoly_a() : product_len;
    
    if(m_aln.GetSeqStop(0) + 1 + m_allowed_unaligned < polya_pos) {
        loc->SetPartialStop(true, eExtreme_Biological);
    }
    return loc;
    
}

CRef<CSeq_loc_Mapper> CFeatureGenerator::SImplementation::SMapper::x_Mapper()
{
    if (!m_mapper) {
        m_mapper.Reset
            (new CSeq_loc_Mapper(m_aln, m_aln.GetSeq_id(m_genomic_row),
                                 &m_scope, m_opts));
    }
    return m_mapper;
}

CConstRef<CSeq_align>
CFeatureGenerator::
SImplementation::CleanAlignment(const CSeq_align& align_in)
{
    if (!align_in.CanGetSegs() || !align_in.GetSegs().IsSpliced())
        return CConstRef<CSeq_align>(&align_in);

    CRef<CSeq_align> align(new CSeq_align);
    align->Assign(align_in);

    StitchSmallHoles(*align);
    TrimHolesToCodons(*align);

    if (m_flags & fMaximizeTranslation) {
        MaximizeTranslation(*align);
    }

    return align;
}

static void s_TransformToNucpos(CProduct_pos &pos)
{
    TSeqPos nucpos = pos.AsSeqPos();
    pos.SetNucpos(nucpos);
}

string ExtractGnomonModelNum(const CSeq_id& seq_id)
{
    string model_num;
    if (seq_id.IsGeneral() && seq_id.GetGeneral().CanGetDb() &&
            NStr::EqualNocase(seq_id.GetGeneral().GetDb(), "GNOMON")) {
        model_num = seq_id.GetGeneral().GetTag().GetStr();
        model_num.erase(model_num.size()-2, 2);
    }
    return model_num;
}

bool IsProteinAlign(const CSeq_align& align)
{
    return align.CanGetSegs() && align.GetSegs().IsSpliced()
        && align.GetSegs().GetSpliced().GetProduct_type()
        == CSpliced_seg::eProduct_type_protein;
}

void CFeatureGenerator::
SImplementation::TransformProteinAlignToTranscript(CConstRef<CSeq_align>& align, CRef<CSeq_feat>& cd_feat)
{
        /// This is a protein alignment; transform it into a fake transcript alignment
        /// so the rest of the processing can go on
        bool found_start_codon = false;
        bool found_stop_codon = false;
        ITERATE (CSpliced_seg::TModifiers, mod_it,
                 align->GetSegs().GetSpliced().GetModifiers()) {
            if ((*mod_it)->IsStart_codon_found()) {
                found_start_codon = (*mod_it)->GetStart_codon_found();
            }
            if ((*mod_it)->IsStop_codon_found()) {
                found_stop_codon = (*mod_it)->GetStop_codon_found();
            }
        }


        CBioseq_Handle bsh = m_scope->GetBioseqHandle(align->GetSeq_id(1));
        if (!bsh) {
            NCBI_THROW(CException, eUnknown,
                "Can't find genomic sequence " +
                    align->GetSeq_id(1).AsFastaString());
        }

        CSeq_align *fake_transcript_align = new CSeq_align;
        fake_transcript_align->Assign(*align);
        align.Reset(fake_transcript_align);

        CRef<CSeq_id> prot_id(new CSeq_id);
        prot_id->Assign(fake_transcript_align->GetSeq_id(0));

        {
            /// for the mRna we have to 
            /// create a local id, since the id we have in the alignment is a
            /// protein id
            static CAtomicCounter counter;
            size_t new_id_num = counter.Add(1);
            CTime time(CTime::eCurrent);
            string str("lcl|MRNA_");
            if ((m_flags & fGenerateStableLocalIds) == 0) {
                str += time.AsString("YMD");
                str += "_";
            }
            str += NStr::NumericToString(new_id_num);
            CRef<CSeq_id> fake_rna_id(new CSeq_id(str));
            fake_transcript_align->SetSegs().SetSpliced().SetProduct_id(
                *fake_rna_id);
        }
        fake_transcript_align->SetSegs().SetSpliced().SetProduct_type(
            CSpliced_seg::eProduct_type_transcript);
        NON_CONST_ITERATE (CSpliced_seg::TExons, exon_it,
                      fake_transcript_align->SetSegs().SetSpliced().SetExons())
        {
            s_TransformToNucpos((*exon_it)->SetProduct_start());
            s_TransformToNucpos((*exon_it)->SetProduct_end());
        }

        CRef<CSpliced_exon> last_exon =
            fake_transcript_align->SetSegs().SetSpliced().SetExons().back(); 
        bool aligned_to_the_end = 
            last_exon->GetProduct_end().GetNucpos()+1==
            fake_transcript_align->GetSegs().GetSpliced().GetProduct_length()*3;

        fake_transcript_align->SetSegs().SetSpliced().SetProduct_length() = 
            fake_transcript_align->GetSegs().GetSpliced().GetProduct_length()*3 +
            (((found_stop_codon && aligned_to_the_end) || !aligned_to_the_end)?3:0);

        if (found_stop_codon && aligned_to_the_end) {
            bool is_minus = last_exon->IsSetGenomic_strand() ?
                    last_exon->GetGenomic_strand() == eNa_strand_minus :
                    (fake_transcript_align->GetSegs().GetSpliced()
                        . IsSetGenomic_strand() &&
                     fake_transcript_align->GetSegs().GetSpliced()
                        . GetGenomic_strand() == eNa_strand_minus);

            TSeqPos genomic_length = bsh.GetBioseqLength();
            TSeqPos space_for_codon = min(3u, is_minus
                ? last_exon->GetGenomic_start()
                : genomic_length - last_exon->GetGenomic_end() - 1);
            if (space_for_codon < 3) {
                if (bsh.GetInst_Topology() != CSeq_inst::eTopology_circular) {
                    NCBI_THROW(CException, eUnknown,
                               "Stop codon goes outside genomic sequence");
                }
                CRef<CSpliced_exon> new_exon(new CSpliced_exon);
                new_exon->SetProduct_start().SetNucpos(
                    last_exon->GetProduct_end().GetNucpos() + space_for_codon + 1);
                new_exon->SetProduct_end().SetNucpos(
                    last_exon->GetProduct_end().GetNucpos() + 3);
                new_exon->SetGenomic_start(
                    is_minus ? genomic_length - 3 + space_for_codon : 0);
                new_exon->SetGenomic_end(
                    is_minus ? genomic_length - 1 : 2 - space_for_codon);
                if (last_exon->IsSetProduct_strand()) {
                    new_exon->SetProduct_strand(last_exon->GetProduct_strand());
                }
                if (last_exon->IsSetGenomic_strand()) {
                    new_exon->SetGenomic_strand(last_exon->GetGenomic_strand());
                }
                fake_transcript_align->SetSegs().SetSpliced().SetExons()
                    . push_back(new_exon);
            }

            /// Extend last exon to include whatever part of stop codon fits
            last_exon->SetProduct_end().SetNucpos() += space_for_codon;
            if (is_minus) {
                last_exon->SetGenomic_start() -= space_for_codon;
            } else {
                last_exon->SetGenomic_end() += space_for_codon;
            }
            if (last_exon->IsSetParts() && space_for_codon) {
                CRef<CSpliced_exon_chunk> match_stop_codon
                    (new CSpliced_exon_chunk);
                match_stop_codon->SetMatch(space_for_codon);
                last_exon->SetParts().push_back(match_stop_codon);
            }
        }

        cd_feat.Reset(new CSeq_feat);
        cd_feat->SetData().SetCdregion();

        CRef<CSeq_loc> cds_on_fake_mrna_loc(new CSeq_loc(
            fake_transcript_align->SetSegs().SetSpliced().SetProduct_id(),
            0, fake_transcript_align->GetSegs().GetSpliced().GetProduct_length()-1));
        if (!found_start_codon &&
            fake_transcript_align->SetSegs().SetSpliced().SetExons().front()->GetProduct_start().GetNucpos()==0) {
            cds_on_fake_mrna_loc->SetPartialStart(true, eExtreme_Biological);
        }
        if (!found_stop_codon && aligned_to_the_end) {
            cds_on_fake_mrna_loc->SetPartialStop(true, eExtreme_Biological);
        }
        cd_feat->SetLocation(*cds_on_fake_mrna_loc);

        const COrg_ref &org = sequence::GetOrg_ref(bsh);
        if (org.IsSetGcode()) {
            CRef<CGenetic_code::C_E> code(new CGenetic_code::C_E);
            code->SetId(org.GetGcode());
            cd_feat->SetData().SetCdregion().SetCode().Set().push_back(code);
        }

        cd_feat->SetProduct().SetWhole(*prot_id);

}

void  RenameGeneratedBioseqs(const CSeq_id& query_rna_id, CSeq_id& transcribed_rna_id,
                             CRef<CSeq_feat> cds_feat_on_query_mrna,
                             CRef<CSeq_feat> cds_feat_on_genome_with_translated_product)
{
    transcribed_rna_id.Assign(query_rna_id);
    if (cds_feat_on_genome_with_translated_product &&
        cds_feat_on_genome_with_translated_product->CanGetProduct() &&
        cds_feat_on_query_mrna &&
        cds_feat_on_query_mrna->CanGetProduct()) {
        CSeq_id* translated_protein_id = const_cast<CSeq_id*>(cds_feat_on_genome_with_translated_product->SetProduct().GetId());
        translated_protein_id->Assign(*cds_feat_on_query_mrna->GetProduct().GetId());
    }
}

CRef<CSeq_feat>
CFeatureGenerator::
SImplementation::ConvertAlignToAnnot(const CSeq_align& input_align,
                                     CSeq_annot& annot,
                                     CBioseq_set& seqs,
                                     int gene_id,
                                     const CSeq_feat* cds_feat_on_query_mrna_ptr,
                                     bool call_on_align_list)
{
    if (HasMixedGenomicIds(input_align)) {
        return ConvertMixedAlignToAnnot(input_align, annot, seqs, gene_id, cds_feat_on_query_mrna_ptr,
                                        call_on_align_list);
    }

    CConstRef<CSeq_align> align(&input_align);
    CRef<CSeq_feat> cds_feat_on_query_mrna;
    bool is_protein_align = IsProteinAlign(*align);
    if (is_protein_align) {
        TransformProteinAlignToTranscript(align, cds_feat_on_query_mrna);
    }

    CSeq_loc_Mapper::TMapOptions opts = 0;
    if (m_flags & fDensegAsExon) {
        opts |= CSeq_loc_Mapper::fAlign_Dense_seg_TotalRange;
    }

    SMapper mapper(*align, *m_scope, m_allowed_unaligned, opts);

    const CSeq_id& query_rna_id = align->GetSeq_id(mapper.GetRnaRow());

    if (!ExtractGnomonModelNum(query_rna_id).empty()) {
        m_is_gnomon = true;
    } else {
        CSeq_id_Handle best_id;
        if (!(m_flags & fDeNovoProducts)) {
            best_id = sequence::GetId(query_rna_id, *m_scope,
                                      sequence::eGetId_Best);
        }
        CSeq_id::EAccessionInfo rna_acc_info =
            best_id ? best_id.IdentifyAccession() : query_rna_id.IdentifyAccession();
        m_is_best_refseq = rna_acc_info == CSeq_id::eAcc_refseq_mrna ||
                           rna_acc_info == CSeq_id::eAcc_refseq_ncrna;
    }


    if (cds_feat_on_query_mrna_ptr) {
        cds_feat_on_query_mrna.Reset(new CSeq_feat);
        cds_feat_on_query_mrna->Assign(*cds_feat_on_query_mrna_ptr);
    } else if (!is_protein_align && !(m_flags & fDeNovoProducts)) {
        CMappedFeat cdregion_handle = GetCdsOnMrna(query_rna_id, *m_scope);
        if (cdregion_handle) {
            cds_feat_on_query_mrna.Reset(new CSeq_feat);
            cds_feat_on_query_mrna->Assign(cdregion_handle.GetMappedFeature());
        }
    }

    CMappedFeat full_length_rna;
    vector<CMappedFeat> ncRNAs;

    CBioseq_Handle query_rna_handle = m_scope->GetBioseqHandle(query_rna_id);
    if (query_rna_handle) {
        for (CFeat_CI feat_iter(query_rna_handle, CSeqFeatData::e_Rna);
             feat_iter;  ++feat_iter) {
            const CSeq_loc &rna_loc = feat_iter->GetLocation();
            if (feat_iter->GetData().GetSubtype() !=
                           CSeqFeatData::eSubtype_mRNA &&
                    ++rna_loc.begin() == rna_loc.end() &&
                    rna_loc.GetTotalRange().GetLength() ==
                        query_rna_handle.GetBioseqLength())
            {
                full_length_rna = *feat_iter;
            } else if (feat_iter->GetData().GetSubtype() ==
                           CSeqFeatData::eSubtype_ncRNA)
            {
                ncRNAs.push_back(*feat_iter);
            }
        }
    }

    CTime time(CTime::eCurrent);
    static CAtomicCounter counter;
    size_t model_num = counter.Add(1);

    /// we always need the mRNA location as a reference
    CRef<CSeq_loc> rna_feat_loc_on_genome(new CSeq_loc);
    rna_feat_loc_on_genome->Assign(mapper.GetRnaLoc());

    CRef<CSeq_feat> cds_feat_on_transcribed_mrna;
    list<CRef<CSeq_loc> > transcribed_mrna_seqloc_refs;

    /// create a new bioseq for this mRNA; if the mRNA sequence is not found,
    /// this is needed in order to translate the protein
    /// alignment, even if flag fForceTranscribeMrna wasn't set
    CRef<CSeq_id> transcribed_rna_id =
        x_CreateMrnaBioseq(*align, rna_feat_loc_on_genome, time,
                           model_num, seqs,
                           cds_feat_on_query_mrna, cds_feat_on_transcribed_mrna);

    CRef<CSeq_feat> mrna_feat_on_genome_with_translated_product =
        full_length_rna && (m_flags&fPropagateNcrnaFeats)
        /// If there is a full-length RNA feature, propagate it instead of
        /// creating a new one. Create the bioseq separately
        ? x_CreateNcRnaFeature(&full_length_rna.GetOriginalFeature(),
                               *align, rna_feat_loc_on_genome, opts)
        : x_CreateMrnaFeature(rna_feat_loc_on_genome, query_rna_id,
                              *transcribed_rna_id, cds_feat_on_query_mrna);
    if (mrna_feat_on_genome_with_translated_product &&
        !mrna_feat_on_genome_with_translated_product->IsSetProduct()) {
        /// Propagated full-length feature; add product
        mrna_feat_on_genome_with_translated_product->
            SetProduct().SetWhole().Assign(*transcribed_rna_id);
    }

    CRef<CSeq_feat> gene_feat;

    if(!call_on_align_list){
        const CSeq_id& genomic_id = align->GetSeq_id(mapper.GetGenomicRow());
        if (gene_id) {
            TGeneMap::iterator gene = genes.find(gene_id);
            if (gene == genes.end()) {
                x_CreateGeneFeature(gene_feat, query_rna_handle, mapper,
                                    rna_feat_loc_on_genome, genomic_id, gene_id);
                if (gene_feat) {
                    _ASSERT(gene_feat->GetData().Which() !=
                            CSeqFeatData::e_not_set);
                    annot.SetData().SetFtable().push_back(gene_feat);
                }
                gene = genes.insert(make_pair(gene_id,gene_feat)).first;
            } else {
                gene_feat = gene->second;                
                gene_feat->SetLocation(*MergeSeq_locs(&gene_feat->GetLocation(),
                                                      &mrna_feat_on_genome_with_translated_product->GetLocation()));
            }
    
            CRef< CSeqFeatXref > genexref( new CSeqFeatXref() );
            genexref->SetId(*gene_feat->SetIds().front());
        
            CRef< CSeqFeatXref > mrnaxref( new CSeqFeatXref() );
            mrnaxref->SetId(*mrna_feat_on_genome_with_translated_product->SetIds().front());
    
            gene_feat->SetXref().push_back(mrnaxref);
            mrna_feat_on_genome_with_translated_product->SetXref().push_back(genexref);
            
        } else {
            x_CreateGeneFeature(gene_feat, query_rna_handle, mapper,
                                rna_feat_loc_on_genome, genomic_id);
            if (gene_feat) {
                _ASSERT(gene_feat->GetData().Which() != CSeqFeatData::e_not_set);
                annot.SetData().SetFtable().push_back(gene_feat);
            }
        }
    }

    if (mrna_feat_on_genome_with_translated_product) {
        _ASSERT(mrna_feat_on_genome_with_translated_product->GetData().Which() != CSeqFeatData::e_not_set);

        annot.SetData().SetFtable().push_back(mrna_feat_on_genome_with_translated_product); // NOTE: added after gene!
    }

    CSeq_annot::C_Data::TFtable propagated_features;

    CRef<CSeq_feat> cds_feat_on_genome_with_translated_product =
        x_CreateCdsFeature(cds_feat_on_query_mrna, cds_feat_on_transcribed_mrna,
                           transcribed_mrna_seqloc_refs,
                           *align, rna_feat_loc_on_genome, time, model_num, seqs, opts);


    if(cds_feat_on_genome_with_translated_product.NotNull()) {
        propagated_features.push_back(cds_feat_on_genome_with_translated_product);

        if (cds_feat_on_query_mrna && cds_feat_on_query_mrna->CanGetProduct()) {
            CBioseq_Handle prot_handle =
                m_scope->GetBioseqHandle(*cds_feat_on_query_mrna->GetProduct().GetId());
            if (prot_handle) {
                for (CFeat_CI feat_iter(prot_handle,
                                        CSeqFeatData::e_Prot);
                     feat_iter; ++feat_iter) {
                    const CProt_ref &prot_ref =
                        feat_iter->GetData().GetProt();
                    if (prot_ref.IsSetName() &&
                        !prot_ref.GetName().empty()) {
                        CRef< CSeqFeatXref > prot_xref(
                                                       new CSeqFeatXref());
                        prot_xref->SetData().SetProt().SetName()
                            . push_back(prot_ref.GetName().front());
                        cds_feat_on_genome_with_translated_product->SetXref().push_back(prot_xref);
                        break;
                    }
                }
            }
        }
    }
        
    ITERATE(vector<CMappedFeat>, it, ncRNAs){
        CRef<CSeq_feat> ncrna_feat =
            x_CreateNcRnaFeature(&it->GetOriginalFeature(), *align, rna_feat_loc_on_genome, opts);
        if(ncrna_feat)
            propagated_features.push_back(ncrna_feat);
    }

    NON_CONST_ITERATE(CSeq_annot::C_Data::TFtable, it, propagated_features){
        _ASSERT((*it)->GetData().Which() != CSeqFeatData::e_not_set);
        annot.SetData().SetFtable().push_back(*it);
        
        if (m_is_gnomon) {  // create xrefs for gnomon models
            CRef< CSeqFeatXref > propagatedxref( new CSeqFeatXref() );
            if ((*it)->IsSetIds()) {
                propagatedxref->SetId(*(*it)->SetIds().front());
            }
        
            CRef< CSeqFeatXref > mrnaxref( new CSeqFeatXref() );
            mrnaxref->SetId(*mrna_feat_on_genome_with_translated_product->SetIds().front());
        
            (*it)->SetXref().push_back(mrnaxref);
            mrna_feat_on_genome_with_translated_product->SetXref().push_back(propagatedxref);
        }
    }

    if(!call_on_align_list){
        if(propagated_features.empty()){
            SetPartialFlags(gene_feat, mrna_feat_on_genome_with_translated_product, CRef<CSeq_feat>());
        }
        ITERATE(CSeq_annot::C_Data::TFtable, it, propagated_features){
            x_CheckInconsistentDbxrefs(gene_feat, *it);
            SetPartialFlags(gene_feat, mrna_feat_on_genome_with_translated_product, *it);
        }
        x_CopyAdditionalFeatures(query_rna_handle, mapper, annot);
    }

    if (!(m_flags & fGenerateLocalIds)) {
        if (mrna_feat_on_genome_with_translated_product) {
            mrna_feat_on_genome_with_translated_product->SetProduct().SetWhole().Assign(query_rna_id);
        }
        if (cds_feat_on_genome_with_translated_product) {
            if (cds_feat_on_query_mrna->CanGetProduct()) {
                cds_feat_on_genome_with_translated_product->
                    SetProduct().Assign(cds_feat_on_query_mrna->GetProduct());
                cds_feat_on_transcribed_mrna->
                    SetProduct().Assign(cds_feat_on_query_mrna->GetProduct());
            }
            CRef<CSeq_id> seq_id(new CSeq_id);
            seq_id->Assign(query_rna_id);
            cds_feat_on_transcribed_mrna->SetLocation().SetId(*seq_id);
            NON_CONST_ITERATE (list<CRef<CSeq_loc> >, loc, transcribed_mrna_seqloc_refs) {
                (*loc)->SetId(*seq_id);
            }
        }

        // rename generated bioseqs if query bioseqs do not exist
        if (!query_rna_handle) {
            RenameGeneratedBioseqs(query_rna_id, *transcribed_rna_id,
                                   cds_feat_on_query_mrna, cds_feat_on_genome_with_translated_product);
        }
   }

    if (mrna_feat_on_genome_with_translated_product) {
        CBioseq_Handle rna_handle =
            m_scope->GetBioseqHandle(query_rna_id);
        CSeq_entry_Handle rna_seh;
        if (!rna_handle) {
            rna_seh = m_scope->AddTopLevelSeqEntry(*seqs.SetSeq_set().front());
        }

        SetFeatureExceptions(*mrna_feat_on_genome_with_translated_product, align,
                             cds_feat_on_genome_with_translated_product.GetPointer(),
                             cds_feat_on_query_mrna.GetPointer(),
                             cds_feat_on_transcribed_mrna.GetPointer());

        if (rna_seh) {
            m_scope->RemoveTopLevelSeqEntry(rna_seh);
        }
    }
    if (cds_feat_on_genome_with_translated_product) {
        CBioseq_Handle prot_handle =
            m_scope->GetBioseqHandle(*cds_feat_on_genome_with_translated_product->GetProduct().GetId());
        CSeq_entry_Handle prot_seh;
        if (!prot_handle) {
            prot_seh = m_scope->AddTopLevelSeqEntry(*seqs.SetSeq_set().back());
        }

        TSeqPos clean_match_count = 0;
        SetFeatureExceptions(*cds_feat_on_genome_with_translated_product, align, NULL,
                             cds_feat_on_query_mrna.GetPointer(),
                             cds_feat_on_transcribed_mrna.GetPointer(),
                             &transcribed_mrna_seqloc_refs,
                             &clean_match_count);
        if (!clean_match_count) {
            /// Not even one base matched cleanly; remove feature
            annot.SetData().SetFtable().remove(cds_feat_on_genome_with_translated_product);
            cds_feat_on_genome_with_translated_product = NULL;
        }
        if (prot_seh) {
            m_scope->RemoveTopLevelSeqEntry(prot_seh);
        }
    }

    if (!(m_flags & fGenerateLocalIds)) {
        RenameGeneratedBioseqs(query_rna_id, *transcribed_rna_id, cds_feat_on_query_mrna, cds_feat_on_genome_with_translated_product);
    }
    if (m_is_gnomon) {
        // add generated bioseqs to the scope
        ITERATE(CBioseq_set::TSeq_set, it, seqs.SetSeq_set()) {
            m_scope->AddTopLevelSeqEntry(**it);
        }
    }

    if (!(m_flags & fForceTranscribeMrna) ||
        !(m_flags & fForceTranslateCds))
    {
        /// We created Bioseqs the user didn't ask for,
        /// so we need to now remove them
        for (CBioseq_set::TSeq_set::iterator bioseq_it =
                 seqs.SetSeq_set().begin();
            bioseq_it != seqs.SetSeq_set().end(); )
        {
            if (((*bioseq_it)->GetSeq().IsNa() &&
                    !(m_flags & fForceTranscribeMrna)) ||
                ((*bioseq_it)->GetSeq().IsAa() &&
                    !(m_flags & fForceTranslateCds)))
            {
                bioseq_it = seqs.SetSeq_set().erase(bioseq_it);
            } else {
                ++bioseq_it;
            }
        }
    }

    return is_protein_align ? cds_feat_on_genome_with_translated_product : mrna_feat_on_genome_with_translated_product;
}

void
CFeatureGenerator::
SImplementation::ConvertAlignToAnnot(
                         const list< CRef<CSeq_align> > &aligns,
                         CSeq_annot &annot,
                         CBioseq_set &seqs)
{
    CSeq_loc_Mapper::TMapOptions opts = 0;
    if (m_flags & fDensegAsExon) {
        opts |= CSeq_loc_Mapper::fAlign_Dense_seg_TotalRange;
    }

    CRef<CSeq_feat> gene_feat;
    CSeq_annot gene_annot;
    CSeq_id_Handle gene_handle;
    ITERATE(list< CRef<CSeq_align> >, align_it, aligns){
        CConstRef<CSeq_align> clean_align = CleanAlignment(**align_it);
        CRef<CSeq_feat> mrna_feat = ConvertAlignToAnnot(*clean_align, gene_annot, seqs, 0, NULL, true);

        SMapper mapper(*clean_align, *m_scope, m_allowed_unaligned, opts);
        const CSeq_id& genomic_id = clean_align->GetSeq_id(mapper.GetGenomicRow());
        const CSeq_id& rna_id = clean_align->GetSeq_id(mapper.GetRnaRow());
        if(!gene_handle)
            gene_handle = CSeq_id_Handle::GetHandle(genomic_id);
        else if(!(gene_handle == genomic_id))
            NCBI_THROW(CException, eUnknown,
                       "Bad list of alignments to ConvertAlignToAnnot(); alignments on different genes");

        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->Assign(mapper.GetRnaLoc());

        CBioseq_Handle handle = m_scope->GetBioseqHandle(rna_id);
        x_CreateGeneFeature(gene_feat, handle, mapper, loc, genomic_id);

        x_CopyAdditionalFeatures(handle, mapper, gene_annot);
    }
    NON_CONST_ITERATE(CSeq_annot::C_Data::TFtable, feat_it, gene_annot.SetData().SetFtable())
    {
        x_CheckInconsistentDbxrefs(gene_feat, *feat_it);
    }
    gene_annot.SetData().SetFtable().push_front(gene_feat);
    RecomputePartialFlags(gene_annot);
    annot.SetData().SetFtable().splice(annot.SetData().SetFtable().end(),
                                       gene_annot.SetData().SetFtable());
}

bool IsContinuous(const CSeq_loc& loc)
{
    ITERATE (CSeq_loc, loc_it, loc) {
        if ((loc_it.GetRange().GetFrom() != loc.GetTotalRange().GetFrom() && loc_it.GetRangeAsSeq_loc()->IsPartialStart(eExtreme_Positional)) ||
            (loc_it.GetRange().GetTo() != loc.GetTotalRange().GetTo() && loc_it.GetRangeAsSeq_loc()->IsPartialStop(eExtreme_Positional))) {
            return false;
        }
    }
    return true;
}
    
void AddLiteral(CSeq_inst& inst, const string& seq, CSeq_inst::EMol mol_class)
{
    if (inst.IsSetExt()) {
        if (!inst.SetExt().SetDelta().Set().empty()) {
            CDelta_seq& delta_seq = *inst.SetExt().SetDelta().Set().back();
            if (delta_seq.IsLiteral() && delta_seq.GetLiteral().IsSetSeq_data()) {
                string iupacna;
                switch(delta_seq.GetLiteral().GetSeq_data().Which()) {
                case CSeq_data::e_Iupacna:
                    iupacna = delta_seq.GetLiteral().GetSeq_data().GetIupacna();
                    break;
                case CSeq_data::e_Ncbi2na:
                    CSeqConvert::Convert(delta_seq.GetLiteral().GetSeq_data().GetNcbi2na().Get(), CSeqUtil::e_Ncbi2na,
                                         0, delta_seq.GetLiteral().GetLength(), iupacna, CSeqUtil::e_Iupacna);
                    break;
                case CSeq_data::e_Ncbi4na:
                    CSeqConvert::Convert(delta_seq.GetLiteral().GetSeq_data().GetNcbi4na().Get(), CSeqUtil::e_Ncbi4na,
                                         0, delta_seq.GetLiteral().GetLength(), iupacna, CSeqUtil::e_Iupacna);
                    break;
                case CSeq_data::e_Ncbi8na:
                    CSeqConvert::Convert(delta_seq.GetLiteral().GetSeq_data().GetNcbi8na().Get(), CSeqUtil::e_Ncbi8na,
                                         0, delta_seq.GetLiteral().GetLength(), iupacna, CSeqUtil::e_Iupacna);
                    break;
                case CSeq_data::e_Iupacaa:
                    delta_seq.SetLiteral().SetSeq_data().SetIupacaa().Set() += seq;
                    delta_seq.SetLiteral().SetLength() += seq.size();
                    return;
                default:
                    inst.SetExt().SetDelta().AddLiteral(seq, mol_class);
                    return;
                }
                iupacna += seq;
                delta_seq.SetLiteral().SetSeq_data().SetIupacna().Set(iupacna);
                delta_seq.SetLiteral().SetLength(iupacna.size());
                CSeqportUtil::Pack(&delta_seq.SetLiteral().SetSeq_data());
                return;
            }
        }
        inst.SetExt().SetDelta().AddLiteral(seq, mol_class);
    } else {
        inst.SetSeq_data().SetIupacna().Set() += seq;
    }
}

void CFeatureGenerator::
SImplementation::x_CollectMrnaSequence(CSeq_inst& inst,
                                       const CSeq_align& align,
                                       const CSeq_loc& loc,
                                       bool add_unaligned_parts,
                                       bool mark_transcript_deletions,
                                       bool* has_gap,
                                       bool* has_indel)
{
    /// set up the inst
    inst.SetMol(CSeq_inst::eMol_rna);

    // this is created as a transcription of the genomic location

    CSeq_loc_Mapper to_mrna(align, CSeq_loc_Mapper::eSplicedRow_Prod);
    CSeq_loc_Mapper to_genomic(align, CSeq_loc_Mapper::eSplicedRow_Gen);

    to_mrna.SetMergeNone();
    to_genomic.SetMergeNone();

    int seq_size = 0;
    int prev_product_to = -1;
    bool prev_fuzz = false;

    for (CSeq_loc_CI loc_it(loc,
                            CSeq_loc_CI::eEmpty_Skip,
                            CSeq_loc_CI::eOrder_Biological);
         loc_it;  ++loc_it) {

        CConstRef<CSeq_loc> exon = loc_it.GetRangeAsSeq_loc();
        CRef<CSeq_loc> mrna_loc = to_mrna.Map(*exon);

        if ((prev_product_to > -1  &&
             loc_it.GetRangeAsSeq_loc()->IsPartialStart(eExtreme_Biological))  ||
            prev_fuzz) {
            if (has_gap != NULL) {
                *has_gap = true;
            }
            if (!inst.IsSetExt()) {
                inst.SetExt().SetDelta().AddLiteral
                    (inst.GetSeq_data().GetIupacna().Get(),CSeq_inst::eMol_rna);
                inst.ResetSeq_data();
            }
            int gap_len = add_unaligned_parts ? mrna_loc->GetTotalRange().GetFrom()-(prev_product_to+1) : 0;
            seq_size += gap_len;
            prev_product_to += gap_len;
            inst.SetExt().SetDelta().AddLiteral(gap_len);
            if (gap_len == 0)
                inst.SetExt().SetDelta().Set().back()
                    ->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
        }

        int part_count = 0;
        for (CSeq_loc_CI part_it(*mrna_loc);  part_it;  ++part_it) {
            ++part_count;
            if (prev_product_to<0) {
                prev_product_to = part_it.GetRange().GetFrom()-1;
                if (add_unaligned_parts && part_it.GetRange().GetFrom() > 0) {
                    seq_size = part_it.GetRange().GetFrom();
                    inst.SetExt().SetDelta().AddLiteral(seq_size);
                }
            }
            int deletion_len = part_it.GetRange().GetFrom()-(prev_product_to+1);
            /// If this is the first part of the mapped segment, the deletion is
            /// in the CDS location on the transcript; mark with Ns only if
            /// mark_transcript_deletions is set. If this is a later part, the
            /// deletion is in the transcript mapping to the genomic sequence;
            /// mark always
            if (deletion_len > 0) {
                if (mark_transcript_deletions && part_count == 1) {
                    // check if the deletion is in the alignment or the original multupart cds
                    
                    CSeq_loc deletion_loc;
                    deletion_loc.SetInt().SetId().Assign(part_it.GetSeq_id());
                    deletion_loc.SetInt().SetFrom(prev_product_to+1);
                    deletion_loc.SetInt().SetTo(part_it.GetRange().GetFrom()-1);

                    deletion_len -= (int)GetLength(*to_genomic.Map(deletion_loc), NULL);
                }

                if (deletion_len > 0 && (mark_transcript_deletions || part_count > 1)) {
                    if (has_indel != NULL) {
                        *has_indel = true;
                    }
                    string deletion(deletion_len, 'N');
                    AddLiteral(inst, deletion, CSeq_inst::eMol_rna);
                    seq_size += deletion.size();
                }
            }

            CConstRef<CSeq_loc> part = part_it.GetRangeAsSeq_loc();
            CRef<CSeq_loc> genomic_loc = to_genomic.Map(*part);
           CSeqVector vec(*genomic_loc, *m_scope, CBioseq_Handle::eCoding_Iupac);
            string seq;
            vec.GetSeqData(0, vec.size(), seq);

            AddLiteral(inst, seq, CSeq_inst::eMol_rna);

            seq_size += vec.size();

            prev_product_to = part_it.GetRange().GetTo();
        }
        if (part_count > 1 && has_indel != NULL) {
            *has_indel = true;
        }
            
        prev_fuzz = loc_it.GetRangeAsSeq_loc()->IsPartialStop(eExtreme_Biological);
    }

    if (add_unaligned_parts && align.GetSegs().IsSpliced()) {
        const CSpliced_seg& spl = align.GetSegs().GetSpliced();
        if (spl.IsSetProduct_length()) {
            TSeqPos length = spl.GetProduct_length();
            if (seq_size < (int)length) {
                if (!inst.IsSetExt()) {
                    inst.SetExt().SetDelta().AddLiteral
                        (inst.GetSeq_data().GetIupacna().Get(),CSeq_inst::eMol_rna);
                    inst.ResetSeq_data();
                }
                inst.SetExt().SetDelta().AddLiteral(length-seq_size);
                seq_size = length;
            }
        }
    }

    inst.SetLength(seq_size);
    if (inst.IsSetExt()) {
        inst.SetRepr(CSeq_inst::eRepr_delta);
    } else {
        inst.SetRepr(CSeq_inst::eRepr_raw);
        CSeqportUtil::Pack(&inst.SetSeq_data());
    }
}

CRef<CSeq_id> CFeatureGenerator::
SImplementation::x_CreateMrnaBioseq(const CSeq_align& align,
                                    CConstRef<CSeq_loc> rna_feat_loc_on_genome,
                                    const CTime& time,
                                    size_t model_num,
                                    CBioseq_set& seqs,
                                    CConstRef<CSeq_feat> cds_feat_on_query_mrna,
                                    CRef<CSeq_feat>& cds_feat_on_transcribed_mrna)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq& bioseq = entry->SetSeq();
    
    CRef<CSeqdesc> mdes(new CSeqdesc);
    entry->SetSeq().SetDescr().Set().push_back(mdes);
    mdes->SetMolinfo().SetBiomol(cds_feat_on_query_mrna.IsNull()
        ? CMolInfo::eBiomol_transcribed_RNA : CMolInfo::eBiomol_mRNA);

    CMolInfo::ECompleteness completeness;
    if (!IsContinuous(*rna_feat_loc_on_genome)) {
        completeness = CMolInfo::eCompleteness_partial;
    } else if (cds_feat_on_query_mrna.IsNull()) {
        completeness = CMolInfo::eCompleteness_unknown;
    } else if (cds_feat_on_query_mrna->GetLocation().IsPartialStart(eExtreme_Biological) &&
               cds_feat_on_query_mrna->GetLocation().IsPartialStop(eExtreme_Biological)
               ) {
        completeness = CMolInfo::eCompleteness_no_ends;
    } else if (cds_feat_on_query_mrna->GetLocation().IsPartialStart(eExtreme_Biological)) {
        completeness = CMolInfo::eCompleteness_no_left;
    } else if (cds_feat_on_query_mrna->GetLocation().IsPartialStop(eExtreme_Biological)) {
        completeness = CMolInfo::eCompleteness_no_right;
    } else {
        completeness = CMolInfo::eCompleteness_unknown;
    }
    mdes->SetMolinfo().SetCompleteness(completeness);

    x_CollectMrnaSequence(bioseq.SetInst(), align, *rna_feat_loc_on_genome);

    CRef<CSeq_align> assembly(new CSeq_align);
    assembly->Assign(align);
    bioseq.SetInst().SetHist().SetAssembly().push_back(assembly);

    CRef<CSeq_id> transcribed_rna_id(new CSeq_id);
    {{
        /// create a new seq-id for this
        string str("lcl|CDNA_");
        if ((m_flags & fGenerateStableLocalIds) == 0) {
            str += time.AsString("YMD");
            str += "_";
        }
        str += NStr::SizetToString(model_num);
        transcribed_rna_id->Set(str);
    }}
    bioseq.SetId().push_back(transcribed_rna_id);

    if (cds_feat_on_query_mrna.NotNull()) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        entry->SetSeq().SetAnnot().push_back(annot);
        _ASSERT(cds_feat_on_query_mrna->GetData().Which() != CSeqFeatData::e_not_set);

        cds_feat_on_transcribed_mrna.Reset(new CSeq_feat);
        cds_feat_on_transcribed_mrna->Assign(*cds_feat_on_query_mrna);
        cds_feat_on_transcribed_mrna->SetLocation().SetId(*transcribed_rna_id);

        annot->SetData().SetFtable().push_back(cds_feat_on_transcribed_mrna);

        // remap code-breaks
        CSeqFeatData::TCdregion& cds =
            cds_feat_on_transcribed_mrna->SetData().SetCdregion();
        if (cds.IsSetCode_break()) {
            for (CCdregion::TCode_break::iterator it = cds.SetCode_break().begin();  it != cds.SetCode_break().end(); ++it) {
                (*it)->SetLoc().SetId(*transcribed_rna_id);
            }
        }
    }
    
    if ((m_flags & fForceTranscribeMrna) && (m_flags & fForceTranslateCds)) {
        seqs.SetClass(CBioseq_set::eClass_nuc_prot);
    }

    seqs.SetSeq_set().push_back(entry);

    return transcribed_rna_id;
}

void AddCodeBreak(CSeq_feat& feat, CSeq_loc& loc, char ncbieaa)
{
    CRef<CCode_break> code_break(new CCode_break);
    code_break->SetLoc(loc);
    code_break->SetAa().SetNcbieaa(ncbieaa);
    if (feat.IsSetData() && feat.SetData().IsCdregion()) {
        feat.SetData().SetCdregion().SetCode_break().push_back(code_break);
    } else {
        NCBI_THROW(CException, eUnknown, "Adding code break to non-cdregion feature");
    }
}

const CBioseq& CFeatureGenerator::
SImplementation::x_CreateProteinBioseq(CSeq_loc* cds_loc,
                                       CRef<CSeq_feat> cds_feat_on_transcribed_mrna,
                                       list<CRef<CSeq_loc> >& transcribed_mrna_seqloc_refs,
                                       const CTime& time,
                                       size_t model_num,
                                       CBioseq_set& seqs)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq& bioseq = entry->SetSeq();
    
    // create a new seq-id for this
    string str("lcl|PROT_");
    if ((m_flags & fGenerateStableLocalIds) == 0) {
        str += time.AsString("YMD");
        str += "_";
    }
    str += NStr::SizetToString(model_num);
    CRef<CSeq_id> translated_protein_id(new CSeq_id(str));
    cds_feat_on_transcribed_mrna->SetProduct().SetWhole(*translated_protein_id);

    bioseq.SetId().push_back(translated_protein_id);

    CRef<CSeqdesc> desc(new CSeqdesc);
    desc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);

    CMolInfo::ECompleteness completeness;
    if (!IsContinuous(*cds_loc)) {
        completeness = CMolInfo::eCompleteness_partial;
    } else if (cds_loc->IsPartialStart(eExtreme_Biological) &&
               cds_loc->IsPartialStop(eExtreme_Biological)
               ) {
        completeness = CMolInfo::eCompleteness_no_ends;
    } else if (cds_loc->IsPartialStart(eExtreme_Biological)) {
        completeness = CMolInfo::eCompleteness_no_left;
    } else if (cds_loc->IsPartialStop(eExtreme_Biological)) {
        completeness = CMolInfo::eCompleteness_no_right;
    } else {
        completeness = CMolInfo::eCompleteness_complete;
    }
    desc->SetMolinfo().SetCompleteness(completeness);

    bioseq.SetDescr().Set().push_back(desc);

    // set up the inst

    CSeq_entry_Handle mrna_seh = m_scope->AddTopLevelSeqEntry(*seqs.SetSeq_set().back());

    string strprot;
    CSeqTranslator::Translate(*cds_feat_on_transcribed_mrna, *m_scope, strprot, true, false);

    CRef<CSeq_loc> protloc_on_mrna(new CSeq_loc);
    protloc_on_mrna->Assign(cds_feat_on_transcribed_mrna->GetLocation());
    protloc_on_mrna->SetId(*const_cast<CSeq_id*>(cds_feat_on_transcribed_mrna->GetLocation().GetId()));

    // Remove final stop codon from sequence
    bool final_code_break = false;
    if (!protloc_on_mrna->IsPartialStop(eExtreme_Biological)) {
        final_code_break = (strprot[strprot.size()-1] != '*');
            
        strprot.resize(strprot.size()-1);
    }

    CSeq_inst& seq_inst = bioseq.SetInst();
    seq_inst.SetMol(CSeq_inst::eMol_aa);

        seq_inst.SetRepr(CSeq_inst::eRepr_delta);
        seq_inst.SetExt().SetDelta();
        CSeqVector seqv(*protloc_on_mrna, m_scope, CBioseq_Handle::eCoding_Ncbi);
        CConstRef<CSeqMap> map;
        map.Reset(&seqv.GetSeqMap());

        const CCdregion& cdr = cds_feat_on_transcribed_mrna->GetData().GetCdregion();
        int frame = 0;
        if (cdr.IsSetFrame ()) {
            switch (cdr.GetFrame ()) {
            case CCdregion::eFrame_two :
                frame = 1;
                break;
            case CCdregion::eFrame_three :
                frame = 2;
                break;
            default :
                break;
            }
        }

        bool starts_with_code_break = false;
        if (cdr.IsSetCode_break()) {
            ITERATE (CCdregion::TCode_break, it, cdr.GetCode_break()) {
                if ((*it)->GetLoc().GetStart(eExtreme_Positional) == 0) {
                    starts_with_code_break = true;
                    break;
                }
            }
        }

        size_t b = 0;
        size_t e = 0;
        size_t skip_5_prime = 0;
        size_t skip_3_prime = 0;

        for( CSeqMap_CI ci = map->BeginResolved(m_scope.GetPointer()); ci; ci.Next()) {
            int codon_start_pos = (int)ci.GetPosition() + frame;
            int len = int(ci.GetLength()) - frame;
            frame = len >=0 ? -(len%3) : -len;
            _ASSERT( -3 < frame && frame < 3 );
            len += frame;
            if (len==0) {
                if (b==0 &&
                    (ci.IsUnknownLength() || !ci.IsSetData()) &&
                    cds_loc->IsPartialStart(eExtreme_Biological)) {

                    skip_5_prime += 1;
                    b += 1;
                    frame += 3;
                }
                continue;
            }
            e = b + len/3;
            bool stop_codon_included = e > strprot.size();
            if (stop_codon_included) {
                _ASSERT( len%3 != 0 || !protloc_on_mrna->IsPartialStop(eExtreme_Biological) );
                --e;
                len = len >= 3 ? len-3 : 0;
            }

            if (ci.IsUnknownLength()) {
                seq_inst.SetExt().SetDelta().AddLiteral(len);
                seq_inst.SetExt().SetDelta().Set().back()->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
            } else if (!ci.IsSetData()) { // unaligned mRNA portion
                if (b==skip_5_prime &&
                    cds_loc->IsPartialStart(eExtreme_Biological)) {
                    skip_5_prime += e-b;
                } else if (stop_codon_included && b==e) {
                    // just stop codon
                    // do not add zero length gap 
                } else {
                    if (strprot[b] != 'X') { // preceding partial codon translated unambiguously - add it
                        AddLiteral(seq_inst, strprot.substr(b,1), CSeq_inst::eMol_aa);
                        b += 1;
                    }
                    if (b < e) {
                        seq_inst.SetExt().SetDelta().AddLiteral(e-b);
                    }
                }
            } else {
                if (stop_codon_included && final_code_break) {
                    TSeqPos pos_on_mrna = codon_start_pos + protloc_on_mrna->GetStart(eExtreme_Positional) + (e-b)*3;
                    CRef<CSeq_loc> stop_codon_on_mrna = protloc_on_mrna->Merge(CSeq_loc::fMerge_SingleRange, NULL);
                    stop_codon_on_mrna->SetInt().SetFrom(pos_on_mrna);
                    stop_codon_on_mrna->SetInt().SetTo(pos_on_mrna + 2);
                    AddCodeBreak(*cds_feat_on_transcribed_mrna, *stop_codon_on_mrna, '*');
                    transcribed_mrna_seqloc_refs.push_back(stop_codon_on_mrna);
                }
                if (b < e) {

                    if (b==0 && strprot[b] != 'M' &&
                        !starts_with_code_break &&
                        !protloc_on_mrna->IsPartialStart(eExtreme_Biological)) {
                        strprot[b] = 'M';
                        TSeqPos pos_on_mrna = codon_start_pos + protloc_on_mrna->GetStart(eExtreme_Positional);
                        CRef<CSeq_loc> start_codon_on_mrna = protloc_on_mrna->Merge(CSeq_loc::fMerge_SingleRange, NULL);
                        start_codon_on_mrna->SetInt().SetFrom(pos_on_mrna);
                        start_codon_on_mrna->SetInt().SetTo(pos_on_mrna + 2);
                        AddCodeBreak(*cds_feat_on_transcribed_mrna, *start_codon_on_mrna, 'M');
                        transcribed_mrna_seqloc_refs.push_back(start_codon_on_mrna);
                    }

                    // Repair any internal stops with Xs
                    size_t stop_aa_pos = b-1;
                    while ((stop_aa_pos = strprot.find('*', stop_aa_pos+1)) < e) {
                        strprot[stop_aa_pos] = 'X';

                        TSeqPos pos_on_mrna = codon_start_pos + protloc_on_mrna->GetStart(eExtreme_Positional) + (stop_aa_pos-b)*3;
                        CRef<CSeq_loc> internal_stop_on_mrna = protloc_on_mrna->Merge(CSeq_loc::fMerge_SingleRange, NULL);
                        internal_stop_on_mrna->SetInt().SetFrom(pos_on_mrna);
                        internal_stop_on_mrna->SetInt().SetTo(pos_on_mrna + 2);
                        AddCodeBreak(*cds_feat_on_transcribed_mrna, *internal_stop_on_mrna, 'X');
                        transcribed_mrna_seqloc_refs.push_back(internal_stop_on_mrna);
                    }


                    AddLiteral(seq_inst, strprot.substr(b,e-b), CSeq_inst::eMol_aa);
                }
            }
            b = e;
        }
        _ASSERT( -2 <= frame && frame <= 0 );

        if (frame < 0) { //last codon partial
            if (b < strprot.size() && strprot[b] != 'X') { // last partial codon translated unambiguously - add it
                _ASSERT( b == strprot.size()-1 );
                AddLiteral(seq_inst, strprot.substr(b,1), CSeq_inst::eMol_aa);
                b += 1;
                frame = 0;
            }
        }

        _ASSERT( b <= strprot.size() &&
                 strprot.size() <= b + (frame==0?0:1) );

    if (cds_loc->IsPartialStop(eExtreme_Biological)) {
        while (seq_inst.GetExt().GetDelta().Get().size() > 0 &&
               !seq_inst.GetExt().GetDelta().Get().back()->GetLiteral().IsSetSeq_data()) {
            skip_3_prime += seq_inst.GetExt().GetDelta().Get().back()->GetLiteral().GetLength();
            seq_inst.SetExt().SetDelta().Set().pop_back();
        }
    }

    if (skip_5_prime || skip_3_prime) {
        CSeq_loc_Mapper to_prot(*cds_feat_on_transcribed_mrna, CSeq_loc_Mapper::eLocationToProduct);
        CSeq_loc_Mapper to_mrna(*cds_feat_on_transcribed_mrna, CSeq_loc_Mapper::eProductToLocation);

        CRef<CSeq_loc> prot_loc = to_prot.Map(*protloc_on_mrna)->Merge(CSeq_loc::fMerge_SingleRange, NULL);

        prot_loc->SetInt().SetFrom(skip_5_prime);
        prot_loc->SetInt().SetTo(b-skip_3_prime-1+(skip_3_prime?0:1));
        prot_loc->SetPartialStart(skip_5_prime, eExtreme_Biological);
        prot_loc->SetPartialStop(skip_3_prime, eExtreme_Biological);

        cds_feat_on_transcribed_mrna->SetLocation(*to_mrna.Map(*prot_loc));
    }

    seq_inst.SetLength(b-skip_5_prime-skip_3_prime);

    if (seq_inst.SetExt().SetDelta().Set().size() == 1 && seq_inst.SetExt().SetDelta().Set().back()->GetLiteral().IsSetSeq_data()) {
        seq_inst.SetRepr(CSeq_inst::eRepr_raw);
        CRef<CSeq_data> dprot(new CSeq_data);
        dprot->Assign(seq_inst.SetExt().SetDelta().Set().back()->GetLiteral().GetSeq_data());
        seq_inst.SetSeq_data(*dprot);
        seq_inst.ResetExt();
    }

#ifdef _DEBUG
    CSeq_entry_Handle prot_seh = m_scope->AddTopLevelSeqEntry(*entry);

    CBioseq_Handle prot_h = m_scope->GetBioseqHandle(*translated_protein_id);
    CSeqVector vec(prot_h, CBioseq_Handle::eCoding_Iupac);
    string result;
    vec.GetSeqData(0, vec.size(), result);
    _ASSERT( b-skip_5_prime-skip_3_prime==result.size() );

    m_scope->RemoveTopLevelSeqEntry(prot_seh);
#endif

    m_scope->RemoveTopLevelSeqEntry(mrna_seh);
    
    seqs.SetSeq_set().push_back(entry);
    return bioseq;
}

CRef<CSeq_feat>
CFeatureGenerator::
SImplementation::x_CreateMrnaFeature(CRef<CSeq_loc> loc,
                                     const CSeq_id& query_rna_id,
                                     CSeq_id& transcribed_rna_id,
                                     CConstRef<CSeq_feat> cds_feat_on_query_mrna)
{
    CRef<CSeq_feat> mrna_feat;
    if (m_flags & fCreateMrna) {
        mrna_feat.Reset(new CSeq_feat());
        CRNA_ref::TType type = CRNA_ref::eType_unknown;
        string name;    
        string RNA_class;

        string gnomon_model_num = ExtractGnomonModelNum(query_rna_id);
        if (!gnomon_model_num.empty()) {
            CRef<CObject_id> obj_id( new CObject_id() );
            obj_id->SetStr("rna." + gnomon_model_num);
            CRef<CFeat_id> feat_id( new CFeat_id() );
            feat_id->SetLocal(*obj_id);
            mrna_feat->SetIds().push_back(feat_id);
        }

        mrna_feat->SetProduct().SetWhole().Assign(transcribed_rna_id);
        CBioseq_Handle handle = m_scope->GetBioseqHandle(query_rna_id);
        if (handle) {
            const CMolInfo* info = s_GetMolInfo(handle);
            if (info  &&  info->IsSetBiomol()) {
                switch (info->GetBiomol()) {
                case CMolInfo::eBiomol_mRNA:
                    type = CRNA_ref::eType_mRNA;
                    break;
                case CMolInfo::eBiomol_rRNA:
                    type = CRNA_ref::eType_rRNA;
                    break;
                case CMolInfo::eBiomol_tRNA:
                    type = CRNA_ref::eType_tRNA;
                    break;
                case CMolInfo::eBiomol_snRNA:
                    type = CRNA_ref::eType_snRNA;
                    break;
                case CMolInfo::eBiomol_snoRNA:
                    type = CRNA_ref::eType_snoRNA;
                    break;
                case CMolInfo::eBiomol_scRNA:
                    type = CRNA_ref::eType_scRNA;
                    break;
                case CMolInfo::eBiomol_pre_RNA:
                    type = CRNA_ref::eType_premsg;
                    break;
                case CMolInfo::eBiomol_ncRNA:
                    type = CRNA_ref::eType_ncRNA;
                    if (info->IsSetGbmoltype()) {
                        RNA_class = info->GetGbmoltype();
                    }
                    break;
                case CMolInfo::eBiomol_transcribed_RNA:
                    type = CRNA_ref::eType_miscRNA;
                    break;
                default:
                    type = CRNA_ref::eType_other;
                    break;
                }
            }
        } else {
            type = cds_feat_on_query_mrna.IsNull()
                ? CRNA_ref::eType_miscRNA : CRNA_ref::eType_mRNA;
        }

        mrna_feat->SetData().SetRna().SetType(type);
        if (!RNA_class.empty()) {
            mrna_feat->SetData().SetRna().SetExt().SetGen().SetClass(RNA_class);
        }
        name = x_ConstructRnaName(handle);
        if (!name.empty()) {
            if (!RNA_class.empty()) {
                mrna_feat->SetData().SetRna().SetExt().SetGen().SetProduct(name);
            } else {
                mrna_feat->SetData().SetRna().SetExt().SetName(name);
            }
        }

        mrna_feat->SetLocation(*loc);
    }
    return mrna_feat;
}

void
CFeatureGenerator::
SImplementation::x_CreateGeneFeature(CRef<CSeq_feat> &gene_feat,
                                     const CBioseq_Handle& handle,
                                     SMapper& mapper,
                                     CRef<CSeq_loc> loc,
                                     const CSeq_id& genomic_id, int gene_id)
{
    if (m_flags & fCreateGene) {
        CFeat_CI feat_iter;
        if (handle) {
            feat_iter = CFeat_CI(handle, CSeqFeatData::eSubtype_gene);
        }
        bool update_existing_gene = gene_feat;
        string gene_id_str = "gene.";
        if (gene_id) {
            gene_id_str += NStr::IntToString(gene_id);
        }

        if (!update_existing_gene) {
            if (feat_iter  &&  feat_iter.GetSize()) {
                gene_feat.Reset(new CSeq_feat());
                gene_feat->Assign(feat_iter->GetOriginalFeature());
            } 
            if (!(m_flags & fPropagateOnly)) {
                /// if we didn't find am existing gene feature, create one
                if (!gene_feat) {
                    gene_feat.Reset(new CSeq_feat());
                }
                if (gene_id) {
                    CRef<CObject_id> obj_id( new CObject_id() );
                    obj_id->SetStr(gene_id_str);
                    CRef<CFeat_id> feat_id( new CFeat_id() );
                    feat_id->SetLocal(*obj_id);
                    gene_feat->SetIds().push_back(feat_id);
                }
            }
        }

        if (!gene_feat) {
            /// Couldn't create gene feature
            return;
        }

        CRef<CSeq_loc> gene_loc;
        if (!(m_flags & fPropagateOnly)) {
            gene_loc = loc;
        } else if (feat_iter  &&  feat_iter.GetSize()) {
            gene_loc = mapper.Map(feat_iter->GetLocation());
        }

        if (gene_loc) {
            gene_feat->SetLocation
                (*MergeSeq_locs(gene_loc,
                                update_existing_gene ? &gene_feat->GetLocation() : NULL));
        }

        if (feat_iter  &&  feat_iter.GetSize() == 1 && update_existing_gene) {
            /// check if gene feature has any dbxrefs that we don't have yet
            if (feat_iter->IsSetDbxref()) {
                ITERATE (CSeq_feat::TDbxref, xref_it,
                         feat_iter->GetDbxref()) {
                    CRef<CDbtag> tag(new CDbtag);
                    tag->Assign(**xref_it);
                    bool duplicate = false;
                    if(gene_feat->IsSetDbxref()){
                        /// Check for duplications
                        ITERATE(CSeq_feat::TDbxref, previous_xref_it,
                                gene_feat->GetDbxref())
                            if((*previous_xref_it)->Match(**xref_it)){
                                duplicate = true;
                                break;
                            }
                    }
                    if(!duplicate)
                        gene_feat->SetDbxref().push_back(tag);
                }
            }
        }

        if ( !gene_feat->SetData().SetGene().IsSetLocus() ) {
            /// Didn't find locus in bioseq's gene feature; try to use bioseq's title instead
            string title;
            if (handle) {
                title = sequence::CDeflineGenerator().GenerateDefline(handle);
            }
            if ( !title.empty() ) {
                gene_feat->SetData().SetGene().SetLocus(title);
            }    
        }

        if (gene_id) {
            /// Special case for gnomon, set gene desc from gnomon id
            gene_feat->SetData().SetGene().SetDesc(gene_id_str);
        }
    }
}

CRef<CSeq_feat>
CFeatureGenerator::
SImplementation::x_CreateCdsFeature(CConstRef<CSeq_feat> cds_feat_on_query_mrna,
                                    CRef<CSeq_feat> cds_feat_on_transcribed_mrna,
                                    list<CRef<CSeq_loc> >& transcribed_mrna_seqloc_refs,
                                    const CSeq_align& align,
                                    CConstRef<CSeq_loc> loc,
                                    const CTime& time,
                                    size_t model_num,
                                    CBioseq_set& seqs,
                                    CSeq_loc_Mapper::TMapOptions opts)
{
    CRef<CSeq_feat> cds_feat;
    if (!(m_flags & fCreateCdregion) || cds_feat_on_query_mrna.IsNull()) {
        return cds_feat;
    }

            TSeqPos offset;
            CRef<CSeq_feat> cds_feat_on_genome = x_MapFeature(cds_feat_on_query_mrna.GetNonNullPointer(),
                                                              align, loc, opts, offset);
            CRef<CSeq_loc> cds_loc;
            if (cds_feat_on_genome) {
                cds_loc = &cds_feat_on_genome->SetLocation();
            }
            if (cds_loc  && cds_loc->Which() != CSeq_loc::e_not_set) {
                CRangeCollection<TSeqPos> loc_ranges;
                ITERATE (CSeq_loc, loc_it, *cds_loc) {
                    loc_ranges += loc_it.GetRange();
                }

                bool is_partial_5prime = offset > 0 || cds_loc->IsPartialStart(eExtreme_Biological);
                cds_loc->SetPartialStart(is_partial_5prime, eExtreme_Biological);

                string gnomon_model_num;

                if (cds_feat_on_query_mrna->CanGetProduct()) {
                    gnomon_model_num = ExtractGnomonModelNum(
                        *cds_feat_on_query_mrna->GetProduct().GetId());
                }
                    /// create a new bioseq for the CDS
                    if (!gnomon_model_num.empty()) {
                        CRef<CObject_id> obj_id( new CObject_id() );
                        obj_id->SetStr("cds." + gnomon_model_num);
                        CRef<CFeat_id> feat_id( new CFeat_id() );
                        feat_id->SetLocal(*obj_id);
                        cds_feat_on_transcribed_mrna->SetIds().push_back(feat_id);
                    }
                    x_CreateProteinBioseq(cds_loc, cds_feat_on_transcribed_mrna,
                                          transcribed_mrna_seqloc_refs,
                                          time, model_num, seqs);

               cds_feat.Reset(new CSeq_feat());
               cds_feat->Assign(*cds_feat_on_transcribed_mrna);
               cds_feat->ResetId();

               cds_feat->SetLocation(*cds_loc);

                /// make sure we set the CDS frame correctly
                /// if we're 5' partial, we may need to adjust the frame
                /// to ensure that conceptual translations are in-frame
                if (is_partial_5prime && offset) {
                    int orig_frame = 0;
                    if (cds_feat->GetData().GetCdregion().IsSetFrame()) {
                        orig_frame = cds_feat->GetData()
                            .GetCdregion().GetFrame();
                        if (orig_frame) {
                            orig_frame -= 1;
                        }
                    }
                    int frame = (offset - orig_frame) % 3;
                    if (frame < 0) {
                        frame = -frame;
                    }
                    frame = (3 - frame) % 3;
                    if (frame != orig_frame) {
                        switch (frame) {
                        case 0:
                            cds_feat->SetData().SetCdregion()
                                .SetFrame(CCdregion::eFrame_one);
                            break;
                        case 1:
                            cds_feat->SetData().SetCdregion()
                                .SetFrame(CCdregion::eFrame_two);
                            break;
                        case 2:
                            cds_feat->SetData().SetCdregion()
                                .SetFrame(CCdregion::eFrame_three);
                            break;

                        default:
                            NCBI_THROW(CException, eUnknown,
                                       "mod 3 out of bounds");
                        }
                    }
                }

                if (!gnomon_model_num.empty() && !is_partial_5prime) {
                    int cds_start = cds_feat_on_transcribed_mrna->GetLocation().GetTotalRange().GetFrom();
                    if (cds_start >= 3) {
                        CBioseq_Handle rna_handle =
                            m_scope->GetBioseqHandle(*cds_feat_on_transcribed_mrna->GetLocation().GetId());

                        string strprot;
                        if (rna_handle) {
                            CSeqVector vec(rna_handle, CBioseq_Handle::eCoding_Iupac);
                            string mrna;
                            vec.GetSeqData(cds_start % 3, cds_start, mrna);
                            const CGenetic_code *code = NULL;
                            if (cds_feat_on_transcribed_mrna->GetData().GetCdregion().IsSetCode()) {
                                code = &cds_feat_on_transcribed_mrna->GetData().GetCdregion().GetCode();
                            }
                            CSeqTranslator::Translate
                                (mrna, strprot,
                                 CSeqTranslator::fIs5PrimePartial, code);
                        }
                        SIZE_TYPE stop_5prime = strprot.rfind('*');
                        if (stop_5prime != NPOS) {
                            stop_5prime = stop_5prime*3+cds_start%3;
                            CRef<CSeq_feat> stop_5prime_feature(new CSeq_feat);
                            stop_5prime_feature->SetData().SetImp().SetKey("misc_feature");
                            stop_5prime_feature->SetComment("upstream in-frame stop codon");
                            CRef<CSeq_loc> stop_5prime_location(new CSeq_loc());
                            stop_5prime_location->SetInt().SetFrom(stop_5prime);
                            stop_5prime_location->SetInt().SetTo(stop_5prime+2);
                            stop_5prime_location->SetInt().SetStrand(eNa_strand_plus);
                            stop_5prime_location->SetId(*rna_handle.GetSeqId());
                            stop_5prime_feature->SetLocation(*stop_5prime_location);

                            SAnnotSelector sel(CSeq_annot::C_Data::e_Ftable);
                            sel.SetResolveNone();
                            CAnnot_CI it(rna_handle, sel);
                            it->GetEditHandle().AddFeat(*stop_5prime_feature);
                        }
                    }
                }

                /// also copy the code break if it exists
                if (cds_feat->GetData().GetCdregion().IsSetCode_break()) {
                    SMapper mapper(align, *m_scope, 0, opts);
                    mapper.IncludeSourceLocs();
                    mapper.SetMergeNone();
    
                    CSeqFeatData::TCdregion& cds =
                        cds_feat->SetData().SetCdregion();
                    CCdregion::TCode_break::iterator it =
                        cds.SetCode_break().begin();
                    for ( ;  it != cds.SetCode_break().end();  ) {
                        CSeq_loc code_break_loc;
                        code_break_loc.Assign((*it)->GetLoc());
                        code_break_loc.SetId(align.GetSeq_id(0)); // set query mrna id - the mapper maps from query mrna to genome
                        CRef<CSeq_loc> new_cb_loc = mapper.Map(code_break_loc);

                        // we may get an equiv. If we do, get just mapped loc
                        if (new_cb_loc->IsEquiv()) {
                            new_cb_loc = new_cb_loc->GetEquiv().Get().front();
                        }

                        CRangeCollection<TSeqPos> new_cb_ranges;
                        if (new_cb_loc && !new_cb_loc->IsNull()) {
                            ITERATE (CSeq_loc, loc_it, *new_cb_loc) {
                                new_cb_ranges += loc_it.GetRange();
                            }
                            new_cb_ranges &= loc_ranges;
                        }
                        if (new_cb_ranges.GetCoveredLength() == 3) {
                            (*it)->SetLoc(*new_cb_loc);
                            ++it;
                        } else {
                            it = cds.SetCode_break().erase(it);
                        }
                    }
                    if (cds.GetCode_break().empty()) {
                        cds.ResetCode_break();
                    }
                }

            }

    return cds_feat;
}

string CFeatureGenerator::
SImplementation::x_ConstructRnaName(const CBioseq_Handle& handle)
{
    string name;
    if (handle) {
        name = sequence::CDeflineGenerator().GenerateDefline(handle);
        try {
            const COrg_ref &org = sequence::GetOrg_ref(handle);
            if (org.IsSetTaxname() && NStr::StartsWith(name, org.GetTaxname())) {
                name.erase(0, org.GetTaxname().size());
            }
        }
        catch (CException&) {
        }
        NStr::ReplaceInPlace(name, ", nuclear gene encoding mitochondrial protein",
                                   "");
        CFeat_CI feat_iter(handle, CSeqFeatData::eSubtype_gene);
        if (feat_iter && feat_iter.GetSize() &&
            feat_iter->GetData().GetGene().IsSetLocus())
        {
            NStr::ReplaceInPlace(
                name, " (" + feat_iter->GetData().GetGene().GetLocus() + ')', "");
        }
        size_t last_comma = name.rfind(',');
        if (last_comma != string::npos) {
            name.erase(last_comma);
        }
        NStr::TruncateSpacesInPlace(name);
    }
    return name;
}

CRef<CSeq_feat>
CFeatureGenerator::
SImplementation::x_CreateNcRnaFeature(const CSeq_feat* ncrnafeature_on_mrna,
                                      const CSeq_align& align,
                                      CConstRef<CSeq_loc> loc,
                                      CSeq_loc_Mapper::TMapOptions opts)
{
    CRef<CSeq_feat> ncrna_feat;
    if ((m_flags & fPropagateNcrnaFeats) && ncrnafeature_on_mrna != NULL) {

        TSeqPos offset;
        ncrna_feat = x_MapFeature(ncrnafeature_on_mrna,
                                  align, loc, opts, offset);
    }
    return ncrna_feat;
}

CRef<CSeq_feat>
CFeatureGenerator::
SImplementation::x_MapFeature(const objects::CSeq_feat* feature_on_mrna,
                                            const CSeq_align& align,
                                            CConstRef<CSeq_loc> loc,
                                            CSeq_loc_Mapper::TMapOptions opts,
                                            TSeqPos &offset)
{
    // from this point on, we will get complex locations back
    SMapper mapper(align, *m_scope, 0, opts);
    mapper.IncludeSourceLocs();
    mapper.SetMergeNone();
    
    CRef<CSeq_loc> mapped_loc;

    ///
    /// mapping is complex
    /// we try to map each segment of the CDS
    /// in general, there will be only one; there are some corner cases
    /// (OAZ1, OAZ2, PAZ3, PEG10) in which there are more than one
    /// segment.
    ///
    for (CSeq_loc_CI loc_it(feature_on_mrna->GetLocation());
         loc_it;  ++loc_it) {
        /// location for this interval
        CConstRef<CSeq_loc> this_loc = loc_it.GetRangeAsSeq_loc();

        /// map it
        CRef<CSeq_loc> equiv = mapper.Map(*this_loc);
        if ( !equiv  ||
             equiv->IsNull()  ||
             equiv->IsEmpty() ) {
            continue;
        }

        /// we are using a special variety that will tell us what
        /// portion really mapped
        ///
        /// the first part is the mapped location

        if (equiv->GetEquiv().Get().size() != 2) {
            NCBI_THROW(CException, eUnknown,
                       "failed to find requisite parts of "
                       "mapped seq-loc");
        }
        CRef<CSeq_loc> this_loc_mapped =
            equiv->GetEquiv().Get().front();
        if ( !this_loc_mapped  ||
             this_loc_mapped->IsNull()  ||
             this_loc_mapped->IsEmpty() ) {
            continue;
        }

        if ( !mapped_loc ) {
            mapped_loc.Reset(new CSeq_loc);
            /// This is start of mapped location; record offset
            offset = equiv->GetEquiv().Get().back()->GetTotalRange().GetFrom() -
                     feature_on_mrna->GetLocation().GetTotalRange().GetFrom();
        }

        /// we take the extreme bounds of the interval only;
        /// internal details will be recomputed based on intersection
        /// with the mRNA location
        ENa_strand strand = this_loc_mapped->GetStrand();
        CSeq_loc sub;
        sub.SetInt().SetFrom(this_loc_mapped->GetTotalRange().GetFrom());
        sub.SetInt().SetTo(this_loc_mapped->GetTotalRange().GetTo());
        sub.SetInt().SetStrand(loc->GetStrand());
        sub.SetInt().SetId().Assign(*this_loc_mapped->GetId());

        bool is_partial_5prime =
            this_loc_mapped->IsPartialStart(eExtreme_Biological);
        bool is_partial_3prime =
            this_loc_mapped->IsPartialStop(eExtreme_Biological);

        this_loc_mapped = loc->Intersect(sub,
                                         CSeq_loc::fSort,
                                         NULL);
        this_loc_mapped->SetStrand(strand);

        if (this_loc_mapped->IsMix()) {
            /// Propagate any internal fuzzy boundaries on the mRNA to the CDS
            set<TSeqPos> mrna_fuzzy_boundaries;
            ITERATE (CSeq_loc, subloc_it, *loc) {
                if (subloc_it.GetRangeAsSeq_loc()->
                        IsPartialStart(eExtreme_Positional))
                {
                    mrna_fuzzy_boundaries.insert(
                        subloc_it.GetRange().GetFrom());
                }
                if (subloc_it.GetRangeAsSeq_loc()->
                        IsPartialStop(eExtreme_Positional))
                {
                    mrna_fuzzy_boundaries.insert(
                        subloc_it.GetRange().GetTo());
                }
            }
    
            NON_CONST_ITERATE (CSeq_loc_mix::Tdata, subloc_it,
                               this_loc_mapped->SetMix().Set())
            {
                (*subloc_it)->SetPartialStart(
                     mrna_fuzzy_boundaries.count(
                         (*subloc_it)->GetStart(eExtreme_Positional)),
                     eExtreme_Positional);
                (*subloc_it)->SetPartialStop(
                     mrna_fuzzy_boundaries.count(
                         (*subloc_it)->GetStop(eExtreme_Positional)),
                     eExtreme_Positional);
            }
        }

        this_loc_mapped->SetPartialStart(is_partial_5prime, eExtreme_Biological);
        this_loc_mapped->SetPartialStop(is_partial_3prime, eExtreme_Biological);

        mapped_loc->SetMix().Set().push_back(this_loc_mapped);
    }
    if (mapped_loc) {
        mapped_loc->ChangeToPackedInt();
        mapped_loc->SetId(*loc->GetId());
    }

    if (loc->GetStart(eExtreme_Positional) > loc->GetStop(eExtreme_Positional)) {
        mapped_loc = FixOrderOfCrossTheOriginSeqloc(*mapped_loc,
                                                    (loc->GetStart(eExtreme_Positional) + loc->GetStop(eExtreme_Positional))/2);
    }

    if (mapped_loc && feature_on_mrna->GetData().IsRna())
    {
        if (mapped_loc->IsPartialStop(eExtreme_Biological) &&
            !feature_on_mrna->GetLocation().IsPartialStop(eExtreme_Biological) &&
            align.GetSegs().IsSpliced() &&
            align.GetSegs().GetSpliced().CanGetPoly_a())
        {
            /// When propagaring RNA feature, don't create fuzz at 3' end if
            /// alignment has poly-a flag
            mapped_loc->SetPartialStop(false, eExtreme_Biological);
        }
        if ((mapped_loc->IsPartialStart(eExtreme_Biological) &&
             !feature_on_mrna->GetLocation().IsPartialStart(eExtreme_Biological)) ||
            (mapped_loc->IsPartialStop(eExtreme_Biological) &&
             !feature_on_mrna->GetLocation().IsPartialStop(eExtreme_Biological)))
        {
            CSeq_loc_Mapper reverse_mapper(align, 0, m_scope.GetPointer(), opts);
            CSeq_id &mapped_loc_id = const_cast<CSeq_id &>(*mapped_loc->GetId());
            TSignedSeqPos feat_start = feature_on_mrna->GetLocation().GetStart(eExtreme_Biological);
            CSeq_loc start_loc(mapped_loc_id, mapped_loc->GetStart(eExtreme_Biological));
            TSignedSeqPos mapped_start = reverse_mapper.Map(start_loc)->GetStart(eExtreme_Biological);
            if (!feature_on_mrna->GetLocation().IsPartialStart(eExtreme_Biological) &&
                TSeqPos(abs(feat_start - mapped_start)) <= m_allowed_unaligned)
            {
                /// No fuzz in original, and overhang is within limits; shouldn't have fuzz
                mapped_loc->SetPartialStart(false, eExtreme_Biological);
            }
            TSignedSeqPos feat_stop = feature_on_mrna->GetLocation().GetStop(eExtreme_Biological);
            CSeq_loc stop_loc(mapped_loc_id, mapped_loc->GetStop(eExtreme_Biological));
            TSignedSeqPos mapped_stop = reverse_mapper.Map(stop_loc)->GetStop(eExtreme_Biological);
            if (!feature_on_mrna->GetLocation().IsPartialStop(eExtreme_Biological) &&
                TSeqPos(abs(feat_stop - mapped_stop)) <= m_allowed_unaligned)
            {
                /// No fuzz in original, and overhang is within limits; shouldn't have fuzz
                mapped_loc->SetPartialStop(false, eExtreme_Biological);
            }
        }
    }

    if (mapped_loc && feature_on_mrna->GetData().IsCdregion()) {
        /// For CDS features, trip not to begin/end in gaps
        /// Trim beginning
        CSeqVector vec(*mapped_loc, *m_scope);
        TSeqPos start_gap = 0;
        for (; vec.IsInGap(start_gap); ++start_gap);
        if (start_gap > 0) {
            offset += start_gap;
            while (mapped_loc->SetPacked_int().Set().front()->GetLength()
                        <= start_gap)
            {
                start_gap -= mapped_loc->SetPacked_int().Set().front()->GetLength();
                mapped_loc->SetPacked_int().Set().pop_front();
            }
            if (start_gap) {
                CSeq_interval &first_exon =
                    *mapped_loc->SetPacked_int().Set().front();
                if (first_exon.GetStrand() == eNa_strand_minus) {
                    first_exon.SetTo() -= start_gap;
                } else {
                    first_exon.SetFrom() += start_gap;
                }
            }
            mapped_loc->SetPartialStart(true, eExtreme_Biological);
        }
        TSeqPos end_gap = 0;
        for (; vec.IsInGap(vec.size() - 1 - end_gap); ++end_gap);
        if (end_gap > 0) {
            while (mapped_loc->SetPacked_int().Set().back()->GetLength() <= end_gap)
            {
                end_gap -= mapped_loc->SetPacked_int().Set().back()->GetLength();
                mapped_loc->SetPacked_int().Set().pop_back();
            }
            if (end_gap) {
                CSeq_interval &last_exon =
                    *mapped_loc->SetPacked_int().Set().back();
                if (last_exon.GetStrand() == eNa_strand_minus) {
                    last_exon.SetFrom() += end_gap;
                } else {
                    last_exon.SetTo() -= end_gap;
                }
            }
            mapped_loc->SetPartialStop(true, eExtreme_Biological);
        }
    }

    CRef<CSeq_feat> mapped_feat;
    if (mapped_loc  && mapped_loc->Which() != CSeq_loc::e_not_set) {
        mapped_feat.Reset(new CSeq_feat());
        mapped_feat->Assign(*feature_on_mrna);
        mapped_feat->ResetId();
        
        mapped_feat->SetLocation(*mapped_loc);
    }
    return mapped_feat;
}

void CFeatureGenerator::
SImplementation::SetPartialFlags(CRef<CSeq_feat> gene_feat,
                                 CRef<CSeq_feat> mrna_feat,
                                 CRef<CSeq_feat> propagated_feat)
{
    if(propagated_feat){
        for (CSeq_loc_CI loc_it(propagated_feat->GetLocation());  loc_it;  ++loc_it) {
            if (loc_it.GetRangeAsSeq_loc()->IsPartialStart(eExtreme_Biological)  ||  loc_it.GetRangeAsSeq_loc()->IsPartialStop(eExtreme_Biological)) {
                propagated_feat->SetPartial(true);
                if(gene_feat)
                    gene_feat->SetPartial(true);
                break;
            }
        }
    }

    ///
    /// partial flags may require a global analysis - we may need to mark some
    /// locations partial even if they are not yet partial
    ///
    if (mrna_feat  &&  propagated_feat)
    {
        /// in addition to marking the mrna feature partial, we must mark the
        /// location partial to match the partialness in the CDS
        CSeq_loc& propagated_feat_loc = propagated_feat->SetLocation();
        CSeq_loc& mrna_loc = mrna_feat->SetLocation();
        if (propagated_feat_loc.IsPartialStart(eExtreme_Biological) &&
            propagated_feat_loc.GetStart(eExtreme_Biological) ==
            mrna_loc.GetStart(eExtreme_Biological)) {
            mrna_loc.SetPartialStart(true, eExtreme_Biological);
        }
        if (propagated_feat_loc.IsPartialStop(eExtreme_Biological) &&
            propagated_feat_loc.GetStop(eExtreme_Biological) ==
            mrna_loc.GetStop(eExtreme_Biological)) {
            mrna_loc.SetPartialStop(true, eExtreme_Biological);
        }
    }

    /// set the partial flag for mrna_feat if it has any fuzzy intervals
    if(mrna_feat){
        for (CSeq_loc_CI loc_it(mrna_feat->GetLocation());  loc_it;  ++loc_it) {
            if (loc_it.GetRangeAsSeq_loc()->IsPartialStart(eExtreme_Biological)  ||  loc_it.GetRangeAsSeq_loc()->IsPartialStop(eExtreme_Biological)) {
                mrna_feat->SetPartial(true);
                if(gene_feat)
                    gene_feat->SetPartial(true);
                break;
            }
        }
    }

    ///
    /// set gene partialness if mRNA is partial
    if (gene_feat  &&  mrna_feat){
        CSeq_loc& mrna_loc = mrna_feat->SetLocation();
        CSeq_loc& gene_loc = gene_feat->SetLocation();
        if (mrna_loc.IsPartialStart(eExtreme_Biological)) {
            gene_loc.SetPartialStart(true, eExtreme_Biological);
        }
        if (mrna_loc.IsPartialStop(eExtreme_Biological)) {
            gene_loc.SetPartialStop(true, eExtreme_Biological);
        }
    }

    ///
    /// set gene partialness if CDS is partial
    if (gene_feat  &&  propagated_feat){
        CSeq_loc& propagated_loc = propagated_feat->SetLocation();
        CSeq_loc& gene_loc = gene_feat->SetLocation();
        if (propagated_loc.IsPartialStart(eExtreme_Biological)) {
            gene_loc.SetPartialStart(true, eExtreme_Biological);
        }
        if (propagated_loc.IsPartialStop(eExtreme_Biological)) {
            gene_loc.SetPartialStop(true, eExtreme_Biological);
        }
    }
}

/// Check whether range1 contains range2
static inline bool s_Contains(const TSeqRange &range1, const TSeqRange &range2)
{
    return range1.GetFrom() <= range2.GetFrom() &&
           range1.GetTo() >= range2.GetTo();
}

void CFeatureGenerator::
SImplementation::RecomputePartialFlags(CSeq_annot& annot)
{
    CScope scope(*CObjectManager::GetInstance());
    CSeq_annot_Handle sah = scope.AddSeq_annot(annot);

    /// We're going to recalculate Partial flags for all features,
    /// and fuzzy ends for gene features; reset them if they're currently set
    for(CFeat_CI ci(sah); ci; ++ci){
        CSeq_feat_EditHandle handle(*ci);
        CRef<CSeq_feat> feat(const_cast<CSeq_feat*>(handle.GetSeq_feat().GetPointer()));
        feat->ResetPartial();
        if(feat->GetData().IsGene()){
            feat->SetLocation().SetPartialStart(false, eExtreme_Biological);
            feat->SetLocation().SetPartialStop(false, eExtreme_Biological);
        }
    }

    feature::CFeatTree tree(sah);
    vector<CMappedFeat> top_level_features = tree.GetChildren(CMappedFeat());

    /// Sort top features (i.e. Seq_feat objects with no parent) by type
    vector< vector<CMappedFeat> > top_level_features_by_type;
    top_level_features_by_type.resize(CSeqFeatData::e_MaxChoice);

    ITERATE(vector<CMappedFeat>, it, top_level_features)
        top_level_features_by_type[it->GetData().Which()].push_back(*it);

    /// Add null gene and rna features; this makes the programming easier for
    /// dealing with top-level rnas and top-level cd regions
    top_level_features_by_type[CSeqFeatData::e_Gene].push_back(CMappedFeat());
    top_level_features_by_type[CSeqFeatData::e_Rna].push_back(CMappedFeat());

    ITERATE(vector<CMappedFeat>, gene_it,
            top_level_features_by_type[CSeqFeatData::e_Gene])
    {
        CRef<CSeq_feat> gene_feat;
        if(*gene_it){
            CSeq_feat_EditHandle gene_handle(*gene_it);
            gene_feat.Reset(const_cast<CSeq_feat*>(gene_handle.GetSeq_feat().GetPointer()));
        }
        // Get gene's children; or, if we've reached the sentinel null gene
        // feature, get top-level rnas.
        vector<CMappedFeat> gene_children =
                gene_feat ? tree.GetChildren(*gene_it)
                          : top_level_features_by_type[CSeqFeatData::e_Rna];
        sort(gene_children.begin(), gene_children.end());

        ITERATE(vector<CMappedFeat>, child_it, gene_children){
            CRef<CSeq_feat> child_feat;
            if(*child_it){
                CSeq_feat_EditHandle child_handle(*child_it);
                child_feat.Reset(const_cast<CSeq_feat*>(child_handle.GetSeq_feat().GetPointer()));
            }
            if(child_feat && child_feat->GetData().IsCdregion()){
                // We have gene and cds with no RNA feature
                SetPartialFlags(gene_feat, CRef<CSeq_feat>(), child_feat);
            } else if(!child_feat || child_feat->GetData().IsRna()){
                vector<CMappedFeat> rna_children =
                    child_feat ? tree.GetChildren(*child_it)
                               : top_level_features_by_type[CSeqFeatData::e_Cdregion];
                /// When propagating a ncRNA feature, the propagated feature will have a range
                /// contained within the range of the newly-created RNA feature, and should be
                /// treated as its child. Unfortunately the logic of CFeatTree does not recognize
                /// one RNA feature as a child of the other, so we need to do that manually.
                while((child_it+1) != gene_children.end() &&
                      (child_it+1)->GetData().GetSubtype() == CSeqFeatData::eSubtype_ncRNA &&
                      s_Contains(child_feat->GetLocation().GetTotalRange(),
                                 (child_it+1)->GetTotalRange())){
                    rna_children.push_back(*(++child_it));
                }
                if(rna_children.empty()){
                    // We have gene and RNA with no cds feature
                    SetPartialFlags(gene_feat, child_feat, CRef<CSeq_feat>());
                } else {
                    ITERATE(vector<CMappedFeat>, rna_child_it, rna_children){
                        CRef<CSeq_feat> rna_child_feat;
                        CSeq_feat_EditHandle rna_child_handle(*rna_child_it);
                        rna_child_feat.Reset(const_cast<CSeq_feat*>(rna_child_handle.GetSeq_feat().GetPointer()));
                        SetPartialFlags(gene_feat, child_feat, rna_child_feat);
                    }
                }
            }
        }
    }
}


void CFeatureGenerator::SImplementation::x_CheckInconsistentDbxrefs(
                       CConstRef<CSeq_feat> gene_feat,
                       CConstRef<CSeq_feat> propagated_feature)
{
    if(!gene_feat || !gene_feat->IsSetDbxref() ||
       !propagated_feature || !propagated_feature->IsSetDbxref())
        return;

    ITERATE (CSeq_feat::TDbxref, gene_xref_it, gene_feat->GetDbxref())
    /// Special case for miRBase; the gene feature and propagated ncRNA features can
    /// legitimately have different tags for it
    if((*gene_xref_it)->GetDb() != "miRBase")
        ITERATE (CSeq_feat::TDbxref, propagated_xref_it, propagated_feature->GetDbxref())
            if((*gene_xref_it)->GetDb() == (*propagated_xref_it)->GetDb() &&
               !(*gene_xref_it)->Match(**propagated_xref_it))
            {
                string propagated_feature_desc;
                if(propagated_feature->GetData().IsCdregion())
                    propagated_feature_desc = "corresponding cdregion";
                else {
                    NCBI_ASSERT(propagated_feature->GetData().GetSubtype() == CSeqFeatData::eSubtype_ncRNA,
                                "Unexpected propagated feature type");
                    propagated_feature_desc = "propagated ncRNA feature";
                }
                if(propagated_feature->CanGetProduct())
                    propagated_feature_desc += " " + propagated_feature->GetProduct().GetId()->AsFastaString();
                LOG_POST(Warning << "Features for gene "
                                 << gene_feat->GetLocation().GetId()->AsFastaString()
                                 << " and " << propagated_feature_desc
                                 << " have " << (*gene_xref_it)->GetDb()
                                 << " dbxrefs with inconsistent tags");
            }
}



void CFeatureGenerator::SImplementation::x_CopyAdditionalFeatures(const CBioseq_Handle& handle, SMapper& mapper, CSeq_annot& annot)
{
    if (m_flags & fPromoteAllFeatures) {
        SAnnotSelector sel;
        sel.SetResolveAll()
            .SetAdaptiveDepth(true)
            .ExcludeFeatSubtype(CSeqFeatData::eSubtype_gene)
            .ExcludeFeatSubtype(CSeqFeatData::eSubtype_mRNA)
            .ExcludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
        for (CFeat_CI feat_iter(handle, sel);  feat_iter;  ++feat_iter) {
            CRef<CSeq_feat> feat(new CSeq_feat());
            feat->Assign(feat_iter->GetOriginalFeature());
            CRef<CSeq_loc> new_loc =
                mapper.Map(feat_iter->GetLocation());
            feat->SetLocation(*new_loc);

            _ASSERT(feat->GetData().Which() != CSeqFeatData::e_not_set);

            annot.SetData().SetFtable().push_back(feat);
        }
    }
}



//////////////////////////////////////////////////////////////////////////////


///
/// Handle feature exceptions
///
void CFeatureGenerator::SImplementation::x_HandleRnaExceptions(CSeq_feat& feat,
                                  const CSeq_align* align,
                                  CSeq_feat* cds_feat,
                                  const CSeq_feat* cds_feat_on_mrna)
{
    if ( !feat.IsSetProduct() ) {
        ///
        /// corner case:
        /// we may be a CDS feature for an Ig locus
        /// check to see if we have an overlapping V/C/D/J/S region
        /// we trust only featu-id xrefs here
        ///
        if (feat.IsSetXref()) {
            CBioseq_Handle bsh = m_scope->GetBioseqHandle(*feat.GetLocation().GetId());
            const CTSE_Handle& tse = bsh.GetTSE_Handle();

            ITERATE (CSeq_feat::TXref, it, feat.GetXref()) {
                if ( !(*it)->IsSetId() ) {
                    continue;
                }

                CSeq_feat_Handle h;
                const CFeat_id& feat_id = (*it)->GetId();
                if (feat_id.IsLocal()) {
                    if (feat_id.GetLocal().IsId()) {
                        h = tse.GetFeatureWithId(CSeqFeatData::e_not_set,
                                                 feat_id.GetLocal().GetId());
                    } else {
                        h = tse.GetFeatureWithId(CSeqFeatData::e_not_set,
                                                 feat_id.GetLocal().GetStr());
                    }
                }

                if (h) {
                    switch (h.GetData().GetSubtype()) {
                    case CSeqFeatData::eSubtype_C_region:
                    case CSeqFeatData::eSubtype_D_loop:
                    case CSeqFeatData::eSubtype_D_segment:
                    case CSeqFeatData::eSubtype_J_segment:
                    case CSeqFeatData::eSubtype_S_region:
                    case CSeqFeatData::eSubtype_V_region:
                    case CSeqFeatData::eSubtype_V_segment:
                        /// found it
                        feat.SetExcept(true);
                        feat.SetExcept_text
                            ("rearrangement required for product");
                        break;

                    default:
                        break;
                    }
                }
            }
        }
        return;
    }

    ///
    /// check to see if there is a Spliced-seg alignment
    /// if there is, and it corresponds to this feature, we should use it to
    /// record our exceptions
    ///

    CConstRef<CSeq_align> al;
    if (align  &&  align->GetSegs().IsSpliced()) {
        al.Reset(align);
    }
    if ( !al ) {
        SAnnotSelector sel;
        sel.SetResolveAll();
        CAlign_CI align_iter(*m_scope, feat.GetLocation(), sel);
        for ( ;  align_iter;  ++align_iter) {
            const CSeq_align& this_align = *align_iter;
            if (this_align.GetSegs().IsSpliced()  &&
                sequence::IsSameBioseq
                (sequence::GetId(feat.GetProduct(), m_scope.GetPointer()),
                 this_align.GetSeq_id(0),
                 m_scope.GetPointer())) {
                al.Reset(&this_align);
                break;
            }
        }
    }

    bool has_length_mismatch = false;
    //bool has_polya_tail = false;
    bool has_incomplete_polya_tail = false;
    bool partial_unaligned_section = false;
    CRangeCollection<TSeqPos> mismatch_locs;
    CRangeCollection<TSeqPos> insert_locs;
    CRangeCollection<TSeqPos> delete_locs;
    map<TSeqPos,TSeqPos> delete_sizes;

    CBioseq_Handle prod_bsh    = m_scope->GetBioseqHandle(*feat.GetProduct().GetId());
    if ( !prod_bsh ) {
        /// Product doesn't exist (will happen for fake transcript when handling
        /// protein alignments); no basis for creating exceptions
        return;
    }

    TSeqPos loc_len = sequence::GetLength(feat.GetLocation(), m_scope.GetPointer());
    if (loc_len > prod_bsh.GetBioseqLength()) {
        has_length_mismatch = true;
    }

    if (al && al->GetSegs().GetSpliced().GetProduct_type()==CSpliced_seg::eProduct_type_transcript) {
        ///
        /// can do full comparison
        ///

        /// we know we have a Spliced-seg
        /// evaluate for gaps or mismatches
        TSeqPos prev_to = 0;
        ITERATE (CSpliced_seg::TExons, exon_it,
                 al->GetSegs().GetSpliced().GetExons()) {
            const CSpliced_exon& exon = **exon_it;
            TSeqPos pos = exon.GetProduct_start().GetNucpos();
            if (exon_it != al->GetSegs().GetSpliced().GetExons().begin()) {
                TSeqRange gap(prev_to+1, pos-1);
                if (gap.NotEmpty()) {
                    if (feat.IsSetPartial()) {
                        partial_unaligned_section = true;
                    } else {
                        insert_locs += gap;
                    }
                }
            }
            prev_to = exon.GetProduct_end().GetNucpos();
            if (exon.IsSetParts()) {
                ITERATE (CSpliced_exon::TParts, part_it, exon.GetParts()) {
                    switch ((*part_it)->Which()) {
                    case CSpliced_exon_chunk::e_Match:
                        pos += (*part_it)->GetMatch();
                        break;
                    case CSpliced_exon_chunk::e_Mismatch:
                        mismatch_locs +=
                            TSeqRange(pos, pos+(*part_it)->GetMismatch()-1);
                        pos += (*part_it)->GetMismatch();
                        break;
                    case CSpliced_exon_chunk::e_Diag:
                        pos += (*part_it)->GetDiag();
                        break;
                    case CSpliced_exon_chunk::e_Genomic_ins:
                        delete_locs += TSeqRange(pos, pos);
                        delete_sizes[pos] = (*part_it)->GetGenomic_ins();
                        break;
                    case CSpliced_exon_chunk::e_Product_ins:
                        insert_locs +=
                            TSeqRange(pos, pos+(*part_it)->GetProduct_ins()-1);
                        pos += (*part_it)->GetProduct_ins();
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        /// Check against aligned range - see if there is a 5' or 3'
        /// discrepancy
        TSeqRange r = al->GetSeqRange(0);
        if (r.GetFrom() != 0) {
            if (feat.IsSetPartial()) {
                partial_unaligned_section = true;
            } else {
                insert_locs += TSeqRange(0, r.GetFrom()-1);
            }
        }

        TSeqPos max_align_len = 0;
        if (al->GetSegs().GetSpliced().IsSetPoly_a()) {
            //has_polya_tail = true;
            max_align_len = al->GetSegs().GetSpliced().GetPoly_a();
        } else if (al->GetSegs().GetSpliced().IsSetProduct_length()) {
            max_align_len = al->GetSegs().GetSpliced().GetProduct_length();
        } else {
            max_align_len = prod_bsh.GetBioseqLength();
        }

        if (r.GetTo() + 1 < max_align_len) {
            if (feat.IsSetPartial()) {
                partial_unaligned_section = true;
            } else {
                insert_locs += TSeqRange(r.GetTo()+1, max_align_len-1);
            }
        }

        /// also note the poly-A
        /**
        if (al->GetSegs().GetSpliced().IsSetPoly_a()) {
            has_polya_tail = true;
        }
        **/
    }

    if ( insert_locs.empty() && delete_locs.empty() && !partial_unaligned_section)
    {
        /// only compare for mismatches and 3' unaligned
        /// we assume that the feature is otherwise aligned

        CSeqVector nuc_vec(feat.GetLocation(), *m_scope,
                           CBioseq_Handle::eCoding_Iupac);

        CSeqVector rna_vec(prod_bsh,
                           CBioseq_Handle::eCoding_Iupac);

        CSeqVector::const_iterator prod_it  = rna_vec.begin();
        CSeqVector::const_iterator prod_end = rna_vec.end();

        CSeqVector::const_iterator genomic_it  = nuc_vec.begin();
        CSeqVector::const_iterator genomic_end = nuc_vec.end();
        mismatch_locs.clear();

        for ( ;  prod_it != prod_end  &&  genomic_it != genomic_end;
              ++prod_it, ++genomic_it) {
            if (*prod_it != *genomic_it) {
                mismatch_locs += TSeqRange(prod_it.GetPos(), prod_it.GetPos());
            }
        }

        size_t tail_len = prod_end - prod_it;
        size_t count_a = 0;
        for ( ;  prod_it != prod_end;  ++prod_it) {
            if (*prod_it == 'A') {
                ++count_a;
            }
        }

        if (tail_len  &&  count_a >= tail_len * 0.8) {
            //has_polya_tail = true;
            if (count_a < tail_len * 0.95) {
                has_incomplete_polya_tail = true;
            }
        }
        else if (tail_len) {
            if (feat.IsSetPartial()) {
                partial_unaligned_section = true;
            } else {
                TSeqPos end_pos = feat.GetLocation().GetTotalRange().GetTo();
                insert_locs += TSeqRange(end_pos-tail_len+1, end_pos);
            }
        }
    }

    string except_text;
    if (!insert_locs.empty() ||
        !delete_locs.empty() ||
        has_length_mismatch  ||
        has_incomplete_polya_tail ||
        partial_unaligned_section) {
        except_text = "unclassified transcription discrepancy";
    }
    else if (!mismatch_locs.empty()) {
        except_text = "mismatches in transcription";
    }

    x_SetExceptText(feat, except_text);
    x_SetComment(feat, cds_feat, cds_feat_on_mrna, align, mismatch_locs,
                 insert_locs, delete_locs, delete_sizes,
                 partial_unaligned_section);
}


void CFeatureGenerator::SImplementation::x_HandleCdsExceptions(CSeq_feat& feat,
                                  const CSeq_align* align,
                                  const CSeq_feat* cds_feat_on_query_mrna,
                                  const CSeq_feat* cds_feat_on_transcribed_mrna,
                                  list<CRef<CSeq_loc> >* transcribed_mrna_seqloc_refs,
                                  TSeqPos *clean_match_count)
{
    if ( !feat.IsSetProduct()
         || ( cds_feat_on_query_mrna && !cds_feat_on_query_mrna->IsSetProduct() )
         ) {
        ///
        /// corner case:
        /// we may be a CDS feature for an Ig locus
        /// check to see if we have an overlapping V/C/D/J/S region
        /// we trust only featu-id xrefs here
        ///
        if (feat.IsSetXref()) {
            CBioseq_Handle bsh = m_scope->GetBioseqHandle(*feat.GetLocation().GetId());
            const CTSE_Handle& tse = bsh.GetTSE_Handle();

            ITERATE (CSeq_feat::TXref, it, feat.GetXref()) {
                if ( !(*it)->IsSetId() ) {
                    continue;
                }

                CSeq_feat_Handle h;
                const CFeat_id& feat_id = (*it)->GetId();
                if (feat_id.IsLocal()) {
                    if (feat_id.GetLocal().IsId()) {
                        h = tse.GetFeatureWithId(CSeqFeatData::e_not_set,
                                                 feat_id.GetLocal().GetId());
                    } else {
                        h = tse.GetFeatureWithId(CSeqFeatData::e_not_set,
                                                 feat_id.GetLocal().GetStr());
                    }
                }

                if (h) {
                    switch (h.GetData().GetSubtype()) {
                    case CSeqFeatData::eSubtype_C_region:
                    case CSeqFeatData::eSubtype_D_loop:
                    case CSeqFeatData::eSubtype_D_segment:
                    case CSeqFeatData::eSubtype_J_segment:
                    case CSeqFeatData::eSubtype_S_region:
                    case CSeqFeatData::eSubtype_V_region:
                    case CSeqFeatData::eSubtype_V_segment:
                        /// found it
                        feat.SetExcept(true);
                        feat.SetExcept_text
                            ("rearrangement required for product");
                        break;

                    default:
                        break;
                    }
                }
            }
        }
        return;
    }

    ///
    /// exceptions here are easy:
    /// we compare the annotated product to the conceptual translation and
    /// report problems
    ///
    bool has_start         = false;
    bool has_stop          = false;
    TSeqPos mismatch_count = 0;
    bool has_gap           = false;
    bool has_indel         = false;

    string xlate;

    CRef<CSeq_feat> corrected_cds_feat_on_query_mrna;
    CRef<CSeq_feat> corrected_cds_feat_on_transcribed_mrna;
    if (cds_feat_on_query_mrna) {
        /// In some cases, the id in the CDS feature is not the same as in the
        /// alignment; make sure the mapping is done with matching ids

        corrected_cds_feat_on_query_mrna.Reset(new CSeq_feat);
        corrected_cds_feat_on_query_mrna->Assign(*cds_feat_on_query_mrna);
        corrected_cds_feat_on_query_mrna->SetLocation().SetId(align->GetSeq_id(0));

        corrected_cds_feat_on_transcribed_mrna.Reset(new CSeq_feat);
        corrected_cds_feat_on_transcribed_mrna->Assign(*cds_feat_on_transcribed_mrna);
        corrected_cds_feat_on_transcribed_mrna->SetLocation().SetId(align->GetSeq_id(0));
    }

    int cds_start_on_mrna = 0;
    int frame_on_mrna = 0;

    if (align != NULL) {
        CBioseq bioseq;
        x_CollectMrnaSequence(bioseq.SetInst(), *align, feat.GetLocation(), true, true, &has_gap, &has_indel);
        CSeqVector seq(bioseq, m_scope.GetPointer(),
                       CBioseq_Handle::eCoding_Iupac);

        int cds_len_on_query_mrna = GetLength(feat.GetLocation(), NULL);
        if (cds_feat_on_query_mrna) {
            cds_start_on_mrna
                = cds_feat_on_query_mrna->GetLocation().GetStrand() != eNa_strand_minus
                ? cds_feat_on_query_mrna->GetLocation().GetStart(eExtreme_Biological)
                : cds_feat_on_query_mrna->GetLocation().GetStop(eExtreme_Biological);
            cds_len_on_query_mrna = GetLength(cds_feat_on_query_mrna->GetLocation(), NULL);

            if (cds_feat_on_query_mrna->GetData().GetCdregion().IsSetFrame()) {
                switch (cds_feat_on_query_mrna->GetData().GetCdregion().GetFrame()) {
                case CCdregion::eFrame_two :
                    frame_on_mrna = 1;
                    break;
                case CCdregion::eFrame_three :
                    frame_on_mrna = 2;
                    break;
                default :
                    break;
                }
            }
        }
        string mrna;
        seq.GetSeqData(cds_start_on_mrna + frame_on_mrna, cds_start_on_mrna + cds_len_on_query_mrna, mrna);

        const CGenetic_code *code = NULL;
        if (feat.GetData().GetCdregion().IsSetCode()) {
            code = &feat.GetData().GetCdregion().GetCode();
        }
        bool partial_start = feat.GetLocation().IsPartialStart(eExtreme_Biological);
        CSeqTranslator::Translate(mrna, xlate,
                                  partial_start
                                  ? CSeqTranslator::fIs5PrimePartial
                                  : CSeqTranslator::fDefault,
                                  code);
        if (xlate.size() && xlate[0] == '-') {
            /// First codon couldn't be translated as initial codon; translate
            /// as mid-sequence codon instead
            string first_codon = mrna.substr(0,3);
            string first_aa;
            CSeqTranslator::Translate(first_codon, first_aa,
                                      CSeqTranslator::fIs5PrimePartial, code);
            xlate[0] = first_aa[0];
        }

        // deal with code breaks
        // NB: these should be folded into the translation machinery instead...
        if (feat.GetData().GetCdregion().IsSetCode_break()) {
            CRef<CSeq_loc_Mapper> genomic_to_mrna;
            if (corrected_cds_feat_on_transcribed_mrna) {
                genomic_to_mrna.Reset(
                                      new CSeq_loc_Mapper(*align, CSeq_loc_Mapper::eSplicedRow_Prod));
            }

            ITERATE (CCdregion::TCode_break, it,
                     feat.GetData().GetCdregion().GetCode_break()) {

                if (!genomic_to_mrna) continue;

                const CSeq_loc& cb_on_genome = (*it)->GetLoc();
                CRef<CSeq_loc> cb_on_mrna = genomic_to_mrna->Map(cb_on_genome);
                if (!cb_on_mrna) continue;

                TSeqRange r = cb_on_mrna->GetTotalRange();
                if (r.GetLength() != 3) {
                    continue;
                }

                int pos = (cb_on_mrna->GetStart(eExtreme_Biological)-(cds_start_on_mrna+frame_on_mrna))/3;

                string src;
                CSeqUtil::ECoding src_coding = CSeqUtil::e_Ncbieaa;

                switch ((*it)->GetAa().Which()) {
                case CCode_break::TAa::e_Ncbieaa:
                    src += (char)(*it)->GetAa().GetNcbieaa();
                    src_coding = CSeqUtil::e_Ncbieaa;
                    break;
                    
                case CCode_break::TAa::e_Ncbistdaa:
                    src += (char)(*it)->GetAa().GetNcbistdaa();
                    src_coding = CSeqUtil::e_Ncbistdaa;
                    break;

                case CCode_break::TAa::e_Ncbi8aa:
                    src += (char)(*it)->GetAa().GetNcbi8aa();
                    src_coding = CSeqUtil::e_Ncbi8aa;
                    break;

                default:
                    break;
                }

                if (src.size()) {
                    string dst;
                    CSeqConvert::Convert(src, src_coding, 0, 1,
                                         dst, CSeqUtil::e_Ncbieaa);
                    xlate[pos] = dst[0];
                }
            }
        }
    } else {
        CSeqTranslator::Translate(feat, *m_scope, xlate);
    }

    CRef<CSeq_loc_Mapper> to_prot;
    CRef<CSeq_loc_Mapper> to_mrna;
    CRef<CSeq_loc_Mapper> to_genomic;
    CRef<CSeq_id> mapped_protein_id;
    if (corrected_cds_feat_on_transcribed_mrna) {
        to_prot.Reset(
            new CSeq_loc_Mapper(*corrected_cds_feat_on_query_mrna,
                                CSeq_loc_Mapper::eLocationToProduct));
        to_mrna.Reset(
            new CSeq_loc_Mapper(*corrected_cds_feat_on_query_mrna,
                                CSeq_loc_Mapper::eProductToLocation));
        to_genomic.Reset(
            new CSeq_loc_Mapper(*align, CSeq_loc_Mapper::eSplicedRow_Gen));
        mapped_protein_id.Reset(new CSeq_id);
        mapped_protein_id->Assign(*corrected_cds_feat_on_query_mrna->GetProduct().GetId());
    }

    CRef<CSeq_id> cds_id(new CSeq_id);
    cds_id->Assign(*feat.GetProduct().GetId());

    string actual;
    CRef<CSeq_loc> whole_product(new CSeq_loc);
    whole_product->SetWhole(*cds_id);
    CSeqVector vec(*whole_product, *m_scope,
                   CBioseq_Handle::eCoding_Iupac);
    CRangeCollection<TSeqPos> product_ranges(TSeqRange::GetWhole());
    if (cds_feat_on_transcribed_mrna) {
        /// make sure we're comparing to aligned part of product

        CSeq_loc cds_feat_on_transcribed_mrna_loc;
        cds_feat_on_transcribed_mrna_loc.Assign(corrected_cds_feat_on_transcribed_mrna->GetLocation());
        if (cds_feat_on_transcribed_mrna_loc.GetStrand() == eNa_strand_minus) {
            cds_feat_on_transcribed_mrna_loc.FlipStrand();
        }

        CRef<CSeq_loc> aligned_range =
            align->CreateRowSeq_loc(0)->Intersect(cds_feat_on_transcribed_mrna_loc, 0, NULL);
        CRef<CSeq_loc> product_loc = to_prot->Map(*aligned_range);
        product_ranges.clear();
        ITERATE (CSeq_loc, loc_it, *product_loc) {
            product_ranges += loc_it.GetRange();
        }

        
        if (!corrected_cds_feat_on_transcribed_mrna->GetLocation().IsPartialStop(eExtreme_Biological) &&
            aligned_range->Intersect(corrected_cds_feat_on_transcribed_mrna->GetLocation(), 0, NULL)->GetStop(eExtreme_Biological) ==
            corrected_cds_feat_on_transcribed_mrna->GetLocation().GetStop(eExtreme_Biological)) {
            // trim off the stop
            product_ranges -= TSeqRange(product_ranges.GetTo(),
                                        product_ranges.GetTo());
        }
    }


    if ((xlate.size() == product_ranges.GetTo()+2  ||
         product_ranges.GetTo() == TSeqRange::GetWholeTo()) &&
        xlate[xlate.size() - 1] == '*')
    { /// strip a terminal stop
        xlate.resize(xlate.size() - 1);
        has_stop = true;
    }
    else if (feat.GetLocation().IsPartialStop(eExtreme_Biological)) {
        has_stop = true;
    } else {
        has_stop = false;
    }

    if ( (product_ranges.GetFrom()==0 && xlate.size()  &&  xlate[0] == 'M')  ||
         feat.GetLocation().IsPartialStart(eExtreme_Biological) ) {
        has_start = true;
    }

    if (product_ranges.Empty()) {
        has_gap = true;
    } else {
        string whole;
	vec.GetSeqData(0, vec.size(), whole);
        if (product_ranges[0].IsWhole()) {
            actual = whole;
        } else {
	    string xlate_trimmed;
            ITERATE (CRangeCollection<TSeqPos>, range_it, product_ranges) {
                if (range_it->GetFrom() + range_it->GetLength() <= whole.size()) {
                    actual += whole.substr(range_it->GetFrom(), range_it->GetLength());
                } else {
                    // product sequence is shorter than aligned ranges
                    // most probably product is partial with 5' and/or 3' ends trimmed
                    // assume it corresponds to aligned portion
                    actual = whole;
                }
                xlate_trimmed += xlate.substr(range_it->GetFrom(), range_it->GetLength());
            }
            xlate = xlate_trimmed;
        }
        if (actual != whole) {
            has_gap = true;
        }
    }

    ///
    /// now, compare the two
    /// we deliberately look for problems here rather than using a direct
    /// string compare
    /// NB: we could actually compare lengths first; a length difference imples
    /// the unclassified translation discrepancy state, but we may expand these
    /// states in the future, so it's better to be explicit about our data
    /// recording first
    ///

    string::const_iterator it1     = actual.begin();
    string::const_iterator it1_end = actual.end();
    string::const_iterator it2     = xlate.begin();
    string::const_iterator it2_end = xlate.end();

    for ( ;  it1 != it1_end  &&  it2 != it2_end;  ++it1, ++it2) {
        CRef<CSeq_loc> mapped;
        if (to_mrna) {
            TSeqPos pos = it1 - actual.begin();
            ITERATE (CRangeCollection<TSeqPos>, range_it, product_ranges) {
                if (range_it->GetLength() > pos) {
                    pos += range_it->GetFrom();
                    break;
                } else {
                    pos -= range_it->GetLength();
                }
            }
            CSeq_loc base_loc(*mapped_protein_id, pos, pos);
            CRef<CSeq_loc> mrna_loc = to_mrna->Map(base_loc);
            mapped = to_genomic->Map(*mrna_loc);
        }
        if (*it2 == '*' && mapped) {
            /// Inte5rnal stop codon; annotate with a code-break instead
            /// of an exception
            TSeqPos mapped_total_bases = 0;
            ITERATE (CSeq_loc, loc_it, *mapped) {
                mapped_total_bases += loc_it.GetRange().GetLength();
            }
            if (mapped_total_bases == 3) {
                if (!mapped->IsInt()) {
                    mapped->ChangeToPackedInt();
                }
                char actual_aa = *it1;
                if (!(m_flags & fTrustProteinSeq)) {
                    actual_aa = 'X';
                    if (*it1 != 'X') {
                        ++mismatch_count;
                    }
                }
                AddCodeBreak(feat, *mapped, actual_aa);
                if (transcribed_mrna_seqloc_refs) {
                    transcribed_mrna_seqloc_refs->push_back(mapped);
                }
            } else {
                has_gap = true;
            }
        } else if (*it2 == '-' || *it2 == '*') {
            has_gap = true;
        } else if (*it1 != *it2) {
            ++mismatch_count;
        } else if (clean_match_count && (!mapped ||
                (mapped->IsInt() && mapped->GetTotalRange().GetLength() == 3)))
        {
            ++*clean_match_count;
        }
    }

    string except_text;

    /// The process for setting the comment in some cases finds indels that our
    /// process here misses, so check the comment to determine if we have indels
    if (feat.IsSetComment() &&
        (feat.GetComment().find("indel") != string::npos ||
         feat.GetComment().find("inserted") != string::npos ||
         feat.GetComment().find("deleted") != string::npos))
    {
        has_indel = true;
    }

    if (actual.size() != xlate.size()  ||
        !has_stop  ||  !has_start  ||
        has_gap || has_indel) {
        except_text = "unclassified translation discrepancy";
    }
    else if (mismatch_count) {
        except_text = "mismatches in translation";
    }

    x_SetExceptText(feat, except_text);
}

void CFeatureGenerator::SImplementation::x_SetExceptText(
      CSeq_feat& feat, const string &text)
{
    string except_text = text;

    list<string> except_toks;
    if (feat.IsSetExcept_text()) {
        NStr::Split(feat.GetExcept_text(), ",", except_toks);

        for (list<string>::iterator it = except_toks.begin();
             it != except_toks.end();  ) {
            NStr::TruncateSpacesInPlace(*it);
            if (it->empty()  ||
                *it == "annotated by transcript or proteomic data" ||
                *it == "unclassified transcription discrepancy"  ||
                *it == "mismatches in transcription" ||
                *it == "unclassified translation discrepancy"  ||
                *it == "mismatches in translation") {
                except_toks.erase(it++);
            }
            else {
                ++it;
            }
        }
    }

    if ( !except_text.empty() ) {
        /// Check whether this is a Refseq product
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(*feat.GetProduct().GetId());
        ITERATE(CBioseq_Handle::TId, it, bsh.GetId())
        if(it->GetSeqId()->IsOther() &&
           it->GetSeqId()->GetOther().GetAccession()[0] == 'N' &&
           string("MRP").find(it->GetSeqId()->GetOther().GetAccession()[1]) != string::npos)
        {
            except_text = "annotated by transcript or proteomic data";

            /// Refseq exception has to be combined with an inference qualifer
            string product_type_string;
            if(feat.GetData().IsCdregion())
                product_type_string = "AA sequence";
            else {
                NCBI_ASSERT(feat.GetData().IsRna(), "Bad feature type");
                product_type_string = "RNA sequence";
                if(feat.GetData().GetRna().CanGetType() &&
                   feat.GetData().GetRna().GetType() == CRNA_ref::eType_mRNA)
                    product_type_string += ", mRNA";
            }
            CRef<CGb_qual> qualifier(new CGb_qual);
            qualifier->SetQual("inference");
            qualifier->SetVal("similar to " + product_type_string + " (same species):RefSeq:" +
                              it->GetSeqId()->GetOther().GetAccession() + '.' +
                              NStr::IntToString(it->GetSeqId()->GetOther().GetVersion()));
            feat.SetQual().push_back(qualifier);
        }

        except_toks.push_back(except_text);
    }
    except_text = NStr::Join(except_toks, ", ");

    if (except_text.empty()) {
        // no exception; set states correctly
        feat.ResetExcept_text();
        feat.ResetExcept();
    } else {
        feat.SetExcept(true);
        feat.SetExcept_text(except_text);
    }
}

void CFeatureGenerator::SImplementation::x_SetQualForGapFilledModel(
      CSeq_feat& feat, CSeq_id_Handle id)
{
    CBioseq_Handle bsh = m_scope->GetBioseqHandle(id);
    CSeq_id_Handle best_id = sequence::GetId(id, *m_scope);

    string product_type_string = "RNA sequence";
    const CMolInfo *mol_info = s_GetMolInfo(bsh);
    if (mol_info && mol_info->CanGetBiomol() &&
        mol_info->GetBiomol() == CMolInfo::eBiomol_mRNA) {
        product_type_string += ", mRNA";
    }

    string db = "INSD";
    if(best_id.GetSeqId()->IsOther() &&
       best_id.GetSeqId()->GetOther().GetAccession()[0] == 'N' &&
       string("MRP").find(best_id.GetSeqId()->GetOther().GetAccession()[1]) != string::npos)
        {
            db = "RefSeq";
        }

    CRef<CGb_qual> qualifier(new CGb_qual);
    qualifier->SetQual("inference");
    qualifier->SetVal("similar to " + product_type_string + " (same species):"+db+":" +
                      best_id.GetSeqId()->GetSeqIdString(true));
    feat.SetQual().push_back(qualifier);

}

void CFeatureGenerator::SImplementation::SetFeatureExceptions(CSeq_feat& feat,
                                      const CSeq_align* align,
                                      CSeq_feat* cds_feat,
                                      const CSeq_feat* cds_feat_on_query_mrna,
                                      const CSeq_feat* cds_feat_on_transcribed_mrna,
                                      list<CRef<CSeq_loc> >* transcribed_mrna_seqloc_refs,
                                      TSeqPos *clean_match_count)
{
    CConstRef<CSeq_align> align_ref;

    if (align && IsProteinAlign(*align)) {
        align_ref.Reset(align);
        CRef<CSeq_feat> fake_cds_feat;
        TransformProteinAlignToTranscript(align_ref, fake_cds_feat);
        align = align_ref.GetPointer();
    }

    // We're going to set the exception and add any needed inference qualifiers,
    // so if there's already an inference qualifer there, remove it.
    if (feat.IsSetQual()) {
        for (CSeq_feat::TQual::iterator it = feat.SetQual().begin();
            it != feat.SetQual().end();  )
        {
            if ((*it)->CanGetQual() && (*it)->GetQual() == "inference") {
                it = feat.SetQual().erase(it);
            }
            else {
                ++it;
            }
        }
        if (feat.GetQual().empty()) {
            feat.ResetQual();
        }
    }

    // Exceptions identified are:
    //
    //   - unclassified transcription discrepancy
    //   - mismatches in transcription
    //   - unclassified translation discrepancy
    //   - mismatches in translation
    switch (feat.GetData().Which()) {
    case CSeqFeatData::e_Rna:
        x_HandleRnaExceptions(feat, align, cds_feat, cds_feat_on_query_mrna);
        break;

    case CSeqFeatData::e_Cdregion:
        x_HandleCdsExceptions(feat, align,
                              cds_feat_on_query_mrna, cds_feat_on_transcribed_mrna,
                              transcribed_mrna_seqloc_refs,
                              clean_match_count);
        break;

    case CSeqFeatData::e_Imp:
        switch (feat.GetData().GetSubtype()) {
        case CSeqFeatData::eSubtype_C_region:
        case CSeqFeatData::eSubtype_D_loop:
        case CSeqFeatData::eSubtype_D_segment:
        case CSeqFeatData::eSubtype_J_segment:
        case CSeqFeatData::eSubtype_S_region:
        case CSeqFeatData::eSubtype_V_region:
        case CSeqFeatData::eSubtype_V_segment:
            x_HandleRnaExceptions(feat, align, NULL, NULL);
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }
}

static string s_Count(unsigned num, const string &item_name)
{
    return NStr::NumericToString(num) + ' ' + item_name + (num == 1 ? "" : "s");
}

void CFeatureGenerator::SImplementation::x_SetComment(CSeq_feat& rna_feat,
                      CSeq_feat *cds_feat,
                      const CSeq_feat *cds_feat_on_mrna,
                      const CSeq_align *align,
                      const CRangeCollection<TSeqPos> &mismatch_locs,
                      const CRangeCollection<TSeqPos> &insert_locs,
                      const CRangeCollection<TSeqPos> &delete_locs,
                      const map<TSeqPos,TSeqPos> &delete_sizes,
                      bool partial_unaligned_section)
{
    if (mismatch_locs.empty() && insert_locs.empty() && delete_locs.empty() &&
        !partial_unaligned_section &&
        !(m_is_gnomon && cds_feat &&
          cds_feat->GetData().GetCdregion().IsSetCode_break()))
    {
        return;
    }
    
    string rna_comment, cds_comment;
    CRangeCollection<TSeqPos> inserts_in_cds, deletes_in_cds, cds_ranges;
    if (cds_feat) {
        CSeq_loc_Mapper to_mrna(*align, CSeq_loc_Mapper::eSplicedRow_Prod);
        for (CSeq_loc_CI loc_it(cds_feat->GetLocation()); loc_it;  ++loc_it) {
            CRef<CSeq_loc> cds_on_mrna = to_mrna.Map(*loc_it.GetRangeAsSeq_loc());
            inserts_in_cds += cds_on_mrna->GetTotalRange();
	    deletes_in_cds += cds_on_mrna->GetTotalRange();
	}
        inserts_in_cds &= insert_locs;
        deletes_in_cds &= delete_locs;
    }
    if (cds_feat_on_mrna) {
        for (CSeq_loc_CI loc_it(cds_feat_on_mrna->GetLocation());
             loc_it;  ++loc_it)
        {
            cds_ranges += loc_it.GetRange();
        }
    }

    if (m_is_best_refseq) {
        size_t indel_count = insert_locs.size() + delete_locs.size();
        unsigned pct_coverage = 100, cds_pct_coverage = 100;
        rna_comment = "The RefSeq transcript";
        if (!mismatch_locs.empty()) {
            rna_comment += " has " +
                s_Count(mismatch_locs.GetCoveredLength(), "substitution");
        }
        if (indel_count) {
            rna_comment += (mismatch_locs.empty() ? " has " : " and ") +
                           s_Count(indel_count, "indel");
        }
        if (partial_unaligned_section) {
            pct_coverage =
                CScoreBuilderBase().GetPercentCoverage(*m_scope, *align);
            cds_pct_coverage =
                CScoreBuilderBase().GetPercentCoverage(*m_scope, *align,
                                                       cds_ranges);
            if (!mismatch_locs.empty() || indel_count) {
                rna_comment += " and";
            }
            rna_comment += " aligns at "
                         + NStr::NumericToString(pct_coverage)
                         + "% coverage";
        }
        if (rna_comment == "The RefSeq transcript") {
            rna_comment.clear();
        } else {
            rna_comment += " compared to this genomic sequence";
        }
        if (cds_feat && cds_feat_on_mrna) {
            size_t cds_indel_count = inserts_in_cds.size()
                                   + deletes_in_cds.size();
            size_t cds_mismatch_count = 0;
            CSeqVector prot(cds_feat->GetProduct(), *m_scope,
                            CBioseq_Handle::eCoding_Iupac);
            const CTrans_table &translate =
                CGen_code_table::GetTransTable(1);
            CSeq_loc_Mapper to_mrna(*cds_feat_on_mrna,
                                    CSeq_loc_Mapper::eProductToLocation);
            CSeq_loc_Mapper to_genomic(
                *align, CSeq_loc_Mapper::eSplicedRow_Gen);
            CRef<CSeq_id> cds_id(new CSeq_id);
            cds_id->Assign(*cds_feat->GetProduct().GetId());
            TSeqPos start_pos =
                cds_feat->GetProduct().GetStart(eExtreme_Positional);
            bool single_interval_product = ++cds_feat->GetProduct().begin()
                                          == cds_feat->GetProduct().end();
            if (!single_interval_product) {
                NCBI_THROW(CException, eUnknown,
                           "product is required to be a single interval");
            }
            for (TSeqPos pos = start_pos; pos < start_pos + prot.size(); ++pos)
            {
                CSeq_loc aa_loc(*cds_id, pos, pos);
                CRef<CSeq_loc> rna_codon = to_mrna.Map(aa_loc);
                CRef<CSeq_loc> genomic_codon = to_genomic.Map(*rna_codon);
                CSeqVector codon(*genomic_codon, *m_scope,
                                 CBioseq_Handle::eCoding_Iupac);
                if (codon.size() == 3) {
                    int state = CTrans_table::SetCodonState(
                                    codon[0], codon[1], codon[2]);
                    char translated_codon = pos == 0
                        ? translate.GetStartResidue(state)
                        : translate.GetCodonResidue(state);
                    if (translated_codon != prot[pos]) {
                        ++cds_mismatch_count;
                    }
                }
            }
            if (cds_mismatch_count || cds_indel_count || cds_pct_coverage < 100)
            {
                cds_comment = "The RefSeq protein";
                if (cds_mismatch_count) {
                    cds_comment += " has "
                                 + s_Count(cds_mismatch_count, "substitution");
                }
                if (cds_indel_count) {
                    cds_comment += (cds_mismatch_count ? " and " : " has ")
                                 + s_Count(cds_indel_count, "indel");
                }
                if (cds_pct_coverage < 100) {
                    if (cds_mismatch_count || cds_indel_count) {
                        cds_comment += " and";
                    }
                    cds_comment += " aligns at "
                                 + NStr::NumericToString(cds_pct_coverage)
                                 + "% coverage";
                    }
                cds_comment += " compared to this genomic sequence";
            }
        }
    } else if (m_is_gnomon) {
        set<TSeqPos> insert_codons, delete_codons;
        TSeqPos inserted_bases = insert_locs.GetCoveredLength(),
                cds_inserted_bases = inserts_in_cds.GetCoveredLength(),
                deleted_bases = 0, cds_deleted_bases = 0,
                code_breaks = 0;
        ITERATE (CRangeCollection<TSeqPos>, delete_it, delete_locs) {
            NCBI_ASSERT(delete_it->GetLength() == 1,
                        "Delete locations should always be one base");
            deleted_bases += delete_sizes.find(delete_it->GetFrom())->second;
        }
        ITERATE (CRangeCollection<TSeqPos>, insert_it, inserts_in_cds) {
            for (TSeqPos pos = insert_it->GetFrom();
                 pos <= insert_it->GetTo(); ++pos)
            {
                insert_codons.insert((pos - cds_ranges.GetFrom()) / 3);
            }
        }
        ITERATE (CRangeCollection<TSeqPos>, delete_it, deletes_in_cds) {
            NCBI_ASSERT(delete_it->GetLength() == 1,
                        "Delete locations should always be one base");
            delete_codons.insert((delete_it->GetFrom() -
                                  cds_ranges.GetFrom()) / 3);
            cds_deleted_bases +=
                delete_sizes.find(delete_it->GetFrom())->second;
        }
        if(cds_feat && cds_feat->GetData().GetCdregion().IsSetCode_break()) {
            code_breaks = cds_feat->GetData().GetCdregion()
                              .GetCode_break().size();
        }
        if (inserted_bases || deleted_bases) {
            rna_comment =
            "The sequence of the model RefSeq transcript was modified relative "
            "to this genomic sequence to represent the inferred complete CDS";
        }
        if (inserted_bases) {
            rna_comment += ": inserted " + s_Count(inserted_bases, "base")
                         + " in " + s_Count(insert_codons.size(), "codon");
        }
        if (deleted_bases) {
            rna_comment += string(NStr::EndsWith(rna_comment,"CDS") ? ":" : ";")
                         + " deleted " + s_Count(deleted_bases, "base")
                         + " in " + s_Count(delete_codons.size(), "codon");
        }
        if (cds_inserted_bases || cds_deleted_bases || code_breaks) {
            cds_comment =
              "The sequence of the model RefSeq protein was modified relative "
              "to this genomic sequence to represent the inferred complete CDS";
        }
        if (cds_inserted_bases) {
            cds_comment += ": inserted " + s_Count(cds_inserted_bases, "base")
                         + " in " + s_Count(insert_codons.size(), "codon");
        }
        if (cds_deleted_bases) {
            cds_comment += string(NStr::EndsWith(cds_comment,"CDS") ? ":" : ";")
                         + " deleted " + s_Count(cds_deleted_bases, "base")
                         + " in " + s_Count(delete_codons.size(), "codon");
        }
        if (code_breaks) {
            cds_comment += string(NStr::EndsWith(cds_comment,"CDS") ? ":" : ";")
                         + " substituted " + s_Count(code_breaks, "base")
                         + " at " + s_Count(code_breaks, "genomic stop codon");
        }
    }

    if (!rna_comment.empty()) {
        if (!rna_feat.IsSetComment()) {
            rna_feat.SetComment(rna_comment);
        /// If comment is already set, check it doesn't already contain our text
        } else if (rna_feat.GetComment().find(rna_comment) == string::npos) {
            rna_feat.SetComment() += "; " + rna_comment;
        }
    }
    if (!cds_comment.empty()) {
        if (!cds_feat->IsSetComment()) {
            cds_feat->SetComment(cds_comment);
        /// If comment is already set, check it doesn't already contain our text
        } else if (cds_feat->GetComment().find(cds_comment) == string::npos) {
            cds_feat->SetComment() += "; " + cds_comment;
        }
    }
}

void CFeatureGenerator::SImplementation::x_SetCommentForGapFilledModel
(CSeq_feat& feat,
 TSeqPos insert_length)
{
    _ASSERT(insert_length > 0);
    string comment = "added " + s_Count(insert_length, "base") + " not found in genome assembly";
    if (!feat.IsSetComment()) {
        feat.SetComment(comment);
        /// If comment is already set, check it doesn't already contain our text
    } else if (feat.GetComment().find(comment) == string::npos) {
        feat.SetComment() += "; " + comment;
    }
}

CRef<CSeq_loc> CFeatureGenerator::SImplementation::MergeSeq_locs(const CSeq_loc* loc1, const CSeq_loc* loc2)
{
    CRef<CSeq_loc> merged_loc;
    
    if (loc1->GetStart(eExtreme_Positional) < loc1->GetStop(eExtreme_Positional) &&
        (loc2==NULL ||
         loc2->GetStart(eExtreme_Positional) < loc2->GetStop(eExtreme_Positional))) {
            
        if (loc2==NULL)
            merged_loc = loc1->Merge(CSeq_loc::fMerge_SingleRange, NULL);
        else
            merged_loc = loc1->Add(*loc2, CSeq_loc::fMerge_SingleRange, NULL);
    } else {
        // cross the origin

        _ASSERT(loc2 == NULL ||
                (loc1->Intersect(*loc2, 0, NULL)->IsNull() == false &&
                 loc1->Intersect(*loc2, 0, NULL)->IsEmpty() == false));
    
        CRef<CSeq_id> id(new CSeq_id);
        id->Assign(*loc1->GetId());
        
        TSeqPos genomic_size = m_scope->GetSequenceLength(*id);
        CRef<CSeq_loc> left_loc(new CSeq_loc(*id, genomic_size-1, genomic_size-1, loc1->GetStrand()));
        CRef<CSeq_loc> right_loc(new CSeq_loc(*id, 0, 0, loc1->GetStrand()));

        merged_loc = left_loc;
        merged_loc->Add(*right_loc);
        merged_loc->Add(*loc1);
        if (loc2 != NULL)
        merged_loc->Add(*loc2);

        TSeqPos x[] = {
            loc1->GetStart(eExtreme_Positional),
            loc1->GetStop(eExtreme_Positional),
            (loc2 ? loc2->GetStart(eExtreme_Positional) : 0),
            (loc2 ? loc2->GetStop(eExtreme_Positional) : 0)
        };

        if (x[0] > x[1])
            x[1] += genomic_size;
        if (x[2] > x[3])
            x[3] += genomic_size;

        if (x[1] < x[2]) {
            x[0] += genomic_size;
            x[1] += genomic_size;
        } else if (x[3] < x[0]) {
            x[2] += genomic_size;
            x[3] += genomic_size;
        }


        x[0] = min(x[0], x[2]);
        x[1] = max(x[1], x[3]) - genomic_size;
        _ASSERT( x[0] > x[1] +1 );

        merged_loc = FixOrderOfCrossTheOriginSeqloc(*merged_loc,
                                                    (x[0]+x[1])/2,
                                                    CSeq_loc::fMerge_SingleRange);
    }
    return merged_loc;
}

CRef<CSeq_loc> CFeatureGenerator::SImplementation::FixOrderOfCrossTheOriginSeqloc
(const CSeq_loc& loc,
 TSeqPos outside_point,
 CSeq_loc::TOpFlags flags)
{
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*loc.GetId());

    TSeqPos genomic_size = m_scope->GetSequenceLength(*id);
    CRef<CSeq_loc> left_loc(new CSeq_loc);
    CRef<CSeq_loc> right_loc(new CSeq_loc);
                                                     
    ITERATE(CSeq_loc, it, loc) {
        if (it.GetRangeAsSeq_loc()->GetStart(eExtreme_Biological) > outside_point)
            left_loc->Add(*it.GetRangeAsSeq_loc());
        else
            right_loc->Add(*it.GetRangeAsSeq_loc());
    }

    left_loc = left_loc->Merge(flags, NULL);
    right_loc = right_loc->Merge(flags, NULL);

    bool no_gap_at_origin = (left_loc->GetStop(eExtreme_Positional) == genomic_size-1 &&
                             right_loc->GetStart(eExtreme_Positional) == 0);

    if (loc.IsReverseStrand()) {
        swap(left_loc, right_loc);
    }
    left_loc->Add(*right_loc);

    if (no_gap_at_origin) {
        left_loc->ChangeToPackedInt();
        NON_CONST_ITERATE(CPacked_seqint::Tdata, it,left_loc->SetPacked_int().Set()) {
            CSeq_interval& interval = **it;
            if (interval.GetFrom() == 0) {
                interval.SetFuzz_from().SetLim(CInt_fuzz::eLim_circle);
            }
            if (interval.GetTo() == genomic_size-1) {
                interval.SetFuzz_to().SetLim(CInt_fuzz::eLim_circle);
            }
        }
    }

    return left_loc;
}

bool CFeatureGenerator::
SImplementation::HasMixedGenomicIds(const CSeq_align& align)
{
    set<CSeq_id_Handle> genomic_ids;

    if (!align.GetSegs().IsSpliced()) {
        return false;
    }

    const CSpliced_seg& sps = align.GetSegs().GetSpliced();
    if(sps.CanGetGenomic_id())
        genomic_ids.insert(CSeq_id_Handle::GetHandle(sps.GetGenomic_id()));

    const CSpliced_seg& spliced_seg = align.GetSegs().GetSpliced();
    ITERATE(CSpliced_seg::TExons, it, spliced_seg.GetExons()) {
        const CSpliced_exon& exon = **it;
        if (exon.CanGetGenomic_id()) {
            genomic_ids.insert(CSeq_id_Handle::GetHandle(exon.GetGenomic_id()));
        }
    }

    return genomic_ids.size() > 1;
}

void AddInsertWithGaps(
                       CRef<CSeq_loc>& edited_sequence_seqloc,
                       CSeq_id& genomic_seqid,
                       int& region_begin,
                       int& region_end,
                       int& offset,
                       CRef<CSeq_loc>& insert,
                       const int k_gap_length,
                       const int next_exon_start)
{
    if (insert->SetMix().Set().size() > 1) {
        NCBI_THROW(CException, eUnknown, "spliced-seq with several insert exons in a row not supported");
    }
    
    if (insert->SetMix().Set().size() > 0) {
        int half_intron_length = (next_exon_start - region_end)/2;
        int copy_length = min(k_gap_length, half_intron_length);
        region_end += copy_length;
        
        if (region_begin < region_end) {
            CRef<CSeq_loc> genome_loc(new CSeq_loc(genomic_seqid,
                                                   region_begin,
                                                   region_end -1));
            edited_sequence_seqloc->SetMix().Set().push_back(genome_loc);
        }
        if (copy_length < k_gap_length) {
            int gap_length = k_gap_length - copy_length;
            // fill gap with sequence from the genome itself for simplicity
            // do not bother creating nonexisting sequence
            CRef<CSeq_loc> gap_loc(new CSeq_loc(genomic_seqid, 0, gap_length-1)); 
            edited_sequence_seqloc->SetMix().Set().push_back(gap_loc);
            offset += gap_length;
        }
        
        edited_sequence_seqloc->SetMix().Set().push_back(insert);
        insert.Reset(new CSeq_loc);
        
        if (copy_length < k_gap_length) {
            int gap_length = k_gap_length - copy_length;
            CRef<CSeq_loc> gap_loc(new CSeq_loc(genomic_seqid, 0, gap_length-1));
            edited_sequence_seqloc->SetMix().Set().push_back(gap_loc);
            offset += gap_length;
        }
        
        region_begin = region_end;
    }
}

CRef<CSeq_feat>
CFeatureGenerator::
SImplementation::ConvertMixedAlignToAnnot(const CSeq_align& input_align,
                                     CSeq_annot& annot,
                                     CBioseq_set& seqs,
                                     int gene_id,
                                     const CSeq_feat* cds_feat_on_query_mrna_ptr,
                                     bool call_on_align_list)
{

    CRef<CSeq_align> align(new CSeq_align);
    align->Assign(input_align);

    CRef<CSeq_loc> edited_sequence_seqloc(new CSeq_loc);


    CSpliced_seg& spliced_seg = align->SetSegs().SetSpliced();
    if (!spliced_seg.CanGetGenomic_id()) {
        NCBI_THROW(CException, eUnknown, "Mixed-genomic spliced-seq does not have spliced-seg.genomic_id");
    }
    CRef<CSeq_id> genomic_seqid(new CSeq_id);
    genomic_seqid->Assign(spliced_seg.GetGenomic_id());
    ENa_strand genomic_strand = eNa_strand_plus;
    if (spliced_seg.CanGetGenomic_strand()) {
        genomic_strand = spliced_seg.GetGenomic_strand();
    } else {
        ITERATE(CSpliced_seg::TExons, it, spliced_seg.GetExons()) {
            const CSpliced_exon& exon = **it;
            if ((!exon.CanGetGenomic_id() || exon.GetGenomic_id().Match(*genomic_seqid)) &&
                exon.CanGetGenomic_strand()) {
                genomic_strand = exon.GetGenomic_strand();
                break;
            }
        }
    }

    CSeq_id_Handle idh= CSeq_id_Handle::GetHandle(*genomic_seqid);
    CBioseq_Handle bsh = m_scope->GetBioseqHandle(idh);
    TSeqPos genomic_length = bsh.GetBioseqLength();

    { // collect genomic seqlocs for virtual sequence and map exons to it

        const int k_gap_length = min(1000, int(genomic_length));

    if (genomic_strand == eNa_strand_minus) {
        // reverse exons to process them same way as plus strand
        // will reverse back in the end
        spliced_seg.SetExons().reverse();
    }
    int region_begin = 0; //included endpoint
    int region_end = 0;   //not included endpoint
    int offset = 0;
    CRef<CSeq_loc> insert(new CSeq_loc);
    NON_CONST_ITERATE(CSpliced_seg::TExons, it, spliced_seg.SetExons()) {
        CSpliced_exon& exon = **it;
        CSeq_id& seqid = exon.CanGetGenomic_id() ? exon.SetGenomic_id() : *genomic_seqid;

        int exon_start = exon.GetGenomic_start(); // included endpoint
        int exon_stop = exon.GetGenomic_end();    // included endpoint

        if (!seqid.Match(*genomic_seqid)) {

            ENa_strand strand = exon.CanGetGenomic_strand() ? exon.GetGenomic_strand() : genomic_strand;
            CRef<CSeq_loc> loc(new CSeq_loc(seqid, exon_start, exon_stop, strand));
            if (genomic_strand == eNa_strand_minus) {
                loc->FlipStrand();
            }
            insert->SetMix().Set().push_back(loc);

            int exon_length = exon_stop - exon_start +1;
            exon_stop = region_end + k_gap_length -1;
            exon_start = region_end + k_gap_length - exon_length;
            offset += exon_length;
        } else {
            if (exon.CanGetGenomic_strand() && exon.GetGenomic_strand() != genomic_strand) {
                NCBI_THROW(CException, eUnknown, "spliced-seq with mixed genomic strands not supported");
            }
            if (!(region_end <= exon_start)) {
                NCBI_THROW(CException, eUnknown, "spliced-seq with exons out of order not supported");
            }

            AddInsertWithGaps(edited_sequence_seqloc,
                              *genomic_seqid,
                              region_begin,
                              region_end,
                              offset,
                              insert,
                              k_gap_length,
                              exon_start);

            region_end = exon_stop +1;
        }

        exon.ResetGenomic_id();
        exon.ResetGenomic_strand();

        exon.SetGenomic_start(exon_start + offset);
        exon.SetGenomic_end(exon_stop + offset);
    }

    AddInsertWithGaps(edited_sequence_seqloc,
                      *genomic_seqid,
                      region_begin,
                      region_end,
                      offset,
                      insert,
                      k_gap_length,
                      genomic_length);

    if (region_begin < (int)genomic_length) {
        CRef<CSeq_loc> genome_loc(new CSeq_loc(*genomic_seqid,
                                               region_begin,
                                               genomic_length -1));
        edited_sequence_seqloc->SetMix().Set().push_back(genome_loc);
    }

    if (genomic_strand == eNa_strand_minus) {
        // reverse exons back
        spliced_seg.SetExons().reverse();
    }
    spliced_seg.SetGenomic_strand(genomic_strand);
    }

    edited_sequence_seqloc->ChangeToPackedInt();
    CRef<CBioseq> bioseq(new CBioseq(*edited_sequence_seqloc));
    CRef<CSeq_entry> seqentry(new CSeq_entry);
    seqentry->SetSeq(*bioseq);
    
    bioseq->SetInst().SetTopology() = bsh.GetCompleteBioseq()->GetInst().GetTopology();
    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Source);
        if (desc) {
            CRef<CSeqdesc> seq_desc(new CSeqdesc);
            seq_desc->Assign(*desc);
            bioseq->SetDescr().Set().push_back(seq_desc);
        }
    }}
    {{
        CSeqdesc_CI desc(bsh, CSeqdesc::e_Org);
        if (desc) {
            CRef<CSeqdesc> seq_desc(new CSeqdesc);
            seq_desc->Assign(*desc);
            bioseq->SetDescr().Set().push_back(seq_desc);
        }
    }}

    CBioseq_Handle bioseq_handle = m_scope->AddBioseq(*bioseq);

    CRef<CSeq_id> bioseq_id(new CSeq_id);
    bioseq_id->Assign(*bioseq->GetFirstId());
    spliced_seg.SetGenomic_id(*bioseq_id);

    CRef<CSeq_feat> gene_feat;
    if (gene_id) {
        TGeneMap::iterator gene = genes.find(gene_id);
        if (gene != genes.end()) {
            gene_feat = gene->second;
            genes.erase(gene);
        }
    }

    CSeq_annot annot_local;
    CBioseq_set seqs_tmp;
    ConvertAlignToAnnot(*align, annot_local, seqs_tmp, gene_id, cds_feat_on_query_mrna_ptr,
                                               call_on_align_list);

    m_scope->RemoveBioseq(bioseq_handle);
    annot_local.SetData().SetFtable().clear();

    if (gene_id) {
        if (gene_feat) {
            genes[gene_id] = gene_feat;
        } else {
            genes.erase(gene_id);
        }
    }

    set<CSeq_id_Handle> insert_ids;
    TSeqPos insert_length = 0;

    align.Reset(new CSeq_align);
    align->Assign(input_align);
    {
        CSpliced_seg& spliced_seg = align->SetSegs().SetSpliced();
        ERASE_ITERATE(CSpliced_seg::TExons, it, spliced_seg.SetExons()) {
            CSpliced_exon& exon = **it;
            CSeq_id& seqid = exon.CanGetGenomic_id() ? exon.SetGenomic_id() : *genomic_seqid;
            
            if (!seqid.Match(*genomic_seqid)) {
                insert_ids.insert(CSeq_id_Handle::GetHandle(seqid));
                insert_length += exon.GetGenomic_end()-exon.GetGenomic_start()+1;
                spliced_seg.SetExons().erase(it);
            }
        }
    }

    CBioseq_set seqs_discard;
    CRef<CSeq_feat> feat =
        ConvertAlignToAnnot(*align, annot_local, seqs_discard,
                            gene_id, cds_feat_on_query_mrna_ptr,
                            call_on_align_list);

    // inst.hist.assembly = input annot
    align.Reset(new CSeq_align);
    align->Assign(input_align);
    NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, seqs_tmp.SetSeq_set()) {
        CSeq_entry& entry = **it;
        if (entry.IsSeq() &&
            entry.GetSeq().IsSetInst() &&
            entry.GetSeq().GetInst().IsSetHist() &&
            entry.GetSeq().GetInst().GetHist().IsSetAssembly()) {
            
            entry.SetSeq().SetInst().SetHist().SetAssembly().front() = 
                align;
            break;
        }
    }
    if (seqs_tmp.IsSetClass()) {
        seqs.SetClass(seqs_tmp.GetClass());
    }
    seqs.SetSeq_set().splice(seqs.SetSeq_set().end(), seqs_tmp.SetSeq_set());

    for (list<CRef<CSeq_feat> >::reverse_iterator it = annot_local.SetData().SetFtable().rbegin();
         it != annot_local.SetData().SetFtable().rend(); ++it) {
        CSeq_feat& f = **it;
        if (f.GetData().IsGene()) {
            continue;
        }
        
        x_SetExceptText(f, k_except_text_for_gap_filled_gnomon_model);
        _ASSERT(insert_ids.size() > 0);
        NON_CONST_ITERATE (set<CSeq_id_Handle>, id, insert_ids) {
            x_SetQualForGapFilledModel(f, *id);
        }
        x_SetCommentForGapFilledModel(f, insert_length);

    }

    annot.SetData().SetFtable().splice(annot.SetData().SetFtable().end(),
                                       annot_local.SetData().SetFtable());

    return feat;
}

END_NCBI_SCOPE
