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


bool CLocationEditPolicy::Is5AtEndOfSeq(const CSeq_loc& loc, CBioseq_Handle bsh)
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


bool CLocationEditPolicy::Is3AtEndOfSeq(const CSeq_loc& loc, CBioseq_Handle bsh)
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
            } else if (m_Extend5 && !Is5AtEndOfSeq(loc, bsh)
                       && (bsh || s_Know5WithoutBsh(loc))) {
                do_set_5_partial = true;
            }
            break;
        case ePartialPolicy_eSetAtEnd:
            if (Is5AtEndOfSeq(loc, bsh) 
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
                && !Is5AtEndOfSeq(orig_feat.GetLocation(), bsh)
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
            } else if (m_Extend3 && !Is3AtEndOfSeq(loc, bsh) && (bsh || s_Know3WithoutBsh(loc))) {
                do_set_3_partial = true;
            }
            break;
        case ePartialPolicy_eSetAtEnd:
            if (!orig_feat.GetLocation().IsPartialStop(eExtreme_Biological) 
                && Is3AtEndOfSeq(orig_feat.GetLocation(), bsh)
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
                && !Is3AtEndOfSeq(orig_feat.GetLocation(), bsh)
                && (bsh || s_Know3WithoutBsh(loc))) {
                do_clear_3_partial = true;
            }
            break;
        case ePartialPolicy_eClearForGoodEnd:
            if (orig_feat.GetLocation().IsPartialStop(eExtreme_Biological)
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
            Extend5(feat, scope);
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
            // extend end
            Extend3(feat, scope);
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
                if (sequence::Compare(*new_loc, feat.GetLocation(), &scope, sequence::fCompareOverlapping) != sequence::eSame) {
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

bool CLocationEditPolicy::Extend5(CSeq_feat& feat, CScope& scope)
{
    bool extend = false;
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
                    extend = true;
                } else {
                    diff = 0;
                }
            }
        } else  if (start > 0) {
            diff = start;
            CRef<CSeq_loc> new_loc = SeqLocExtend(feat.GetLocation(), 0, &scope);
            if (new_loc) {
                feat.SetLocation().Assign(*new_loc);
                extend = true;
            } else {
                diff = 0;
            }
        }
        // adjust frame to maintain consistency
        if (diff % 3 > 0 && feat.GetData().IsCdregion()) {
            int orig_frame = 1;
            if (feat.GetData().GetCdregion().IsSetFrame()) {
                if (feat.GetData().GetCdregion().GetFrame() == CCdregion::eFrame_two) {
                    orig_frame = 2;
                } else if (feat.GetData().GetCdregion().GetFrame() == CCdregion::eFrame_three) {
                    orig_frame = 3;
                }
            }
            CCdregion::EFrame new_frame = CCdregion::eFrame_not_set;
            switch ((orig_frame + diff % 3) % 3) {
                case 1:
                    new_frame = CCdregion::eFrame_not_set;
                    break;
                case 2:
                    new_frame = CCdregion::eFrame_two;
                    break;
                case 0:
                    new_frame = CCdregion::eFrame_three;
                    break;
            }
            feat.SetData().SetCdregion().SetFrame(new_frame);
        }
    }
    return extend;
}

