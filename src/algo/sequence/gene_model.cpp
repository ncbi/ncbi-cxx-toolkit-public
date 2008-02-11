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

#include <objects/general/Dbtag.hpp>
#include <objects/seqalign/Seq_align.hpp>
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
#include <objtools/alnmgr/alnmix.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objtools/alnmgr/alnvec.hpp>


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
    /// first, merge the alignment
    /// this is necessary as the input may be (will likely be)
    /// a discontinuous segment
    CConstRef<CDense_seg> ds;
    if (align.GetSegs().IsDenseg()) {
        ds.Reset(&align.GetSegs().GetDenseg());
    } else {
        CAlnMix mix(scope);
        mix.Add(align);
        mix.Merge(CAlnMix::fGapJoin);

        ds.Reset(&mix.GetDenseg());
    }

    /// make sure we only have two rows
    /// anything else represents a mixed-strand case or more than
    /// two sequences
    if (ds->GetIds().size() != 2) {
        NCBI_THROW(CException, eUnknown,
            "CreateGeneModelFromAlign(): failed to create consistent alignment");
    }

    /// we use CAlnMap, not CAlnVec, to avoid some overhead
    /// from the object manager
    CAlnMap alnmgr(*ds);

    /// there should ideally be one genomic sequence; find it, and record
    /// both ID and row
    CRef<CSeq_id> target_id;
    size_t target_row = 0;
    for (int i = 0;  i < alnmgr.GetNumRows();  ++i) {
        CBioseq_Handle handle = scope.GetBioseqHandle(alnmgr.GetSeqId(i));
        if ( !handle ) {
            continue;
        }

        const CMolInfo* info = s_GetMolInfo(handle);
        if (info  &&  info->IsSetBiomol()  &&
            info->GetBiomol() == CMolInfo::eBiomol_genomic) {
            target_id.Reset(new CSeq_id);
            target_id->Assign(alnmgr.GetSeqId(i));
            target_row = i;
            break;
        }
    }

    if ( !target_id ) {
        TSeqPos max_len = 0;
        for (int i = 0;  i < alnmgr.GetNumRows();  ++i) {
            CBioseq_Handle handle = scope.GetBioseqHandle(alnmgr.GetSeqId(i));
            if ( !handle ) {
                continue;
            }

            if (handle.GetBioseqLength() < max_len) {
                continue;
            }

            switch (handle.GetInst_Mol()) {
            case CSeq_inst::eMol_na:
            case CSeq_inst::eMol_dna:
                target_id.Reset(new CSeq_id);
                target_id->Assign(alnmgr.GetSeqId(i));
                target_row = i;
                break;

            default:
                break;
            }
        }
    }

    if ( !target_id ) {
        NCBI_THROW(CException, eUnknown,
            "CreateGeneModelFromAlign(): failed to find genomic sequence");
    }

    /// anchor on the genomic sequence
    alnmgr.SetAnchor(target_row);
    TSignedSeqRange range(alnmgr.GetAlnStart(), alnmgr.GetAlnStop());

    CSeq_loc_Mapper::TMapOptions opts = 0;
    if (flags & fDensegAsExon) {
        opts |= CSeq_loc_Mapper::fAlign_Dense_seg_TotalRange;
    }

    /// now, for each row, create a feature
    for (int row = 0;  row < alnmgr.GetNumRows();  ++row) {
        if (row == target_row) {
            continue;
        }
        CBioseq_Handle handle = scope.GetBioseqHandle(alnmgr.GetSeqId(row));
        CSeq_loc_Mapper mapper(align, *target_id, &scope, opts);

        /// use the strand as reported by the alignment manager
        /// if a sequence has mixed-strand alignments, each will
        /// appear as a separate row
        /// thus, we will not mix strands in our features
        ENa_strand strand = eNa_strand_plus;
        if (alnmgr.IsNegativeStrand(row)) {
            strand = eNa_strand_minus;
        }

        /// we always need the mRNA location as a reference
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().Assign(alnmgr.GetSeqId(row));
        loc = mapper.Map(*loc);

        static CAtomicCounter counter;
        size_t model_num = counter.Add(1);
        CTime time(CTime::eCurrent);

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
                mrna_feat->SetProduct().SetWhole().Assign(alnmgr.GetSeqId(row));
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
                        ITERATE (CSeq_feat::TDbxref, xref_it, gene_feat->GetDbxref()) {
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
                gene_feat->SetLocation().SetId(*target_id);
                gene_feat->SetLocation()
                    .SetStrand(sequence::GetStrand(*loc));
            }

            if (gene_feat) {
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
                CRef<CSeq_loc> new_loc =
                    mapper.Map(feat_iter->GetLocation());
                cds_feat->SetLocation(*new_loc);

                /// copy any existing dbxrefs
                if (cds_feat  &&
                    !cds_feat->IsSetDbxref()  &&
                    gene_feat->IsSetDbxref()) {
                    ITERATE (CSeq_feat::TDbxref, xref_it, gene_feat->GetDbxref()) {
                        CRef<CDbtag> tag(new CDbtag);
                        tag->Assign(**xref_it);
                        cds_feat->SetDbxref().push_back(tag);
                    }
                }

                /// also copy the code break if it exists
                if (cds_feat->GetData().GetCdregion().IsSetCode_break()) {
                    CSeqFeatData::TCdregion& cds = cds_feat->SetData().SetCdregion();
                    NON_CONST_ITERATE(CCdregion::TCode_break, it, cds.SetCode_break()) {
                        CRef<CSeq_loc> new_cb_loc = mapper.Map((*it)->GetLoc());
                        (*it)->SetLoc(*new_cb_loc);
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
                        (*new_loc, handle, inst.SetSeq_data().SetIupacaa().Set(),
                         NULL /* default genetic code */,
                         false /* trim at first stop codon */);
                    inst.SetLength(inst.GetSeq_data().GetIupacaa().Get().size());

                    seqs.SetSeq_set().push_back(entry);
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
}


END_NCBI_SCOPE
