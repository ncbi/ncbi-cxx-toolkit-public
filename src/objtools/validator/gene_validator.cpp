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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   validation of gene features
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/single_feat_validator.hpp>
#include <objtools/validator/validerror_feat.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/format/items/gene_finder.hpp>

#include <serial/serialbase.hpp>

#include <objmgr/seqdesc_ci.hpp>
#include <objects/seq/seqport_util.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <util/sgml_entity.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


static bool s_IsLocDirSub(const CSeq_loc& loc,
    const CTSE_Handle & tse,
    CCacheImpl & cache,
    CScope *scope);

static const char* const sc_BadGeneSynText [] = {
  "HLA",
  "alpha",
  "alternative",
  "beta",
  "cellular",
  "cytokine",
  "delta",
  "drosophila",
  "epsilon",
  "gamma",
  "homolog",
  "mouse",
  "orf",
  "partial",
  "plasma",
  "precursor",
  "pseudogene",
  "putative",
  "rearranged",
  "small",
  "trna",
  "unknown",
  "unknown function",
  "unknown protein",
  "unnamed"
};
typedef CStaticArraySet<const char*, PCase_CStr> TBadGeneSynSet;
DEFINE_STATIC_ARRAY_MAP(TBadGeneSynSet, sc_BadGeneSyn, sc_BadGeneSynText);