bool CLocationEditPolicy::Extend3(CSeq_feat& feat, CScope& scope)
{
    bool extend = false;
    CBioseq_Handle bsh = scope.GetBioseqHandle(feat.GetLocation());
    if (bsh || s_Know3WithoutBsh(feat.GetLocation())) {
        ENa_strand strand = feat.GetLocation().GetStrand();
        size_t stop = feat.GetLocation().GetStop(eExtreme_Biological);
        if (strand == eNa_strand_minus) {                
            if (stop > 0) {
                CRef<CSeq_loc> new_loc = SeqLocExtend(feat.GetLocation(), 0, &scope);
                if (new_loc) {
                    feat.SetLocation().Assign(*new_loc);
                    extend = true;
                }
            }
        } else  if (stop < bsh.GetInst_Length() - 1) {
            CRef<CSeq_loc> new_loc = SeqLocExtend(feat.GetLocation(), bsh.GetInst_Length() - 1, &scope);
            if (new_loc) {
                feat.SetLocation().Assign(*new_loc);
                extend = true;
            }
        }
    }
    return extend;
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


bool OkToAdjustLoc(const CSeq_interval& interval, const CSeq_id* seqid)
{
    bool rval = true;
    if (seqid) {
        if (!interval.IsSetId() || interval.GetId().Compare(*seqid) != CSeq_id::e_YES) {
            rval = false;
        }
    }
    return rval;
}


bool OkToAdjustLoc(const CSeq_point& pnt, const CSeq_id* seqid)
{
    bool rval = true;
    if (seqid) {
        if (!pnt.IsSetId() || pnt.GetId().Compare(*seqid) != CSeq_id::e_YES) {
            rval = false;
        }
    }
    return rval;
}


bool OkToAdjustLoc(const CPacked_seqpnt& pack, const CSeq_id* seqid)
{
    bool rval = true;
    if (seqid) {
        if (!pack.IsSetId() || pack.GetId().Compare(*seqid) != CSeq_id::e_YES) {
            rval = false;
        }
    }
    return rval;
}


void NormalizeLoc(CSeq_loc& loc)
{
    switch (loc.Which()) {
        case CSeq_loc::e_Equiv:
            {{
                CSeq_loc::TEquiv::Tdata::iterator it = loc.SetEquiv().Set().begin();
                while (it != loc.SetEquiv().Set().end()) {
                    NormalizeLoc(**it);
                    if (loc.Which() == CSeq_loc::e_not_set) {
                        it = loc.SetEquiv().Set().erase(it);
                    } else {
                        ++it;
                    }
                }

                // if only one, make regular loc
                if (loc.GetEquiv().Get().size() == 1) {
                    CRef<CSeq_loc> sub(new CSeq_loc());
                    sub->Assign(*(loc.GetEquiv().Get().front()));
                    loc.Assign(*sub);
                } else if (loc.GetEquiv().Get().size() == 0) {
                    // no sub intervals, reset
                    loc.Reset();
                }                
            }}
            break;
        case CSeq_loc::e_Mix:
            {{
                CSeq_loc::TMix::Tdata::iterator it = loc.SetMix().Set().begin();
                while (it != loc.SetMix().Set().end()) {
                    NormalizeLoc(**it);
                    if (loc.Which() == CSeq_loc::e_not_set) {
                        it = loc.SetMix().Set().erase(it);
                    } else {
                        ++it;
                    }
                }

                // if only one, make regular loc
                if (loc.GetMix().Get().size() == 1) {
                    CRef<CSeq_loc> sub(new CSeq_loc());
                    sub->Assign(*(loc.GetMix().Get().front()));
                    loc.Assign(*sub);
                } else if (loc.GetMix().Get().size() == 0) {
                    // no sub intervals, reset
                    loc.Reset();
                }                
            }}
            break;
        case CSeq_loc::e_Packed_int:
            if (loc.GetPacked_int().Get().size() == 0) {
                loc.Reset();
            } else if (loc.GetPacked_int().Get().size() == 1) {
                CRef<CSeq_interval> sub(new CSeq_interval());
                sub->Assign(*(loc.GetPacked_int().Get().front()));
                loc.SetInt().Assign(*sub);
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            if (loc.GetPacked_pnt().GetPoints().size() == 0) {
                loc.Reset();
            } else if (loc.GetPacked_pnt().GetPoints().size() == 1) {
                CRef<CSeq_point> sub(new CSeq_point());
                if (loc.GetPacked_pnt().IsSetStrand()) {
                    sub->SetStrand(loc.GetPacked_pnt().GetStrand());
                }
                if (loc.GetPacked_pnt().IsSetId()) {
                    sub->SetId().Assign(loc.GetPacked_pnt().GetId());
                }
                if (loc.GetPacked_pnt().IsSetFuzz()) {
                    sub->SetFuzz().Assign(loc.GetPacked_pnt().GetFuzz());
                }
                sub->SetPoint(loc.GetPacked_pnt().GetPoints()[0]);
                loc.SetPnt().Assign(*sub);
            }
            break;
        default:
            // do nothing
            break;
    }
}


void SeqLocAdjustForTrim(CSeq_interval& interval, 
                         TSeqPos cut_from, TSeqPos cut_to,
                         const CSeq_id* seqid,
                         bool& bCompleteCut,
                         TSeqPos& trim5,
                         bool& bAdjusted)
{
    if (!OkToAdjustLoc(interval, seqid)) {
        return;
    }

    // These are required fields
    if ( !(interval.CanGetFrom() && interval.CanGetTo()) )
    {
        return;
    }

    // Feature location
    TSeqPos feat_from = interval.GetFrom();
    TSeqPos feat_to = interval.GetTo();

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
        trim5 += feat_from - feat_to + 1;
        return;
    }

    // Case 3: feature is completely past the cut
    if (feat_from > cut_to)
    {
        // Shift the feature by the cut_size
        feat_from -= cut_size;
        feat_to -= cut_size;
        interval.SetFrom(feat_from);
        interval.SetTo(feat_to);
        bAdjusted = true;
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
        if (interval.IsSetStrand() && interval.GetStrand() == eNa_strand_minus) {
            TSeqPos diff = cut_from - 1 - feat_to;
            trim5 += diff;
        }
        feat_to = cut_from - 1;
    }

    // Take care of the feat_from from the left side cut case
    if (feat_from >= cut_from) {
        if (!interval.IsSetStrand() || interval.GetStrand() != eNa_strand_minus) {
            TSeqPos diff = cut_to + 1 - feat_from;
            trim5 += diff;
        }
        feat_from = cut_to + 1;
        feat_from -= cut_size;
    }

    interval.SetFrom(feat_from);
    interval.SetTo(feat_to);
    bAdjusted = true;
}


void SeqLocAdjustForTrim(CPacked_seqint& packint, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid,
                bool& bCompleteCut,
                TSeqPos& trim5,
                bool& bAdjusted)
{
    if (packint.IsSet()) {
        bool from5 = true;
        // Process each interval in the list
        CPacked_seqint::Tdata::iterator it;
        for (it = packint.Set().begin(); 
                it != packint.Set().end(); ) 
        {
            bool bDeleted = false;
            TSeqPos this_trim = 0;
            SeqLocAdjustForTrim(**it, from, to, seqid, 
                                bDeleted, this_trim, bAdjusted);

            if (from5) {
                trim5 += this_trim;
            }
            // Should interval be deleted from list?
            if (bDeleted) {
                it = packint.Set().erase(it);
            }
            else {
                from5 = false;
                ++it;
            }
        }
        if (packint.Get().empty()) {
            packint.Reset();
        }
    }    
    if (!packint.IsSet()) {
        bCompleteCut = true;
    }
}


void SeqLocAdjustForTrim(CSeq_loc_mix& mix, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid,
                bool& bCompleteCut,
                TSeqPos& trim5,
                bool& bAdjusted)
{
    if (mix.IsSet()) {
        bool from5 = true;
        // Process each seqloc in the list
        CSeq_loc_mix::Tdata::iterator it;
        for (it = mix.Set().begin(); 
                it != mix.Set().end(); ) 
        {
            bool bDeleted = false;
            TSeqPos this_trim = 0;
            SeqLocAdjustForTrim(**it, from, to, seqid, bDeleted, this_trim, bAdjusted);

            if (from5) {
                trim5 += this_trim;
            }
            // Should seqloc be deleted from list?
            if (bDeleted) {
                it = mix.Set().erase(it);
            }
            else {
                from5 = false;
                ++it;
            }
        }
    }
    if (!mix.IsSet() || mix.Set().empty()) {
        bCompleteCut = true;
    }
}


void SeqLocAdjustForTrim(CSeq_point& pnt, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid,
                bool& bCompleteCut,
                TSeqPos& trim5,
                bool& bAdjusted)
{
    if (!OkToAdjustLoc(pnt, seqid)) {
        return;
    }

    if (to < pnt.GetPoint()) {
        size_t diff = to - from + 1;
        pnt.SetPoint(pnt.GetPoint() - diff);
        bAdjusted = true;
    } else if (from < pnt.GetPoint()) {
        bCompleteCut = true;
        trim5 += 1;
    }
}


void SeqLocAdjustForTrim(CPacked_seqpnt& pack, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid,
                bool& bCompleteCut, TSeqPos& trim5, bool& bAdjusted)
{
    if (!OkToAdjustLoc(pack, seqid)) {
        return;
    }

    if (pack.IsSetPoints()) {
        bool from5 = true;
        CPacked_seqpnt::TPoints::iterator it = pack.SetPoints().begin();
        while (it != pack.SetPoints().end()) {
            if (to < *it) {
                size_t diff = to - from + 1;
                *it -= diff;
                it++;
                bAdjusted = true;
                from5 = false;
            } else if (from < *it) {
                it = pack.SetPoints().erase(it);
                bAdjusted = true;
                if (from5) {
                    trim5 += 1;
                }
            } else {
                it++;
                from5 = false;
            }
        }
    }
    if (pack.SetPoints().empty()) {
        bCompleteCut = true;
    }
}


void SeqLocAdjustForTrim(CSeq_bond& bond, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid,
                bool& bCompleteCut,
                TSeqPos& trim5,
                bool& bAdjusted)
{
    bool cutA = false, cutB = false;
    if (bond.IsSetA()) {
        SeqLocAdjustForTrim(bond.SetA(), from, to, seqid, cutA, trim5, bAdjusted);
    } else {
        cutA = true;
    }

    if (bond.IsSetB()) {
        SeqLocAdjustForTrim(bond.SetB(), from, to, seqid, cutB, trim5, bAdjusted);
    } else {
        cutB = true;
    }
    if (cutA && cutB) {
        bCompleteCut = true;
    }
}


void SeqLocAdjustForTrim(CSeq_loc_equiv& equiv, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid,                
                bool& bCompleteCut,
                TSeqPos& trim5,
                bool& bAdjusted)
{
    TSeqPos max_trim5 = 0;
    CSeq_loc_equiv::Tdata::iterator it = equiv.Set().begin();
    while (it != equiv.Set().end()) {
        bool cut = false;
        TSeqPos this_trim5 = 0;
        SeqLocAdjustForTrim(**it, from, to, seqid, cut, this_trim5, bAdjusted);
        if (this_trim5 > max_trim5) {
            max_trim5 = this_trim5;
        }
        if (cut) {
            it = equiv.Set().erase(it);
        } else {
            it++;
        }
    }
    if (equiv.Set().empty()) {
        bCompleteCut = true;
    }
    trim5 = max_trim5;
}


void SeqLocAdjustForTrim(CSeq_loc& loc, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid,
                bool& bCompleteCut,
                TSeqPos& trim5, bool& bAdjusted)
{
    // Given a seqloc and a range, cut the seqloc

    switch(loc.Which())
    {
        // Single interval
        case CSeq_loc::e_Int:
            SeqLocAdjustForTrim(loc.SetInt(), from, to, seqid, 
                                bCompleteCut, trim5, bAdjusted);
            break;

        // Multiple intervals
        case CSeq_loc::e_Packed_int:
            SeqLocAdjustForTrim(loc.SetPacked_int(), from, to, seqid, bCompleteCut, trim5, bAdjusted);
            break;

        // Multiple seqlocs
        case CSeq_loc::e_Mix:
            SeqLocAdjustForTrim(loc.SetMix(), from, to, seqid, bCompleteCut, trim5, bAdjusted);
            break;
        case CSeq_loc::e_Pnt:
            SeqLocAdjustForTrim(loc.SetPnt(), from, to , seqid, bCompleteCut, trim5, bAdjusted); 
            break;
        case CSeq_loc::e_Packed_pnt:
            SeqLocAdjustForTrim(loc.SetPacked_pnt(), from, to, seqid, bCompleteCut, trim5, bAdjusted);
            break;
        case CSeq_loc::e_Bond:
            SeqLocAdjustForTrim(loc.SetBond(), from, to, seqid, bCompleteCut, trim5, bAdjusted);
            break;
        case CSeq_loc::e_Equiv:
            SeqLocAdjustForTrim(loc.SetEquiv(), from, to, seqid, bCompleteCut, trim5, bAdjusted);
            break;
        case CSeq_loc::e_Empty:
        case CSeq_loc::e_Null:
        case CSeq_loc::e_not_set:
        case CSeq_loc::e_Whole:
        case CSeq_loc::e_Feat:
            // no adjustment needeed
            break;
    }
    if (!bCompleteCut) {
        NormalizeLoc(loc);
    }
}


void SeqLocAdjustForInsert(CSeq_interval& interval, 
                         TSeqPos insert_from, TSeqPos insert_to,
                         const CSeq_id* seqid)
{
    if (!OkToAdjustLoc(interval, seqid)) {
        return;
    }

    // These are required fields
    if ( !(interval.CanGetFrom() && interval.CanGetTo()) )
    {
        return;
    }

    // Feature location
    TSeqPos feat_from = interval.GetFrom();
    TSeqPos feat_to = interval.GetTo();

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
        interval.SetFrom(feat_from);
        interval.SetTo(feat_to);
        return;
    }

    // Case 3: insert occurs within interval
    if (feat_from <= insert_from && feat_to >= insert_from)
    {
        feat_to += insert_size;
        interval.SetTo(feat_to);
        return;
    }
}


