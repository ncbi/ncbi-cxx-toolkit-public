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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
* Maintainer: Frank Ludwig
*						
* File Description:
*   new (early 2003) flat-file generator -- representation of features
*   (mainly of interest to implementors)
*
*
* WHEN EDITING THE LIST OF QUALIFIERS:
*
* - there is currently a lot of parallel logic for the FTable case 
*   (CFeatureItem::x_AddFTableQuals()) and the standard case 
*   (CFeatureItem::x_Add...Quals()). Make sure to edit both cases as 
*   appropriate.
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Heterogen.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/general/Object_id.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <util/static_set.hpp>
#include <util/static_map.hpp>
#include <util/sequtil/sequtil.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <algorithm>
#include <objtools/format/formatter.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/context.hpp>
#include <objtools/format/items/qualifiers.hpp>
#include "utils.hpp"


// On Mac OS X 10.3, FixMath.h defines ff as a one-argument macro(!)
#ifdef ff
#  undef ff
#endif

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


// -- static functions

static bool s_ValidId(const CSeq_id& id)
{
    return id.IsGenbank()  ||  id.IsEmbl()    ||  id.IsDdbj()  ||
           id.IsOther()    ||  id.IsPatent()  ||  
           id.IsTpg()      ||  id.IsTpe()     ||  id.IsTpd()   ||
           id.IsGpipe();
}


static bool s_QualifiersMeetPrimerReqs( 
    const string& fwd_name, 
    const string& fwd_seq, 
    const string& rev_name, 
    const string& rev_seq )
{
    if ( fwd_seq.empty() ) {
        return false;
    }
    if ( rev_seq.empty() ) {
        return false;
    }
    // watch out, there are probably more, related to compound values in each of the
    // constituents. Test cases?
    //
    return true;
}


static string s_MakePcrPrimerNote(
    const string& fwd_name,
    const string& fwd_seq,
    const string& rev_name,
    const string& rev_seq )
{
    if ( fwd_name.empty() && fwd_seq.empty() && rev_name.empty() && rev_seq.empty() ) {
        return "";
    }

    string note( "PCR_primers=" );
    size_t init_length = note.length();
    if ( ! fwd_name.empty() ) {
        note += string( "fwd_name: " ) + fwd_name;
    }
    if ( ! fwd_seq.empty() ) {
        if ( note.length() > init_length ) {
            note += ", ";
        }
        note += string( "fwd_seq: " ) + fwd_seq;
    }
    if ( ! rev_name.empty() ) {
        if ( note.length() > init_length ) {
            note += ", ";
        }
        note += string( "rev_name: " ) + rev_name;
    }
    if ( ! rev_seq.empty() ) {
        if ( note.length() > init_length ) {
            note += ", ";
        }
        note += string( "rev_seq: " ) + rev_seq;
    }
    return note;
}


static bool s_CheckQuals_cdregion(const CSeq_feat& feat, const CSeq_loc& loc, CBioseqContext& ctx)
{
    if ( !ctx.Config().CheckCDSProductId() ) {
        return true;
    }
    
    CScope& scope = ctx.GetScope();

    // non-pseudo CDS must have /product
    bool pseudo = feat.CanGetPseudo()  &&  feat.GetPseudo();
    if ( !pseudo ) {
        const CGene_ref* grp = feat.GetGeneXref();
        if ( grp == NULL ) {
            CConstRef<CSeq_feat> gene = GetOverlappingGene(loc, scope);
            if (gene) {
                pseudo = gene->CanGetPseudo()  &&  gene->GetPseudo();
                if ( !pseudo ) {
                    grp = &(gene->GetData().GetGene());
                }
            }
        }
        if ( !pseudo  &&  grp != NULL ) {
            pseudo = grp->GetPseudo();
        }
    }

    bool just_stop = false;
    if ( feat.CanGetLocation() ) {
        const CSeq_loc& Loc = feat.GetLocation();
        if ( Loc.IsPartialStart(eExtreme_Biological)  &&  !Loc.IsPartialStop(eExtreme_Biological) ) {
            if ( GetLength(Loc, &scope) <= 5 ) {
                just_stop = true;
            }
        }
    }

    if ( pseudo ||  just_stop ) {
        return true;
    } 

    // make sure the product has a valid accession
    if (feat.CanGetProduct()) {
        CConstRef<CSeq_id> id;
        try {
            id.Reset(&(GetId(feat.GetProduct(), &scope)));
        } catch ( CException& ) {
            id.Reset(NULL);
        }
        if (id) {
            if ((id->IsGi()  &&  id->GetGi() > 0) ||  id->IsLocal()) {
                CBioseq_Handle prod = scope.GetBioseqHandleFromTSE(*id, ctx.GetHandle());
                if (prod) {
                    ITERATE (CBioseq_Handle::TId, it, prod.GetId()) {
                        if (s_ValidId(*it->GetSeqId())) {
                            CConstRef<CTextseq_id> tsip(it->GetSeqId()->GetTextseq_Id());
                            if (tsip  &&  tsip->IsSetAccession()  &&
                                IsValidAccession(tsip->GetAccession())) {
                                return true;
                            }
                        }
                    }
                } else if (id->IsGi()  &&  id->GetGi() > 0) {
                    // RELEASE_MODE requires that /protein_id is an accession
                    if (ctx.Config().IsModeRelease()) {
                        try {
                            if (IsValidAccession(GetAccessionForGi(id->GetGi(), scope))) {
                                return true;
                            }
                        } catch (CException&) {
                        }
                    }
                }
            } else if (s_ValidId(*id)) {
                CConstRef<CTextseq_id> tsip(id->GetTextseq_Id());
                if (tsip  &&  tsip->IsSetAccession()  &&
                    IsValidAccession(tsip->GetAccession())) {
                    return true;
                }
            }
        }
    } else {  // no product
        if (feat.IsSetExcept()  &&  feat.GetExcept()  &&
            feat.IsSetExcept_text() ) {
            if (NStr::Find(feat.GetExcept_text(),
                    "rearrangement required for product") != NPOS) {
                return true;
            }
        }
    }

    return false;
}



static bool s_HasPub(const CSeq_feat& feat, CBioseqContext& ctx)
{
    ITERATE(CBioseqContext::TReferences, it, ctx.GetReferences()) {
        if ((*it)->Matches(feat.GetCit())) {
            return true;
        }
    }

    return false;
}


static bool s_HasCompareOrCitation(const CSeq_feat& feat, CBioseqContext& ctx)
{
    // check for /compare
    if (!NStr::IsBlank(feat.GetNamedQual("compare"))) {
        return true;
    }

    // check for /citation
    if (feat.CanGetCit()) {
        return s_HasPub(feat, ctx);
    }

    return false;
}


// conflict requires /citation or /compare
static bool s_CheckQuals_conflict(const CSeq_feat& feat, CBioseqContext& ctx)
{
    // RefSeq allows conflict with accession in comment instead of sfp->cit
    if (ctx.IsRefSeq()  &&
        feat.CanGetComment()  &&  !NStr::IsBlank(feat.GetComment())) {
        return true;
    }

    return s_HasCompareOrCitation(feat, ctx);
}

// old_sequence requires /citation or /compare
static bool s_CheckQuals_old_seq(const CSeq_feat& feat, CBioseqContext& ctx)
{    
    return s_HasCompareOrCitation(feat, ctx);
}


static bool s_CheckQuals_gene(const CSeq_feat& feat)
{
    // gene requires /gene or /locus_tag, but desc or syn can be mapped to /gene
    const CSeqFeatData::TGene& gene = feat.GetData().GetGene();
    if ( (gene.CanGetLocus()      &&  !gene.GetLocus().empty())      ||
         (gene.CanGetLocus_tag()  &&  !gene.GetLocus_tag().empty())  ||
         (gene.CanGetDesc()       &&  !gene.GetDesc().empty())       ||
         (!gene.GetSyn().empty()  &&  !gene.GetSyn().front().empty()) ) {
        return true;
    }

    return false;
}


static bool s_CheckQuals_bind(const CSeq_feat& feat)
{
    // protein_bind or misc_binding require eFQ_bound_moiety
    return !NStr::IsBlank(feat.GetNamedQual("bound_moiety"));
}


static bool s_CheckQuals_mod_base(const CSeq_feat& feat)
{
    // modified_base requires eFQ_mod_base
    return !NStr::IsBlank(feat.GetNamedQual("mod_base"));
}


static bool s_CheckQuals_gap(const CSeq_feat& feat)
{
    // gap feature must have /estimated_length qual
    return !feat.GetNamedQual("estimated_length").empty();
}


static bool s_CheckMandatoryQuals(const CSeq_feat& feat, const CSeq_loc& loc, CBioseqContext& ctx)
{
    switch ( feat.GetData().GetSubtype() ) {
    case CSeqFeatData::eSubtype_cdregion:
        {
            return s_CheckQuals_cdregion(feat, loc, ctx);
        }
    case CSeqFeatData::eSubtype_conflict:
        {
            return s_CheckQuals_conflict(feat, ctx);
        }
    case CSeqFeatData::eSubtype_old_sequence:
        {
            return s_CheckQuals_old_seq(feat, ctx);
        }
    case CSeqFeatData::eSubtype_gene:
        {
            return s_CheckQuals_gene(feat);
        }
    case CSeqFeatData::eSubtype_protein_bind:
    case CSeqFeatData::eSubtype_misc_binding:
        {
            return s_CheckQuals_bind(feat);
        }
    case CSeqFeatData::eSubtype_modified_base:
        {
            return s_CheckQuals_mod_base(feat);
        }
    case CSeqFeatData::eSubtype_gap:
        {
            return s_CheckQuals_gap(feat);
        }
    default:
        break;
    }

    return true;
}

/* "not used"

static bool s_SuppressCommentFeature
(const CSeq_feat& feat,
 const CSeq_loc& loc,
 CBioseqContext& ctx)
{
    _ASSERT(feat.GetData().IsComment());

    ECompare comp = Compare(ctx.GetLocation(), loc, &ctx.GetScope());
    if (comp == eContained  ||  comp == eSame) {
        return true;
    }

    const CSeq_loc& orig_loc = feat.GetLocation();
    if (orig_loc.IsWhole()) {
        return true;
    }

    CBioseq_Handle feat_seq = ctx.GetScope().GetBioseqHandle(orig_loc);
    if (orig_loc.GetStart(eExtreme_Positional) == 0  &&
        orig_loc.GetStop(eExtreme_Positional) == feat_seq.GetInst_Length() - 1) {
        return true;
    }
    return false;
}
*/

static bool s_SkipFeature(const CSeq_feat& feat, const CSeq_loc& loc, CBioseqContext& ctx)
{
    CSeqFeatData::E_Choice type    = feat.GetData().Which();
    CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();        
   
    if ( subtype == CSeqFeatData::eSubtype_pub              ||
         subtype == CSeqFeatData::eSubtype_non_std_residue  ||
         subtype == CSeqFeatData::eSubtype_biosrc           ||
         subtype == CSeqFeatData::eSubtype_rsite            ||
         subtype == CSeqFeatData::eSubtype_seq ) {
        return true;
    }
    
    const CFlatFileConfig& cfg = ctx.Config();

    // check feature customization flags
    if ( cfg.ValidateFeatures()  &&
        (subtype == CSeqFeatData::eSubtype_bad  ||
         subtype == CSeqFeatData::eSubtype_virion) ) {
        return true;
    }
    
    if ( ctx.IsNuc()  &&  subtype == CSeqFeatData::eSubtype_het ) {
        return true;
    }
    
    if ( cfg.HideImpFeatures()  &&  type == CSeqFeatData::e_Imp ) {
        return true;
    }
    
    if ( cfg.HideSNPFeatures()  &&  subtype == CSeqFeatData::eSubtype_variation ) {
        return true;
    }

    if ( cfg.HideExonFeatures()  &&  subtype == CSeqFeatData::eSubtype_exon ) {
        return true;
    }

    if ( cfg.HideIntronFeatures()  &&  subtype == CSeqFeatData::eSubtype_intron ) {
        return true;
    }

    if ( cfg.HideRemoteImpFeatures()  &&  type == CSeqFeatData::e_Imp ) {
        if ( subtype == CSeqFeatData::eSubtype_variation  ||
             subtype == CSeqFeatData::eSubtype_exon       ||
             subtype == CSeqFeatData::eSubtype_intron     ||
             subtype == CSeqFeatData::eSubtype_misc_feature ) {
            return true;
        }
    }

    // skip genes in DDBJ format
    if ( cfg.IsFormatDDBJ()  &&  type == CSeqFeatData::e_Gene ) {
        return true;
    }

    // if RELEASE mode, make sure we have all info to create mandatory quals.
    if ( cfg.NeedRequiredQuals() ) {
        return !s_CheckMandatoryQuals(feat, loc, ctx);
    }

    return false;
}


static bool s_IsLegalECNumber(const string& ec_number)
{
    ITERATE (string, it, ec_number) {
        if (!isdigit((unsigned char)(*it))  &&  *it != '.'  &&  *it != '-') {
            return false;
        }
    }

    return true;
}


static const string& s_GetBondName(CSeqFeatData::TBond bond)
{
    static const string kOther = "unclassified";
    return (bond == CSeqFeatData::eBond_other) ? kOther :
        CSeqFeatData::ENUM_METHOD_NAME(EBond)()->FindName(bond, true);
}
                                    
static void s_QualVectorToNote( 
    const CFlatFeature::TQuals& qualVector,
    bool noRedundancy,
    string& note, 
    string& punctuation,
    bool& addPeriod) {

    string qual, prefix;
    ITERATE (CFlatFeature::TQuals, it, qualVector) {
        qual = (*it)->GetValue();
        
        prefix.erase();
        if (!note.empty()) {
            prefix = punctuation;
            if (!NStr::EndsWith(prefix, '\n')) {
                prefix += (*it)->GetPrefix();
            }
        }
        JoinString(note, prefix, qual, noRedundancy);
        addPeriod = (*it)->GetAddPeriod();
        punctuation = (*it)->GetSuffix();
    }
}

static void s_NoteFinalize(
   bool addPeriod,
   string& noteStr,
   CFlatFeature& flatFeature,
   ETildeStyle style = eTilde_newline ) {
   
    if (!noteStr.empty()) {
        if (addPeriod  &&  !NStr::EndsWith(noteStr, ".")) {

            AddPeriod(noteStr);
        }
        // Policy change: expand tilde on both descriptors and features
        ExpandTildes(noteStr, style);

        CRef<CFormatQual> note(new CFormatQual("note", noteStr));
        flatFeature.SetQuals().push_back(note);
    }
}

static int s_GetOverlap( CConstRef<CSeq_feat> feat ) 
{
    if (feat->CanGetLocation()) {

        const CSeq_loc& location = feat->GetLocation();
        switch( location.Which() ) {

            case CSeq_loc_Base::e_Int: {
                const CSeq_interval& interval = location.GetInt();
                if ( interval.CanGetFrom() && interval.CanGetTo() ) {
                    return int(interval.GetTo() - interval.GetFrom() );
                }
                break;
            }
            case CSeq_loc_Base::e_Whole: {
//                const CSeq_id& whole = location.GetWhole(); 
                return numeric_limits<int>::max();
            }
            default:
                break;
        }
    }
    return 0;
}

static const CSeq_feat* s_GetBestProtFeature(const CBioseq_Handle& seq)
{
    //
    //  The best protein feature is defined as the one that has the most overlap
    //  with the given DNA.
    //  If there is a tie between two protein features in overlap then the one with
    //  the higher processing status is declared the winner.
    //
    SAnnotSelector sel(CSeqFeatData::e_Prot);
    sel.SetLimitTSE(seq.GetTSE_Handle());

    CConstRef<CSeq_feat> best;
    CProt_ref::TProcessed best_processed = CProt_ref::eProcessed_transit_peptide;
    int best_overlap = 0;

    for (CFeat_CI it(seq, sel); it; ++it) {

        if (best.Empty()) {

            best.Reset(&it->GetOriginalFeature());
            best_processed = it->GetData().GetProt().GetProcessed();
            best_overlap = s_GetOverlap(best);

        } else {
            
            CConstRef<CSeq_feat> current;
            current.Reset( &it->GetOriginalFeature() );
            int current_overlap = s_GetOverlap(current);
            CProt_ref::TProcessed current_processed = it->GetData().GetProt().GetProcessed();

            if ( best_overlap < current_overlap ) {

                best_overlap = current_overlap;
                best_processed = current_processed;
                best.Reset(&it->GetOriginalFeature());

            } else if ( (best_overlap == current_overlap) && (best_processed < current_processed) ) {
            
                best_processed = current_processed;
                best.Reset(&it->GetOriginalFeature());
            }
        }
    }
    return best.GetPointerOrNull();
}

// -- FeatureHeader

CFeatHeaderItem::CFeatHeaderItem(CBioseqContext& ctx) : CFlatItem(&ctx)
{
    x_GatherInfo(ctx);
}


void CFeatHeaderItem::x_GatherInfo(CBioseqContext& ctx)
{
    if ( ctx.Config().IsFormatFTable() ) {
        m_Id.Reset(ctx.GetPrimaryId());
    }
}

static bool s_CheckFuzz(const CInt_fuzz& fuzz)
{
    return !(fuzz.IsLim()  &&  fuzz.GetLim() == CInt_fuzz::eLim_unk);
}

static bool s_LocIsFuzz(const CSeq_feat& feat, const CSeq_loc& loc)
{
    if ( feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_imp  &&
         feat.GetData().IsImp() ) {  // unmappable impfeats
        const CSeqFeatData::TImp& imp = feat.GetData().GetImp();
        if ( imp.CanGetLoc() ) {
            const string& imploc = imp.GetLoc();
            if ( imploc.find('<') != NPOS  ||  imploc.find('>') != NPOS ) {
                return true;
            }
        }
    } else {    // any regular feature test location for fuzz
        for ( CSeq_loc_CI it(loc, CSeq_loc_CI::eEmpty_Allow); it; ++it ) {
            const CSeq_loc& l = it.GetSeq_loc();
            switch ( l.Which() ) {
            case CSeq_loc::e_Pnt:
            {{
                if ( l.GetPnt().CanGetFuzz() ) {
                    if ( s_CheckFuzz(l.GetPnt().GetFuzz()) ) {
                        return true;
                    }
                }
                break;
            }}
            case CSeq_loc::e_Packed_pnt:
            {{
                if ( l.GetPacked_pnt().CanGetFuzz() ) {
                    if ( s_CheckFuzz(l.GetPacked_pnt().GetFuzz()) ) {
                        return true;
                    }
                }
                break;
            }}
            case CSeq_loc::e_Int:
            {{
                bool fuzz = false;
                if ( l.GetInt().CanGetFuzz_from() ) {
                    fuzz = s_CheckFuzz(l.GetInt().GetFuzz_from());
                }
                if ( !fuzz  &&  l.GetInt().CanGetFuzz_to() ) {
                    fuzz = s_CheckFuzz(l.GetInt().GetFuzz_to());
                }
                if ( fuzz ) {
                    return true;
                }
                break;
            }}
            case CSeq_loc::e_Packed_int:
            {{  
                if ( l.GetPacked_int().IsPartialStart(eExtreme_Biological) 
                  || l.GetPacked_int().IsPartialStop(eExtreme_Biological) ) {
                    return true;
                }
                break;
            }}
            case CSeq_loc::e_Null:
            {{
                return true;
            }}
            default:
                break;
            }
        }
    }

    return false;
}

