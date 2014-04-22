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
* Author: Colleen Bollin, NCBI
*
* File Description:
*   functions for editing and working with locations
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/edit/loc_edit.hpp>
#include <objtools/edit/cds_fix.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


bool s_Is5AtEndOfSeq(const CSeq_loc& loc, CBioseq_Handle bsh)
{
    bool rval = false;

    ENa_strand strand = loc.GetStrand();
    if (strand == eNa_strand_minus) {
        if (loc.GetStart(eExtreme_Biological) == bsh.GetInst_Length() - 1) {
            rval = true;
        }
    } else {
        if (loc.GetStart(eExtreme_Biological) == 0) {
            rval = true;
        }
    }
    return rval;
}


bool CLocationEditPolicy::Interpret5Policy
(const CSeq_feat& orig_feat, 
 CScope& scope, 
 bool& do_set_5_partial, 
 bool& do_clear_5_partial) const
{
    do_set_5_partial = false;
    do_clear_5_partial = false;
    
    CBioseq_Handle bsh = scope.GetBioseqHandle(orig_feat.GetLocation());

    switch (m_PartialPolicy5) {
        case ePartialPolicy_eNoChange:
            // do nothing
            break;
        case ePartialPolicy_eSet:
            if (!orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)) {
                do_set_5_partial = true;
            } else if (m_Extend5 && !s_Is5AtEndOfSeq(orig_feat.GetLocation(), bsh)) {
                do_set_5_partial = true;
            }
            break;
        case ePartialPolicy_eSetAtEnd:
            if (s_Is5AtEndOfSeq(orig_feat.GetLocation(), bsh) 
                && !orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)) {
                do_set_5_partial = true;
            }
            break;
        case ePartialPolicy_eSetForBadEnd:
            if (!orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()) {
                string transl_prot;
                try {
                    CSeqTranslator::Translate(orig_feat, scope, transl_prot,
                                                false,   // do not include stop codons
                                                false);  // do not remove trailing X/B/Z

                } catch ( const runtime_error& ) {
                }
                if (!NStr::StartsWith(transl_prot, "M", NStr::eNocase)) {
                    do_set_5_partial = true;
                }
            }
            break;
        case ePartialPolicy_eSetForFrame:
            if (!orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()
                && orig_feat.GetData().GetCdregion().IsSetFrame()
                && orig_feat.GetData().GetCdregion().GetFrame() != CCdregion::eFrame_not_set
                && orig_feat.GetData().GetCdregion().GetFrame() != CCdregion::eFrame_one) {
                do_set_5_partial = true;
            }
            break;
        case ePartialPolicy_eClear:
            if (orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)) {
                do_clear_5_partial = true;
            }
            break;
        case ePartialPolicy_eClearNotAtEnd:
            if (orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)
                && !s_Is5AtEndOfSeq(orig_feat.GetLocation(), bsh)) {
                do_clear_5_partial = true;
            }
            break;
        case ePartialPolicy_eClearForGoodEnd:
            if (orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()) {
                string transl_prot;
                try {
                    CSeqTranslator::Translate(orig_feat, scope, transl_prot,
                                                false,   // do not include stop codons
                                                false);  // do not remove trailing X/B/Z

                } catch ( const runtime_error& ) {
                }
                if (NStr::StartsWith(transl_prot, "M", NStr::eNocase)) {
                    do_clear_5_partial = true;
                }
            }
            break;
    }
    return do_set_5_partial || do_clear_5_partial;
}


bool s_Is3AtEndOfSeq(const CSeq_loc& loc, CBioseq_Handle bsh)
{
    bool rval = false;
    ENa_strand strand = loc.GetStrand();

    if (strand == eNa_strand_minus) {
        if (loc.GetStop(eExtreme_Biological) == 0) {
            rval = true;
        }
    } else {
        if (loc.GetStop(eExtreme_Biological) == bsh.GetInst_Length() - 1) {                        
            rval = true;
        }
    }
    return rval;
}


