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
*
* File Description:
*   new (early 2003) flat-file generator -- representation of features
*   (mainly of interest to implementors)
*
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
#include <objmgr/seq_loc_mapper.hpp>
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
           id.IsTpg()      ||  id.IsTpe()     ||  id.IsTpd();
}


static bool s_CheckQuals_cdregion(const CSeq_feat& feat, CBioseqContext& ctx)
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
            CConstRef<CSeq_feat> gene = GetBestGeneForCds(feat, scope);
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
        const CSeq_loc& loc = feat.GetLocation();
        if ( loc.IsPartialLeft()  &&  !loc.IsPartialRight() ) {
            if ( GetLength(loc, &scope) <= 5 ) {
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


static bool s_CheckMandatoryQuals(const CSeq_feat& feat, CBioseqContext& ctx)
{
    switch ( feat.GetData().GetSubtype() ) {
    case CSeqFeatData::eSubtype_cdregion:
        {
            return s_CheckQuals_cdregion(feat, ctx);
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


static bool s_SkipFeature(const CSeq_feat& feat, CBioseqContext& ctx)
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

    // supress full length comment features
    if ( type == CSeqFeatData::e_Comment ) {
        ECompare comp = Compare(ctx.GetLocation(), feat.GetLocation(), &ctx.GetScope());
        if ( comp == eContained  ||  comp == eSame ) {
            return true;
        }
    }

    // if RELEASE mode, make sure we have all info to create mandatory quals.
    if ( cfg.NeedRequiredQuals() ) {
        return !s_CheckMandatoryQuals(feat, ctx);
    }

    return false;
}


static bool s_IsLegalECNumber(const string& ec_number)
{
    ITERATE (string, it, ec_number) {
        if (!isdigit(*it)  &&  *it != '.'  &&  *it != '-') {
            return false;
        }
    }

    return true;
}


typedef pair<CSeqFeatData::TBond, string>   TBondPair;
static const TBondPair sc_bond_list[] = {
    TBondPair(CSeqFeatData::eBond_disulfide,  "disulfide"),
    TBondPair(CSeqFeatData::eBond_thiolester, "thiolester"),
    TBondPair(CSeqFeatData::eBond_xlink,      "xlink"),
    TBondPair(CSeqFeatData::eBond_thioether,  "thioether"),
    TBondPair(CSeqFeatData::eBond_other,      "unclassified")
    
};
typedef CStaticArrayMap<CSeqFeatData::TBond, string> TBondList;
static const TBondList sc_BondList(sc_bond_list, sizeof(sc_bond_list));


static const string s_SiteList[] = {
    "",
    "active",
    "binding",
    "cleavage",
    "inhibit",
    "modified",
    "glycosylation",
    "myristoylation",
    "mutagenized",
    "metal-binding",
    "phosphorylation",
    "acetylation",
    "amidation",
    "methylation",
    "hydroxylation",
    "sulfatation",
    "oxidative-deamination",
    "pyrrolidone-carboxylic-acid",
    "gamma-carboxyglutamic-acid",
    "blocked",
    "lipid-binding",
    "np-binding",
    "DNA binding",
    "signal-peptide",
    "transit-peptide",
    "transmembrane-region",
    "unclassified"
};


static const string s_PsecStrText[] = {
    "",
    "helix",
    "sheet",
    "turn"
};


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


// -- FeatureItem



// constructor
CFeatureItemBase::CFeatureItemBase
(const CSeq_feat& feat,
 CBioseqContext& ctx,
 const CSeq_loc* loc) :
    CFlatItem(&ctx), m_Feat(&feat), m_Loc(loc != 0 ? loc : &feat.GetLocation())
{
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
        for ( CSeq_loc_CI it(loc); it; ++it ) {
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
            default:
                break;
            }
        }
    }

    return false;
}


/////////////////////////////////////////////////////////////////////////////
//  CFeatureItem

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
        if ((subtype == CSeqFeatData::eSubtype_preprotein  &&  !ctx.IsRefSeq())  ||
            subtype == CSeqFeatData::eSubtype_site                               ||
            subtype == CSeqFeatData::eSubtype_bond                               ||
            subtype == CSeqFeatData::eSubtype_region                             ||
            subtype == CSeqFeatData::eSubtype_comment) {
            return "misc_feature";
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
    if ( s_SkipFeature(GetFeat(), ctx) ) {
        x_SetSkip();
        return;
    }
    m_Type = m_Feat->GetData().GetSubtype();
    x_AddQuals(ctx);
}


void CFeatureItem::x_AddQuals(CBioseqContext& ctx)
{
    if ( ctx.Config().IsFormatFTable() ) {
        x_AddFTableQuals(ctx);
        return;
    }

    CScope&             scope = ctx.GetScope();
    const CSeqFeatData& data  = m_Feat->GetData();
    const CSeq_loc&     loc   = GetLoc();
    CSeqFeatData::E_Choice type = data.Which();
    CSeqFeatData::ESubtype subtype = data.GetSubtype();

    bool pseudo = m_Feat->CanGetPseudo() ? m_Feat->GetPseudo() : false;
    bool had_prot_desc = false;
    string precursor_comment;
    const TGeneSyn* gene_syn = NULL;

    // add various common qualifiers...

    // partial
    if ( !(IsMappedFromCDNA()  &&  ctx.IsProt())  &&
         !ctx.Config().HideUnclassPartial() ) {
        if ( m_Feat->CanGetPartial()  &&  m_Feat->GetPartial() ) {
            if ( !s_LocIsFuzz(*m_Feat, loc) ) {
                int partial = SeqLocPartialCheck(m_Feat->GetLocation(), &scope);
                if ( partial == eSeqlocPartial_Complete ) {
                    x_AddQual(eFQ_partial, new CFlatBoolQVal(true));
                }
            }
        }
    }
    
    // dbxref
    if (m_Feat->IsSetDbxref()) {
        x_AddQual(eFQ_db_xref, new CFlatXrefQVal(m_Feat->GetDbxref(), &m_Quals));
    }
    // model_evidance and go quals
    if ( m_Feat->IsSetExt() ) {
        x_AddExtQuals(m_Feat->GetExt());
    }
    // evidence
    if (m_Feat->IsSetExp_ev()) {
        x_AddQual(eFQ_evidence, new CFlatExpEvQVal(m_Feat->GetExp_ev()));
    }
    // citation
    if (m_Feat->IsSetCit()) {
        x_AddQual(eFQ_citation, new CFlatPubSetQVal(m_Feat->GetCit()));
    }
    // exception
    x_AddExceptionQuals(ctx);

    if (!data.IsGene()  &&
        (subtype != CSeqFeatData::eSubtype_operon)  &&
        (subtype != CSeqFeatData::eSubtype_gap)) {

        const CGene_ref* grp = m_Feat->GetGeneXref();
        CConstRef<CSeq_feat> overlap_gene;

        if (grp != NULL  &&  grp->CanGetDb()) {
            x_AddQual(eFQ_gene_xref, new CFlatXrefQVal(grp->GetDb()));
        }
        if (grp == NULL) {
            if (ctx.IsProt()  ||  !IsMapped()) {
                overlap_gene = GetBestOverlappingFeat(
                    *m_Feat,
                    CSeqFeatData::e_Gene,
                    sequence::eOverlap_Contains,
                    scope);
            } else {
                overlap_gene = GetOverlappingGene(GetLoc(), scope);
            }
            if (overlap_gene) {
                if (overlap_gene->CanGetComment()) {
                    x_AddQual(eFQ_gene_note,
                        new CFlatStringQVal(overlap_gene->GetComment()));
                }
                if ( overlap_gene->CanGetPseudo()  &&  overlap_gene->GetPseudo() ) {
                    pseudo = true;
                }
                grp = &overlap_gene->GetData().GetGene();
                if ( grp != NULL  &&  grp->IsSetDb() ) {
                    x_AddQual(eFQ_gene_xref, new CFlatXrefQVal(grp->GetDb()));
                } else if ( overlap_gene->IsSetDbxref() ) {
                    x_AddQual(eFQ_gene_xref, new CFlatXrefQVal(overlap_gene->GetDbxref()));
                }
            }
        }
        if (grp != NULL) {
            if (grp->CanGetPseudo()  &&  grp->GetPseudo() ) {
                pseudo = true;
            }
            if (!grp->IsSuppressed()) {
                gene_syn = x_AddQuals(*grp, pseudo, subtype, overlap_gene.NotEmpty());
            }
        }
        if ( type != CSeqFeatData::e_Cdregion  &&  type !=  CSeqFeatData::e_Rna ) {
            x_RemoveQuals(eFQ_gene_xref);
        }
    }

    // operon qual for any feature but operon and gap
    if (subtype != CSeqFeatData::eSubtype_operon  &&
        subtype != CSeqFeatData::eSubtype_gap) {
        const CGene_ref* grp = m_Feat->GetGeneXref();
        if (grp == NULL  ||  !grp->IsSuppressed()) {
            const CSeq_loc& operon_loc = (ctx.IsProt()  ||  !IsMapped()) ? 
                m_Feat->GetLocation() : GetLoc();
            CConstRef<CSeq_feat> operon = GetOverlappingOperon(operon_loc, scope);
            if (operon) {
                const string& operon_name = operon->GetNamedQual("operon");
                if (!operon_name.empty()) {
                    x_AddQual(eFQ_operon, new CFlatStringQVal(operon_name));
                }
            }
        }
    }

    

    // specific fields set here...
    switch ( type ) {
    case CSeqFeatData::e_Gene:
        gene_syn = x_AddGeneQuals(*m_Feat, pseudo);
        break;
    case CSeqFeatData::e_Cdregion:
        x_AddCdregionQuals(*m_Feat, ctx, pseudo, had_prot_desc);
        break;
    case CSeqFeatData::e_Rna:
        x_AddRnaQuals(*m_Feat, ctx, pseudo);
        break;
    case CSeqFeatData::e_Prot:
        x_AddProtQuals(*m_Feat, ctx, pseudo, had_prot_desc, precursor_comment);
        break;
    case CSeqFeatData::e_Region:
        x_AddRegionQuals(*m_Feat, ctx);
        break;
    case CSeqFeatData::e_Site:
        x_AddSiteQuals(*m_Feat, ctx);
        break;
    case CSeqFeatData::e_Bond:
        x_AddBondQuals(*m_Feat, ctx);
        break;
    // !!! case CSeqFeatData::e_XXX:
    default:
        break;
    }
    
    // pseudo
    if (pseudo) {
        x_AddQual(eFQ_pseudo, new CFlatBoolQVal(true));
    }
    // comment
    CRef<CFlatStringQVal> seqfeat_note;
    bool add_period = false;
    if (m_Feat->IsSetComment()) {
        string comment = m_Feat->GetComment();
        TrimSpacesAndJunkFromEnds(comment, true);
        add_period = RemovePeriodFromEnd(comment, true);

        if (precursor_comment != comment) {
            seqfeat_note.Reset(new CFlatStringQVal(comment));
            x_AddQual(eFQ_seqfeat_note, seqfeat_note);
        }
    }

    // now go through gbqual list
    if (m_Feat->IsSetQual()) {
        x_ImportQuals(ctx);
    }

    // cleanup (drop illegal quals, duplicate information etc.)
    x_CleanQuals(had_prot_desc, gene_syn);

    if (seqfeat_note) {
        if (add_period  &&  !had_prot_desc) {
            seqfeat_note->SetAddPeriod();
        }
    }
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


void CFeatureItem::x_AddRnaQuals
(const CSeq_feat& feat,
 CBioseqContext& ctx,
 bool& pseudo) const
{
    const CRNA_ref& rna = feat.GetData().GetRna();
    const CFlatFileConfig& cfg = ctx.Config();

    if ( rna.CanGetPseudo()  &&  rna.GetPseudo() ) {
        pseudo = true;
    }

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
                    x_AddQual(eFQ_trna_codons, new CFlatTrnaCodonsQVal(trna));
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
        try {
            if ( feat.CanGetProduct() ) {
                const CSeq_id& id = GetId(feat.GetProduct(), &ctx.GetScope());
                CBioseq_Handle prod = 
                    ctx.GetScope().GetBioseqHandleFromTSE(id, ctx.GetHandle());
                EFeatureQualifier slot = 
                    (ctx.IsRefSeq()  ||  cfg.IsModeDump()  ||  cfg.IsModeGBench()) ?
                    eFQ_transcript_id : eFQ_transcript_id_note;
                if ( prod ) {
                    x_AddProductIdQuals(prod, slot);
                } else {
                    x_AddQual(slot, new CFlatSeqIdQVal(id));
                    if ( id.IsGi() ) {
                        x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(id, true));
                    }
                }
            }
        } catch (CObjmgrUtilException&) {}
        if ( !pseudo  &&  cfg.ShowTranscript() ) {
            CSeqVector vec(feat.GetLocation(), ctx.GetScope());
            vec.SetCoding(CBioseq_Handle::eCoding_Iupac);
            string transcription;
            vec.GetSeqData(0, vec.size(), transcription);
            x_AddQual(eFQ_transcription, new CFlatStringQVal(transcription));
        }
        // intentional fall through
    }
    default:
        if ( rna.CanGetExt()  &&  rna.GetExt().IsName() ) {
            x_AddQual(eFQ_product, new CFlatStringQVal(rna.GetExt().GetName()));
        }
        break;
    } // end of switch
}


