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
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

string PrintBestSeqId(const CSeq_id& sid, CScope& scope) 
{
   string best_id(kEmptyStr);

   // Best seq_id
   CSeq_id_Handle sid_hl = sequence::GetId(sid, scope, sequence::eGetId_Best);
   if (sid_hl) {
      CConstRef <CSeq_id> new_id = sid_hl.GetSeqId();
      if (new_id) {
        best_id = sid_hl.GetSeqId()->AsFastaString();
      }
   }
   else best_id = sid.AsFastaString();

   return best_id;
};

static const string strand_symbol[] = {"", "", "c","b", "r"};
string PrintSeqIntUseBestID(const CSeq_interval& seq_int, CScope& scope, bool range_only)
{
    string location(kEmptyStr);

    // Best seq_id
    if (!range_only) {
      location = PrintBestSeqId(seq_int.GetId(), scope) + ":";
    }

    // strand
    ENa_strand 
      this_strand 
         = (seq_int.CanGetStrand()) ? seq_int.GetStrand(): eNa_strand_unknown;
    location += strand_symbol[(int)this_strand];
    int from, to;
    string lab_from(kEmptyStr), lab_to(kEmptyStr);
    if (eNa_strand_minus == this_strand 
                               || eNa_strand_both_rev ==  this_strand) {
           to = seq_int.GetFrom();
           from = seq_int.GetTo();
           if (seq_int.CanGetFuzz_to()) {
               const CInt_fuzz& f_from = seq_int.GetFuzz_to();
               f_from.GetLabel(&lab_from, from, false); 
           }
           else lab_from = NStr::IntToString(++from);
           if (seq_int.CanGetFuzz_from()) {
               const CInt_fuzz& f_to = seq_int.GetFuzz_from();
               f_to.GetLabel(&lab_to, to); 
           }
           else lab_to = NStr::IntToString(++to);
    }
    else {
           to = seq_int.GetTo();
           from = seq_int.GetFrom();
           if (seq_int.CanGetFuzz_from()) {
                const CInt_fuzz& f_from = seq_int.GetFuzz_from();
                f_from.GetLabel(&lab_from, from, false);
           }
           else lab_from = NStr::IntToString(++from);
           if (seq_int.CanGetFuzz_to()) {
               const CInt_fuzz& f_to = seq_int.GetFuzz_to();
               f_to.GetLabel(&lab_to, to);
           }
           else lab_to = NStr::IntToString(++to);
    }
    location += lab_from + "-" + lab_to; 
    return location;
};

string PrintPntAndPntsUseBestID(const CSeq_loc& seq_loc, CScope& scope, bool range_only)
{
  string location(kEmptyStr);

   // Best seq_id
  if (!range_only) {
     if (seq_loc.IsPnt()) {
       location = PrintBestSeqId(seq_loc.GetPnt().GetId(), scope) + ":";
     }
     else if (seq_loc.IsPacked_pnt()) {
       location = PrintBestSeqId(seq_loc.GetPacked_pnt().GetId(), scope) + ":";
     }
  }

  if (!location.empty()) {
     string strtmp;
     seq_loc.GetLabel(&strtmp);
     location += strtmp.substr(strtmp.find(":")+1);
  }
  return location;
}
    
