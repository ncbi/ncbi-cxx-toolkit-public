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
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const CSourceItem::TTaxid CSourceItem::kInvalidTaxid = -1;


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


void CSourceItem::x_GatherInfo(CBioseqContext& ctx)
{
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
  kEmptyStr
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
  kEmptyStr
};


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
        
        m_Organelle = &(s_organelle_prefix[genome]);
        
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
                *gbacr = 0, *gbana = 0, *gbsyn = 0;
            ITERATE( COrgName::TMod, mod, org_name.GetMod() ) {
                if ( (*mod)->CanGetSubtype()  &&  (*mod)->CanGetSubname() ) {
                    switch ( (*mod)->GetSubtype() ) {
                    case COrgMod::eSubtype_common:
                        com = &((*mod)->GetSubname());
                        break;
                    case COrgMod::eSubtype_acronym:
                        acr = &((*mod)->GetSubname());
                        break;
                    case COrgMod::eSubtype_synonym:
                        syn = &((*mod)->GetSubname());
                        break;
                    case COrgMod::eSubtype_anamorph:
                        ana = &((*mod)->GetSubname());
                        break;
                    case COrgMod::eSubtype_gb_acronym:
                        gbacr = &((*mod)->GetSubname());
                        break;
                    case COrgMod::eSubtype_gb_anamorph:
                        gbana = &((*mod)->GetSubname());
                        break;
                    case COrgMod::eSubtype_gb_synonym:
                        gbsyn = &((*mod)->GetSubname());
                        break;
                    default:
                        break;
                    }
                }
            }
            
            if ( m_Common->empty()  &&  syn != 0 ) {
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
            } else if ( m_Common->empty() ) {
                m_Common = common;
            }
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
        if (taxid != 0) {
            m_Taxid = taxid;
        }
    }}
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.12  2005/03/14 18:19:02  grichenk
* Added SAnnotSelector(TFeatSubtype), fixed initialization of CFeat_CI and
* SAnnotSelector.
*
* Revision 1.11  2005/02/07 15:01:27  shomrat
* Added Taxid
*
* Revision 1.10  2004/11/01 19:33:09  grichenk
* Removed deprecated methods
*
* Revision 1.9  2004/08/19 16:33:46  shomrat
* m_Lineage no longer a pointer
*
* Revision 1.8  2004/08/12 15:50:15  shomrat
* use new organelle prefix
*
* Revision 1.7  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.6  2004/04/22 15:53:09  shomrat
* Changes in context
*
* Revision 1.5  2004/03/25 20:46:49  shomrat
* remove redundant include directive
*
* Revision 1.4  2004/03/05 22:02:47  shomrat
* fixed gathering of common name
*
* Revision 1.3  2004/02/11 22:56:14  shomrat
* use IsFormatDDBJ method
*
* Revision 1.2  2003/12/18 17:43:36  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:24:48  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
