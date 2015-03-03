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
*   functions for editing and working with coding regions
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/edit/cds_fix.hpp>
#include <objtools/edit/loc_edit.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/seqfeat_macros.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

unsigned char GetCodeBreakCharacter(const CCode_break& cbr)
{
    unsigned char ex = 0;
    vector<char> seqData;
    string str = "";

    if (!cbr.IsSetAa()) {
        return ex;
    }
    switch (cbr.GetAa().Which()) {
        case CCode_break::C_Aa::e_Ncbi8aa:
            str = cbr.GetAa().GetNcbi8aa();
            CSeqConvert::Convert(str, CSeqUtil::e_Ncbi8aa, 0, str.size(), seqData, CSeqUtil::e_Ncbieaa);
            ex = seqData[0];
            break;
        case CCode_break::C_Aa::e_Ncbistdaa:
            str = cbr.GetAa().GetNcbi8aa();
            CSeqConvert::Convert(str, CSeqUtil::e_Ncbistdaa, 0, str.size(), seqData, CSeqUtil::e_Ncbieaa);
            ex = seqData[0];
            break;
        case CCode_break::C_Aa::e_Ncbieaa:
            seqData.push_back(cbr.GetAa().GetNcbieaa());
            ex = seqData[0];
            break;
        default:
            // do nothing, code break wasn't actually set

            break;
    }
    return ex;
}


bool DoesCodingRegionHaveTerminalCodeBreak(const objects::CCdregion& cdr)
{
    if (!cdr.IsSetCode_break()) {
        return false;
    }
    bool rval = false;
    ITERATE(objects::CCdregion::TCode_break, it, cdr.GetCode_break()) {
        if (GetCodeBreakCharacter(**it) == '*') {
            rval = true;
            break;
        }
    }
    return rval;
}


TSeqPos GetLastPartialCodonLength(const objects::CSeq_feat& cds, objects::CScope& scope)
{
    if (!cds.IsSetData() || !cds.GetData().IsCdregion()) {
        return 0;
    }
    // find the length of the last partial codon.
    const objects::CCdregion& cdr = cds.GetData().GetCdregion();
    int dna_len = sequence::GetLength(cds.GetLocation(), &scope);
    TSeqPos except_len = 0;
    if (cds.GetLocation().IsPartialStart(eExtreme_Biological) && cdr.IsSetFrame()) {
        switch (cdr.GetFrame()) {
            case objects::CCdregion::eFrame_two:
                except_len = (dna_len - 1) % 3;
                break;
            case objects::CCdregion::eFrame_three:
                except_len = (dna_len - 2) % 3;
                break;
            default:
                except_len = dna_len % 3;
                break;
        }
    } else {
        except_len = dna_len % 3;
    }
    return except_len;
}


CRef<CSeq_loc> GetLastCodonLoc(const CSeq_feat& cds, CScope& scope)
{
    TSeqPos except_len = GetLastPartialCodonLength(cds, scope);
    if (except_len == 0) {
        except_len = 3;
    }
    const CSeq_loc& cds_loc = cds.GetLocation();
    TSeqPos stop = cds_loc.GetStop(eExtreme_Biological);
    CRef<CSeq_id> new_id(new CSeq_id());
    new_id->Assign(*(cds_loc.GetId()));
    CRef<CSeq_loc> codon_loc(new CSeq_loc());
    codon_loc->SetInt().SetId(*new_id);    
    if (cds_loc.GetStrand() == eNa_strand_minus) {
        codon_loc->SetInt().SetFrom(stop);
        codon_loc->SetInt().SetTo(stop + except_len - 1);
        codon_loc->SetInt().SetStrand(eNa_strand_minus);
    } else {
        codon_loc->SetInt().SetFrom(stop - except_len + 1);
        codon_loc->SetInt().SetTo(stop);
    }
    return codon_loc;
}


bool AddTerminalCodeBreak(CSeq_feat& cds, CScope& scope)
{
    CRef<CSeq_loc> codon_loc = GetLastCodonLoc(cds, scope);
    if (!codon_loc) {
        return false;
    }

    CRef<objects::CCode_break> cbr(new objects::CCode_break());
    cbr->SetAa().SetNcbieaa('*');
    cbr->SetLoc().Assign(*codon_loc);
    cds.SetData().SetCdregion().SetCode_break().push_back(cbr);
    return true;
}


bool DoesCodingRegionEndWithStopCodon(const objects::CSeq_feat& cds, objects::CScope& scope)
{
    string transl_prot;
    bool alt_start = false;
    bool rval = false;
    try {
        CSeqTranslator::Translate(cds, scope, transl_prot,
                                    true,   // include stop codons
                                    false,  // do not remove trailing X/B/Z
                                    &alt_start);
        if (NStr::EndsWith(transl_prot, "*")) {
            rval = true;
        }
    } catch (CException&) {
        // can't translate
    }
    return rval;
}


void ExtendStop(CSeq_loc& loc, TSeqPos len, CScope& scope)
{
    if (len == 0) {
        return;
    }
    TSeqPos stop = loc.GetStop(eExtreme_Biological);
    CRef<CSeq_loc> add(new CSeq_loc());
    add->SetInt().SetId().Assign(*(loc.GetId()));
    if (loc.GetStrand() == eNa_strand_minus) {
        add->SetInt().SetFrom(stop - len);
        add->SetInt().SetTo(stop - 1);
        add->SetInt().SetStrand(eNa_strand_minus);
    } else {
        add->SetInt().SetId().Assign(*(loc.GetId()));
        add->SetInt().SetFrom(stop + 1);
        add->SetInt().SetTo(stop + len);
    }
    loc.Assign(*(sequence::Seq_loc_Add(loc, *add, CSeq_loc::fMerge_All|CSeq_loc::fSort, &scope)));
}