string SeqLocPrintUseBestID(const CSeq_loc& seq_loc, CScope& scope, bool range_only)
{
  string location(kEmptyStr);
  if (seq_loc.IsInt()) {
     location = PrintSeqIntUseBestID(seq_loc.GetInt(), scope, range_only);
  } 
  else if (seq_loc.IsMix() || seq_loc.IsEquiv()) {
     location = "(";
     const list <CRef <CSeq_loc> >* seq_loc_ls; 
     if (seq_loc.IsMix()) {
        seq_loc_ls = &(seq_loc.GetMix().Get());
     }
     else {
        seq_loc_ls = &(seq_loc.GetEquiv().Get());
     }
     ITERATE (list <CRef <CSeq_loc> >, it, *seq_loc_ls) {
        if (it == seq_loc.GetMix().Get().begin()) {
           location += SeqLocPrintUseBestID(**it, scope, range_only);
        }
        else location += SeqLocPrintUseBestID(**it, scope, true);
        location += ", ";
     }
     if (!location.empty()) {
        location = location.substr(0, location.size()-2);
     }
     location += ")"; 
  }
  else if (seq_loc.IsPacked_int()) {
     location = "(";
     ITERATE (list <CRef <CSeq_interval> >, it, seq_loc.GetPacked_int().Get()) {
        if (it == seq_loc.GetPacked_int().Get().begin()) {
           location += PrintSeqIntUseBestID(**it, scope, range_only);
        }
        else {
            location += PrintSeqIntUseBestID(**it, scope, true);
        }
        location += ", ";
     }
     if (!location.empty()) {
        location = location.substr(0, location.size()-2);
     }
     location += ")";
  }
  else if (seq_loc.IsPnt() || seq_loc.IsPacked_pnt()) {
     location = PrintPntAndPntsUseBestID(seq_loc, scope, range_only);
  }
  else if (seq_loc.IsBond()) {
     CSeq_loc tmp_loc;
     tmp_loc.SetBond().Assign(seq_loc.GetBond().GetA());
     location = PrintPntAndPntsUseBestID(tmp_loc, scope, range_only);
     if (seq_loc.GetBond().CanGetB()) {
       tmp_loc.SetBond().Assign(seq_loc.GetBond().GetB());
       location 
          += "=" + PrintPntAndPntsUseBestID(tmp_loc, scope, range_only);
     } 
  }
  else {
    seq_loc.GetLabel(&location);
  }
  return location;
}


bool s_Is5AtEndOfSeq(const CSeq_loc& loc, CBioseq_Handle bsh)
{
    bool rval = false;

    ENa_strand strand = loc.GetStrand();
    if (strand == eNa_strand_minus) {
        if (bsh && loc.GetStart(eExtreme_Biological) == bsh.GetInst_Length() - 1) {
            rval = true;
        }
    } else {
        if (loc.GetStart(eExtreme_Biological) == 0) {
            rval = true;
        }
    }
    return rval;
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
        if (bsh && loc.GetStop(eExtreme_Biological) == bsh.GetInst_Length() - 1) {                        
            rval = true;
        }
    }
    return rval;
}


bool s_Know5WithoutBsh(const CSeq_loc& loc)
{
    ENa_strand strand = loc.GetStrand();

    if (strand == eNa_strand_minus) {
        return false;
    } else {
        return true;
    }
}


bool s_Know3WithoutBsh(const CSeq_loc& loc)
{
    ENa_strand strand = loc.GetStrand();

    if (strand == eNa_strand_minus) {
        return true;
    } else {
        return false;
    }
}