static const string sc_ValidExceptionText[] = {
  "RNA editing",
  "reasons given in citation"
};
DEFINE_STATIC_ARRAY_MAP(CStaticArraySet<string>, sc_LegatExceptText, sc_ValidExceptionText);

static bool s_IsValidExceptionText(const string& text)
{
    return sc_LegatExceptText.find(text) != sc_LegatExceptText.end();
}


static const string sc_ValidRefSeqExceptionText[] = {
    "RNA editing",
    "alternative processing",
    "alternative start codon",
    "artificial frameshift",
    "modified codon recognition",
    "nonconsensus splice site",
    "rearrangement required for product",
    "reasons given in citation",
    "ribosomal slippage",
    "trans-splicing",
    "unclassified transcription discrepancy",
    "unclassified translation discrepancy"
};
DEFINE_STATIC_ARRAY_MAP(CStaticArraySet<string>, sc_LegalRefSeqExceptText, sc_ValidRefSeqExceptionText);

static bool s_IsValidRefSeqExceptionText(const string& text)
{
    return sc_LegalRefSeqExceptText.find(text) != sc_LegalRefSeqExceptText.end();
}

bool s_GetGbValue( CConstRef<CSeq_feat> feat, const string& key, string& value )
{
    if ( ! feat->CanGetQual() ) {
        return false;
    }
    ITERATE( CSeq_feat::TQual, it, feat->GetQual() ) {
        if (!(*it)->CanGetQual()  ||  !(*it)->CanGetVal()) {
            continue;
        }
        if ( (*it)->GetQual() != key ) {
            continue;
        }
        value = (*it)->GetVal(); 
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
static CConstRef< CSeq_feat > s_GetGeneFeatureByLocus_tag(
    CBioseqContext& ctx,
    const string& given_tag )
//
//  Retrieve gene in given context carrying the given locus tag. Returns invalid
//  feature if not found.
//  ----------------------------------------------------------------------------
{
    CConstRef< CSeq_feat > GeneFeature;

    try {
        SAnnotSelector sel;
        sel.SetFeatType( CSeqFeatData::e_Gene )
            .SetFeatSubtype( CSeqFeatData::eSubtype_gene );
        for ( CFeat_CI it( ctx.GetTopLevelEntry(), sel ); it; ++it ) {
            const CSeq_feat& feat = it->GetMappedFeature();
            if ( ! feat.GetData().IsGene() ) {
                continue;
            }
            if ( ! feat.GetData().GetGene().IsSetLocus_tag() ) {
                continue;
            }
            if ( feat.GetData().GetGene().GetLocus_tag() == given_tag ) {
                GeneFeature.Reset( &feat );
                break;
            }
        }
    }
    catch( CException& ) {
        _TRACE( "s_GetGeneFeatureByLocus_tag(): Error: Feature iteration failed" );
    }
    return GeneFeature;
}


// -- FeatureItemBase

CFeatureItemBase::CFeatureItemBase
(const CSeq_feat& feat,
 CBioseqContext& ctx,
 const CSeq_loc* loc) :
    CFlatItem(&ctx), m_Feat(&feat), m_Loc(loc != 0 ? loc : &feat.GetLocation())
{
    x_SetObject(*m_Feat);
}

CConstRef<CFlatFeature> CFeatureItemBase::Format(void) const
{
    CRef<CFlatFeature> ff(new CFlatFeature(GetKey(),
                          *new CFlatSeqLoc(GetLoc(), *GetContext()),
                          *m_Feat));
    if ( ff ) {
        x_FormatQuals(*ff);
    }
    return ff;
}


//  -- CFeatureItem

string CFeatureItem::GetKey(void) const
{
    CBioseqContext& ctx = *GetContext();

    CSeqFeatData::E_Choice type = m_Feat->GetData().Which();
    CSeqFeatData::ESubtype subtype = m_Feat->GetData().GetSubtype();

    if (GetContext()->IsProt()) {   // protein
        if ( IsMappedFromProt()  &&  type == CSeqFeatData::e_Prot ) {
            if ( subtype == CSeqFeatData::eSubtype_preprotein         ||
                 subtype == CSeqFeatData::eSubtype_mat_peptide_aa     ||
                subtype == CSeqFeatData::eSubtype_sig_peptide_aa     ||
                subtype == CSeqFeatData::eSubtype_transit_peptide_aa ) {
                return "Precursor";
            } 
        }
        switch ( subtype ) {
        case CSeqFeatData::eSubtype_region:
            return "Region";
        case CSeqFeatData::eSubtype_bond:
            return "Bond";
        case CSeqFeatData::eSubtype_site:
            return "Site";
        default:
            break;
        }
    } else {  // nucleotide
        switch ( subtype ) {

        case CSeqFeatData::eSubtype_ncRNA:
            return "ncRNA";

        case CSeqFeatData::eSubtype_tmRNA:
            return "tmRNA";

        case CSeqFeatData::eSubtype_preprotein:
            if ( !ctx.IsRefSeq() ) {
                return "misc_feature";
            }
            break;

        case CSeqFeatData::eSubtype_site:
        case CSeqFeatData::eSubtype_bond:
        case CSeqFeatData::eSubtype_region:
        case CSeqFeatData::eSubtype_comment:
            return "misc_feature";

        default:
            break;
        }
    }

    // deal with unmappable impfeats
    if (subtype == CSeqFeatData::eSubtype_imp  &&  type == CSeqFeatData::e_Imp) {
        const CSeqFeatData::TImp& imp = m_Feat->GetData().GetImp();
        if ( imp.CanGetKey() ) {
            return imp.GetKey();
        }
    }

    return CFeatureItemBase::GetKey();
}


// constructor from CSeq_feat
CFeatureItem::CFeatureItem
(const CSeq_feat& feat,
 CBioseqContext& ctx,
 const CSeq_loc* loc,
 EMapped mapped) :
    CFeatureItemBase(feat, ctx, loc), m_Mapped(mapped)
{
    x_GatherInfo(ctx);
}


void CFeatureItem::x_GatherInfo(CBioseqContext& ctx)
{
    if ( s_SkipFeature(GetFeat(), GetLoc(), ctx) ) {
        x_SetSkip();
        return;
    }
    m_Type = m_Feat->GetData().GetSubtype();
    x_AddQuals(ctx);
}


//  ----------------------------------------------------------------------------
bool CFeatureItem::x_ExceptionIsLegalForFeature() const
//  ----------------------------------------------------------------------------
{
    CSeqFeatData::ESubtype subtype = m_Feat->GetData().GetSubtype();

    return ( subtype == CSeqFeatData::eSubtype_gene ||
             subtype == CSeqFeatData::eSubtype_cdregion ||
             subtype == CSeqFeatData::eSubtype_mRNA ||
             subtype == CSeqFeatData::eSubtype_tRNA ||
             subtype == CSeqFeatData::eSubtype_preRNA ||
             subtype == CSeqFeatData::eSubtype_otherRNA ||
             subtype == CSeqFeatData::eSubtype_3clip ||
             subtype == CSeqFeatData::eSubtype_3UTR ||
             subtype == CSeqFeatData::eSubtype_5clip ||
             subtype == CSeqFeatData::eSubtype_5UTR );
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualPartial(
    CBioseqContext& ctx )
//
//  Note: /partial has been depricated since DEC-2001. Current policy is to 
//  suppress /partial in entrez and release modes and let it stand in gbench and 
//  dump modes
//  ----------------------------------------------------------------------------
{
    if ( !ctx.Config().HideUnclassPartial() ) {
        if ( !IsMappedFromCDNA() || !ctx.IsProt() ) {
            if ( m_Feat->CanGetPartial()  &&  m_Feat->GetPartial() ) {
                if ( !s_LocIsFuzz( *m_Feat, GetLoc() ) ) {
                    x_AddQual( eFQ_partial, new CFlatBoolQVal( true ) );
                }
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualOperon(
    CBioseqContext& ctx,
    CSeqFeatData::ESubtype subtype )
//  ----------------------------------------------------------------------------
{
    if ( ! ctx.HasOperon() ) {
        return;
    }

    if ( subtype == CSeqFeatData::eSubtype_operon ||
         subtype == CSeqFeatData::eSubtype_gap ) {
        return;
    }

    const CGene_ref* gene_ref = m_Feat->GetGeneXref();
    if ( gene_ref == NULL  ||  !gene_ref->IsSuppressed()) {
            const CSeq_loc& operon_loc = ( ctx.IsProt() || !IsMapped() ) ? 
                m_Feat->GetLocation() : GetLoc();
        CConstRef<CSeq_feat> operon 
            = GetOverlappingOperon( operon_loc, ctx.GetScope() );
        if ( operon ) {
            const string& operon_name = operon->GetNamedQual( "operon" );
            if ( !operon_name.empty() ) {
                x_AddQual(eFQ_operon, new CFlatStringQVal(operon_name));
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualPseudo(
    CBioseqContext& ctx,
    CSeqFeatData::E_Choice type,
    CSeqFeatData::ESubtype subtype,
    bool pseudo )
//  ----------------------------------------------------------------------------
{
    if ( !pseudo || subtype == CSeqFeatData::eSubtype_repeat_region ) {
        return;
    }

    if (ctx.Config().DropIllegalQuals()  &&  
        ( type == CSeqFeatData::e_Rna || type == CSeqFeatData::e_Imp ) ) 
    {
        switch (subtype) {
            case  CSeqFeatData::eSubtype_allele:
            case  CSeqFeatData::eSubtype_attenuator:
            case  CSeqFeatData::eSubtype_CAAT_signal:
            case  CSeqFeatData::eSubtype_conflict:
            case  CSeqFeatData::eSubtype_D_loop:
            case  CSeqFeatData::eSubtype_enhancer:
            case  CSeqFeatData::eSubtype_GC_signal:
            case  CSeqFeatData::eSubtype_iDNA:
            case  CSeqFeatData::eSubtype_LTR:
            case  CSeqFeatData::eSubtype_misc_binding:
            case  CSeqFeatData::eSubtype_misc_difference:
            case  CSeqFeatData::eSubtype_misc_recomb:
            case  CSeqFeatData::eSubtype_misc_RNA:
            case  CSeqFeatData::eSubtype_misc_signal:
            case  CSeqFeatData::eSubtype_misc_structure:
            case  CSeqFeatData::eSubtype_modified_base:
            case  CSeqFeatData::eSubtype_mutation:
            case  CSeqFeatData::eSubtype_old_sequence:
            case  CSeqFeatData::eSubtype_polyA_signal:
            case  CSeqFeatData::eSubtype_polyA_site:
            case  CSeqFeatData::eSubtype_precursor_RNA:
            case  CSeqFeatData::eSubtype_prim_transcript:
            case  CSeqFeatData::eSubtype_primer_bind:
            case  CSeqFeatData::eSubtype_protein_bind:
            case  CSeqFeatData::eSubtype_RBS:
            case  CSeqFeatData::eSubtype_repeat_region:
            case  CSeqFeatData::eSubtype_repeat_unit:
            case  CSeqFeatData::eSubtype_rep_origin:
            case  CSeqFeatData::eSubtype_satellite:
            case  CSeqFeatData::eSubtype_stem_loop:
            case  CSeqFeatData::eSubtype_STS:
            case  CSeqFeatData::eSubtype_TATA_signal:
            case  CSeqFeatData::eSubtype_terminator:
            case  CSeqFeatData::eSubtype_unsure:
            case  CSeqFeatData::eSubtype_variation:
            case  CSeqFeatData::eSubtype_3clip:
            case  CSeqFeatData::eSubtype_3UTR:
            case  CSeqFeatData::eSubtype_5clip:
            case  CSeqFeatData::eSubtype_5UTR:
            case  CSeqFeatData::eSubtype_10_signal:
            case  CSeqFeatData::eSubtype_35_signal:
                return;
            default:
                break;
        }
    }
    x_AddQual( eFQ_pseudo, new CFlatBoolQVal( true ) );
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualSeqfeatNote()
//  ----------------------------------------------------------------------------
{
    if ( ! m_Feat->IsSetComment() ) {
        return;
    }
    string comment = m_Feat->GetComment();

    if ( ! comment.empty() ) {
        bool bAddPeriod = RemovePeriodFromEnd( comment, true );
        CRef<CFlatStringQVal> seqfeat_note( new CFlatStringQVal( comment ) );
        if ( bAddPeriod &&  ! x_GetStringQual(eFQ_prot_desc ) ) {
            seqfeat_note->SetAddPeriod();
        }
        x_AddQual( eFQ_seqfeat_note, seqfeat_note );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualExpInv(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    if ( ! m_Feat->IsSetExp_ev() ) {
        return;
    }

    if ( m_Feat->GetExp_ev() == CSeq_feat::eExp_ev_experimental ) {
        x_AddQual(eFQ_experiment, new CFlatExperimentQVal());
        return;
    }

    string value;
    CSeq_feat::TQual gbQuals = m_Feat->GetQual();
    for ( CSeq_feat::TQual::iterator it = gbQuals.begin();
        it != gbQuals.end(); ++it ) 
    {
        //
        //  Idea:
        //  If a gbqual specifying the inference exists then bail out and let
        //  gbqual processing take care of this qualifier. If no such gbqual is
        //  present then add a default inference qualifier.
        //
        if (!(*it)->CanGetQual()  ||  !(*it)->CanGetVal()) {
            continue;
        }
        if ( (*it)->GetQual() == "inference" ) {
            return;
        }          
    }
    x_AddQual(eFQ_inference, new CFlatInferenceQVal( "" ));
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualExceptions( 
    CBioseqContext& ctx )
//
//  Add any existing exception qualifiers.
//  Note: These include /ribosomal_slippage and /trans-splicing as special 
//  cases. Also, some exceptions are listed as notes.
//  ----------------------------------------------------------------------------
{
    const CSeqFeatData& data  = m_Feat->GetData();

    string raw_exception;
    const CFlatFileConfig& cfg = ctx.Config();

    if ( m_Feat->CanGetExcept_text()  &&  !m_Feat->GetExcept_text().empty() ) {
        raw_exception = m_Feat->GetExcept_text();
    }
    if ( raw_exception == "" ) {
        return;
    }

    list<string> exceptions;
    NStr::Split( raw_exception, ",", exceptions );

    list<string> output_exceptions;
    list<string> output_notes;
    ITERATE( list<string>, it, exceptions ) {
        string cur = NStr::TruncateSpaces( *it );

        //
        //  If exceptions aren't legal for the feature, then turn them into
        //  notes for strict modes, and let them stand in relaxed modes:
        //
        if ( ! x_ExceptionIsLegalForFeature() ) {
            if ( cfg.DropIllegalQuals() ) {
                output_notes.push_back( cur );
            }
            else {
                output_exceptions.push_back( cur );
            }
        }

        //
        //  If exceptions are legal then it depends on the exception. Some are
        //  turned into their own custom qualifiers. Others are allowed to stand
        //  as exceptions, while others are turned into notes.
        //
        else {
            if ( cur == "ribosomal slippage" ) {
                x_AddQual( eFQ_ribosomal_slippage, new CFlatBoolQVal( true ) );
                continue;
            }
            if ( cur == "trans-splicing" ) {
                x_AddQual( eFQ_trans_splicing, new CFlatBoolQVal( true ) );
                continue;
            }
            if ( cur == "nonconsensus splice site" ) {
                x_AddQual( eFQ_exception_note, new CFlatStringQVal( cur ) );
                continue;
            }
            if ( cur == "reasons given in citation" ) {
                if ( data.IsCdregion() || data.IsGene() ) {
                    output_exceptions.push_back( cur );
                }
                else { 
                    output_notes.push_back( cur );
                }
                continue;
            }
            if ( s_IsValidExceptionText( cur ) ) {
                output_exceptions.push_back( cur );
            }
            else if ( ctx.IsRefSeq() && s_IsValidRefSeqExceptionText( cur ) ) {
                output_exceptions.push_back( cur );
            }
            else {
//                if ( cfg.DropIllegalQuals() ) {
//                }
//                else {
                    output_notes.push_back( cur );
//                }
            }
        }
    }
    if ( ! output_exceptions.empty() ) {
        string exception = NStr::Join( output_exceptions, ", " );
        x_AddQual(eFQ_exception, new CFlatStringQVal( exception ) );
    }
    if ( ! output_notes.empty() ) {
        string note = NStr::Join( output_notes, ", " );
        x_AddQual(eFQ_exception_note, new CFlatStringQVal( note ) );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_GetAssociatedGeneInfo(
    CBioseqContext& ctx,
    const CGene_ref*& g_ref,                    //  out: gene ref
    CConstRef<CSeq_feat>& s_feat )              //  out: gene seq feat
//
//  Find the feature's related gene information. The association is established
//  through dbxref if it exists, and through bet overlap otherwise.
//
//  Note: Any of the two outs may be invalid if the corresponding information
//  could not be found.
//  ----------------------------------------------------------------------------
{
    g_ref = m_Feat->GetGeneXref();
    if (g_ref != NULL  &&  g_ref->CanGetDb() && g_ref->CanGetLocus_tag() ) {

        s_feat = s_GetGeneFeatureByLocus_tag( ctx, g_ref->GetLocus_tag() );
    }

    if ( s_feat == 0 ) {
        if ( ctx.IsProt()  ||  !IsMapped()) {

            s_feat = GetBestOverlappingFeat(
                *m_Feat,
                CSeqFeatData::e_Gene,
                sequence::eOverlap_Contained,
                ctx.GetScope(),
                fBestFeat_NoExpensive);
        } 
        else {
            s_feat = GetOverlappingGene(GetLoc(), ctx.GetScope());
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualNote(
    CConstRef<CSeq_feat> gene_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! gene_feat || ! gene_feat->CanGetComment() ) {
        return;
    }
    x_AddQual( eFQ_gene_note, new CFlatStringQVal( 
        gene_feat->GetComment() ) );
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualGeneXref(
    const CGene_ref* gene_ref, 
    const CConstRef<CSeq_feat>& gene_feat )
//  ----------------------------------------------------------------------------
{
    const CSeqFeatData& data  = m_Feat->GetData();
    CSeqFeatData::E_Choice type = data.Which();
    
    if ( type == CSeqFeatData::e_Cdregion || type == CSeqFeatData::e_Rna ) {    
        if ( ! gene_ref && gene_feat ) {
            gene_ref = &gene_feat->GetData().GetGene();
            if ( gene_ref != NULL  &&  gene_ref->IsSetDb() ) {
                x_AddQual(
                    eFQ_gene_xref, new CFlatXrefQVal( gene_ref->GetDb() ) );
            } else if ( gene_feat->IsSetDbxref() ) {
                x_AddQual(
                    eFQ_gene_xref, new CFlatXrefQVal( gene_feat->GetDbxref() ) );
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualOldLocusTag(
    CConstRef<CSeq_feat> gene_feat )
//
//  For non-gene features, add /old_locus_tag, if one exists somewhere.
//  ----------------------------------------------------------------------------
{
    if ( ! gene_feat ) {
        return;
    }

    const CSeq_feat::TQual& quals = gene_feat->GetQual();
    for ( size_t iPos = 0; iPos < quals.size(); ++iPos ) {
        CRef< CGb_qual > qual = quals[ iPos ];
        if ( ! qual->CanGetQual() || ! qual->CanGetVal() ) {
            continue;
        }
        if ( qual->GetQual() == "old_locus_tag" ) {
            x_AddQual(eFQ_old_locus_tag, 
                new CFlatStringQVal( qual->GetVal() ) );            
        }
    }
}

//  ----------------------------------------------------------------------------
bool CFeatureItem::x_GetPseudo(  
    const CGene_ref* gene_ref, 
    const CSeq_feat* gene_feat ) const
//  ----------------------------------------------------------------------------
{
    const CSeqFeatData& data  = m_Feat->GetData();
    CSeqFeatData::E_Choice type = data.Which();
    CSeqFeatData::ESubtype subtype = data.GetSubtype();
    
    bool pseudo = m_Feat->CanGetPseudo() ? m_Feat->GetPseudo() : false;
    if ( type != CSeqFeatData::e_Gene &&
         subtype != CSeqFeatData::eSubtype_operon &&
         subtype != CSeqFeatData::eSubtype_gap ) 
    {
        if ( gene_feat && gene_feat->CanGetPseudo() && gene_feat->GetPseudo() ) {
            return true;
            const CGene_ref* altref = &gene_feat->GetData().GetGene();
            if ( altref && altref->CanGetPseudo() && altref->GetPseudo() ) {
                return true;
            }
        }
        if ( gene_ref && gene_ref->CanGetPseudo() && gene_ref->GetPseudo() ) {
            return true;
        }
    }
    if ( type == CSeqFeatData::e_Gene ) {
        if ( data.GetGene().CanGetPseudo() && data.GetGene().GetPseudo() ) {
            return true;
        }
    }
    if ( type == CSeqFeatData::e_Rna ) {
        if ( data.GetRna().CanGetPseudo() && data.GetRna().GetPseudo() ) {
            return true;
        }
    }
    return pseudo;
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQuals(
    CBioseqContext& ctx )
//
//  Add the various qualifiers to this feature. Top level function.
//  ----------------------------------------------------------------------------
{
    if ( ctx.Config().IsFormatFTable() ) {
        x_AddFTableQuals( ctx );
        return;
    }
    //
    //  Collect/Compute data that will be shared between several qualifier
    //  collectors:
    //
    const CSeqFeatData& data  = m_Feat->GetData();
    CSeqFeatData::E_Choice type = data.Which();
    CSeqFeatData::ESubtype subtype = data.GetSubtype();

    const CGene_ref* gene_ref = 0;
    CConstRef<CSeq_feat> gene_feat;

    if ( type != CSeqFeatData::e_Gene &&
         subtype != CSeqFeatData::eSubtype_operon &&
         subtype != CSeqFeatData::eSubtype_gap ) 
    {
        x_GetAssociatedGeneInfo( ctx, gene_ref, gene_feat );
    }
    bool pseudo = x_GetPseudo(gene_ref, gene_feat );

    //
    //  Collect qualifiers that are common to most feature types:
    //
    x_AddQualPartial( ctx );
    x_AddQualDbXref();
    x_AddQualExt();
    x_AddQualExpInv( ctx );
    x_AddQualCitation();
    x_AddQualExceptions( ctx );
    x_AddQualNote( gene_feat );
    x_AddQualOldLocusTag( gene_feat );
    x_AddQualDb( gene_ref );
    x_AddQualGeneXref( gene_ref, gene_feat );
    x_AddQualOperon( ctx, subtype );
    x_AddQualsGene( gene_ref, gene_feat, gene_ref ? false : gene_feat.NotEmpty() );

    //
    //  Collect qualifiers that are specific to a single or just a few feature
    //  types:
    //
    switch ( type ) {
    case CSeqFeatData::e_Cdregion:
        x_AddQualsCdregion(*m_Feat, ctx, pseudo);
        break;
    case CSeqFeatData::e_Rna:
        x_AddQualsRna(*m_Feat, ctx, pseudo);
        break;
    case CSeqFeatData::e_Prot:
        x_AddQualsProt(ctx, pseudo);
        break;
    case CSeqFeatData::e_Region:
        x_AddQualsRegion( ctx );
        break;
    case CSeqFeatData::e_Site:
        x_AddQualsSite( ctx );
        break;
    case CSeqFeatData::e_Bond:
        x_AddQualsBond( ctx );
        break;    
    default:
        break;
    }

    x_AddQualPseudo( ctx, type, subtype, pseudo );
    x_AddQualSeqfeatNote();
    x_AddQualsGb( ctx );

    // cleanup (drop illegal quals, duplicate information etc.)
    x_CleanQuals( gene_ref );
}


static const string s_TrnaList[] = {
  "tRNA-Gap",
  "tRNA-Ala",
  "tRNA-Asx",
  "tRNA-Cys",
  "tRNA-Asp",
  "tRNA-Glu",
  "tRNA-Phe",
  "tRNA-Gly",
  "tRNA-His",
  "tRNA-Ile",
  "tRNA-Lys",
  "tRNA-Leu",
  "tRNA-Met",
  "tRNA-Asn",
  "tRNA-Pro",
  "tRNA-Gln",
  "tRNA-Arg",
  "tRNA-Ser",
  "tRNA-Thr",
  "tRNA-Sec",
  "tRNA-Val",
  "tRNA-Trp",
  "tRNA-OTHER",
  "tRNA-Tyr",
  "tRNA-Glx",
  "tRNA-TERM"
};


static const string& s_AaName(int aa)
{
    int shift = 0, idx = 255;

    if ( aa <= 74 ) {
        shift = 0;
    } else if (aa > 79) {
        shift = 2;
    } else {
        shift = 1;
    }
    if (aa != '*') {
        idx = aa - (64 + shift);
    } else {
        idx = 25;
    }
    if ( idx > 0 && idx < 26 ) {
        return s_TrnaList [idx];
    }
    return kEmptyStr;
}


static int s_ToIupacaa(int aa)
{
    vector<char> n(1, static_cast<char>(aa));
    vector<char> i;
    CSeqConvert::Convert(n, CSeqUtil::e_Ncbieaa, 0, 1, i, CSeqUtil::e_Iupacaa);
    return i.front();
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsRna(
    const CSeq_feat& feat,
    CBioseqContext& ctx,
    bool pseudo )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData::ESubtype subtype = m_Feat->GetData().GetSubtype();
    const CRNA_ref& rna = feat.GetData().GetRna();
    const CFlatFileConfig& cfg = ctx.Config();
    CScope& scope = ctx.GetScope();

    CRNA_ref::TType rna_type = rna.CanGetType() ?
        rna.GetType() : CRNA_ref::eType_unknown;
    switch ( rna_type ) {
    case CRNA_ref::eType_tRNA:
    {
        if (rna.IsSetExt()) {
            const CRNA_ref::C_Ext& ext = rna.GetExt();
            switch (ext.Which()) {
            case CRNA_ref::C_Ext::e_Name:
            {
                // amino acid could not be parsed into structured form
                if (!cfg.DropIllegalQuals()) {
                    x_AddQual(eFQ_product,
                        new CFlatStringQVal(ext.GetName()));
                } else {
                    x_AddQual(eFQ_product,
                        new CFlatStringQVal("tRNA-OTHER"));
                }
                break;
            }
            case CRNA_ref::C_Ext::e_TRNA:
            {
                const CTrna_ext& trna = ext.GetTRNA();
                int aa = 0;
                if ( trna.CanGetAa()  &&  trna.GetAa().IsNcbieaa() ) {
                    aa = trna.GetAa().GetNcbieaa();
                } else {
                    // !!!
                    return;
                }
                if (aa == 'U') {
                    if ( ctx.Config().SelenocysteineToNote() ) {
                        x_AddQual(eFQ_selenocysteine_note,
                            new CFlatStringQVal("selenocysteine"));
                    } else {
                        x_AddQual(eFQ_selenocysteine, new CFlatBoolQVal(true));
                    }
                }
                if ( cfg.IupacaaOnly() ) {
                    aa = s_ToIupacaa(aa);
                }
                const string& aa_str = s_AaName(aa);
                if ( !aa_str.empty() ) {
                    x_AddQual(eFQ_product, new CFlatStringQVal(aa_str));
                    if ( trna.CanGetAnticodon()  &&  !aa_str.empty() ) {
                        x_AddQual(eFQ_anticodon,
                            new CFlatAnticodonQVal(trna.GetAnticodon(),
                                                   aa_str.substr(5, NPOS)));
                    }
                }
                if ( trna.IsSetCodon() ) {
                    const string& comment =
                        m_Feat->IsSetComment() ? m_Feat->GetComment() : kEmptyStr;
                    x_AddQual(eFQ_trna_codons, new CFlatTrnaCodonsQVal(trna, comment));
                }
                break;
            }
            default:
                break;
            } // end of internal switch
        }
        break;
    }
    case CRNA_ref::eType_mRNA:
    {
        EFeatureQualifier slot = 
            (ctx.IsRefSeq()  ||  cfg.IsModeDump()  ||  cfg.IsModeGBench()) ?
                eFQ_transcript_id : eFQ_transcript_id_note;
        try {
            if (feat.IsSetProduct()) {
                CConstRef<CSeq_id> sip(feat.GetProduct().GetId());
                if (sip) {
                    CBioseq_Handle prod = 
                        scope.GetBioseqHandleFromTSE(*sip, ctx.GetHandle());
                    if ( prod ) {
                        x_AddProductIdQuals(prod, slot);
                    } else {
                        if (sip->IsGi()) {
                            string acc = GetAccessionForGi(sip->GetGi(), scope);
                            if (!cfg.DropIllegalQuals()  ||  IsValidAccession(acc)) {
                                CRef<CSeq_id> acc_id(new CSeq_id(acc));
                                x_AddQual(slot, new CFlatSeqIdQVal(*acc_id));
                            }
                            x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(*sip, true));
                        }
                    }
                }
            }
        } catch (CObjmgrUtilException&) {}
        if ( !pseudo  &&  cfg.ShowTranscript() ) {
            CSeqVector vec(feat.GetLocation(), scope);
            vec.SetCoding(CBioseq_Handle::eCoding_Iupac);
            string transcription;
            vec.GetSeqData(0, vec.size(), transcription);
            x_AddQual(eFQ_transcription, new CFlatStringQVal(transcription));
        }
        // intentional fall through
    }
    default:
        switch ( subtype ) {

        case CSeqFeatData::eSubtype_ncRNA:
        case CSeqFeatData::eSubtype_tmRNA:
            break;
        default:
            if ( rna.CanGetExt()  &&  rna.GetExt().IsName() ) {
                x_AddQual(eFQ_product, new CFlatStringQVal(rna.GetExt().GetName()));
            }
            break;
        }
    } // end of switch
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualTranslation( 
    CBioseq_Handle& bsh,
    CBioseqContext& ctx, 
    bool pseudo )
//  ----------------------------------------------------------------------------
{
    const CFlatFileConfig& cfg = ctx.Config();
    CScope& scope = ctx.GetScope();

    if ( pseudo || cfg.NeverTranslateCDS() ) {
        return;
    }

    string translation;
    if ( cfg.AlwaysTranslateCDS() || (cfg.TranslateIfNoProduct() && !bsh) ) {
        CCdregion_translate::TranslateCdregion( translation, *m_Feat, scope );
    }
    else if ( bsh ) {
        CSeqVector seqv = bsh.GetSeqVector();
        CSeq_data::E_Choice coding = cfg.IupacaaOnly() ?
            CSeq_data::e_Iupacaa : CSeq_data::e_Ncbieaa;
        seqv.SetCoding( coding );
        seqv.GetSeqData( 0, seqv.size(), translation );
    }

    if (!NStr::IsBlank(translation)) {
        x_AddQual(eFQ_translation, new CFlatStringQVal( translation ) );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualCodeBreak( 
    const CCdregion& cdr,
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    if ( ! cdr.IsSetCode_break() ) {
        return;
    }

    ITERATE ( CCdregion::TCode_break, it, cdr.GetCode_break() ) {
        if ( !(*it)->IsSetAa() ) {
            continue;
        }
        const CCode_break::C_Aa& cbaa = (*it)->GetAa();
        bool is_U = false;
        switch ( cbaa.Which() ) {
        case CCode_break::C_Aa::e_Ncbieaa:
            is_U = (cbaa.GetNcbieaa() == 'U');
            break;
        case CCode_break::C_Aa::e_Ncbi8aa:
            is_U = (cbaa.GetNcbieaa() == 'U');
        case CCode_break::C_Aa::e_Ncbistdaa:
            is_U = (cbaa.GetNcbieaa() == 24);
            break;
        default:
            break;
        }
        
        if ( is_U ) {
            if ( ctx.Config().SelenocysteineToNote() ) {
                x_AddQual( eFQ_selenocysteine_note,
                    new CFlatStringQVal( "selenocysteine" ) );
            } else {
                x_AddQual( eFQ_selenocysteine, new CFlatBoolQVal( true ) );
            }
            break;
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualTranslationTable( 
    const CCdregion& cdr,
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    if ( ! cdr.CanGetCode() ) {
        return;
    }
    int gcode = cdr.GetCode().GetId();
    if ( gcode == 1 || gcode == 255 ) {
        return;
    }
    if ( ctx.Config().IsFormatGBSeq() || gcode != 1 ) {
        x_AddQual(eFQ_transl_table, new CFlatIntQVal(gcode));
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualCodonStart( 
    const CCdregion& cdr,
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    CCdregion::TFrame frame = cdr.GetFrame();

    if ( !ctx.IsProt() || !IsMappedFromCDNA() ) {
        if ( frame == CCdregion::eFrame_not_set ) {
            frame = CCdregion::eFrame_one;
        }
        x_AddQual( eFQ_codon_start, new CFlatIntQVal( frame ) );
    } 
    else {
        if ( frame > 1 ) {
            x_AddQual( eFQ_codon_start, new CFlatIntQVal( frame ) );
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualTranslationException( 
    const CCdregion& cdr,
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
     if ( !ctx.IsProt() || !IsMappedFromCDNA() ) {
        if ( cdr.IsSetCode_break() ) {
            x_AddQual( eFQ_transl_except, 
                new CFlatCodeBreakQVal( cdr.GetCode_break() ) );
        }
        
    } 
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProteinConflict( 
    const CCdregion& cdr,
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    static const string conflic_msg = 
        "Protein sequence is in conflict with the conceptual translation";

    if ( !ctx.IsProt() || !IsMappedFromCDNA() ) {
        bool has_prot = false;
        if ( m_Feat->IsSetProduct() && m_Feat->GetProduct().GetId() != 0 ) {
            const CSeq_id* prot_id = m_Feat->GetProduct().GetId();
            has_prot = ctx.GetScope().GetBioseqHandleFromTSE( *prot_id, 
                ctx.GetTopLevelEntry() );
        }
        if ( cdr.CanGetConflict()  &&  cdr.GetConflict()  &&  has_prot ) {
            x_AddQual( eFQ_prot_conflict, new CFlatStringQVal( conflic_msg ) );
        }
    } 
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualCodedBy( 
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    if ( ctx.IsProt()  &&  IsMappedFromCDNA() ) {
        x_AddQual( eFQ_coded_by, new CFlatSeqLocQVal( m_Feat->GetLocation() ) );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtComment(
    const CBioseq_Handle& protHandle )
//  ----------------------------------------------------------------------------
{
    if ( ! protHandle ) {
        return;
    }   
    CSeqdesc_CI comm( protHandle, CSeqdesc::e_Comment, 1 );
    if ( comm && !comm->GetComment().empty() ) {
        x_AddQual( eFQ_prot_comment, new CFlatStringQVal( comm->GetComment() ) );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtMethod(
    const CBioseq_Handle& protHandle )
//  ----------------------------------------------------------------------------
{
    if ( ! protHandle ) {
        return;
    }   
    CSeqdesc_CI mi( protHandle, CSeqdesc::e_Molinfo );
    if ( mi ) {
        CMolInfo::TTech prot_tech = mi->GetMolinfo().GetTech();
        if ( prot_tech >  CMolInfo::eTech_standard       &&
             prot_tech != CMolInfo::eTech_concept_trans  &&
             prot_tech != CMolInfo::eTech_concept_trans_a ) {
            if ( !GetTechString( prot_tech ).empty() ) {
                x_AddQual( eFQ_prot_method, new CFlatStringQVal( 
                    "Method: " + GetTechString( prot_tech) ) );
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_GetAssociatedProtInfo(
    CBioseqContext& ctx,
    CBioseq_Handle& protHandle,
    const CProt_ref*& protRef,
    CConstRef<CSeq_feat>& protFeat,
    CConstRef<CSeq_id>& protId )
//  ----------------------------------------------------------------------------
{
    const CFlatFileConfig& cfg = ctx.Config();
    CScope& scope = ctx.GetScope();

    protId.Reset( m_Feat->GetProduct().GetId() );
    if ( protId ) {
        if ( !cfg.AlwaysTranslateCDS() ) {
            CScope::EGetBioseqFlag get_flag = CScope::eGetBioseq_Loaded;
            if ( cfg.ShowFarTranslations() || ctx.IsGED() ) {
                get_flag = CScope::eGetBioseq_All;
            }
            protHandle =  scope.GetBioseqHandle(*protId, get_flag);
        }
    }
    
    protRef = 0;
    if ( protHandle ) {
        protFeat.Reset( s_GetBestProtFeature( protHandle ) );
        if ( protFeat ) {
            protRef = &( protFeat->GetData().GetProt() );
        }
    } 
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtNote( 
    const CProt_ref* protRef, 
    CConstRef<CSeq_feat> protFeat )
//  ----------------------------------------------------------------------------
{
    if ( ! protRef ) {
        return;
    }
    if ( protFeat->IsSetComment() ) {
        if ( protRef->GetProcessed() == CProt_ref::eProcessed_not_set  ||
                protRef->GetProcessed() == CProt_ref::eProcessed_preprotein ) {
            string prot_note = protFeat->GetComment();
            TrimSpacesAndJunkFromEnds( prot_note, true );
            RemovePeriodFromEnd( prot_note, true );
            x_AddQual( eFQ_prot_note, new CFlatStringQVal( prot_note ) );
        }
    }
}


//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProteinId(
    CBioseqContext& ctx,
    const CBioseq_Handle& protHandle,
    CConstRef<CSeq_id> protId )
//  ----------------------------------------------------------------------------
{
    if ( protHandle || !protId ) {
        return;
    }

    CScope& scope = ctx.GetScope();
    const CFlatFileConfig& cfg = ctx.Config();

    if ( protId->IsGi() && protId->GetGi() > 0 ) {
        string prot_acc;
        try {
            prot_acc = GetAccessionForGi( protId->GetGi(), scope );
            if ( !cfg.DropIllegalQuals() || IsValidAccession( prot_acc ) ) {
                CRef<CSeq_id> acc_id( new CSeq_id( prot_acc ) );
                x_AddQual( eFQ_protein_id, new CFlatSeqIdQVal( *acc_id ) );
            }
        } catch ( CException& ) {}
        x_AddQual( eFQ_db_xref, new CFlatSeqIdQVal( *protId, true ) );
    }
    else {
        x_AddQual( eFQ_protein_id, new CFlatSeqIdQVal( *protId ) );
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualCdsProduct(
    CBioseqContext& ctx,
    const CProt_ref* protRef )
//  ----------------------------------------------------------------------------
{
    if ( !protRef ) {
        return;
    }

    const CFlatFileConfig& cfg = ctx.Config();
    const CProt_ref::TName& names = protRef->GetName();
    if ( !names.empty() ) {
        if ( ! cfg.IsModeDump() ) {
            x_AddQual( eFQ_cds_product, 
                new CFlatStringQVal( names.front() ) );
            if ( names.size() > 1 ) {
                x_AddQual( eFQ_prot_names, 
                    new CFlatProductNamesQVal( names, m_Gene ) );
            }

        } else {
            ITERATE(CProt_ref::TName, it, names) {
                x_AddQual( eFQ_cds_product, new CFlatStringQVal(*it) );
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtDesc(
    const CProt_ref* protRef )
//  ----------------------------------------------------------------------------
{
    if ( !protRef || !protRef->CanGetDesc() ) {
        return;
    }

    string desc = protRef->GetDesc();
    TrimSpacesAndJunkFromEnds( desc );
    bool add_period = RemovePeriodFromEnd( desc, true );
    CRef<CFlatStringQVal> prot_desc( new CFlatStringQVal( desc ) );
    if ( add_period ) {
        prot_desc->SetAddPeriod();
    }
    x_AddQual( eFQ_prot_desc, prot_desc );
}
    
//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtActivity(
    const CProt_ref* protRef )
//  ----------------------------------------------------------------------------
{
    if ( !protRef || protRef->GetActivity().empty() ) {
        return;
    }
    ITERATE (CProt_ref::TActivity, it, protRef->GetActivity()) {
        x_AddQual(eFQ_prot_activity, new CFlatStringQVal(*it));
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualProtEcNumber(
    CBioseqContext& ctx,
    const CProt_ref* protRef )
//  ----------------------------------------------------------------------------
{
    if ( !protRef || protRef->GetEc().empty() ) {
        return;
    }

    const CFlatFileConfig& cfg = ctx.Config();
    ITERATE(CProt_ref::TEc, ec, protRef->GetEc()) {
        if ( !cfg.DropIllegalQuals()  ||  s_IsLegalECNumber( *ec ) ) {
            x_AddQual( eFQ_prot_EC_number, new CFlatStringQVal( *ec ) );
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsCdregion(
    const CSeq_feat& cds,
    CBioseqContext& ctx,
    bool pseudo)
//  ----------------------------------------------------------------------------
{
    const CCdregion& cdr = cds.GetData().GetCdregion();

    const CProt_ref* protRef = 0;
    CConstRef<CSeq_feat> protFeat;
    CConstRef<CSeq_id> prot_id;
    CBioseq_Handle prot;

    x_AddQualCodeBreak( cdr, ctx );
    x_AddQualTranslationTable( cdr, ctx );
    x_AddQualCodonStart( cdr, ctx );
    x_AddQualTranslationException( cdr, ctx );
    x_AddQualProteinConflict( cdr, ctx );
    x_AddQualCodedBy( ctx );
    if ( ctx.IsProt()  &&  IsMappedFromCDNA() ) {
        return;
    }

    // protein qualifiers
    if (m_Feat->IsSetProduct()) {
        x_GetAssociatedProtInfo( ctx, prot, protRef, protFeat, prot_id );
        x_AddQualProtComment( prot );
        x_AddQualProtMethod( prot );
        x_AddQualProtNote( protRef, protFeat );
        x_AddQualsProductId( prot );
        x_AddQualProteinId( ctx, prot, prot_id );
        x_AddQualTranslation( prot, ctx, pseudo );
    }

    // add qualifiers where associated xref overrides the ref:
    const CProt_ref* protXRef = m_Feat->GetProtXref();
    if ( ! protXRef ) {
        protXRef = protRef;
    }
    x_AddQualCdsProduct( ctx, protXRef );
    x_AddQualProtDesc( protXRef );
    x_AddQualProtActivity( protXRef );
    x_AddQualProtEcNumber( ctx, protXRef );
}

static int s_ScoreSeqIdHandle(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    CRef<CSeq_id> id_non_const
        (const_cast<CSeq_id*>(id.GetPointer()));
    return CSeq_id::Score(id_non_const);
}


CSeq_id_Handle s_FindBestIdChoice(const CBioseq_Handle::TId& ids)
{
    //
    //  Objective:
    //  Find the best choice among a given subset of id types. I.e. if a certain
    //  id scores well but is not of a type we approve of, we still reject it.
    //
    CBestChoiceTracker< CSeq_id_Handle, int (*)(const CSeq_id_Handle&) > 
        tracker(s_ScoreSeqIdHandle);

    ITERATE( CBioseq_Handle::TId, it, ids ) {
        switch( (*it).Which() ) {
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
            case CSeq_id::e_Gi:
            case CSeq_id::e_Other:
            case CSeq_id::e_General:
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
            case CSeq_id::e_Gpipe:
                tracker(*it);
                break;
            default:
                break;
        }
    }
    return tracker.GetBestChoice();
}

//  ---------------------------------------------------------------------------
void CFeatureItem::x_AddProductIdQuals(
    CBioseq_Handle& prod, 
    EFeatureQualifier slot)
//  ---------------------------------------------------------------------------
{
    //
    //  Objective (according to the C toolkit):
    //  We need one (and only one) /xxx_id tag. If there are multiple ids 
    //  available, try and pick the "best" one.
    //  In addition, if an id of type GI is available, turn it into a /db_xref 
    //  tag, regardless of whether we already used it for the /xxx_id tag
    //  or not.
    //

    if (!prod) {
        return;
    }
    const CBioseq_Handle::TId& ids = prod.GetId();
    if (ids.empty()) {
        return;
    }

    CSeq_id_Handle best = s_FindBestIdChoice(ids);
    if (!best) {
        return;
    }
    x_AddQual(slot, new CFlatSeqIdQVal(*best.GetSeqId()));
    
    ITERATE (CBioseq_Handle::TId, it, ids) {
        if ( it->Which() != CSeq_id::e_Gi ) {
            continue;
        }
        CConstRef<CSeq_id> id = it->GetSeqId();
        if (!id->IsGeneral()) {
            x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(*id, id->IsGi()));
        }
    }
}

//  ---------------------------------------------------------------------------
void CFeatureItem::x_AddQualsProductId(
    CBioseq_Handle& protHandle )
//  ---------------------------------------------------------------------------
{
    //
    //  Objective (according to the C toolkit):
    //  We need one (and only one) /xxx_id tag. If there are multiple ids 
    //  available, try and pick the "best" one.
    //  In addition, if an id of type GI is available, turn it into a /db_xref 
    //  tag, regardless of whether we already used it for the /xxx_id tag
    //  or not.
    //

    if ( !protHandle ) {
        return;
    }
    const CBioseq_Handle::TId& ids = protHandle.GetId();
    if ( ids.empty() ) {
        return;
    }

    CSeq_id_Handle best = s_FindBestIdChoice( ids );
    if ( !best ) {
        return;
    }
    x_AddQual( eFQ_protein_id, new CFlatSeqIdQVal( *best.GetSeqId() ) );
    
    ITERATE (CBioseq_Handle::TId, it, ids) {
        if ( it->Which() != CSeq_id::e_Gi ) {
            continue;
        }
        CConstRef<CSeq_id> id = it->GetSeqId();
        if (!id->IsGeneral()) {
            x_AddQual( eFQ_db_xref, new CFlatSeqIdQVal( *id, id->IsGi() ) );
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsRegion(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    assert( m_Feat->GetData().IsRegion() );

    const CSeqFeatData& data = m_Feat->GetData();
    const string& region = data.GetRegion();
    if ( region.empty() ) {
        return;
    }

    if ( ctx.IsProt()  &&
         data.GetSubtype() == CSeqFeatData::eSubtype_region ) {
        x_AddQual(eFQ_region_name, new CFlatStringQVal(region));
    } else {
        x_AddQual(eFQ_region, new CFlatStringQVal("Region: " + region));
    }
}


//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsBond(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    assert( m_Feat->GetData().IsBond() );

    const CSeqFeatData& data = m_Feat->GetData();
    const string& bond = s_GetBondName( data.GetBond() );
    if ( NStr::IsBlank( bond ) ) {
        return;
    }
    
    if ( ctx.IsGenbankFormat()  &&  ctx.IsProt() ) {
        x_AddQual( eFQ_bond_type, new CFlatStringQVal( bond ) );
    } else {
        x_AddQual( eFQ_bond, new CFlatBondQVal( bond ) );
    }
}


static const string& s_GetSiteName(CSeqFeatData::TSite site)
{
    static const string kOther = "other";
    static const string kDnaBinding = "DNA binding";

    if (site == CSeqFeatData::eSite_other) {
        return kOther;
    } else if (site == CSeqFeatData::eSite_dna_binding) {
        return kDnaBinding;
    } else {
        return CSeqFeatData::ENUM_METHOD_NAME(ESite)()->FindName(site, true);
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsSite( 
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    assert( m_Feat->GetData().IsSite() );

    const CSeqFeatData& data = m_Feat->GetData();
    CSeqFeatData::TSite site = data.GetSite();
    const string& site_name = s_GetSiteName( site );

    if ( ctx.Config().IsFormatGenbank()  &&  ctx.IsProt() ) {
        x_AddQual(eFQ_site_type, new CFlatSiteQVal( site_name ) );
    } else {
        if ( !m_Feat->IsSetComment() ||  
            ( NStr::Find( m_Feat->GetComment(), site_name ) == NPOS ) ) {
            x_AddQual( eFQ_site, new CFlatSiteQVal( site_name ) );
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsExt(
    const CSeq_feat::TExt& ext )
//  ----------------------------------------------------------------------------
{
    ITERATE (CUser_object::TData, it, ext.GetData()) {
        const CUser_field& field = **it;
        if ( !field.CanGetData() ) {
            continue;
        }
        if ( field.GetData().IsObject() ) {
            const CUser_object& obj = field.GetData().GetObject();
            x_AddQualsExt(obj);
            return;
        } else if ( field.GetData().IsObjects() ) {
            ITERATE (CUser_field::C_Data::TObjects, o, field.GetData().GetObjects()) {
                x_AddQualsExt(**o);
            }
            return;
        }
    }
    if ( ext.CanGetType()  &&  ext.GetType().IsStr() ) {
        const string& oid = ext.GetType().GetStr();
        if ( oid == "ModelEvidence" ) {
            x_AddQual(eFQ_modelev, new CFlatModelEvQVal(ext));
        } else if ( oid == "GeneOntology" ) {
            x_AddGoQuals(ext);
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddGoQuals(
    const CUser_object& uo )
//  ----------------------------------------------------------------------------
{
    ITERATE (CUser_object::TData, uf_it, uo.GetData()) {
        const CUser_field& field = **uf_it;
        if ( field.IsSetLabel()  &&  field.GetLabel().IsStr() ) {
            const string& label = field.GetLabel().GetStr();
            EFeatureQualifier slot = eFQ_none;
            if ( label == "Process" ) {
                slot = eFQ_go_process;
            } else if ( label == "Component" ) {               
                slot = eFQ_go_component;
            } else if ( label == "Function" ) {
                slot = eFQ_go_function;
            }
            if ( slot == eFQ_none ) {
                continue;
            }

            ITERATE (CUser_field::TData::TFields, it, field.GetData().GetFields()) {
                if ( (*it)->GetData().IsFields() ) {
                    x_AddQual(slot, new CFlatGoQVal(**it));
                }
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsGene(
    const CGene_ref* gene_ref,
    CConstRef<CSeq_feat>& gene_feat,
    bool from_overlap )
//  ----------------------------------------------------------------------------
{
    const CSeqFeatData& data = m_Feat->GetData();
    CSeqFeatData::ESubtype subtype = data.GetSubtype();

    if ( subtype == CSeqFeatData::eSubtype_primer_bind ) {
        return;
    }
    if ( m_Feat->GetData().Which() == CSeqFeatData::e_Gene ) {
        gene_ref = &( m_Feat->GetData().GetGene() );
    }
    if ( ! gene_ref && gene_feat ) {
        gene_ref = & gene_feat->GetData().GetGene();
    }
    if ( ! gene_ref || gene_ref->IsSuppressed() ) {
        return;
    }

    const CBioseqContext& ctx = *GetContext();

    bool is_gene = (subtype == CSeqFeatData::eSubtype_gene);

    const string* locus = (gene_ref->IsSetLocus()  &&  !NStr::IsBlank(gene_ref->GetLocus())) ?
        &gene_ref->GetLocus() : NULL;
    const string* desc = (gene_ref->IsSetDesc() &&  !NStr::IsBlank(gene_ref->GetDesc())) ?
        &gene_ref->GetDesc() : NULL;
    const TGeneSyn* syn = (gene_ref->IsSetSyn()  &&  !gene_ref->GetSyn().empty()) ?
        &gene_ref->GetSyn() : NULL;
    const string* locus_tag =
        (gene_ref->IsSetLocus_tag()  &&  !NStr::IsBlank(gene_ref->GetLocus_tag())) ?
        &gene_ref->GetLocus_tag() : 0;

    EFeatureQualifier syn_qual = ctx.Config().GeneSynsToNote()  ||  !ctx.IsRefSeq() ?
        eFQ_gene_syn : eFQ_gene_syn_refseq;

    if ( !from_overlap  ||  subtype != CSeqFeatData::eSubtype_repeat_region ) {
        if (locus != NULL) {
            m_Gene = *locus;
            x_AddQual(eFQ_gene, new CFlatGeneQVal(m_Gene));
            if (locus_tag != NULL) {
                x_AddQual(eFQ_locus_tag, new CFlatStringQVal(*locus_tag));
            }
            if (is_gene  &&  desc != NULL) {
                x_AddQual(eFQ_gene_desc, new CFlatStringQVal(*desc));
            }
            if (is_gene  &&  syn != NULL) {
                x_AddQual(syn_qual, new CFlatGeneSynonymsQVal(*syn));
            }
        } else if (locus_tag != NULL) {
            x_AddQual(eFQ_locus_tag, new CFlatStringQVal(*locus_tag));
            if (is_gene  &&  desc != NULL) {
                x_AddQual(eFQ_gene_desc, new CFlatStringQVal(*desc));
            }
            if (is_gene  &&  syn != NULL) {
                x_AddQual(syn_qual, new CFlatGeneSynonymsQVal(*syn));
            }
        } else if (desc != NULL) {
            m_Gene = *desc;
            x_AddQual(eFQ_gene, new CFlatGeneQVal(m_Gene));
            if (is_gene  &&  syn != NULL) {
                x_AddQual(syn_qual, new CFlatGeneSynonymsQVal(*syn));
            }
        } else if (syn != NULL) {
            // add the first as the gene name ...
            CGene_ref::TSyn syns = *syn;
            m_Gene = syns.front();
            x_AddQual(eFQ_gene, new CFlatGeneQVal(m_Gene));
            syns.pop_front();
            // ... and the rest as synonyms
            if (is_gene  &&  !syns.empty() ) {
                x_AddQual(syn_qual, new CFlatGeneSynonymsQVal(syns));
            }
        }
    }


    // /allele
    if (subtype != CSeqFeatData::eSubtype_variation && 
      subtype != CSeqFeatData::eSubtype_repeat_region &&
      subtype != CSeqFeatData::eSubtype_primer_bind ) {
        if (gene_ref->IsSetAllele()  &&  !NStr::IsBlank(gene_ref->GetAllele())) {
            x_AddQual(eFQ_gene_allele, new CFlatStringQVal(gene_ref->GetAllele()));
        }
    }

    if (gene_ref->IsSetDb()) {
        x_AddQual(eFQ_gene_xref, new CFlatXrefQVal(gene_ref->GetDb()));
    }

    if (!from_overlap  &&  gene_ref->IsSetMaploc()) {
        x_AddQual(eFQ_gene_map, new CFlatStringQVal(gene_ref->GetMaploc()));
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddQualsProt(
    CBioseqContext& ctx, 
    bool pseudo)
//  ----------------------------------------------------------------------------
{
    assert( m_Feat->GetData().IsProt() );

    const CSeqFeatData& data = m_Feat->GetData();
    const CProt_ref& pref = data.GetProt();
    CProt_ref::TProcessed processed = pref.GetProcessed();

    if ( ctx.IsNuc()  ||  (ctx.IsProt()  &&  !IsMappedFromProt()) ) {
        if ( pref.IsSetName()  &&  !pref.GetName().empty() ) {
            const CProt_ref::TName& names = pref.GetName();
            x_AddQual(eFQ_product, new CFlatStringQVal(names.front()));
            if (names.size() > 1) {
                x_AddQual(eFQ_prot_names, new CFlatProductNamesQVal(names, m_Gene));
            }
        }
        if ( pref.CanGetDesc()  &&  !pref.GetDesc().empty() ) {
            if ( !ctx.IsProt() ) {
                string desc = pref.GetDesc();
                TrimSpacesAndJunkFromEnds(desc);
                bool add_period = RemovePeriodFromEnd(desc, true);
                CRef<CFlatStringQVal> prot_desc(new CFlatStringQVal(desc));
                if (add_period) {
                    prot_desc->SetAddPeriod();
                }
                x_AddQual(eFQ_prot_desc, prot_desc);
//                had_prot_desc = true;
            } else {
                x_AddQual(eFQ_prot_name, new CFlatStringQVal(pref.GetDesc()));
            }
        }
        if ( pref.IsSetActivity()  &&  !pref.GetActivity().empty() ) {
            if ( ctx.IsNuc()  ||  processed != CProt_ref::eProcessed_mature ) {
                ITERATE (CProt_ref::TActivity, it, pref.GetActivity()) {
                    if (!NStr::IsBlank(*it)) {
                        x_AddQual(eFQ_prot_activity, new CFlatStringQVal(*it));
                    }
                }
            }
        }
        if (!pref.GetEc().empty()) {
            ITERATE(CProt_ref::TEc, ec, pref.GetEc()) {
                if (s_IsLegalECNumber(*ec)) {
                    x_AddQual(eFQ_prot_EC_number, new CFlatStringQVal(*ec));
                }
            }
        }
        if ( m_Feat->CanGetProduct() ) {
            CBioseq_Handle prot = 
                ctx.GetScope().GetBioseqHandle( m_Feat->GetProduct() );
            if ( prot ) {
                x_AddProductIdQuals(prot, eFQ_protein_id);
            } else {
                try {
                    const CSeq_id& prod_id = 
                        GetId( m_Feat->GetProduct(), &ctx.GetScope());
                    if ( ctx.IsRefSeq()  ||  !ctx.Config().ForGBRelease() ) {
                        x_AddQual(eFQ_protein_id, new CFlatSeqIdQVal(prod_id));
                    }
                } catch (CObjmgrUtilException&) {}
            }
        }
    } else { // protein feature on subpeptide bioseq
        x_AddQual(eFQ_derived_from, new CFlatSeqLocQVal(m_Feat->GetLocation()));
    }
    if ( !pseudo  &&  ctx.Config().ShowPeptides() ) {
        if ( processed == CProt_ref::eProcessed_mature          ||
             processed == CProt_ref::eProcessed_signal_peptide  ||
             processed == CProt_ref::eProcessed_transit_peptide ) {
            CSeqVector pep(m_Feat->GetLocation(), ctx.GetScope());
            pep.SetCoding(CSeq_data::e_Ncbieaa);
            string peptide;
            pep.GetSeqData(pep.begin(), pep.end(), peptide);
            if (!NStr::IsBlank(peptide)) {
                x_AddQual(eFQ_peptide, new CFlatStringQVal(peptide));
            }
        }
    }

    // cleanup
    if ( processed == CProt_ref::eProcessed_signal_peptide  ||
         processed == CProt_ref::eProcessed_transit_peptide ) {
        if ( !ctx.IsRefSeq() ) {
           // Only RefSeq allows product on signal or transit peptide
           x_RemoveQuals(eFQ_product);
        }
    }
    if ( processed == CProt_ref::eProcessed_preprotein  &&
         !ctx.IsRefSeq()  &&  !ctx.IsProt()  &&  
         data.GetSubtype() == CSeqFeatData::eSubtype_preprotein ) {
        const CFlatStringQVal* product = x_GetStringQual(eFQ_product);
        if (product != NULL) {
            x_AddQual(eFQ_encodes, new CFlatStringQVal("encodes " + product->GetValue()));
            x_RemoveQuals(eFQ_product);
        }
    }
}


static void s_ParseParentQual(const CGb_qual& gbqual, list<string>& vals)
{
    vals.clear();

    if (!gbqual.IsSetVal()  || NStr::IsBlank(gbqual.GetVal())) {
        return;
    }

    const string& val = gbqual.GetVal();
    
    if (val.length() > 1  &&  NStr::StartsWith(val, '(')  &&
        NStr::EndsWith(val, ')')  && val.find(',') != NPOS) {
        NStr::Split(val, "(,)", vals);
    } else {
        vals.push_back(val);
    }

    list<string>::iterator it = vals.begin();
    while (it != vals.end()) {
        if (NStr::IsBlank(*it)) {
            it = vals.erase(it);
        } else {
            ConvertQuotes(*it);
            ExpandTildes(*it, eTilde_space);
            ++it;
        }
    }
}


struct SLegalImport {
    const char*       m_Name;
    EFeatureQualifier m_Value;

    operator string(void) const { return m_Name; }
};


static bool s_IsValidDirection(const string& direction) {
    return NStr::EqualNocase(direction, "LEFT")   ||
           NStr::EqualNocase(direction, "RIGHT")  ||
           NStr::EqualNocase(direction, "BOTH");
}


static bool s_IsValidnConsSplice(const string& cons_splice) {
    return NStr::EqualNocase(cons_splice, "(5'site:YES, 3'site:YES)")     ||
           NStr::EqualNocase(cons_splice, "(5'site:YES, 3'site:NO)")      ||
           NStr::EqualNocase(cons_splice, "(5'site:YES, 3'site:ABSENT)")  ||
           NStr::EqualNocase(cons_splice, "(5'site:NO, 3'site:YES)")      ||
           NStr::EqualNocase(cons_splice, "(5'site:NO, 3'site:NO)")       ||
           NStr::EqualNocase(cons_splice, "(5'site:NO, 3'site:ABSENT)")   ||
           NStr::EqualNocase(cons_splice, "(5'site:ABSENT, 3'site:YES)")  ||
           NStr::EqualNocase(cons_splice, "(5'site:ABSENT, 3'site:NO)")   ||
           NStr::EqualNocase(cons_splice, "(5'site:ABSENT, 3'site:ABSENT)");
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_ImportQuals(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    _ASSERT(m_Feat->IsSetQual());

    typedef pair<const char*, EFeatureQualifier> TLegalImport;
    static const TLegalImport kLegalImports[] = {
        // Must be in case-insensitive alphabetical order!
#define DO_IMPORT(x) TLegalImport(#x, eFQ_##x)
        DO_IMPORT(allele),
        DO_IMPORT(bound_moiety),
        DO_IMPORT(clone),
        DO_IMPORT(codon),
        DO_IMPORT(compare),
        DO_IMPORT(cons_splice),
        DO_IMPORT(direction),
        DO_IMPORT(EC_number),
        DO_IMPORT(estimated_length),
        DO_IMPORT(evidence),
        DO_IMPORT(experiment),
        DO_IMPORT(frequency),
        DO_IMPORT(function),
        DO_IMPORT(inference),
        DO_IMPORT(label),
        DO_IMPORT(map),
        DO_IMPORT(mobile_element),
        DO_IMPORT(mod_base),
        DO_IMPORT(ncRNA_class),
        DO_IMPORT(number),
        DO_IMPORT(old_locus_tag),
        DO_IMPORT(operon),
        DO_IMPORT(organism),
        DO_IMPORT(PCR_conditions),
        DO_IMPORT(phenotype),
        DO_IMPORT(product),
        DO_IMPORT(replace),
        DO_IMPORT(ribosomal_slippage),
        DO_IMPORT(rpt_family),
        DO_IMPORT(rpt_type),
        DO_IMPORT(rpt_unit),
        DO_IMPORT(rpt_unit_range),
        DO_IMPORT(rpt_unit_seq),
        DO_IMPORT(standard_name),
        DO_IMPORT(trans_splicing),
        DO_IMPORT(usedin)
#undef DO_IMPORT
    };
    typedef const CStaticArrayMap<const char*, EFeatureQualifier, PNocase_CStr> TLegalImportMap;
    DEFINE_STATIC_ARRAY_MAP(TLegalImportMap, kLegalImportMap, kLegalImports);

    bool check_qual_syntax = ctx.Config().CheckQualSyntax();
    bool is_operon = (m_Feat->GetData().GetSubtype() == CSeqFeatData::eSubtype_operon);

    ITERATE (CSeq_feat::TQual, it, m_Feat->GetQual()) {
        if (!(*it)->CanGetQual()  ||  !(*it)->CanGetVal()) {
            continue;
        }
        const string& val = (*it)->GetVal();

        const char* name = (*it)->GetQual().c_str();
        const TLegalImportMap::const_iterator li = kLegalImportMap.find(name);
        EFeatureQualifier   slot = eFQ_illegal_qual;
        if ( li != kLegalImportMap.end() ) {
            slot = li->second;
        } else if (check_qual_syntax) {
            continue;
        }

        // operon quals for non-operon feature is obtained from 
        // overlapping operon feature.
        if (!is_operon  &&  slot == eFQ_operon) {
            continue;
        }

        switch (slot) {
        case eFQ_allele:
            // if /allele inherited from gene, suppress allele gbqual on feature
            if (x_HasQual(eFQ_gene_allele)) {
                continue;
            }
        case eFQ_codon:
            if ((*it)->IsSetVal()  &&  !NStr::IsBlank(val)) {
                x_AddQual(slot, new CFlatStringQVal(val));
            }
            break;
        case eFQ_cons_splice:
            if ((*it)->IsSetVal()) {
                if (!check_qual_syntax  ||  s_IsValidnConsSplice(val)) {
                    x_AddQual(slot, new CFlatStringQVal(val));
                }
            }
            break;
        case eFQ_direction:
            if ((*it)->IsSetVal()) {
                if (!check_qual_syntax  ||  s_IsValidDirection(val)) {
                    x_AddQual(slot, new CFlatNumberQVal(val));
                }
            }
            break;
        case eFQ_estimated_length:
        case eFQ_mod_base:
        case eFQ_number:
            if ((*it)->IsSetVal()  &&  !NStr::IsBlank(val)) {
                x_AddQual(slot, new CFlatNumberQVal(val));
            }
            break;
        case eFQ_rpt_type:
            x_AddRptTypeQual(val, check_qual_syntax);
            break;
        case eFQ_rpt_unit:
            if ((*it)->IsSetVal()) {
                x_AddRptUnitQual(val);
            }
            break;
        case eFQ_usedin:
        case eFQ_old_locus_tag:
        {{
            list<string> vals;
            s_ParseParentQual(**it, vals);
            ITERATE (list<string>, i, vals) {
                x_AddQual(slot, new CFlatStringQVal(*i, CFormatQual::eQuoted));
            }
            break;
        }}
        case eFQ_rpt_family:
            if ((*it)->IsSetVal()  &&  !NStr::IsBlank(val)) {
                x_AddQual(slot, new CFlatStringQVal(val));
            }
            break;
        case eFQ_label:
            x_AddQual(slot, new CFlatLabelQVal(val));
            break;
        case eFQ_EC_number:
            if ((*it)->IsSetVal()  &&  s_IsLegalECNumber(val)) {
                x_AddQual(slot, new CFlatStringQVal(val));
            }
            break;
        case eFQ_illegal_qual:
            x_AddQual(slot, new CFlatIllegalQVal(**it));
            break;
        case eFQ_product:
            if (!x_HasQual(eFQ_product)) {
                x_AddQual(slot, new CFlatStringQVal(val));
            } else if (!x_HasQual(eFQ_xtra_prod_quals)) {
                const CFlatStringQVal* gene = x_GetStringQual(eFQ_gene);
                const string& gene_val =
                    gene != NULL ? gene->GetValue() : kEmptyStr;
                const CFlatStringQVal* product = x_GetStringQual(eFQ_product);
                const string& product_val =
                    product != NULL ? product->GetValue() : kEmptyStr;
                if (val != gene_val  &&  val != product_val) {
                    if (m_Feat->GetData().GetSubtype() != CSeqFeatData::eSubtype_tRNA  ||
                        NStr::Find(val, "RNA") == NPOS) {
                        x_AddQual(eFQ_xtra_prod_quals, new CFlatStringQVal(val));
                    }
                }
            }
            break;
        case eFQ_compare:
            {{
                list<string> vals;
                s_ParseParentQual(**it, vals);
                ITERATE (list<string>, i, vals) {
                    if (!ctx.Config().CheckQualSyntax()  ||
                        IsValidAccession(*i, eValidateAccDotVer)) {
                        x_AddQual(slot, new CFlatStringQVal(*i, CFormatQual::eUnquoted));
                    }
                }
            }}
            break;
        case eFQ_evidence:
            {{
                if ( val == "EXPERIMENTAL" ) {
                    x_AddQual(eFQ_experiment, new CFlatExperimentQVal());
                } else if ( val == "NOT_EXPERIMENTAL" ) {
                    x_AddQual(eFQ_inference, new CFlatInferenceQVal());
                }
            }}
            break;

        default:
            x_AddQual(slot, new CFlatStringQVal(val));
            break;
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddRptUnitQual(
    const string& rpt_unit )
//  ----------------------------------------------------------------------------
{
    if (rpt_unit.empty()) {
        return;
    }

    vector<string> units;

    if (NStr::StartsWith(rpt_unit, '(')  &&  NStr::EndsWith(rpt_unit, ')')  &&
        NStr::Find(rpt_unit, "(", 1) == NPOS) {
        string tmp = rpt_unit.substr(1, rpt_unit.length() - 2);
        NStr::Tokenize(tmp, ",", units);
    } else {
        units.push_back(rpt_unit);
    }

    NON_CONST_ITERATE (vector<string>, it, units) {
        if (!it->empty()) {
            NStr::TruncateSpacesInPlace(*it);
            x_AddQual(eFQ_rpt_unit, new CFlatStringQVal(*it));
        }
    }
}


static bool s_IsValidRptType(const string& type)
{
    static const string valid_rpt[] = {
        "direct", "dispersed", "flanking", "inverted", "other",
        "tandem", "terminal"
    };
    typedef CStaticArraySet<string, PNocase> TValidRptTypes;
    DEFINE_STATIC_ARRAY_MAP(TValidRptTypes, valid_types, valid_rpt);

    return valid_types.find(type) != valid_types.end();
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddRptTypeQual(
    const string& rpt_type, 
    bool check_qual_syntax )
//  ----------------------------------------------------------------------------
{
    if (rpt_type.empty()) {
        return;
    }
    
    string value( rpt_type );
    NStr::TruncateSpacesInPlace( value );
    if ( check_qual_syntax && ! s_IsValidRptType( value ) ) {
        return;
    }
    x_AddQual( eFQ_rpt_type, new CFlatStringQVal( value, CFormatQual::eUnquoted ) );
}


void CFeatureItem::x_FormatQuals(CFlatFeature& ff) const
{
    const CFlatFileConfig& cfg = GetContext()->Config();

    if ( cfg.IsFormatFTable() ) {
        ff.SetQuals() = m_FTableQuals;
        return;
    }

    ff.SetQuals().reserve(m_Quals.Size());
    CFlatFeature::TQuals& qvec = ff.SetQuals();

#define DO_QUAL(x) x_FormatQual(eFQ_##x, #x, qvec)
    DO_QUAL(partial);
    DO_QUAL(gene);

    DO_QUAL(locus_tag);
    DO_QUAL(old_locus_tag);
    x_FormatQual(eFQ_gene_syn_refseq, "synonym", qvec);
    if ( ! cfg.GeneSynsToNote() ) {
        DO_QUAL(gene_syn);
    }
    x_FormatQual(eFQ_gene_allele, "allele", qvec);

    DO_QUAL(operon);

    DO_QUAL(ncRNA_class);

    DO_QUAL(product);

    x_FormatQual(eFQ_prot_EC_number, "EC_number", qvec);
    x_FormatQual(eFQ_prot_activity,  "function", qvec);

    DO_QUAL(standard_name);
    DO_QUAL(coded_by);
    DO_QUAL(derived_from);

    x_FormatQual(eFQ_prot_name, "name", qvec);
    DO_QUAL(region_name);
    DO_QUAL(bond_type);
    DO_QUAL(site_type);
    DO_QUAL(sec_str_type);
    DO_QUAL(heterogen);

    // DO_QUAL(tag_peptide); // not until December 15, 2007

    if ( !cfg.GoQualsToNote() ) {
        x_FormatQual(eFQ_go_component, "GO_component", qvec);
        x_FormatQual(eFQ_go_function, "GO_function", qvec);
        x_FormatQual(eFQ_go_process, "GO_process", qvec);
    }

    x_FormatNoteQuals(ff);
        
    DO_QUAL(citation);

    DO_QUAL(number);

    DO_QUAL(pseudo);
    DO_QUAL(selenocysteine);

    DO_QUAL(codon_start);

    DO_QUAL(anticodon);
    DO_QUAL(bound_moiety);
    DO_QUAL(clone);
    DO_QUAL(compare);
    DO_QUAL(cons_splice);
    DO_QUAL(direction);
    DO_QUAL(function);
    DO_QUAL(evidence);
    DO_QUAL(experiment);
    DO_QUAL(inference);
    DO_QUAL(exception);
    DO_QUAL(frequency);
    DO_QUAL(EC_number);
    x_FormatQual(eFQ_gene_map, "map", qvec);
    DO_QUAL(estimated_length);
    DO_QUAL(allele);
    DO_QUAL(map);
    DO_QUAL(mod_base);
    DO_QUAL(PCR_conditions);
    DO_QUAL(phenotype);
    DO_QUAL(ribosomal_slippage);
    DO_QUAL(rpt_family);
    DO_QUAL(rpt_type);
    DO_QUAL(rpt_unit);
    DO_QUAL(rpt_unit_range);
    DO_QUAL(rpt_unit_seq);
    DO_QUAL(trans_splicing);
    DO_QUAL(mobile_element);
    DO_QUAL(usedin);

    // extra imports, actually...
    x_FormatQual(eFQ_illegal_qual, "illegal", qvec);

    DO_QUAL(replace);

    DO_QUAL(transl_except);
    DO_QUAL(transl_table);
    DO_QUAL(codon);
    if ( ! cfg.CodonRecognizedToNote() ) {
        DO_QUAL(trna_codons);
    }
    DO_QUAL(organism);
    DO_QUAL(label);
    x_FormatQual(eFQ_cds_product, "product", qvec);
    DO_QUAL(protein_id);
    DO_QUAL(transcript_id);
    DO_QUAL(db_xref);
    x_FormatQual(eFQ_gene_xref, "db_xref", qvec);
    DO_QUAL(translation);
    DO_QUAL(transcription);
    DO_QUAL(peptide);
#undef DO_QUAL
}

/*
// check if str2 is a sub string of str1
static bool s_IsRedundant(const string& str1, const string& str2)
{
    size_t pos = NPOS;
    bool whole = false;
    for (pos = NStr::Find(str1, str2); pos != NPOS  &&  !whole; pos += str2.length()) {
        whole = IsWholeWord(str1, pos);
    }
    return (pos != NPOS  && whole);
}


// Remove redundant elements that occur twice or as part of other elements.
static void s_PruneNoteQuals(CFlatFeature::TQuals& qvec)
{
    if (qvec.empty()) {
        return;
    }
    CFlatFeature::TQuals::iterator it1 = qvec.begin();
    while (it1 != qvec.end()) {
        CFlatFeature::TQuals::iterator it2 = it1 + 1;
        const string& val1 = (*it1)->GetValue();
        while (it2 != qvec.end()) {
            const string& val2 = (*it2)->GetValue();
            if (s_IsRedundant(val1, val2)) {
                it2 = qvec.erase(it2);
            } else if (s_IsRedundant(val2, val1)) {                
                break;
            } else {
                ++it2;
            }
        }
        if (it2 != qvec.end()) {
            it1 = qvec.erase(it1);
        } else {
            ++it1;
        }
    }
}
*/

void CFeatureItem::x_FormatNoteQuals(CFlatFeature& ff) const
{
    const CFlatFileConfig& cfg = GetContext()->Config();
    CFlatFeature::TQuals qvec;

#define DO_NOTE(x) x_FormatNoteQual(eFQ_##x, #x, qvec)
    x_FormatNoteQual(eFQ_transcript_id_note, "tscpt_id_note", qvec);
    DO_NOTE(gene_desc);

    if ( cfg.GeneSynsToNote() ) {
        DO_NOTE(gene_syn);
    }
    if ( cfg.CodonRecognizedToNote() ) {
        DO_NOTE(trna_codons);
    }
    DO_NOTE(encodes);
    DO_NOTE(prot_desc);
    DO_NOTE(prot_note);
    DO_NOTE(prot_comment);
    DO_NOTE(prot_method);
    DO_NOTE(maploc);
    DO_NOTE(prot_conflict);
    DO_NOTE(prot_missing);
    DO_NOTE(seqfeat_note);
    DO_NOTE(exception_note);
    DO_NOTE(region);
    DO_NOTE(selenocysteine_note);
    DO_NOTE(prot_names);
    DO_NOTE(bond);
    DO_NOTE(site);
    DO_NOTE(rrna_its);
    DO_NOTE(xtra_prod_quals);
    DO_NOTE(modelev);
    DO_NOTE(tag_peptide); // through December 15, 2007
#undef DO_NOTE

    string notestr;
    string suffix = kEmptyStr;
    bool add_period = false;

    s_QualVectorToNote(qvec, true, notestr, suffix, add_period);

    if (GetContext()->Config().GoQualsToNote()) {
        qvec.clear();
        x_FormatNoteQual(eFQ_go_component, "GO_component", qvec);
        x_FormatNoteQual(eFQ_go_function, "GO_function", qvec);
        x_FormatNoteQual(eFQ_go_process, "GO_process", qvec);
        s_QualVectorToNote(qvec, false, notestr, suffix, add_period);
    }
    s_NoteFinalize(add_period, notestr, ff);
}


void CFeatureItem::x_FormatQual
(EFeatureQualifier slot,
 const char* name,
 CFlatFeature::TQuals& qvec,
 IFlatQVal::TFlags flags) const
{
    TQCI it = const_cast<const TQuals&>(m_Quals).LowerBound(slot);
    TQCI end = const_cast<const TQuals&>(m_Quals).end();
    while (it != end  &&  it->first == slot) {
        it->second->Format(qvec, name, *GetContext(), flags);
        ++it;
    }
}


void CFeatureItem::x_FormatNoteQual
(EFeatureQualifier slot,
 const char* name, 
 CFlatFeature::TQuals& qvec,
 IFlatQVal::TFlags flags) const
{
    flags |= IFlatQVal::fIsNote;

    TQCI it = const_cast<const TQuals&>(m_Quals).LowerBound(slot);
    TQCI end = const_cast<const TQuals&>(m_Quals).end();
    while (it != end  &&  it->first == slot) {
        it->second->Format(qvec, name, *GetContext(), flags);
        ++it;
    }
}


const CFlatStringQVal* CFeatureItem::x_GetStringQual(EFeatureQualifier slot) const
{
    const IFlatQVal* qual = 0;
    if ( x_HasQual(slot) ) {
        qual = m_Quals.Find(slot)->second;
    }
    return dynamic_cast<const CFlatStringQVal*>(qual);
}


CFlatStringListQVal* CFeatureItem::x_GetStringListQual(EFeatureQualifier slot) const
{
    IFlatQVal* qual = 0;
    if (x_HasQual(slot)) {
        qual = const_cast<IFlatQVal*>(&*m_Quals.Find(slot)->second);
    }
    return dynamic_cast<CFlatStringListQVal*>(qual);
}

void CFeatureItem::x_CleanQuals(
    const CGene_ref* gene_ref )
{ 
    const TGeneSyn* gene_syn = 
        (gene_ref && gene_ref->IsSetSyn() && !gene_ref->GetSyn().empty() ) 
        ?
        &gene_ref->GetSyn() 
        : 
        0;
    const CBioseqContext& ctx = *GetContext();

    if (ctx.Config().DropIllegalQuals()) {
        x_DropIllegalQuals();
    }

    CFlatStringListQVal* prod_names = x_GetStringListQual(eFQ_prot_names);
    const CFlatStringQVal* gene = x_GetStringQual(eFQ_gene);
    const CFlatStringQVal* prot_desc = x_GetStringQual(eFQ_prot_desc);
    const CFlatStringQVal* standard_name = x_GetStringQual(eFQ_standard_name);

    if (gene != NULL) {
        const string& gene_name = gene->GetValue();

        // /gene same as feature.comment will suppress /note
        if (m_Feat->CanGetComment()) {
            if (NStr::Equal(gene_name, m_Feat->GetComment())) {
                x_RemoveQuals(eFQ_seqfeat_note);
            }
        }

        // remove protein description that equals the gene name, case sensitive
        if (prot_desc != NULL) {
            if (NStr::Equal(gene_name, prot_desc->GetValue())) {
                x_RemoveQuals(eFQ_prot_desc);
                prot_desc = NULL;
            }
        }

        // remove prot name if equals gene
        if (prod_names != NULL) {
            NON_CONST_ITERATE(CFlatStringListQVal::TValue, it, prod_names->SetValue()) {
                if (NStr::Equal(gene_name, *it)) {
                    it = prod_names->SetValue().erase(it);
                    it--;
                }
            }
            if (prod_names->GetValue().empty()) {
                x_RemoveQuals(eFQ_prot_names);
                prod_names = NULL;
            }
        }
    }

    // remove prot name if in prot_desc
    if (prot_desc != NULL) {
        const string& pdesc = prot_desc->GetValue();

        // remove prot name if in prot_desc
        if (prod_names != NULL) {
            NON_CONST_ITERATE(CFlatStringListQVal::TValue, it, prod_names->SetValue()) {
                if (NStr::Find(pdesc, *it) != NPOS) {
                    it = prod_names->SetValue().erase(it);
                    it--;
                }
            }
            if (prod_names->GetValue().empty()) {
                x_RemoveQuals(eFQ_prot_names);
                prod_names = NULL;
            }
        }
        // remove protein description that equals the cds product, case sensitive
        const CFlatStringQVal* cds_prod = x_GetStringQual(eFQ_cds_product);
        if (cds_prod != NULL) {
            if (NStr::Equal(pdesc, cds_prod->GetValue())) {
                x_RemoveQuals(eFQ_prot_desc);
                prot_desc = NULL;
            }
        }

        // remove protein description that equals the standard name
        if (prot_desc != NULL  &&  standard_name != NULL) {
            if (NStr::EqualNocase(pdesc, standard_name->GetValue())) {
                x_RemoveQuals(eFQ_prot_desc);
                prot_desc = NULL;
            }
        }

        // remove protein description that equals a gene synonym
        // NC_001823 leave in prot_desc if no cds_product
        if (prot_desc != NULL  &&  gene_syn != NULL  &&  cds_prod != NULL) {
            ITERATE (TGeneSyn, it, *gene_syn) {
                if (!NStr::IsBlank(*it)  &&  pdesc == *it) {
                    x_RemoveQuals(eFQ_prot_desc);
                    prot_desc = NULL;
                    break;
                }
            }
        }
    }

    // product same as seqfeat_comment will suppress /note
    if (m_Feat->IsSetComment()) {
        const string& feat_comment = m_Feat->GetComment();
        const CFlatStringQVal* product     = x_GetStringQual(eFQ_product);
        const CFlatStringQVal* cds_product = x_GetStringQual(eFQ_cds_product);
        if (product != NULL) {
            if (NStr::Equal(product->GetValue(), feat_comment)) {
                x_RemoveQuals(eFQ_seqfeat_note);
            }
        }
        if (cds_product != NULL) {
            if (NStr::Equal(cds_product->GetValue(), feat_comment)) {
                x_RemoveQuals(eFQ_seqfeat_note);
            }
        }
        // suppress selenocysteine note if already in comment
        if (NStr::Find(feat_comment, "selenocysteine") != NPOS) {
            x_RemoveQuals(eFQ_selenocysteine_note);
        }

        // /EC_number same as feat.comment will suppress /note
        for (TQCI it = x_GetQual(eFQ_EC_number); it != m_Quals.end()  &&  it->first == eFQ_EC_number; ++it) {
            const CFlatStringQVal* ec = dynamic_cast<const CFlatStringQVal*>(it->second.GetPointerOrNull());
            if (ec != NULL) {
                if (NStr::EqualNocase(feat_comment, ec->GetValue())) {
                    x_RemoveQuals(eFQ_seqfeat_note);
                }
            }
        }
    }

    const CFlatStringQVal* note = x_GetStringQual(eFQ_seqfeat_note);
    if (note != NULL  &&  standard_name != NULL) {
        if (NStr::Equal(note->GetValue(), standard_name->GetValue())) {
            x_RemoveQuals(eFQ_seqfeat_note);
            note = NULL;
        }
    }
    if (note != NULL  &&  gene_syn != NULL) {
        ITERATE (TGeneSyn, it, *gene_syn) {
            if (NStr::EqualNocase(note->GetValue(), *it)) {
                x_RemoveQuals(eFQ_seqfeat_note);
                note = NULL;
                break;
            }
        }
    }


    if (prot_desc == NULL) {
//        had_prot_desc = false;
    }
}


typedef pair<EFeatureQualifier, CSeqFeatData::EQualifier> TQualPair;
static const TQualPair sc_GbToFeatQualMap[] = {
    TQualPair(eFQ_none, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_allele, CSeqFeatData::eQual_allele),
    TQualPair(eFQ_anticodon, CSeqFeatData::eQual_anticodon),
    TQualPair(eFQ_bond, CSeqFeatData::eQual_note),
    TQualPair(eFQ_bond_type, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_bound_moiety, CSeqFeatData::eQual_bound_moiety),
    TQualPair(eFQ_cds_product, CSeqFeatData::eQual_product),
    TQualPair(eFQ_citation, CSeqFeatData::eQual_citation),
    TQualPair(eFQ_clone, CSeqFeatData::eQual_clone),
    TQualPair(eFQ_coded_by, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_codon, CSeqFeatData::eQual_codon),
    TQualPair(eFQ_codon_start, CSeqFeatData::eQual_codon_start),
    TQualPair(eFQ_compare, CSeqFeatData::eQual_compare),
    TQualPair(eFQ_cons_splice, CSeqFeatData::eQual_cons_splice),
    TQualPair(eFQ_db_xref, CSeqFeatData::eQual_db_xref),
    TQualPair(eFQ_derived_from, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_direction, CSeqFeatData::eQual_direction),
    TQualPair(eFQ_EC_number, CSeqFeatData::eQual_EC_number),
    TQualPair(eFQ_encodes, CSeqFeatData::eQual_note),
    TQualPair(eFQ_estimated_length, CSeqFeatData::eQual_estimated_length),
    TQualPair(eFQ_experiment, CSeqFeatData::eQual_experiment),
    TQualPair(eFQ_exception, CSeqFeatData::eQual_exception),
    TQualPair(eFQ_exception_note, CSeqFeatData::eQual_note),
    TQualPair(eFQ_figure, CSeqFeatData::eQual_note),
    TQualPair(eFQ_frequency, CSeqFeatData::eQual_frequency),
    TQualPair(eFQ_function, CSeqFeatData::eQual_function),
    TQualPair(eFQ_gene, CSeqFeatData::eQual_gene),
    TQualPair(eFQ_gene_desc, CSeqFeatData::eQual_note),
    TQualPair(eFQ_gene_allele, CSeqFeatData::eQual_allele),
    TQualPair(eFQ_gene_map, CSeqFeatData::eQual_map),
    TQualPair(eFQ_gene_syn, CSeqFeatData::eQual_note),
    TQualPair(eFQ_gene_syn_refseq, CSeqFeatData::eQual_note),
    TQualPair(eFQ_gene_note, CSeqFeatData::eQual_note),
    TQualPair(eFQ_gene_xref, CSeqFeatData::eQual_db_xref),
    TQualPair(eFQ_go_component, CSeqFeatData::eQual_note),
    TQualPair(eFQ_go_function, CSeqFeatData::eQual_note),
    TQualPair(eFQ_go_process, CSeqFeatData::eQual_note),
    TQualPair(eFQ_heterogen, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_illegal_qual, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_inference, CSeqFeatData::eQual_inference),
    TQualPair(eFQ_label, CSeqFeatData::eQual_label),
    TQualPair(eFQ_locus_tag, CSeqFeatData::eQual_locus_tag),
    TQualPair(eFQ_map, CSeqFeatData::eQual_map),
    TQualPair(eFQ_maploc, CSeqFeatData::eQual_note),
    TQualPair(eFQ_mobile_element, CSeqFeatData::eQual_mobile_element),
    TQualPair(eFQ_mod_base, CSeqFeatData::eQual_mod_base),
    TQualPair(eFQ_modelev, CSeqFeatData::eQual_note),
    TQualPair(eFQ_ncRNA_class, CSeqFeatData::eQual_ncRNA_class),
    TQualPair(eFQ_number, CSeqFeatData::eQual_number),
    TQualPair(eFQ_old_locus_tag, CSeqFeatData::eQual_old_locus_tag),
    TQualPair(eFQ_operon, CSeqFeatData::eQual_operon),
    TQualPair(eFQ_organism, CSeqFeatData::eQual_organism),
    TQualPair(eFQ_partial, CSeqFeatData::eQual_partial),
    TQualPair(eFQ_PCR_conditions, CSeqFeatData::eQual_PCR_conditions),
    TQualPair(eFQ_peptide, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_phenotype, CSeqFeatData::eQual_phenotype),
    TQualPair(eFQ_product, CSeqFeatData::eQual_product),
    TQualPair(eFQ_product_quals, CSeqFeatData::eQual_product),
    TQualPair(eFQ_prot_activity, CSeqFeatData::eQual_function),
    TQualPair(eFQ_prot_comment, CSeqFeatData::eQual_note),
    TQualPair(eFQ_prot_EC_number, CSeqFeatData::eQual_EC_number),
    TQualPair(eFQ_prot_note, CSeqFeatData::eQual_note),
    TQualPair(eFQ_prot_method, CSeqFeatData::eQual_note),
    TQualPair(eFQ_prot_conflict, CSeqFeatData::eQual_note),
    TQualPair(eFQ_prot_desc, CSeqFeatData::eQual_note),
    TQualPair(eFQ_prot_missing, CSeqFeatData::eQual_note),
    TQualPair(eFQ_prot_name, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_prot_names, CSeqFeatData::eQual_note),
    TQualPair(eFQ_protein_id, CSeqFeatData::eQual_protein_id),
    TQualPair(eFQ_pseudo, CSeqFeatData::eQual_pseudo),
    TQualPair(eFQ_region, CSeqFeatData::eQual_note),
    TQualPair(eFQ_region_name, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_replace, CSeqFeatData::eQual_replace),
    TQualPair(eFQ_ribosomal_slippage, CSeqFeatData::eQual_ribosomal_slippage),
    TQualPair(eFQ_rpt_family, CSeqFeatData::eQual_rpt_family),
    TQualPair(eFQ_rpt_type, CSeqFeatData::eQual_rpt_type),
    TQualPair(eFQ_rpt_unit, CSeqFeatData::eQual_rpt_unit),
    TQualPair(eFQ_rpt_unit_range, CSeqFeatData::eQual_rpt_unit_range),
    TQualPair(eFQ_rpt_unit_seq, CSeqFeatData::eQual_rpt_unit_seq),
    TQualPair(eFQ_rrna_its, CSeqFeatData::eQual_note),
    TQualPair(eFQ_sec_str_type, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_selenocysteine, CSeqFeatData::eQual_note),
    TQualPair(eFQ_selenocysteine_note, CSeqFeatData::eQual_note),
    TQualPair(eFQ_seqfeat_note, CSeqFeatData::eQual_note),
    TQualPair(eFQ_site, CSeqFeatData::eQual_note),
    TQualPair(eFQ_site_type, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_standard_name, CSeqFeatData::eQual_standard_name),
    TQualPair(eFQ_tag_peptide, CSeqFeatData::eQual_tag_peptide),
    TQualPair(eFQ_trans_splicing, CSeqFeatData::eQual_trans_splicing),
    TQualPair(eFQ_transcription, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_transcript_id, CSeqFeatData::eQual_note),
    TQualPair(eFQ_transcript_id_note, CSeqFeatData::eQual_note),
    TQualPair(eFQ_transl_except, CSeqFeatData::eQual_transl_except),
    TQualPair(eFQ_transl_table, CSeqFeatData::eQual_transl_table),
    TQualPair(eFQ_translation, CSeqFeatData::eQual_translation),
    TQualPair(eFQ_trna_aa, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_trna_codons, CSeqFeatData::eQual_note),
    TQualPair(eFQ_usedin, CSeqFeatData::eQual_usedin),
    TQualPair(eFQ_xtra_prod_quals, CSeqFeatData::eQual_note)
};
typedef CStaticArrayMap<EFeatureQualifier, CSeqFeatData::EQualifier> TQualMap;
DEFINE_STATIC_ARRAY_MAP(TQualMap, sc_QualMap, sc_GbToFeatQualMap);

static CSeqFeatData::EQualifier s_GbToSeqFeatQual(EFeatureQualifier qual)
{
    TQualMap::const_iterator it = sc_QualMap.find(qual);
    if ( it != sc_QualMap.end() ) {
        return it->second;
    }
    return CSeqFeatData::eQual_bad;
}


void CFeatureItem::x_DropIllegalQuals(void) const
{
    const CSeqFeatData& data = m_Feat->GetData();

    TQI it = m_Quals.begin();
    while ( it != m_Quals.end() ) {
        CSeqFeatData::EQualifier qual = s_GbToSeqFeatQual(it->first);
        if ( !data.IsLegalQualifier(qual) ) {
            it = m_Quals.Erase(it);
        } else {
            ++it;
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableQuals(
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    bool pseudo = m_Feat->CanGetPseudo()  &&  m_Feat->GetPseudo();

    const CSeqFeatData& data = m_Feat->GetData();

    switch ( m_Feat->GetData().Which() ) {
    case CSeqFeatData::e_Gene:
        pseudo |= x_AddFTableGeneQuals(data.GetGene());
        break;
    case CSeqFeatData::e_Rna:
        x_AddFTableRnaQuals(*m_Feat, ctx);
        break;
    case CSeqFeatData::e_Cdregion:
        x_AddFTableCdregionQuals(*m_Feat, ctx);
        break;
    case CSeqFeatData::e_Prot:
        x_AddFTableProtQuals(*m_Feat);
        break;
    case CSeqFeatData::e_Region:
        x_AddFTableRegionQuals(data.GetRegion());
        break;
    case CSeqFeatData::e_Bond:
        x_AddFTableBondQuals(data.GetBond());
        break;
    case CSeqFeatData::e_Site:
        x_AddFTableSiteQuals(data.GetSite());
        break;
    case CSeqFeatData::e_Psec_str:
        x_AddFTablePsecStrQuals(data.GetPsec_str());
        break;
    case CSeqFeatData::e_Het:
        x_AddFTablePsecStrQuals(data.GetHet());
        break;
    case CSeqFeatData::e_Biosrc:
        x_AddFTableBiosrcQuals(data.GetBiosrc());
        break;
    default:
        break;
    }
    if ( pseudo ) {
        x_AddFTableQual("pseudo");
    }
    const CGene_ref* grp = m_Feat->GetGeneXref();
    if ( grp != 0  &&  grp->IsSuppressed() ) {
        x_AddFTableQual("gene", "-");
    }
    if ( m_Feat->CanGetComment()  &&  !m_Feat->GetComment().empty() ) {
        x_AddFTableQual("note", m_Feat->GetComment());
    }
    if ( m_Feat->CanGetExp_ev() ) {
        string ev;
        switch ( m_Feat->GetExp_ev() ) {
        case CSeq_feat::eExp_ev_experimental:
            ev = "experimental";
            break;
        case CSeq_feat::eExp_ev_not_experimental:
            ev = "not_experimental";
            break;
        }
        x_AddFTableQual("evidence", ev);
    }
    if ( m_Feat->CanGetExcept_text()  &&  !m_Feat->GetExcept_text().empty() ) {
        string exception_text = m_Feat->GetExcept_text();
        if ( exception_text == "ribosomal slippage" ) {
          x_AddFTableQual("ribosomal_slippage");
        }
        else if ( exception_text == "trans-splicing" ) {
          x_AddFTableQual("trans_splicing");
        }
        x_AddFTableQual("exception", m_Feat->GetExcept_text());
    } else if ( m_Feat->CanGetExcept()  &&  m_Feat->GetExcept() ) {
        x_AddFTableQual("exception");
    }
    ITERATE (CSeq_feat::TQual, it, m_Feat->GetQual()) {
        const CGb_qual& qual = **it;
        const string& key = qual.CanGetQual() ? qual.GetQual() : kEmptyStr;
        const string& val = qual.CanGetVal() ? qual.GetVal() : kEmptyStr;
        if ( !key.empty()  &&  !val.empty() ) {
            x_AddFTableQual(key, val);
        }
    }
    if ( m_Feat->IsSetExt() ) {
        x_AddFTableExtQuals(m_Feat->GetExt());
    }
    if ( data.IsGene() ) {
        x_AddFTableDbxref(data.GetGene().GetDb());
    } else if ( data.IsProt() ) {
        x_AddFTableDbxref(data.GetProt().GetDb());
    }
    x_AddFTableDbxref(m_Feat->GetDbxref());
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableExtQuals(
    const CSeq_feat::TExt& ext )
//  ----------------------------------------------------------------------------
{
    ITERATE (CUser_object::TData, it, ext.GetData()) {
        const CUser_field& field = **it;
        if ( !field.CanGetData() ) {
            continue;
        }
        if ( field.GetData().IsObject() ) {
            const CUser_object& obj = field.GetData().GetObject();
            x_AddQualsExt(obj);
            return;
        } else if ( field.GetData().IsObjects() ) {
            ITERATE (CUser_field::C_Data::TObjects, o, field.GetData().GetObjects()) {
                x_AddQualsExt(**o);
            }
            return;
        }
    }
    if ( ext.CanGetType()  &&  ext.GetType().IsStr() ) {
        const string& oid = ext.GetType().GetStr();
        if ( oid == "GeneOntology" ) {
            ITERATE (CUser_object::TData, uf_it, ext.GetData()) {
                const CUser_field& field = **uf_it;
                if ( field.IsSetLabel()  &&  field.GetLabel().IsStr() ) {
                    const string& label = field.GetLabel().GetStr();
                    string name;
                    if ( label == "Process" ) {
                        name = "GO_process";
                    } else if ( label == "Component" ) {               
                        name = "GO_component";
                    } else if ( label == "Function" ) {
                        name = "GO_function";
                    }
                    if ( name.empty() ) {
                        continue;
                    }
                    
                    ITERATE (CUser_field::TData::TFields, it, field.GetData().GetFields()) {
                        if ( (*it)->GetData().IsFields() ) {
                            CFlatGoQVal(**it).Format(m_FTableQuals, name, *GetContext(), 0);;
                        }
                    }
                }
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableDbxref(
    const CSeq_feat::TDbxref& dbxref )
//  ----------------------------------------------------------------------------
{
    ITERATE (CSeq_feat::TDbxref, it, dbxref) {
        const CDbtag& dbt = **it;
        if ( dbt.CanGetDb()  &&  !dbt.GetDb().empty()  &&
             dbt.CanGetTag() ) {
            const CObject_id& oid = dbt.GetTag();
            switch ( oid.Which() ) {
            case CObject_id::e_Str:
                if ( !oid.GetStr().empty() ) {
                    x_AddFTableQual("db_xref", dbt.GetDb() + ":" + oid.GetStr());
                }
                break;
            case CObject_id::e_Id:
                x_AddFTableQual("db_xref", dbt.GetDb() + ":" + NStr::IntToString(oid.GetId()));
                break;
            default:
                break;
            }
        }
    }
}

//  ----------------------------------------------------------------------------
bool CFeatureItem::x_AddFTableGeneQuals(
    const CGene_ref& gene )
//  ----------------------------------------------------------------------------
{
    if ( gene.CanGetLocus()  &&  !gene.GetLocus().empty() ) {
        x_AddFTableQual("gene", gene.GetLocus());
    }
    ITERATE (CGene_ref::TSyn, it, gene.GetSyn()) {
        x_AddFTableQual("gene_syn", *it);
    }
    if ( gene.CanGetDesc()  &&  !gene.GetDesc().empty() ) {
        x_AddFTableQual("gene_desc", gene.GetDesc());
    }
    if ( gene.CanGetMaploc()  &&  !gene.GetMaploc().empty() ) {
        x_AddFTableQual("map", gene.GetMaploc());
    }
    if ( gene.CanGetLocus_tag()  &&  !gene.GetLocus_tag().empty() ) {
        x_AddFTableQual("locus_tag", gene.GetLocus_tag());
    }

    return (gene.CanGetPseudo()  &&  gene.GetPseudo());
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableRnaQuals(
    const CSeq_feat& feat, 
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    string label;

    if ( !feat.GetData().IsRna() ) {
        return;
    }
    const CSeqFeatData::TRna& rna = feat.GetData().GetRna();
    if (rna.CanGetExt()) {
        const CRNA_ref::TExt& ext = rna.GetExt();
        if (ext.IsName()) {
            if (!ext.GetName().empty()) {
                x_AddFTableQual("product", ext.GetName());
            }
        } else if (ext.IsTRNA()) {
            feature::GetLabel(feat, &label, feature::eContent, &ctx.GetScope());
            x_AddFTableQual("product", label);
        }
    }

    if ( feat.CanGetProduct() ) {
        CBioseq_Handle prod = 
            ctx.GetScope().GetBioseqHandle(m_Feat->GetProduct());
        if ( prod ) {
            CConstRef<CSeq_id> id = GetId(prod, eGetId_Best).GetSeqId();
            string id_str;
            if ( id->IsGenbank()  ||  id->IsEmbl()  ||  id->IsDdbj()  ||
                 id->IsTpg()  ||  id->IsTpd()  ||  id->IsTpe()  ||
                 id->IsOther() ||
                 (id->IsLocal()  &&  !ctx.Config().SuppressLocalId()) ) {
                id_str = id->GetSeqIdString(true);
            } else if ( id->IsGeneral() ) {
                id_str = id->AsFastaString();
            }
            x_AddFTableQual("transcript_id", id_str);
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableCdregionQuals(
    const CSeq_feat& feat, 
    CBioseqContext& ctx )
//  ----------------------------------------------------------------------------
{
    CBioseq_Handle prod;
    if ( feat.CanGetProduct() ) {
        prod = ctx.GetScope().GetBioseqHandle(feat.GetProduct());
    }
    if ( prod ) {
        CConstRef<CSeq_feat> prot = GetBestOverlappingFeat(
            feat.GetProduct(),
            CSeqFeatData::e_Prot,
            eOverlap_Simple,
            ctx.GetScope());
        if ( prot ) {
            x_AddFTableProtQuals(*prot);
        }
    }
    const CCdregion& cdr = feat.GetData().GetCdregion();
    if ( cdr.CanGetFrame()  &&  cdr.GetFrame() > CCdregion::eFrame_one ) {
        x_AddFTableQual("codon_start", NStr::IntToString(cdr.GetFrame()));
    }
    ITERATE (CCdregion::TCode_break, it, cdr.GetCode_break()) {
        string pos = CFlatSeqLoc((*it)->GetLoc(), ctx).GetString();
        string aa  = "OTHER";
        switch ((*it)->GetAa().Which()) {
        case CCode_break::C_Aa::e_Ncbieaa:
            aa = GetAAName((*it)->GetAa().GetNcbieaa(), true);
            break;
        case CCode_break::C_Aa::e_Ncbi8aa:
            aa = GetAAName((*it)->GetAa().GetNcbi8aa(), false);
            break;
        case CCode_break::C_Aa::e_Ncbistdaa:
            aa = GetAAName((*it)->GetAa().GetNcbistdaa(), false);
            break;
        default:
            break;
        }
        x_AddFTableQual("transl_except", "(pos:" + pos + ",aa:" + aa + ")");
    }
    CConstRef<CSeq_id> id;
    string id_str;
    if ( prod ) {
        id = GetId(prod, eGetId_Best).GetSeqId();
    } else if ( feat.CanGetProduct() ) {
        try { 
            id.Reset(&GetId(feat.GetProduct(), &ctx.GetScope()));
            if ( id->IsGi() ) {
                // get "normal" id 
            }
        } catch (CObjmgrUtilException&) {
            id.Reset();
        }
    }
    if ( id ) {
        if ( id->IsGenbank()  ||  id->IsEmbl()  ||  id->IsDdbj()  ||
             id->IsTpg()  ||  id->IsTpd()  ||  id->IsTpe()  ||
             id->IsOther() ||
             (id->IsLocal()  &&  !ctx.Config().SuppressLocalId()) ) {
            id_str = id->GetSeqIdString(true);
        } else if ( id->IsGi() ) {
            id_str = id->AsFastaString();
        }
        x_AddFTableQual("protein_id", id_str);
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableProtQuals(
    const CSeq_feat& prot )
//  ----------------------------------------------------------------------------
{
    if ( !prot.GetData().IsProt() ) {
        return;
    }
    const CProt_ref& pref = prot.GetData().GetProt();
    ITERATE (CProt_ref::TName, it, pref.GetName()) {
        if ( !it->empty() ) {
            x_AddFTableQual("product", *it);
        }
    }
    if ( pref.CanGetDesc()  &&  !pref.GetDesc().empty() ) {
        x_AddFTableQual("prot_desc", pref.GetDesc());
    }
    ITERATE (CProt_ref::TActivity, it, pref.GetActivity()) {
        if ( !it->empty() ) {
            x_AddFTableQual("function", *it);
        }
    }
    ITERATE (CProt_ref::TEc, it, pref.GetEc()) {
        if ( !it->empty() ) {
            x_AddFTableQual("EC_number", *it);
        }
    }
    if ( prot.CanGetComment()  &&  !prot.GetComment().empty() ) {
        x_AddFTableQual("prot_note", prot.GetComment());
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableRegionQuals(
    const CSeqFeatData::TRegion& region )
//  ----------------------------------------------------------------------------
{
    if ( !region.empty() ) {
        x_AddFTableQual("region", region);
    }
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableBondQuals(
    const CSeqFeatData::TBond& bond )
//  ----------------------------------------------------------------------------
{
    x_AddFTableQual("bond_type", s_GetBondName(bond));
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableSiteQuals(
    const CSeqFeatData::TSite& site)
//  ----------------------------------------------------------------------------
{
    x_AddFTableQual("site_type", s_GetSiteName(site));
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTablePsecStrQuals(
    const CSeqFeatData::TPsec_str& psec_str )
//  ----------------------------------------------------------------------------
{
    const string& psec = CSeqFeatData::ENUM_METHOD_NAME(EPsec_str)()->FindName(
        psec_str, true );
    x_AddFTableQual("sec_str_type", psec);
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTablePsecStrQuals(
    const CSeqFeatData::THet& het)
//  ----------------------------------------------------------------------------
{
    if ( !het.Get().empty() ) {
        x_AddFTableQual("heterogen", het.Get());
    }
}


static const string s_GetSubtypeString(const COrgMod::TSubtype& subtype)
{
    switch ( subtype ) {
        case COrgMod::eSubtype_strain:           return "strain";
        case COrgMod::eSubtype_substrain:        return "substrain";
        case COrgMod::eSubtype_type:             return "type";
        case COrgMod::eSubtype_subtype:          return "subtype";
        case COrgMod::eSubtype_variety:          return "variety";
        case COrgMod::eSubtype_serotype:         return "serotype";
        case COrgMod::eSubtype_serogroup:        return "serogroup";
        case COrgMod::eSubtype_serovar:          return "serovar";
        case COrgMod::eSubtype_cultivar:         return "cultivar";
        case COrgMod::eSubtype_pathovar:         return "pathovar";
        case COrgMod::eSubtype_chemovar:         return "chemovar";
        case COrgMod::eSubtype_biovar:           return "biovar";
        case COrgMod::eSubtype_biotype:          return "biotype";
        case COrgMod::eSubtype_group:            return "group";
        case COrgMod::eSubtype_subgroup:         return "subgroup";
        case COrgMod::eSubtype_isolate:          return "isolate";
        case COrgMod::eSubtype_common:           return "common";
        case COrgMod::eSubtype_acronym:          return "acronym";
        case COrgMod::eSubtype_dosage:           return "dosage";
        case COrgMod::eSubtype_nat_host:         return "nat_host";
        case COrgMod::eSubtype_sub_species:      return "sub_species";
        case COrgMod::eSubtype_specimen_voucher: return "specimen_voucher";
        case COrgMod::eSubtype_authority:        return "authority";
        case COrgMod::eSubtype_forma:            return "forma";
        case COrgMod::eSubtype_forma_specialis:  return "dosage";
        case COrgMod::eSubtype_ecotype:          return "ecotype";
        case COrgMod::eSubtype_synonym:          return "synonym";
        case COrgMod::eSubtype_anamorph:         return "anamorph";
        case COrgMod::eSubtype_teleomorph:       return "teleomorph";
        case COrgMod::eSubtype_breed:            return "breed";
        case COrgMod::eSubtype_gb_acronym:       return "gb_acronym";
        case COrgMod::eSubtype_gb_anamorph:      return "gb_anamorph";
        case COrgMod::eSubtype_gb_synonym:       return "gb_synonym";
        case COrgMod::eSubtype_old_lineage:      return "old_lineage";
        case COrgMod::eSubtype_old_name:         return "old_name";
        case COrgMod::eSubtype_other:            return "note";
        default:                                 return kEmptyStr;
    }
    return kEmptyStr;
}


static const string s_GetSubsourceString(const CSubSource::TSubtype& subtype)
{
    switch ( subtype ) {
        case CSubSource::eSubtype_chromosome: return "chromosome";
        case CSubSource::eSubtype_map: return "map";
        case CSubSource::eSubtype_clone: return "clone";
        case CSubSource::eSubtype_subclone: return "subclone";
        case CSubSource::eSubtype_haplotype: return "haplotype";
        case CSubSource::eSubtype_genotype: return "genotype";
        case CSubSource::eSubtype_sex: return "sex";
        case CSubSource::eSubtype_cell_line: return "cell_line";
        case CSubSource::eSubtype_cell_type: return "cell_type";
        case CSubSource::eSubtype_tissue_type: return "tissue_type";
        case CSubSource::eSubtype_clone_lib: return "clone_lib";
        case CSubSource::eSubtype_dev_stage: return "dev_stage";
        case CSubSource::eSubtype_frequency: return "frequency";
        case CSubSource::eSubtype_germline: return "germline";
        case CSubSource::eSubtype_rearranged: return "rearranged";
        case CSubSource::eSubtype_lab_host: return "lab_host";
        case CSubSource::eSubtype_pop_variant: return "pop_variant";
        case CSubSource::eSubtype_tissue_lib: return "tissue_lib";
        case CSubSource::eSubtype_plasmid_name: return "plasmid_name";
        case CSubSource::eSubtype_transposon_name: return "transposon_name";
        case CSubSource::eSubtype_insertion_seq_name: return "insertion_seq_name";
        case CSubSource::eSubtype_plastid_name: return "plastid_name";
        case CSubSource::eSubtype_country: return "country";
        case CSubSource::eSubtype_segment: return "segment";
        case CSubSource::eSubtype_endogenous_virus_name: return "endogenous_virus_name";
        case CSubSource::eSubtype_transgenic: return "transgenic";
        case CSubSource::eSubtype_environmental_sample: return "environmental_sample";
        case CSubSource::eSubtype_isolation_source: return "isolation_source";
        case CSubSource::eSubtype_other: return "note";
        default: return kEmptyStr;
    }
    return kEmptyStr;
}

//  ----------------------------------------------------------------------------
void CFeatureItem::x_AddFTableBiosrcQuals(
    const CBioSource& src )
//  ----------------------------------------------------------------------------
{
    if ( src.CanGetOrg() ) {
        const CBioSource::TOrg& org = src.GetOrg();

        if ( org.CanGetTaxname()  &&  !org.GetTaxname().empty() ) {
            x_AddFTableQual("organism", org.GetTaxname());
        }

        if ( org.CanGetOrgname() ) {
            ITERATE (COrgName::TMod, it, org.GetOrgname().GetMod()) {
                if ( (*it)->CanGetSubtype() ) {
                    string str = s_GetSubtypeString((*it)->GetSubtype());
                    if ( str.empty() ) {
                        continue;
                    }
                    if ( (*it)->CanGetSubname()  &&  !(*it)->GetSubname().empty() ) {
                        str += (*it)->GetSubname();
                    }
                    x_AddFTableQual(str);
                }
            }
        }
    }

    ITERATE (CBioSource::TSubtype, it, src.GetSubtype()) {
        if ( (*it)->CanGetSubtype() ) {
            string str = s_GetSubsourceString((*it)->GetSubtype());
            if ( str.empty() ) {
                continue;
            }
            if ( (*it)->CanGetName() ) {
                str += (*it)->GetName();
            }
            x_AddFTableQual(str);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//   Source Feature
/////////////////////////////////////////////////////////////////////////////

CSourceFeatureItem::CSourceFeatureItem
(const CSeq_feat& feat,
 CBioseqContext& ctx,
 const CSeq_loc* loc)
    : CFeatureItemBase(feat, ctx, loc),
      m_WasDesc(false), m_IsFocus(false), m_IsSynthetic(false)
{
    x_GatherInfo(ctx);
}


CSourceFeatureItem::CSourceFeatureItem
(const CMappedFeat& feat,
 CBioseqContext& ctx,
 const CSeq_loc* loc)
    : CFeatureItemBase(feat.GetOriginalFeature(), ctx, loc ? loc : &feat.GetLocation()),
      m_WasDesc(false), m_IsFocus(false), m_IsSynthetic(false)
{
    x_GatherInfo(ctx);
}


void CSourceFeatureItem::x_GatherInfo(CBioseqContext& ctx)
{
    const CBioSource& bsrc = GetSource();
    if (!bsrc.IsSetOrg()) {
        m_Feat.Reset();
        x_SetSkip();
        return;
    }

    m_IsFocus = bsrc.IsSetIs_focus();
    if (bsrc.GetOrigin() == CBioSource::eOrigin_synthetic) {
        m_IsSynthetic = true;
    }
    if (!m_IsSynthetic  &&  bsrc.GetOrg().IsSetOrgname()) {
        m_IsSynthetic = bsrc.GetOrg().GetOrgname().IsSetDiv()  &&
            NStr::EqualNocase(bsrc.GetOrg().GetOrgname().GetDiv(), "SYN");
    }
    if (!m_IsSynthetic  &&  bsrc.CanGetOrg() && bsrc.GetOrg().CanGetTaxname()) {
        if (NStr::EqualNocase(bsrc.GetOrg().GetTaxname(), "synthetic construct")) {
            m_IsSynthetic = true;
        }
    }
    x_AddQuals(ctx);
}


void CSourceFeatureItem::x_AddQuals(CBioseqContext& ctx)
{
    const CSeqFeatData& data = m_Feat->GetData();
    _ASSERT(data.IsOrg()  ||  data.IsBiosrc());
    // add various generic qualifiers...
    x_AddQual(eSQ_mol_type,
              new CFlatMolTypeQVal(ctx.GetBiomol(), ctx.GetMol()));
    if (m_Feat->IsSetComment()) {
        x_AddQual(eSQ_seqfeat_note, new CFlatStringQVal(m_Feat->GetComment()));
    }
    if (m_Feat->IsSetTitle()) {
        x_AddQual(eSQ_label, new CFlatLabelQVal(m_Feat->GetTitle()));
    }
    if (m_Feat->IsSetCit()) {
        x_AddQual(eSQ_citation, new CFlatPubSetQVal(m_Feat->GetCit()));
    }
    if (m_Feat->IsSetDbxref()) {
        x_AddQual(eSQ_org_xref, new CFlatXrefQVal(m_Feat->GetDbxref()));
    }
    // add qualifiers from biosource fields
    x_AddQuals(data.GetBiosrc(), ctx);
}


static ESourceQualifier s_OrgModToSlot(const COrgMod& om)
{
    switch ( om.GetSubtype() ) {
#define CASE_ORGMOD(x) case COrgMod::eSubtype_##x:  return eSQ_##x;
        CASE_ORGMOD(strain);
        CASE_ORGMOD(substrain);
        CASE_ORGMOD(type);
        CASE_ORGMOD(subtype);
        CASE_ORGMOD(variety);
        CASE_ORGMOD(serotype);
        CASE_ORGMOD(serogroup);
        CASE_ORGMOD(serovar);
        CASE_ORGMOD(cultivar);
        CASE_ORGMOD(pathovar);
        CASE_ORGMOD(chemovar);
        CASE_ORGMOD(biovar);
        CASE_ORGMOD(biotype);
        CASE_ORGMOD(group);
        CASE_ORGMOD(subgroup);
        CASE_ORGMOD(isolate);
        CASE_ORGMOD(common);
        CASE_ORGMOD(acronym);
        CASE_ORGMOD(dosage);
    case COrgMod::eSubtype_nat_host:  return eSQ_spec_or_nat_host;
        CASE_ORGMOD(sub_species);
        CASE_ORGMOD(specimen_voucher);
        CASE_ORGMOD(authority);
        CASE_ORGMOD(forma);
        CASE_ORGMOD(forma_specialis);
        CASE_ORGMOD(ecotype);
        CASE_ORGMOD(synonym);
        CASE_ORGMOD(anamorph);
        CASE_ORGMOD(teleomorph);
        CASE_ORGMOD(breed);
        CASE_ORGMOD(gb_acronym);
        CASE_ORGMOD(gb_anamorph);
        CASE_ORGMOD(gb_synonym);
        CASE_ORGMOD(old_lineage);
        CASE_ORGMOD(old_name);
#undef CASE_ORGMOD
    case COrgMod::eSubtype_other:  return eSQ_orgmod_note;
    default:                       return eSQ_none;
    }
}


void CSourceFeatureItem::x_AddQuals(const COrg_ref& org, CBioseqContext& ctx) const
{
    string taxname, common;
    if ( org.IsSetTaxname() ) {
        taxname = org.GetTaxname();
    }
    if ( taxname.empty()  &&  ctx.Config().NeedOrganismQual() ) {
        taxname = "unknown";
        if ( org.IsSetCommon() ) {
            common = org.GetCommon();
        }
    }
    if ( !taxname.empty() ) {
        x_AddQual(eSQ_organism, new CFlatStringQVal(taxname));
    }
    if ( !common.empty() ) {
        x_AddQual(eSQ_common_name, new CFlatStringQVal(common));
    }
    if ( org.CanGetOrgname() ) {
        ITERATE (COrgName::TMod, it, org.GetOrgname().GetMod()) {
            ESourceQualifier slot = s_OrgModToSlot(**it);
            if (slot != eSQ_none) {
                x_AddQual(slot, new CFlatOrgModQVal(**it));
            }
        }
    }
    if (!WasDesc()  &&  org.IsSetMod()) {
        x_AddQual(eSQ_unstructured, new CFlatStringListQVal(org.GetMod()));
    }
    if ( org.IsSetDb() ) {
        x_AddQual(eSQ_db_xref, new CFlatXrefQVal(org.GetDb()));
    }
}


static ESourceQualifier s_SubSourceToSlot(const CSubSource& ss)
{
    switch (ss.GetSubtype()) {
#define DO_SS(x) case CSubSource::eSubtype_##x:  return eSQ_##x;
        DO_SS(chromosome);
        DO_SS(map);
        DO_SS(clone);
        DO_SS(subclone);
        DO_SS(haplotype);
        DO_SS(genotype);
        DO_SS(sex);
        DO_SS(cell_line);
        DO_SS(cell_type);
        DO_SS(tissue_type);
        DO_SS(clone_lib);
        DO_SS(dev_stage);
        DO_SS(frequency);
        DO_SS(germline);
        DO_SS(rearranged);
        DO_SS(lab_host);
        DO_SS(pop_variant);
        DO_SS(tissue_lib);
        DO_SS(plasmid_name);
        DO_SS(transposon_name);
        DO_SS(insertion_seq_name);
        DO_SS(plastid_name);
        DO_SS(country);
        DO_SS(segment);
        DO_SS(endogenous_virus_name);
        DO_SS(transgenic);
        DO_SS(environmental_sample);
        DO_SS(isolation_source);
        DO_SS(lat_lon);
        DO_SS(collection_date);
        DO_SS(collected_by);
        DO_SS(identified_by);
        DO_SS(fwd_primer_seq);
        DO_SS(rev_primer_seq);
        DO_SS(fwd_primer_name);
        DO_SS(rev_primer_name);
#undef DO_SS
    case CSubSource::eSubtype_other:  return eSQ_subsource_note;
    default:                          return eSQ_none;
    }
}


void CSourceFeatureItem::x_AddQuals(const CBioSource& src, CBioseqContext& ctx) const
{
    // add qualifiers from Org_ref field
    if ( src.CanGetOrg() ) {
        x_AddQuals(src.GetOrg(), ctx);
    }
    x_AddQual(eSQ_focus, new CFlatBoolQVal(src.IsSetIs_focus()));

    
    bool insertion_seq_name = false,
         plasmid_name = false,
         transposon_name = false;
    string primer_fwd_seq, 
        primer_fwd_name, 
        primer_rev_seq, 
        primer_rev_name;

    ITERATE (CBioSource::TSubtype, it, src.GetSubtype()) {
        ESourceQualifier slot = s_SubSourceToSlot(**it);

        switch( slot ) {

        case eSQ_insertion_seq_name:
            insertion_seq_name = true;
            x_AddQual(slot, new CFlatSubSourceQVal(**it));
            break;

        case eSQ_plasmid_name:
            plasmid_name = true;
            x_AddQual(slot, new CFlatSubSourceQVal(**it));
            break;

        case eSQ_transposon_name:
            transposon_name = true;
            x_AddQual(slot, new CFlatSubSourceQVal(**it));
            break;

        case eSQ_fwd_primer_seq:
            primer_fwd_seq = (**it).GetName();
            break;

        case eSQ_fwd_primer_name:
            primer_fwd_name = (**it).GetName();
            break;

        case eSQ_rev_primer_seq:
            primer_rev_seq = (**it).GetName();
            break;

        case eSQ_rev_primer_name:
            primer_rev_name = (**it).GetName();
            break;

        default:
            if (slot != eSQ_none) {
                x_AddQual(slot, new CFlatSubSourceQVal(**it));
            }
            break;
        }
    }

    //  ------------------------------------------------------------------------
    //  PCR primer rules:
    //
    //  In general, primer_fwd_name, primer_fwd_seq, primer_rev_name, and
    //  primer_rev_seq are combined into a single PCR_primer qualifier in the
    //  flat file. However, for this to happen, some criteria on presence of
    //  certain fields and their cardinalities have to be met.
    //
    //  If those criteria are not met, then there will not be a PCR_primer
    //  qualifier, and the existing *_name and *_seq are instead appended to
    //  the feature /note.
    //  ------------------------------------------------------------------------
    if ( s_QualifiersMeetPrimerReqs( 
        primer_fwd_name, primer_fwd_seq, primer_rev_name, primer_rev_seq ) ) 
    {
        x_AddQual( eSQ_pcr_primer, new CFlatSubSourcePrimer( 
            primer_fwd_name, primer_fwd_seq, primer_rev_name, primer_rev_seq ) );
    } 
    else 
    {
        //
        //  Alert:
        //  We have potentially four constituents all of which go into /notes.
        //  /notes is composed of substrings that are separated by semicolons.
        //  The problem is that the PCR_primer pieces are supposed to be a single
        //  atomic substring of /notes, internally using commas to separate
        //  sub components.
        //  The easy way out is to compose the entire PCR-primer subnote all at
        //  once, and then stuff that string into a single slot for the /note
        //  formatter to pick up.
        //  The following combines any of /fwd_primer_name, /fwd_primer_seq,
        //  /rev/primer_name, /rev_primer_seq into a /pcr_primer_note qualifier.
        //
        string primer_value = s_MakePcrPrimerNote( 
            primer_fwd_name, primer_fwd_seq, primer_rev_name, primer_rev_seq );
        if ( !primer_value.empty() ) {
            x_AddQual( eSQ_pcr_primer_note, new CFlatStringQVal( primer_value ) );
        }
     }
    //< end of special PCR_primer handling

    // some qualifiers are flags in genome and names in subsource,
    // print once with name
    CBioSource::TGenome genome = src.GetGenome();
    CRef<CFlatOrganelleQVal> organelle(new CFlatOrganelleQVal(genome));
    if ( (insertion_seq_name  &&  genome == CBioSource::eGenome_insertion_seq)  ||
         (plasmid_name  &&  genome == CBioSource::eGenome_plasmid)  ||
         (transposon_name  &&  genome == CBioSource::eGenome_transposon) ) {
        organelle.Reset();
    }
    if ( organelle ) {
        x_AddQual(eSQ_organelle, organelle);
    }

    if ( !WasDesc()  &&  m_Feat->CanGetComment() ) {
        x_AddQual(eSQ_seqfeat_note, new CFlatStringQVal(m_Feat->GetComment()));
    }
}


void CSourceFeatureItem::x_FormatQuals(CFlatFeature& ff) const
{
    ff.SetQuals().reserve(m_Quals.Size());
    CFlatFeature::TQuals& qvec = ff.SetQuals();

#define DO_QUAL(x) x_FormatQual(eSQ_##x, #x, qvec)
    DO_QUAL(organism);

    DO_QUAL(organelle);

    DO_QUAL(mol_type);

    DO_QUAL(strain);
    x_FormatQual(eSQ_substrain, "sub_strain", qvec);
    DO_QUAL(variety);
    DO_QUAL(serotype);
    DO_QUAL(serovar);
    DO_QUAL(cultivar);
    DO_QUAL(isolate);
    DO_QUAL(isolation_source);
    x_FormatQual(eSQ_spec_or_nat_host, "host", qvec);
    DO_QUAL(sub_species);
    DO_QUAL(specimen_voucher);

    DO_QUAL(db_xref);
    x_FormatQual(eSQ_org_xref, "db_xref", qvec);

    DO_QUAL(chromosome);

    DO_QUAL(segment);

    DO_QUAL(map);
    DO_QUAL(clone);
    x_FormatQual(eSQ_subclone, "sub_clone", qvec);
    DO_QUAL(haplotype);
    DO_QUAL(sex);
    DO_QUAL(cell_line);
    DO_QUAL(cell_type);
    DO_QUAL(tissue_type);
    DO_QUAL(clone_lib);
    DO_QUAL(dev_stage);
    DO_QUAL(ecotype);
    DO_QUAL(frequency);

    DO_QUAL(germline);
    DO_QUAL(rearranged);
    DO_QUAL(transgenic);
    DO_QUAL(environmental_sample);

    DO_QUAL(lab_host);
    DO_QUAL(pop_variant);
    DO_QUAL(tissue_lib);

    x_FormatQual(eSQ_plasmid_name, "plasmid", qvec);
    x_FormatQual(eSQ_mobile_element, "mobile_element", qvec);

    DO_QUAL(country);

    DO_QUAL(focus);
    DO_QUAL(lat_lon);
    DO_QUAL(identified_by);
    DO_QUAL(pcr_primer);
    DO_QUAL(collection_date);
    DO_QUAL(collected_by);

    if ( !GetContext()->Config().SrcQualsToNote() ) {
        // some note qualifiers appear as regular quals in GBench or Dump mode
        x_FormatGBNoteQuals(ff);
    }

    DO_QUAL(sequenced_mol);
    DO_QUAL(label);
    DO_QUAL(usedin);
    DO_QUAL(citation);
#undef DO_QUAL

    // Format the rest of the note quals (ones that weren't formatted above)
    // as a single note qualifier
    x_FormatNoteQuals(ff);
}


void CSourceFeatureItem::x_FormatGBNoteQuals(CFlatFeature& ff) const
{
    _ASSERT(!GetContext()->Config().SrcQualsToNote());
    CFlatFeature::TQuals& qvec = ff.SetQuals();

#define DO_QUAL(x) x_FormatQual(eSQ_##x, #x, qvec)
    DO_QUAL(type);
    DO_QUAL(subtype);
    DO_QUAL(serogroup);
    DO_QUAL(pathovar);
    DO_QUAL(chemovar);
    DO_QUAL(biovar);
    DO_QUAL(biotype);
    DO_QUAL(group);
    DO_QUAL(subgroup);
    DO_QUAL(common);
    DO_QUAL(acronym);
    DO_QUAL(dosage);
    
    DO_QUAL(authority);
    DO_QUAL(forma);
    DO_QUAL(forma_specialis);
    DO_QUAL(synonym);
    DO_QUAL(anamorph);
    DO_QUAL(teleomorph);
    DO_QUAL(breed);

//    DO_QUAL(collection_date);
//    DO_QUAL(collected_by);
//    DO_QUAL(identified_by);
//    DO_QUAL(pcr_primer);
    DO_QUAL(genotype);
    x_FormatQual(eSQ_plastid_name, "plastid", qvec);
    
    x_FormatQual(eSQ_endogenous_virus_name, "endogenous_virus", qvec);

    x_FormatQual(eSQ_zero_orgmod, "?", qvec);
    x_FormatQual(eSQ_one_orgmod,  "?", qvec);
    x_FormatQual(eSQ_zero_subsrc, "?", qvec);
#undef DO_QUAL
}


void CSourceFeatureItem::x_FormatNoteQuals(CFlatFeature& ff) const
{
    CFlatFeature::TQuals qvec;
    bool add_period = false;

#define DO_NOTE(x) x_FormatNoteQual(eSQ_##x, #x, qvec)
    if (m_WasDesc) {
        x_FormatNoteQual(eSQ_seqfeat_note, "note", qvec);
        DO_NOTE(orgmod_note);
        DO_NOTE(subsource_note);
    } else {
        DO_NOTE(unstructured);
    }

    if ( GetContext()->Config().SrcQualsToNote() ) {
        DO_NOTE(type);
        DO_NOTE(subtype);
        DO_NOTE(serogroup);
        DO_NOTE(pathovar);
        DO_NOTE(chemovar);
        DO_NOTE(biovar);
        DO_NOTE(biotype);
        DO_NOTE(group);
        DO_NOTE(subgroup);
        DO_NOTE(common);
        DO_NOTE(acronym);
        DO_NOTE(dosage);
        
        DO_NOTE(authority);
        DO_NOTE(forma);
        DO_NOTE(forma_specialis);
        DO_NOTE(synonym);
        DO_NOTE(anamorph);
        DO_NOTE(teleomorph);
        DO_NOTE(breed);
        
        DO_NOTE(genotype);
        x_FormatNoteQual(eSQ_plastid_name, "plastid", qvec);
        
        x_FormatNoteQual(eSQ_endogenous_virus_name, "endogenous_virus", qvec);
    }
    DO_NOTE(pcr_primer_note);

    if (!m_WasDesc) {
        x_FormatNoteQual(eSQ_seqfeat_note, "note", qvec);
        DO_NOTE(orgmod_note);
        DO_NOTE(subsource_note);
    }

    x_FormatNoteQual(eSQ_common_name, "common", qvec);

    if ( GetContext()->Config().SrcQualsToNote() ) {
        x_FormatNoteQual(eSQ_zero_orgmod, "?", qvec);
        x_FormatNoteQual(eSQ_one_orgmod,  "?", qvec);
        x_FormatNoteQual(eSQ_zero_subsrc, "?", qvec);
    }
#undef DO_NOTE

    string notestr;
    string suffix = kEmptyStr;

    if ( GetSource().CanGetGenome()  &&  
        GetSource().GetGenome() == CBioSource::eGenome_extrachrom ) {
        static const string kEOL = "\n";
        notestr += "extrachromosomal";
        suffix = kEOL;
    }

    s_QualVectorToNote(qvec, true, notestr, suffix, add_period);
    s_NoteFinalize(add_period, notestr, ff, eTilde_newline);
}


CSourceFeatureItem::CSourceFeatureItem
(const CBioSource& src,
 TRange range,
 CBioseqContext& ctx)
    : CFeatureItemBase(*new CSeq_feat, ctx),
      m_WasDesc(true)
{
    if (!src.IsSetOrg()) {
        m_Feat.Reset();
        x_SetSkip();
        return;
    }
    x_SetObject(src);

    CSeq_feat& feat = const_cast<CSeq_feat&>(*m_Feat);
    feat.SetData().SetBiosrc(const_cast<CBioSource&>(src));
    if ( range.IsWhole() ) {
        feat.SetLocation().SetWhole(*ctx.GetPrimaryId());
    } else {
        CSeq_interval& ival = feat.SetLocation().SetInt();
        ival.SetFrom(range.GetFrom());
        ival.SetTo(range.GetTo());
        ival.SetId(*ctx.GetPrimaryId());
    }

    x_GatherInfo(ctx);
}


void CSourceFeatureItem::x_FormatQual
(ESourceQualifier slot,
 const string& name,
 CFlatFeature::TQuals& qvec,
 IFlatQVal::TFlags flags) const
{
    TQCI it = const_cast<const TQuals&>(m_Quals).LowerBound(slot);
    TQCI end = const_cast<const TQuals&>(m_Quals).end();
    while (it != end  &&  it->first == slot) {
        const IFlatQVal* qual = it->second;
        qual->Format(qvec, name, *GetContext(),
                     flags | IFlatQVal::fIsSource);
        ++it;
    }
}


void CSourceFeatureItem::Subtract(const CSourceFeatureItem& other, CScope &scope)
{
    m_Loc = Seq_loc_Subtract(GetLoc(), other.GetLoc(), 0, &scope);
}


void CSourceFeatureItem::SetLoc(const CSeq_loc& loc)
{
    m_Loc.Reset(&loc);
}


END_SCOPE(objects)
END_NCBI_SCOPE