void CGeneValidator::Validate()
{
    CSingleFeatValidator::Validate();

    const CGene_ref& gene = m_Feat.GetData().GetGene();

    m_Imp.IncrementGeneCount();

    if ( (! gene.IsSetLocus()      ||  gene.GetLocus().empty())   &&
         (! gene.IsSetAllele()     ||  gene.GetAllele().empty())  &&
         (! gene.IsSetDesc()       ||  gene.GetDesc().empty())    &&
         (! gene.IsSetMaploc()     ||  gene.GetMaploc().empty())  &&
         (! gene.IsSetDb()         ||  gene.GetDb().empty())      &&
         (! gene.IsSetSyn()        ||  gene.GetSyn().empty())     &&
         (! gene.IsSetLocus_tag()  ||  gene.GetLocus_tag().empty()) ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_GeneRefHasNoData,
                "There is a gene feature where all fields are empty");
    }
    if ( gene.IsSetLocus() && m_Feat.IsSetComment() && NStr::EqualCase (m_Feat.GetComment(), gene.GetLocus())) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_RedundantFields, 
                 "Comment has same value as gene locus");
    }

    if ( gene.IsSetLocus_tag()  &&  !NStr::IsBlank (gene.GetLocus_tag()) ) {
            const string& locus_tag = gene.GetLocus_tag();

        for (auto it : locus_tag) {
            if ( isspace((unsigned char)(it)) != 0 ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_LocusTagHasSpace,
                        "Gene locus_tag '" + gene.GetLocus_tag() + 
                        "' should be a single word without any spaces");
                break;
            }
        }

        if (gene.IsSetLocus() && !NStr::IsBlank(gene.GetLocus())
            && NStr::EqualNocase(locus_tag, gene.GetLocus())) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_LocusTagGeneLocusMatch,
                    "Gene locus and locus_tag '" + gene.GetLocus() + "' match");
        }
        if (m_Feat.IsSetComment() && NStr::EqualCase (m_Feat.GetComment(), locus_tag)) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_RedundantFields, 
                         "Comment has same value as gene locus_tag");
        }
        if (m_Feat.IsSetQual()) {
            for (auto it : m_Feat.GetQual()) {
                if (it->IsSetQual() && NStr::EqualNocase(it->GetQual(), "old_locus_tag") && it->IsSetVal()) {
                    if (NStr::EqualCase(it->GetVal(), locus_tag)) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_RedundantFields,
                            "old_locus_tag has same value as gene locus_tag");
                        PostErr(eDiag_Error, eErr_SEQ_FEAT_LocusTagProblem,
                            "Gene locus_tag and old_locus_tag '" + locus_tag + "' match");
                    }
                    if (NStr::Find(it->GetVal(), ",") != string::npos) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_OldLocusTagBadFormat,
                            "old_locus_tag has comma, multiple old_locus_tags should be split into separate qualifiers");
                    }
                }
            }
        }                        
    } else if (m_Imp.DoesAnyGeneHaveLocusTag() && 
               !s_IsLocDirSub(m_Feat.GetLocation(), m_Imp.GetTSE_Handle(), m_Imp.GetCache(), &m_Scope)) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_MissingGeneLocusTag, 
                 "Missing gene locus tag");
    }

    if ( gene.IsSetDb () ) {
        m_Imp.ValidateDbxref(gene.GetDb(), m_Feat);
    }

    if (m_Imp.IsRefSeq() && gene.IsSetSyn()) {
        for (auto it : gene.GetSyn()) {
            if (sc_BadGeneSyn.find (it.c_str()) != sc_BadGeneSyn.end()) {
                PostErr (m_Imp.IsGpipe() ? eDiag_Info : eDiag_Warning, eErr_SEQ_FEAT_UndesiredGeneSynonym, 
                         "Uninformative gene synonym '" + it + "'");
            }
            if (gene.IsSetLocus() && !NStr::IsBlank(gene.GetLocus())
                && NStr::Equal(gene.GetLocus(), it)) {
                PostErr (m_Imp.IsGpipe() ? eDiag_Info : eDiag_Warning, eErr_SEQ_FEAT_UndesiredGeneSynonym,
                         "gene synonym has same value as gene locus");
            }
        }
    }

    if (gene.IsSetLocus() && gene.IsSetDesc() 
        && NStr::EqualCase (gene.GetLocus(), gene.GetDesc())
        && !m_Imp.IsGpipe()) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UndesiredGeneSynonym,
                 "gene description has same value as gene locus");
    }

    if (!gene.IsSetLocus() && !gene.IsSetDesc() && gene.IsSetSyn() && gene.GetSyn().size() > 0) {
        PostErr (m_Imp.IsGpipe() ? eDiag_Info : eDiag_Warning, eErr_SEQ_FEAT_UndesiredGeneSynonym,
                 "gene synonym without gene locus or description");
    }

    if (gene.IsSetLocus()) {
        ValidateCharactersInField (gene.GetLocus(), "Gene locus");
    }

    // check for SGML
    if (gene.IsSetLocus() && ContainsSgml(gene.GetLocus())) {
        PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                "gene locus " + gene.GetLocus() + " has SGML");
    }
    if (gene.IsSetLocus_tag() && ContainsSgml(gene.GetLocus_tag())) {
        PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                "gene locus_tag " + gene.GetLocus_tag() + " has SGML");
    }
    if (gene.IsSetDesc()) {
        string desc = gene.GetDesc();
        if (ContainsSgml(desc)) {
                PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                        "gene description " + gene.GetDesc() + " has SGML");
        }
    }
    if (gene.IsSetSyn()) {
        for (auto it : gene.GetSyn()) {
            if (ContainsSgml(it)) {
                PostErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
                    "gene synonym " + it + " has SGML");
            }
        }
    }
    x_ValidateOperon();

    x_ValidateMultiIntervalGene();
}


void CGeneValidator::x_ValidateExceptText(const string& text)
{
    CSingleFeatValidator::x_ValidateExceptText(text);

    if (NStr::Find(text, "gene split at ") != string::npos &&
        (!m_Feat.GetData().GetGene().IsSetLocus_tag() || NStr::IsBlank(m_Feat.GetData().GetGene().GetLocus_tag()))) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptionRequiresLocusTag, "Gene has split exception but no locus_tag");
    }
}


