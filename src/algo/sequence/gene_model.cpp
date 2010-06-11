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
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Splice_site.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Object_id.hpp>




BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

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


void CGeneModel::CreateGeneModelFromAlign(const objects::CSeq_align& align,
                                          CScope& scope,
                                          CSeq_annot& annot,
                                          CBioseq_set& seqs,
                                          TGeneModelCreateFlags flags,
                                          TSeqPos allowed_unaligned)
{
    struct SMapper
    {
    public:

        SMapper(const CSeq_align& aln, CScope& scope,
                TSeqPos allowed_unaligned = 10,
                CSeq_loc_Mapper::TMapOptions opts = 0)
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

        const CSeq_loc& GetRnaLoc()
        {
            if(rna_loc.IsNull()) {
                if(m_aln.GetSegs().IsSpliced()) {
                    rna_loc = x_GetLocFromSplicedExons(m_aln);
                } else {
                    const CSeq_id& id = m_aln.GetSeq_id(GetRnaRow());
                    CBioseq_Handle handle = m_scope.GetBioseqHandle(id);
                    CRef<CSeq_loc> range_loc =
                        handle.GetRangeSeq_loc(0, 0, eNa_strand_plus); //0-0 meanns whole range
                    //todo: truncate the range loc not to include polyA, or else the remapped loc will be erroneously partial
                    //not a huge issue as it only applies to seg alignments only.
                    rna_loc = m_mapper->Map(*range_loc);
                }
            }
            return *rna_loc;
        }

        CSeq_align::TDim GetGenomicRow() const
        {
            return m_genomic_row;
        }

        CSeq_align::TDim GetRnaRow() const
        {
            //we checked that alignment contains exactly 2 rows
            return GetGenomicRow() == 0 ? 1 : 0;
        }

        CRef<CSeq_loc> Map(const CSeq_loc& loc)
        {
            CRef<CSeq_loc> mapped_loc  = m_mapper->Map(loc);
            return mapped_loc;
        }

        void IncludeSourceLocs(bool b = true)
        {
            if (m_mapper) {
                m_mapper->IncludeSourceLocs(b);
            }
        }

        void SetMergeNone()
        {
            if (m_mapper) {
                m_mapper->SetMergeNone();
            }
        }

    private:

        /// This has special logic to set partialness based on alignment
        /// properties
        /// In addition, we need to interpret partial exons correctly
        CRef<CSeq_loc> x_GetLocFromSplicedExons(const CSeq_align& aln) const
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
                    if(prev_exon->GetProduct_end().GetNucpos() + 1 != exon.GetProduct_start().GetNucpos()) {
                        // gap between exons on rna. But which exon is partial?
                        // if have non-strict consensus splice site - blame it
                        // for partialness. If can't disambiguate on this - set
                        // both.
                        bool donor_ok =
                            (prev_exon->IsSetDonor_after_exon()  &&
                             prev_exon->GetDonor_after_exon().GetBases() == "GT");
                        bool acceptor_ok =
                            (exon.IsSetAcceptor_before_exon()  &&
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
        const CSeq_align& m_aln;
        CScope& m_scope;
        CRef<CSeq_loc_Mapper> m_mapper;
        CSeq_align::TDim m_genomic_row;
        CRef<CSeq_loc> rna_loc;
        TSeqPos m_allowed_unaligned;
    };



    ////////////////////////////////////////////////////////////////////////////
    CSeq_loc_Mapper::TMapOptions opts = 0;
    if (flags & fDensegAsExon) {
        opts |= CSeq_loc_Mapper::fAlign_Dense_seg_TotalRange;
    }

    SMapper mapper(align, scope, allowed_unaligned, opts);

    /// now, for each row, create a feature
    CTime time(CTime::eCurrent);


    const CSeq_id& rna_id = align.GetSeq_id(mapper.GetRnaRow());
    const CSeq_id& genomic_id = align.GetSeq_id(mapper.GetGenomicRow());
    CBioseq_Handle handle = scope.GetBioseqHandle(rna_id);

    /// we always need the mRNA location as a reference
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->Assign(mapper.GetRnaLoc());

    static CAtomicCounter counter;
    size_t model_num = counter.Add(1);

    ///
    /// create our mRNA feature
    ///
    CRef<CSeq_feat> mrna_feat;
    if (flags & fCreateMrna) {
        mrna_feat.Reset(new CSeq_feat());
        const CMolInfo* info = s_GetMolInfo(handle);
        CRNA_ref::TType type = CRNA_ref::eType_unknown;
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
            default:
                type = CRNA_ref::eType_other;
                break;
            }
        }
        mrna_feat->SetData().SetRna().SetType(type);
        mrna_feat->SetData().SetRna().SetExt()
            .SetName(sequence::GetTitle(handle));

        mrna_feat->SetLocation(*loc);
        if (loc->IsPartialStart(eExtreme_Positional)  ||
            loc->IsPartialStop(eExtreme_Positional)) {
            mrna_feat->SetPartial(true);
        }

        if (flags & fForceTranscribeMrna) {
            /// create a new bioseq for this mRNA
            CRef<CSeq_entry> entry(new CSeq_entry);
            CBioseq& bioseq = entry->SetSeq();

            /// create a new seq-id for this
            string str("lcl|CDNA_");
            str += time.AsString("YMD");
            str += "_";
            str += NStr::IntToString(model_num);
            CRef<CSeq_id> id(new CSeq_id(str));
            bioseq.SetId().push_back(id);
            mrna_feat->SetProduct().SetWhole().Assign(*id);

            /// set up the inst
            CSeq_inst& inst = bioseq.SetInst();
            inst.SetRepr(CSeq_inst::eRepr_raw);
            inst.SetMol(CSeq_inst::eMol_na);

            /// this is created as a translation of the genomic
            /// location
            CSeqVector vec(*loc, scope);
            vec.GetSeqData(0, vec.size(),
                           inst.SetSeq_data().SetIupacna().Set());
            inst.SetLength(inst.GetSeq_data().GetIupacna().Get().size());
            CSeqportUtil::Pack(&inst.SetSeq_data());

            seqs.SetSeq_set().push_back(entry);
        } else {
            mrna_feat->SetProduct().SetWhole().Assign(rna_id);
        }
    }

    ///
    /// create our gene feature
    ///
    CRef<CSeq_feat> gene_feat;
    if (flags & fCreateGene) {
        if (flags & fPropagateOnly) {
            //
            // only create a gene feature if one exists on the mRNA feature
            //
            CFeat_CI feat_iter(handle, CSeqFeatData::eSubtype_gene);
            if (feat_iter  &&  feat_iter.GetSize()) {
                gene_feat.Reset(new CSeq_feat());
                gene_feat->Assign(feat_iter->GetOriginalFeature());
                CRef<CSeq_loc> new_loc =
                    mapper.Map(feat_iter->GetLocation());
                CSeq_loc& gene_loc = gene_feat->SetLocation();
                gene_loc.SetInt().SetFrom(new_loc->GetTotalRange().GetFrom());
                gene_loc.SetInt().SetTo  (new_loc->GetTotalRange().GetTo());
                gene_loc.SetStrand(sequence::GetStrand(*new_loc, &scope));
                gene_loc.SetId(sequence::GetId(*new_loc, &scope));

                if (mrna_feat  &&
                    !mrna_feat->IsSetDbxref()  &&
                    gene_feat->IsSetDbxref()) {
                    ITERATE (CSeq_feat::TDbxref, xref_it,
                             gene_feat->GetDbxref()) {
                        CRef<CDbtag> tag(new CDbtag);
                        tag->Assign(**xref_it);
                        mrna_feat->SetDbxref().push_back(tag);
                    }
                }
            }
        } else {
            //
            // always create a gene feature
            //
            gene_feat.Reset(new CSeq_feat());
            gene_feat->SetData().SetGene()
                .SetLocus(sequence::GetTitle(handle));
            gene_feat->SetLocation().SetInt()
                .SetFrom(loc->GetTotalRange().GetFrom());
            gene_feat->SetLocation().SetInt()
                .SetTo  (loc->GetTotalRange().GetTo());
            gene_feat->SetLocation().SetId(genomic_id);
            gene_feat->SetLocation()
                .SetStrand(sequence::GetStrand(*loc));
        }

        if (gene_feat) {
            annot.SetData().SetFtable().push_back(gene_feat);
        }
    }

    if (mrna_feat) {
        SetFeatureExceptions(*mrna_feat, scope, &align);
        /// NOTE: added after gene!
        annot.SetData().SetFtable().push_back(mrna_feat);
    }

    CRef<CSeq_feat> cds_feat;
    if (flags & fCreateCdregion) {
        //
        // only create a CDS feature if one exists on the mRNA feature
        //
        CFeat_CI feat_iter(handle, CSeqFeatData::eSubtype_cdregion);
        if (feat_iter  &&  feat_iter.GetSize()) {
            cds_feat.Reset(new CSeq_feat());
            cds_feat->Assign(feat_iter->GetOriginalFeature());

            /// from this point on, we will get complex locations back
            SMapper mapper(align, scope, opts);
            mapper.IncludeSourceLocs();
            mapper.SetMergeNone();

            CRef<CSeq_loc> source_loc;

            ///
            /// mapping is complex
            /// we try to map each segment of the CDS
            /// in general, there will be only one; there are some corner cases
            /// (OAZ1, OAZ2, PAZ3, PEG10) in which there are more than one
            /// segment.
            ///
            CRef<CSeq_loc> cds_loc;
            for (CSeq_loc_CI loc_it(feat_iter->GetLocation());
                 loc_it;  ++loc_it) {
                /// location for this interval
                CSeq_loc this_loc;
                this_loc.SetInt().SetFrom(loc_it.GetRange().GetFrom());
                this_loc.SetInt().SetTo(loc_it.GetRange().GetTo());
                this_loc.SetInt().SetStrand(loc_it.GetStrand());
                this_loc.SetInt().SetId().Assign
                    (*loc_it.GetSeq_id_Handle().GetSeqId());

                /// map it
                CRef<CSeq_loc> equiv = mapper.Map(this_loc);
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

                CRef<CSeq_loc> this_loc_source = equiv->GetEquiv().Get().back();
                if ( !source_loc ) {
                    source_loc.Reset(new CSeq_loc);
                }
                source_loc->SetMix().Set().push_back(this_loc_source);

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

                if (is_partial_5prime) {
                    this_loc_mapped->SetPartialStart(true, eExtreme_Biological);
                }
                if (is_partial_3prime) {
                    this_loc_mapped->SetPartialStop(true, eExtreme_Biological);
                }

                if ( !cds_loc ) {
                    cds_loc.Reset(new CSeq_loc);
                }
                cds_loc->SetMix().Set().push_back(this_loc_mapped);
            }
            if (cds_loc) {
                cds_loc->ChangeToPackedInt();
            }

            if (cds_loc  &&
                cds_loc->Which() != CSeq_loc::e_not_set) {

                bool is_partial_5prime =
                    cds_loc->IsPartialStart(eExtreme_Biological);
                bool is_partial_3prime =
                    cds_loc->IsPartialStop(eExtreme_Biological);

                cds_loc->SetId(*loc->GetId());
                cds_feat->SetLocation(*cds_loc);

                if (is_partial_5prime  ||  is_partial_3prime) {
                    cds_feat->SetPartial(true);
                }

                /// make sure we set the CDS frame correctly
                /// if we're 5' partial, we may need to adjust the frame
                /// to ensure that conceptual translations are in-frame
                if (is_partial_5prime) {
                    TSeqRange orig_range = 
                        feat_iter->GetOriginalFeature().GetLocation().GetTotalRange();
                    TSeqRange mappable_range = 
                        source_loc->GetTotalRange();

                    TSeqPos offs = mappable_range.GetFrom() - orig_range.GetFrom();
                    if (offs) {
                        int orig_frame = 0;
                        if (cds_feat->GetData().GetCdregion().IsSetFrame()) {
                            orig_frame = cds_feat->GetData()
                                .GetCdregion().GetFrame();
                            if (orig_frame) {
                                orig_frame -= 1;
                            }
                        }
                        int frame = (offs - orig_frame) % 3;
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
                }


                /// copy any existing dbxrefs
                if (cds_feat  &&  gene_feat  &&
                    !cds_feat->IsSetDbxref()  &&  gene_feat->IsSetDbxref()) {
                    ITERATE (CSeq_feat::TDbxref, xref_it,
                             gene_feat->GetDbxref()) {
                        CRef<CDbtag> tag(new CDbtag);
                        tag->Assign(**xref_it);
                        cds_feat->SetDbxref().push_back(tag);
                    }
                }

                /// also copy the code break if it exists
                if (cds_feat->GetData().GetCdregion().IsSetCode_break()) {
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

                        if (new_cb_loc  &&  !new_cb_loc->IsNull()) {
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

                SetFeatureExceptions(*cds_feat, scope, &align);
                annot.SetData().SetFtable().push_back(cds_feat);

                if (flags & fForceTranslateCds) {
                    /// create a new bioseq for the CDS
                    CRef<CSeq_entry> entry(new CSeq_entry);
                    CBioseq& bioseq = entry->SetSeq();

                    /// create a new seq-id for this
                    string str("lcl|PROT_");
                    str += time.AsString("YMD");
                    str += "_";
                    str += NStr::IntToString(model_num);
                    CRef<CSeq_id> id(new CSeq_id(str));
                    bioseq.SetId().push_back(id);
                    cds_feat->SetProduct().SetWhole().Assign(*id);

                    /// set up the inst
                    CSeq_inst& inst = bioseq.SetInst();
                    inst.SetRepr(CSeq_inst::eRepr_raw);
                    inst.SetMol(CSeq_inst::eMol_aa);

                    /// this is created as a translation of the genomic
                    /// location
                    CSeqTranslator::Translate
                        (*cds_loc, handle.GetScope(),
                         inst.SetSeq_data().SetIupacaa().Set(),
                         NULL /* default genetic code */,
                         false /* trim at first stop codon */);
                    inst.SetLength(inst.GetSeq_data().GetIupacaa().Get().size());

                    seqs.SetSeq_set().push_back(entry);
                }
            }
        }
    }

    ///
    /// partial flags may require a global analysis - we may need to mark some
    /// locations partial even if they are not yet partial
    ///
    if (mrna_feat  &&  cds_feat  &&
        cds_feat->IsSetPartial()  &&  cds_feat->GetPartial()) {
        mrna_feat->SetPartial(true);

        /// in addition to marking the mrna feature partial, we must mark the
        /// location partial to match the partialness in the CDS
        CSeq_loc& cds_loc = cds_feat->SetLocation();
        CSeq_loc& mrna_loc = mrna_feat->SetLocation();
        if (cds_loc.IsPartialStart(eExtreme_Biological)) {
            mrna_loc.SetPartialStart(true, eExtreme_Biological);
        }
        if (cds_loc.IsPartialStop(eExtreme_Biological)) {
            mrna_loc.SetPartialStop(true, eExtreme_Biological);
        }
    }

    if (gene_feat  &&  mrna_feat  &&
        mrna_feat->IsSetPartial()  &&  mrna_feat->GetPartial()) {
        gene_feat->SetPartial(true);

        /// in addition to marking the gene feature partial, we must mark the
        /// location partial to match the partialness in the mRNA
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
    /// copy additional features
    ///
    if (flags & fPromoteAllFeatures) {
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
            annot.SetData().SetFtable().push_back(feat);
        }
    }
}


//////////////////////////////////////////////////////////////////////////////


///
/// Handle feature exceptions
///
static void s_HandleRnaExceptions(CSeq_feat& feat,
                                  CScope& scope,
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
            CBioseq_Handle bsh = scope.GetBioseqHandle(feat.GetLocation());
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
    if (align->GetSegs().IsSpliced()) {
        al.Reset(align);
    }
    if ( !al ) {
        SAnnotSelector sel;
        sel.SetResolveAll();
        CAlign_CI align_iter(scope, feat.GetLocation(), sel);
        for ( ;  align_iter;  ++align_iter) {
            const CSeq_align& this_align = *align_iter;
            if (this_align.GetSegs().IsSpliced()  &&
                sequence::IsSameBioseq
                (sequence::GetId(feat.GetProduct(), &scope),
                 this_align.GetSeq_id(0),
                 &scope)) {
                al.Reset(&this_align);
                break;
            }
        }
    }

    bool has_length_mismatch = false;
    bool has_5prime_unaligned = false;
    bool has_3prime_unaligned = false;
    bool has_polya_tail = false;
    bool has_mismatches = false;
    bool has_gaps = false;
    if (al) {
        ///
        /// can do full comparison
        ///

        /// we know we have a Spliced-seg
        /// evaluate for gaps or mismatches
        ITERATE (CSpliced_seg::TExons, exon_it,
                 al->GetSegs().GetSpliced().GetExons()) {
            const CSpliced_exon& exon = **exon_it;
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
            CBioseq_Handle prod_bsh = scope.GetBioseqHandle(feat.GetProduct());
            max_align_len = prod_bsh.GetBioseqLength();
        }

        if (r.GetTo() + 1 < max_align_len) {
            has_3prime_unaligned = true;
        }

        /// also note the poly-A
        if (al->GetSegs().GetSpliced().IsSetPoly_a()) {
            has_polya_tail = true;
        }

        /**
        LOG_POST(Error << "  spliced-seg:"
                 << " has_mismatches=" << (has_mismatches ? "yes" : "no")
                 << " has_gaps=" << (has_gaps ? "yes" : "no")
                 << " has_polya_tail=" << (has_polya_tail ? "yes" : "no")
                 << " has_5prime_unaligned=" << (has_5prime_unaligned ? "yes" : "no")
                 << " has_3prime_unaligned=" << (has_3prime_unaligned ? "yes" : "no"));
                 **/
    } else {
        /// only compare for mismatches and 3' unaligned
        /// we assume that the feature is otherwise aligned

        CBioseq_Handle prod_bsh    = scope.GetBioseqHandle(feat.GetProduct());
        CSeqVector nuc_vec(feat.GetLocation(), scope,
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
                break;
            }
        }

        size_t tail_len = prod_end - prod_it;
        size_t count_a = 0;
        for ( ;  prod_it != prod_end;  ++prod_it) {
            if (*prod_it == 'A') {
                ++count_a;
            }
        }

        if (count_a >= tail_len * 0.8) {
            has_polya_tail = true;
            tail_len -= count_a;
        }
        if (tail_len) {
            has_3prime_unaligned = true;
        }

        /**
        LOG_POST(Error << "  raw sequence:"
                 << " has_mismatches=" << (has_mismatches ? "yes" : "no")
                 << " has_gaps=" << (has_gaps ? "yes" : "no")
                 << " has_polya_tail=" << (has_polya_tail ? "yes" : "no")
                 << " has_5prime_unaligned=" << (has_5prime_unaligned ? "yes" : "no")
                 << " has_3prime_unaligned=" << (has_3prime_unaligned ? "yes" : "no"));
                 **/
    }

    string except_text;
    if (has_5prime_unaligned  ||  has_3prime_unaligned  ||
        has_gaps  ||  has_length_mismatch) {
        except_text = "unclassified transcription discrepancy";
    }
    else if (has_mismatches) {
        except_text = "mismatches in transcription";
    }

    /**
    LOG_POST(Error << "    existing flag: "
             << ((feat.IsSetExcept()  &&  feat.GetExcept()) ? "exception" : "no exception"));
    LOG_POST(Error << "    existing text: "
             << (feat.IsSetExcept_text() ? feat.GetExcept_text() : ""));
             **/

    if (except_text.empty()) {
        /**
        LOG_POST(Error << "    new flag:      no exception");
        LOG_POST(Error << "    new text:      ");
        **/

        if ( !feat.IsSetExcept()  ||  !feat.GetExcept()) {
            /// we simply make exception mark-up consistent
            feat.ResetExcept_text();
        }
    } else {
        /**
        LOG_POST(Error << "    new flag:      exception");
        LOG_POST(Error << "    new text:      " << except_text);
        **/

        /// corner case:
        /// our exception may already be set
        bool found = false;
        if (feat.IsSetExcept_text()) {
            list<string> toks;
            NStr::Split(feat.GetExcept_text(), ",", toks);
            NON_CONST_ITERATE (list<string>, it, toks) {
                NStr::TruncateSpacesInPlace(*it);
                if (*it == except_text) {
                    found = true;
                    break;
                }
            }

            if ( !found ) {
                except_text += ", ";
                except_text += feat.GetExcept_text();
            }
        }

        feat.SetExcept(true);
        if ( !found ) {
            feat.SetExcept_text(except_text);
        }
    }
}


static void s_HandleCdsExceptions(CSeq_feat& feat,
                                  CScope& scope,
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
            CBioseq_Handle bsh = scope.GetBioseqHandle(feat.GetLocation());
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
    bool has_stop          = false;
    bool has_internal_stop = false;
    bool has_mismatches    = false;
    bool has_gap           = false;

    string xlate;
    CSeqTranslator::Translate(feat, scope, xlate);
    if (xlate.size()  &&  xlate[xlate.size() - 1] == '*') {
        /// strip a terminal stop
        xlate.resize(xlate.size() - 1);
        has_stop = true;
    } else {
        has_stop = false;
    }

    string actual;
    CSeqVector vec(feat.GetProduct(), scope,
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
        has_internal_stop  ||  has_gap) {
        except_text = "unclassified translation discrepancy";
    }
    else if (has_mismatches) {
        except_text = "mismatches in translation";
    }

    /**
    if ((feat.IsSetExcept()  &&  feat.GetExcept()) != !except_text.empty()) {
        LOG_POST(Error << "    existing flag: "
                 << ((feat.IsSetExcept()  &&  feat.GetExcept()) ? "exception" : "no exception"));
        LOG_POST(Error << "    existing text: "
                 << (feat.IsSetExcept_text() ? feat.GetExcept_text() : ""));
        LOG_POST(Error << "    new flag:      "
                 << ( !except_text.empty() ? "exception" : "no exception"));
        LOG_POST(Error << "    new text:      " << except_text);

        LOG_POST(Error << "    has_internal_stop="
                 << (has_internal_stop ? "yes" : "no"));
        LOG_POST(Error << "    has_stop="
                 << (has_stop ? "yes" : "no"));
        LOG_POST(Error << "    has_gap="
                 << (has_gap ? "yes" : "no"));
        LOG_POST(Error << "    has_mismatches="
                 << (has_mismatches ? "yes" : "no"));
        LOG_POST(Error << "    size_match="
                 << ((actual.size() == xlate.size()) ? "yes" : "no"));
        LOG_POST(Error << "    actual: " << actual);
        LOG_POST(Error << "    xlate:  " << xlate);
    }
    **/

    /// there are some exceptions we don't account for
    /// we would like to set the exception state to whatever we compute above
    /// on the other hand, an annotated exception such as ribosomal slippage or
    /// rearrangement required for assembly trumps any computed values
    /// scan to see if it is set to an exception state we can account for
    if (except_text.empty()) {
        if ( !feat.IsSetExcept()  ||  !feat.GetExcept()) {
            /// we simply make exception mark-up consistent
            feat.ResetExcept_text();
        }
    } else {
        feat.SetExcept(true);

        /// corner case:
        /// our exception may already be set
        bool found = false;
        if (feat.IsSetExcept_text()) {
            list<string> toks;
            NStr::Split(feat.GetExcept_text(), ",", toks);
            NON_CONST_ITERATE (list<string>, it, toks) {
                NStr::TruncateSpacesInPlace(*it);
                if (*it == except_text) {
                    found = true;
                    break;
                }
            }

            if ( !found ) {
                except_text += ", ";
                except_text += feat.GetExcept_text();
            }
        }

        if ( !found ) {
            feat.SetExcept_text(except_text);
        }
    }
}


void CGeneModel::SetFeatureExceptions(CSeq_feat& feat,
                                      CScope& scope,
                                      const CSeq_align* align)
{
    // Exceptions identified are:
    //
    //   - unclassified transcription discrepancy
    //   - mismatches in transcription
    //   - unclassified translation discrepancy
    //   - mismatches in translation
    switch (feat.GetData().Which()) {
    case CSeqFeatData::e_Rna:
        s_HandleRnaExceptions(feat, scope, align);
        break;

    case CSeqFeatData::e_Cdregion:
        s_HandleCdsExceptions(feat, scope, align);
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
            s_HandleRnaExceptions(feat, scope, align);
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