TSeqPos ExtendLocationForTranslExcept(objects::CSeq_loc& loc, objects::CScope& scope)
{
    CBioseq_Handle bsh = scope.GetBioseqHandle(loc);
    TSeqPos stop = loc.GetStop(eExtreme_Biological);
    TSeqPos len = 0;
    TSeqPos except_len = 0;
    CRef<CSeq_loc> overhang(new CSeq_loc());
    overhang->SetInt().SetId().Assign(*loc.GetId());

    if (loc.GetStrand() == eNa_strand_minus) {                
        if (stop < 3) {
            len = stop;
        } else {
            len = 3;
        }
        if (len > 0) {
            overhang->SetInt().SetFrom(0);
            overhang->SetInt().SetTo(stop - 1);
            overhang->SetStrand(eNa_strand_minus);
        }
    } else {
        len = bsh.GetBioseqLength() - stop;
        if (len > 3) {
            len = 3;
        }
        if (len > 0) {
            overhang->SetInt().SetFrom(stop + 1);
            overhang->SetInt().SetTo(stop + len);
        }
    }
    if (len > 0) {
        CSeqVector vec(*overhang, scope, CBioseq_Handle::eCoding_Iupac);
        string seq_string;
        vec.GetSeqData(0, len - 1, seq_string);   
        if (vec[0] == 'T') {
            except_len++;
            if (len > 1 && vec[1] == 'A') {
                except_len++;
                if (len > 2 && vec[2] == 'A') {
                    // adding a real stop codon
                    except_len ++;
                }
            }            
        }
    } 
    // extend
    if (except_len > 0) {
        ExtendStop(loc, except_len, scope);
    }
    return except_len;
}


bool IsOverhangOkForTerminalCodeBreak(const CSeq_feat& cds, CScope& scope, bool strict)
{
    CRef<CSeq_loc> loc = GetLastCodonLoc(cds, scope);
    if (!loc) {
        return false;
    }
    TSeqPos len = sequence::GetLength(*loc, &scope);
    CSeqVector vec(*loc, scope, CBioseq_Handle::eCoding_Iupac);
    bool rval = true;
    if (strict) {
        if (vec[0] != 'T') {
            rval = false;
        } else if (len > 1 && vec[1] != 'A') {
            rval = false;
        }
    } else {
        if (vec[0] != 'T' && vec[0] != 'N') {
            rval = false;
        }
    }
    return rval;
}


/// SetTranslExcept
/// A function to set a code break at the 3' end of a coding region to indicate
/// that the stop codon is formed by the addition of a poly-A tail.
/// @param cds      The coding region to be adjusted (if necessary)
/// @param comment  The string to place in the note on cds if a code break is added
/// @param strict   Only add code break if last partial codon consists of "TA" or just "T".
///                 If strict is false, add code break if first NT of last partial codon
///                 is T or N.
/// @param extend   If true, extend coding region to cover partial stop codon
bool SetTranslExcept(objects::CSeq_feat& cds, const string& comment, bool strict, bool extend, objects::CScope& scope)
{
    // do nothing if this isn't a coding region
    if (!cds.IsSetData() || !cds.GetData().IsCdregion()) {
        return false;
    }
    // do nothing if coding region is 3' partial
    if (cds.GetLocation().IsPartialStop(eExtreme_Biological)) {
        return false;
    }

    objects::CCdregion& cdr = cds.SetData().SetCdregion();
    // do nothing if coding region already has terminal code break
    if (DoesCodingRegionHaveTerminalCodeBreak(cdr)) {
        return false;
    }

    // find the length of the last partial codon.
    TSeqPos except_len = GetLastPartialCodonLength(cds, scope);

    bool extended = false;
    if (except_len == 0 && extend && !DoesCodingRegionEndWithStopCodon(cds, scope)) {
        except_len = ExtendLocationForTranslExcept(cds.SetLocation(), scope);
        if (except_len > 0) {
            extended = true;
        }
    }

    if (except_len == 0) {
        return false;
    }

    bool added_code_break = false;
    if (!extend || !DoesCodingRegionEndWithStopCodon(cds, scope)) {
        // TODO: check for strictness
        if (IsOverhangOkForTerminalCodeBreak(cds, scope, strict)
            && AddTerminalCodeBreak(cds, scope)) {
            if (!NStr::IsBlank(comment)) {
                if (cds.IsSetComment() && !NStr::IsBlank(cds.GetComment())) {
                    string orig_comment = cds.GetComment();
                    if (NStr::Find(orig_comment, comment) == string::npos) {
                        cds.SetComment(cds.GetComment() + ";" + comment);
                    }
                } else {
                    cds.SetComment(comment);
                }
            }
            added_code_break = true;
        }
    }

    return extended || added_code_break;
}


/// AdjustProteinMolInfoToMatchCDS
/// A function to change an existing MolInfo to match a coding region
/// @param molinfo  The MolInfo to be adjusted (if necessary)
/// @param cds      The CDS to match
///
/// @return         Boolean to indicate whether molinfo was changed

bool AdjustProteinMolInfoToMatchCDS(CMolInfo& molinfo, const CSeq_feat& cds)
{
    bool rval = false;
    if (!molinfo.IsSetBiomol() || molinfo.GetBiomol() != CMolInfo::eBiomol_peptide) {
        molinfo.SetBiomol(CMolInfo::eBiomol_peptide);
        rval = true;
    }

    bool partial5 = cds.GetLocation().IsPartialStart(eExtreme_Biological);
    bool partial3 = cds.GetLocation().IsPartialStop(eExtreme_Biological);
    CMolInfo::ECompleteness completeness = CMolInfo::eCompleteness_complete;
    if (partial5 && partial3) {
        completeness = CMolInfo::eCompleteness_no_ends;
    } else if (partial5) {
        completeness = CMolInfo::eCompleteness_no_left;
    } else if (partial3) {
        completeness = CMolInfo::eCompleteness_no_right;
    }
    if (!molinfo.IsSetCompleteness() || molinfo.GetCompleteness() != completeness) {
        molinfo.SetCompleteness(completeness);
        rval = true;
    }
    return rval;
}