void CGeneValidator::x_ValidateOperon()
{
    CConstRef<CSeq_feat> operon = 
        GetOverlappingOperon(m_Feat.GetLocation(), m_Scope);
    if ( !operon  ||  !operon->IsSetQual() ) {
        return;
    }

    string label;
    feature::GetLabel(m_Feat, &label, feature::fFGL_Content, &m_Scope);
    if ( label.empty() ) {
        return;
    }
    
    for (auto qual_iter : operon->GetQual()) {
        const CGb_qual& qual = *qual_iter;
        if( qual.CanGetQual()  &&  qual.CanGetVal() ) {
            if ( NStr::Compare(qual.GetQual(), "operon") == 0  &&
                 NStr::CompareNocase(qual.GetVal(), label) == 0) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidOperonMatchesGene,
                    "Operon is same as gene - " + qual.GetVal());
            }
        }
    }
}


bool s_HasMobileElementForInterval(TSeqPos from, TSeqPos to, CBioseq_Handle bsh)
{
    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->SetInt().SetId().Assign(*(bsh.GetSeqId()));
    if (from < to) {
        loc->SetInt().SetFrom(from);
        loc->SetInt().SetTo(to);
    } else {
        loc->SetInt().SetFrom(to);
        loc->SetInt().SetTo(from);
    }
    CRef<CSeq_loc> rev_loc(new CSeq_loc());
    rev_loc->Assign(*loc);
    rev_loc->SetInt().SetStrand(eNa_strand_minus);

    TFeatScores mobile_elements;
    GetOverlappingFeatures(*loc, CSeqFeatData::e_Imp,
        CSeqFeatData::eSubtype_mobile_element, eOverlap_Contained, mobile_elements, bsh.GetScope());
    ITERATE(TFeatScores, m, mobile_elements) {
        if (m->second->GetLocation().Compare(*loc, CSeq_loc::fCompare_Default) == 0
            || m->second->GetLocation().Compare(*rev_loc, CSeq_loc::fCompare_Default) == 0) {
            return true;
        }
    }
    mobile_elements.clear();
    GetOverlappingFeatures(*rev_loc, CSeqFeatData::e_Imp,
        CSeqFeatData::eSubtype_mobile_element, eOverlap_Contained, mobile_elements, bsh.GetScope());
    ITERATE(TFeatScores, m, mobile_elements) {
        if (m->second->GetLocation().Compare(*loc, CSeq_loc::fCompare_Default) == 0
            || m->second->GetLocation().Compare(*rev_loc, CSeq_loc::fCompare_Default) == 0) {
            return true;
        }
    }

    return false;
}


bool CGeneValidator::x_AllIntervalGapsAreMobileElements()
{
    const CSeq_loc& loc = m_Feat.GetLocation();
    CSeq_loc_CI si(loc);
    if (!si) {
        return false;
    }
    ENa_strand loc_strand = loc.GetStrand();
    while (si) {
        TSeqPos gap_start;
        if (loc_strand == eNa_strand_minus) {
            gap_start = si.GetRange().GetFrom() + 1;
        } else {
            gap_start = si.GetRange().GetTo() + 1;
        }
        ++si;
        if (si) {
            TSeqPos gap_end;
            if (loc_strand == eNa_strand_minus) {
                gap_end = si.GetRange().GetTo();
            } else {
                gap_end = si.GetRange().GetFrom();
            }
            if (gap_end > 0) {
                gap_end--;
            }
            if (!s_HasMobileElementForInterval(gap_start, gap_end, m_LocationBioseq)) {
                return false;
            }
        }
    }
    return true;
}


static bool s_LocIntervalsSpanOrigin (const CSeq_loc& loc, CBioseq_Handle bsh)
{
    CSeq_loc_CI si(loc);
    if (!si) {
        return false;
    }
    if(loc.GetStrand() == eNa_strand_minus) {
        if (si.GetRange().GetFrom() != 0) {
            return false;
        }
        ++si;
        if (!si || si.GetRange().GetTo() != bsh.GetBioseqLength() - 1) {
            return false;
        }
        ++si;
    } else {
        if (si.GetRange().GetTo() != bsh.GetBioseqLength() - 1) {
            return false;
        }
        ++si;
        if (!si || si.GetRange().GetFrom() != 0) {
            return false;
        }        
        ++si;
    }
    if (si) {
        return false;
    } else {
        return true;
    }
}


