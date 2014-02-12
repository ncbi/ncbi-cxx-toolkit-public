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
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Id_pat.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/items/item.hpp>
#include <objtools/format/item_formatter.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/genbank_formatter.hpp>
#include <objtools/format/embl_formatter.hpp>
#include <objtools/format/gff3_formatter.hpp>
#include <objtools/format/ftable_formatter.hpp>
#include <objtools/format/gbseq_formatter.hpp>
#include <objtools/format/context.hpp>
#include <objtools/format/flat_expt.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// static members
const string CFlatItemFormatter::s_GenbankMol[] = {
    "    ", "DNA ", "RNA ", "mRNA", "rRNA", "tRNA", /* "snRNA" */ "RNA", /* "scRNA" */ "RNA",
    " AA ", "DNA ", "DNA ", "cRNA ", /* "snoRNA" */ "RNA", "RNA ", "RNA ", "tmRNA "
};

// static members
const string CFlatItemFormatter::s_EmblMol[] = {
    "xxx", "DNA", "RNA", "RNA", "RNA", "RNA", "RNA",
    "RNA", "AA ", "DNA", "DNA", "RNA", "RNA", "RNA"
};


CFlatItemFormatter* CFlatItemFormatter::New(CFlatFileConfig::TFormat format)
{
    switch ( format ) {
    case CFlatFileConfig::eFormat_GenBank:
    case CFlatFileConfig::eFormat_FeaturesOnly:
        return new CGenbankFormatter;
        
    case CFlatFileConfig::eFormat_EMBL:
        return new CEmblFormatter;

    case CFlatFileConfig::eFormat_GFF:
        return new CGFFFormatter;

    case CFlatFileConfig::eFormat_GFF3:
        return new CGFF3_Formatter;

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

void CFlatItemFormatter::SetContext(CFlatFileContext& ctx)
{
    m_Ctx.Reset(&ctx);
    if (ctx.GetConfig().DoHTML()) {
        SetWrapFlags() |= NStr::fWrap_HTMLPre;
    }
}


CFlatItemFormatter::~CFlatItemFormatter(void)
{
}


void CFlatItemFormatter::Format(const IFlatItem& item, IFlatTextOStream& text_os)
{
    item.Format(*this, text_os);
}


static void s_PrintAccessions
(CNcbiOstream& os,
 const vector<string>& accs,
 char separator)
{
    ITERATE (CAccessionItem::TExtra_accessions, it, accs) {
        os << separator <<*it;
    }
}


static bool s_IsSuccessor(const string& acc, const string& prev)
{
    if (acc.length() != prev.length()) {
        return false;
    }
    size_t i;
    for (i = 0; i < acc.length()  &&  !isdigit((unsigned char) acc[i]); ++i) {
        if (acc[i] != prev[i]) {
            return false;
        }
    }
    if (i < acc.length()) {
        if (NStr::StringToUInt(acc.substr(i)) == NStr::StringToUInt(prev.substr(i)) + 1) {
            return true;
        }
    }
    return false;
}

static void s_FormatSecondaryAccessions
(CNcbiOstream& os,
 const CAccessionItem::TExtra_accessions& xtra,
 char separator)
{
    static const size_t kAccCutoff = 20;
    static const size_t kBinCutoff = 5;

    if (xtra.size() < kAccCutoff) {
        s_PrintAccessions(os, xtra, separator);
        return;
    }
    _ASSERT(!xtra.empty());

    typedef vector<string>      TAccBin;
    typedef vector <TAccBin>    TAccBins;
    TAccBins bins;
    TAccBin* curr_bin = NULL;

    // populate the bins
    CAccessionItem::TExtra_accessions::const_iterator prev = xtra.begin();
    ITERATE (CAccessionItem::TExtra_accessions, it, xtra) {
        if (!s_IsSuccessor(*it, *prev) || NStr::EndsWith( *prev, "000000" ) ) {
            bins.push_back(TAccBin());
            curr_bin = &bins.back();
        }
        curr_bin->push_back(*it);
        prev = it;
    }
    
    ITERATE (TAccBins, bin_it, bins) {
        if (bin_it->size() <= kBinCutoff) {
            s_PrintAccessions(os, *bin_it, separator);
        } else {
            os << separator<< bin_it->front() << '-' << bin_it->back();
        }
    }
}


string  CFlatItemFormatter::x_FormatAccession
(const CAccessionItem& acc,
 char separator) const
{
    CNcbiOstrstream acc_line;

    CBioseqContext& ctx = *acc.GetContext();

    const string& primary = ctx.IsHup() ? ";" : acc.GetAccession();
    
    acc_line << primary;

    if ( ctx.IsWGS() && ! acc.GetWGSAccession().empty() ) {
        const bool is_html = ctx.Config().DoHTML();
        if( is_html ) {
            acc_line << separator << "<a href=\"" << strLinkBaseNucSearch << acc.GetWGSAccession() << 
                "\">" << acc.GetWGSAccession() << "</a>";
        } else {
            acc_line << separator << acc.GetWGSAccession();
        }
    }

    if ( ctx.IsTSA() && ! acc.GetTSAAccession().empty() ) {
        const bool is_html = ctx.Config().DoHTML();
        if( is_html ) {
            acc_line << separator << "<a href=\"" << strLinkBaseNucSearch << acc.GetTSAAccession() << 
                "\">" << acc.GetTSAAccession() << "</a>";
        } else {
            acc_line << separator << acc.GetTSAAccession();
        }
    }

    if (!acc.GetExtraAccessions().empty()) {
        s_FormatSecondaryAccessions(acc_line, acc.GetExtraAccessions(), separator);
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
    case eBarcode:   return x_Pad(s, out, 35, string(16, ' '));
    default:         return out; // shouldn't happen, but some compilers whine
    }
}


list<string>& CFlatItemFormatter::Wrap
(list<string>& l,
 SIZE_TYPE width,
 const string& tag,
 const string& body,
 EPadContext where,
 bool htmlaware) const
{
    string tag2;
    Pad(tag, tag2, where);
    const string& indent = (where == eFeat ? m_FeatIndent : m_Indent);
    int flags = m_WrapFlags;
    if ( htmlaware ) {
        flags |= NStr::fWrap_HTMLPre;
    }
    NStr::Wrap(body, width, l, flags, indent, tag2);
    NON_CONST_ITERATE (list<string>, it, l) {
        TrimSpaces(*it, indent.length());
    }
    return l;
}


list<string>& CFlatItemFormatter::Wrap
(list<string>& l,
 const string& tag,
 const string& body,
 EPadContext where,
 bool htmlaware,
 int internalIndentation ) const
{
    string padded_tag;
    Pad(tag, padded_tag, where);
    const string* indent = &m_Indent;  // default
    if (where == eFeat) {
        indent = &m_FeatIndent;
    } else if (where == eBarcode) {
        indent = &m_BarcodeIndent;
    }

    int flags = m_WrapFlags;
    if ( htmlaware ) {
        flags |= NStr::fWrap_HTMLPre;
    }
    if (body.empty()) {
        l.push_back(padded_tag);
    } else {
        if( internalIndentation >  0 ) {
            string padded_indent = *indent;
            padded_indent.resize( padded_indent.size() + internalIndentation, ' ');
            NStr::Wrap(body, GetWidth(), l, flags, padded_indent, padded_tag );
        } else {
            NStr::Wrap(body, GetWidth(), l, flags, *indent, padded_tag );
        }
    }
    NON_CONST_ITERATE (list<string>, it, l) {
        TrimSpaces(*it, indent->length());
    }
    return l;
}


void
CFlatItemFormatter::Start(
    IFlatTextOStream& Out )
{
    const string strHtmlHead(
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n"
        "    \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
        "<html lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">\n"
        "<head>\n"
        "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=us-ascii\" />"
        "<title>GenBank entry</title>\n"
        "</head>\n"
        "<body>\n"
        "<hr /><div class=\"sequence\">" );

    const string strCgiHead("<div class=\"sequence\">");

    //mss-249
    if (m_Ctx->GetConfig().DoHTML()) {
        m_Ctx->GetConfig().IsModeEntrez() ?
            Out.AddLine( strCgiHead ) :
            Out.AddLine( strHtmlHead );
    }
};


void
CFlatItemFormatter::End(
    IFlatTextOStream& Out )
{
    const string strHtmlTail(
        "</div><hr />\n"
        "</body>\n"
        "</html>" );
    const string strCgiTail( "</div>" );

    //mss-249
    if ( m_Ctx->GetConfig().DoHTML() ) {
        m_Ctx->GetConfig().IsModeEntrez() ?
            Out.AddLine( strCgiTail ) :
            Out.AddLine( strHtmlTail );
    }
};


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
    if (authors.IsSetNames()) {
        const CAuth_list::C_Names& names = authors.GetNames();
        switch (names.Which()) {
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


static void s_FormatYear(const CDate& date, string& year)
{
    if (date.IsStr() && ! date.GetStr().empty() && date.GetStr() != "?" ) {
        year += '(';
        year += date.GetStr();
        year += ')';
    } else if (date.IsStd()  && date.GetStd().IsSetYear()  &&
        date.GetStd().GetYear() != 0) {
        date.GetDate(&year, "(%Y)");
    }
}


static string s_DoSup(const string& issue, const string& part_sup, const string& part_supi)
{
    string str;

    if (!NStr::IsBlank(part_sup)) {
        str += " (";
        str += part_sup;
        str += ')';
    }

    if (NStr::IsBlank(issue)  &&  NStr::IsBlank(part_supi)) {
        return str;
    }

    str += " (";
    string sep;
    if (!NStr::IsBlank(issue)) {
        str += issue;
        sep = " ";
    }
    if (!NStr::IsBlank(part_supi)) {
        str += sep;
        str += part_supi;
    }
    str += ')';

    return str;
}


static void s_FixPages( string& pages )
//
//  Note: The following code is written to be mostly feature for feature and
//  bug for bug compatible with the C toolkit version. 
//
{
    const char* digits = "0123456789";
    string::iterator it;
    string firstText, firstNumber, dash, lastText, lastNumber;

    //
    //  Test 1:
    //  Don't touch that string if it doesn't have any digits in it.
    //
    size_t firstDigit = pages.find_first_of( digits );
    if ( NPOS == firstDigit ) {
        return;
    }

    //
    //  If the string starts with a digit, try to parse input into
    //  firstNumber firstText [ dash lastNumber lastText ] :
    //
    if ( 0 == firstDigit ) {
        it = pages.begin();
        while ( it != pages.end() ) {
            if ( ::isdigit( *it ) ) {
                firstNumber += *it;
            }
            else if ( *it != ' ' ) {
                break;
            }
            ++it;
        }
        while ( it != pages.end() ) {
            if ( ::isalpha( *it ) || ' ' == *it ) {
                firstText += *it;
            }
            else {
                break;
            }
            ++it;
        }

        //
        // If we covered the entire page string then we are obviously done. If
        // we are at anything other than a dash then we take what we got and
        // give up on the rest.  Likewise if we __are__ at a dash but nothing
        // follows:
        //
        if ( it == pages.end() ) {
            pages  = firstNumber;
            pages += firstText;
            return;
        }
        if( it != pages.end() && ::isalnum(*it) ) {
            // E.g. NM_002638
            return;
        }
        if ( it != pages.end() && *it != '-' ) {
            pages = firstNumber;
            pages += firstText;
            return;
        } else {
            ++it;
            string::iterator locationRightAfterFirstDash = it;
            while( it != pages.end() && *it == '-' ) {
                ++it;
            }
            // if pages ends in a dash, then duplicate the first part and get out of here
            // (e.g. AF003826)
            if( it == pages.end() ) {
                // burn off extra dashes, if any
                pages.erase( locationRightAfterFirstDash, pages.end() );

                pages += firstNumber;
                pages += firstText;
                return;
            }
        }
        if ( it == pages.end() ) {
            pages = firstNumber;
            pages += firstText;
            return;
        }

        while ( it != pages.end() ) {
            if ( ::isdigit( *it ) ) {
                lastNumber += *it;
            }
            else if ( *it != ' ' ) {
                break;
            }
            ++it;
        }

        while ( it != pages.end() ) {
            if ( ::isalpha( *it ) || ' ' == *it ) {
                lastText += *it;
            }
            else {
                break;
            }
            ++it;
        }

        const bool firstNumberEmpty = firstNumber.empty();
        const bool firstTextEmpty = firstText.empty();
        const bool lastNumberEmpty = lastNumber.empty();
        const bool lastTextEmpty = lastText.empty();
        
        if( (lastNumberEmpty  && firstTextEmpty) ||
            (firstNumberEmpty && lastTextEmpty) ) {
            return;
        }

        if ( it != pages.end() ) {
            if( *it == '?' ) {
                lastNumber = firstNumber;
                lastText = firstText;
                pages = firstNumber + firstText + "-" + lastNumber + lastText;
            } else if( ! isalnum(*it) ) {
                pages = firstNumber + firstText + "-" + lastNumber + lastText;
            }
            return;
        }
    }

    //
    //  Otherwise, try to parse input into 
    //  firstText firstNumber [ dash lastText lastNumber ] :
    //
    else {
        it = pages.begin();
        while ( it != pages.end() ) {
            if ( ::isalpha( *it ) || ' ' == *it ) {
                firstText += *it;
            }
            else {
                break;
            }
            ++it;
        }
        while ( it != pages.end() ) {
            if ( ::isdigit( *it ) ) {
                firstNumber += *it;
            }
            else if ( *it != ' ' ) {
                break;
            }
            ++it;
        }
        if ( it == pages.end() ) {
            return;
        }

        if ( it != pages.end() && *it != '-' ) {
            return;
        }
        ++it;
        if ( it == pages.end() ) {
            return;
        }

        while ( it != pages.end() ) {
            if ( ::isalpha( *it ) || ' ' == *it ) {
                lastText += *it;
            }
            else {
                break;
            }
            ++it;
        }
        while ( it != pages.end() ) {
            if ( ::isdigit( *it ) ) {
                lastNumber += *it;
            }
            else if ( *it != ' ' ) {
                break;
            }
            ++it;
        }

        if ( it != pages.end() ) {
//            pages = "";
            return;
        }
    }

    //
    //  The textual part of the page number must be a single letter.
    //  If there is both a first and a last page then the two textual
    //  parts must match.
    //
    if ( ! firstText.empty() ) {
        if ( firstText.length() != 1 ) {
            return;
        }
    }
    if ( ! lastText.empty() ) {
        if ( lastText != firstText ) {
            return;
        }
    }

    //
    //  If we are dealing with a single page rather than a page range then
    //  we are ready to produce the final output:
    //
    if ( lastText.empty() && lastNumber.empty() ) {
        pages = ( (0 == firstDigit) ? 
            (firstNumber + firstText) : (firstText + firstNumber) );
        return;
    }

    //
    //  In test ranges, the first page has a numeric part if and only if 
    //  the the last page has.
    //
    if ( firstNumber.empty() && ! lastNumber.empty() ) {
        return;
    }
    if ( ! firstNumber.empty() && lastNumber.empty() ) {
        return;
    }

    //
    //  Normalize last page number by prepending any implied leading digits.
    //  Normalize empty last page text by making it equal to the first page
    //  text.
    //
    if ( lastNumber.length() < firstNumber.length() ) {
        lastNumber = firstNumber.substr( 0, 
            firstNumber.length() - lastNumber.length() ) + lastNumber;
    }
    if ( lastText.empty() ) {
        lastText = firstText;
    }

    //
    //  Make sure the numerical part of the last page is no less than the
    //  numerical part of the first page:
    //
    if ( NStr::StringToULong( firstNumber ) > NStr::StringToULong( lastNumber ) ) {
        return;
    }

    //
    //  Finally ready to produce the output string:
    //
    if ( 0 == firstDigit ) {
        pages = firstNumber + firstText + "-" + lastNumber + lastText;
    }
    else {
        pages = firstText + firstNumber + "-" + lastText + lastNumber;
    }
    return;
}

static void s_FormatCitBookArt(const CReferenceItem& ref, string& journal, bool do_gb)
{
    _ASSERT(ref.IsSetBook());
    _ASSERT(ref.GetBook().IsSetImp()  &&  ref.GetBook().IsSetTitle());

    const CCit_book&         book = ref.GetBook();
    const CCit_book::TImp&   imp  = book.GetImp();
    const CCit_book::TTitle& ttl  = book.GetTitle();

    journal.erase();

    // format the year
    string year;
    if (imp.IsSetDate()) {
        s_FormatYear(imp.GetDate(), year);
        if( year.empty() ) {
            year = "(?)";
        }
    }

    if (imp.IsSetPrepub()) {
        CImprint::TPrepub prepub = imp.GetPrepub();
        if (prepub == CImprint::ePrepub_submitted  ||  prepub == CImprint::ePrepub_other) {
            journal = "Unpublished";
            journal += year;
            return;
        }
    }

    string title = ttl.GetTitle();
    if (title.length() < 3) {
        journal = ".";
        return;
    }

    CNcbiOstrstream jour;
    jour << "(in) ";
    if (book.CanGetAuthors()) {
        const CCit_book::TAuthors& auth = book.GetAuthors();
        string authstr;
        CReferenceItem::FormatAuthors(auth, authstr);
        if (!authstr.empty()) {
            jour << authstr;
            size_t num_auth = s_NumAuthors(auth);
            jour << ((num_auth == 1) ? " (Ed.);" : " (Eds.);") << '\n';
        }
    }
    jour << NStr::ToUpper(title);

    string issue, part_sup, part_supi;
    if (do_gb) {
        issue = imp.IsSetIssue() ? imp.GetIssue(): kEmptyStr;
        part_sup = imp.IsSetPart_sup() ? imp.GetPart_sup() : kEmptyStr;
        part_supi = imp.IsSetPart_supi() ? imp.GetPart_supi() : kEmptyStr;
    }

    string volume = imp.IsSetVolume() ? imp.GetVolume() : kEmptyStr;
    if (!NStr::IsBlank(volume)  &&  volume != "0") {
        jour << ", Vol. " << volume;
        jour << s_DoSup(issue, part_sup, part_supi);
    }
    
    if (imp.IsSetPages()) {
        string pages = imp.GetPages();
        s_FixPages(pages);
        if (!NStr::IsBlank(pages)) {
            jour << ": " << pages;
        }
    }

    jour << ';' << '\n';

    if (imp.CanGetPub()) {
        string affil;
        CReferenceItem::FormatAffil(imp.GetPub(), affil);
        if (!NStr::IsBlank(affil)) {
            jour << affil << ' ';
        }
    }

    jour << year;

    if (do_gb) {
        if (imp.IsSetPrepub()  &&  imp.GetPrepub() == CImprint::ePrepub_in_press) {
            jour << " In press";
        }
    }

    journal = CNcbiOstrstreamToString(jour);
}


static void s_FormatCitBook(const CReferenceItem& ref, string& journal)
{
    _ASSERT(ref.IsSetBook()  &&  ref.GetBook().IsSetImp());

    const CCit_book&       book = ref.GetBook();
    const CCit_book::TImp& imp  = book.GetImp();

    journal.erase();

    CNcbiOstrstream jour;

    string title = book.GetTitle().GetTitle();
    jour << "(in) " << NStr::ToUpper(title) << '.';

    // add the affiliation
    string affil;
    if (imp.CanGetPub()) {
        CReferenceItem::FormatAffil(imp.GetPub(), affil);
        if (!NStr::IsBlank(affil)) {
            jour << ' ' << affil;
        }
    }

    // add the year
    string year;
    if (imp.IsSetDate()) {
        s_FormatYear(imp.GetDate(), year);
        if (!NStr::IsBlank(year)) {
            jour << (!NStr::IsBlank(affil) ? " " : kEmptyStr) << year;
        }
    }

    if (imp.CanGetPrepub()  &&  imp.GetPrepub() == CImprint::ePrepub_in_press) {
        jour << ", In press";
    }

    journal = CNcbiOstrstreamToString(jour);
}


static void s_FormatCitGen
(const CReferenceItem& ref,
 string& journal,
 const CFlatFileConfig& cfg,
 CBioseqContext& ctx
 )
{
    _ASSERT(ref.IsSetGen());

    journal.erase();

    const CCit_gen& gen = ref.GetGen();
    string cit = gen.IsSetCit() ? gen.GetCit() : kEmptyStr;

    if (!gen.IsSetJournal()  &&  NStr::StartsWith(cit, "unpublished", NStr::eNocase)) {
        if (cfg.NoAffilOnUnpub()) {
            // a comment in asn2gnb5.c says "remove [...] section once QA against asn2ff is done",
            // so I suspect we'll have to remove this if-statement at some point
            if (cfg.DropBadCitGens() && ( ctx.IsEMBL() || ctx.IsDDBJ() ) ) {
                string year;
                if (gen.IsSetDate()) {
                    const CDate& date = gen.GetDate();
                    if (date.IsStr()  ||
                        date.IsStd()  &&  date.GetStd().IsSetYear()  &&  date.GetStd().GetYear() != 0) {
                        gen.GetDate().GetDate(&year, "(%Y)");
                    }
                }
                journal += "Unpublished";
                if (!NStr::IsBlank(year)) {
                    journal += ' ';
                    journal += year;
                }
                return;
            }
            journal = "Unpublished";
            return;
        }

        if (gen.IsSetAuthors()  &&  gen.GetAuthors().IsSetAffil()) {
            string affil;
            CReferenceItem::FormatAffil(gen.GetAuthors().GetAffil(), affil, true);
            if (!NStr::IsBlank(affil)) {
                journal = "Unpublished ";
                journal += affil;
                NStr::TruncateSpacesInPlace(journal);
                return;
            }
        }

        journal = cit;
        NStr::TruncateSpacesInPlace(journal);
        return;
    }

    string year;
    if (gen.IsSetDate()) {
        s_FormatYear(gen.GetDate(), year);
    }

    string pages;
    if (gen.IsSetPages()) {
        pages = gen.GetPages();
        s_FixPages(pages);
    }
    
    if (gen.IsSetJournal()) {
        journal = gen.GetJournal().GetTitle();
    }
    string prefix;
    string in_press;
    if (!NStr::IsBlank(cit)) {
        SIZE_TYPE pos = NStr::FindNoCase(cit, "Journal=\"");
        if (pos != NPOS) {
            pos += 9; // skip over the string part "Journal=\""
            if( cit.at( cit.length() - 1) == '"' ) { // There should be a double-quote at the end to complete the "Journal" entry
                journal = cit.substr(pos, cit.length() - pos - 1 );
                prefix = ' ';
            } else {
                journal.erase(); // error: double-quote that completes journal was not there
            }
        } else if (NStr::StartsWith(cit, "submitted", NStr::eNocase)  ||
                   NStr::StartsWith(cit, "unpublished", NStr::eNocase)) {
            if (!cfg.DropBadCitGens()  ||  !NStr::IsBlank(journal)) {
                in_press = cit;
            } else {
                in_press = "Unpublished";
            }
        } else if (NStr::StartsWith(cit, "Online Publication", NStr::eNocase)  ||
                   NStr::StartsWith(cit, "Published Only in DataBase", NStr::eNocase)) {
            in_press = cit;
        } else if (NStr::StartsWith(cit, "In press", NStr::eNocase) ) {
            in_press = cit;
            prefix = ' ';
        } else if( NStr::StartsWith(cit, "(er) ", NStr::eNocase) ) {
            journal = cit;
            prefix = ' ';
        } else if (!cfg.DropBadCitGens()  &&  NStr::IsBlank(journal)) {
            journal = cit;
            prefix = ' ';
        }
    }

    SIZE_TYPE pos = journal.find_first_of("=\"");
    if (pos != NPOS) {
        journal.resize(pos);
        prefix = kEmptyStr;
    }

    if (!NStr::IsBlank(in_press)) {
        (journal += prefix) += in_press;
        prefix = ' ';
    }

    if (gen.IsSetVolume()  &&  !NStr::IsBlank(gen.GetVolume())) {
        if (prefix.empty()  &&  NStr::EndsWith(journal, ".")) {
            prefix = ' ';
        }
        (journal += prefix) += gen.GetVolume();
        prefix = ' ';
    }

    if (!NStr::IsBlank(pages) ) {
        if (cfg.IsFormatGenbank()) {
            journal += ", " + pages;
        } else if (cfg.IsFormatEMBL()) {
            journal += ':' + pages;
        }
    }

    if (!NStr::IsBlank(year)) {
        (journal += prefix) += year;
    }
}


static void s_FormatThesis(const CReferenceItem& ref, string& journal)
{
    _ASSERT(ref.IsSetBook()  &&  ref.GetBook().IsSetImp());

    const CCit_book&       book = ref.GetBook();
    const CCit_book::TImp& imp  = book.GetImp();

    journal.erase();

    journal = "Thesis ";
    if (imp.IsSetDate()) {
        string year;
        s_FormatYear(imp.GetDate(), year);
        journal += year;
    }

    if (imp.CanGetPub()) {
        string affil;
        CReferenceItem::FormatAffil(imp.GetPub(), affil);
        if (!NStr::IsBlank(affil)) {
            ConvertQuotes(affil);
            journal += ' ';
            journal += affil;
        }
    }

    if ( imp.CanGetPub() && imp.CanGetPrepub()  &&  imp.GetPrepub() == CImprint::ePrepub_in_press) {
        journal += ", In press";
    }
}


static void s_FormatCitSub
(const CReferenceItem& ref,
 string& journal,
 bool do_embl)
{
    _ASSERT(ref.IsSetSub());

    const CCit_sub& sub = ref.GetSub();

    journal = "Submitted ";

    string date;
    if (sub.IsSetDate()) {
        DateToString(sub.GetDate(), date, eDateToString_cit_sub);
    } else {
        date = "\?\?-\?\?\?-\?\?\?\?";
    }
    ((journal += '(') += date) += ')';

    if (sub.IsSetAuthors()) {
        if (sub.GetAuthors().IsSetAffil()) {
            string affil;
            CReferenceItem::FormatAffil(sub.GetAuthors().GetAffil(), affil, true);
            if (do_embl) {
                bool embl_affil =
                    NStr::StartsWith( affil, " to the EMBL/GenBank/DDBJ databases." );
                if ( !embl_affil ) {
                    journal += " to the EMBL/GenBank/DDBJ databases.\n";
                } else {
                    journal += ' ';
                }
            } else {
                journal += ' ';
            }
            journal += affil;
        } else if (do_embl) {
            journal += " to the EMBL/GenBank/DDBJ databases.\n";
        }
    }
}


static void s_FormatPatent
(const CReferenceItem& ref,
 string& journal,
 const CBioseqContext &ctx )
{
    _ASSERT(ref.IsSetPatent());

    const CFlatFileConfig& cfg = ctx.Config();

    const CCit_pat& pat = ref.GetPatent();
    bool embl    = (cfg.GetFormat() == CFlatFileConfig::eFormat_EMBL);
    bool genbank = (cfg.GetFormat() == CFlatFileConfig::eFormat_GenBank);

    const bool is_html = cfg.DoHTML();

    journal.erase();

    string header;
    string suffix;
    //
    //  Pre grant publication handling:
    //  As the ASN.1 spec does not dedicate fields for this intermediate state,
    //  the application oriented slots are used.
    //  Recognize pre grant publications by using of app-number and app-date, and
    //  by specifying the doc-type as "".
    //  And of course, it must not have a number--- otherwise it would be a patent
    //  already...
    //
    bool use_pre_grant_formatting = ! pat.CanGetNumber();
    use_pre_grant_formatting &= ( 
        pat.CanGetApp_number() &&
        pat.CanGetCountry() && pat.GetCountry() == "US" );
    //
    //  2006-01-26:
    //  Pre grant formatting currently only in non-release mode until quarantine
    //  period is over.
    //
    use_pre_grant_formatting = use_pre_grant_formatting &&
        ( cfg.GetMode() != CFlatFileConfig::eMode_Release );

    // and, there must be a good "patent" id in the Seq-ids for this Bioseq
    // CSeqId
    if( use_pre_grant_formatting ) {
        bool any_good_patents = false;
        ITERATE( CBioseq::TId, id_iter, ctx.GetBioseqIds() ) {
            const CSeq_id &id = **id_iter;
            if( id.IsPatent() && 
                id.GetPatent().IsSetCit() &&
                id.GetPatent().GetCit().IsSetId() &&
                id.GetPatent().GetCit().GetId().IsApp_number() &&
                !NStr::IsBlank(id.GetPatent().GetCit().GetId().GetApp_number()) ) 
            {
                any_good_patents = true;
                break;
            }
        }
        if( ! any_good_patents ) {
            use_pre_grant_formatting = false;
        }
    }
        
    if (genbank) {
        if ( use_pre_grant_formatting ) {
            header = "Pre-Grant Patent: ";
            suffix = " ";
        }
        else {
            header = "Patent: ";
            suffix = " ";
        }
    } else if (embl) {
        header = "Patent number ";
    }

    CNcbiOstrstream jour;
    jour << header;

    if (pat.IsSetCountry()  &&  !NStr::IsBlank(pat.GetCountry())) {
        jour << pat.GetCountry() << suffix;
    }
    if (pat.IsSetNumber()  &&  !NStr::IsBlank(pat.GetNumber())) {
        const bool do_us_patent_html = 
            is_html && pat.IsSetCountry() && pat.GetCountry() == "US";
        if( do_us_patent_html ) {
            jour << "<a href=\"" << strLinkBaseUSPTO << pat.GetNumber() << "\">";
        }
        jour << pat.GetNumber();
        if( do_us_patent_html ) {
            jour << "</a>";
        }
    } else if (pat.IsSetApp_number()  &&  !NStr::IsBlank(pat.GetApp_number())) {
        if ( use_pre_grant_formatting ) {
            jour << pat.GetApp_number();
        }
        else {
            jour << '(' << pat.GetApp_number() << ')';
        }
    }
    if (pat.IsSetDoc_type()  &&  !NStr::IsBlank(pat.GetDoc_type())) {
        jour << '-' << pat.GetDoc_type();
    }

    if (ref.GetPatSeqid() > 0) {
        if (embl) {
            jour << '/' << ref.GetPatSeqid() << ", ";
        } else {
            jour << ' ' << ref.GetPatSeqid() << ' ';
        }
    } else {
        jour << ' ';
    }
    
    
    // Date 
    string date;
    if (pat.IsSetDate_issue()) {
        DateToString(pat.GetDate_issue(), date, eDateToString_patent );        
    } else if (pat.IsSetApp_date()) {
        DateToString(pat.GetApp_date(), date, eDateToString_patent);
    }
    if (!NStr::IsBlank(date)) {
        jour << date;
    }
    if (genbank) {
        jour << ';';
    } else if (embl) {
        jour << '.';
    }

    // add affiliation field
    if (pat.IsSetAuthors()  &&  pat.GetAuthors().IsSetAffil()) {
        const CAffil& affil = pat.GetAuthors().GetAffil();
        if (affil.IsStr()  &&  !NStr::IsBlank(affil.GetStr())) {
            jour << '\n' << affil.GetStr();
        } else if (affil.IsStd()) {
            const CAffil::TStd& std = affil.GetStd();

            // if affiliation fields are non-blank, put them on a new line.
            if ((std.IsSetAffil()    &&  !NStr::IsBlank(std.GetAffil()))   ||
                (std.IsSetStreet()   &&  !NStr::IsBlank(std.GetStreet()))  ||
                (std.IsSetDiv()      &&  !NStr::IsBlank(std.GetDiv()))     ||
                (std.IsSetCity()     &&  !NStr::IsBlank(std.GetCity()))    ||
                (std.IsSetSub()      &&  !NStr::IsBlank(std.GetSub()))     ||
                (std.IsSetCountry()  &&  !NStr::IsBlank(std.GetCountry()))) {
                jour << '\n';
            }

            // Write out the affiliation fields
            string prefix;
            if (std.IsSetAffil()  &&  !NStr::IsBlank(std.GetAffil())) {
                jour << std.GetAffil() << ';';
                prefix = ' ';
            }
            if (std.IsSetStreet()  &&  !NStr::IsBlank(std.GetStreet())) {
                jour << prefix << std.GetStreet() << ';';
                prefix = ' ';
            }
            if (std.IsSetDiv()  &&  !NStr::IsBlank(std.GetDiv())) {
                jour << prefix << std.GetDiv() << ';';
                prefix = ' ';
            }
            if (std.IsSetCity()  &&  !NStr::IsBlank(std.GetCity())) {
                jour << prefix << std.GetCity();
                prefix = ", ";
            }
            if (std.IsSetSub()  &&  !NStr::IsBlank(std.GetSub())) {
                jour << prefix << std.GetSub();
            }
            if (std.IsSetCountry()  &&  !NStr::IsBlank(std.GetCountry())) {
                jour << ';' << '\n' << std.GetCountry() << ';';
            }
        }
    }
 
    if (pat.IsSetAssignees()  &&  pat.GetAssignees().IsSetAffil()) {
        const CCit_pat::TAssignees& assignees = pat.GetAssignees();
        const CAffil& affil = assignees.GetAffil();
        string authors;
        CReferenceItem::FormatAuthors(assignees, authors);

        if (affil.IsStr()) {
            if (!NStr::IsBlank(authors)  ||  !NStr::IsBlank(affil.GetStr())) {
                jour << '\n' << authors << '\n' << affil.GetStr();
            }
        } else if (affil.IsStd()) {
            const CAffil::TStd& std = affil.GetStd();

            // if affiliation fields are non-blank, put them on a new line.
            if (!NStr::IsBlank(authors)                                    ||
                (std.IsSetAffil()    &&  !NStr::IsBlank(std.GetAffil()))   ||
                (std.IsSetStreet()   &&  !NStr::IsBlank(std.GetStreet()))  ||
                (std.IsSetDiv()      &&  !NStr::IsBlank(std.GetDiv()))     ||
                (std.IsSetCity()     &&  !NStr::IsBlank(std.GetCity()))    ||
                (std.IsSetSub()      &&  !NStr::IsBlank(std.GetSub()))     ||
                (std.IsSetCountry()  &&  !NStr::IsBlank(std.GetCountry()))) {
                jour << '\n';
            }

            // Write out the affiliation fields
            string prefix;
            if (!NStr::IsBlank(authors)) {
                jour << authors << ';';
                prefix = ' ';
            }

            // !!! add consortium
            
            if (std.IsSetAffil()  &&  !NStr::IsBlank(std.GetAffil())) {
                // prefix not printed in order to match C toolkit's behavior
                jour << std.GetAffil() << ';';
                prefix = ' ';
            }
            if (std.IsSetStreet()  &&  !NStr::IsBlank(std.GetStreet())) {
                jour << prefix << std.GetStreet() << ';';
                prefix = ' ';
            }
            if (std.IsSetDiv()  &&  !NStr::IsBlank(std.GetDiv())) {
                jour << prefix << std.GetDiv() << ';';
                prefix = ' ';
            }
            if (std.IsSetCity()  &&  !NStr::IsBlank(std.GetCity())) {
                jour << prefix << std.GetCity();
                prefix = ", ";
            }
            if (std.IsSetSub()  &&  !NStr::IsBlank(std.GetSub())) {
                jour << prefix << std.GetSub();
            }
            if (std.IsSetCountry()  &&  !NStr::IsBlank(std.GetCountry())) {
                jour << ';' << '\n' << std.GetCountry() << ';';
            }
        }
    }

    journal = CNcbiOstrstreamToString(jour);
}


static bool s_StrictIsoJta(CBioseqContext& ctx)
{
    if (!ctx.Config().CitArtIsoJta()) {
        return false;
    }

    bool strict = false;
    ITERATE (CBioseq_Handle::TId, it, ctx.GetHandle().GetId()) {
        switch (it->Which()) {
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
                strict = true;
                break;
            default:
                break;
        }
    }
    return strict;
}


static void s_FormatJournal
(const CReferenceItem& ref,
 string& journal,
 CBioseqContext& ctx)
{
    _ASSERT(ref.IsSetJournal());

    const CFlatFileConfig& cfg = ctx.Config();

    journal.erase();

    const CCit_jour& cit_jour = ref.GetJournal();
    const CTitle& ttl = cit_jour.GetTitle();

    if (!cit_jour.IsSetImp()) {
        return;
    }
    const CImprint& imp = cit_jour.GetImp();

    string year;
    if (imp.IsSetDate()) {
        s_FormatYear(imp.GetDate(), year);
    }

    CImprint::TPrepub prepub = CImprint::EPrepub(0);
    if (imp.IsSetPrepub()) {
        prepub = imp.GetPrepub();
        if (prepub == CImprint::ePrepub_submitted  ||  prepub == CImprint::ePrepub_other) {
            journal += "Unpublished";
            if (!NStr::IsBlank(year)) {
                journal += ' ';
                journal += year;
            }
            return;
        }
    }

    // always use iso_jta title if present
    string title;
    ITERATE (CTitle::Tdata, it, ttl.Get()) {
        if ((*it)->IsIso_jta()) {
            title = (*it)->GetIso_jta();
        }
    }

    if (NStr::IsBlank(title)  &&  s_StrictIsoJta(ctx)  &&  !ref.IsElectronic()) {
        return;
    }

    if (NStr::IsBlank(title)) {
        title = ttl.GetTitle();
    }

    if (title.length() < 3) {
        journal = '.';
        return;
    }

    CNcbiOstrstream jour;

//    if (ref.IsElectronic()  &&  !NStr::StartsWith(title, "(er")) {
//        jour << "(er) ";
//    }
    jour << title;

    string issue, part_sup, part_supi;
    if (cfg.IsFormatGenbank() || cfg.IsFormatGBSeq() ) {
        issue = imp.IsSetIssue() ? imp.GetIssue(): kEmptyStr;
        part_sup = imp.IsSetPart_sup() ? imp.GetPart_sup() : kEmptyStr;
        part_supi = imp.IsSetPart_supi() ? imp.GetPart_supi() : kEmptyStr;
    }

    string volume = imp.IsSetVolume() ? imp.GetVolume() : kEmptyStr;
    if (!NStr::IsBlank(volume)) {
        jour << ' ' << volume;
    }

    string pages;
    if (imp.IsSetPages()) {
        pages = imp.GetPages();
        if (!ref.IsElectronic()) {
            s_FixPages(pages);
        }
    }

    if (!NStr::IsBlank(volume)  ||  !NStr::IsBlank(pages)) {
        jour << s_DoSup(issue, part_sup, part_supi);
    }

    if (cfg.IsFormatGenbank() || cfg.IsFormatGBSeq()) {
        if (!NStr::IsBlank(pages)) {
            jour << ", " << pages;
        }
    } else if (cfg.IsFormatEMBL()) {
        if (!NStr::IsBlank(pages)) {
            jour << ":" << pages;
        }
        if (prepub == CImprint::ePrepub_in_press  || NStr::IsBlank(volume)) {
            jour << " 0:0-0";
        }
    }   
    
    if (!NStr::IsBlank(year)) {
        jour << ' ' << year;
    }
    
    if (cfg.IsFormatGenbank() || cfg.IsFormatGBSeq()) {
        if (prepub == CImprint::ePrepub_in_press) {
            jour << " In press";
        } else if (imp.IsSetPubstatus()  &&  imp.GetPubstatus() == 10  &&  NStr::IsBlank(pages)) {
            jour << " In press";
        }
    }

    journal = CNcbiOstrstreamToString(jour);
}


void CFlatItemFormatter::x_FormatRefJournal
(const CReferenceItem& ref,
 string& journal,
 CBioseqContext& ctx) const
{
    const CFlatFileConfig& cfg = ctx.Config();

    journal.erase();
    
    switch (ref.GetPubType()) {
        case CReferenceItem::ePub_sub:
            if (ref.IsSetSub()) {
                s_FormatCitSub(ref, journal, cfg.IsFormatEMBL());
            }
            break;

        case CReferenceItem::ePub_gen:
            if (ref.IsSetGen()) {
                s_FormatCitGen(ref, journal, cfg, ctx);
            }
            break;

        case CReferenceItem::ePub_jour:
            if (ref.IsSetJournal()) {
                s_FormatJournal(ref, journal, ctx);
            }
            break;

        case CReferenceItem::ePub_book:
            if (ref.IsSetBook()  &&  ref.GetBook().IsSetImp()) {
                s_FormatCitBook(ref, journal);
            }
            break;

        case CReferenceItem::ePub_book_art:
            if (ref.IsSetBook()  &&
                ref.GetBook().IsSetImp()  &&  ref.GetBook().IsSetTitle()) {
                s_FormatCitBookArt(ref, journal, cfg.IsFormatGenbank());
            }
            break;

        case CReferenceItem::ePub_thesis:
            if (ref.IsSetBook()  &&  ref.GetBook().IsSetImp()) {
                s_FormatThesis(ref, journal);
            }
            break;

        case CReferenceItem::ePub_pat:
            if (ref.IsSetPatent()) {
                s_FormatPatent(ref, journal, ctx);
            }
            break;

        default:
            break;
    }

    if (NStr::IsBlank(journal)) {
        journal = "Unpublished";
        /* if ( ref.IsSetDate() ) {
            string year;
            s_FormatYear(ref.GetDate(), year);
            journal += string(" ") + year;
        } */
    }
    StripSpaces(journal);
}


void CFlatItemFormatter::x_GetKeywords
(const CKeywordsItem& kws,
 const string& prefix,
 list<string>& l) const
{
    string keywords = NStr::Join(kws.GetKeywords(), "; ");
    if( ! NStr::EndsWith(keywords, ".") ) {
        keywords += '.';
    }

    ExpandTildes( keywords, eTilde_space );
    Wrap(l, prefix, keywords);
}


END_SCOPE(objects)
END_NCBI_SCOPE
