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

#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Splice_site.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Object_id.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include "feature_generator.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

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


/////////////////////////////////////

CFeatureGenerator::SImplementation::SImplementation(CScope& scope)
    : m_scope(&scope)
    , m_flags(fDefaults)
    , m_min_intron(kDefaultMinIntron)
    , m_allowed_unaligned(kDefaultAllowedUnaligned)
    , m_is_gnomon(false)
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


///
/// Return the mol-info object for a given sequence
///
static const CMolInfo* s_GetMolInfo(const CBioseq_Handle& handle)
{
    CSeqdesc_CI desc_iter(handle, CSeqdesc::e_Molinfo);
    for ( ;  desc_iter;  ++desc_iter) {
        return &desc_iter->GetMolinfo();
    }

    return NULL;
}

CFeatureGenerator::SImplementation::SMapper::SMapper(const CSeq_align& aln, CScope& scope,
                                                     TSeqPos allowed_unaligned,
                                                     CSeq_loc_Mapper::TMapOptions opts)
    : m_aln(aln), m_scope(scope), m_allowed_unaligned(allowed_unaligned)
{
    if(aln.GetSegs().IsSpliced()) {
        //row 1 is always genomic in spliced-segs
        m_genomic_row = 1;
        m_mapper.Reset
            (new CSeq_loc_Mapper(aln, aln.GetSeq_id(1),
                                 &scope, opts));
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
                    if(m_mapper.IsNull()) {
                        m_mapper.Reset
                            (new CSeq_loc_Mapper(aln, aln.GetSeq_id(i),
                                                 &scope, opts));
                        m_genomic_row = i;
                    } else {
                        NCBI_THROW(CException, eUnknown,
                                   "CreateGeneModelFromAlign(): "
                                   "More than one genomic row in alignment");
                    }
                }
        }
        if (m_mapper.IsNull()) {
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
            rna_loc = m_mapper->Map(*range_loc);
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
    CRef<CSeq_loc> mapped_loc  = m_mapper->Map(loc);
    return mapped_loc;
}

void CFeatureGenerator::SImplementation::SMapper::IncludeSourceLocs(bool b)
{
    if (m_mapper) {
        m_mapper->IncludeSourceLocs(b);
    }
}

void CFeatureGenerator::SImplementation::SMapper::SetMergeNone()
{
    if (m_mapper) {
        m_mapper->SetMergeNone();
    }
}