void CGeneValidator::x_ValidateMultiIntervalGene()
{
    try {
        const CSeq_loc& loc = m_Feat.GetLocation();
        CSeq_loc_CI si(loc);
        if ( !(++si) ) {  // if only a single interval
            return;
        }

        if (m_Feat.IsSetExcept() && m_Feat.IsSetExcept_text()
            && NStr::FindNoCase (m_Feat.GetExcept_text(), "trans-splicing") != string::npos) {
            //ignore - has exception
            return;
        }

        if (x_AllIntervalGapsAreMobileElements()) {
            // ignore, "space between" is a mobile element
            return;
        }

        if ( !IsOneBioseq(loc, &m_Scope) ) {
            return;
        } else if ( m_LocationBioseq.GetInst().GetTopology() == CSeq_inst::eTopology_circular
                && s_LocIntervalsSpanOrigin (loc, m_LocationBioseq)) {            
            // spans origin
            return;
        } else if (m_Imp.IsSmallGenomeSet()) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MultiIntervalGene,
                "Multiple interval gene feature in small genome set - "
                "set trans-splicing exception if appropriate");
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MultiIntervalGene,
                "Gene feature on non-segmented sequence should not "
                "have multiple intervals");
        }
    } catch ( const exception& e ) {
        if (NStr::Find(e.what(), "Error: Cannot resolve") == string::npos) {
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,
                string("Exception while validating multi-interval genes. EXCEPTION: ") +
                e.what());
        }
    }
}


// to be DirSub, must not have ID of type other, must not have WGS keyword or tech, and
// must not be both complete and bacterial
static bool s_IsLocDirSub (const CSeq_loc& loc,
                           const CTSE_Handle & tse,
                           CCacheImpl & cache,
                           CScope *scope)
{
    if (!loc.GetId()) {
        for ( CSeq_loc_CI si(loc); si; ++si ) {
            if (!s_IsLocDirSub(si.GetEmbeddingSeq_loc(), tse, cache, scope)) {
                return false;
            }
        }
        return true;
    }


    if (loc.GetId()->IsOther()) {
        return false;
    }

    CBioseq_Handle bsh = cache.GetBioseqHandleFromLocation (scope, loc, tse);
    if (!bsh) {
        return true;
    }

    bool rval = true;
    FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh.GetCompleteBioseq())) {
        if ((*it)->IsOther()) {
            rval = false;
        }
    }

    if (rval) {
        // look for WGS keyword
        for (CSeqdesc_CI diter (bsh, CSeqdesc::e_Genbank); diter && rval; ++diter) {
            FOR_EACH_KEYWORD_ON_GENBANKBLOCK (it, diter->GetGenbank()) {
                if (NStr::EqualNocase (*it, "WGS")) {
                    rval = false;
                }
            }
        }

        // look for WGS tech and completeness
        bool completeness = false;

        for (CSeqdesc_CI diter (bsh, CSeqdesc::e_Molinfo); diter && rval; ++diter) {
            if (diter->GetMolinfo().IsSetTech() && diter->GetMolinfo().GetTech() == CMolInfo::eTech_wgs) {
                rval = false;
            }
            if (diter->GetMolinfo().IsSetCompleteness() && diter->GetMolinfo().GetCompleteness() == CMolInfo::eCompleteness_complete) {
                completeness = true;
            }
        }

        // look for bacterial
        if (completeness) {
            for (CSeqdesc_CI diter (bsh, CSeqdesc::e_Source); diter && rval; ++diter) {
                if (diter->GetSource().IsSetDivision()
                    && NStr::EqualNocase (diter->GetSource().GetDivision(), "BCT")) {
                    rval = false;
                }
            }
        }
    }
    return rval;
}



END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