const CFeatureItem::TGeneSyn* CFeatureItem::x_AddGeneQuals
(const CSeq_feat& gene,
 bool &pseudo) const
{
    return x_AddQuals(gene.GetData().GetGene(), pseudo, CSeqFeatData::eSubtype_gene, false);
}


void CFeatureItem::x_AddCdregionQuals
(const CSeq_feat& cds,
 CBioseqContext& ctx,
 bool& pseudo, bool& had_prot_desc) const
{
    CScope& scope = ctx.GetScope();
    const CFlatFileConfig& cfg = ctx.Config();
    
    x_AddQuals(cds.GetData().GetCdregion());
    if ( ctx.IsProt()  &&  IsMappedFromCDNA() ) {
        x_AddQual(eFQ_coded_by, new CFlatSeqLocQVal(m_Feat->GetLocation()));
    } else {
        const CProt_ref* pref = 0;
        // protein qualifiers
        if (m_Feat->IsSetProduct()) {
            const CSeq_id* prot_id = 0;
            // protein id
            try {
                prot_id = &GetId(m_Feat->GetProduct(), &scope);
            } catch (CException&) {
                prot_id = 0;
            }

            CBioseq_Handle prot;
            if (prot_id != NULL) {
                if (!cfg.AlwaysTranslateCDS()) {
                    // by default only show /translation if product bioseq is within
                    // entity, but flag can override and force far /translation
                    if (cfg.ShowFarTranslations()) {
                        prot =  scope.GetBioseqHandle(*prot_id);
                    } else {
                        prot = scope.GetBioseqHandleFromTSE(*prot_id, ctx.GetHandle());
                    }
                }
            }

            
            if (prot) {
                // get the "best" id for the protein
                prot_id = &GetId(prot, eGetId_Best);
                // Add protein quals (comment, note, names ...) 
                pref = x_AddProteinQuals(prot);
            } else {
                x_AddQual(eFQ_protein_id, new CFlatSeqIdQVal(*prot_id));
                if ( prot_id->IsGi() ) {
                    x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(*prot_id, true));
                }
            }

            // translation
            if (!pseudo) {
                string translation;

                if ((!prot  &&  cfg.TranslateIfNoProduct())  ||  cfg.AlwaysTranslateCDS()) {
                    CCdregion_translate::TranslateCdregion(translation, *m_Feat, scope);
                } else if (prot) {
                    CSeqVector seqv = prot.GetSeqVector();
                    CSeq_data::E_Choice coding = cfg.IupacaaOnly() ?
                        CSeq_data::e_Iupacaa : CSeq_data::e_Ncbieaa;
                    seqv.SetCoding(coding);
                    seqv.GetSeqData(0, seqv.size(), translation);
                }

                if (!NStr::IsBlank(translation)) {
                    x_AddQual(eFQ_translation, new CFlatStringQVal(translation));
                } else {
                    // !!! release mode error
                }
            }
        }

        // protein xref overrides names, but should not prevent /protein_id, etc.
        const CProt_ref* p = m_Feat->GetProtXref();
        if (p != NULL) {
            pref = p;
        }
        if (pref != NULL) {
            if (!pref->GetName().empty()) {
                CProt_ref::TName names = pref->GetName();
                x_AddQual(eFQ_cds_product, new CFlatStringQVal(names.front()));
                names.pop_front();
                if (!names.empty()) {
                    x_AddQual(eFQ_prot_names, new CFlatStringListQVal(names));
                }
            }
            if ( pref->CanGetDesc() ) {
                string desc = pref->GetDesc();
                TrimSpacesAndJunkFromEnds(desc);
                bool add_period = RemovePeriodFromEnd(desc, true);
                CRef<CFlatStringQVal> prot_desc(new CFlatStringQVal(desc));
                if (add_period) {
                    prot_desc->SetAddPeriod();
                }
                x_AddQual(eFQ_prot_desc, prot_desc);
                had_prot_desc = true;
            }
            if ( !pref->GetActivity().empty() ) {
                ITERATE (CProt_ref::TActivity, it, pref->GetActivity()) {
                    x_AddQual(eFQ_prot_activity, new CFlatStringQVal(*it));
                }
            }
            if ( !pref->GetEc().empty() ) {
                ITERATE(CProt_ref::TEc, ec, pref->GetEc()) {
                    if (!cfg.DropIllegalQuals()  ||  s_IsLegalECNumber(*ec)) {
                        x_AddQual(eFQ_prot_EC_number, new CFlatStringQVal(*ec));
                    }
                }
            }
        }
    }
}


