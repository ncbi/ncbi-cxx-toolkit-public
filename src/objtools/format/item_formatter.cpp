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
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
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
    for (i = 0; i < acc.length()  &&  !isdigit(acc[i]); ++i) {
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
        if (!s_IsSuccessor(*it, *prev)) {
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

    if ( ctx.IsWGS() ) {
        acc_line << separator << acc.GetWGSAccession();
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
    string tag2;
    Pad(tag, tag2, where);
    const string& indent = (where == eFeat ? m_FeatIndent : m_Indent);
    NStr::Wrap(body, width, l, m_WrapFlags, indent, tag2);
    NON_CONST_ITERATE (list<string>, it, l) {
        TrimSpaces(*it, indent.length());
    }
    return l;
}


list<string>& CFlatItemFormatter::Wrap
(list<string>& l,
 const string& tag,
 const string& body,
 EPadContext where) const
{
    string tag2;
    Pad(tag, tag2, where);
    const string& indent = (where == eFeat ? m_FeatIndent : m_Indent);
    NStr::Wrap(body, GetWidth(), l, m_WrapFlags, indent, tag2);
    NON_CONST_ITERATE (list<string>, it, l) {
        TrimSpaces(*it, indent.length());
    }
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
    if (date.IsStr()) {
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
        str += ' ';
        str +=part_sup;
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


static bool s_ParsePagesPart(const string& pages, SIZE_TYPE& dig, string& let)
{
    static const string kDigits  = "0123456789";

    bool first_digits = true;

    if (pages.empty()) {
        return false;
    }

    // numbers come first
    if (isdigit(pages[0])) {
        first_digits = true;
        dig = NStr::StringToUInt(pages, 10, NStr::eCheck_Skip);
        SIZE_TYPE i = pages.find_first_not_of(kDigits);
        if (i != NPOS) {
            let = pages.substr(i);
        }
    } else if (isalpha(pages[0])) {  // letters come first
        first_digits = false;
        SIZE_TYPE i = pages.find_first_of(kDigits);
        if (i == NPOS) {
            let = pages;
        } else {
            let = pages.substr(0, i);
            dig = NStr::StringToUInt(pages.substr(i), 10, NStr::eCheck_Skip);
        }
    }

    return first_digits;
}


static bool s_ParsePages
(const string& pages,
 SIZE_TYPE& dig1,
 string& let1,
 SIZE_TYPE& dig2,
 string& let2,
 bool &first_digits)
{
    if (pages.empty()) {
        return false;
    }

    dig1 = dig2 = 0;
    let1.erase();
    let2.erase();

    SIZE_TYPE hyphen = pages.find('-');
    _ASSERT(hyphen != NPOS);

    first_digits = s_ParsePagesPart(pages.substr(0, hyphen), dig1, let1);
    s_ParsePagesPart(pages.substr(hyphen + 1, NPOS), dig2, let2);

    return true;
}


static void s_FixPages(string& pages)
{
    static const string kRomans  = "IVXLCDM-";

    NStr::TruncateSpacesInPlace(pages);
    if (pages.empty()) {
        return;
    }

    // allow all roman numerals (and hyphen) 
    if (pages.find_first_not_of(kRomans) == NPOS) {
        return;
    }

    // reject if no dash
    if (pages.find('-') == NPOS) {
        return;
    }

    // reject if contain non alpha numeric characters other than dash
    ITERATE (string, it, pages) {
        if (!isalnum(*it)  &&  *it != '-') {
            return;
        }
    }
    
    SIZE_TYPE dig1, dig2;
    string let1, let2;
    bool first_digits;

    s_ParsePages(pages, dig1, let1, dig2, let2, first_digits);

    if (first_digits) {
        if (dig2 == 0) {
            return;
        }
    } else {
        if ((dig1 == 0)  &&  (dig2 != 0)) {
            return;
        }
    }

    // The following expands "F502-512" into "F502-F512" and
    // checks, for entries like "12a-12c" that a > c.  "12aa-12ab",
    // "125G-137A", "125-G137" would be rejected.
    if (!let1.empty()  &&  let2.empty()  &&  !first_digits) {
        let2 = let1;
    }
    if (first_digits  &&  !let1.empty()  &&  !let2.empty()) {
        if (let2 < let1) {
            return;
        }
    }

    // The following expands "125-37" into "125-137".
    if (dig1 != 0  &&  dig2 != 0) {
        if (dig2 < dig1) {
            string num1 = NStr::UIntToString(dig1);
            string num2 = NStr::UIntToString(dig2);
            if (num1.length() > num2.length()) {
                size_t diff = num1.length() - num2.length();
                dig2 = NStr::StringToUInt(num1.substr(0, diff) + num2);
            }
        }
    }
    if (dig2 < dig1) {
        return;
    }

    if (first_digits) {
        pages = NStr::UIntToString(dig1) + let1 + "-" + NStr::UIntToString(dig2) + let2;
    } else {
        pages = let1;
        if (dig1 != 0) {
            pages += NStr::UIntToString(dig1);
        }
        pages += '-';
        pages += let2;
        if (dig2 != 0) {
            pages += NStr::UIntToString(dig2);
        }
    }
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
    }

    if (imp.IsSetPrepub()) {
        CImprint::TPrepub prepub = imp.GetPrepub();
        if (prepub == CImprint::ePrepub_submitted  ||  prepub == CImprint::ePrepub_other) {
            journal = "Unpublished ";
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
            jour << ((num_auth == 1) ? " (Ed.);" : " (Eds.);") << Endl();
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

    jour << ';' << Endl();

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
 const CFlatFileConfig& cfg
 )
{
    _ASSERT(ref.IsSetGen());

    journal.erase();

    const CCit_gen& gen = ref.GetGen();
    string cit = gen.IsSetCit() ? gen.GetCit() : kEmptyStr;

    if (!gen.IsSetJournal()  &&  NStr::StartsWith(cit, "unpublished", NStr::eNocase)) {
        if (cfg.NoAffilOnUnpub()) {
            if (cfg.DropBadCitGens()) {
                string year;
                if (gen.IsSetDate()) {
                    gen.GetDate().GetDate(&year, "(%Y)");
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
            journal = cit.substr(pos + 9);
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
        } else if (!cfg.DropBadCitGens()  &&  NStr::IsBlank(journal)) {
            journal = cit;
            prefix = ' ';
        }
    }

    SIZE_TYPE pos = journal.find_first_of("=\"");
    if (pos != NPOS) {
        journal.erase();
        prefix = kEmptyStr;
    }

    if (!NStr::IsBlank(in_press)) {
        (journal += prefix) += in_press;
        prefix = ' ';
    }

    if (gen.IsSetVolume()  &&  !NStr::IsBlank(gen.GetVolume())) {
        (journal += prefix) += gen.GetVolume();
        prefix = ' ';
    }

    if (!NStr::IsBlank(pages)) {
        if (cfg.IsFormatGenbank()) {
            journal += ", ";
        } else if (cfg.IsFormatEMBL()) {
            journal += ':';
        }
        journal+= pages;
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

    if (imp.CanGetPrepub()  &&  imp.GetPrepub() == CImprint::ePrepub_in_press) {
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
        DateToString(sub.GetDate(), date, true);
    } else {
        date = "~?~????";
    }
    ((journal += '(') += date) += ')';

    if (sub.IsSetAuthors()) {
        if (sub.GetAuthors().IsSetAffil()) {
            string affil;
            CReferenceItem::FormatAffil(sub.GetAuthors().GetAffil(), affil, true);
            if (do_embl) {
                if (!NStr::StartsWith(affil, " to the EMBL/GenBank/DDBJ databases.")) {
                    journal += " to the EMBL/GenBank/DDBJ databases.\n";
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
 CFlatFileConfig::TFormat format)
{
    _ASSERT(ref.IsSetPatent());

    const CCit_pat& pat = ref.GetPatent();
    bool embl    = (format == CFlatFileConfig::eFormat_EMBL);
    bool genbank = (format == CFlatFileConfig::eFormat_GenBank);

    journal.erase();

    string header;
    string suffix;
    if (genbank) {
        header = "Patent: ";
        suffix = " ";
    } else if (embl) {
        header = "Patent number ";
    }

    CNcbiOstrstream jour;
    jour << header;

    if (pat.IsSetCountry()  &&  !NStr::IsBlank(pat.GetCountry())) {
        jour << pat.GetCountry() << suffix;
    }
    if (pat.IsSetNumber()  &&  !NStr::IsBlank(pat.GetNumber())) {
        jour << pat.GetNumber();
    } else if (pat.IsSetApp_number()  &&  !NStr::IsBlank(pat.GetApp_number())) {
        jour << '(' << pat.GetApp_number() << ')';
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
        DateToString(pat.GetDate_issue(), date);        
    } else if (pat.IsSetApp_date()) {
        DateToString(pat.GetApp_date(), date);
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
            jour << Endl() << affil.GetStr();
        } else if (affil.IsStd()) {
            const CAffil::TStd& std = affil.GetStd();

            // if affiliation fields are non-blank, put them on a new line.
            if ((std.IsSetAffil()    &&  !NStr::IsBlank(std.GetAffil()))   ||
                (std.IsSetStreet()   &&  !NStr::IsBlank(std.GetStreet()))  ||
                (std.IsSetDiv()      &&  !NStr::IsBlank(std.GetDiv()))     ||
                (std.IsSetCity()     &&  !NStr::IsBlank(std.GetCity()))    ||
                (std.IsSetSub()      &&  !NStr::IsBlank(std.GetSub()))     ||
                (std.IsSetCountry()  &&  !NStr::IsBlank(std.GetCountry()))) {
                jour << Endl();
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
                jour << ';' << Endl() << std.GetCountry() << ';';
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
                jour << Endl() << authors << Endl() << affil.GetStr();
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
                jour << Endl();
            }

            // Write out the affiliation fields
            string prefix;
            if (!NStr::IsBlank(authors)) {
                jour << authors << ';';
                prefix = ' ';
            }

            // !!! add consortium
            
            if (std.IsSetAffil()  &&  !NStr::IsBlank(std.GetAffil())) {
                jour << prefix << std.GetAffil() << ';';
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
                jour << ';' << Endl() << std.GetCountry() << ';';
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

    if (ref.IsElectronic()  &&  !NStr::StartsWith(title, "(er")) {
        jour << "(er) ";
    }
    jour << title;

    string issue, part_sup, part_supi;
    if (cfg.IsFormatGenbank()) {
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

    if (cfg.IsFormatGenbank()) {
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
    
    if (cfg.IsFormatGenbank()) {
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
                s_FormatCitGen(ref, journal, cfg);
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
                s_FormatPatent(ref, journal, cfg.GetFormat());
            }
            break;

        default:
            break;
    }

    if (NStr::IsBlank(journal)) {
        journal = "Unpublished";
    }
    StripSpaces(journal);
}


void CFlatItemFormatter::x_GetKeywords
(const CKeywordsItem& kws,
 const string& prefix,
 list<string>& l) const
{
    string keywords = NStr::Join(kws.GetKeywords(), "; ");
    keywords += '.';

    Wrap(l, prefix, keywords);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.23  2005/03/28 17:22:57  shomrat
* Added assignees for Cit-pat
*
* Revision 1.22  2005/03/02 16:32:08  shomrat
* Implement range format for secondary accessions
*
* Revision 1.21  2005/02/09 14:59:12  shomrat
* Relax requirement for iso_jta journal
*
* Revision 1.20  2005/02/07 15:00:31  shomrat
* initial support for HTML format
*
* Revision 1.19  2005/01/12 16:45:37  shomrat
* Code refactoring, moved journal formatting to format classes
*
* Revision 1.18  2004/11/24 16:51:56  shomrat
* Specify flat-file specific line wrap
*
* Revision 1.17  2004/11/15 20:10:10  shomrat
* Handle electronic publications
*
* Revision 1.16  2004/10/18 18:49:44  shomrat
* Handle str dates in journal format
*
* Revision 1.15  2004/10/05 15:45:03  shomrat
* Fixed journal formatting
*
* Revision 1.14  2004/08/30 13:39:52  shomrat
* fixed reference formatting
*
* Revision 1.13  2004/08/19 16:35:49  shomrat
* strip spaces from journal
*
* Revision 1.12  2004/06/21 18:54:03  ucko
* Add a GFF3 format.
*
* Revision 1.11  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
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