/// AdjustProteinFeaturePartialsToMatchCDS
/// A function to change an existing MolInfo to match a coding region
/// @param new_prot  The protein feature to be adjusted (if necessary)
/// @param cds       The CDS to match
///
/// @return          Boolean to indicate whether the protein feature was changed
bool AdjustProteinFeaturePartialsToMatchCDS(CSeq_feat& new_prot, const CSeq_feat& cds)
{
    bool any_change = false;
    bool partial5 = cds.GetLocation().IsPartialStart(eExtreme_Biological);
    bool partial3 = cds.GetLocation().IsPartialStop(eExtreme_Biological);
    bool prot_5 = new_prot.GetLocation().IsPartialStart(eExtreme_Biological);
    bool prot_3 = new_prot.GetLocation().IsPartialStop(eExtreme_Biological);
    if ((partial5 && !prot_5) || (!partial5 && prot_5)
        || (partial3 && !prot_3) || (!partial3 && prot_3)) {
        new_prot.SetLocation().SetPartialStart(partial5, eExtreme_Biological);
        new_prot.SetLocation().SetPartialStop(partial3, eExtreme_Biological);
        any_change = true;
    }
    any_change |= AdjustFeaturePartialFlagForLocation(new_prot);
    return any_change;
}


/// AdjustFeaturePartialFlagForLocation
/// A function to ensure that Seq-feat.partial is set if either end of the
/// feature is partial, and clear if neither end of the feature is partial
/// @param new_feat   The feature to be adjusted (if necessary)
///
/// @return           Boolean to indicate whether the feature was changed
bool AdjustFeaturePartialFlagForLocation(CSeq_feat& new_feat)
{
    bool any_change = false;
    bool partial5 = new_feat.GetLocation().IsPartialStart(eExtreme_Biological);
    bool partial3 = new_feat.GetLocation().IsPartialStop(eExtreme_Biological);
    bool should_be_partial = partial5 || partial3;
    bool is_partial = false;
    if (new_feat.IsSetPartial() && new_feat.GetPartial()) {
        is_partial = true;
    }
    if (should_be_partial && !is_partial) {
        new_feat.SetPartial(true);
        any_change = true;
    } else if (!should_be_partial && is_partial) {
        new_feat.ResetPartial();        
        any_change = true;
    }
    return any_change;
}


/// AdjustForCDSPartials
/// A function to make all of the necessary related changes to
/// a Seq-entry after the partialness of a coding region has been
/// changed.
/// @param cds        The feature for which adjustments are to be made
/// @param seh        The Seq-entry-handle to be adjusted (if necessary)
///
/// @return           Boolean to indicate whether the Seq-entry-handle was changed
bool AdjustForCDSPartials(const CSeq_feat& cds, CSeq_entry_Handle seh)
{
    if (!cds.IsSetProduct() || !seh) {
        return false;
    }

    // find Bioseq for product
    CBioseq_Handle product = seh.GetScope().GetBioseqHandle(cds.GetProduct());
    if (!product) {
        return false;
    }

    bool any_change = false;
    // adjust protein feature
    CFeat_CI f(product, SAnnotSelector(CSeqFeatData::eSubtype_prot));
    if (f) {
        // This is necessary, to make sure that we are in "editing mode"
        const CSeq_annot_Handle& annot_handle = f->GetAnnot();
        CSeq_entry_EditHandle eh = annot_handle.GetParentEntry().GetEditHandle();
        CSeq_feat_EditHandle feh(*f);
        CRef<CSeq_feat> new_feat(new CSeq_feat());
        new_feat->Assign(*(f->GetSeq_feat()));
        if (AdjustProteinFeaturePartialsToMatchCDS(*new_feat, cds)) {
            feh.Replace(*new_feat);
            any_change = true;
        }
    }        

    // change or create molinfo on protein bioseq
    bool found = false;
    CBioseq_EditHandle beh = product.GetEditHandle();
    NON_CONST_ITERATE(CBioseq::TDescr::Tdata, it, beh.SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            any_change |= AdjustProteinMolInfoToMatchCDS((*it)->SetMolinfo(), cds);
            found = true;
        }
    }
    if (!found) {
        CRef<objects::CSeqdesc> new_molinfo_desc( new CSeqdesc );
        AdjustProteinMolInfoToMatchCDS(new_molinfo_desc->SetMolinfo(), cds);
        beh.SetDescr().Set().push_back(new_molinfo_desc);
        any_change = true;
    }

    return any_change;
}


/// AdjustForCDSPartials
/// A function to make all of the necessary related changes to
/// a protein product sequence after a coding region has been
/// changed.
/// @param cds        The feature for which adjustments are to be made
/// @param seh        The scope in which adjustments are to be made (if necessary)
///
/// @return           Boolean to indicate whether the record was changed
bool RetranslateCDS(const CSeq_feat& cds, CScope& scope)
{
    // feature must be cds and already have product
    if (!cds.IsSetData() || !cds.GetData().IsCdregion() || !cds.IsSetProduct()) { 
        return false;
    }

    // Use Cdregion.Product to get handle to protein bioseq 
    CBioseq_Handle prot_bsh = scope.GetBioseqHandle(cds.GetProduct());

    // Should be a protein!
    if (!prot_bsh || !prot_bsh.IsProtein())
    {
        return false;
    }

    CBioseq_EditHandle peh = prot_bsh.GetEditHandle();
    CRef<CBioseq> new_protein = CSeqTranslator::TranslateToProtein(cds, scope);
    if (new_protein && new_protein->IsSetInst()) {
        CRef<CSeq_inst> new_inst(new CSeq_inst());
        new_inst->Assign(new_protein->GetInst());
        peh.SetInst(*new_inst);
    }

    // If protein feature exists, update location
    CFeat_CI f(prot_bsh, SAnnotSelector(CSeqFeatData::eSubtype_prot));
    if (f) {
        // This is necessary, to make sure that we are in "editing mode"
        const CSeq_annot_Handle& annot_handle = f->GetAnnot();
        CSeq_entry_EditHandle eh = annot_handle.GetParentEntry().GetEditHandle();
        CSeq_feat_EditHandle feh(*f);
        CRef<CSeq_feat> new_feat(new CSeq_feat());
        new_feat->Assign(*(f->GetSeq_feat()));
        if ( new_feat->CanGetLocation() &&
             new_feat->GetLocation().IsInt() &&
             new_feat->GetLocation().GetInt().CanGetTo() )
        {
            new_feat->SetLocation().SetInt().SetTo(
                new_protein->GetLength() - 1);
            feh.Replace(*new_feat);
        }
    }        

    AdjustForCDSPartials(cds, peh.GetSeq_entry_Handle());
    return true;
}