const CProt_ref* CFeatureItem::x_AddProteinQuals(CBioseq_Handle& prot) const
{
    _ASSERT(prot);

    x_AddProductIdQuals(prot, eFQ_protein_id);

    CSeqdesc_CI comm(prot, CSeqdesc::e_Comment, 1);
    if ( comm  &&  !comm->GetComment().empty() ) {
        x_AddQual(eFQ_prot_comment, new CFlatStringQVal(comm->GetComment()));
    }

    CSeqdesc_CI mi(prot, CSeqdesc::e_Molinfo);
    if ( mi ) {
        CMolInfo::TTech prot_tech = mi->GetMolinfo().GetTech();
        if ( prot_tech >  CMolInfo::eTech_standard       &&
             prot_tech != CMolInfo::eTech_concept_trans  &&
             prot_tech != CMolInfo::eTech_concept_trans_a ) {
            if ( !GetTechString(prot_tech).empty() ) {
                x_AddQual(eFQ_prot_method, 
                    new CFlatStringQVal("Method: " + GetTechString(prot_tech)));
            }
        }
    }

    const CProt_ref* pref = 0;
    CFeat_CI prot_feat(prot, SAnnotSelector(CSeqFeatData::e_Prot));
    if ( prot_feat ) {
        pref = &(prot_feat->GetData().GetProt());
        if ( prot_feat->IsSetComment() ) {
            if ( pref->GetProcessed() == CProt_ref::eProcessed_not_set  ||
                 pref->GetProcessed() == CProt_ref::eProcessed_preprotein ) {
                string prot_note = prot_feat->GetComment();
                TrimSpacesAndJunkFromEnds(prot_note, true);
                RemovePeriodFromEnd(prot_note, true);
                x_AddQual(eFQ_prot_note, new CFlatStringQVal(prot_note));
            }
        }
    }

    /*
    bool maploc = false, fig = false;
    for ( CSeqdesc_CI it(prot, CSeqdesc::e_Pub); it; ++it ) {
        const CPubdesc& pub = it->GetPub();
        if ( !maploc  &&  pub.CanGetMaploc() ) {
            string mapstr = "Map location " + pub.GetMaploc();
            RemovePeriodFromEnd(mapstr);
            x_AddQual(eFQ_maploc, new CFlatStringQVal(mapstr));
            maploc = true;
        }
        if ( !fig  &&  pub.CanGetFig() ) {
            string figstr = "This sequence comes from " + pub.GetFig();
            RemovePeriodFromEnd(figstr);
            x_AddQual(eFQ_figure, new CFlatStringQVal(figstr));
            fig = true;
        }
    }
    */
    return pref;
}


static const string sc_ValidExceptionText[] = {
  "RNA editing",
  "reasons given in citation"
};
static const CStaticArraySet<string> sc_LegatExceptText(
    sc_ValidExceptionText, sizeof(sc_ValidExceptionText));

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
static const CStaticArraySet<string> sc_LegalRefSeqExceptText(
    sc_ValidRefSeqExceptionText, sizeof(sc_ValidRefSeqExceptionText));

static bool s_IsValidRefSeqExceptionText(const string& text)
{
    return sc_LegalRefSeqExceptText.find(text) != sc_LegalRefSeqExceptText.end();
}


static void s_ParseException
(const string& original,
 string& except,
 string& note,
 CBioseqContext& ctx)
{
    if ( original.empty() ) {
        return;
    }

    except.erase();
    note.erase();

    if ( !ctx.Config().DropIllegalQuals() ) {
        except = original;
        return;
    }

    list<string> l;
    NStr::Split(original, ",", l);
    NON_CONST_ITERATE (list<string>, it, l) {
        NStr::TruncateSpacesInPlace(*it);
    }

    list<string> except_list, note_list;

    ITERATE (list<string>, it, l) {
        if (s_IsValidExceptionText(*it)) {            
            except_list.push_back(*it);
        } else if (ctx.IsRefSeq()  &&  s_IsValidRefSeqExceptionText(*it)) {
            except_list.push_back(*it);
        } else {
            note_list.push_back(*it);
        }
    }

    if (!except_list.empty()) {
        except = NStr::Join(except_list, ", ");
    }
    if (!note_list.empty()) {
        note = NStr::Join(note_list, ", ");
    }
}


