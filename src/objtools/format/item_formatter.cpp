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
*           
*
*/
#include <corelib/ncbistd.hpp>

#include <objmgr/util/sequence.hpp>

#include <objtools/format/items/item.hpp>
#include <objtools/format/item_formatter.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/genbank_formatter.hpp>
#include <objtools/format/embl_formatter.hpp>
#include <objtools/format/gff_formatter.hpp>
#include <objtools/format/context.hpp>
#include <objtools/format/flat_expt.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// static members
const string CFlatItemFormatter::s_GenbankMol[] = {
    "    ", "DNA ", "RNA ", "mRNA", "rRNA", "tRNA", "snRNA",
    "scRNA", " AA ", "DNA ", "DNA ", "RNA ", "snoRNA", "RNA "
};


CFlatItemFormatter* CFlatItemFormatter::New(TFormat format)
{
    switch ( format ) {
    case eFormat_GenBank:
        return new CGenbankFormatter;
        
    case eFormat_EMBL:
        return new CEmblFormatter;

    case eFormat_GFF:
        return new CGFFFormatter;

    case eFormat_DDBJ:
    case eFormat_GBSeq:
    case eFormat_FTable:
    default:
        NCBI_THROW(CFlatException, eNotSupported, 
            "This format is currently not supported");
        break;
    }

    return 0;
}

    
CFlatItemFormatter::~CFlatItemFormatter(void)
{
}


void CFlatItemFormatter::Format(const IFlatItem& item, IFlatTextOStream& text_os)
{
    item.Format(*this, text_os);
}


string  CFlatItemFormatter::x_FormatAccession
(const CAccessionItem& acc,
 char separator) const
{
    CNcbiOstrstream acc_line;

    const CFFContext& ctx = acc.GetContext();

    const string& primary = ctx.IsHup() ? ";" : 
        ctx.GetPrimaryId()->GetSeqIdString(false);
    
    acc_line << primary;

    ITERATE(list<string>, str, acc.GetExtraAccessions()) {
        if ( ValidateAccession(*str) ) {
            acc_line << separator << *str;
        }
    }

    return CNcbiOstrstreamToString(acc_line);
}


string& CFlatItemFormatter::x_Pad(const string& s, string& out, SIZE_TYPE width,
                                const string& indent)
{
    out.assign(indent);
    out += s;
    out.resize(width, ' ');
    return out;
}


string& CFlatItemFormatter::Pad(const string& s, string& out,
                                EPadContext where) const
{
    switch (where) {
    case ePara:      return x_Pad(s, out, 12);
    case eSubp:      return x_Pad(s, out, 12, string(2, ' '));
    case eFeatHead:  return x_Pad(s, out, 21);
    case eFeat:      return x_Pad(s, out, 21, string(5, ' '));
    default:         return out; // shouldn't happen, but some compilers whine
    }
}


list<string>& CFlatItemFormatter::Wrap
(list<string>& l,
 SIZE_TYPE width,
 const string& tag,
 const string& body,
 EPadContext where) const
{
    NStr::TWrapFlags flags = /* ??? NStr::fWrap_HTMLPre :*/ 0;
    string tag2;
    Pad(tag, tag2, where);
    NStr::Wrap(body, width, l, flags,
               where == eFeat ? m_FeatIndent : m_Indent, tag2);
    return l;
}


list<string>& CFlatItemFormatter::Wrap
(list<string>& l,
 const string& tag,
 const string& body,
 EPadContext where) const
{
    NStr::TWrapFlags flags = /* ??? NStr::fWrap_HTMLPre :*/ 0;
    string tag2;
    Pad(tag, tag2, where);
    NStr::Wrap(body, GetWidth(), l, flags,
               where == eFeat ? m_FeatIndent : m_Indent, tag2);
    return l;
}



void CFlatItemFormatter::x_FormatRefLocation
(CNcbiOstrstream& os,
 const CSeq_loc& loc,
 const string& to,
 const string& delim,
 bool is_prot,
 CScope& scope) const
{
    const string* delim_p = &kEmptyStr;

    os << (is_prot ? "(residues " : "(bases ");
    for ( CSeq_loc_CI it(loc);  it;  ++it ) {
        CSeq_loc_CI::TRange range = it.GetRange();
        if ( it.IsWhole() ) {
            range.SetTo(sequence::GetLength(it.GetSeq_id(), &scope) - 1);
        }
        if ( it.IsPoint() ) {
            os << range.GetFrom() + 1;
        } else {
            os << *delim_p << range.GetFrom() + 1 << to << range.GetTo() + 1;
        }
        delim_p = &delim;
    }
    os << ')';
}


void CFlatItemFormatter::x_GetKeywords
(const CKeywordsItem& kws,
 const string& prefix,
 list<string>& l) const
{
    list<string> kw;
    CKeywordsItem::TKeywords::const_iterator last = --(kws.GetKeywords().end());
    ITERATE (CKeywordsItem::TKeywords, it, kws.GetKeywords()) {
        kw.push_back(*it + (it == last ? '.' : ';'));
    }
    if ( kw.empty() ) {
        kw.push_back(".");
    }
    string  str;
    Pad(prefix, str, ePara);
    NStr::WrapList(kw, GetWidth(), " ", l, 0, GetIndent(), &str);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2004/02/11 22:54:00  shomrat
* using types in flag file
*
* Revision 1.3  2004/01/14 16:17:42  shomrat
* added support for GFF formatter
*
* Revision 1.2  2003/12/18 17:43:35  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:23:02  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
