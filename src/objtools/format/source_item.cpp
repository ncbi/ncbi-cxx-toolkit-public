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
* Author:  Mati Shomrat, NCBI
*
* File Description:
*   flat-file generator -- source item implementation
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqblock/GB_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/context.hpp>
#include <objmgr/util/objutil.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const CSourceItem::TTaxid CSourceItem::kInvalidTaxid = INVALID_TAX_ID;


///////////////////////////////////////////////////////////////////////////
//
// SOURCE
//   ORGANISM

CSourceItem::CSourceItem(CBioseqContext& ctx) :
    CFlatItem(&ctx),
    m_Taxname(&scm_Unknown), m_Common(&kEmptyStr),
    m_Organelle(&kEmptyStr), m_Lineage(scm_Unclassified),
    m_SourceLine(&kEmptyStr), m_Mod(&scm_EmptyList), m_Taxid(kInvalidTaxid),
    m_UsingAnamorph(false)
{
    x_GatherInfo(ctx);
}

IFlatItem::EItem CSourceItem::GetItemType(void) const
{
    return eItem_Source;
}

CSourceItem::CSourceItem(CBioseqContext& ctx, const CBioSource& bsrc, const CSerialObject& obj) :
    CFlatItem(&ctx),
    m_Taxname(&scm_Unknown), m_Common(&kEmptyStr),
    m_Organelle(&kEmptyStr), m_Lineage(scm_Unclassified),
    m_SourceLine(&kEmptyStr), m_Mod(&scm_EmptyList), m_Taxid(kInvalidTaxid),
    m_UsingAnamorph(false)
{
    x_GatherInfo(ctx, bsrc, obj);
}


void CSourceItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatSource(*this, text_os);
}


/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/


// static members initialization
const string       CSourceItem::scm_Unknown        = "Unknown.";
const string       CSourceItem::scm_Unclassified   = "Unclassified.";
const list<string> CSourceItem::scm_EmptyList;


static CConstRef<CSeq_feat> x_GetSourceFeatFromCDS  (
    const CBioseq_Handle& bsh
)

{
    CConstRef<CSeq_feat>   cds_feat;
    CConstRef<CSeq_loc>    cds_loc;
    CConstRef<CBioSource>  src_ref;

    CScope& scope = bsh.GetScope();

    cds_feat = sequence::GetCDSForProduct (bsh);

    if (cds_feat) {
        cds_loc = &cds_feat->GetLocation();
        if (cds_loc) {
            CRef<CSeq_loc> cleaned_location( new CSeq_loc );
            cleaned_location->Assign( *cds_loc );
            CConstRef<CSeq_feat> src_feat
                = sequence::GetBestOverlappingFeat (*cleaned_location, CSeqFeatData::eSubtype_biosrc, sequence::eOverlap_SubsetRev, scope);
            if (! src_feat  &&  cleaned_location->IsSetStrand()  &&  IsReverse(cleaned_location->GetStrand())) {
                CRef<CSeq_loc> rev_loc(sequence::SeqLocRevCmpl(*cleaned_location, &scope));
                cleaned_location->Assign(*rev_loc);
                src_feat = sequence::GetBestOverlappingFeat (*cleaned_location, CSeqFeatData::eSubtype_biosrc, sequence::eOverlap_SubsetRev, scope);
            }
            if (src_feat) {
                const CSeq_feat& feat = *src_feat;
                if (feat.IsSetData()) {
                    return src_feat;
                }
            }
        }
    }

    return CConstRef<CSeq_feat> ();
}

static const char * legal_organelles[] = {
    "chloroplast",
    "chromoplast",
    "kinetoplast",
    "mitochondrion",
    "plastid",
    "cyanelle",
    "nucleomorph",
    "apicoplast",
    "leucoplast",
    "proplastid",
    "hydrogenosome",
    "chromatophore"
};

