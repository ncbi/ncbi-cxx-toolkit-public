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
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Imprint.hpp>
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
#include <objtools/format/ftable_formatter.hpp>
#include <objtools/format/gbseq_formatter.hpp>
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


CFlatItemFormatter* CFlatItemFormatter::New(CFlatFileConfig::TFormat format)
{
    switch ( format ) {
    case CFlatFileConfig::eFormat_GenBank:
        return new CGenbankFormatter;
        
    case CFlatFileConfig::eFormat_EMBL:
        return new CEmblFormatter;

    case CFlatFileConfig::eFormat_GFF:
        return new CGFFFormatter;

    case CFlatFileConfig::eFormat_FTable:
        return new CFtableFormatter;

    case CFlatFileConfig::eFormat_GBSeq:
        return new CGBSeqFormatter;

    case CFlatFileConfig::eFormat_DDBJ:
    default:
        NCBI_THROW(CFlatException, eNotSupported, 
            "This format is currently not supported");
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

    CBioseqContext& ctx = *acc.GetContext();

    const string& primary = ctx.IsHup() ? ";" : acc.GetAccession();
    
    acc_line << primary;

    if ( ctx.IsWGS() ) {
        acc_line << separator << acc.GetWGSAccession();
    }

    ITERATE (CAccessionItem::TExtra_accessions, it, acc.GetExtraAccessions()) {
        acc_line << separator << *it;
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
 CBioseqContext& ctx) const
{
    const string* delim_p = &kEmptyStr;
    CScope& scope = ctx.GetScope();

    os << (ctx.IsProt() ? "(residues " : "(bases ");
    for ( CSeq_loc_CI it(loc);  it;  ++it ) {
        CSeq_loc_CI::TRange range = it.GetRange();
        if ( range.IsWhole() ) {
            range.SetTo(sequence::GetLength(it.GetSeq_id(), &scope) - 1);
        }
        
        os << *delim_p << range.GetFrom() + 1 << to << range.GetTo() + 1;
        delim_p = &delim;
    }
    os << ')';
}


static size_t s_NumAuthors(const CCit_book::TAuthors& authors)
{
    if ( authors.CanGetNames() ) {
        const CAuth_list::C_Names& names = authors.GetNames();
        switch ( names.Which() ) {
        case CAuth_list::C_Names::e_Std:
            return names.GetStd().size();
        case CAuth_list::C_Names::e_Ml:
            return names.GetMl().size();
        case CAuth_list::C_Names::e_Str:
            return names.GetStr().size();
        default:
            break;
        }
    }

    return 0;
}


void CFlatItemFormatter::x_FormatRefJournal
(string& journal,
 const CReferenceItem& ref) const
{
    if ( ref.GetBook() == 0 ) {  // not from a book
        
        switch ( ref.GetCategory() ) {
        case CReferenceItem::eSubmission:
            journal = "Submitted ";
            if ( ref.GetDate() != 0 ) {
                journal += '(';
                DateToString(*ref.GetDate(), journal, true);
                journal += ") ";
            }
            journal += ref.GetJournal();
            break;
        case CReferenceItem::eUnpublished:
            journal = ref.GetJournal();
            break;
        case CReferenceItem::ePublished:
            journal = ref.GetJournal();
            if ( !ref.GetVolume().empty() ) {
                journal += " " + ref.GetVolume();
            }
            if ( !ref.GetIssue().empty() ) {
                journal += " (" + ref.GetIssue() + ')';
            }
            if ( !ref.GetPages().empty() ) {
                journal += ", " + ref.GetPages();
            }
            if ( ref.GetDate() != 0 ) {
                ref.GetDate()->GetDate(&journal, " (%Y)");
            }
            break;
        default:
            break;
        }
        if ( ref.GetPrepub() == CImprint::ePrepub_in_press ) {
            journal += " In press";
        }

        if ( journal.empty() ) {
            journal = "Unpublished";
        }
    } else {
        while ( true ) {
            const CCit_book& book = *ref.GetBook();
            _ASSERT(book.CanGetImp()  &&  book.CanGetTitle());
            
            string year;

            if ( ref.GetDate() != 0 ) {
                ref.GetDate()->GetDate(&year, "(%Y)");
            }
            
            if ( ref.GetCategory() == CReferenceItem::eUnpublished ) {
                journal = "Unpublished " + year;
                break;
            }
            
            string title = book.GetTitle().GetTitle();
            if ( title.length() < 3 ) {
                journal = ".";
                break;
            }

            CNcbiOstrstream jour;
            jour << "(in) ";
            if ( book.CanGetAuthors() ) {
                const CCit_book::TAuthors& auth = book.GetAuthors();
                jour << CReferenceItem::GetAuthString(&auth);
                size_t num_auth = s_NumAuthors(auth);
                jour << ((num_auth == 1) ? " (Ed.);" : " (Eds.);") << endl;
            }
            jour << NStr::ToUpper(title);
            if ( !ref.GetVolume().empty()  &&  ref.GetVolume() != "0" ) {
                jour << " " << ref.GetVolume();
            }

            if ( !ref.GetIssue().empty() ) {
                jour << " " << ref.GetIssue();
            }

            if ( !ref.GetPages().empty() ) {
                jour << ": " << ref.GetPages();
            }

            jour << ';' << endl;

            const CCit_book::TImp& imp = book.GetImp();
            if ( imp.CanGetPub() ) {
                string affil;
                CReferenceItem::FormatAffil(imp.GetPub(), affil);
                if ( !affil.empty() ) {
                    jour << affil << ' ';
                }
            }

            jour << year;

            if ( ref.GetPrepub() == CImprint::ePrepub_in_press ) {
                jour << " In press";
            }

            journal = CNcbiOstrstreamToString(jour);
            break;
        }
    }
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
* Revision 1.10  2004/05/06 17:57:22  shomrat
* Fixed Accession and RefLocation formatting
*
* Revision 1.9  2004/04/27 15:12:50  shomrat
* fixed Journal formatting
*
* Revision 1.8  2004/04/22 16:00:58  shomrat
* Changes in context
*
* Revision 1.7  2004/04/13 16:48:39  shomrat
* Added Journal formatting (from fenbank_formatter)
*
* Revision 1.6  2004/04/07 14:51:24  shomrat
* Fixed typo
*
* Revision 1.5  2004/04/07 14:28:19  shomrat
* Added FTable format
*
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
