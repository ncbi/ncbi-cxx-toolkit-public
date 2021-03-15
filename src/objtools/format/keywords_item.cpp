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
*   flat-file generator -- keywords item implementation
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seqblock/PIR_block.hpp>
#include <objects/seqblock/PRF_block.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <util/static_set.hpp>
#include <algorithm>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/context.hpp>
#include <objects/valid/Comment_set.hpp>
#include <objects/valid/Comment_rule.hpp>

#include <objects/misc/sequence_util_macros.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CKeywordsItem::CKeywordsItem(CBioseqContext& ctx) :
    CFlatItem(&ctx)
{
    x_GatherInfo(ctx);
}

IFlatItem::EItem CKeywordsItem::GetItemType(void) const
{
    return eItem_Keywords;
}

void CKeywordsItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const
{
    formatter.FormatKeywords(*this, text_os);
}


/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/


enum ETechFlags {
    e_not_set,
    eEST,
    eSTS,
    eGSS
};


// EST keywords
static const char* const sc_EST[] = {
  "EST", "EST (expressed sequence tag)", "EST PROTO((expressed sequence tag)",
  "EST(expressed sequence tag)", "TSR", "UK putts", "expressed sequence tag",
  "partial cDNA sequence", "putatively transcribed partial sequence",
  "transcribed sequence fragment"
};
typedef CStaticArraySet<const char*, PCase_CStr> TStaticKeywordSet;
DEFINE_STATIC_ARRAY_MAP(TStaticKeywordSet, sc_EST_kw, sc_EST);


// GSS keywords
static const char* const sc_GSS[] = {
  "GSS", "trapped exon"
};
DEFINE_STATIC_ARRAY_MAP(TStaticKeywordSet, sc_GSS_kw, sc_GSS);

// STS keywords
static const char* const sc_STS[] = {
  "STS", "STS (sequence tagged site)", "STS sequence", 
  "STS(sequence tagged site)", "sequence tagged site"
};
DEFINE_STATIC_ARRAY_MAP(TStaticKeywordSet, sc_STS_kw, sc_STS);


static bool s_CheckSpecialKeyword(const string& keyword, ETechFlags tech)
{
    if (tech == eEST) {
        if (sc_STS_kw.find(keyword.c_str()) != sc_STS_kw.end()) {
            return false;
        }
        if (sc_GSS_kw.find(keyword.c_str()) != sc_GSS_kw.end()) {
            return false;
        }
    }
    
    if (tech == eSTS) {
        if (sc_EST_kw.find(keyword.c_str()) != sc_EST_kw.end()) {
            return false;
        }
        if (sc_GSS_kw.find(keyword.c_str()) != sc_GSS_kw.end()) {
            return false;
        }
    }
    
    if (tech == eGSS) {
        if (sc_EST_kw.find(keyword.c_str()) != sc_EST_kw.end()) {
            return false;
        }
        if (sc_STS_kw.find(keyword.c_str()) != sc_STS_kw.end()) {
            return false;
        }
    }

    return true;
}