void SeqLocAdjustForInsert(CPacked_seqint& packint, 
                         TSeqPos insert_from, TSeqPos insert_to,
                         const CSeq_id* seqid)
{
    if (packint.IsSet()) {
        // Process each interval in the list
        CPacked_seqint::Tdata::iterator it;
        for (it = packint.Set().begin(); 
                it != packint.Set().end(); it++) 
        {
            SeqLocAdjustForInsert(**it, insert_from, insert_to, seqid);
        }
    }
}


void SeqLocAdjustForInsert(CSeq_loc_mix& mix, 
                         TSeqPos insert_from, TSeqPos insert_to,
                         const CSeq_id* seqid)
{
    if (mix.IsSet()) {
        // Process each seqloc in the list
        CSeq_loc_mix::Tdata::iterator it;
        for (it = mix.Set().begin(); 
                it != mix.Set().end(); it++) 
        {
            SeqLocAdjustForInsert(**it, insert_from, insert_to, seqid);
        }
    }
}


void SeqLocAdjustForInsert(CSeq_point& pnt, 
                         TSeqPos insert_from, TSeqPos insert_to,
                         const CSeq_id* seqid)
{
    if (!OkToAdjustLoc(pnt, seqid)) {
        return;
    }
    if (!pnt.IsSetPoint()) {
        return;
    }

    if (insert_from < pnt.GetPoint()) {
        size_t diff = insert_to - insert_from + 1;
        pnt.SetPoint(pnt.GetPoint() + diff);
    }
}


