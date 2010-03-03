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
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Splice_site.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqloc/Seq_id.hpp>




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
                                          TGeneModelCreateFlags flags)
{
    struct SMapper
    {
    public:

        SMapper(const CSeq_align& aln, CScope& scope,
                CSeq_loc_Mapper::TMapOptions opts = 0)
            : m_aln(aln), m_scope(scope)
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

    private:

        /// This has special logic to set partialness based on alignment
        /// properties
        CRef<CSeq_loc> x_GetLocFromSplicedExons(const CSeq_align& aln) const
        {
            CRef<CSeq_loc> loc(new CSeq_loc);
            CConstRef<CSpliced_exon> prev_exon;
            CRef<CSeq_interval> prev_int;
            ITERATE(CSpliced_seg::TExons, it, aln.GetSegs().GetSpliced().GetExons()) {
                const CSpliced_exon& exon = **it;
                CRef<CSeq_interval> genomic_int(new CSeq_interval);

                genomic_int->SetId().Assign(aln.GetSeq_id(1));
                genomic_int->SetFrom(exon.GetGenomic_start());
                genomic_int->SetTo(exon.GetGenomic_end());
                genomic_int->SetStrand(
                        exon.IsSetGenomic_strand() ? exon.GetGenomic_strand()
                      : aln.GetSegs().GetSpliced().IsSetGenomic_strand() ? aln.GetSegs().GetSpliced().GetGenomic_strand()
                      : eNa_strand_plus);

                // check for gaps between exons
                if(!prev_exon.IsNull()) {
                    if(prev_exon->GetProduct_end().GetNucpos() + 1 != exon.GetProduct_start().GetNucpos()) {
                        // gap between exons on rna. But which exon is partial?
                        // if have non-strict consensus splice site - blame it
                        // for partialness. If can't disambiguate on this - set
                        // both.
                        bool donor_ok = prev_exon->GetDonor_after_exon().GetBases() == "GT";
                        bool acceptor_ok = exon.GetAcceptor_before_exon().GetBases() == "AG";
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
            if(m_aln.GetSeqStart(0) > 0) {
                loc->SetPartialStart(true, eExtreme_Biological);
            }

            TSeqPos product_len = aln.GetSegs().GetSpliced().GetProduct_length();
            TSeqPos polya_pos = aln.GetSegs().GetSpliced().CanGetPoly_a() ? aln.GetSegs().GetSpliced().GetPoly_a() : product_len;

            if(m_aln.GetSeqStop(0) + 1 < polya_pos) {
                loc->SetPartialStop(true, eExtreme_Biological);
            }
            return loc;

        }
        const CSeq_align& m_aln;
        CScope& m_scope;
        CRef<CSeq_loc_Mapper> m_mapper;
        CSeq_align::TDim m_genomic_row;
        CRef<CSeq_loc> rna_loc;
    };



    ////////////////////////////////////////////////////////////////////////////
    CSeq_loc_Mapper::TMapOptions opts = 0;
    if (flags & fDensegAsExon) {
        opts |= CSeq_loc_Mapper::fAlign_Dense_seg_TotalRange;
    }

    SMapper mapper(align, scope, opts);


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
            if (mrna_feat->IsSetPartial()  &&  mrna_feat->GetPartial()) {
                gene_feat->SetPartial(true);
            }
            annot.SetData().SetFtable().push_back(gene_feat);
        }
    }

    if (mrna_feat) {
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

            ///
            /// mapping is complex
            /// we try to map each segment of the CDS
            /// in general, there will be only one; there are some corner cases
            /// (OAZ1, OAZ2, PAZ3, PEG10) in which there are more than one
            /// segment.
            ///
            CRef<CSeq_loc> cds_loc(new CSeq_loc);
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
                CRef<CSeq_loc> this_loc_mapped =
                    mapper.Map(this_loc);

                /// we take the extreme bounds of the interval only;
                /// internal details will be recomputed based on intersection
                /// with the mRNA location
                CSeq_loc sub;
                sub.SetInt().SetFrom(this_loc_mapped->GetTotalRange().GetFrom());
                sub.SetInt().SetTo(this_loc_mapped->GetTotalRange().GetTo());
                sub.SetInt().SetStrand(this_loc_mapped->GetStrand());
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
                cds_loc->SetMix().Set().push_back(this_loc_mapped);
            }
            cds_loc->ChangeToPackedInt();

            bool is_partial_5prime =
                cds_loc->IsPartialStart(eExtreme_Biological);
            bool is_partial_3prime =
                cds_loc->IsPartialStop(eExtreme_Biological);

            if (cds_loc->Which() != CSeq_loc::e_not_set) {
                cds_loc->SetId(*loc->GetId());
                cds_feat->SetLocation(*cds_loc);

                if (is_partial_5prime  ||  is_partial_3prime) {
                    cds_feat->SetPartial(true);
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
                        CRef<CSeq_loc> new_cb_loc = mapper.Map((*it)->GetLoc());
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


END_NCBI_SCOPE
