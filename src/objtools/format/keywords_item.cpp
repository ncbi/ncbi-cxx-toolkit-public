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
#include <corelib/ncbistd.hpp>

#include <objects/seqblock/PIR_block.hpp>
#include <objects/seqblock/PRF_block.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objmgr/seqdesc_ci.hpp>

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


const list<string>& CKeywordsItem::GetKeywords(void) const 
{ 
    return m_Keywords; 
}


/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/


enum ETechFlags {
    eEST = 1<<0,
    eSTS = 1<<1,
    eGSS = 1<<2
};


void CKeywordsItem::x_GatherInfo(CBioseqContext& ctx)
{
    // add TPA keywords
    if ( ctx.IsTPA() ) {
        x_AddKeyword("Third Party Annotation");
        x_AddKeyword("TPA");
    }
    
    // add keywords based on mol-info
    Uint4 flags = 0;
    switch ( ctx.GetTech() ) {
    case CMolInfo::eTech_est:
        flags |= eEST;
        x_AddKeyword("EST");
        break;
        
    case CMolInfo::eTech_sts:
        flags |= eSTS;
        x_AddKeyword("STS");
        break;
        
    case CMolInfo::eTech_survey:
        flags |= eGSS;
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
        
    default:
        break;
    }
    
    for (CSeqdesc_CI it(ctx.GetHandle());  it;  ++it) {
        const list<string>* keywords = 0;
        
        switch (it->Which()) {
            
        case CSeqdesc::e_Pir:
            if ( it->GetPir().CanGetKeywords() ) {
                keywords = &(it->GetPir().GetKeywords());
            }
            break;
            
        case CSeqdesc::e_Genbank:
            if (it->GetGenbank().CanGetKeywords()) {
                keywords = &(it->GetGenbank().GetKeywords());
            }
            break;
            
        case CSeqdesc::e_Sp:
            if (it->GetSp().CanGetKeywords()) {
                keywords = &(it->GetSp().GetKeywords());
            }
            break;
            
        case CSeqdesc::e_Embl:
            if (it->GetEmbl().CanGetKeywords()) {
                keywords = &(it->GetEmbl().GetKeywords());
            }
            break;
            
        case CSeqdesc::e_Prf:
            if (it->GetPrf().CanGetKeywords()) {
                keywords = &(it->GetPrf().GetKeywords());
            }
            break;
            
        default:
            break;
        }
        
        if ( keywords ) {
            ITERATE ( list<string>, kwd, *keywords ) {
                if ( x_CheckSpecialKeyword(*kwd, flags) ) {
                    x_AddKeyword(*kwd);
                }
            }
        }
    }
}


// Add a keyword to the list 
void CKeywordsItem::x_AddKeyword(const string& keyword, Uint4 flags)
{
    list<string>::const_iterator begin = m_Keywords.begin();
    list<string>::const_iterator end = m_Keywords.end();
    if ( find(begin, end, keyword) == end ) {
        m_Keywords.push_back(keyword);
    }
}


// EST keywords
static const string s_EST_kw[] = {
  "EST", "EST PROTO((expressed sequence tag)", "expressed sequence tag",
  "EST (expressed sequence tag)", "EST(expressed sequence tag)",
  "partial cDNA sequence", "transcribed sequence fragment", "TSR",
  "putatively transcribed partial sequence", "UK putts"
};
static const string* s_EST_kw_end = s_EST_kw + 
    sizeof(s_EST_kw) / sizeof(string);

// GSS keywords
static const string s_GSS_kw[] = {
  "GSS", "trapped exon"
};
static const string* s_GSS_kw_end = s_GSS_kw + 
    sizeof(s_GSS_kw) / sizeof(string);

// STS keywords
static const string s_STS_kw[] = {
  "STS", "STS(sequence tagged site)", "STS (sequence tagged site)",
  "STS sequence", "sequence tagged site"
};
static const string* s_STS_kw_end = s_STS_kw + 
    sizeof(s_STS_kw) / sizeof(string);


bool CKeywordsItem::x_CheckSpecialKeyword(const string& keyword, Uint4 flags) const
{
    if ( flags & eEST ) {
        if ( find(s_STS_kw, s_STS_kw_end, keyword) != s_STS_kw_end ) {
            return false;
        }
        if ( find(s_GSS_kw, s_GSS_kw_end, keyword) != s_GSS_kw_end ) {
            return false;
        }
    }
    
    if ( flags & eSTS ) {
        if ( find(s_EST_kw, s_EST_kw_end, keyword) != s_EST_kw_end ) {
            return false;
        }
        if ( find(s_GSS_kw, s_GSS_kw_end, keyword) != s_GSS_kw_end ) {
            return false;
        }
    }
    
    if ( flags & eGSS ) {
        if ( find(s_EST_kw, s_EST_kw_end, keyword) != s_EST_kw_end ) {
            return false;
        }
        if ( find(s_STS_kw, s_STS_kw_end, keyword) != s_STS_kw_end ) {
            return false;
        }
    }
    
    return true;
}

END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
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