string s_GetProductName (const CProt_ref& prot)
{
    string prot_nm(kEmptyStr);
    if (prot.IsSetName() && prot.GetName().size() > 0) {
        prot_nm = prot.GetName().front();
    }
    return prot_nm;
}


string s_GetProductName (const CSeq_feat& cds, CScope& scope)
{
    string prot_nm(kEmptyStr);
    if (cds.IsSetProduct()) {
        CBioseq_Handle prot_bsq = sequence::GetBioseqFromSeqLoc(cds.GetProduct(), scope);
   
        if (prot_bsq) {
            CFeat_CI prot_ci(prot_bsq, CSeqFeatData::e_Prot);
            if (prot_ci) {
                prot_nm = s_GetProductName(prot_ci->GetOriginalFeature().GetData().GetProt());
            }
        }   
    } else if (cds.IsSetXref()) {
        ITERATE(CSeq_feat::TXref, it, cds.GetXref()) {
            if ((*it)->IsSetData() && (*it)->GetData().IsProt()) {
                prot_nm = s_GetProductName((*it)->GetData().GetProt());
            }
        }
    }
    return prot_nm;

}


string s_GetmRNAName (const CSeq_feat& mrna)
{
    if (!mrna.IsSetData() || mrna.GetData().GetSubtype() != CSeqFeatData::eSubtype_mRNA
        || !mrna.GetData().IsRna() || !mrna.GetData().GetRna().IsSetExt()
        || !mrna.GetData().GetRna().GetExt().IsName()) {
        return "";
    } else {
        return mrna.GetData().GetRna().GetExt().GetName();
    }
}


void s_AdjustForUTR(const CSeq_feat& utr, int cd_start, int cd_stop, CSeq_loc& mrna_loc, bool& found5, bool& found3, CScope& scope)
{
    int utr_start = utr.GetLocation().GetStart(eExtreme_Positional);
    int utr_stop = utr.GetLocation().GetStop(eExtreme_Positional);
    if ( utr.GetData().GetSubtype() == CSeqFeatData::eSubtype_5UTR )
    {
        found5 = true;
        if (abs(utr_stop - cd_start) <= 2)
        {
            mrna_loc.Assign(*SeqLocExtend(mrna_loc, utr_start, &scope));
            mrna_loc.SetPartialStart( utr.GetLocation().IsPartialStart(eExtreme_Positional), eExtreme_Positional );
        }
    }
    else if ( utr.GetData().GetSubtype() == CSeqFeatData::eSubtype_3UTR )
    {
        found3 = true;
        if ( abs(utr_start - cd_stop) <= 2)
        {
            mrna_loc.Assign(*SeqLocExtend(mrna_loc, utr_stop, &scope));
            mrna_loc.SetPartialStop( utr.GetLocation().IsPartialStop(eExtreme_Positional), eExtreme_Positional );
        }
    }
}



/// MakemRNAforCDS
/// A function to create a CSeq_feat that represents the
/// appropriate mRNA for a given CDS. Note that this feature
/// is not added to the Seq-annot in the record; this step is
/// left for the caller.
/// @param cds        The feature for which the mRNA to be made, if one is not already present
/// @param scope      The scope in which adjustments are to be made (if necessary)
///
/// @return           CRef<CSeq_feat> for new mRNA (will be NULL if one was already present)
CRef<CSeq_feat> MakemRNAforCDS(const CSeq_feat& cds, CScope& scope)
{
    CRef <CSeq_feat> new_mrna(NULL);
    string prot_nm = s_GetProductName(cds, scope);
    const CSeq_loc& cd_loc = cds.GetLocation();

    CConstRef <CSeq_feat> mrna(NULL);
    CBioseq_Handle bsh = scope.GetBioseqHandle(*cd_loc.GetId());
    CSeq_feat_Handle fh = scope.GetSeq_featHandle(cds);
    CSeq_annot_Handle sah;
    if (fh) {
        sah = fh.GetAnnot();
    }
    // can only look for overlapping mRNA with sequence::GetOverlappingmRNA
    // if Bioseq is in scope.
    if (bsh) {
        mrna = sequence::GetOverlappingmRNA(cd_loc, scope);
    } else if (sah) {
        size_t best_len = 0;
        for (CFeat_CI mrna_find(sah, CSeqFeatData::eSubtype_mRNA); mrna_find; ++mrna_find) {
            if (sequence::TestForOverlap64(mrna_find->GetLocation(), cd_loc, sequence::eOverlap_CheckIntervals) != -1) {
                size_t len = sequence::GetLength(mrna_find->GetLocation(), &scope);
                if (best_len == 0 || len < best_len) {
                    best_len = len;
                    mrna = &(mrna_find->GetOriginalFeature());
                }
            }
        }
    }

    if (!mrna || !NStr::Equal(prot_nm, s_GetmRNAName(*mrna))) {
        new_mrna.Reset (new CSeq_feat());
        new_mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
        new_mrna->SetLocation().Assign(cds.GetLocation());
        new_mrna->SetData().SetRna().SetExt().SetName(prot_nm);
      
        bool found3 = false;
        bool found5 = false;
        bool test_was_trivial = true;

        int cd_start = cd_loc.GetStart(eExtreme_Positional);
        int cd_stop = cd_loc.GetStop(eExtreme_Positional);
 
        if (bsh) {      
            for (CFeat_CI utr(bsh, CSeqFeatData::e_Imp); utr; ++utr)
            {
                s_AdjustForUTR(utr->GetOriginalFeature(), cd_start, cd_stop, 
                               new_mrna->SetLocation(), found5, found3, scope);
                test_was_trivial = false;
            }
        } else if (sah) {
            for (CFeat_CI utr(sah, CSeqFeatData::e_Imp); utr; ++utr) {
                s_AdjustForUTR(utr->GetOriginalFeature(), cd_start, cd_stop, 
                                new_mrna->SetLocation(), found5, found3, scope);
                test_was_trivial = false;
            }
        }

        //mss-329:
        // only make an assertion if there was an actual test
        if (!test_was_trivial) {
            if (!found5)
                new_mrna->SetLocation().SetPartialStart(true, eExtreme_Positional); 
            if (!found3)
                new_mrna->SetLocation().SetPartialStop(true, eExtreme_Positional); 
            new_mrna->SetPartial(!found5 || !found3);
        }
    }
    return new_mrna;
} 

