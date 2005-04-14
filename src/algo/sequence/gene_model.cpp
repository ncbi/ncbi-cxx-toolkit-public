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
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
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
                                          objects::CScope& scope,
                                          objects::CSeq_annot& annot,
                                          TGeneModelCreateFlags flags)
{
    /// first, merge the alignment
    /// this is necessary as the input may be (will likely be)
    /// a discontinuous segment
    CAlnMix mix(scope);
    mix.Add(align);
    mix.Merge(CAlnMix::fTryOtherMethodOnFail |
              CAlnMix::fGapJoin);

    /// make sure we only have two rows
    /// anything else represents a mixed-strand case or more than
    /// two sequences
    if (mix.GetDenseg().GetIds().size() != 2) {
        NCBI_THROW(CException, eUnknown,
            "CreateGeneModelFromAlign(): failed to create consistent alignment");
    }

    /// we use CAlnMap, not CAlnVec, to avoid some overhead
    /// in the object manager
    CAlnMap alnmgr(mix.GetDenseg());

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
        if (info->IsSetBiomol()  &&  info->GetBiomol() == CMolInfo::eBiomol_genomic) {
            target_id.Reset(new CSeq_id);
            target_id->Assign(alnmgr.GetSeqId(i));
            target_row = i;
            break;
        }
    }

    if ( !target_id ) {
        NCBI_THROW(CException, eUnknown,
            "CreateGeneModelFromAlign(): failed to find genomic sequence");
    }

    /// anchor on the genomic sequence
    alnmgr.SetAnchor(target_row);
    TSignedSeqRange range(alnmgr.GetAlnStart(), alnmgr.GetAlnStop());

    /// now, for each row, create a feature
    for (int row = 0;  row < alnmgr.GetNumRows();  ++row) {
        if (row == target_row) {
            continue;
        }
        CBioseq_Handle handle = scope.GetBioseqHandle(alnmgr.GetSeqId(row));
        CSeq_loc_Mapper mapper(align, *target_id, &scope);

        /// use the strand as reported by the alignment manager
        /// if a sequence has mixed-strand alignments, each will
        /// appear as a separate row
        /// thus, we will not mix strands in our features
        ENa_strand strand = eNa_strand_plus;
        if (alnmgr.IsNegativeStrand(row)) {
            strand = eNa_strand_minus;
        }

        ///
        /// create our mRNA feature
        ///
        CRef<CSeq_feat> mrna_feat(new CSeq_feat());
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

        /**
        /// generate our location
        CRef<CAlnVec::CAlnChunkVec> aln_chunks
                (alnmgr.GetAlnChunks(row, range,
                                    CAlnVec::fSeqOnly |
                                    CAlnVec::fChunkSameAsSeg));
        CPacked_seqint::TRanges ranges;
        for (int i = 0;  i < aln_chunks->size();  ++i) {
            CConstRef<CAlnVec::CAlnChunk> chunk((*aln_chunks)[i]);
            TSeqPos start = chunk->GetRange().GetFrom();
            start = alnmgr.GetSeqPosFromSeqPos(target_row, row, start);

            TSeqPos stop = chunk->GetRange().GetTo();
            stop = alnmgr.GetSeqPosFromSeqPos(target_row, row, stop);

            ranges.push_back(TSeqRange(start, stop));
        }
        CRef<CSeq_loc> loc(new CSeq_loc(*target_id, ranges));
        **/

        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().Assign(alnmgr.GetSeqId(row));
        loc = mapper.Map(*loc);
        mrna_feat->SetLocation(*loc);
        mrna_feat->SetProduct().SetWhole().Assign(alnmgr.GetSeqId(row));

        ///
        /// create our gene feature
        ///
        if (flags & fCreateGene) {
            CRef<CSeq_feat> gene_feat;
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

        if (flags & fCreateCdregion) {
            //
            // only create a gene feature if one exists on the mRNA feature
            //
            CFeat_CI feat_iter(handle, CSeqFeatData::eSubtype_cdregion);
            if (feat_iter  &&  feat_iter.GetSize()) {
                CRef<CSeq_feat> cds_feat(new CSeq_feat());
                cds_feat->Assign(feat_iter->GetOriginalFeature());
                CRef<CSeq_loc> new_loc =
                    mapper.Map(feat_iter->GetLocation());
                cds_feat->SetLocation(*new_loc);
                annot.SetData().SetFtable().push_back(cds_feat);
            }
        }

        ///
        /// copy additional features
        ///
        if (flags & fPromoteAllFeatures) {
            SAnnotSelector sel;
            sel.SetResolveAll()
                .SetAdaptiveDepth(true)
                .SetAnnotType(CSeq_annot::TData::e_Ftable)
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/04/14 16:48:38  dicuccio
 * Initial revision of CGeneModel
 *
 * ===========================================================================
 */