void CSourceItem::x_GatherInfoIdx(CBioseqContext& ctx)
{
    CRef<CSeqEntryIndex> idx = ctx.GetSeqEntryIndex();
    if (! idx) return;
    const CBioseq_Handle& bsh = ctx.GetHandle();
    CRef<CBioseqIndex> bsx = idx->GetBioseqIndex (bsh);
    if (! bsx) return;

    m_Taxname = &bsx->GetTaxname();
    m_Common = &bsx->GetCommon();
    m_Taxid = bsx->GetTaxid();
    m_UsingAnamorph = bsx->IsUsingAnamorph();

    const string& lineage = bsx->GetLineage();
    if (lineage.empty()) {
        m_Lineage = scm_Unclassified;
    } else {
        m_Lineage = bsx->GetLineage();
    }

    const string* orgnlle = &bsx->GetOrganelle();
    for (int i = 0; i < sizeof (legal_organelles) / sizeof (const char*); i++) {
        CTempString str = legal_organelles [i];
        if (NStr::CompareNocase (*orgnlle, str) == 0) {
            m_Organelle = orgnlle;
            return;
        }
    }
}

void CSourceItem::x_GatherInfo(CBioseqContext& ctx)
{
    CConstRef<CSeq_feat>   cds_feat;
    CConstRef<CSeq_loc>    cds_loc;
    CConstRef<CBioSource>  src_ref;
    CConstRef<CSeq_feat>   src_feat;

    /*
    if (ctx.UsingSeqEntryIndex()) {
        x_GatherInfoIdx(ctx);
        return;
    }
    */

    if (ctx.IsProt()) {
        const CBioseq_Handle& bsh = ctx.GetHandle();
        src_feat = x_GetSourceFeatFromCDS (bsh);
        if (src_feat.NotEmpty()) {
            const CSeq_feat& feat = *src_feat;
            x_SetSource(feat.GetData().GetBiosrc(), feat);
            return;
        }
    }

    // For DDBJ format first try a GB-Block descriptor (old style)
    if ( ctx.Config().IsFormatDDBJ() ) {
        CSeqdesc_CI gb_it(ctx.GetHandle(), CSeqdesc::e_Genbank);
        if ( gb_it ) {
            const CGB_block& gb = gb_it->GetGenbank();
            if ( gb.CanGetSource()  &&  !gb.GetSource().empty() ) {
                x_SetSource(gb, *gb_it);
                return;
            }
        }
    }

    // find a biosource descriptor
    CSeqdesc_CI dsrc_it(ctx.GetHandle(), CSeqdesc::e_Source);
    if ( dsrc_it ) {
        x_SetSource(dsrc_it->GetSource(), *dsrc_it);
        return;
    } 
    
    // if no descriptor was found, try a source feature
    CFeat_CI fsrc_it(ctx.GetHandle(), CSeqFeatData::e_Biosrc);
    if ( fsrc_it ) {
        const CSeq_feat& src_feat = fsrc_it->GetOriginalFeature();
        x_SetSource(src_feat.GetData().GetBiosrc(), src_feat);
    }   
}