void SeqLocAdjustForInsert(CPacked_seqpnt& packpnt, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid)
{
    if (!OkToAdjustLoc(packpnt, seqid)) {
        return;
    }

    CPacked_seqpnt::TPoints::iterator it = packpnt.SetPoints().begin();
    while (it != packpnt.SetPoints().end()) {
        if (from < *it) {
            size_t diff = to - from + 1;
            *it += diff;
        }
        it++;
    }
}


void SeqLocAdjustForInsert(CSeq_bond& bond, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid)
{
    if (bond.IsSetA()) {
        SeqLocAdjustForInsert(bond.SetA(), from, to, seqid);
    }

    if (bond.IsSetB()) {
        SeqLocAdjustForInsert(bond.SetB(), from, to, seqid);
    }
}


void SeqLocAdjustForInsert(CSeq_loc_equiv& equiv, 
                TSeqPos from, TSeqPos to,
                const CSeq_id* seqid)
{
    CSeq_loc_equiv::Tdata::iterator it = equiv.Set().begin();
    while (it != equiv.Set().end()) {
        SeqLocAdjustForInsert(**it, from, to, seqid);
        it++;
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
            SeqLocAdjustForInsert(loc.SetInt(), from, to, seqid);
            break;

        // Multiple intervals
        case CSeq_loc::e_Packed_int:
            SeqLocAdjustForInsert(loc.SetPacked_int(), from, to, seqid);
            break;

        // Multiple seqlocs
        case CSeq_loc::e_Mix:
            SeqLocAdjustForInsert(loc.SetMix(), from, to, seqid);
            break;
        case CSeq_loc::e_Pnt:
            SeqLocAdjustForInsert(loc.SetPnt(), from, to, seqid);
            break;

        case CSeq_loc::e_Packed_pnt:
            SeqLocAdjustForInsert(loc.SetPacked_pnt(), from, to, seqid);
            break;
        case CSeq_loc::e_Bond:
            SeqLocAdjustForInsert(loc.SetBond(), from, to, seqid);
            break;
        case CSeq_loc::e_Equiv:
            SeqLocAdjustForInsert(loc.SetEquiv(), from, to, seqid);
            break;
        case CSeq_loc::e_Empty:
        case CSeq_loc::e_Null:
        case CSeq_loc::e_not_set:
        case CSeq_loc::e_Whole:
        case CSeq_loc::e_Feat:
            // no adjustment needeed
            break;
    }
}