bool CLocationEditPolicy::Interpret3Policy
(const CSeq_feat& orig_feat,
 CScope& scope,
 bool& do_set_3_partial,
 bool& do_clear_3_partial) const
{
    do_set_3_partial = false;
    do_clear_3_partial = false;
    
    CBioseq_Handle bsh = scope.GetBioseqHandle(orig_feat.GetLocation());

    switch (m_PartialPolicy3) {
        case ePartialPolicy_eNoChange:
            // do nothing
            break;
        case ePartialPolicy_eSet:
            if (!orig_feat.GetLocation().IsPartialStop(eExtreme_Biological)) {
                do_set_3_partial = true;
            } else if (m_Extend3 && !s_Is3AtEndOfSeq(orig_feat.GetLocation(), bsh)) {
                do_set_3_partial = true;
            }
            break;
        case ePartialPolicy_eSetAtEnd:
            if (!orig_feat.GetLocation().IsPartialStop(eExtreme_Biological) 
                && s_Is3AtEndOfSeq(orig_feat.GetLocation(), bsh)) {
                do_set_3_partial = true;
            }
            break;
        case ePartialPolicy_eSetForBadEnd:
            if (!orig_feat.GetLocation().IsPartialStop(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()) {
                string transl_prot;
                try {
                    CSeqTranslator::Translate(orig_feat, scope, transl_prot,
                                                true,   // include stop codons
                                                false);  // do not remove trailing X/B/Z

                } catch ( const runtime_error& ) {
                }
                if (!NStr::EndsWith(transl_prot, "*", NStr::eNocase)) {
                    do_set_3_partial = true;
                }
            }
            break;
        case ePartialPolicy_eSetForFrame:
            // not allowed for 3' end
            break;
        case ePartialPolicy_eClear:
            if (orig_feat.GetLocation().IsPartialStop(eExtreme_Biological)) {
                do_clear_3_partial = true;
            }
            break;
        case ePartialPolicy_eClearNotAtEnd:
            if (orig_feat.GetLocation().IsPartialStop(eExtreme_Biological)
                && !s_Is3AtEndOfSeq(orig_feat.GetLocation(), bsh)) {
                do_clear_3_partial = true;
            }
            break;
        case ePartialPolicy_eClearForGoodEnd:
            if (orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()) {
                string transl_prot;
                try {
                    CSeqTranslator::Translate(orig_feat, scope, transl_prot,
                                                true,   // include stop codons
                                                false);  // do not remove trailing X/B/Z

                } catch ( const runtime_error& ) {
                }
                if (NStr::EndsWith(transl_prot, "*", NStr::eNocase)) {
                    do_clear_3_partial = true;
                }
            }
            break;
    }
    return do_set_3_partial || do_clear_3_partial;
}


CRef<CSeq_loc> SeqLocExtend(const CSeq_loc& loc, size_t pos, CScope* scope)
{
    size_t loc_start = loc.GetStart(eExtreme_Positional);
    size_t loc_stop = loc.GetStop(eExtreme_Positional);
    bool partial_start = loc.IsPartialStart(eExtreme_Positional);
    bool partial_stop = loc.IsPartialStop(eExtreme_Positional);
    ENa_strand strand = loc.GetStrand();
    CRef<CSeq_loc> new_loc(NULL);

    if (pos < loc_start) {
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*(loc.GetId()));
        CRef<CSeq_loc> add(new CSeq_loc(*id, pos, loc_start - 1, strand));
        add->SetPartialStart(partial_start, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, scope);
    } else if (pos > loc_stop) {
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*(loc.GetId()));
        CRef<CSeq_loc> add(new CSeq_loc(*id, loc_stop + 1, pos, strand));
        add->SetPartialStop(partial_stop, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, scope);
    }
    return new_loc;
}


