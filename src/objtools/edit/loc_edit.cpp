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
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_inst.hpp>
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
#include <objects/misc/sequence_util_macros.hpp>
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


bool CLocationEditPolicy::Is5AtEndOfSeq(const CSeq_loc& loc, CScope& scope, bool& confident)
{
    bool rval = false;
    confident = true;
    CSeq_loc_CI first_l(loc);
    if (!first_l.IsSetStrand() || first_l.GetStrand() != eNa_strand_minus) {
        // positive strand, just need to know if it's at zero
        rval = (first_l.GetRange().GetFrom() == 0);
    } else {
        // negative strand
        try {
            CBioseq_Handle bsh = scope.GetBioseqHandle(first_l.GetSeq_id());
            rval = (first_l.GetRange().GetTo() == bsh.GetBioseqLength() - 1);
        } catch (CException&) {
            confident = false;
        }
    }
    return rval;
}


bool CLocationEditPolicy::Is3AtEndOfSeq(const CSeq_loc& loc, CScope& scope, bool& confident)
{
    bool rval = false;
    confident = true;
    CSeq_loc_CI last_l(loc);
    size_t num_intervals = last_l.GetSize();
    last_l.SetPos(num_intervals - 1);
    if (!last_l.IsSetStrand() || last_l.GetStrand() != eNa_strand_minus) {
        // positive strand
        try {
            CBioseq_Handle bsh = scope.GetBioseqHandle(last_l.GetSeq_id());
            if (bsh) {
                rval = (last_l.GetRange().GetTo() == bsh.GetBioseqLength() - 1);
            } else {
                confident = false;
            }
        } catch (CException&) {
            confident = false;
        }
    } else {
        // negative strand, just need to know if it's at zero
        rval = (last_l.GetRange().GetFrom() == 0);
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
    const CSeq_loc& loc = orig_feat.GetLocation();

    switch (m_PartialPolicy5) {
        case ePartialPolicy_eNoChange:
            // do nothing
            break;
        case ePartialPolicy_eSet:
            if (!orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)) {
                do_set_5_partial = true;
            } else if (m_Extend5) {
                bool confident = false;
                bool at_5 = Is5AtEndOfSeq(loc, scope, confident);
                if (confident && !at_5) {
                    do_set_5_partial = true;
                }
            }
            break;
        case ePartialPolicy_eSetAtEnd:
            if (!orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)) {
                bool confident = false;
                if (Is5AtEndOfSeq(loc, scope, confident) && confident) {
                    do_set_5_partial = true;
                }
            }
            break;
        case ePartialPolicy_eSetForBadEnd:
            if (!orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()) {
                string transl_prot;
                bool confident = true;
                try {
                    CSeqTranslator::Translate(orig_feat, scope, transl_prot,
                                                false,   // do not include stop codons
                                                false);  // do not remove trailing X/B/Z

                } catch ( const CException& ) {
                    confident = false;
                }
                if (confident && !NStr::StartsWith(transl_prot, "M", NStr::eNocase)) {
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
            if (orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)) {
                bool confident = false;
                if (!Is5AtEndOfSeq(loc, scope, confident) && confident) {
                    do_clear_5_partial = true;
                }
            }
            break;
        case ePartialPolicy_eClearForGoodEnd:
            if (orig_feat.GetLocation().IsPartialStart(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()
                && (!orig_feat.GetData().GetCdregion().IsSetFrame() ||
                    orig_feat.GetData().GetCdregion().GetFrame() == CCdregion::eFrame_not_set ||
                    orig_feat.GetData().GetCdregion().GetFrame() == CCdregion::eFrame_one)) {
                string transl_prot;
                bool confident = true;
                try {
                    CSeqTranslator::Translate(orig_feat, scope, transl_prot,
                                                false,   // do not include stop codons
                                                false);  // do not remove trailing X/B/Z

                } catch ( const CException& ) {
                    confident = false;
                }
                if (confident && NStr::StartsWith(transl_prot, "M", NStr::eNocase)) {
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

    switch (m_PartialPolicy3) {
        case ePartialPolicy_eNoChange:
            // do nothing
            break;
        case ePartialPolicy_eSet:
            if (!loc.IsPartialStop(eExtreme_Biological)) {
                do_set_3_partial = true;
            } else if (m_Extend3) {
                bool confident = false;
                bool at_3 = Is3AtEndOfSeq(loc, scope, confident);
                if (confident && !at_3) {
                    do_set_3_partial = true;
                }
            }
            break;
        case ePartialPolicy_eSetAtEnd:
            if (!loc.IsPartialStop(eExtreme_Biological)) {
                bool confident = false;
                bool at_3 = Is3AtEndOfSeq(loc, scope, confident);
                if (at_3 && confident) {
                    do_set_3_partial = true;
                }
            }
            break;
        case ePartialPolicy_eSetForBadEnd:
            if (!loc.IsPartialStop(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()) {
                string transl_prot;
                bool confident = true;
                try {
                    CSeqTranslator::Translate(orig_feat, scope, transl_prot,
                                                true,   // include stop codons
                                                false);  // do not remove trailing X/B/Z

                } catch ( const CException& ) {
                    confident = false;
                }
                if (confident && !NStr::EndsWith(transl_prot, "*", NStr::eNocase)) {
                    do_set_3_partial = true;
                }
            }
            break;
        case ePartialPolicy_eSetForFrame:
            // not allowed for 3' end
            break;
        case ePartialPolicy_eClear:
            if (loc.IsPartialStop(eExtreme_Biological)) {
                do_clear_3_partial = true;
            }
            break;
        case ePartialPolicy_eClearNotAtEnd:
            if (loc.IsPartialStop(eExtreme_Biological)) {
                bool confident = false;
                bool at_3 = Is3AtEndOfSeq(loc, scope, confident);
                if (!at_3 && confident) {
                    do_clear_3_partial = true;
                }
            }
            break;
        case ePartialPolicy_eClearForGoodEnd:
            if (loc.IsPartialStop(eExtreme_Biological)
                && orig_feat.GetData().IsCdregion()) {
                string transl_prot;
                bool confident = true;
                try {
                    CSeqTranslator::Translate(orig_feat, scope, transl_prot,
                                                true,   // include stop codons
                                                false);  // do not remove trailing X/B/Z

                } catch ( const CException& ) {
                    confident = false;
                }
                if (confident && NStr::EndsWith(transl_prot, "*", NStr::eNocase)) {
                    do_clear_3_partial = true;
                }
            }
            break;
    }
    return do_set_3_partial || do_clear_3_partial;
}


CRef<CSeq_loc> SeqLocExtend5(const CSeq_loc& loc, TSeqPos pos, CScope* scope)
{
    CSeq_loc_CI first_l(loc);
    CConstRef<CSeq_loc> first_loc = first_l.GetRangeAsSeq_loc();

    TSeqPos loc_start = first_loc->GetStart(eExtreme_Biological);
    bool partial_start = first_loc->IsPartialStart(eExtreme_Biological);
    ENa_strand strand = first_loc->IsSetStrand() ? first_loc->GetStrand() : eNa_strand_plus;
    CRef<CSeq_loc> new_loc(NULL);

    CRef<CSeq_id> id(new CSeq_id());
    id->Assign(first_l.GetSeq_id());

    if (pos < loc_start && strand != eNa_strand_minus) {
        CRef<CSeq_loc> add(new CSeq_loc(*id, pos, loc_start - 1, strand));
        add->SetPartialStart(partial_start, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, scope);
    } else if (pos > loc_start && strand == eNa_strand_minus) {
        CRef<CSeq_loc> add(new CSeq_loc(*id, loc_start + 1, pos, strand));
        add->SetPartialStop(partial_start, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, scope);
    }
    return new_loc;
}


CRef<CSeq_loc> SeqLocExtend3(const CSeq_loc& loc, TSeqPos pos, CScope* scope)
{
    CSeq_loc_CI last_l(loc);
    size_t num_intervals = last_l.GetSize();
    last_l.SetPos(num_intervals - 1);
    CConstRef<CSeq_loc> last_loc = last_l.GetRangeAsSeq_loc();

    TSeqPos loc_stop = last_loc->GetStop(eExtreme_Biological);
    bool partial_stop = last_loc->IsPartialStop(eExtreme_Biological);
    ENa_strand strand = last_loc->IsSetStrand() ? last_loc->GetStrand() : eNa_strand_plus;
    CRef<CSeq_loc> new_loc(NULL);

    CRef<CSeq_id> id(new CSeq_id());
    id->Assign(last_l.GetSeq_id());

    if (pos > loc_stop && strand != eNa_strand_minus) {
        CRef<CSeq_loc> add(new CSeq_loc(*id, loc_stop + 1, pos, strand));
        add->SetPartialStop(partial_stop, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, scope);
    } else if (pos < loc_stop && strand == eNa_strand_minus) {
        CRef<CSeq_loc> add(new CSeq_loc(*id, pos, loc_stop - 1, strand));
        add->SetPartialStart(partial_stop, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, scope);
    }
    return new_loc;
}



CRef<CSeq_loc> SeqLocExtend(const CSeq_loc& loc, size_t pos, CScope* scope)
{
    TSeqPos loc_start = loc.GetStart(eExtreme_Positional);
    TSeqPos loc_stop = loc.GetStop(eExtreme_Positional);
    bool partial_start = loc.IsPartialStart(eExtreme_Positional);
    bool partial_stop = loc.IsPartialStop(eExtreme_Positional);
    ENa_strand strand = loc.GetStrand();
    CRef<CSeq_loc> new_loc(NULL);

    if (pos < loc_start) {
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*(loc.GetId()));
        CRef<CSeq_loc> add(
            new CSeq_loc(*id, static_cast<TSeqPos>(pos), loc_start - 1, strand));
        add->SetPartialStart(partial_start, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(
            loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, scope);
    } else if (pos > loc_stop) {
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*(loc.GetId()));
        CRef<CSeq_loc> add(new CSeq_loc(
            *id, loc_stop + 1, static_cast<TSeqPos>(pos), strand));
        add->SetPartialStop(partial_stop, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(
            loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, scope);
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

    any_change |= feature::AdjustFeaturePartialFlagForLocation(feat);

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

void AdjustFrameFor5Extension(CSeq_feat& feat, size_t diff)
{
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


bool CLocationEditPolicy::Extend5(CSeq_feat& feat, CScope& scope)
{
    bool extend = false;

    bool confident = false;
    if (!Is5AtEndOfSeq(feat.GetLocation(), scope, confident) && confident) {
        int diff = 0;
        CSeq_loc_CI first_l(feat.GetLocation());
        if (first_l.IsSetStrand() && first_l.GetStrand() == eNa_strand_minus) {
            CBioseq_Handle bsh = scope.GetBioseqHandle(first_l.GetSeq_id());
            diff = bsh.GetInst().GetLength() - first_l.GetRange().GetTo() - 1;
            CRef<CSeq_loc> new_loc = SeqLocExtend5(feat.GetLocation(), bsh.GetInst_Length() - 1, &scope);
            if (new_loc) {
                feat.SetLocation().Assign(*new_loc);
                extend = true;
            } else {
                diff = 0;
            }
        } else {
            diff = first_l.GetRange().GetFrom();
            CRef<CSeq_loc> new_loc = SeqLocExtend5(feat.GetLocation(), 0, &scope);
            if (new_loc) {
                feat.SetLocation().Assign(*new_loc);
                extend = true;
            } else {
                diff = 0;
            }
        }

        // adjust frame to maintain consistency
        AdjustFrameFor5Extension(feat, diff);
    }
    return extend;
}

bool CLocationEditPolicy::Extend3(CSeq_feat& feat, CScope& scope)
{
    bool extend = false;

    bool confident = false;
    if (!Is3AtEndOfSeq(feat.GetLocation(), scope, confident) && confident) {
        CSeq_loc_CI last_l(feat.GetLocation());
        size_t num_intervals = last_l.GetSize();
        last_l.SetPos(num_intervals - 1);

        ENa_strand strand = last_l.GetStrand();
        if (strand == eNa_strand_minus) {
            CRef<CSeq_loc> new_loc = SeqLocExtend3(feat.GetLocation(), 0, &scope);
            if (new_loc) {
                feat.SetLocation().Assign(*new_loc);
                extend = true;
            }
        } else {
            CBioseq_Handle bsh = scope.GetBioseqHandle(last_l.GetSeq_id());
            CRef<CSeq_loc> new_loc = SeqLocExtend3(feat.GetLocation(), bsh.GetInst_Length() - 1, &scope);
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
                TSeqPos feat_start = new_feat->GetLocation().GetStart(eExtreme_Biological);
                TSeqPos feat_stop = new_feat->GetLocation().GetStop(eExtreme_Biological);
                CRef<CSeq_feat> new_gene(new CSeq_feat());
                new_gene->Assign(*old_gene);
                bool gene_change = false;
                // adjust ends of gene to match ends of feature
                CRef<CSeq_loc> new_loc = SeqLocExtend5(new_gene->GetLocation(), feat_start, &scope);
                if (new_loc) {
                    new_gene->SetLocation().Assign(*new_loc);
                    gene_change = true;
                }
                new_loc = SeqLocExtend3(new_gene->GetLocation(), feat_stop, &scope);
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
            if (!retranslate_cds || !feature::RetranslateCDS(*new_feat, scope)) {
                CSeq_loc_CI l(new_feat->GetLocation());
                feature::AdjustForCDSPartials(*new_feat, scope);
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
        auto diff = to - from + 1;
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
        auto it = pack.SetPoints().begin();
        while (it != pack.SetPoints().end()) {
            if (to < *it) {
                auto diff = to - from + 1;
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
        auto diff = insert_to - insert_from + 1;
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

    auto it = packpnt.SetPoints().begin();
    while (it != packpnt.SetPoints().end()) {
        if (from < *it) {
            auto diff = to - from + 1;
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
    TSeqPos start, TSeqPos stop,
    const CSeq_id* seqid, bool& cut,
    unsigned int options)
{
    cut = false;
    if (!OkToAdjustLoc(before, seqid)) {
        return CRef<CSeq_interval>(NULL);
    }
    // These are required fields
    if (!(before.CanGetFrom() && before.CanGetTo()))
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

    if (feat_from > stop && !(options & eSplitLocOption_split_in_intron)) {
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
                         TSeqPos start, TSeqPos stop,
                         const CSeq_id* seqid, unsigned int options)
{
    if (before_intervals.IsSet()) {
        if (before_intervals.IsReverseStrand()) {
            reverse(before_intervals.Set().begin(), before_intervals.Set().end());
        }

        auto it = before_intervals.Set().begin();
        while (it != before_intervals.Set().end()) {
            CSeq_interval& sub_interval = **it;

            TSeqPos int_from = sub_interval.GetFrom();
            if (int_from > stop && after_intervals.IsSet() && !after_intervals.Get().empty()) {
                after_intervals.Set().push_back(Ref(&sub_interval));
                it = before_intervals.Set().erase(it);
            }
            else {
                bool cut = false;
                CRef<CSeq_interval> after = SplitLocationForGap(sub_interval, start, stop, seqid, cut, options);

                // Should interval be deleted from list?
                if (cut) {
                    it = before_intervals.Set().erase(it);
                }
                else {
                    ++it;
                }
                if (after) {
                    after_intervals.Set().push_back(after);
                }
            }
        }

        if (after_intervals.IsReverseStrand()) {
            reverse(after_intervals.Set().begin(), after_intervals.Set().end());
        }
        if (before_intervals.IsReverseStrand()) {
            reverse(before_intervals.Set().begin(), before_intervals.Set().end());
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
                CRef<CSeq_interval> after = SplitLocationForGap(loc1.SetInt(),
                    static_cast<TSeqPos>(start), static_cast<TSeqPos>(stop),
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
                    static_cast<TSeqPos>(start), static_cast<TSeqPos>(stop),
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
                if (before_mix.IsReverseStrand()) {
                    reverse(before_mix.Set().begin(), before_mix.Set().end());
                }
                auto it = before_mix.Set().begin();
                while (it != before_mix.Set().end()) {
                    CSeq_loc& sub_loc = **it;

                    TSeqPos from = sub_loc.GetStart(eExtreme_Positional);
                    if (from > stop && after_mix->IsSet() && !after_mix->Get().empty()) {
                        after_mix->Set().push_back(Ref(&sub_loc));
                        it = before_mix.Set().erase(it);
                    }
                    else {
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
                        }
                    }
                }

                if (after_mix->IsReverseStrand()) {
                    reverse(after_mix->Set().begin(), after_mix->Set().end());
                }
                if (before_mix.IsReverseStrand()) {
                    reverse(before_mix.Set().begin(), before_mix.Set().end());
                }

                // Update the original list
                if (before_mix.Set().empty()) {
                    loc1.Reset();
                }
                if (!after_mix->Set().empty()) {
                    if (loc2.Which() == CSeq_loc::e_not_set) {
                        loc2.SetMix().Assign(*after_mix);
                    }
                    else {
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
        switch (feat.GetData().GetSubtype()) {
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
        switch (feat.GetData().GetSubtype()) {
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


bool s_PPntComparePlus(const TSeqPos& p1, const TSeqPos& p2)
{
    return (p1 < p2);
}


bool s_PPntCompareMinus(const TSeqPos& p1, const TSeqPos& p2)
{
    return (p1 > p2);
}


bool CorrectIntervalOrder(CPacked_seqpnt& ppnt)
{
    bool rval = false;
    if (!ppnt.IsSetPoints()) {
        // nothing to do
    } else if (!ppnt.IsSetStrand() || ppnt.GetStrand() == eNa_strand_plus || ppnt.GetStrand() == eNa_strand_unknown) {
        if (!seq_mac_is_sorted(ppnt.GetPoints().begin(), ppnt.GetPoints().end(), s_PPntComparePlus)) {
            stable_sort(ppnt.SetPoints().begin(), ppnt.SetPoints().end(), s_PPntComparePlus);
            rval = true;
        }
    } else if (ppnt.IsSetStrand() && ppnt.GetStrand() == eNa_strand_minus) {
        if (!seq_mac_is_sorted(ppnt.GetPoints().begin(), ppnt.GetPoints().end(), s_PPntCompareMinus)) {
            stable_sort(ppnt.SetPoints().begin(), ppnt.SetPoints().end(), s_PPntCompareMinus);
            rval = true;
        }
    }
    return rval;
}


bool s_StrandsConsistent(const CSeq_interval& a, const CSeq_interval& b)
{
    if (a.IsSetStrand() && a.GetStrand() == eNa_strand_minus) {
        if (!b.IsSetStrand() || b.GetStrand() != eNa_strand_minus) {
            return false;
        } else {
            return true;
        }
    } else if (b.IsSetStrand() && b.GetStrand() == eNa_strand_minus) {
        return false;
    } else {
        return true;
    }
}


bool CorrectIntervalOrder(CPacked_seqint& pint)
{
    if (pint.Get().size() < 2) {
        return false;
    }
    bool any_change = false;

    bool this_change = true;
    while (this_change) {
        this_change = false;

        // can only swap elements if they have the same strand and Seq-id
        CPacked_seqint::Tdata::iterator a = pint.Set().begin();
        CPacked_seqint::Tdata::iterator b = a;
        b++;
        while (b != pint.Set().end()) {
            if ((*a)->IsSetId() && (*b)->IsSetId() &&
                (*a)->GetId().Equals((*b)->GetId()) &&
                (*a)->IsSetFrom() && (*a)->IsSetTo() && (*a)->GetFrom() < (*a)->GetTo() &&
                (*b)->IsSetFrom() && (*b)->IsSetTo() && (*b)->GetFrom() < (*b)->GetTo() &&
                s_StrandsConsistent(**a, **b)) {
                if ((*a)->IsSetStrand() && (*a)->GetStrand() == eNa_strand_minus) {
                    if ((*b)->GetTo() > (*a)->GetFrom()) {
                        CRef<CSeq_interval> swp(a->GetPointer());
                        a->Reset(b->GetPointer());
                        b->Reset(swp.GetPointer());
                        this_change = true;
                        any_change = true;
                    }
                } else {
                    if ((*b)->GetTo() < (*a)->GetFrom()) {
                        CRef<CSeq_interval> swp(a->GetPointer());
                        a->Reset(b->GetPointer());
                        b->Reset(swp.GetPointer());
                        this_change = true;
                        any_change = true;
                    }
                }
            }
            ++a;
            ++b;
        }
    }
    return any_change;
}


bool OneIdOneStrand(const CSeq_loc& loc, const CSeq_id** id, ENa_strand& strand)
{
    try {
        CSeq_loc_CI li(loc);
        *id = &(li.GetSeq_id());
        strand = li.IsSetStrand() ? li.GetStrand() : eNa_strand_plus;
        if (strand == eNa_strand_unknown) {
            strand = eNa_strand_plus;
        }
        if (strand != eNa_strand_plus && strand != eNa_strand_minus) {
            return false;
        }
        ++li;
        while (li) {
            if (!li.GetSeq_id().Equals(**id)) {
                return false;
            }
            ENa_strand this_strand = li.IsSetStrand() ? li.GetStrand() : eNa_strand_plus;
            if (this_strand == eNa_strand_unknown) {
                this_strand = eNa_strand_plus;
            }
            if (this_strand != strand) {
                return false;
            }
            ++li;
        }
        return true;
    } catch (CException&) {
        return false;
    }
}


bool CorrectIntervalOrder(CSeq_loc::TMix::Tdata& mix)
{
    bool any_change = false;
    NON_CONST_ITERATE(CSeq_loc::TMix::Tdata, it, mix) {
        any_change |= CorrectIntervalOrder(**it);
    }
    if (mix.size() < 2) {
        return any_change;
    }
    bool this_change = true;
    while (this_change) {
        this_change = false;

        // can only swap elements if they have the same strand and Seq-id
        CSeq_loc::TMix::Tdata::iterator a = mix.begin();
        CSeq_loc::TMix::Tdata::iterator b = a;
        b++;
        while (b != mix.end()) {
            try {
                const CSeq_id* a_id;
                const CSeq_id* b_id;
                ENa_strand a_strand;
                ENa_strand b_strand;
                if (OneIdOneStrand(**a, &a_id, a_strand) &&
                    OneIdOneStrand(**b, &b_id, b_strand) &&
                    a_id->Equals(*b_id) &&
                    a_strand == b_strand) {
                    if (a_strand == eNa_strand_plus) {
                        if ((*a)->GetStart(eExtreme_Biological) > (*b)->GetStop(eExtreme_Biological)) {
                            CRef<CSeq_loc> swp(a->GetPointer());
                            a->Reset(b->GetPointer());
                            b->Reset(swp.GetPointer());
                            this_change = true;
                            any_change = true;
                        }
                    } else if (a_strand == eNa_strand_minus) {
                        if ((*a)->GetStart(eExtreme_Biological) < (*b)->GetStop(eExtreme_Biological)) {
                            CRef<CSeq_loc> swp(a->GetPointer());
                            a->Reset(b->GetPointer());
                            b->Reset(swp.GetPointer());
                            this_change = true;
                            any_change = true;
                        }
                    }
                }
            } catch (CException&) {
                // not just one id
            }
            ++a;
            ++b;
        }
    }
    return any_change;
}


bool CorrectIntervalOrder(CSeq_loc& loc)
{
    bool any_change = false;
    switch (loc.Which()) {
        case CSeq_loc::e_Bond:
        case CSeq_loc::e_Empty:
        case CSeq_loc::e_Equiv:
        case CSeq_loc::e_Feat:
        case CSeq_loc::e_Int:
        case CSeq_loc::e_not_set:
        case CSeq_loc::e_Null:
        case CSeq_loc::e_Pnt:
        case CSeq_loc::e_Whole:
            // nothing to do
            break;
        case CSeq_loc::e_Mix:
            any_change = CorrectIntervalOrder(loc.SetMix().Set());
            break;
        case CSeq_loc::e_Packed_int:
            any_change = CorrectIntervalOrder(loc.SetPacked_int());
            break;
        case CSeq_loc::e_Packed_pnt:
            any_change = CorrectIntervalOrder(loc.SetPacked_pnt());
            break;
    }
    return any_change;
}


bool IsExtendableLeft(TSeqPos left, const CBioseq& seq, CScope* scope, TSeqPos& extend_len)
{
    bool rval = false;
    if (left < 3) {
        rval = true;
        extend_len = left;
    } else if (seq.IsSetInst() && seq.GetInst().IsSetRepr() &&
               seq.GetInst().GetRepr() == CSeq_inst::eRepr_delta &&
               seq.GetInst().IsSetExt() &&
               seq.GetInst().GetExt().IsDelta()) {
        TSeqPos offset = 0;
        TSeqPos last_gap_stop = 0;
        ITERATE(CDelta_ext::Tdata, it, seq.GetInst().GetExt().GetDelta().Get()) {
            if ((*it)->IsLiteral()) {
                offset += (*it)->GetLiteral().GetLength();
                if (!(*it)->GetLiteral().IsSetSeq_data()) {
                    last_gap_stop = offset;
                } else if ((*it)->GetLiteral().GetSeq_data().IsGap()) {
                    last_gap_stop = offset;
                }
            } else if ((*it)->IsLoc()) {
                offset += sequence::GetLength((*it)->GetLoc(), scope);
            }
            if (offset > left) {
                break;
            }
        }
        if (left >= last_gap_stop && left - last_gap_stop <= 3) {
            rval = true;
            extend_len = left - last_gap_stop;
        }
    }
    return rval;
}


bool IsExtendableRight(TSeqPos right, const CBioseq& seq, CScope* scope, TSeqPos& extend_len)
{
    bool rval = false;
    if (right > seq.GetLength() - 5) {
        rval = true;
        extend_len = seq.GetLength() - right - 1;
    } else if (seq.IsSetInst() && seq.GetInst().IsSetRepr() &&
        seq.GetInst().GetRepr() == CSeq_inst::eRepr_delta &&
        seq.GetInst().IsSetExt() &&
        seq.GetInst().GetExt().IsDelta()) {
        TSeqPos offset = 0;
        TSeqPos next_gap_start = 0;
        ITERATE(CDelta_ext::Tdata, it, seq.GetInst().GetExt().GetDelta().Get()) {
            if ((*it)->IsLiteral()) {
                if (!(*it)->GetLiteral().IsSetSeq_data()) {
                    next_gap_start = offset;
                } else if ((*it)->GetLiteral().GetSeq_data().IsGap()) {
                    next_gap_start = offset;
                }
                offset += (*it)->GetLiteral().GetLength();
            } else if ((*it)->IsLoc()) {
                offset += sequence::GetLength((*it)->GetLoc(), scope);
            }
            if (offset > right + 4) {
                break;
            }
        }
        if (next_gap_start > right && next_gap_start - right - 1 <= 3) {
            rval = true;
            extend_len = next_gap_start - right - 1;
        }
    }
    return rval;
}


bool AdjustFeatureEnd5(CSeq_feat& cds, vector<CRef<CSeq_feat> > related_features, CScope& scope)
{
    if (!cds.GetLocation().IsPartialStart(eExtreme_Biological)) {
        return false;
    }

    bool rval = false;

    CSeq_loc_CI first_l(cds.GetLocation());
    CBioseq_Handle bsh = scope.GetBioseqHandle(first_l.GetSeq_id());
    CConstRef<CBioseq> seq = bsh.GetCompleteBioseq();

    TSeqPos start = cds.GetLocation().GetStart(eExtreme_Biological);
    TSeqPos new_start = start;
    TSeqPos extend_len = 0;
    bool extendable = false;

    if (!first_l.IsSetStrand() || first_l.GetStrand() != eNa_strand_minus) {
        // positive strand
        if (start > 0) {
            extendable = IsExtendableLeft(start, *seq, &scope, extend_len);
            new_start = start - extend_len;
        }
    } else {
        if (start < seq->GetInst().GetLength() - 1) {
            extendable = IsExtendableRight(start, *seq, &scope, extend_len);
            new_start = start + extend_len;
        }
    }
    if (extendable) {
        CRef<CSeq_loc> cds_loc = SeqLocExtend5(cds.GetLocation(), new_start, &scope);
        if (cds_loc) {
            for (auto it : related_features) {
                if (it->GetLocation().GetStart(eExtreme_Biological) == start) {
                    CRef<CSeq_loc> related_loc = SeqLocExtend5(it->GetLocation(), new_start, &scope);
                    if (related_loc) {
                        it->SetLocation().Assign(*related_loc);
                        if (it->IsSetData() && it->GetData().IsCdregion()) {
                            AdjustFrameFor5Extension(cds, extend_len);
                        }
                    }
                }
            }
            cds.SetLocation().Assign(*cds_loc);
            if (cds.IsSetData() && cds.GetData().IsCdregion()) {
                AdjustFrameFor5Extension(cds, extend_len);
            }

            rval = true;
        }
    }

    return rval;
}


bool AdjustFeatureEnd3(CSeq_feat& cds, vector<CRef<CSeq_feat> > related_features, CScope& scope)
{
    if (!cds.GetLocation().IsPartialStop(eExtreme_Biological)) {
        return false;
    }

    bool rval = false;

    CSeq_loc_CI last_l(cds.GetLocation());
    size_t num_intervals = last_l.GetSize();
    last_l.SetPos(num_intervals - 1);

    CBioseq_Handle bsh = scope.GetBioseqHandle(last_l.GetSeq_id());
    CConstRef<CBioseq> seq = bsh.GetCompleteBioseq();

    TSeqPos stop = cds.GetLocation().GetStop(eExtreme_Biological);
    TSeqPos new_stop = stop;
    TSeqPos extend_len = 0;
    bool extendable = false;

    if (!last_l.IsSetStrand() || last_l.GetStrand() != eNa_strand_minus) {
        // positive strand
        if (stop < seq->GetInst().GetLength() - 1) {
            extendable = IsExtendableRight(stop, *seq, &scope, extend_len);
            new_stop = stop + extend_len;
        }
    } else {
        if (stop > 0) {
            extendable = IsExtendableLeft(stop, *seq, &scope, extend_len);
            new_stop = stop - extend_len;
        }
    }
    if (extendable) {
        CRef<CSeq_loc> cds_loc = SeqLocExtend3(cds.GetLocation(), new_stop, &scope);
        if (cds_loc) {
            for (auto it : related_features) {
                if (it->GetLocation().GetStop(eExtreme_Biological) == stop) {
                    CRef<CSeq_loc> related_loc = SeqLocExtend3(it->GetLocation(), new_stop, &scope);
                    if (related_loc) {
                        it->SetLocation().Assign(*related_loc);
                    }
                }
            }
            cds.SetLocation().Assign(*cds_loc);
            rval = true;
        }
    }

    return rval;
}


bool IsExtendable(const CSeq_feat& cds, CScope& scope)
{
    if (cds.GetLocation().IsPartialStart(eExtreme_Positional)) {
        CSeq_loc_CI first_l(cds.GetLocation());
        CBioseq_Handle bsh = scope.GetBioseqHandle(first_l.GetSeq_id());
        CConstRef<CBioseq> seq = bsh.GetCompleteBioseq();
        TSeqPos extend_len = 0;
        TSeqPos start = first_l.GetRange().GetFrom();
        if (IsExtendableLeft(start, *seq, &scope, extend_len) && extend_len > 0) {
            return true;
        }
    }
    if (cds.GetLocation().IsPartialStop(eExtreme_Positional)) {
        CSeq_loc_CI last_l(cds.GetLocation());
        size_t num_intervals = last_l.GetSize();
        last_l.SetPos(num_intervals - 1);
        CBioseq_Handle bsh = scope.GetBioseqHandle(last_l.GetSeq_id());
        CConstRef<CBioseq> seq = bsh.GetCompleteBioseq();

        TSeqPos stop = cds.GetLocation().GetStop(eExtreme_Positional);
        TSeqPos extend_len = 0;

        if (IsExtendableRight(stop, *seq, &scope, extend_len) && extend_len > 0) {
            return true;
        }
    }
    return false;
}


bool ExtendPartialFeatureEnds(CBioseq_Handle bsh)
{
    bool any_change = false;
    CFeat_CI f(bsh);
    CRef<feature::CFeatTree> tr(new feature::CFeatTree(f));
    while (f) {
        if (f->GetData().IsCdregion()) {
            CMappedFeat gene = tr->GetBestGene(*f);
            CMappedFeat mRNA = tr->GetParent(*f, CSeqFeatData::eSubtype_mRNA);
            vector<CRef<CSeq_feat> > related_features;
            if (gene) {
                CRef<CSeq_feat> new_gene(new CSeq_feat());
                new_gene->Assign(*(gene.GetOriginalSeq_feat()));
                related_features.push_back(new_gene);
            }
            if (mRNA) {
                CRef<CSeq_feat> new_mRNA(new CSeq_feat());
                new_mRNA->Assign(*(mRNA.GetOriginalSeq_feat()));
                related_features.push_back(new_mRNA);
            }
            CRef<CSeq_feat> new_cds(new CSeq_feat());
            new_cds->Assign(*(f->GetOriginalSeq_feat()));

            const bool adjusted_5prime = AdjustFeatureEnd5(*new_cds, related_features, bsh.GetScope());
            const bool adjusted_3prime = AdjustFeatureEnd3(*new_cds, related_features, bsh.GetScope());

            if (adjusted_5prime || adjusted_3prime) {
                feature::RetranslateCDS(*new_cds, bsh.GetScope());
                CSeq_feat_EditHandle feh(*f);
                feh.Replace(*new_cds);
                if (gene) {
                    CSeq_feat_EditHandle geh(gene);
                    geh.Replace(*(related_features.front()));
                }
                if (mRNA) {
                    CSeq_feat_EditHandle meh(mRNA);
                    meh.Replace(*(related_features.back()));
                }
                any_change = true;
            }
        }
        ++f;
    }
    return any_change;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