CRef<CSeq_loc> CFeatureGenerator::SImplementation::SMapper::x_GetLocFromSplicedExons(const CSeq_align& aln) const
{
    CRef<CSeq_loc> loc(new CSeq_loc);
    CConstRef<CSpliced_exon> prev_exon;
    CRef<CSeq_interval> prev_int;
    
    const CSpliced_seg& spliced_seg = aln.GetSegs().GetSpliced();
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
        if(!prev_exon.IsNull()) {
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

    return align;
}

static void s_TransformToNucpos(CProduct_pos &pos)
{
    TSeqPos nucpos = pos.GetProtpos().GetAmin()*3;
    if (pos.GetProtpos().CanGetFrame() && pos.GetProtpos().GetFrame() > 0) {
        nucpos += pos.GetProtpos().GetFrame() - 1;
    }
    pos.SetNucpos(nucpos);
}

string ExtractGnomonModelNum(const CSeq_id& seq_id)
{
    string model_num;
    if (seq_id.IsGeneral() && seq_id.GetGeneral().CanGetDb() && seq_id.GetGeneral().GetDb() == "GNOMON") {
        model_num = seq_id.GetGeneral().GetTag().GetStr();
        model_num.erase(model_num.size()-2, 2);
    }
    return model_num;
}

CRef<CSeq_feat>
CFeatureGenerator::
SImplementation::ConvertAlignToAnnot(const CSeq_align& input_align,
                                     CSeq_annot& annot,
                                     CBioseq_set& seqs,
                                     int gene_id,
                                     const CSeq_feat* cdregion,
                                     bool call_on_align_list)
{
    CConstRef<CSeq_align> align(&input_align);
    CRef<CSeq_feat> cd_feat;
    bool is_protein_align = align->CanGetSegs() && align->GetSegs().IsSpliced()
                          && align->GetSegs().GetSpliced().GetProduct_type()
                             == CSpliced_seg::eProduct_type_protein;
    if (is_protein_align)
    {
        /// This is a protein alignment; transform it into a fake transcript alignment
        /// so the rest of the processing can go on
        CSeq_align *fake_transcript_align = new CSeq_align;
        align.Reset(fake_transcript_align);
        fake_transcript_align->Assign(input_align);
        fake_transcript_align->SetSegs().SetSpliced().SetProduct_type(
            CSpliced_seg::eProduct_type_transcript);
        NON_CONST_ITERATE (CSpliced_seg::TExons, exon_it,
                      fake_transcript_align->SetSegs().SetSpliced().SetExons())
        {
            s_TransformToNucpos((*exon_it)->SetProduct_start());
            s_TransformToNucpos((*exon_it)->SetProduct_end());
        }

        CRef<CSeq_id> prot_id(new CSeq_id);
        prot_id->Assign(fake_transcript_align->GetSeq_id(0));
        TSeqPos prot_length =
            fake_transcript_align->GetSegs().GetSpliced().CanGetProduct_length()
            ? fake_transcript_align->GetSegs().GetSpliced().GetProduct_length()
            : m_scope->GetBioseqHandle(*prot_id).GetBioseqLength();
        CRef<CSeq_loc> fake_prot_loc(new CSeq_loc(*prot_id, 0, prot_length*3-1));

        cd_feat.Reset(new CSeq_feat);
        cd_feat->SetData().SetCdregion().SetFrame(CCdregion::eFrame_one);
        cd_feat->SetLocation(*fake_prot_loc);
        cd_feat->SetProduct().SetWhole(*prot_id);
        cdregion = cd_feat.GetPointer();
    }

    CScopeTransaction tr = m_scope->GetTransaction(); // to rollback any changes at return

    CSeq_loc_Mapper::TMapOptions opts = 0;
    if (m_flags & fDensegAsExon) {
        opts |= CSeq_loc_Mapper::fAlign_Dense_seg_TotalRange;
    }

    CScope mapper_scope(*CObjectManager::GetInstance());
    mapper_scope.AddScope(*m_scope);

    SMapper mapper(*align, mapper_scope, m_allowed_unaligned, opts);

    CTime time(CTime::eCurrent);

    const CSeq_id& rna_id = align->GetSeq_id(mapper.GetRnaRow());

    m_is_gnomon = !ExtractGnomonModelNum(rna_id).empty();


    /// we always need the mRNA location as a reference
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->Assign(mapper.GetRnaLoc());

    static CAtomicCounter counter;
    size_t model_num = counter.Add(1);

    vector<CMappedFeat> ncRNAs;
    CMappedFeat cdregion_handle;

    if (!cdregion) {
        cdregion_handle = GetCdsOnMrna(rna_id);
        if (cdregion_handle) {
            cdregion = &cdregion_handle.GetMappedFeature();
        }
    }

    CBioseq_Handle handle = m_scope->GetBioseqHandle(rna_id);
    if (handle) {
        for (CFeat_CI feat_iter(handle, CSeqFeatData::eSubtype_ncRNA);
             feat_iter;  ++feat_iter) {
            ncRNAs.push_back(*feat_iter);
        }
    }
    else {
        LOG_POST(Warning << "failed to retrieve sequence for: "
                 << CSeq_id_Handle::GetHandle(rna_id));
    }

    CRef<CSeq_feat> mrna_feat =
        x_CreateMrnaFeature(*align, loc, time, model_num,
                            seqs, rna_id, cdregion);

    CRef<CSeq_feat> gene_feat;

    if (!handle) {
        handle = m_scope->GetBioseqHandle(rna_id);
    }

    if(!call_on_align_list){
        const CSeq_id& genomic_id = align->GetSeq_id(mapper.GetGenomicRow());
        if (gene_id) {
            TGeneMap::iterator gene = genes.find(gene_id);
            if (gene == genes.end()) {
                x_CreateGeneFeature(gene_feat, handle, mapper, loc,
                                    genomic_id, gene_id);
                if (gene_feat) {
                    _ASSERT(gene_feat->GetData().Which() != CSeqFeatData::e_not_set);
                    annot.SetData().SetFtable().push_back(gene_feat);
                }
                gene = genes.insert(make_pair(gene_id,gene_feat)).first;
            } else {
                gene_feat = gene->second;
                gene_feat->SetLocation
                    (*gene_feat->GetLocation().Add(mrna_feat->GetLocation(),
                                                   CSeq_loc::fMerge_SingleRange,
                                                   NULL));
            }
    
            CRef< CSeqFeatXref > genexref( new CSeqFeatXref() );
            genexref->SetId(*gene_feat->SetIds().front());
        
            CRef< CSeqFeatXref > mrnaxref( new CSeqFeatXref() );
            mrnaxref->SetId(*mrna_feat->SetIds().front());
    
            gene_feat->SetXref().push_back(mrnaxref);
            mrna_feat->SetXref().push_back(genexref);
            
        } else {
            x_CreateGeneFeature(gene_feat, handle, mapper,
                                loc, genomic_id);
            if (gene_feat) {
                _ASSERT(gene_feat->GetData().Which() != CSeqFeatData::e_not_set);
                annot.SetData().SetFtable().push_back(gene_feat);
            }
        }
    }

    if (!is_protein_align && mrna_feat) {
        // NOTE: added after gene!

        _ASSERT(mrna_feat->GetData().Which() != CSeqFeatData::e_not_set);
        annot.SetData().SetFtable().push_back(mrna_feat);
    }

    CSeq_annot::C_Data::TFtable propagated_features;

    CRef<CSeq_feat> cds_feat =
        x_CreateCdsFeature(cdregion, *align, loc, time, model_num, seqs, opts);

    if(cds_feat)
        propagated_features.push_back(cds_feat);

    ITERATE(vector<CMappedFeat>, it, ncRNAs){
        CRef<CSeq_feat> ncrna_feat =
            x_CreateNcRnaFeature(&it->GetOriginalFeature(), *align, loc, opts);
        if(ncrna_feat)
            propagated_features.push_back(ncrna_feat);
    }

    NON_CONST_ITERATE(CSeq_annot::C_Data::TFtable, it, propagated_features){
        _ASSERT((*it)->GetData().Which() != CSeqFeatData::e_not_set);
        annot.SetData().SetFtable().push_back(*it);
        
        if (m_is_gnomon) {  // create xrefs for gnomon models
            CRef< CSeqFeatXref > propagatedxref( new CSeqFeatXref() );
            propagatedxref->SetId(*(*it)->SetIds().front());
        
            CRef< CSeqFeatXref > mrnaxref( new CSeqFeatXref() );
            mrnaxref->SetId(*mrna_feat->SetIds().front());
        
            (*it)->SetXref().push_back(mrnaxref);
            mrna_feat->SetXref().push_back(propagatedxref);
        }
    }

    if(!call_on_align_list){
        if(propagated_features.empty()){
            SetPartialFlags(gene_feat, mrna_feat, CRef<CSeq_feat>());
        }
        ITERATE(CSeq_annot::C_Data::TFtable, it, propagated_features){
            x_CheckInconsistentDbxrefs(gene_feat, *it);
            SetPartialFlags(gene_feat, mrna_feat, *it);
        }
        x_CopyAdditionalFeatures(handle, mapper, annot);
    }

    if (m_is_gnomon) {
        tr.Commit();
    } else {
        tr.RollBack();
    }

    if (mrna_feat) {
        SetFeatureExceptions(*mrna_feat, align);
    }
    if (cds_feat) {
        SetFeatureExceptions(*cds_feat, align);
    }

    return is_protein_align ? cds_feat : mrna_feat;
}

void
CFeatureGenerator::
SImplementation::ConvertAlignToAnnot(
                         const list< CRef<CSeq_align> > &aligns,
                         CSeq_annot &annot,
                         CBioseq_set &seqs)
{
    CScope mapper_scope(*CObjectManager::GetInstance());
    mapper_scope.AddScope(*m_scope);
    
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

        SMapper mapper(*clean_align, mapper_scope, m_allowed_unaligned, opts);
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

bool IsContinuous(CSeq_loc& loc)
{
    for (CSeq_loc_CI loc_it(loc); loc_it;  ++loc_it) {
        if ((loc_it.GetRange().GetFrom() != loc.GetTotalRange().GetFrom() && loc_it.GetFuzzFrom() != NULL) ||
            (loc_it.GetRange().GetTo() != loc.GetTotalRange().GetTo() && loc_it.GetFuzzTo() != NULL)) {
            return false;
        }
    }
    return true;
}
    
CConstRef<CSeq_loc> GetSeq_loc(CSeq_loc_CI& loc_it)
{
    CRef<CSeq_loc> this_loc(new CSeq_loc);
    this_loc->SetInt().SetFrom(loc_it.GetRange().GetFrom());
    this_loc->SetInt().SetTo(loc_it.GetRange().GetTo());
    this_loc->SetInt().SetStrand(loc_it.GetStrand());
    this_loc->SetInt().SetId().Assign
        (*loc_it.GetSeq_id_Handle().GetSeqId());
    this_loc->SetPartialStart(loc_it.GetFuzzFrom()!=NULL, eExtreme_Positional);
    this_loc->SetPartialStop(loc_it.GetFuzzTo()!=NULL, eExtreme_Positional);
    return this_loc;
}

void AddLiteral(CSeq_inst& inst, const string& seq, CSeq_inst::EMol mol_class)
{
    if (inst.IsSetExt()) {
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
            default:
                inst.SetExt().SetDelta().AddLiteral(seq, mol_class);
                return;
            }
            iupacna += seq;
            delta_seq.SetLiteral().SetSeq_data().SetIupacna().Set(iupacna);
            delta_seq.SetLiteral().SetLength(iupacna.size());
            CSeqportUtil::Pack(&delta_seq.SetLiteral().SetSeq_data());
        } else {
            inst.SetExt().SetDelta().AddLiteral(seq, mol_class);
        }
    } else {
        inst.SetSeq_data().SetIupacna().Set() += seq;
    }
}