void CSourceItem::x_GatherInfo(CBioseqContext& ctx, const CBioSource& bsrc, const CSerialObject& obj)
{
    CConstRef<CSeq_feat>   cds_feat;
    CConstRef<CSeq_loc>    cds_loc;
    CConstRef<CBioSource>  src_ref;
    CConstRef<CSeq_feat>   src_feat;

    /*
    if (ctx.UsingSeqEntryIndex()) {
        x_GatherInfoIdx(ctx);
        return;
    }
    */

    if (ctx.IsProt()) {
        const CBioseq_Handle& bsh = ctx.GetHandle();
        src_feat = x_GetSourceFeatFromCDS (bsh);
        if (src_feat.NotEmpty()) {
            const CSeq_feat& feat = *src_feat;
            x_SetSource(feat.GetData().GetBiosrc(), feat);
            return;
        }
    }

    // For DDBJ format first try a GB-Block descriptor (old style)
    if ( ctx.Config().IsFormatDDBJ() ) {
        CSeqdesc_CI gb_it(ctx.GetHandle(), CSeqdesc::e_Genbank);
        if ( gb_it ) {
            const CGB_block& gb = gb_it->GetGenbank();
            if ( gb.CanGetSource()  &&  !gb.GetSource().empty() ) {
                x_SetSource(gb, *gb_it);
                return;
            }
        }
    }

    x_SetSource(bsrc, obj);
    return;

    // find a biosource descriptor
    CSeqdesc_CI dsrc_it(ctx.GetHandle(), CSeqdesc::e_Source);
    if ( dsrc_it ) {
        x_SetSource(dsrc_it->GetSource(), *dsrc_it);
        return;
    } 
    
    // if no descriptor was found, try a source feature
    CFeat_CI fsrc_it(ctx.GetHandle(), CSeqFeatData::e_Biosrc);
    if ( fsrc_it ) {
        const CSeq_feat& src_feat = fsrc_it->GetOriginalFeature();
        x_SetSource(src_feat.GetData().GetBiosrc(), src_feat);
    }   
}

// for old-style
void CSourceItem::x_SetSource
(const CGB_block&  gb,
 const CSeqdesc& desc)
{
    x_SetObject(desc);

    // set source line
    if ( gb.CanGetSource() ) {
        m_SourceLine = &(gb.GetSource());
    }
}


static const string s_old_organelle_prefix[] = {
  kEmptyStr,
  kEmptyStr,
  "Chloroplast ",
  "Chromoplast ",
  "Kinetoplast ",
  "Mitochondrion ",
  "Plastid ",
  kEmptyStr,
  kEmptyStr,
  kEmptyStr,
  kEmptyStr,
  kEmptyStr,
  "Cyanelle ",
  kEmptyStr,
  kEmptyStr,
  "Nucleomorph ",
  "Apicoplast ",
  "Leucoplast ",
  "Proplastid ",
  kEmptyStr,
  "Hydrogenosome ",
  kEmptyStr,
  "Chromatophore "
};

static const string s_organelle_prefix[] = {
  kEmptyStr,
  kEmptyStr,
  "chloroplast ",
  "chromoplast ",
  "kinetoplast ",
  "mitochondrion ",
  "plastid ",
  kEmptyStr,
  kEmptyStr,
  kEmptyStr,
  kEmptyStr,
  kEmptyStr,
  "cyanelle ",
  kEmptyStr,
  kEmptyStr,
  "nucleomorph ",
  "apicoplast ",
  "leucoplast ",
  "proplastid ",
  kEmptyStr,
  "hydrogenosome ",
  kEmptyStr,
  "chromatophore "
};
static const unsigned int s_organelle_prefix_size
    = sizeof( s_organelle_prefix ) /sizeof( const string );