CRef<CSeq_interval> SplitLocationForGap(CSeq_interval& before, 
                                        size_t start, size_t stop,
                                        const CSeq_id* seqid, bool& cut,
                                        unsigned int options)
{
    cut = false;
    if (!OkToAdjustLoc(before, seqid)) {
        return CRef<CSeq_interval>(NULL);
    }
    // These are required fields
    if ( !(before.CanGetFrom() && before.CanGetTo()) )
    {
        return CRef<CSeq_interval>(NULL);
    }

    // Feature location
    TSeqPos feat_from = before.GetFrom();
    TSeqPos feat_to = before.GetTo();

    CRef<CSeq_interval> after(NULL);
    if (feat_to < start) {
        // gap completely after location
        return after;
    }

    if (feat_from > start && !(options & eSplitLocOption_split_in_intron)) {
        // if gap completely before location, but not splitting in introns,
        // no change
        return after;
    }

    if (feat_from < start && feat_to > stop) {
        // gap entirely in inteval
        if (!(options & eSplitLocOption_split_in_exon)) {
            return after;
        }
    }

    if (feat_to > stop) {
        after.Reset(new CSeq_interval());
        after->Assign(before);
        if (stop + 1 > feat_from) {
            after->SetFrom(stop + 1);
            if (options & eSplitLocOption_make_partial) {
                after->SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
            }
        }
    } 
    if (feat_from < start) {
        before.SetTo(start - 1);
        if (options & eSplitLocOption_make_partial) {
            before.SetFuzz_to().SetLim(CInt_fuzz::eLim_gt);
        }
    } else {
        cut = true;
    }
    return after;
}