bool CLocationEditPolicy::ApplyPolicyToFeature(CSeq_feat& feat, CScope& scope) const
{
    if (m_PartialPolicy5 == ePartialPolicy_eNoChange
        && m_PartialPolicy3 == ePartialPolicy_eNoChange
        && m_MergePolicy == eMergePolicy_NoChange) {
        return false;
    }

    bool any_change = false;

    // make changes to 5' end
    bool do_set_5_partial = false;
    bool do_clear_5_partial = false;
    any_change |= Interpret5Policy(feat, scope, do_set_5_partial, do_clear_5_partial);
    if (do_set_5_partial) {
        feat.SetLocation().SetPartialStart(true, eExtreme_Biological);
        if (m_Extend5) {
            // extend end
            CBioseq_Handle bsh = scope.GetBioseqHandle(feat.GetLocation());
            ENa_strand strand = feat.GetLocation().GetStrand();
            size_t start = feat.GetLocation().GetStart(eExtreme_Biological);
            int diff = 0;
            if (strand == eNa_strand_minus) {                
                if (start < bsh.GetInst_Length() - 1) {
                    diff = bsh.GetInst_Length() - feat.GetLocation().GetStart(eExtreme_Biological) - 1;
                    CRef<CSeq_loc> new_loc = SeqLocExtend(feat.GetLocation(), bsh.GetInst_Length() - 1, &scope);
                    if (new_loc) {
                        feat.SetLocation().Assign(*new_loc);
                    } else {
                        diff = 0;
                    }
                }
            } else  if (start > 0) {
                diff = start;
                CRef<CSeq_loc> new_loc = SeqLocExtend(feat.GetLocation(), 0, &scope);
                if (new_loc) {
                    feat.SetLocation().Assign(*new_loc);
                } else {
                    diff = 0;
                }
            }
            // adjust frame to maintain consistency
            if (diff % 3 > 0 && feat.GetData().IsCdregion()) {
                int orig_frame = 0;
                if (feat.GetData().GetCdregion().IsSetFrame()) {
                    if (feat.GetData().GetCdregion().GetFrame() == CCdregion::eFrame_two) {
                        orig_frame = 1;
                    } else if (feat.GetData().GetCdregion().GetFrame() == CCdregion::eFrame_three) {
                        orig_frame = 2;
                    }
                }
                CCdregion::EFrame new_frame = CCdregion::eFrame_not_set;
                switch ((3 + orig_frame - diff % 3) % 3) {
                    case 1:
                        new_frame = CCdregion::eFrame_two;
                        break;
                    case 2:
                        new_frame = CCdregion::eFrame_three;
                        break;
                    default:
                        new_frame = CCdregion::eFrame_not_set;
                        break;
                }
                feat.SetData().SetCdregion().SetFrame(new_frame);
            }
        }
    } else if (do_clear_5_partial) {
        feat.SetLocation().SetPartialStart(false, eExtreme_Biological);
    }

    // make changes to 3' end
    bool do_set_3_partial = false;
    bool do_clear_3_partial = false;
    any_change |= Interpret3Policy(feat, scope, do_set_3_partial, do_clear_3_partial);
    if (do_set_3_partial) {
        feat.SetLocation().SetPartialStop(true, eExtreme_Biological);
        if (m_Extend3) {
            CBioseq_Handle bsh = scope.GetBioseqHandle(feat.GetLocation());
            ENa_strand strand = feat.GetLocation().GetStrand();
            size_t stop = feat.GetLocation().GetStop(eExtreme_Biological);
            if (strand == eNa_strand_minus) {                
                if (stop > 0) {
                    CRef<CSeq_loc> new_loc = SeqLocExtend(feat.GetLocation(), 0, &scope);
                    if (new_loc) {
                        feat.SetLocation().Assign(*new_loc);
                    }
                }
            } else  if (stop < bsh.GetInst_Length() - 1) {
                CRef<CSeq_loc> new_loc = SeqLocExtend(feat.GetLocation(), bsh.GetInst_Length() - 1, &scope);
                if (new_loc) {
                    feat.SetLocation().Assign(*new_loc);
                }
            }
        }
    } else if (do_clear_3_partial) {
        feat.SetLocation().SetPartialStop(false, eExtreme_Biological);
    }

    // make merge changes
    switch (m_MergePolicy) {
        case CLocationEditPolicy::eMergePolicy_Join:
            {
                // remove NULLS between, if present
                bool changed = false;
                CRef<CSeq_loc> new_loc = ConvertToJoin(feat.GetLocation(), changed);
                if (changed) {
                    feat.SetLocation().Assign(*new_loc);
                    any_change = true;
                }                
            }
            break;
        case CLocationEditPolicy::eMergePolicy_Order:
            {
                // add NULLS between if not present
                bool changed = false;
                CRef<CSeq_loc> new_loc = ConvertToOrder(feat.GetLocation(), changed);
                if (changed) {
                    feat.SetLocation().Assign(*new_loc);
                    any_change = true;
                } 
            }
            break;
        case CLocationEditPolicy::eMergePolicy_SingleInterval:
            {
                CRef<CSeq_loc> new_loc = sequence::Seq_loc_Merge(feat.GetLocation(), CSeq_loc::fMerge_SingleRange, &scope);
                if (sequence::Compare(*new_loc, feat.GetLocation(), &scope) != sequence::eSame) {
                    feat.SetLocation().Assign(*new_loc);
                    any_change = true;
                }
            }
            break;
        case CLocationEditPolicy::eMergePolicy_NoChange:
            break;
    }

    any_change |= AdjustFeaturePartialFlagForLocation(feat);

    return any_change;
}


bool CLocationEditPolicy::HasNulls(const CSeq_loc& orig_loc)
{
    if (orig_loc.Which() == CSeq_loc::e_Mix) {
        ITERATE(CSeq_loc_mix::Tdata, it, orig_loc.GetMix().Get()) {
            if ((*it)->IsNull()) {
                return true;
            }
        }
    }
    return false;
}


CRef<CSeq_loc> CLocationEditPolicy::ConvertToJoin(const CSeq_loc& orig_loc, bool &changed)
{
    changed = false;
    CRef<CSeq_loc> new_loc(new CSeq_loc());
    if (!HasNulls(orig_loc)) {
        new_loc->Assign(orig_loc);
    } else {
        CSeq_loc_CI ci(orig_loc);
        new_loc->SetMix();
        while (ci) {
            CConstRef<CSeq_loc> subloc = ci.GetRangeAsSeq_loc();
            if (subloc && !subloc->IsNull()) {
                CRef<CSeq_loc> add(new CSeq_loc());
                add->Assign(*subloc);
                new_loc->SetMix().Set().push_back(add);
            }
            ++ci;
        }
        changed = true;
    }
    return new_loc;
}