bool CLocationEditPolicy::Interpret5Policy
(const CSeq_feat& orig_feat, 
 CScope& scope, 
 bool& do_set_5_partial, 
 bool& do_clear_5_partial) const
{
    do_set_5_partial = false;
    do_clear_5_partial = false;
    const CSeq_loc& loc = orig_feat.GetLocation();
    CBioseq_Handle bsh = scope.GetBioseqHandle(loc);

    switch (m_PartialPolicy5) {
        case ePartialPolicy_eNoChange:
            // do nothing
            break;
        case ePartialPolicy_eSet:
            if (!orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)) {
                do_set_5_partial = true;
            } else if (m_Extend5 && !s_Is5AtEndOfSeq(loc, bsh)
                       && (bsh || s_Know5WithoutBsh(loc))) {
                do_set_5_partial = true;
            }
            break;
        case ePartialPolicy_eSetAtEnd:
            if (s_Is5AtEndOfSeq(loc, bsh) 
                && !orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)
                && (bsh || s_Know5WithoutBsh(loc))) {
                do_set_5_partial = true;
            }
            break;
        case ePartialPolicy_eSetForBadEnd:
            if (!orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()
                && bsh) {
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
                && !s_Is5AtEndOfSeq(orig_feat.GetLocation(), bsh)
                && (bsh || s_Know5WithoutBsh(loc))) {
                do_clear_5_partial = true;
            }
            break;
        case ePartialPolicy_eClearForGoodEnd:
            if (orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()
                && bsh) {
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


bool CLocationEditPolicy::Interpret3Policy
(const CSeq_feat& orig_feat,
 CScope& scope,
 bool& do_set_3_partial,
 bool& do_clear_3_partial) const
{
    do_set_3_partial = false;
    do_clear_3_partial = false;
    
    const CSeq_loc& loc = orig_feat.GetLocation();
    CBioseq_Handle bsh = scope.GetBioseqHandle(loc);

    switch (m_PartialPolicy3) {
        case ePartialPolicy_eNoChange:
            // do nothing
            break;
        case ePartialPolicy_eSet:
            if (!orig_feat.GetLocation().IsPartialStop(eExtreme_Biological)) {
                do_set_3_partial = true;
            } else if (m_Extend3 && !s_Is3AtEndOfSeq(loc, bsh) && (bsh || s_Know3WithoutBsh(loc))) {
                do_set_3_partial = true;
            }
            break;
        case ePartialPolicy_eSetAtEnd:
            if (!orig_feat.GetLocation().IsPartialStop(eExtreme_Biological) 
                && s_Is3AtEndOfSeq(orig_feat.GetLocation(), bsh)
                && (bsh || s_Know3WithoutBsh(loc))) {
                do_set_3_partial = true;
            }
            break;
        case ePartialPolicy_eSetForBadEnd:
            if (!orig_feat.GetLocation().IsPartialStop(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()
                && bsh) {
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
                && !s_Is3AtEndOfSeq(orig_feat.GetLocation(), bsh)
                && (bsh || s_Know3WithoutBsh(loc))) {
                do_clear_3_partial = true;
            }
            break;
        case ePartialPolicy_eClearForGoodEnd:
            if (orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()
                && bsh) {
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
            if (bsh || s_Know5WithoutBsh(feat.GetLocation())) {
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
            if (bsh || s_Know3WithoutBsh(feat.GetLocation())) {
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
            if (!retranslate_cds || !RetranslateCDS(*new_feat, scope)) {
                AdjustForCDSPartials(*new_feat, scope.GetBioseqHandle(new_feat->GetLocation()).GetSeq_entry_Handle());
            }
        }
    }
    return any_change;
}


void ReverseComplementLocation(CSeq_interval& interval, CScope& scope)
{
    // flip strand
    interval.FlipStrand();
    if (interval.IsSetId()) {
        CBioseq_Handle bsh = scope.GetBioseqHandle(interval.GetId());
        if (bsh) {
            if (interval.IsSetFrom()) {
                interval.SetFrom(bsh.GetInst_Length() - interval.GetFrom() - 1);
            }
            if (interval.IsSetTo()) {
                interval.SetTo(bsh.GetInst_Length() - interval.GetTo() - 1);
            }

            // reverse from and to
            if (interval.IsSetFrom()) {
                TSeqPos from = interval.GetFrom();
                if (interval.IsSetTo()) {
                    interval.SetFrom(interval.GetTo());
                } else {
                    interval.ResetFrom();
                }
                interval.SetTo(from);
            } else if (interval.IsSetTo()) {
                interval.SetFrom(interval.GetTo());
                interval.ResetTo();
            }

            if (interval.IsSetFuzz_from()) {
                interval.SetFuzz_from().Negate(bsh.GetInst_Length());
            }
            if (interval.IsSetFuzz_to()) {
                interval.SetFuzz_to().Negate(bsh.GetInst_Length());
            }

            // swap fuzz
            if (interval.IsSetFuzz_from()) {
                CRef<CInt_fuzz> swap(new CInt_fuzz());
                swap->Assign(interval.GetFuzz_from());
                if (interval.IsSetFuzz_to()) {
                    interval.SetFuzz_from().Assign(interval.GetFuzz_to());
                } else {
                    interval.ResetFuzz_from();
                }
                interval.SetFuzz_to(*swap);
            } else if (interval.IsSetFuzz_to()) {
                interval.SetFuzz_from().Assign(interval.GetFuzz_to());
                interval.ResetFuzz_to();
            }
        }
    }
}


void ReverseComplementLocation(CSeq_point& pnt, CScope& scope)
{
    // flip strand
    pnt.FlipStrand();
    if (pnt.IsSetId()) {
        CBioseq_Handle bsh = scope.GetBioseqHandle(pnt.GetId());
        if (bsh) {
            if (pnt.IsSetPoint()) {
                pnt.SetPoint(bsh.GetInst_Length() - pnt.GetPoint() - 1);
            }
            if (pnt.IsSetFuzz()) {
                pnt.SetFuzz().Negate(bsh.GetInst_Length());
            }
        }
    }
}


void ReverseComplementLocation(CPacked_seqpnt& ppnt, CScope& scope)
{
    // flip strand
    ppnt.FlipStrand();
    CBioseq_Handle bsh = scope.GetBioseqHandle(ppnt.GetId());
    if (bsh) {
        // flip fuzz
        if (ppnt.IsSetFuzz()) {
            ppnt.SetFuzz().Negate(bsh.GetInst_Length());
        }
        //complement points
        if (ppnt.IsSetPoints()) {
            vector<int> new_pnts;
            ITERATE(CPacked_seqpnt::TPoints, it, ppnt.SetPoints()) {
                new_pnts.push_back(bsh.GetInst_Length() - *it - 1);
            }
            ppnt.ResetPoints();
            ITERATE(vector<int>, it, new_pnts) {
                ppnt.SetPoints().push_back(*it);
            }
        }
    }

}


void ReverseComplementLocation(CSeq_loc& loc, CScope& scope)
{
    switch (loc.Which()) {
        case CSeq_loc::e_Empty:
        case CSeq_loc::e_Whole:
        case CSeq_loc::e_Feat:
        case CSeq_loc::e_Null:
        case CSeq_loc::e_not_set:
            // do nothing
            break;
        case CSeq_loc::e_Int:
            ReverseComplementLocation(loc.SetInt(), scope);
            loc.InvalidateCache();
            break;
        case CSeq_loc::e_Pnt:
            ReverseComplementLocation(loc.SetPnt(), scope);
            loc.InvalidateCache();
            break;
        case CSeq_loc::e_Bond:
            if (loc.GetBond().IsSetA()) {
                ReverseComplementLocation(loc.SetBond().SetA(), scope);
            }
            if (loc.GetBond().IsSetB()) {
                ReverseComplementLocation(loc.SetBond().SetB(), scope);
            }
            loc.InvalidateCache();
            break;
        case CSeq_loc::e_Mix:
            // revcomp individual elements
            NON_CONST_ITERATE(CSeq_loc_mix::Tdata, it, loc.SetMix().Set()) {
                ReverseComplementLocation(**it, scope);
            }
            loc.InvalidateCache();
            break;
        case CSeq_loc::e_Equiv:
            // revcomp individual elements
            NON_CONST_ITERATE(CSeq_loc_equiv::Tdata, it, loc.SetEquiv().Set()) {
                ReverseComplementLocation(**it, scope);
            }
            loc.InvalidateCache();
            break;
        case CSeq_loc::e_Packed_int:
            // revcomp individual elements
            NON_CONST_ITERATE(CPacked_seqint::Tdata, it, loc.SetPacked_int().Set()) {
                ReverseComplementLocation(**it, scope);
            }
            loc.InvalidateCache();
            break;
        case CSeq_loc::e_Packed_pnt:
            ReverseComplementLocation(loc.SetPacked_pnt(), scope);
            loc.InvalidateCache();
            break;

    }
}


void ReverseComplementCDRegion(CCdregion& cdr, CScope& scope)
{
    if (cdr.IsSetCode_break()) {
        NON_CONST_ITERATE(CCdregion::TCode_break, it, cdr.SetCode_break()) {
            if ((*it)->IsSetLoc()) {
                ReverseComplementLocation((*it)->SetLoc(), scope);
            }
        }
    }
}


void ReverseComplementTrna(CTrna_ext& trna, CScope& scope)
{
    if (trna.IsSetAnticodon()) {
        ReverseComplementLocation(trna.SetAnticodon(), scope);
    }
}


void ReverseComplementFeature(CSeq_feat& feat, CScope& scope)
{
    if (feat.IsSetLocation()) {
        ReverseComplementLocation(feat.SetLocation(), scope);
    }
    if (feat.IsSetData()) {
        switch (feat.GetData().GetSubtype()) {
            case CSeqFeatData::eSubtype_cdregion:
                ReverseComplementCDRegion(feat.SetData().SetCdregion(), scope);
                break;
            case CSeqFeatData::eSubtype_tRNA:
                ReverseComplementTrna(feat.SetData().SetRna().SetExt().SetTRNA(), scope);
                break;
            default:
                break;
        }
    }
}


void x_SeqIntervalDelete(CRef<CSeq_interval> interval, 
                         TSeqPos cut_from, TSeqPos cut_to,
                         const CSeq_id* seqid,
                         bool& bCompleteCut,
                         bool& bTrimmed)
{
    // These are required fields
    if ( !(interval->CanGetFrom() && interval->CanGetTo()) )
    {
        return;
    }

    // Feature location
    TSeqPos feat_from = interval->GetFrom();
    TSeqPos feat_to = interval->GetTo();

    // Size of the cut
    TSeqPos cut_size = cut_to - cut_from + 1;

    // Case 1: feature is located completely before the cut
    if (feat_to < cut_from)
    {
        // Nothing needs to be done - cut does not affect feature
        return;
    }

    // Case 2: feature is completely within the cut
    if (feat_from >= cut_from && feat_to <= cut_to)
    {
        // Feature should be deleted
        bCompleteCut = true;
        return;
    }

    // Case 3: feature is completely past the cut
    if (feat_from > cut_to)
    {
        // Shift the feature by the cut_size
        feat_from -= cut_size;
        feat_to -= cut_size;
        interval->SetFrom(feat_from);
        interval->SetTo(feat_to);
        bTrimmed = true;
        return;
    }

    /***************************************************************************
     * Cases below are partial overlapping cases
    ***************************************************************************/
    // Case 4: Cut is completely inside the feature 
    //         OR
    //         Cut is to the "left" side of the feature (i.e., feat_from is 
    //         inside the cut)
    //         OR
    //         Cut is to the "right" side of the feature (i.e., feat_to is 
    //         inside the cut)
    if (feat_to > cut_to) {
        // Left side cut or cut is completely inside feature
        feat_to -= cut_size;
    }
    else {
        // Right side cut
        feat_to = cut_from - 1;
    }

    // Take care of the feat_from from the left side cut case
    if (feat_from >= cut_from) {
        feat_from = cut_to + 1;
        feat_from -= cut_size;
    }

    interval->SetFrom(feat_from);
    interval->SetTo(feat_to);
    bTrimmed = true;
}


void SeqLocAdjustForTrim(CSeq_loc& loc, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid,
                bool& bCompleteCut,
                bool& bTrimmed)
{
    // Given a seqloc and a range, cut the seqloc

    switch(loc.Which())
    {
        // Single interval
        case CSeq_loc::e_Int:
            {{
                CRef<CSeq_interval> interval(new CSeq_interval);
                interval->Assign(loc.GetInt());
                x_SeqIntervalDelete(interval, from, to, seqid, 
                                    bCompleteCut, bTrimmed);
                loc.SetInt(*interval);
            }}
            break;

        // Multiple intervals
        case CSeq_loc::e_Packed_int:
            {{
                CRef<CSeq_loc::TPacked_int> intervals(new CSeq_loc::TPacked_int);
                intervals->Assign(loc.GetPacked_int());
                if (intervals->CanGet()) {
                    // Process each interval in the list
                    CPacked_seqint::Tdata::iterator it;
                    for (it = intervals->Set().begin(); 
                         it != intervals->Set().end(); ) 
                    {
                        // Initial value: assume that all intervals 
                        // will be deleted resulting in bCompleteCut = true.
                        // Later on if any interval is not deleted, then set
                        // bCompleteCut = false
                        if (it == intervals->Set().begin()) {
                            bCompleteCut = true;
                        }

                        bool bDeleted = false;
                        x_SeqIntervalDelete(*it, from, to, seqid, 
                                            bDeleted, bTrimmed);

                        // Should interval be deleted from list?
                        if (bDeleted) {
                            it = intervals->Set().erase(it);
                        }
                        else {
                            ++it;
                            bCompleteCut = false;
                        }
                    }

                    // Update the original list
                    loc.SetPacked_int(*intervals);
                }
            }}
            break;

        // Multiple seqlocs
        case CSeq_loc::e_Mix:
            {{
                CRef<CSeq_loc_mix> mix(new CSeq_loc_mix);
                mix->Assign(loc.GetMix());
                if (mix->CanGet()) {
                    // Process each seqloc in the list
                    CSeq_loc_mix::Tdata::iterator it;
                    for (it = mix->Set().begin(); 
                         it != mix->Set().end(); ) 
                    {
                        // Initial value: assume that all seqlocs
                        // will be deleted resulting in bCompleteCut = true.
                        // Later on if any seqloc is not deleted, then set
                        // bCompleteCut = false
                        if (it == mix->Set().begin()) {
                            bCompleteCut = true;
                        }

                        bool bDeleted = false;
                        SeqLocAdjustForTrim(**it, from, to, seqid, bDeleted, bTrimmed);

                        // Should seqloc be deleted from list?
                        if (bDeleted) {
                            it = mix->Set().erase(it);
                        }
                        else {
                            ++it;
                            bCompleteCut = false;
                        }
                    }

                    // Update the original list
                    loc.SetMix(*mix);
                }
            }}
            break;
        case CSeq_loc::e_Pnt:
            if (to < loc.GetPnt().GetPoint()) {
                size_t diff = to - from + 1;
                loc.SetPnt().SetPoint(loc.GetPnt().GetPoint() - diff);
            } else if (from < loc.GetPnt().GetPoint()) {
                bCompleteCut = true;
            }
            break;

        // Other choices not supported yet 
        default:
        {           
        }
        break;
    }
}


void x_SeqIntervalInsert(CRef<CSeq_interval> interval, 
                         TSeqPos insert_from, TSeqPos insert_to,
                         const CSeq_id* seqid)
{
    // These are required fields
    if ( !(interval->CanGetFrom() && interval->CanGetTo()) )
    {
        return;
    }

    // Feature location
    TSeqPos feat_from = interval->GetFrom();
    TSeqPos feat_to = interval->GetTo();

    // Size of the insert
    TSeqPos insert_size = insert_to - insert_from + 1;

    // Case 1: feature is located before the insert
    if (feat_to < insert_from)
    {
        // Nothing needs to be done - cut does not affect feature
        return;
    }

    // Case 2: feature is located after the insert
    if (feat_from > insert_from) {
        feat_from += insert_size;
        feat_to += insert_size;
        interval->SetFrom(feat_from);
        interval->SetTo(feat_to);
        return;
    }

    // Case 3: insert occurs within interval
    if (feat_from <= insert_from && feat_to >= insert_from)
    {
        feat_to += insert_size;
        interval->SetTo(feat_to);
        return;
    }
}


void SeqLocAdjustForInsert(CSeq_loc& loc, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid)
{
    // Given a seqloc and a range, insert into the seqloc

    switch(loc.Which())
    {
        // Single interval
        case CSeq_loc::e_Int:
            {{
                CRef<CSeq_interval> interval(new CSeq_interval);
                interval->Assign(loc.GetInt());
                x_SeqIntervalInsert(interval, from, to, seqid);
                loc.SetInt(*interval);
            }}
            break;

        // Multiple intervals
        case CSeq_loc::e_Packed_int:
            {{
                CRef<CSeq_loc::TPacked_int> intervals(new CSeq_loc::TPacked_int);
                intervals->Assign(loc.GetPacked_int());
                if (intervals->CanGet()) {
                    // Process each interval in the list
                    CPacked_seqint::Tdata::iterator it;
                    for (it = intervals->Set().begin(); 
                         it != intervals->Set().end(); ) 
                    {
                        x_SeqIntervalInsert(*it, from, to, seqid);
                    }

                    // Update the original list
                    loc.SetPacked_int(*intervals);
                }
            }}
            break;

        // Multiple seqlocs
        case CSeq_loc::e_Mix:
            {{
                CRef<CSeq_loc_mix> mix(new CSeq_loc_mix);
                mix->Assign(loc.GetMix());
                if (mix->CanGet()) {
                    // Process each seqloc in the list
                    CSeq_loc_mix::Tdata::iterator it;
                    for (it = mix->Set().begin(); 
                         it != mix->Set().end(); ) 
                    {
                        SeqLocAdjustForInsert(**it, from, to, seqid);
                    }

                    // Update the original list
                    loc.SetMix(*mix);
                }
            }}
            break;
        case CSeq_loc::e_Pnt:
            if (to < loc.GetPnt().GetPoint()) {
                size_t diff = to - from + 1;
                loc.SetPnt().SetPoint(loc.GetPnt().GetPoint() + diff);
            }
            break;

        // Other choices not supported yet 
        default:
        {           
        }
        break;
    }
}


void CdregionAdjustForTrim(CCdregion& cdr,
                            TSeqPos from, TSeqPos to,
                            const CSeq_id* seqid)
{
    CCdregion::TCode_break::iterator it = cdr.SetCode_break().begin();
    while (it != cdr.SetCode_break().end()) {
        if ((*it)->IsSetLoc()) {
            bool cut = false;
            bool trimmed = false;
            SeqLocAdjustForTrim((*it)->SetLoc(), from, to, seqid, cut, trimmed);
            if (cut) {
                it = cdr.SetCode_break().erase(it);
            } else {
                it++;
            }
        } else {
            it++;
        }
    }
    if (cdr.GetCode_break().empty()) {
        cdr.ResetCode_break();
    }
}


void TrnaAdjustForTrim(CTrna_ext& trna,
                        TSeqPos from, TSeqPos to,
                        const CSeq_id* seqid)
{
    if (trna.IsSetAnticodon()) {
        bool cut = false;
        bool trimmed = false;
        SeqLocAdjustForTrim(trna.SetAnticodon(), from, to, seqid, cut, trimmed);
        if (cut) {
            trna.ResetAnticodon();
        } 
    }
}


void FeatureAdjustForTrim(CSeq_feat& feat, 
                            TSeqPos from, TSeqPos to,
                            const CSeq_id* seqid,
                            bool& bCompleteCut,
                            bool& bTrimmed)
{
    SeqLocAdjustForTrim (feat.SetLocation(), from, to, seqid, bCompleteCut, bTrimmed);
    if (bCompleteCut) {
        return;
    }

    if (feat.IsSetData()) {
        switch (feat.GetData().Which()) {
            case CSeqFeatData::eSubtype_cdregion:
                CdregionAdjustForTrim(feat.SetData().SetCdregion(), from, to, seqid);
                break;
            case CSeqFeatData::eSubtype_tRNA:
                TrnaAdjustForTrim(feat.SetData().SetRna().SetExt().SetTRNA(), from, to, seqid);
                break;
            default:
                break;
        }
    }
}


void CdregionAdjustForInsert(CCdregion& cdr,
                            TSeqPos from, TSeqPos to,
                            const CSeq_id* seqid)
{
    CCdregion::TCode_break::iterator it = cdr.SetCode_break().begin();
    while (it != cdr.SetCode_break().end()) {
        if ((*it)->IsSetLoc()) {
            SeqLocAdjustForInsert((*it)->SetLoc(), from, to, seqid);
        }
        it++;
    }
    if (cdr.GetCode_break().empty()) {
        cdr.ResetCode_break();
    }
}


void TrnaAdjustForInsert(CTrna_ext& trna,
                        TSeqPos from, TSeqPos to,
                        const CSeq_id* seqid)
{
    if (trna.IsSetAnticodon()) {
        SeqLocAdjustForInsert(trna.SetAnticodon(), from, to, seqid); 
    }
}


void FeatureAdjustForInsert(CSeq_feat& feat, 
                            TSeqPos from, TSeqPos to,
                            const CSeq_id* seqid)
{
    SeqLocAdjustForInsert (feat.SetLocation(), from, to, seqid);

    if (feat.IsSetData()) {
        switch (feat.GetData().Which()) {
            case CSeqFeatData::eSubtype_cdregion:
                CdregionAdjustForInsert(feat.SetData().SetCdregion(), from, to, seqid);
                break;
            case CSeqFeatData::eSubtype_tRNA:
                TrnaAdjustForInsert(feat.SetData().SetRna().SetExt().SetTRNA(), from, to, seqid);
                break;
            default:
                break;
        }
    }
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