/// GetmRNAforCDS
/// A function to find a CSeq_feat representing the
/// appropriate mRNA for a given CDS. 
/// @param cds        The feature for which the mRNA to be found
/// @param scope      The scope 
///
/// @return           CConstRef<CSeq_feat> for new mRNA (will be NULL if none is found)

CConstRef<CSeq_feat> GetmRNAforCDS(const CSeq_feat& cds, CScope& scope)
{
    CConstRef<CSeq_feat> mrna;
    
    bool has_xref = false;
    if (cds.IsSetXref()) {
        /* using FeatID from feature cross-references:
        * if CDS refers to an mRNA by feature ID, use that feature
        */
        CBioseq_Handle bsh = scope.GetBioseqHandle(cds.GetLocation());
        CTSE_Handle tse = bsh.GetTSE_Handle();
        FOR_EACH_SEQFEATXREF_ON_SEQFEAT (it, cds) {
            if ((*it)->IsSetId() && (*it)->GetId().IsLocal() && (*it)->GetId().GetLocal().IsId()) {
                CSeq_feat_Handle mrna_h = tse.GetFeatureWithId(CSeqFeatData::eSubtype_mRNA, (*it)->GetId().GetLocal().GetId());
                if (mrna_h) {
                    mrna = mrna_h.GetSeq_feat();
                }
                has_xref = true;
            }
        }
    }
    if (!has_xref) {
        /* using original location to find mRNA: 
        * mRNA must include the CDS location and the internal interval boundaries need to be identical
        */
        mrna = sequence::GetBestOverlappingFeat( cds.GetLocation(), CSeqFeatData::eSubtype_mRNA, sequence::eOverlap_CheckIntRev, scope);
    }
    return mrna;
}

/// GetGeneticCodeForBioseq
/// A function to construct the appropriate CGenetic_code object to use
/// when constructing a coding region for a given Bioseq (if the default code
/// should not be used).
/// @param bh         The Bioseq_Handle of the nucleotide sequence on which the
///                   coding region is to be created.
///
/// @return           CRef<CGenetic_code> for new CGenetic_code (will be NULL if default should be used)
CRef<CGenetic_code> GetGeneticCodeForBioseq(CBioseq_Handle bh)
{
    CRef<CGenetic_code> code(NULL);
    if (!bh) {
        return code;
    }
    CSeqdesc_CI src(bh, CSeqdesc::e_Source);
    if (src && src->GetSource().IsSetOrg() && src->GetSource().GetOrg().IsSetOrgname()) {
        if (src->GetSource().IsSetGenome()) {
            switch (src->GetSource().GetGenome()) {
                case CBioSource::eGenome_mitochondrion:
                case CBioSource::eGenome_kinetoplast:
                case CBioSource::eGenome_hydrogenosome:
                    if (src->GetSource().IsSetMgcode()) {
                        code.Reset(new CGenetic_code());
                        code->SetId(src->GetSource().GetMgcode());
                    }
                    break;
                case CBioSource::eGenome_chloroplast:
                case CBioSource::eGenome_chromoplast:
                case CBioSource::eGenome_plasmid:
                case CBioSource::eGenome_cyanelle:
                case CBioSource::eGenome_apicoplast:
                case CBioSource::eGenome_leucoplast:
                case CBioSource::eGenome_proplastid:
                case CBioSource::eGenome_chromatophore:
                    if (src->GetSource().IsSetPgcode()) {
                        code.Reset(new CGenetic_code());
                        code->SetId(src->GetSource().GetPgcode());
                    } else {
                        code.Reset(new CGenetic_code());
                        code->SetId(11);
                    }
                    break;
                default:
                    if (src->GetSource().IsSetGcode()) {
                        code.Reset(new CGenetic_code());
                        code->SetId(src->GetSource().GetGcode());
                    }
                    break;
            }
        } else {
            if (src->GetSource().IsSetGcode()) {
                code.Reset(new CGenetic_code());
                code->SetId(src->GetSource().GetGcode());
            }
        }
    }
    return code;
}