void CFeatureGenerator::
SImplementation::x_CollectMrnaSequence(CSeq_inst& inst,
                                       const CSeq_align& align,
                                       const CSeq_loc& loc,
                                       bool add_unaligned_parts,
                                       bool* has_gap,
                                       bool* has_indel)
{
    /// set up the inst
    inst.SetMol(CSeq_inst::eMol_rna);

    // this is created as a transcription of the genomic location

    CSeq_loc_Mapper to_mrna(align, CSeq_loc_Mapper::eSplicedRow_Prod);
    CSeq_loc_Mapper to_genomic(align, CSeq_loc_Mapper::eSplicedRow_Gen);

    to_mrna.SetMergeNone();
    to_mrna.KeepNonmappingRanges();
    to_genomic.SetMergeNone();
    to_genomic.KeepNonmappingRanges();

    int seq_size = 0;
    int prev_to = -1;
    bool prev_fuzz = false;

    for (CSeq_loc_CI loc_it(loc,
                            CSeq_loc_CI::eEmpty_Skip,
                            CSeq_loc_CI::eOrder_Biological);
         loc_it;  ++loc_it) {

        CConstRef<CSeq_loc> exon = GetSeq_loc(loc_it);
        CRef<CSeq_loc> mrna_loc = to_mrna.Map(*exon);

        if ((prev_to > -1  &&
             (loc_it.GetStrand() != eNa_strand_minus ?
              loc_it.GetFuzzFrom() : loc_it.GetFuzzTo()) != NULL)  ||
            prev_fuzz) {
            if (has_gap != NULL) {
                *has_gap = true;
            }
            if (!inst.IsSetExt()) {
                inst.SetExt().SetDelta().AddLiteral
                    (inst.GetSeq_data().GetIupacna().Get(),CSeq_inst::eMol_rna);
                inst.ResetSeq_data();
            }
            int gap_len = add_unaligned_parts ? mrna_loc->GetTotalRange().GetFrom()-(prev_to+1) : 0;
            inst.SetExt().SetDelta().AddLiteral(gap_len);
            if (gap_len == 0)
                inst.SetExt().SetDelta().Set().back()
                    ->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
        }

        int part_count = 0;
        for (CSeq_loc_CI part_it(*mrna_loc);  part_it;  ++part_it) {
            ++part_count;
            if (prev_to<0) {
                prev_to = part_it.GetRange().GetFrom()-1;
                if (add_unaligned_parts && part_it.GetRange().GetFrom() > 0) {
                    seq_size = part_it.GetRange().GetFrom();
                    inst.SetExt().SetDelta().AddLiteral(seq_size);
                }
            }
            int deletion_len = part_it.GetRange().GetFrom()-(prev_to+1);
            if (deletion_len > 0) {
                if (has_indel != NULL) {
                    *has_indel = true;
                }
                string deletion(deletion_len, 'N');
                AddLiteral(inst, deletion, CSeq_inst::eMol_rna);
            }

            CConstRef<CSeq_loc> part = GetSeq_loc(part_it);
            CRef<CSeq_loc> genomic_loc = to_genomic.Map(*part);

            CSeqVector vec(*genomic_loc, *m_scope, CBioseq_Handle::eCoding_Iupac);
            string seq;
            vec.GetSeqData(0, vec.size(), seq);

            AddLiteral(inst, seq, CSeq_inst::eMol_rna);

            seq_size += part_it.GetRange().GetTo()-prev_to;

            prev_to = part_it.GetRange().GetTo();
        }
        if (part_count > 1 && has_indel != NULL) {
            *has_indel = true;
        }
            
        prev_fuzz = (loc_it.GetStrand() != eNa_strand_minus ?
                     loc_it.GetFuzzTo() : loc_it.GetFuzzFrom()) != NULL;
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
    
    CRef<CSeq_align> assembly(new CSeq_align);
    assembly->Assign(align);
    inst.SetHist().SetAssembly().push_back(assembly);
}

void CFeatureGenerator::
SImplementation::x_CreateMrnaBioseq(const CSeq_align& align,
                                    CRef<CSeq_loc> loc,
                                    const CTime& time,
                                    size_t model_num,
                                    CBioseq_set& seqs,
                                    const CSeq_id& rna_id,
                                    const CSeq_feat* cdregion)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq& bioseq = entry->SetSeq();
    
    CRef<CSeq_id> id;
    if (m_flags & fGenerateLocalIds) {
        /// create a new seq-id for this
        string str("lcl|CDNA_");
        str += time.AsString("YMD");
        str += "_";
        str += NStr::SizetToString(model_num);
        id.Reset(new CSeq_id(str));
    } else {
        id.Reset(new CSeq_id());
        id->Assign(rna_id);
    }
    bioseq.SetId().push_back(id);
    
    CRef<CSeqdesc> mdes(new CSeqdesc);
    entry->SetSeq().SetDescr().Set().push_back(mdes);
    mdes->SetMolinfo().SetBiomol(cdregion==NULL ? CMolInfo::eBiomol_ncRNA : CMolInfo::eBiomol_mRNA);

    CMolInfo::ECompleteness completeness;
    if (!IsContinuous(*loc)) {
        completeness = CMolInfo::eCompleteness_partial;
    } else if (cdregion==NULL) {
        completeness = CMolInfo::eCompleteness_unknown;
    } else if (cdregion->GetLocation().IsPartialStart(eExtreme_Biological) &&
               cdregion->GetLocation().IsPartialStop(eExtreme_Biological)
               ) {
        completeness = CMolInfo::eCompleteness_no_ends;
    } else if (cdregion->GetLocation().IsPartialStart(eExtreme_Biological)) {
        completeness = CMolInfo::eCompleteness_no_left;
    } else if (cdregion->GetLocation().IsPartialStop(eExtreme_Biological)) {
        completeness = CMolInfo::eCompleteness_no_right;
    } else {
        completeness = CMolInfo::eCompleteness_unknown;
    }
    mdes->SetMolinfo().SetCompleteness(completeness);

    x_CollectMrnaSequence(bioseq.SetInst(), align, *loc);

    if (cdregion != NULL) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        entry->SetSeq().SetAnnot().push_back(annot);
        CRef<CSeq_feat> cdregion_ref(new CSeq_feat);
        cdregion_ref->Assign(*cdregion);
        _ASSERT(cdregion_ref->GetData().Which() != CSeqFeatData::e_not_set);
        annot->SetData().SetFtable().push_back(cdregion_ref);
    }
    
    if ((m_flags & fForceTranscribeMrna) && (m_flags & fForceTranslateCds)) {
        seqs.SetClass(CBioseq_set::eClass_nuc_prot);
    }
    seqs.SetSeq_set().push_back(entry);

    CBioseq_Handle handle = m_scope->GetBioseqHandle(*id);
    if (!handle) {
        m_scope->ResetHistory(); //otherwise adding new entry to non-clean scope produces warnings
        m_scope->AddTopLevelSeqEntry(*entry);
    } else {
        CBioseq_EditHandle edithandle = m_scope->GetEditHandle(handle);
        edithandle.SetInst(bioseq.SetInst());
    }
}