void SplitLocationForGap(CSeq_loc::TPacked_int& before_intervals,
                         CSeq_loc::TPacked_int& after_intervals,
                         size_t start, size_t stop, 
                         const CSeq_id* seqid, unsigned int options)
{
    if (before_intervals.IsSet()) {
        // Process each interval in the list
        CPacked_seqint::Tdata::iterator it;
        for (it = before_intervals.Set().begin(); 
                it != before_intervals.Set().end(); ) 
        {
            bool cut = false;
            CRef<CSeq_interval> after = SplitLocationForGap(**it, start, stop, seqid, cut, options);

            // Should interval be deleted from list?
            if (cut) {
                it = before_intervals.Set().erase(it);
            }
            else {
                ++it;
            }
            if (after) {
                after_intervals.Set().push_back(after);
                // from here on, always move intervals to the right to the other loc
                options |= eSplitLocOption_split_in_intron;
            }
        }
    }
}


void SplitLocationForGap(CSeq_loc& loc1, CSeq_loc& loc2, 
                         size_t start, size_t stop, 
                         const CSeq_id* seqid, unsigned int options)
{
    // Given a seqloc and a range, place the portion of the location before the range
    // into loc1 and the remainder of the location into loc2

    switch(loc1.Which())
    {
        // Single interval
        case CSeq_loc::e_Int:
            {{
                bool cut = false;
                CRef<CSeq_interval> after = SplitLocationForGap(loc1.SetInt(), start, stop,
                                                                seqid, cut, options);
                if (cut) {
                    loc1.Reset();
                }
                if (after) {
                    if (loc2.Which() == CSeq_loc::e_not_set) {
                        loc2.SetInt(*after);
                    } else {
                        CRef<CSeq_loc> add(new CSeq_loc());
                        add->SetInt(*after);
                        loc2.Add(*add);
                    }
                }
            }}
            break;
        // Single point
        case CSeq_loc::e_Pnt:
            if (OkToAdjustLoc(loc1.GetPnt(), seqid)) {
                if (stop < loc1.GetPnt().GetPoint()) {
                    if (loc2.Which() == CSeq_loc::e_not_set) {
                        loc2.SetPnt().Assign(loc1.GetPnt());
                    } else {
                        loc2.Add(loc1);
                    }
                    loc1.Reset();
                }
            }
            break;

        // Multiple intervals
        case CSeq_loc::e_Packed_int:
            {{
                CSeq_loc::TPacked_int& before_intervals = loc1.SetPacked_int();
                CRef<CSeq_loc::TPacked_int> after_intervals(new CSeq_loc::TPacked_int);
                SplitLocationForGap(before_intervals, *after_intervals,
                                    start, stop,
                                    seqid, options);

                if (before_intervals.Set().empty()) {
                    loc1.Reset();
                }
                if (!after_intervals->Set().empty()) {
                    if (loc2.Which() == CSeq_loc::e_not_set) {
                        loc2.SetPacked_int().Assign(*after_intervals);
                    } else {
                        CRef<CSeq_loc> add(new CSeq_loc());
                        add->SetPacked_int().Assign(*after_intervals);
                        loc2.Add(*add);
                    }
                }
            }}
            break;

        // Multiple seqlocs
        case CSeq_loc::e_Mix:
            {{
                CSeq_loc_mix& before_mix = loc1.SetMix();
                CRef<CSeq_loc_mix> after_mix(new CSeq_loc_mix);
                 if (before_mix.IsSet()) {
                    // Process each seqloc in the list
                    CSeq_loc_mix::Tdata::iterator it;
                    for (it = before_mix.Set().begin(); 
                         it != before_mix.Set().end(); ) 
                    {
                        CRef<CSeq_loc> after(new CSeq_loc());
                        SplitLocationForGap(**it, *after, start, stop, seqid, options);
                        // Should seqloc be deleted from list?
                        if ((*it)->Which() == CSeq_loc::e_not_set) {
                            it = before_mix.Set().erase(it);
                        }
                        else {
                            ++it;
                        }
                        if (after->Which() != CSeq_loc::e_not_set) {
                            after_mix->Set().push_back(after);
                            // from here on, always move intervals to the right to the other loc
                            options |= eSplitLocOption_split_in_intron;
                        }
                    }

                    // Update the original list
                    if (before_mix.Set().empty()) {
                        loc1.Reset();
                    }
                    if (!after_mix->Set().empty()) {
                        if (loc2.Which() == CSeq_loc::e_not_set) {
                            loc2.SetMix().Assign(*after_mix);
                        } else {
                            CRef<CSeq_loc> add(new CSeq_loc());
                            add->SetMix().Assign(*after_mix);
                            loc2.Add(*add);
                        }
                    }
                }
            }}
            break;
        case CSeq_loc::e_Equiv:
             {{
                CSeq_loc_equiv& before_equiv = loc1.SetEquiv();
                CRef<CSeq_loc_equiv> after_equiv(new CSeq_loc_equiv);
                 if (before_equiv.IsSet()) {
                    // Process each seqloc in the list
                    CSeq_loc_equiv::Tdata::iterator it;
                    for (it = before_equiv.Set().begin(); 
                         it != before_equiv.Set().end(); ) 
                    {
                        CRef<CSeq_loc> after(new CSeq_loc());
                        SplitLocationForGap(**it, *after, start, stop, seqid, options);
                        // Should seqloc be deleted from list?
                        if ((*it)->Which() == CSeq_loc::e_not_set) {
                            it = before_equiv.Set().erase(it);
                        }
                        else {
                            ++it;
                        }
                        if (after->Which() != CSeq_loc::e_not_set) {
                            after_equiv->Set().push_back(after);
                        }
                    }

                    // Update the original list
                    if (before_equiv.Set().empty()) {
                        loc1.Reset();
                    }
                    if (!after_equiv->Set().empty()) {
                        if (loc2.Which() == CSeq_loc::e_not_set) {
                            loc2.SetMix().Assign(*after_equiv);
                        } else {
                            CRef<CSeq_loc> add(new CSeq_loc());
                            add->SetMix().Assign(*after_equiv);
                            loc2.Add(*add);
                        }
                    }
                }
            }}
            break;
        case CSeq_loc::e_Packed_pnt:
            if (OkToAdjustLoc(loc1.GetPacked_pnt(), seqid)) {
                CPacked_seqpnt::TPoints& before_points = loc1.SetPacked_pnt().SetPoints();
                CPacked_seqpnt::TPoints after_points;
                CPacked_seqpnt::TPoints::iterator it = loc1.SetPacked_pnt().SetPoints().begin();
                while (it != loc1.SetPacked_pnt().SetPoints().end()) {
                    if (stop < *it) {
                        after_points.push_back(*it);
                    }
                    if (start >= *it) {
                        it = before_points.erase(it);
                    } else {
                        it++;
                    }
                }
                if (!after_points.empty()) {
                    CRef<CPacked_seqpnt> after(new CPacked_seqpnt());
                    after->Assign(loc1.GetPacked_pnt());
                    after->SetPoints().assign(after_points.begin(), after_points.end());
                    if (loc2.Which() == CSeq_loc::e_not_set) {
                        loc2.SetPacked_pnt().Assign(*after);
                    } else {
                        CRef<CSeq_loc> add(new CSeq_loc());
                        add->SetPacked_pnt().Assign(*after);
                        loc2.Add(*add);
                    }
                }
                if (before_points.empty()) {
                    loc1.Reset();
                }
            }
            break;
        case CSeq_loc::e_Empty:
        case CSeq_loc::e_Null:
        case CSeq_loc::e_not_set:
        case CSeq_loc::e_Whole:
        case CSeq_loc::e_Feat:
        case CSeq_loc::e_Bond:
            // no adjustment needeed
            break;
    }
    NormalizeLoc(loc1);
    NormalizeLoc(loc2);
}


void CdregionAdjustForTrim(CCdregion& cdr,
                            TSeqPos from, TSeqPos to,
                            const CSeq_id* seqid)
{
    CCdregion::TCode_break::iterator it = cdr.SetCode_break().begin();
    while (it != cdr.SetCode_break().end()) {
        if ((*it)->IsSetLoc()) {
            bool cut = false;
            bool adjusted = false;
            TSeqPos trim5 = 0;
            SeqLocAdjustForTrim((*it)->SetLoc(), from, to, seqid, cut, trim5, adjusted);
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
        TSeqPos trim5 = 0;
        SeqLocAdjustForTrim(trna.SetAnticodon(), from, to, seqid, cut, trim5, trimmed);
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
    TSeqPos trim5 = 0;
    SeqLocAdjustForTrim (feat.SetLocation(), from, to, seqid, bCompleteCut, trim5, bTrimmed);
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

