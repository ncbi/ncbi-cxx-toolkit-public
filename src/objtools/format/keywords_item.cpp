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
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqblock/PIR_block.hpp>
#include <objects/seqblock/PRF_block.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <util/static_set.hpp>
#include <algorithm>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CKeywordsItem::CKeywordsItem(CBioseqContext& ctx) :
    CFlatItem(&ctx)
{
    x_GatherInfo(ctx);
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
static const string sc_EST[] = {
  "EST", "EST (expressed sequence tag)", "EST PROTO((expressed sequence tag)",
  "EST(expressed sequence tag)", "TSR", "UK putts", "expressed sequence tag",
  "partial cDNA sequence", "putatively transcribed partial sequence",
  "transcribed sequence fragment"
};
static const CStaticArraySet<string> sc_EST_kw(sc_EST, sizeof(sc_EST));


// GSS keywords
static const string sc_GSS[] = {
  "GSS", "trapped exon"
};
static const CStaticArraySet<string> sc_GSS_kw(sc_GSS, sizeof(sc_GSS));

// STS keywords
static const string sc_STS[] = {
  "STS", "STS (sequence tagged site)", "STS sequence", 
  "STS(sequence tagged site)", "sequence tagged site"
};
static const CStaticArraySet<string> sc_STS_kw(sc_STS, sizeof(sc_STS));


static bool s_CheckSpecialKeyword(const string& keyword, ETechFlags tech)
{
    if (tech == eEST) {
        if (sc_STS_kw.find(keyword) != sc_STS_kw.end()) {
            return false;
        }
        if (sc_GSS_kw.find(keyword) != sc_GSS_kw.end()) {
            return false;
        }
    }
    
    if (tech == eSTS) {
        if (sc_EST_kw.find(keyword) != sc_EST_kw.end()) {
            return false;
        }
        if (sc_GSS_kw.find(keyword) != sc_GSS_kw.end()) {
            return false;
        }
    }
    
    if (tech == eGSS) {
        if (sc_EST_kw.find(keyword) != sc_EST_kw.end()) {
            return false;
        }
        if (sc_STS_kw.find(keyword) != sc_STS_kw.end()) {
            return false;
        }
    }

    return true;
}


void CKeywordsItem::x_GatherInfo(CBioseqContext& ctx)
{
    // add TPA keywords
    if ( ctx.IsTPA() ) {
        x_AddKeyword("Third Party Annotation");
        x_AddKeyword("TPA");
    }
    
    // add keywords based on mol-info
    ETechFlags tech = e_not_set;
    switch ( ctx.GetTech() ) {
    case CMolInfo::eTech_est:
        tech = eEST;
        x_AddKeyword("EST");
        break;
        
    case CMolInfo::eTech_sts:
        tech = eSTS;
        x_AddKeyword("STS");
        break;
        
    case CMolInfo::eTech_survey:
        tech = eGSS;
        x_AddKeyword("GSS");
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
        
    case CMolInfo::eTech_barcode:
        x_AddKeyword("BARCODE");
        break;

    default:
        break;
    }

    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_User);  it;  ++it) {
        const CUser_object& uo = it->GetUser();
        if (uo.IsSetType()  &&  uo.GetType().IsStr()) {
            if (NStr::EqualNocase(uo.GetType().GetStr(), "ENCODE")) {
                x_AddKeyword("ENCODE");
                break;
            }
        }
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
void CKeywordsItem::x_AddKeyword(const string& keyword)
{
    ITERATE (TKeywords, it, m_Keywords) {
        if (NStr::EqualNocase(keyword, *it)) {
            return;
        }
    }
    m_Keywords.push_back(keyword);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.10  2005/04/15 14:03:06  shomrat
* Added ENCODE
*
* Revision 1.9  2004/10/18 17:48:43  shomrat
* Bug Fix
*
* Revision 1.8  2004/10/05 15:53:14  shomrat
* Add only unique keywords (case insensitive test)
*
* Revision 1.7  2004/09/01 19:58:02  shomrat
* Add BARCODE
*
* Revision 1.6  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2004/04/22 15:58:42  shomrat
* Changes in context
*
* Revision 1.4  2004/03/31 17:14:42  shomrat
* EFlags -> ETechFlags
*
* Revision 1.3  2004/03/25 20:44:42  shomrat
* remove redundant include directive
*
* Revision 1.2  2003/12/18 17:43:35  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:23:26  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