static CRef<CSeq_loc> TruncateSeqLoc (const CSeq_loc& orig_loc, size_t new_len)
{
    CRef<CSeq_loc> new_loc;

    size_t len = 0;
    for (CSeq_loc_CI it(orig_loc); it && len < new_len; ++it) {
        size_t this_len = it.GetRange().GetLength();
        CConstRef<CSeq_loc> this_loc = it.GetRangeAsSeq_loc();
        if (len + this_len <= new_len) {
            if (new_loc) {
                new_loc->Add(*this_loc);
            } else {
                new_loc.Reset(new CSeq_loc());
                new_loc->Assign(*this_loc);
            }
            len += this_len;
        } else {
            CRef<CSeq_loc> partial_loc(new CSeq_loc());
            size_t len_wanted = new_len - len;
            size_t start = this_loc->GetStart(eExtreme_Biological);
            if (len_wanted == 1) {
                // make a point
                partial_loc->SetPnt().SetPoint(start);
            } else {
                // make an interval
                if (this_loc->IsSetStrand() && this_loc->GetStrand() == eNa_strand_minus) {
                    partial_loc->SetInt().SetFrom(start - len_wanted + 1);
                    partial_loc->SetInt().SetTo(start);
                } else {
                    partial_loc->SetInt().SetFrom(start);
                    partial_loc->SetInt().SetTo(start + len_wanted - 1);
                }
            }
            partial_loc->SetId(*this_loc->GetId());
            if (this_loc->IsSetStrand()) {
                partial_loc->SetStrand(this_loc->GetStrand());
            }
            if (new_loc) {
                new_loc->Add(*partial_loc);
            } else {
                new_loc.Reset(new CSeq_loc());
                new_loc->Assign(*partial_loc);
            }
            len += len_wanted;  
        }
    }

    return new_loc;
}


/// TruncateCDSAtStop
/// A function to truncate a CDS location after the first stop codon in the
/// protein translation. Note that adjustments are not made to the protein
/// sequence in this function, only the location and partialness of the coding
/// region.
/// @param cds        The feature to adjust
/// @param scope      The scope in which adjustments are to be made (if necessary)
///
/// @return           true if stop codon was found, false otherwise
bool TruncateCDSAtStop(CSeq_feat& cds, CScope& scope)
{
    bool rval = false;
    CRef<CBioseq> bioseq = CSeqTranslator::TranslateToProtein (cds, scope);
    if (bioseq) {
        CRef<CSeq_loc> new_loc(NULL);
        string prot_str = "";
        CSeqTranslator::Translate(cds, scope, prot_str);
        size_t pos = NStr::Find(prot_str, "*");
        if (pos != string::npos) {
            // want to truncate the location and retranslate
            size_t len_wanted =  3 * (pos + 1);
            if (cds.GetData().GetCdregion().IsSetFrame()) {
                CCdregion::EFrame frame = cds.GetData().GetCdregion().GetFrame();
                if (frame == CCdregion::eFrame_two) {
                    len_wanted += 1;
                } else if (frame == CCdregion::eFrame_three) {
                    len_wanted += 2;
                }
            }
            if (len_wanted > 0) {
                new_loc = TruncateSeqLoc (cds.GetLocation(), len_wanted);
                if (new_loc) {
                    new_loc->SetPartialStop(false, eExtreme_Biological);
                    cds.SetLocation().Assign(*new_loc);
                    if (cds.GetLocation().IsPartialStart(eExtreme_Biological)) {
                        cds.SetPartial(true);
                    } else {
                        cds.ResetPartial();
                    }
                    rval = true;
                }
            }
        }
    }
    return rval;
}