void CKeywordsItem::x_GatherInfo(CBioseqContext& ctx)
{
    switch( ctx.GetRepr() ) {
    case CSeq_inst::eRepr_map:
        x_AddKeyword("Whole_Genome_Map");
        break;
    default:
        // no action needed yet for other types
        break;
    }

    // check if env sample or metagenome_source
    bool is_env_sample = false;
    bool is_metagenome_source = false;
    CSeqdesc_CI src_desc(ctx.GetHandle(), CSeqdesc::e_Source);
    if (src_desc) {
        ITERATE(CBioSource::TSubtype, it, src_desc->GetSource().GetSubtype()) {
            if (! (*it)->IsSetSubtype()) continue;
            if ((*it)->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                is_env_sample = true;
            }
        }
        if (src_desc->GetSource().IsSetOrg()) {
            const CBioSource::TOrg& org = src_desc->GetSource().GetOrg();
            if ( org.IsSetOrgname()) {
                ITERATE (COrgName::TMod, it, org.GetOrgname().GetMod()) {
                    if (! (*it)->IsSetSubtype()) continue;
                    if ((*it)->GetSubtype() == COrgMod::eSubtype_metagenome_source) {
                        is_metagenome_source = true;
                    }
                }
            }
        }
    }

    // we might set this in the mol-info switch statement below
    bool is_tsa = false;

    // add keywords based on mol-info
    ETechFlags tech = e_not_set;
    // don't do tech-related keywords if molinfo isn't set
    if( ctx.GetMolinfo() != NULL ) {
        switch ( ctx.GetTech() ) {
        case CMolInfo::eTech_est:
            tech = eEST;
            x_AddKeyword("EST");
            if (is_env_sample) {
                x_AddKeyword("ENV");
            }
            break;

        case CMolInfo::eTech_sts:
            tech = eSTS;
            x_AddKeyword("STS");
            break;

        case CMolInfo::eTech_survey:
            tech = eGSS;
            x_AddKeyword("GSS");
            if (is_env_sample) {
                x_AddKeyword("ENV");
            }
            break;

        case CMolInfo::eTech_htgs_0:
            x_AddKeyword("HTG");
            x_AddKeyword("HTGS_PHASE0");
            break;

        case CMolInfo::eTech_htgs_1:
            x_AddKeyword("HTG");
            x_AddKeyword("HTGS_PHASE1");
            break;

        case CMolInfo::eTech_htgs_2:
            x_AddKeyword("HTG");
            x_AddKeyword("HTGS_PHASE2");
            break;

        case CMolInfo::eTech_htgs_3:
            x_AddKeyword("HTG");
            break;

        case CMolInfo::eTech_fli_cdna:
            x_AddKeyword("FLI_CDNA");
            break;

        case CMolInfo::eTech_htc:
            x_AddKeyword("HTC");
            break;

        case CMolInfo::eTech_wgs:
            x_AddKeyword("WGS");
            break;

        case CMolInfo::eTech_tsa:
            x_AddKeyword("TSA");
            x_AddKeyword("Transcriptome Shotgun Assembly");
            is_tsa = true; // remember so we don't add it twice
            break;

        case CMolInfo::eTech_targeted:
            x_AddKeyword("TLS");
            x_AddKeyword("Targeted Locus Study");
            break;

        case CMolInfo::eTech_unknown:
        case CMolInfo::eTech_standard:
        case CMolInfo::eTech_other:
            if (is_env_sample) {
                x_AddKeyword("ENV");
            }
            break;

        default:
            break;
        }
    }

    if (is_metagenome_source) {
        x_AddKeyword("Metagenome Assembled Genome");
        x_AddKeyword("MAG");
    }

    // propagate TSA keyword from nuc to prot in same nuc-prot set
    if( ! is_tsa && ctx.IsProt() && ctx.IsInNucProt() ) {
        CBioseq_set_Handle parent_bioseq_set = ctx.GetHandle().GetParentBioseq_set();
        if( parent_bioseq_set ) {
            CBioseq_CI bioseq_ci( parent_bioseq_set, CSeq_inst::eMol_na );
            if( bioseq_ci ) {
                CBioseq_Handle nuc = *bioseq_ci;
                if( nuc ) {
                    CSeqdesc_CI desc_ci( nuc, CSeqdesc::e_Molinfo );
                    for( ; desc_ci; ++desc_ci ) {
                        if( desc_ci->GetMolinfo().CanGetTech() &&
                            desc_ci->GetMolinfo().GetTech() == CMolInfo::eTech_tsa ) 
                        {
                            x_AddKeyword("TSA");
                            x_AddKeyword("Transcriptome Shotgun Assembly");
                            break;
                        }
                    }
                }
            }
        }
    }

    CBioseq_Handle bsh = ctx.GetHandle();
    for (CSeqdesc_CI di(bsh, CSeqdesc::e_User); di; ++di) {
        const CUser_object& usr = di->GetUser();
        if ( ! CComment_rule::IsStructuredComment (usr) ) continue;
        string pfx = CComment_rule::GetStructuredCommentPrefix ( usr, true );
        bool is_valid = false;
        CConstRef<CComment_set> comment_rules = CComment_set::GetCommentRules();
        if (comment_rules) {
            CConstRef<CComment_rule> ruler = comment_rules->FindCommentRuleEx(pfx);
            if (ruler) {
                const CComment_rule& rule = *ruler;
                CComment_rule::TErrorList errors = rule.IsValid(usr);
                if(errors.size() == 0) {
                    is_valid = true;
                }
            }
        }
        if ( is_valid ) {
            if ( NStr::EqualNocase (pfx, "MIGS:5.0-Data" )) {
                x_AddKeyword("GSC:MIxS");
                x_AddKeyword("MIGS:5.0.");
            } else if ( NStr::EqualNocase (pfx, "MIMS:5.0-Data" )) {
                x_AddKeyword("GSC:MIxS");
                x_AddKeyword("MIMS:5.0.");
            } else if ( NStr::EqualNocase (pfx, "MIMARKS:5.0-Data" )) {
                x_AddKeyword("GSC:MIxS");
                x_AddKeyword("MIMARKS:5.0.");
            } else if ( NStr::EqualNocase (pfx, "MISAG:5.0-Data" )) {
                x_AddKeyword("GSC:MIxS");
                x_AddKeyword("MISAG:5.0.");
            } else if ( NStr::EqualNocase (pfx, "MIMAG:5.0-Data" )) {
                x_AddKeyword("GSC:MIxS");
                x_AddKeyword("MIMAG:5.0.");
            } else if ( NStr::EqualNocase (pfx, "MIUVIG:5.0-Data" )) {
                x_AddKeyword("GSC:MIxS");
                x_AddKeyword("MIUVIG:5.0.");
            }
        }
        try {
            list<string> keywords = CComment_set::GetKeywords(usr);
            FOR_EACH_STRING_IN_LIST ( s_itr, keywords ) {
                x_AddKeyword(*s_itr);
            }
        } catch (CException) {
        }
    }

    CBioseqContext::TUnverified unv = ctx.GetUnverifiedType();
    if ((unv & CBioseqContext::fUnverified_SequenceOrAnnotation) != 0) {
        x_AddKeyword("UNVERIFIED");
    }
    if ((unv & CBioseqContext::fUnverified_Organism) != 0) {
        x_AddKeyword("UNVERIFIED");
        x_AddKeyword("UNVERIFIED_ORGANISM");
    }
    if ((unv & CBioseqContext::fUnverified_Misassembled) != 0) {
        x_AddKeyword("UNVERIFIED");
        x_AddKeyword("UNVERIFIED_MISASSEMBLY");
    }
    if ((unv & CBioseqContext::fUnverified_Contaminant) != 0) {
        x_AddKeyword("UNVERIFIED");
        x_AddKeyword("UNVERIFIED_CONTAMINANT");
    }

    if (ctx.IsEncode()) {
        x_AddKeyword("ENCODE");
    }

    if( ctx.IsGenomeAssembly() && ! ctx.GetFinishingStatus().empty() ) {
        x_AddKeyword( ctx.GetFinishingStatus() );
    }

    if ( ctx.IsTPA() ) {
        // add TPA keywords
        x_AddKeyword("Third Party Data");
        x_AddKeyword("TPA");
    } else if ( ctx.IsRefSeq() ) {
        // add RefSeq keyword
        x_AddKeyword("RefSeq");
    }

    if ( ctx.IsCrossKingdom() && ctx.IsRSUniqueProt() ) {
        // add CrossKingdom keyword
        x_AddKeyword("CROSS_KINGDOM");
    }

    for (CSeqdesc_CI it(ctx.GetHandle());  it;  ++it) {
        const list<string>* keywords = NULL;
        
        switch (it->Which()) {
            
        case CSeqdesc::e_Pir:
            keywords = &(it->GetPir().GetKeywords());
            break;
            
        case CSeqdesc::e_Genbank:
            keywords = &(it->GetGenbank().GetKeywords());
            break;
            
        case CSeqdesc::e_Sp:
            keywords = &(it->GetSp().GetKeywords());
            break;
            
        case CSeqdesc::e_Embl:
            keywords = &(it->GetEmbl().GetKeywords());
            break;
            
        case CSeqdesc::e_Prf:
            keywords = &(it->GetPrf().GetKeywords());
            break;
        
        default:
            keywords = NULL;
            break;
        }
        
        if (keywords != NULL) {
            if (!IsSetObject()) {
                x_SetObject(*it);
            }
            ITERATE (list<string>, kwd, *keywords) {
                if (s_CheckSpecialKeyword(*kwd, tech)) {
                    x_AddKeyword(*kwd);
                }
            }
        }
    }
}


// Add a keyword to the list
static bool x_OkayToAddKeyword(const string& keyword, vector<string> keywords)
{
    ITERATE (vector<string>, it, keywords) {
        if (NStr::EqualNocase(keyword, *it)) {
            return false;
        }
    }
    return true;
}
void CKeywordsItem::x_AddKeyword(const string& keyword)
{
    list<string> kywds;
    NStr::Split( keyword, ";", kywds, NStr::fSplit_Tokenize );
    FOR_EACH_STRING_IN_LIST ( k_itr, kywds ) {
        const string& kw = *k_itr;
        if (x_OkayToAddKeyword (kw, m_Keywords)) {
            m_Keywords.push_back(kw);
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