void CFeatureItem::x_AddExceptionQuals(CBioseqContext& ctx) const
{
    string except_text, note_text;
    const CFlatFileConfig& cfg = ctx.Config();

    if ( m_Feat->CanGetExcept_text()  &&  !m_Feat->GetExcept_text().empty() ) {
        except_text = m_Feat->GetExcept_text();
    }
    
    // /exception currently legal only on cdregion
    if ( m_Feat->GetData().IsCdregion()  ||  !cfg.DropIllegalQuals() ) {
        // exception flag is set, but no exception text supplied
        if ( except_text.empty()  &&
             m_Feat->CanGetExcept()  &&  m_Feat->GetExcept() ) {
            // if no /exception text, use text in comment, remove from /note
            if ( x_HasQual(eFQ_seqfeat_note) ) {
                const CFlatStringQVal* qval = 
                    dynamic_cast<const CFlatStringQVal*>(x_GetQual(eFQ_seqfeat_note).first->second.GetPointerOrNull());
                if ( qval != 0 ) {
                    const string& seqfeat_note = qval->GetValue();
                    if ( !cfg.DropIllegalQuals()  ||
                        s_IsValidExceptionText(seqfeat_note) ) {
                        except_text = seqfeat_note;
                        x_RemoveQuals(eFQ_seqfeat_note);
                    }
                }
            } else {
                except_text = "No explanation supplied";
            }

            // if DropIllegalQuals is set, check CDS list here as well
            if ( cfg.DropIllegalQuals()  &&
                 !s_IsValidExceptionText(except_text) ) {
                except_text.erase();
            }
        }
        
        if ( cfg.DropIllegalQuals() ) {
            string except = except_text;
            s_ParseException(except, except_text, note_text, ctx);
        }
    } else if ( !except_text.empty() ) {
        note_text = except_text;
        except_text.erase();
    }

    if ( !except_text.empty() ) {
        x_AddQual(eFQ_exception, new CFlatStringQVal(except_text));
    }
    if ( !note_text.empty() ) {
        x_AddQual(eFQ_exception_note, new CFlatStringQVal(note_text));
    }
}


static int s_ScoreSeqIdHandle(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    CRef<CSeq_id> id_non_const
        (const_cast<CSeq_id*>(id.GetPointer()));
    return CSeq_id::Score(id_non_const);
}


void CFeatureItem::x_AddProductIdQuals(CBioseq_Handle& prod, EFeatureQualifier slot) const
{
    if (!prod) {
        return;
    }

    const CBioseq_Handle::TId& ids = prod.GetId();
    if (ids.empty()) {
        return;
    }

    // the product id (transcript or protein) is set to the best id
    CSeq_id_Handle best = FindBestChoice(ids, s_ScoreSeqIdHandle);
    if (best) {
        switch (best.Which()) {
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
        case CSeq_id::e_Gi:
        case CSeq_id::e_Other:
        case CSeq_id::e_General:
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            // these are the only types we allow as product ids
            break;
        default:
            best.Reset();
        }
    }
    if (!best) {
        return;
    }
    x_AddQual(slot, new CFlatSeqIdQVal(*best.GetSeqId()));
    
    ITERATE (CBioseq_Handle::TId, it, ids) {
        CSeq_id::E_Choice choice = it->Which();
        if (choice != CSeq_id::e_Genbank  &&
            choice != CSeq_id::e_Embl  &&
            choice != CSeq_id::e_Ddbj  &&
            choice != CSeq_id::e_Gi  &&
            choice != CSeq_id::e_Other  &&
            choice != CSeq_id::e_General  &&
            choice != CSeq_id::e_Tpg  &&
            choice != CSeq_id::e_Tpe  &&
            choice != CSeq_id::e_Tpd ) {
            continue;
        }
        if (*it == best) {
            // we've already done 'best'. 
            continue;
        }
        
        const CSeq_id& id = *(it->GetSeqId());
        if (!id.IsGeneral()) {
            x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(id, id.IsGi()));
        }

        /*const CSeq_id& id = *(it->GetSeqId());
        if (id.IsGeneral()  &&  ids.size() == 1) {
            const CDbtag& dbt = it->GetSeqId()->GetGeneral();
            if (dbt.GetType() != CDbtag::eDbtagType_PID) {
                x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(id, id.IsGi()));
            }
        } else {
            x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(id, id.IsGi()));
        }*/
    }
}


void CFeatureItem::x_AddRegionQuals(const CSeq_feat& feat, CBioseqContext& ctx) const
{
    const string& region = feat.GetData().GetRegion();
    if ( region.empty() ) {
        return;
    }

    if ( ctx.IsProt()  &&
         feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_region ) {
        x_AddQual(eFQ_region_name, new CFlatStringQVal(region));
    } else {
        x_AddQual(eFQ_region, new CFlatStringQVal("Region: " + region));
    }
}


void CFeatureItem::x_AddBondQuals(const CSeq_feat& feat, CBioseqContext& ctx) const
{
    CSeqFeatData::TBond bond = feat.GetData().GetBond();
    TBondList::const_iterator it = sc_BondList.find(bond);
    if (it != sc_BondList.end()) {
        if (ctx.IsGenbankFormat()  &&  ctx.IsProt()) {
            x_AddQual(eFQ_bond_type, new CFlatStringQVal(it->second));
        } else {
            x_AddQual(eFQ_bond, new CFlatBondQVal(it->second));
        }
    }
}


void CFeatureItem::x_AddSiteQuals(const CSeq_feat& feat, CBioseqContext& ctx) const
{
    CSeqFeatData::TSite site = feat.GetData().GetSite();

    string site_name;
    if (site == CSeqFeatData::eSite_other) {
        site_name = "unclassified";
    } else if (site == CSeqFeatData::eSite_dna_binding) {
        site_name = "DNA binding";
    } else {
        site_name = CSeqFeatData::ENUM_METHOD_NAME(ESite)()->FindName(site, true);
    }

    if (ctx.Config().IsFormatGenbank()  &&  ctx.IsProt()) {
        x_AddQual(eFQ_site_type, new CFlatSiteQVal(site_name));
    } else {
        if (!feat.IsSetComment()  ||  
            (NStr::Find(feat.GetComment(), site_name) == NPOS)) {
            x_AddQual(eFQ_site, new CFlatSiteQVal(site_name));
        }
    }
}