/// ExtendCDSToStopCodon
/// A function to extend a CDS location to the first in-frame stop codon in the
/// protein translation. Note that adjustments are not made to the protein
/// sequence in this function, only the location and partialness of the coding
/// region.
/// @param cds        The feature to adjust
/// @param scope      The scope in which adjustments are to be made (if necessary)
///
/// @return           true if stop codon was found, false otherwise
bool ExtendCDSToStopCodon (CSeq_feat& cds, CScope& scope)
{
    if (!cds.IsSetLocation()) {
        return false;
    }

    const CSeq_loc& loc = cds.GetLocation();

    CBioseq_Handle bsh = scope.GetBioseqHandle(*(loc.GetId()));
    if (!bsh) {
        return false;
    }

    const CGenetic_code* code = NULL;
    if (cds.IsSetData() && cds.GetData().IsCdregion() && cds.GetData().GetCdregion().IsSetCode()) {
        code = &(cds.GetData().GetCdregion().GetCode());
    }

    size_t stop = loc.GetStop(eExtreme_Biological);
    // figure out if we have a partial codon at the end
    size_t orig_len = sequence::GetLength(loc, &scope);
    size_t len = orig_len;
    if (cds.IsSetData() && cds.GetData().IsCdregion() && cds.GetData().GetCdregion().IsSetFrame()) {
        CCdregion::EFrame frame = cds.GetData().GetCdregion().GetFrame();
        if (frame == CCdregion::eFrame_two) {
            len -= 1;
        } else if (frame == CCdregion::eFrame_three) {
            len -= 2;
        }
    }
    size_t mod = len % 3;
    CRef<CSeq_loc> vector_loc(new CSeq_loc());
    vector_loc->SetInt().SetId().Assign(*(loc.GetId()));

    if (loc.IsSetStrand() && loc.GetStrand() == eNa_strand_minus) {
        vector_loc->SetInt().SetFrom(0);
        vector_loc->SetInt().SetTo(stop + mod - 1);
        vector_loc->SetStrand(eNa_strand_minus);
    } else {
        vector_loc->SetInt().SetFrom(stop - mod + 1);
        vector_loc->SetInt().SetTo(bsh.GetInst_Length() - 1);
    }

    CSeqVector seq(*vector_loc, scope, CBioseq_Handle::eCoding_Iupac);
    // reserve our space
    const size_t usable_size = seq.size();

    // get appropriate translation table
    const CTrans_table & tbl =
        (code ? CGen_code_table::GetTransTable(*code) :
                CGen_code_table::GetTransTable(1));

    // main loop through bases
    CSeqVector::const_iterator start = seq.begin();

    size_t i;
    size_t k;
    size_t state = 0;
    size_t length = usable_size / 3;

    CRef<CSeq_loc> new_loc(NULL);

    for (i = 0;  i < length;  ++i) {
        // loop through one codon at a time
        for (k = 0;  k < 3;  ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
        }

        if (tbl.GetCodonResidue (state) == '*') {
            CSeq_loc_CI it(loc);
            CSeq_loc_CI it_next = it;
            ++it_next;
            while (it_next) {
                CConstRef<CSeq_loc> this_loc = it.GetRangeAsSeq_loc();
                if (new_loc) {
                    new_loc->Add(*this_loc);
                } else {
                    new_loc.Reset(new CSeq_loc());
                    new_loc->Assign(*this_loc);
                }
                it = it_next;
                ++it_next;
            }
            CRef<CSeq_loc> last_interval(new CSeq_loc());
            CConstRef<CSeq_loc> this_loc = it.GetRangeAsSeq_loc();
            size_t this_start = this_loc->GetStart(eExtreme_Positional);
            size_t this_stop = this_loc->GetStop(eExtreme_Positional);
            size_t extension = ((i + 1) * 3) - mod;
            last_interval->SetInt().SetId().Assign(*(this_loc->GetId()));
            if (this_loc->IsSetStrand() && this_loc->GetStrand() == eNa_strand_minus) {
                last_interval->SetStrand(eNa_strand_minus);
                last_interval->SetInt().SetFrom(this_start - extension);
                last_interval->SetInt().SetTo(this_stop);
            } else {
                last_interval->SetInt().SetFrom(this_start);
                last_interval->SetInt().SetTo(this_stop + extension);
            }
                
            if (new_loc) {
                new_loc->Add(*last_interval);
            } else {
                new_loc.Reset(new CSeq_loc());
                new_loc->Assign(*last_interval);
            }
            new_loc->SetPartialStop(false, eExtreme_Biological);

            cds.SetLocation().Assign(*new_loc);
            return true;
        }
    }

    if (usable_size < 3 && !new_loc) {
        if (loc.GetStrand() == eNa_strand_minus) {
            new_loc = SeqLocExtend(loc, 0, &scope);
        } else {
            new_loc = SeqLocExtend(loc, bsh.GetInst_Length() - 1, &scope);
        }
        new_loc->SetPartialStop(true, eExtreme_Biological);
        cds.SetLocation().Assign(*new_loc);
        return true;
    }

    return false;
}


void AdjustCDSFrameForStartChange(CCdregion& cds, int change)
{
    TSeqPos old_frame = CCdregion::eFrame_one;
    if (cds.IsSetFrame() && cds.GetFrame() != CCdregion::eFrame_not_set) {
        old_frame = cds.GetFrame();
    }

    TSignedSeqPos new_frame = old_frame - (change % 3);
    if (new_frame < 1)
    {
        new_frame += 3;
    }
    cds.SetFrame((CCdregion::EFrame)new_frame);
}


bool PromoteCDSToNucProtSet(objects::CSeq_feat_Handle& orig_feat)
{
    // only move coding regions to nuc-prot set, and only do this if they
    // have a product that is "local"
    if (!orig_feat.IsSetData() || !orig_feat.GetData().IsCdregion()
        || !orig_feat.IsSetProduct()) {
        return false;
    }
    CBioseq_Handle nuc_bsh = orig_feat.GetScope().GetBioseqHandle(orig_feat.GetLocation());
    if (!nuc_bsh) {
        return false;
    }
    CBioseq_Handle p_bsh = orig_feat.GetScope().GetBioseqHandleFromTSE(*(orig_feat.GetProduct().GetId()), nuc_bsh);
    if (!p_bsh) {
        return false;
    }

    // This is necessary, to make sure that we are in "editing mode"
    const CSeq_annot_Handle& annot_handle = orig_feat.GetAnnot();
    CSeq_entry_EditHandle eh = annot_handle.GetParentEntry().GetEditHandle();

    CSeq_feat_EditHandle feh(orig_feat);
    CSeq_entry_Handle parent_entry = feh.GetAnnot().GetParentEntry();

    bool rval = false;

    if (parent_entry.IsSet()
            && parent_entry.GetSet().IsSetClass()
            && parent_entry.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot) {
        // already on nuc-prot set, leave it alone
    } else {
        CBioseq_set_Handle nuc_parent = parent_entry.GetParentBioseq_set();
        if (nuc_parent && nuc_parent.IsSetClass() && nuc_parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
            CSeq_annot_Handle ftable;
            CSeq_entry_Handle parent_seh = nuc_parent.GetParentEntry();
            CSeq_annot_CI annot_ci(parent_seh, CSeq_annot_CI::eSearch_entry);
            for (; annot_ci; ++annot_ci) {
                if ((*annot_ci).IsFtable()) {
                    ftable = *annot_ci;
                    break;
                }
            }

            if (!ftable) {
                CRef<CSeq_annot> new_annot(new CSeq_annot());
                new_annot->SetData().SetFtable();
                CSeq_entry_EditHandle eh = parent_seh.GetEditHandle();
                ftable = eh.AttachAnnot(*new_annot);
            }

            CSeq_annot_EditHandle old_annot = annot_handle.GetEditHandle();
            CSeq_annot_EditHandle new_annot = ftable.GetEditHandle();
            orig_feat = new_annot.TakeFeat(feh);
            const list< CRef< CSeq_feat > > &feat_list = old_annot.GetSeq_annotCore()->GetData().GetFtable();
            if (feat_list.empty())
            {
                old_annot.Remove();       
            }
            rval = true;
        }
    }    
    return rval;
}