void CFeatureGenerator::
SImplementation::x_CreateProteinBioseq(CSeq_loc* cds_loc,
                                       CSeq_feat& cds_on_mrna,
                                       const CTime& time,
                                       size_t model_num,
                                       CBioseq_set& seqs,
                                       const CSeq_id& prot_id)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq& bioseq = entry->SetSeq();
    
    CRef<CSeq_id> id;
    if ((m_flags & fGenerateLocalIds) || prot_id.Which() == CSeq_id::e_not_set) {
        // create a new seq-id for this
        string str("lcl|PROT_");
        str += time.AsString("YMD");
        str += "_";
        str += NStr::SizetToString(model_num);
        id.Reset(new CSeq_id(str));
    } else {
        id.Reset(new CSeq_id());
        id->Assign(prot_id);
    }
    bioseq.SetId().push_back(id);

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

    string strprot;
    CSeqTranslator::Translate(cds_on_mrna, *m_scope, strprot, false, false);

    CSeq_inst& seq_inst = bioseq.SetInst();
    seq_inst.SetMol(CSeq_inst::eMol_aa);
    seq_inst.SetLength(strprot.size());

        seq_inst.SetRepr(CSeq_inst::eRepr_delta);
        CSeqVector seqv(cds_on_mrna.GetLocation(), m_scope, CBioseq_Handle::eCoding_Ncbi);
        CConstRef<CSeqMap> map;
        map.Reset(&seqv.GetSeqMap());

        const CCdregion& cdr = cds_on_mrna.GetData().GetCdregion();
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

        size_t b = 0;
        size_t e = 0;

        for( CSeqMap_CI ci = map->BeginResolved(m_scope.GetPointer()); ci; ) {
            TSeqPos len = ci.GetLength() - frame;
            frame = (2*len)%3;
            e = b + (len+2)/3;
            if (ci.IsUnknownLength()) {
                seq_inst.SetExt().SetDelta().AddLiteral(len);
                seq_inst.SetExt().SetDelta().Set().back()->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
            } else if (!ci.IsSetData()) {
                seq_inst.SetExt().SetDelta().AddLiteral(e-b);
            } else {
                if (e > strprot.size()) {
                    _ASSERT( len%3 != 0 || !cds_loc->IsPartialStop(eExtreme_Biological) );
                    --e;
                }
                if (b < e)
                    seq_inst.SetExt().SetDelta().AddLiteral(strprot.substr(b,e-b),CSeq_inst::eMol_aa);
                
            }
            b = e;

            ci.Next();
        }
        _ASSERT( b == strprot.size() );

    if (seq_inst.SetExt().SetDelta().Set().size() == 1) {
        seq_inst.SetRepr(CSeq_inst::eRepr_raw);
        CRef<CSeq_data> dprot(new CSeq_data);
        dprot->Assign(seq_inst.SetExt().SetDelta().Set().back()->GetLiteral().GetSeq_data());
        seq_inst.SetSeq_data(*dprot);
        seq_inst.ResetExt();
    }

    CBioseq_Handle handle = m_scope->GetBioseqHandle(*id);
    if (!handle) {
        m_scope->ResetHistory(); //otherwise adding new entry to non-clean scope produces warnings
        m_scope->AddTopLevelSeqEntry(*entry);
    } else {
        CBioseq_EditHandle edithandle = m_scope->GetEditHandle(handle);
        edithandle.SetInst(bioseq.SetInst());
    }

