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
*   CFieldHandler parent class
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/edit/cds_fix.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objmgr/bioseq_handle.hpp>
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
    } catch (CException& e) {
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


void ReverseComplementCDRegion(CCdregion& cdr, CScope& scope)
{
    if (cdr.IsSetCode_break()) {
        NON_CONST_ITERATE(CCdregion::TCode_break, it, cdr.SetCode_break()) {
            if ((*it)->IsSetLoc()) {
                CSeq_loc* new_loc = sequence::SeqLocRevCmpl((*it)->GetLoc(), &scope);
                if (new_loc) {
                    (*it)->SetLoc().Assign(*new_loc);
                } 
            }
        }
    }
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