bool DemoteCDSToNucSeq(objects::CSeq_feat_Handle& orig_feat)
{
    CSeq_feat_EditHandle feh(orig_feat);
    CSeq_entry_Handle parent_entry = feh.GetAnnot().GetParentEntry();

    if (!parent_entry.IsSet() || !parent_entry.GetSet().IsSetClass() ||
        parent_entry.GetSet().GetClass() != CBioseq_set::eClass_nuc_prot) {
        // no change, not on nuc-prot set
        return false;
    }

    CBioseq_CI bi(parent_entry, CSeq_inst::eMol_na);
    if (!bi) {
        // no nucleotide sequence to move to
        return false;
    }


    // This is necessary, to make sure that we are in "editing mode"
    const CSeq_annot_Handle& annot_handle = orig_feat.GetAnnot();
    CSeq_entry_EditHandle eh = annot_handle.GetParentEntry().GetEditHandle();

    CSeq_annot_Handle ftable;
    CSeq_entry_Handle nuc_seh = bi->GetSeq_entry_Handle();
    CSeq_annot_CI annot_ci(nuc_seh, CSeq_annot_CI::eSearch_entry);
    for (; annot_ci; ++annot_ci) {
        if ((*annot_ci).IsFtable()) {
            ftable = *annot_ci;
            break;
        }
    }

    if (!ftable) {
        CRef<CSeq_annot> new_annot(new CSeq_annot());
        new_annot->SetData().SetFtable();
        CSeq_entry_EditHandle eh = nuc_seh.GetEditHandle();
        ftable = eh.AttachAnnot(*new_annot);
    }

    CSeq_annot_EditHandle old_annot = annot_handle.GetEditHandle();
    CSeq_annot_EditHandle new_annot = ftable.GetEditHandle();
    orig_feat = new_annot.TakeFeat(feh);   
    const list< CRef< CSeq_feat > > &feat_list = old_annot.GetSeq_annotCore()->GetData().GetFtable();
    if (feat_list.empty())
    {
        old_annot.Remove();       
    }
    return true;
}


bool ApplyCDSFrame::s_SetCDSFrame(CSeq_feat& cds, ECdsFrame frame_type, CScope& scope)
{
    if (!cds.IsSetData() || !cds.GetData().IsCdregion())
        return false;

    CCdregion::TFrame orig_frame = CCdregion::eFrame_not_set;
    if (cds.GetData().GetCdregion().IsSetFrame()) {
        orig_frame = cds.GetData().GetCdregion().GetFrame();
    }
    // retrieve the new frame
    CCdregion::TFrame new_frame = orig_frame;
    switch (frame_type) {
    case eNotSet:
        break;
    case eBest:
        new_frame = CSeqTranslator::FindBestFrame(cds, scope);
        break;
    case eMatch:
        new_frame = s_FindMatchingFrame(cds, scope);
        break;
    case eOne:
        new_frame = CCdregion::eFrame_one;
        break;
    case eTwo:
        new_frame = CCdregion::eFrame_two;
        break;
    case eThree:
        new_frame = CCdregion::eFrame_three;
        break;
    }

    bool modified = false;
    if (orig_frame != new_frame) {
        cds.SetData().SetCdregion().SetFrame(new_frame);
        modified = true;
    }
    return modified;
}

CCdregion::TFrame ApplyCDSFrame::s_FindMatchingFrame(const CSeq_feat& cds, CScope& scope)
{
    CCdregion::TFrame new_frame = CCdregion::eFrame_not_set;
    //return the frame that matches the protein sequence if it can find one
    if (!cds.IsSetData() || !cds.GetData().IsCdregion() || !cds.IsSetLocation() || !cds.IsSetProduct()) {
        return new_frame;
    }

    // get the protein sequence
    CBioseq_Handle product = scope.GetBioseqHandle(cds.GetProduct());
    if (!product || !product.IsProtein()) {
        return new_frame;
    }

    // obtaining the original protein sequence
    CSeqVector prot_vec = product.GetSeqVector(CBioseq_Handle::eCoding_Ncbi);
    prot_vec.SetCoding(CSeq_data::e_Ncbieaa);
    string orig_prot_seq;
    prot_vec.GetSeqData(0, prot_vec.size(), orig_prot_seq);
    if (NStr::IsBlank(orig_prot_seq)) {
        return new_frame;
    }

    CRef<CSeq_feat> tmp_cds(new CSeq_feat);
    tmp_cds->Assign(cds);
    for (int enumI = CCdregion::eFrame_one; enumI < CCdregion::eFrame_three + 1; ++enumI) {
        CCdregion::EFrame fr = (CCdregion::EFrame) (enumI);
        tmp_cds->SetData().SetCdregion().SetFrame(fr);
    
        string new_prot_seq;
        CSeqTranslator::Translate(*tmp_cds, scope, new_prot_seq);
        if (NStr::EndsWith(new_prot_seq, '*'))
            new_prot_seq.erase(new_prot_seq.end() - 1);
        if (NStr::EqualNocase(new_prot_seq, orig_prot_seq)) {
            new_frame = fr;
            break;
        }
    }

    return new_frame;
}

ApplyCDSFrame::ECdsFrame ApplyCDSFrame::s_GetFrameFromName(const string& name)
{
    ECdsFrame frame = eNotSet;
    if (NStr::EqualNocase(name, "best")) {
        frame = eBest;
    } else if (NStr::EqualNocase(name, "match")) {
        frame = eMatch;
    } else if (NStr::Equal(name, "1") || NStr::EqualNocase(name, "one")) {
        frame = eOne;
    } else if (NStr::Equal(name, "2") || NStr::EqualNocase(name, "two")) {
        frame = eTwo;
    } else if (NStr::Equal(name, "3") || NStr::EqualNocase(name, "three")) {
        frame = eThree;
    }
    return frame;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