#ifdef _DEBUG
    CBioseq_Handle prot_h = m_scope->GetBioseqHandle(*id);
    CSeqVector vec(prot_h, CBioseq_Handle::eCoding_Iupac);
    string result;
    vec.GetSeqData(0, vec.size(), result);
    _ASSERT( strprot.size()==result.size() );
#endif
    
    seqs.SetSeq_set().push_back(entry);
}

CRef<CSeq_feat>
CFeatureGenerator::
SImplementation::x_CreateMrnaFeature(const CSeq_align& align,
                                     CRef<CSeq_loc> loc,
                                     const CTime& time,
                                     size_t model_num,
                                     CBioseq_set& seqs,
                                     const CSeq_id& rna_id,
                                     const CSeq_feat* cdregion)
{
    CRef<CSeq_feat> mrna_feat;
    if (m_flags & fCreateMrna) {
        mrna_feat.Reset(new CSeq_feat());
        CRNA_ref::TType type = CRNA_ref::eType_unknown;
        string name;    
        string RNA_class;
        if (m_flags & fForceTranscribeMrna) {
            /// create a new bioseq for this mRNA
            x_CreateMrnaBioseq(align, loc, time, model_num,
                               seqs, rna_id, cdregion);
        }

        string gnomon_model_num = ExtractGnomonModelNum(rna_id);
        if (!gnomon_model_num.empty()) {
            CRef<CObject_id> obj_id( new CObject_id() );
            obj_id->SetStr("rna." + gnomon_model_num);
            CRef<CFeat_id> feat_id( new CFeat_id() );
            feat_id->SetLocal(*obj_id);
            mrna_feat->SetIds().push_back(feat_id);
        }

        mrna_feat->SetProduct().SetWhole().Assign(rna_id);
        CBioseq_Handle handle = m_scope->GetBioseqHandle(rna_id);
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
            default:
                type = CRNA_ref::eType_other;
                break;
            }
        }

        mrna_feat->SetData().SetRna().SetType(type);
        if (!RNA_class.empty()) {
            mrna_feat->SetData().SetRna().SetExt().SetGen().SetClass(RNA_class);
        }
        name = sequence::GetTitle(handle);
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
            gene_loc = gene_loc->Merge(CSeq_loc::fMerge_SingleRange, NULL);
            if(update_existing_gene){
                TSeqRange gene_range =
                    gene_feat->GetLocation().GetTotalRange() +
                    gene_loc->GetTotalRange();
                gene_feat->SetLocation().SetInt().SetFrom(gene_range.GetFrom());
                gene_feat->SetLocation().SetInt().SetTo(gene_range.GetTo());
            } else {
                gene_feat->SetLocation(*gene_loc);
            }
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
                title = sequence::GetTitle(handle);
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
SImplementation::x_CreateCdsFeature(const CSeq_feat* cdregion_on_mrna,
                                    const CSeq_align& align,
                                    CRef<CSeq_loc> loc,
                                    const CTime& time,
                                    size_t model_num,
                                    CBioseq_set& seqs,
                                    CSeq_loc_Mapper::TMapOptions opts)
{
    CRef<CSeq_feat> cds_feat;
    if (m_flags & fCreateCdregion && cdregion_on_mrna != NULL) {

            TSeqPos offset;
            CRef<CSeq_loc> cds_loc = x_PropagateFeatureLocation(cdregion_on_mrna,
                                                                align, loc, opts, offset);
            if (cds_loc  && cds_loc->Which() != CSeq_loc::e_not_set) {
                cds_feat.Reset(new CSeq_feat());
                cds_feat->Assign(*cdregion_on_mrna);
                cds_feat->ResetId();

                CConstRef<CSeq_id> prot_id(new CSeq_id);
                if (cds_feat->CanGetProduct()) {
                    prot_id.Reset(cds_feat->GetProduct().GetId());
                }
                string gnomon_model_num = ExtractGnomonModelNum(*prot_id);
                if (m_flags & fForceTranslateCds) {
                    /// create a new bioseq for the CDS
                    if (!gnomon_model_num.empty()) {
                        CRef<CObject_id> obj_id( new CObject_id() );
                        obj_id->SetStr("cds." + gnomon_model_num);
                        CRef<CFeat_id> feat_id( new CFeat_id() );
                        feat_id->SetLocal(*obj_id);
                        cds_feat->SetIds().push_back(feat_id);
                    }
                    x_CreateProteinBioseq(cds_loc, *cds_feat, time,
                                          model_num, seqs, *prot_id);
                }

                bool is_partial_5prime =
                    cds_loc->IsPartialStart(eExtreme_Biological);

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
                    int cds_start = cdregion_on_mrna->GetLocation().GetTotalRange().GetFrom();
                    if (cds_start >= 3) {
                        CBioseq_Handle handle =
                            m_scope->GetBioseqHandle(*cdregion_on_mrna->GetLocation().GetId());
                        string strprot;
                        if (handle) {
                            CSeqVector vec(handle, CBioseq_Handle::eCoding_Iupac);
                            string mrna;
                            vec.GetSeqData(cds_start % 3, cds_start, mrna);
                            CSeqTranslator::Translate
                                (mrna, strprot,
                                 CSeqTranslator::fIs5PrimePartial);
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
                            stop_5prime_location->SetId(*handle.GetSeqId());
                            stop_5prime_feature->SetLocation(*stop_5prime_location);

                            SAnnotSelector sel(CSeq_annot::C_Data::e_Ftable);
                            sel.SetResolveNone();
                            CAnnot_CI it(handle, sel);
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

                        // NOTE: we changed the mapper; we may get an equiv.
                        // If we do, do the right thing
                        CRef<CSeq_loc> new_cb_loc = mapper.Map((*it)->GetLoc());
                        if (new_cb_loc->IsEquiv()) {
                            new_cb_loc = new_cb_loc->GetEquiv().Get().front();
                        }

                        if (new_cb_loc  &&  !new_cb_loc->IsNull() && new_cb_loc->GetTotalRange().GetLength()==3) {
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
    }
    return cds_feat;
}

CRef<CSeq_feat>
CFeatureGenerator::
SImplementation::x_CreateNcRnaFeature(const CSeq_feat* ncrnafeature_on_mrna,
                                      const CSeq_align& align,
                                      CRef<CSeq_loc> loc,
                                      CSeq_loc_Mapper::TMapOptions opts)
{
    CRef<CSeq_feat> ncrna_feat;
    if (m_flags & fPropagateNcrnaFeats && ncrnafeature_on_mrna != NULL) {

            TSeqPos offset;
            CRef<CSeq_loc> ncrna_loc = x_PropagateFeatureLocation(ncrnafeature_on_mrna,
                                                                  align, loc, opts, offset);

            if (ncrna_loc  && ncrna_loc->Which() != CSeq_loc::e_not_set) {
                ncrna_feat.Reset(new CSeq_feat());
                ncrna_feat->Assign(*ncrnafeature_on_mrna);
                ncrna_feat->ResetId();

                ncrna_feat->SetLocation(*ncrna_loc);
            }
    }
    return ncrna_feat;
}

CRef<CSeq_loc>
CFeatureGenerator::
SImplementation::x_PropagateFeatureLocation(const objects::CSeq_feat* feature_on_mrna,
                                            const CSeq_align& align,
                                            CRef<CSeq_loc> loc,
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
        CConstRef<CSeq_loc> this_loc = GetSeq_loc(loc_it);

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

        this_loc_mapped->SetPartialStart(is_partial_5prime, eExtreme_Biological);
        this_loc_mapped->SetPartialStop(is_partial_3prime, eExtreme_Biological);

        mapped_loc->SetMix().Set().push_back(this_loc_mapped);
    }
    if (mapped_loc) {
        mapped_loc->ChangeToPackedInt();
    }
    if (mapped_loc  && mapped_loc->Which() != CSeq_loc::e_not_set) {
        mapped_loc->SetId(*loc->GetId());
        if (mapped_loc->IsMix() && mapped_loc->GetMix().Get().size()==1) {
            mapped_loc = mapped_loc->SetMix().Set().front();
        }
    }
    return mapped_loc;
}

void CFeatureGenerator::
SImplementation::SetPartialFlags(CRef<CSeq_feat> gene_feat,
                                 CRef<CSeq_feat> mrna_feat,
                                 CRef<CSeq_feat> propagated_feat)
{
    if(propagated_feat){
        for (CSeq_loc_CI loc_it(propagated_feat->GetLocation());  loc_it;  ++loc_it) {
            if (loc_it.GetFuzzFrom()  ||  loc_it.GetFuzzTo()) {
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
            if (loc_it.GetFuzzFrom()  ||  loc_it.GetFuzzTo()) {
                mrna_feat->SetPartial(true);
                if(gene_feat)
                    gene_feat->SetPartial(true);
                break;
            }
        }
    }

    ///
    /// set geen partialness if mRNA is partial
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
                                  const CSeq_align* align)
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
    bool has_5prime_unaligned = false;
    bool has_3prime_unaligned = false;
    bool has_polya_tail = false;
    bool has_incomplete_polya_tail = false;
    bool has_mismatches = false;
    bool has_gaps = false;

    CBioseq_Handle prod_bsh    = m_scope->GetBioseqHandle(*feat.GetProduct().GetId());
    if ( !prod_bsh ) {
        NCBI_THROW(CException, eUnknown,
                   "failed to retrieve bioseq for "
                   + sequence::GetIdHandle(feat.GetProduct(), NULL).GetSeqId()->AsFastaString());
    }

    TSeqPos loc_len = sequence::GetLength(feat.GetLocation(), m_scope.GetPointer());
    if (loc_len > prod_bsh.GetBioseqLength()) {
        has_length_mismatch = true;
    }

    if (al) {
        ///
        /// can do full comparison
        ///

        /// we know we have a Spliced-seg
        /// evaluate for gaps or mismatches
        TSeqPos prev_to = 0;
        ITERATE (CSpliced_seg::TExons, exon_it,
                 al->GetSegs().GetSpliced().GetExons()) {
            const CSpliced_exon& exon = **exon_it;
            if (prev_to > 0 && prev_to+1 != exon.GetProduct_start().GetNucpos()) // discontinuity
                has_gaps = true;
            prev_to = exon.GetProduct_end().GetNucpos();
            if (exon.IsSetParts()) {
                ITERATE (CSpliced_exon::TParts, part_it, exon.GetParts()) {
                    switch ((*part_it)->Which()) {
                    case CSpliced_exon_chunk::e_Mismatch:
                        has_mismatches = true;
                        break;
                    case CSpliced_exon_chunk::e_Genomic_ins:
                    case CSpliced_exon_chunk::e_Product_ins:
                        has_gaps = true;
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
            has_5prime_unaligned = true;
        }

        TSeqPos max_align_len = 0;
        if (al->GetSegs().GetSpliced().IsSetPoly_a()) {
            has_polya_tail = true;
            max_align_len = al->GetSegs().GetSpliced().GetPoly_a();
        } else if (al->GetSegs().GetSpliced().IsSetProduct_length()) {
            max_align_len = al->GetSegs().GetSpliced().GetProduct_length();
        } else {
            max_align_len = prod_bsh.GetBioseqLength();
        }

        if (r.GetTo() + 1 < max_align_len) {
            has_3prime_unaligned = true;
        }

        /// also note the poly-A
        if (al->GetSegs().GetSpliced().IsSetPoly_a()) {
            has_polya_tail = true;
        }
    }

    if ( !has_5prime_unaligned  &&  !has_gaps  &&  !has_3prime_unaligned ) {
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

        for ( ;  prod_it != prod_end  &&  genomic_it != genomic_end;
              ++prod_it, ++genomic_it) {
            if (*prod_it != *genomic_it) {
                has_mismatches = true;
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
            has_polya_tail = true;
            if (count_a < tail_len * 0.95) {
                has_incomplete_polya_tail = true;
            }
        }
        else if (tail_len) {
            has_3prime_unaligned = true;
        }
    }

    string except_text;
    if (has_5prime_unaligned  ||
        has_3prime_unaligned  ||
        has_gaps  ||
        has_length_mismatch  ||
        has_incomplete_polya_tail) {
        except_text = "unclassified transcription discrepancy";
    }
    else if (has_mismatches) {
        except_text = "mismatches in transcription";
    }

    x_SetExceptText(feat, except_text);
}


void CFeatureGenerator::SImplementation::x_HandleCdsExceptions(CSeq_feat& feat,
                                  const CSeq_align* align)
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
    /// exceptions here are easy:
    /// we compare the annotated product to the conceptual translation and
    /// report problems
    ///
    bool has_start         = false;
    bool has_stop          = false;
    bool has_internal_stop = false;
    bool has_mismatches    = false;
    bool has_gap           = false;
    bool has_indel         = false;

    string xlate;

    if (align != NULL) {
        CBioseq bioseq;
        x_CollectMrnaSequence(bioseq.SetInst(), *align, feat.GetLocation(), false, &has_gap, &has_indel);
        CSeqVector seq(bioseq, m_scope.GetPointer(),
                       CBioseq_Handle::eCoding_Iupac);
        string mrna;
        int frame = 0;
        if (feat.GetData().GetCdregion().IsSetFrame()) {
            switch (feat.GetData().GetCdregion().GetFrame()) {
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
        seq.GetSeqData(frame, seq.size(), mrna);
        CSeqTranslator::Translate(mrna, xlate, CSeqTranslator::fIs5PrimePartial);
    } else {
        CSeqVector seq(feat.GetLocation(), *m_scope, CBioseq_Handle::eCoding_Iupac);
        CSeqTranslator::Translate(feat, *m_scope, xlate);
    }

    // deal with code breaks
    // NB: these should be folded into the translation machinery instead...
    if (feat.GetData().GetCdregion().IsSetCode_break()) {
        CSeq_loc_Mapper mapper(feat, CSeq_loc_Mapper::eLocationToProduct);
        ITERATE (CCdregion::TCode_break, it,
                 feat.GetData().GetCdregion().GetCode_break()) {
            CRef<CSeq_loc> mapped = mapper.Map((*it)->GetLoc());
            if (mapped) {
                TSeqRange r = mapped->GetTotalRange();
                if (r.GetLength() == 1  &&  r.GetFrom() < xlate.size()) {
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
                        xlate[r.GetFrom()] = dst[0];
                    }
                }
            }
        }
    }

    if (xlate.size()  &&  xlate[xlate.size() - 1] == '*') {
        /// strip a terminal stop
        xlate.resize(xlate.size() - 1);
        has_stop = true;
    }
    else if (feat.GetLocation().IsPartialStop(eExtreme_Biological)) {
        has_stop = true;
    } else {
        has_stop = false;
    }

    if ( (xlate.size()  &&  xlate[0] == 'M')  ||
         feat.GetLocation().IsPartialStart(eExtreme_Biological) ) {
        has_start = true;
    }

    string actual;
    CSeqVector vec(feat.GetProduct(), *m_scope,
                   CBioseq_Handle::eCoding_Iupac);
    vec.GetSeqData(0, vec.size(), actual);

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
        if (*it1 != *it2) {
            has_mismatches = true;
        }
        if (*it2 == '*') {
            has_internal_stop = true;
        }
        if (*it2 == '-') {
            has_gap = true;
        }
    }

    string except_text;

    if (actual.size() != xlate.size()  ||
        has_internal_stop  ||  !has_stop  ||  !has_start  ||
        has_gap || has_indel) {
        except_text = "unclassified translation discrepancy";
    }
    else if (has_mismatches) {
        except_text = "mismatches in translation";
    }

    x_SetExceptText(feat, except_text);
}

void CFeatureGenerator::SImplementation::x_SetExceptText(
      CSeq_feat& feat, const string &text)
{
    string except_text = text;

    // there are some exceptions we don't account for
    // we would like to set the exception state to whatever we compute above
    // on the other hand, an annotated exception such as ribosomal slippage or
    // rearrangement required for assembly trumps any computed values
    // scan to see if it is set to an exception state we can account for
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
            else if (*it == "ribosomal slippage") {
                except_text.clear();
                ++it;
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


void CFeatureGenerator::SImplementation::SetFeatureExceptions(CSeq_feat& feat,
                                      const CSeq_align* align)
{
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
        x_HandleRnaExceptions(feat, align);
        break;

    case CSeqFeatData::e_Cdregion:
        x_HandleCdsExceptions(feat, align);
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
            x_HandleRnaExceptions(feat, align);
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }
}



END_NCBI_SCOPE