void CFeatureItem::x_AddExtQuals(const CSeq_feat::TExt& ext) const
{
    ITERATE (CUser_object::TData, it, ext.GetData()) {
        const CUser_field& field = **it;
        if ( !field.CanGetData() ) {
            continue;
        }
        if ( field.GetData().IsObject() ) {
            const CUser_object& obj = field.GetData().GetObject();
            x_AddExtQuals(obj);
            return;
        } else if ( field.GetData().IsObjects() ) {
            ITERATE (CUser_field::C_Data::TObjects, o, field.GetData().GetObjects()) {
                x_AddExtQuals(**o);
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


void CFeatureItem::x_AddGoQuals(const CUser_object& uo) const
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


const CFeatureItem::TGeneSyn* CFeatureItem::x_AddQuals
(const CGene_ref& gene,
 bool& pseudo,
 CSeqFeatData::ESubtype subtype,
 bool from_overlap) const
{
    bool gene_feat = (subtype == CSeqFeatData::eSubtype_gene);

    const string* locus = (gene.IsSetLocus()  &&  !NStr::IsBlank(gene.GetLocus())) ?
        &gene.GetLocus() : NULL;
    const string* desc = (gene.IsSetDesc() &&  !NStr::IsBlank(gene.GetDesc())) ?
        &gene.GetDesc() : NULL;
    const TGeneSyn* syn = (gene.IsSetSyn()  &&  !gene.GetSyn().empty()) ?
        &gene.GetSyn() : NULL;
    const string* locus_tag =
        (gene.IsSetLocus_tag()  &&  !NStr::IsBlank(gene.GetLocus_tag())) ?
        &gene.GetLocus_tag() : 0;

    if (!from_overlap  ||  subtype != CSeqFeatData::eSubtype_repeat_region) {
        if (locus != NULL) {
            x_AddQual(eFQ_gene, new CFlatGeneQVal(*locus));
            if (locus_tag != NULL) {
                x_AddQual(eFQ_locus_tag, new CFlatStringQVal(*locus_tag));
            }
            if (gene_feat  &&  desc != NULL) {
                x_AddQual(eFQ_gene_desc, new CFlatStringQVal(*desc));
            }
            if (gene_feat  &&  syn != NULL) {
                x_AddQual(eFQ_gene_syn, new CFlatGeneSynonymsQVal(*syn));
            }
        } else if (locus_tag != NULL) {
            x_AddQual(eFQ_locus_tag, new CFlatStringQVal(*locus_tag));
            if (gene_feat  &&  desc != NULL) {
                x_AddQual(eFQ_gene_desc, new CFlatStringQVal(*desc));
            }
            if (gene_feat  &&  syn != NULL) {
                x_AddQual(eFQ_gene_syn, new CFlatGeneSynonymsQVal(*syn));
            }
        } else if (desc != NULL) {
            x_AddQual(eFQ_gene, new CFlatGeneQVal(*desc));
            if (gene_feat  &&  syn != NULL) {
                x_AddQual(eFQ_gene_syn, new CFlatGeneSynonymsQVal(*syn));
            }
        } else if (syn != NULL) {
            // add the first as the gene name ...
            CGene_ref::TSyn syns = *syn;
            x_AddQual(eFQ_gene, new CFlatGeneQVal(syns.front()));
            syns.pop_front();
            // ... and the rest as synonyms
            if (gene_feat  &&  !syns.empty() ) {
                x_AddQual(eFQ_gene_syn, new CFlatGeneSynonymsQVal(syns));
            }
        }
    }

    // /allele
    if (subtype != CSeqFeatData::eSubtype_variation) {
        if (gene.IsSetAllele()  &&  !NStr::IsBlank(gene.GetAllele())) {
            x_AddQual(eFQ_gene_allele, new CFlatStringQVal(gene.GetAllele()));
        }
    }

    if (gene.IsSetDb()) {
        x_AddQual(eFQ_gene_xref, new CFlatXrefQVal(gene.GetDb()));
    }

    if (!from_overlap  &&  gene.IsSetMaploc()) {
        x_AddQual(eFQ_gene_map, new CFlatStringQVal(gene.GetMaploc()));
    }

    pseudo = (pseudo  ||  (gene.GetPseudo()));
    return syn;
}


void CFeatureItem::x_AddQuals(const CCdregion& cds) const
{
    CBioseqContext& ctx = *GetContext();
    CScope& scope = ctx.GetScope();

    CCdregion::TFrame frame = cds.GetFrame();

    // code break
    if ( cds.IsSetCode_break() ) {
        // set selenocysteine quals
        ITERATE (CCdregion::TCode_break, it, cds.GetCode_break()) {
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
                    x_AddQual(eFQ_selenocysteine_note,
                        new CFlatStringQVal("selenocysteine"));
                } else {
                    x_AddQual(eFQ_selenocysteine, new CFlatBoolQVal(true));
                }
                break;
            }
        }
    }

    // translation table
    if ( cds.CanGetCode() ) {
        int gcode = cds.GetCode().GetId();
        if ( gcode > 0  &&  gcode != 255 ) {
            // show code 1 only in GBSeq format.
            if ( ctx.Config().IsFormatGBSeq()  ||  gcode > 1 ) {
                x_AddQual(eFQ_transl_table, new CFlatIntQVal(gcode));
            }
        }
    }

    if ( !(ctx.IsProt()  && IsMappedFromCDNA()) ) {
        // frame
        if ( frame == CCdregion::eFrame_not_set ) {
            frame = CCdregion::eFrame_one;
        }
        x_AddQual(eFQ_codon_start, new CFlatIntQVal(frame));

        // translation exception
        if ( cds.IsSetCode_break() ) {
            x_AddQual(eFQ_transl_except, 
                new CFlatCodeBreakQVal(cds.GetCode_break()));
        }
        
        // protein conflict
        static const string conflic_msg = 
            "Protein sequence is in conflict with the conceptual translation";
        bool has_prot = m_Feat->IsSetProduct()  &&
                        (GetLength(m_Feat->GetProduct(), &scope) > 0);
        if ( cds.CanGetConflict()  &&  cds.GetConflict()  &&  has_prot ) {
            x_AddQual(eFQ_prot_conflict, new CFlatStringQVal(conflic_msg));
        }
    } else {
        // frame
        if ( frame > 1 ) {
            x_AddQual(eFQ_codon_start, new CFlatIntQVal(frame));
        }
    }
}

void CFeatureItem::x_AddProtQuals
(const CSeq_feat& feat,
 CBioseqContext& ctx, 
 bool& pseudo,
 bool& had_prot_desc,
 string& precursor_comment) const
{
    const CProt_ref& pref = feat.GetData().GetProt();
    CProt_ref::TProcessed processed = pref.GetProcessed();

    if ( ctx.IsNuc()  ||  (ctx.IsProt()  &&  !IsMappedFromProt()) ) {
        if ( pref.IsSetName()  &&  !pref.GetName().empty() ) {
            CProt_ref::TName names = pref.GetName();
            x_AddQual(eFQ_product, new CFlatStringQVal(names.front()));
            names.pop_front();
            if ( !names.empty() ) {
                x_AddQual(eFQ_prot_names, new CFlatStringListQVal(names));
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
                had_prot_desc = true;
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
        if ( feat.CanGetProduct() ) {
            CBioseq_Handle prot = 
                ctx.GetScope().GetBioseqHandle(feat.GetProduct());
            if ( prot ) {
                x_AddProductIdQuals(prot, eFQ_protein_id);
            } else {
                try {
                    const CSeq_id& prod_id = 
                        GetId(feat.GetProduct(), &ctx.GetScope());
                    if ( ctx.IsRefSeq()  ||  !ctx.Config().ForGBRelease() ) {
                        x_AddQual(eFQ_protein_id, new CFlatSeqIdQVal(prod_id));
                    }
                } catch (CObjmgrUtilException&) {}
            }
        }
    } else { // protein feature on subpeptide bioseq
        x_AddQual(eFQ_derived_from, new CFlatSeqLocQVal(m_Feat->GetLocation()));
        // check precursor_comment
        CConstRef<CSeq_feat> prot = 
            GetBestOverlappingFeat(m_Feat->GetProduct(),
                                   CSeqFeatData::e_Prot,
                                   eOverlap_Simple,
                                   ctx.GetScope());
        if ( prot  &&  prot->CanGetComment() ) {
            precursor_comment = prot->GetComment();
        }
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
         feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_preprotein ) {
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


void CFeatureItem::x_ImportQuals(CBioseqContext& ctx) const
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
        DO_IMPORT(frequency),
        DO_IMPORT(function),
        DO_IMPORT(insertion_seq),
        DO_IMPORT(label),
        DO_IMPORT(map),
        DO_IMPORT(mod_base),
        DO_IMPORT(number),
        DO_IMPORT(old_locus_tag),
        DO_IMPORT(operon),
        DO_IMPORT(organism),
        DO_IMPORT(PCR_conditions),
        DO_IMPORT(phenotype),
        DO_IMPORT(product),
        DO_IMPORT(replace),
        DO_IMPORT(rpt_family),
        DO_IMPORT(rpt_type),
        DO_IMPORT(rpt_unit),
        DO_IMPORT(standard_name),
        DO_IMPORT(transposon),
        DO_IMPORT(usedin)
#undef DO_IMPORT
    };
    typedef const CStaticArrayMap<const char*, EFeatureQualifier, PNocase> TLegalImportMap;
    static TLegalImportMap kLegalImportMap(kLegalImports, sizeof(kLegalImports));

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
        case eFQ_evidence:
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
                x_AddQual(slot, new CFlatStringQVal(*i, CFormatQual::eUnquoted));
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
                break;
            }}
            break;
        default:
            x_AddQual(slot, new CFlatStringQVal(val));
            break;
        }
    }
}


void CFeatureItem::x_AddRptUnitQual(const string& rpt_unit) const
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
    static const TValidRptTypes valid_types(valid_rpt, sizeof(valid_rpt));

    return valid_types.find(type) != valid_types.end();
}