CRef<CSeq_loc> CLocationEditPolicy::ConvertToOrder(const CSeq_loc& orig_loc, bool &changed)
{
    changed = false;
    CRef<CSeq_loc> new_loc(new CSeq_loc());
    if (HasNulls(orig_loc)) {
        new_loc->Assign(orig_loc);
        return new_loc;
    }
    switch(orig_loc.Which()) {
        case CSeq_loc::e_not_set:
        case CSeq_loc::e_Null:
        case CSeq_loc::e_Empty:
        case CSeq_loc::e_Whole:
        case CSeq_loc::e_Int:
        case CSeq_loc::e_Pnt:
        case CSeq_loc::e_Bond:
        case CSeq_loc::e_Feat:
        case CSeq_loc::e_Equiv:
            new_loc->Assign(orig_loc);
            break;
        case CSeq_loc::e_Packed_int:
        case CSeq_loc::e_Packed_pnt:
        case CSeq_loc::e_Mix:
            {
                new_loc->SetMix();
                CSeq_loc_CI ci(orig_loc);
                CRef<CSeq_loc> first(new CSeq_loc());
                first->Assign(*(ci.GetRangeAsSeq_loc()));
                new_loc->SetMix().Set().push_back(first);
                ++ci;
                while (ci) {
                    CRef<CSeq_loc> null_loc(new CSeq_loc());
                    null_loc->SetNull();
                    new_loc->SetMix().Set().push_back(null_loc);
                    CRef<CSeq_loc> add(new CSeq_loc());
                    add->Assign(*(ci.GetRangeAsSeq_loc()));
                    new_loc->SetMix().Set().push_back(add);
                    ++ci;
                }
                changed = true;
            }
            break;

    }
    return new_loc;
}


bool ApplyPolicyToFeature(const CLocationEditPolicy& policy, const CSeq_feat& orig_feat, 
    CScope& scope, bool adjust_gene, bool retranslate_cds)
{
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(orig_feat);

    bool any_change = policy.ApplyPolicyToFeature(*new_feat, scope);
    
    if (any_change) {
        CSeq_feat_Handle fh = scope.GetSeq_featHandle(orig_feat);
        // This is necessary, to make sure that we are in "editing mode"
        const CSeq_annot_Handle& annot_handle = fh.GetAnnot();
        CSeq_entry_EditHandle eh = annot_handle.GetParentEntry().GetEditHandle();
        CSeq_feat_EditHandle feh(fh);

        // adjust gene feature
        if (adjust_gene) {
            CConstRef<CSeq_feat> old_gene = sequence::GetOverlappingGene(orig_feat.GetLocation(), scope);
            if (old_gene) {
                size_t feat_start = new_feat->GetLocation().GetStart(eExtreme_Biological);
                size_t feat_stop = new_feat->GetLocation().GetStop(eExtreme_Biological);
                CRef<CSeq_feat> new_gene(new CSeq_feat());
                new_gene->Assign(*old_gene);
                bool gene_change = false;
                // adjust ends of gene to match ends of feature
                CRef<CSeq_loc> new_loc = SeqLocExtend(new_gene->GetLocation(), feat_start, &scope);
                if (new_loc) {
                    new_gene->SetLocation().Assign(*new_loc);
                    gene_change = true;
                }
                new_loc = SeqLocExtend(new_gene->GetLocation(), feat_stop, &scope);
                if (new_loc) {
                    new_gene->SetLocation().Assign(*new_loc);
                    gene_change = true;
                }
                if (gene_change) {
                    CSeq_feat_Handle gh = scope.GetSeq_featHandle(*old_gene);
                    // This is necessary, to make sure that we are in "editing mode"
                    const CSeq_annot_Handle& ah = gh.GetAnnot();
                    CSeq_entry_EditHandle egh = ah.GetParentEntry().GetEditHandle();
                    CSeq_feat_EditHandle geh(gh);
                    geh.Replace(*new_gene);                    
                }
            }
        }
        feh.Replace(*new_feat);

        // retranslate or resynch if coding region
        if (new_feat->IsSetProduct() && new_feat->GetData().IsCdregion()) {
            if (retranslate_cds) {
                RetranslateCDS(*new_feat, scope);
            } else {
                AdjustForCDSPartials(*new_feat, scope.GetBioseqHandle(new_feat->GetLocation()).GetSeq_entry_Handle());
            }
        }
    }
    return any_change;
}




END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