void CSourceItem::x_SetSource
(const CBioSource& bsrc,
 const CSerialObject& obj)
{
    x_SetObject(obj);

    if ( !bsrc.CanGetOrg() ) {
        return;
    }

    const COrg_ref& org = bsrc.GetOrg();
    
    // Taxname
    {{
        if ( org.CanGetTaxname() ) {
            m_Taxname = &(org.GetTaxname());
        }
    }}

    // Organelle
    {{
        CBioSource::TGenome genome = bsrc.CanGetGenome() ? bsrc.GetGenome() 
            : CBioSource::eGenome_unknown;
        if ( genome >= s_organelle_prefix_size ) {
            m_Organelle = &(kEmptyStr);
        }
        else {
            m_Organelle = &(s_organelle_prefix[genome]);
        }
        
        // If the organelle prefix is already on the  name, don't add it.
        if ( NStr::StartsWith(*m_Taxname, *m_Organelle, NStr::eNocase) ) {
            m_Organelle = &(kEmptyStr);
        }
    }}

    // Mod
    {{
        m_Mod = &org.GetMod();
    }}

    // Common
    {{
        const string* common = &kEmptyStr;
        if ( org.CanGetCommon() ) {
            common = &(org.GetCommon());
        }
        
        if ( org.CanGetOrgname() ) {
            const COrgName& org_name = org.GetOrgname();
            
            const string *com = 0, *acr = 0, *syn = 0, *ana = 0,
                *gbacr = 0, *gbana = 0, *gbsyn = 0, *met = 0;
            int numcom = 0, numacr = 0, numsyn = 0, numana = 0, numgbacr = 0, 
                numgbana = 0, numgbsyn = 0, nummet = 0;
            ITERATE( COrgName::TMod, mod, org_name.GetMod() ) {
                if ( (*mod)->CanGetSubtype()  &&  (*mod)->CanGetSubname() ) {
                    switch ( (*mod)->GetSubtype() ) {
                    case COrgMod::eSubtype_common:
                        com = &((*mod)->GetSubname());
                        ++numcom;
                        break;
                    case COrgMod::eSubtype_acronym:
                        acr = &((*mod)->GetSubname());
                        ++numacr;
                        break;
                    case COrgMod::eSubtype_synonym:
                        syn = &((*mod)->GetSubname());
                        ++numsyn;
                        break;
                    case COrgMod::eSubtype_anamorph:
                        ana = &((*mod)->GetSubname());
                        ++numana;
                        break;
                    case COrgMod::eSubtype_gb_acronym:
                        gbacr = &((*mod)->GetSubname());
                        ++numgbacr;
                        break;
                    case COrgMod::eSubtype_gb_anamorph:
                        gbana = &((*mod)->GetSubname());
                        ++numgbana;
                        break;
                    case COrgMod::eSubtype_gb_synonym:
                        gbsyn = &((*mod)->GetSubname());
                        ++numgbsyn;
                        break;
                    case COrgMod::eSubtype_metagenome_source:
                        met = &((*mod)->GetSubname());
                        ++nummet;
                    default:
                        break;
                    }
                }
            }
            
            if (numacr > 1) {
               acr = NULL;
            }
            if (numana > 1) {
               ana = NULL;
            }
            if (numcom > 1) {
               com = NULL;
            }
            if (numsyn > 1) {
               syn = NULL;
            }
            if (numgbacr > 1) {
               gbacr = NULL;
            }
            if (numgbana > 1) {
               gbana = NULL;
            }
            if (numgbsyn > 1) {
               gbsyn = NULL;
            }
            if( nummet > 1 ) {
                met = NULL;
            }

            if( m_Common->empty()  &&  met != 0 ) {
                m_Common = met;
            } else if ( m_Common->empty()  &&  syn != 0 ) {
                m_Common = syn;
            } else if ( m_Common->empty()  &&  acr != 0 ) {
                m_Common = acr;
            } else if ( m_Common->empty()  &&  ana != 0 ) {
                m_Common = ana;
                m_UsingAnamorph = true;
            } else if ( m_Common->empty()  &&  com != 0 ) {
                m_Common = com;
            } else if ( m_Common->empty()  &&  gbsyn != 0 ) {
                m_Common = gbsyn;
            } else if ( m_Common->empty()  &&  gbacr != 0 ) {
                m_Common = gbacr;
            } else if ( m_Common->empty()  &&  gbana != 0 ) {
                m_Common = gbana;
                m_UsingAnamorph = true;
            }
        }

        if ( m_Common->empty() ) {
            m_Common = common;
        }
    }}

    // Lineage
    {{
        if ( org.CanGetOrgname() ) {
            const COrgName& org_name = org.GetOrgname();
            if ( org_name.CanGetLineage() ) {
                m_Lineage = org_name.GetLineage();
                AddPeriod(m_Lineage);
            }
        }
    }}

    // Taxid
    {{
        TTaxid taxid = org.GetTaxId();
        if (taxid != ZERO_TAX_ID) {
            m_Taxid = taxid;
        }
    }}
}


END_SCOPE(objects)
END_NCBI_SCOPE