void CFeatureItem::x_AddRptTypeQual(const string& rpt_type, bool check_qual_syntax) const
{
    if (rpt_type.empty()) {
        return;
    }

    vector<string> types;

    if (NStr::StartsWith(rpt_type, '(')  &&  NStr::EndsWith(rpt_type, ')')  &&
        NStr::Find(rpt_type, "(", 1) == NPOS) {
        string tmp = rpt_type.substr(1, rpt_type.length() - 2);
        NStr::Tokenize(tmp, ",", types);
    } else {
        types.push_back(rpt_type);
    }

    NON_CONST_ITERATE (vector<string>, it, types) {
        if (check_qual_syntax  &&  !s_IsValidRptType(*it)) {
            continue;
        }
        NStr::TruncateSpacesInPlace(*it);
        x_AddQual(eFQ_rpt_type, new CFlatStringQVal(*it, CFormatQual::eUnquoted));
    }
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
    DO_QUAL(gene_syn_refseq);

    x_FormatQual(eFQ_gene_allele, "allele", qvec);

    DO_QUAL(operon);

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

    if ( !cfg.GoQualsToNote() ) {
        DO_QUAL(go_component);
        DO_QUAL(go_function);
        DO_QUAL(go_process);
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
    DO_QUAL(rpt_family);
    DO_QUAL(rpt_type);
    DO_QUAL(rpt_unit);
    DO_QUAL(insertion_seq);
    DO_QUAL(transposon);
    DO_QUAL(usedin);

    // extra imports, actually...
    x_FormatQual(eFQ_illegal_qual, "illegal", qvec);

    DO_QUAL(replace);

    DO_QUAL(transl_except);
    DO_QUAL(transl_table);
    DO_QUAL(codon);
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


void CFeatureItem::x_FormatNoteQuals(CFlatFeature& ff) const
{
    CFlatFeature::TQuals qvec;

#define DO_NOTE(x) x_FormatNoteQual(eFQ_##x, #x, qvec)
    x_FormatNoteQual(eFQ_transcript_id_note, "tscpt_id_note", qvec);
    DO_NOTE(gene_desc);
    DO_NOTE(gene_syn);
    DO_NOTE(trna_codons);
    DO_NOTE(encodes);
    DO_NOTE(prot_desc);
    DO_NOTE(prot_note);
    DO_NOTE(prot_comment);
    DO_NOTE(prot_method);
    //DO_NOTE(figure);
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
    if ( GetContext()->Config().GoQualsToNote() ) {
        DO_NOTE(go_component);
        DO_NOTE(go_function);
        DO_NOTE(go_process);
    }
#undef DO_NOTE

    string notestr;
    const string* suffix = &kEmptyStr;

    bool add_period = false;
    string qual, prefix;
    ITERATE (CFlatFeature::TQuals, it, qvec) {
        qual = (*it)->GetValue();
        
        prefix.erase();
        if (!notestr.empty()) {
            prefix = *suffix;
            if (!NStr::EndsWith(prefix, '\n')) {
                prefix += (*it)->GetPrefix();
            }
        }
        
        JoinNoRedund(notestr, prefix, qual);
        add_period = (*it)->GetAddPeriod();

        suffix = &(*it)->GetSuffix();
    }

    if (!notestr.empty()) {
        if (add_period  &&  !NStr::EndsWith(notestr, '.') ) {
            AddPeriod(notestr);
        }
        CRef<CFormatQual> note(new CFormatQual("note", notestr));
        ff.SetQuals().push_back(note);
    }
}


void CFeatureItem::x_FormatQual
(EFeatureQualifier slot,
 const string& name,
 CFlatFeature::TQuals& qvec,
 IFlatQVal::TFlags flags) const
{
    pair<TQCI, TQCI> range = const_cast<const TQuals&>(m_Quals).GetQuals(slot);
    for (TQCI it = range.first;  it != range.second;  ++it) {
        it->second->Format(qvec, name, *GetContext(), flags);
    }
}


void CFeatureItem::x_FormatNoteQual
(EFeatureQualifier slot,
 const string& name, 
 CFlatFeature::TQuals& qvec,
 IFlatQVal::TFlags flags) const
{
    flags |= IFlatQVal::fIsNote;

    pair<TQCI, TQCI> range = const_cast<const TQuals&>(m_Quals).GetQuals(slot);
    for (TQCI it = range.first;  it != range.second;  ++it) {
        it->second->Format(qvec, name, *GetContext(), flags);
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


void CFeatureItem::x_CleanQuals(bool& had_prot_desc, const TGeneSyn* gene_syn) const
{
    const CBioseqContext& ctx = *GetContext();

    if (ctx.IsGED()  &&  !ctx.IsProt()  &&  ctx.Config().DropIllegalQuals()) {
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
                    it = (prod_names->SetValue().erase(it))--;
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
                    it = (prod_names->SetValue().erase(it))--;
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
        if (gene_syn != NULL  &&  cds_prod != NULL) {
            ITERATE (TGeneSyn, it, *gene_syn) {
                if (!NStr::IsBlank(*it)  &&  pdesc == *it) {
                    x_RemoveQuals(eFQ_prot_desc);
                    prot_desc = NULL;
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
    }

    const CFlatStringQVal* note = x_GetStringQual(eFQ_seqfeat_note);
    if (note != NULL  &&  standard_name != NULL) {
        if (NStr::Equal(note->GetValue(), standard_name->GetValue())) {
            x_RemoveQuals(eFQ_seqfeat_note);
        }
    }

    if (prot_desc == NULL) {
        had_prot_desc = false;
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
    TQualPair(eFQ_evidence, CSeqFeatData::eQual_evidence),
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
    TQualPair(eFQ_gene_syn_refseq, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_gene_note, CSeqFeatData::eQual_note),
    TQualPair(eFQ_gene_xref, CSeqFeatData::eQual_db_xref),
    TQualPair(eFQ_go_component, CSeqFeatData::eQual_note),
    TQualPair(eFQ_go_function, CSeqFeatData::eQual_note),
    TQualPair(eFQ_go_process, CSeqFeatData::eQual_note),
    TQualPair(eFQ_heterogen, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_illegal_qual, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_insertion_seq, CSeqFeatData::eQual_insertion_seq),
    TQualPair(eFQ_label, CSeqFeatData::eQual_label),
    TQualPair(eFQ_locus_tag, CSeqFeatData::eQual_locus_tag),
    TQualPair(eFQ_map, CSeqFeatData::eQual_map),
    TQualPair(eFQ_maploc, CSeqFeatData::eQual_note),
    TQualPair(eFQ_mod_base, CSeqFeatData::eQual_mod_base),
    TQualPair(eFQ_modelev, CSeqFeatData::eQual_note),
    TQualPair(eFQ_number, CSeqFeatData::eQual_number),
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
    TQualPair(eFQ_rpt_family, CSeqFeatData::eQual_rpt_family),
    TQualPair(eFQ_rpt_type, CSeqFeatData::eQual_rpt_type),
    TQualPair(eFQ_rpt_unit, CSeqFeatData::eQual_rpt_unit),
    TQualPair(eFQ_rrna_its, CSeqFeatData::eQual_note),
    TQualPair(eFQ_sec_str_type, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_selenocysteine, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_selenocysteine_note, CSeqFeatData::eQual_note),
    TQualPair(eFQ_seqfeat_note, CSeqFeatData::eQual_note),
    TQualPair(eFQ_site, CSeqFeatData::eQual_note),
    TQualPair(eFQ_site_type, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_standard_name, CSeqFeatData::eQual_standard_name),
    TQualPair(eFQ_transcription, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_transcript_id, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_transcript_id_note, CSeqFeatData::eQual_note),
    TQualPair(eFQ_transl_except, CSeqFeatData::eQual_transl_except),
    TQualPair(eFQ_transl_table, CSeqFeatData::eQual_transl_table),
    TQualPair(eFQ_translation, CSeqFeatData::eQual_translation),
    TQualPair(eFQ_transposon, CSeqFeatData::eQual_transposon),
    TQualPair(eFQ_trna_aa, CSeqFeatData::eQual_bad),
    TQualPair(eFQ_trna_codons, CSeqFeatData::eQual_note),
    TQualPair(eFQ_usedin, CSeqFeatData::eQual_usedin),
    TQualPair(eFQ_xtra_prod_quals, CSeqFeatData::eQual_note)
};
typedef CStaticArrayMap<EFeatureQualifier, CSeqFeatData::EQualifier> TQualMap;
static const TQualMap sc_QualMap(sc_GbToFeatQualMap, sizeof(sc_GbToFeatQualMap));

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


void CFeatureItem::x_AddFTableQuals(CBioseqContext& ctx) const
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


void CFeatureItem::x_AddFTableExtQuals(const CSeq_feat::TExt& ext) const
{
    ITERATE (CUser_object::TData, it, ext.GetData()) {
        const CUser_field& field = **it;
        if ( !field.CanGetData() ) {
            continue;
        }
        if ( field.GetData().IsObject() ) {
            const CUser_object& obj = field.GetData().GetObject();
            x_AddExtQuals(obj);
            return;
        } else if ( field.GetData().IsObjects() ) {
            ITERATE (CUser_field::C_Data::TObjects, o, field.GetData().GetObjects()) {
                x_AddExtQuals(**o);
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
                        name = "go_process";
                    } else if ( label == "Component" ) {               
                        name = "go_component";
                    } else if ( label == "Function" ) {
                        name = "go_function";
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


void CFeatureItem::x_AddFTableDbxref(const CSeq_feat::TDbxref& dbxref) const
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


bool CFeatureItem::x_AddFTableGeneQuals(const CGene_ref& gene) const
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


void CFeatureItem::x_AddFTableRnaQuals(const CSeq_feat& feat, CBioseqContext& ctx) const
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
            const CSeq_id& id = GetId(prod, eGetId_Best);
            string id_str;
            if ( id.IsGenbank()  ||  id.IsEmbl()  ||  id.IsDdbj()  ||
                 id.IsTpg()  ||  id.IsTpd()  ||  id.IsTpe()  ||
                 id.IsOther() ||
                 (id.IsLocal()  &&  !ctx.Config().SuppressLocalId()) ) {
                id_str = id.GetSeqIdString(true);
            } else if ( id.IsGeneral() ) {
                id_str = id.AsFastaString();
            }
            x_AddFTableQual("transcript_id", id_str);
        }
    }
}


void CFeatureItem::x_AddFTableCdregionQuals(const CSeq_feat& feat, CBioseqContext& ctx) const
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
    const CSeq_id* id = 0;
    string id_str;
    if ( prod ) {
        id = &GetId(prod, eGetId_Best);
    } else if ( feat.CanGetProduct() ) {
        try { 
            id = &GetId(feat.GetProduct(), &ctx.GetScope());
            if ( id->IsGi() ) {
                // get "normal" id 
            }
        } catch (CObjmgrUtilException&) {
            id = 0;
        }
    }
    if ( id != 0 ) {
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


void CFeatureItem::x_AddFTableProtQuals(const CSeq_feat& prot) const
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


void CFeatureItem::x_AddFTableRegionQuals(const CSeqFeatData::TRegion& region) const
{
    if ( !region.empty() ) {
        x_AddFTableQual("region", region);
    }
}


void CFeatureItem::x_AddFTableBondQuals(const CSeqFeatData::TBond& bond) const
{
    TBondList::const_iterator it = sc_BondList.find(bond);
    if (it != sc_BondList.end()) {
        x_AddFTableQual("bond_type", it->second);
    }
}


void CFeatureItem::x_AddFTableSiteQuals(const CSeqFeatData::TSite& site) const
{
    size_t siteidx = static_cast<size_t>(site);
    if ( siteidx == static_cast<size_t>(CSeqFeatData::eSite_other) ) {
        siteidx = 26;
    }
    if ( siteidx > 0  &&  siteidx < 27 ) {
        x_AddFTableQual("site_type", s_SiteList[siteidx]);
    }
}


void CFeatureItem::x_AddFTablePsecStrQuals(const CSeqFeatData::TPsec_str& psec_str) const
{
    size_t idx = static_cast<size_t>(psec_str);
    if ( idx > 0  &&  idx < 3 ) {
        x_AddFTableQual("sec_str_type", s_PsecStrText[idx]);
    }
}


void CFeatureItem::x_AddFTablePsecStrQuals(const CSeqFeatData::THet& het) const
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

void CFeatureItem::x_AddFTableBiosrcQuals(const CBioSource& src) const
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


void CSourceFeatureItem::x_GatherInfo(CBioseqContext& ctx) {
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
    ITERATE (CBioSource::TSubtype, it, src.GetSubtype()) {
        ESourceQualifier slot = s_SubSourceToSlot(**it);
        if ( slot == eSQ_insertion_seq_name ) {
            insertion_seq_name = true;
        } else if ( slot == eSQ_plasmid_name ) {
            plasmid_name = true;
        } else if ( slot == eSQ_transposon_name ) {
            transposon_name = true;
        }
        if (slot != eSQ_none) {
            x_AddQual(slot, new CFlatSubSourceQVal(**it));
        }
    }

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
    x_FormatQual(eSQ_spec_or_nat_host, "specific_host", qvec);
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

    x_FormatQual(eSQ_plasmid_name,       "plasmid", qvec);
    x_FormatQual(eSQ_transposon_name,    "transposon", qvec);
    x_FormatQual(eSQ_insertion_seq_name, "insertion_seq", qvec);

    DO_QUAL(country);

    DO_QUAL(focus);

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

    DO_QUAL(lat_lon);
    DO_QUAL(collection_date);
    DO_QUAL(collected_by);
    DO_QUAL(identified_by);
    DO_QUAL(fwd_primer_seq);
    DO_QUAL(rev_primer_seq);
    DO_QUAL(fwd_primer_name);
    DO_QUAL(rev_primer_name);
    
    DO_QUAL(genotype);
    x_FormatQual(eSQ_plastid_name, "plastid", qvec);
    
    x_FormatQual(eSQ_endogenous_virus_name, "endogenous_virus", qvec);

    x_FormatQual(eSQ_zero_orgmod, "?", qvec);
    x_FormatQual(eSQ_one_orgmod,  "?", qvec);
    x_FormatQual(eSQ_zero_subsrc, "?", qvec);
#undef DO_QUAL
}


//static bool s_RemovePeriodFromEnd(string& str)
//{
//    if (NStr::EndsWith(str, '.')) {
//        str.resize(str.length() - 1);
//        return true;
//    }
//    return false;
//}


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
        
        DO_NOTE(lat_lon);
        DO_NOTE(collection_date);
        DO_NOTE(collected_by);
        DO_NOTE(identified_by);
        DO_NOTE(fwd_primer_seq);
        DO_NOTE(rev_primer_seq);
        DO_NOTE(fwd_primer_name);
        DO_NOTE(rev_primer_name);

        DO_NOTE(genotype);
        x_FormatNoteQual(eSQ_plastid_name, "plastid", qvec);
        
        x_FormatNoteQual(eSQ_endogenous_virus_name, "endogenous_virus", qvec);
        if (!m_WasDesc) {
            x_FormatNoteQual(eSQ_seqfeat_note, "note", qvec);
            DO_NOTE(orgmod_note);
            DO_NOTE(subsource_note);
        }
    }
    
    x_FormatNoteQual(eSQ_common_name, "common", qvec);

    if ( GetContext()->Config().SrcQualsToNote() ) {
        x_FormatNoteQual(eSQ_zero_orgmod, "?", qvec);
        x_FormatNoteQual(eSQ_one_orgmod,  "?", qvec);
        x_FormatNoteQual(eSQ_zero_subsrc, "?", qvec);
    }
#undef DO_NOTE

    string notestr;
    const string* suffix = &kEmptyStr;

    if ( GetSource().CanGetGenome()  &&  
        GetSource().GetGenome() == CBioSource::eGenome_extrachrom ) {
        static const string kEOL = "\n";
        notestr += "extrachromosomal";
        suffix = &kEOL;
    }

    string prefix;
    ITERATE (CFlatFeature::TQuals, it, qvec) {
        string note = (*it)->GetValue();
        add_period = (*it)->GetAddPeriod();
        
        prefix.erase();
        if (!notestr.empty()) {
            prefix = *suffix;
            if (!NStr::EndsWith(prefix, '\n')) {
                prefix += (*it)->GetPrefix();
            }
        }

        JoinNoRedund(notestr, prefix, note);
        suffix = &(*it)->GetSuffix();
    }

    if (!notestr.empty()) {
        if (add_period  &&  !NStr::EndsWith(notestr, "...")) {
            AddPeriod(notestr);
        }

        CRef<CFormatQual> note;
        if (m_WasDesc) {
            // AB055064.1 says ignore tilde on descriptors
            note.Reset(new CFormatQual("note", notestr));
        } else {
            // ASZ93724.1 says eTilde_newline on features
            note.Reset(new CFormatQual("note", ExpandTildes(notestr, eTilde_newline)));
        }
        ff.SetQuals().push_back(note);
    }
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
    pair<TQCI, TQCI> range = const_cast<const TQuals&>(m_Quals).GetQuals(slot);
    for (TQCI it = range.first;  it != range.second;  ++it) {
        const IFlatQVal* qual = it->second;
        qual->Format(qvec, name, *GetContext(),
                     flags | IFlatQVal::fIsSource);
    }
}


void CSourceFeatureItem::Subtract(const CSourceFeatureItem& other, CScope &scope)
{
    m_Loc = Seq_loc_Subtract(GetLoc(), other.GetLoc(), 0, &scope);;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.43  2005/02/09 14:57:06  shomrat
* Added subsource qualifiers for Barcode of Life project
*
* Revision 1.42  2005/01/31 16:32:31  shomrat
* Added /compare qualifier
*
* Revision 1.41  2005/01/13 16:31:59  shomrat
* Show /peptide in Ncbieaa coding
*
* Revision 1.40  2005/01/12 17:24:26  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.39  2005/01/12 16:47:23  shomrat
* more fixes to feature formatting
*
* Revision 1.38  2004/12/17 13:58:15  dicuccio
* Drop unused exception variable (fixes compiler warning)
*
* Revision 1.37  2004/11/24 16:52:50  shomrat
* Standardize flat-file customization flags
*
* Revision 1.36  2004/11/19 15:13:41  shomrat
* Fixed quals from Gene-ref
*
* Revision 1.35  2004/11/17 21:25:13  grichenk
* Moved seq-loc related functions to seq_loc_util.[hc]pp.
* Replaced CNotUnique and CNoLength exceptions with CObjmgrUtilException.
*
* Revision 1.34  2004/11/15 20:07:20  shomrat
* Fixed /note qual
*
* Revision 1.33  2004/11/01 19:33:09  grichenk
* Removed deprecated methods
*
* Revision 1.32  2004/10/18 18:59:13  shomrat
* Added Bond and Gene qual classes; Fixes to note formatting
*
* Revision 1.31  2004/10/05 15:40:55  shomrat
* Fixes to feature formatting
*
* Revision 1.30  2004/08/30 13:44:03  shomrat
* fixes to feature formatting
*
* Revision 1.29  2004/08/19 18:03:31  shomrat
* Add prot_EC_number qual to Prot features
*
* Revision 1.28  2004/08/19 16:38:27  shomrat
* feature_item.cpp
*
* Revision 1.27  2004/08/09 19:19:19  shomrat
* Fixed prot_activity qualifier
*
* Revision 1.26  2004/05/26 14:57:54  shomrat
* removed non-preferred variants ribosome slippage, trans splicing, alternate processing, and non-consensus splice site
*
* Revision 1.25  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.24  2004/05/19 14:47:19  shomrat
* Check for illegal qualifiers in RELEASE mode
*
* Revision 1.23  2004/05/08 12:12:00  dicuccio
* Use best ID representation for protein products
*
* Revision 1.22  2004/05/07 15:22:39  shomrat
* Added qualifiers add Seq-id filters
*
* Revision 1.21  2004/05/06 19:41:00  ucko
* Kill unwanted definition of ff as a macro, if present (as on Mac OS 10.3)
*
* Revision 1.20  2004/05/06 17:49:59  shomrat
* Fixed feature formatting
*
* Revision 1.19  2004/04/22 15:56:47  shomrat
* Changes in context
*
* Revision 1.18  2004/04/13 16:46:39  shomrat
* Changes due to GBSeq format
*
* Revision 1.17  2004/04/12 16:17:27  vasilche
* Fixed '=' <-> '==' typo.
*
* Revision 1.16  2004/04/07 14:27:15  shomrat
* Added methods forFTable format
*
* Revision 1.15  2004/03/31 16:00:12  ucko
* C(Source)FeatureItem::x_FormatQual: make sure to call the const
* version of GetQuals to fix WorkShop build errors.
*
* Revision 1.14  2004/03/30 20:27:09  shomrat
* Separated quals container from feature class
*
* Revision 1.13  2004/03/25 20:37:41  shomrat
* Use handles
*
* Revision 1.12  2004/03/18 15:37:57  shomrat
* Fixes to note qual formatting
*
* Revision 1.11  2004/03/10 21:30:18  shomrat
* + product tRNA qual; limit Seq-is types for product ids
*
* Revision 1.10  2004/03/10 16:22:18  shomrat
* Fixed product-id qualifiers
*
* Revision 1.9  2004/03/08 21:02:18  shomrat
* + Exception quals gathering; fixed product id gathering
*
* Revision 1.8  2004/03/08 15:45:46  shomrat
* Added pseudo qualifier
*
* Revision 1.7  2004/03/05 18:44:48  shomrat
* fixes to qualifiers collection and formatting
*
* Revision 1.6  2004/02/19 18:07:26  shomrat
* HideXXXFeat() => HideXXXFeats()
*
* Revision 1.5  2004/02/11 22:50:35  shomrat
* override GetKey
*
* Revision 1.4  2004/02/11 16:56:42  shomrat
* changes in qualifiers gathering and formatting
*
* Revision 1.3  2004/01/14 16:12:20  shomrat
* uncommenetd code
*
* Revision 1.2  2003/12/18 17:43:33  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:20:52  shomrat
* Initial Revision (adapted from flat lib)
*
* Revision 1.10  2003/10/17 20:58:41  ucko
* Don't assume coding-region features have their "product" fields set.
*
* Revision 1.9  2003/10/16 20:21:53  ucko
* Fix a copy-and-paste error in CFeatureItem::x_AddQuals
*
* Revision 1.8  2003/10/08 21:11:12  ucko
* Add a couple of accessors to CFeatureItemBase for the GFF/GTF formatter.
*
* Revision 1.7  2003/07/22 18:04:13  dicuccio
* Fixed access of unset optional variables
*
* Revision 1.6  2003/06/02 16:06:42  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.5  2003/04/24 16:15:58  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.4  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.3  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.2  2003/03/10 22:01:36  ucko
* Change SLegalImport::m_Name from string to const char* (needed by MSVC).
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
