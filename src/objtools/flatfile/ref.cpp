/* $Id$
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
 * File Name:  ref.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/biblio/Id_pat.hpp>
#include <objects/biblio/Id_pat_.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/ArticleIdSet.hpp>
#include <objects/biblio/ArticleId.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/biblio/Cit_proc.hpp>

#include "index.h"
#include "genbank.h"
#include "embl.h"

#include <objtools/flatfile/flatdefn.h>
#include "ftamed.h"

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "utilref.h"
#include "asci_blk.h"
#include "add.h"
#include "utilfun.h"
#include "ind.hpp"
#include "ref.h"
#include "xgbfeat.h"
#include "xutils.h"
#include "fta_xml.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "ref.cpp"

#define MAXKW 38


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static const char* strip_sub_str[] = {
    "to the EMBL/GenBank/DDBJ databases",
    "to the EMBL/DDBJ/GenBank databases",
    "to the DDBJ/GenBank/EMBL databases",
    "to the DDBJ/EMBL/GenBank databases",
    "to the GenBank/DDBJ/EMBL databases",
    "to the GenBank/EMBL/DDBJ databases",
    "to the INSDC",
    nullptr
};

static const char* ERRemarks[] = {
    // epublish
    "Publication Status: Online-Only",                      /*  1 */
    "Publication Status : Online-Only",                     /*  2 */
    "Publication_Status: Online-Only",                      /*  3 */
    "Publication_Status : Online-Only",                     /*  4 */
    "Publication-Status: Online-Only",                      /*  5 */
    "Publication-Status : Online-Only",                     /*  6 */
    // aheadofprint
    "Publication Status: Available-Online",                 /*  7 */
    "Publication Status : Available-Online",                /*  8 */
    "Publication_Status: Available-Online",                 /*  9 */
    "Publication_Status : Available-Online",                /* 10 */
    "Publication-Status: Available-Online",                 /* 11 */
    "Publication-Status : Available-Online",                /* 12 */
    "Publication Status: Available-Online prior to print",  /* 13 */
    "Publication Status : Available-Online prior to print", /* 14 */
    "Publication_Status: Available-Online prior to print",  /* 15 */
    "Publication_Status : Available-Online prior to print", /* 16 */
    "Publication-Status: Available-Online prior to print",  /* 17 */
    "Publication-Status : Available-Online prior to print", /* 18 */
    nullptr
};

/**********************************************************/
static void normalize_comment(string& comment)
{
    for (size_t pos = 0; pos < comment.size();) {
        pos = comment.find("; ", pos);
        if (pos == string::npos)
            break;
        pos += 2;

        size_t n = 0;
        for (size_t i = pos; i < comment.size(); ++i) {
            char c = comment[i];
            if (c == ' ' || c == ';')
                ++n;
            else
                break;
        }
        if (n > 0)
            comment.erase(pos, n);
    }
}

/**********************************************************
 *
 *   static DatePtr get_lanl_date(s):
 *
 *      Get year, month, day  and return NCBI_DatePtr.
 *      Temporary used for lanl form of date that
 *   is (JUL 21 1993).
 *
 *                                              01-4-94
 *
 **********************************************************/
static CRef<CDate> get_lanl_date(char* s)
{
    int day   = 0;
    int month = 0;
    int year;
    int cal;

    const char* months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    CRef<CDate> date(new CDate);
    for (cal = 0; cal < 12; cal++) {
        if (StringEquNI(s + 1, months[cal], 3)) {
            month = cal + 1;
            break;
        }
    }
    day  = atoi(s + 5);
    year = atoi(s + 8);
    if (year < 1900 || year > 1994) {
        FtaErrPost(SEV_WARNING, ERR_REFERENCE_IllegalDate, "Illegal year: {}", year);
    }

    date->SetStd().SetYear(year);
    date->SetStd().SetMonth(month);
    date->SetStd().SetDay(day);

    if (XDateCheck(date->GetStd()) != 0) {
        FtaErrPost(SEV_WARNING, ERR_REFERENCE_IllegalDate, "Illegal date: {}", s);
        date.Reset();
    }

    return (date);
}

/**********************************************************
 *
 *   static char* clean_up(str):
 *
 *      Deletes front and tail double or single quotes
 *   if any.
 *
 **********************************************************/
static string clean_up(const char* str)
{
    if (! str)
        return {};

    size_t b = 0;
    size_t e = StringLen(str);

    if (b < e && str[e - 1] == ';')
        --e;
    while (b < e && (str[b] == '\"' || str[b] == '\''))
        b++;
    while (b < e && (str[e - 1] == '\"' || str[e - 1] == '\''))
        e--;

    if (b < e)
        return string(str + b, str + e);
    return {};
}

static CRef<CPub> get_num(char* str)
{
    int serial_num = NStr::StringToInt(str, NStr::fAllowTrailingSymbols);

    CRef<CPub> ret(new CPub);
    ret->SetGen().SetSerial_number(serial_num);

    return ret;
}

static CRef<CPub> get_muid(char* str, Parser::EFormat format)
{
    char* p;
    Int4  i;

    CRef<CPub> muid;

    if (! str)
        return muid;

    if (format == Parser::EFormat::GenBank || format == Parser::EFormat::XML)
        p = str;
    else if (format == Parser::EFormat::EMBL) {
        p = StringIStr(str, "MEDLINE;");
        if (! p)
            return muid;
        for (p += 8; *p == ' ';)
            p++;
    } else
        return muid;

    i = NStr::StringToInt(p, NStr::fAllowTrailingSymbols);
    if (i < 1)
        return muid;

    muid.Reset(new CPub);
    muid->SetMuid(ENTREZ_ID_FROM(int, i));
    return muid;
}

/**********************************************************/
static char* get_embl_str_pub_id(char* str, const Char* tag)
{
    const char* p;
    const char* q;

    if (! str || ! tag)
        return nullptr;

    p = StringIStr(str, tag);
    if (! p)
        return nullptr;
    for (p += StringLen(tag); *p == ' ';)
        p++;

    for (q = p; *q != ' ' && *q != '\0';)
        q++;
    q--;
    if (*q != '.')
        q++;
    return StringSave(string_view(p, q - p));
}

/**********************************************************/
static TEntrezId get_embl_pmid(char* str)
{
    char* p;
    long  i;

    if (! str)
        return ZERO_ENTREZ_ID;

    p = StringIStr(str, "PUBMED;");
    if (! p)
        return ZERO_ENTREZ_ID;
    for (p += 7; *p == ' ';)
        p++;
    i = atol(p);
    if (i <= 0)
        return ZERO_ENTREZ_ID;
    return ENTREZ_ID_FROM(long, i);
}

/**********************************************************
 *
 *   static char* check_book_tit(title):
 *
 *      Get volume from book title.
 *
 *                                              12-4-93
 *
 **********************************************************/
static char* check_book_tit(char* title)
{
    char* p;
    char* q;
    char* r;

    p = StringRStr(title, "Vol");
    if (! p)
        return nullptr;

    if (p[3] == '.')
        q = p + 4;
    else if (StringEquN(p + 3, "ume", 3))
        q = p + 6;
    else
        return nullptr;

    while (*q == ' ' || *q == '\t')
        q++;
    for (r = q; *r >= '0' && *r <= '9';)
        r++;

    if (r == q || *r != '\0')
        return nullptr;

    if (p > title) {
        p--;
        if (*p != ' ' && *p != '\t' && *p != ',' && *p != ';' && *p != '.')
            return nullptr;

        while (*p == ' ' || *p == '\t' || *p == ',' || *p == ';' || *p == '.') {
            if (p == title)
                break;
            p--;
        }
        if (*p != ' ' && *p != '\t' && *p != ',' && *p != ';' && *p != '.')
            p++;
    }
    *p = '\0';

    return (q);
}

/**********************************************************
 *
 *   static CitPatPtr get_pat(pp, bptr, auth, title, eptr):
 *
 *      Return a CitPat pointer for patent ref in ncbi or
 *   embl or ddbj.
 *      Leading "I" or "AR" for NCBI or "A" for EMBL or
 *   "E" for DDBJ in accesion number requiered
 *
 *   JOURNAL   Patent: US 4446235-A 6 01-MAY-1984;
 *   or
 *   RL   Patent number US4446235-A/6, 01-MAY-1984.
 *
 *                                              11-14-93
 *
 **********************************************************/
static CRef<CCit_pat> get_pat(ParserPtr pp, char* bptr, CRef<CAuth_list>& auth_list, CRef<CTitle::C_E>& title, char* eptr)
{
    IndexblkPtr ibp;

    CRef<CCit_pat> cit_pat;

    char* country;
    char* number;
    char* type;
    char* app;
    char* s;
    char* p;
    char* q;
    char* temp;

    ErrSev sev;
    Char   ch;

    ibp = pp->entrylist[pp->curindx];

    temp = StringSave(bptr);

    ch = (pp->format == Parser::EFormat::EMBL) ? '.' : ';';
    p  = StringChr(temp, ch);
    if (p)
        *p = '\0';

    p = StringChr(bptr, ch);
    if (p)
        *p = '\0';

    if (ibp->is_pat && ibp->psip.NotEmpty()) {
        FtaErrPost(SEV_ERROR, ERR_FORMAT_MultiplePatRefs, "Too many patent references for patent sequence; ignoring all but the first.");
    }

    if (pp->source == Parser::ESource::USPTO)
        s = bptr;
    else {
        q          = (pp->format == Parser::EFormat::EMBL) ? (char*)"Patent number" : (char*)"Patent:";
        size_t len = StringLen(q);
        if (! StringEquNI(bptr, q, len)) {
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "Illegal format: \"{}\"", temp);
            MemFree(temp);
            return cit_pat;
        }

        for (s = bptr + len; *s == ' ';)
            s++;
    }

    for (country = s, q = s; isalpha((int)*s) || *s == ' '; s++)
        if (*s != ' ')
            q = s;
    if (country == q) {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "No Patent Document Country: \"{}\"", temp);
        MemFree(temp);
        return cit_pat;
    }
    s = q + 1;

    if (pp->format != Parser::EFormat::EMBL &&
        pp->format != Parser::EFormat::XML)
        *s++ = '\0';
    while (*s == ' ')
        s++;
    for (number = s, q = s; isdigit((int)*s) != 0 || *s == ','; s++)
        if (*s != ',')
            *q++ = *s;

    if (number == s) {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "No Patent Document Number: \"{}\"", temp);
        MemFree(temp);
        return cit_pat;
    }

    if (q != s)
        *q = '\0';

    if (*s == '-') {
        *s++ = '\0';
        for (type = s; *s != ' ' && *s != '/' && *s != '\0';)
            s++;
        if (type == s)
            type = nullptr;
    } else
        type = nullptr;
    if (*s != '\0')
        *s++ = '\0';

    if (! type) {
        sev = (ibp->is_pat ? SEV_ERROR : SEV_WARNING);
        FtaErrPost(sev, ERR_REFERENCE_Fail_to_parse, "No Patent Document Type: \"{}\"", temp);
    }

    for (app = s, q = s; *s >= '0' && *s <= '9';)
        s++;
    if (*s != '\0' && *s != ',' && *s != '.' && *s != ' ' && *s != ';' &&
        *s != '\n') {
        sev = (ibp->is_pat ? SEV_ERROR : SEV_WARNING);
        FtaErrPost(sev, ERR_REFERENCE_Fail_to_parse, "No number of sequence in patent: \"{}\"", temp);
        app = nullptr;
        s   = q;
    } else if (*s != '\0')
        for (*s++ = '\0'; *s == ' ';)
            s++;

    CRef<CDate_std> std_date;
    if (*s != '\0') {
        std_date = get_full_date(s, true, pp->source);
    }

    if (std_date.Empty()) {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "Illegal format: \"{}\"", temp);
        MemFree(temp);
        return cit_pat;
    }

    if (p)
        *p = ch;

    string msg = NStr::Sanitize(number);
    if (pp->format == Parser::EFormat::EMBL ||
        pp->source == Parser::ESource::USPTO)
        *number = '\0';

    cit_pat.Reset(new CCit_pat);

    cit_pat->SetCountry(country);
    cit_pat->SetNumber(msg);

    cit_pat->SetDoc_type(type ? type : "");
    cit_pat->SetDate_issue().SetStd(*std_date);
    cit_pat->SetTitle(title.Empty() ? "" : title->GetName());

    if (auth_list.Empty() || ! auth_list->IsSetNames()) {
        CAuth_list& pat_auth_list = cit_pat->SetAuthors();
        pat_auth_list.SetNames().SetStr().push_back("");
    } else
        cit_pat->SetAuthors(*auth_list);

    if (auth_list.NotEmpty()) {
        CAffil& affil = auth_list->SetAffil();

        s += 13;
        if (s < eptr && *s != '\0')
            affil.SetStr(s);
        else
            affil.SetStr("");
    }

    if (ibp->is_pat && ibp->psip.Empty()) {
        ibp->psip = new CPatent_seq_id;
        ibp->psip->SetCit().SetCountry(country);
        ibp->psip->SetCit().SetId().SetNumber(msg);
        ibp->psip->SetSeqid(app ? atoi(app) : 0);
        if (type)
            ibp->psip->SetCit().SetDoc_type(type);
    }

    MemFree(temp);
    return cit_pat;
}

/**********************************************************/
static void fta_get_part_sup(char* parts, CImprint& imp)
{
    char* start;
    char* end;
    char* p;
    char* q;
    Char  ch;
    Int4  i;
    Int4  j;

    if (! parts || *parts == '\0')
        return;

    for (p = parts, i = 0, j = 0; *p != '\0'; p++) {
        if (*p == '(')
            i++;
        else if (*p == ')')
            j++;

        if (j > i || i - j > 1)
            break;
    }

    if (*p != '\0' || i < 2)
        return;

    start = StringChr(parts, '(');
    end   = StringChr(start + 1, ')');

    for (p = start + 1; *p == ' ';)
        p++;
    if (p == end)
        return;

    for (q = end - 1; *q == ' ' && q > p;)
        q--;
    if (*q != ' ')
        q++;

    ch = *q;
    *q = '\0';

    imp.SetPart_sup(p);
    *q = ch;

    fta_StringCpy(start, end + 1);
}

/**********************************************************
 *
 *   static bool get_parts(bptr, eptr, imp):
 *
 *      Return a PARTS from medart2asn.c.
 *
 **********************************************************/
static bool get_parts(char* bptr, char* eptr, CImprint& imp)
{
    char* parts;
    char* p;
    char* q;
    Int4  bad;

    if (! bptr || ! eptr)
        return false;

    parts = StringSave(string_view(bptr, eptr - bptr));

    for (p = parts; *p != '\0'; p++)
        if (*p == '\t')
            *p = ' ';

    fta_get_part_sup(parts, imp);

    bad = 0;
    q   = StringChr(parts, '(');
    p   = StringChr(parts, ')');

    if (p && q) {
        if (p < q || StringChr(p + 1, ')') || StringChr(q + 1, '('))
            bad = 1;
    } else if (p || q)
        bad = 1;

    if (bad != 0) {
        MemFree(parts);
        return false;
    }

    if (q) {
        *q++ = '\0';
        *p   = '\0';

        for (p = q; *p == ' ';)
            p++;
        for (q = p; *q != '\0' && *q != ' ';)
            q++;
        if (*q != '\0')
            *q++ = '\0';
        if (q > p)
            imp.SetIssue(p);
        for (p = q; *p == ' ';)
            p++;
        for (q = p; *q != '\0';)
            q++;
        if (q > p) {
            for (q--; *q == ' ';)
                q--;
            *++q = '\0';

            string supi(" ");
            supi += p;
            imp.SetPart_supi(supi);
        }

        const Char* issue_str = imp.IsSetIssue() ? imp.GetIssue().c_str() : nullptr;
        if (imp.IsSetPart_supi() && issue_str &&
            (issue_str[0] == 'P' || issue_str[0] == 'p') && (issue_str[1] == 'T' || issue_str[1] == 't') &&
            issue_str[2] == '\0') {
            string& issue = imp.SetIssue();
            issue += imp.GetPart_supi();
            imp.ResetPart_supi();
        }
    }

    for (p = parts; *p == ' ';)
        p++;
    for (q = p; *q != '\0' && *q != ' ';)
        q++;
    if (*q != '\0')
        *q++ = '\0';
    if (q > p)
        imp.SetVolume(p);
    for (p = q; *p == ' ';)
        p++;
    for (q = p; *q != '\0';)
        q++;
    if (q > p) {
        for (q--; *q == ' ';)
            q--;
        *++q = '\0';
        imp.SetPart_sup(p);
    }

    MemFree(parts);
    return true;
}

/**********************************************************
 *
 *   static CitArtPtr get_art(pp, bptr, auth, title, pre,
 *                            has_muid, all_zeros, er):
 *
 *      Return a CitArt pointer for GENBANK or EMBL mode.
 *
 **********************************************************/
static CRef<CCit_art> get_art(ParserPtr pp, char* bptr, CRef<CAuth_list>& auth_list, CRef<CTitle::C_E>& title, CImprint::EPrepub pre, bool has_muid, bool* all_zeros, Int4 er)
{
    char* eptr;
    char* end_tit;
    char* s;
    char* ss;
    char* end_volume;
    char* end_pages;
    char* tit    = nullptr;
    char* volume = nullptr;
    char* pages  = nullptr;
    char* year;
    Char  symbol;

    Int4 i;
    Int4 is_er;

    *all_zeros = false;

    is_er = 0;
    if (er > 0)
        is_er |= 01; /* based on REMARKs */
    if (fta_StartsWith(bptr, "(er)"sv))
        is_er |= 02;

    CRef<CCit_art> cit_art;

    if (pp->format == Parser::EFormat::GenBank)
        symbol = ',';
    else if (pp->format == Parser::EFormat::EMBL)
        symbol = ':';
    else if (pp->format == Parser::EFormat::XML) {
        if (pp->source == Parser::ESource::EMBL)
            symbol = ':';
        else
            symbol = ',';
    } else
        return cit_art;

    end_volume = nullptr;

    size_t             len = StringLen(bptr);
    unique_ptr<char[]> pBuf(new char[len + 1]);
    char*              buf = pBuf.get();
    StringCpy(buf, bptr);
    eptr = buf + len - 1;
    while (eptr > buf && (*eptr == ' ' || *eptr == '\t' || *eptr == '.'))
        *eptr-- = '\0';
    if (*eptr != ')') {
        return cit_art;
    }
    for (s = eptr - 1; s > buf && *s != '(';)
        s--;
    if (*s != '(') {
        return cit_art;
    }

    year = s + 1;
    for (s--; s >= buf && isspace((int)*s) != 0;)
        s--;
    if (s < buf)
        s = buf;
    end_pages = s + 1;
    if (buf[0] == 'G' && buf[1] == '3')
        ss = buf + 2;
    else
        ss = buf;
    for (i = 0; ss <= year; ss++) {
        if (*ss == '(')
            i++;
        else if (*ss == ')')
            i--;
        else if (*ss >= '0' && *ss <= '9' && i == 0)
            break;
    }

    for (s = end_pages; s >= buf && *s != symbol;)
        s--;
    if (s < buf)
        s = buf;
    if (*s != symbol) {
        /* try delimiter from other format
         */
        if (pp->format == Parser::EFormat::GenBank)
            symbol = ':';
        else if (pp->format == Parser::EFormat::EMBL)
            symbol = ',';
        else if (pp->format == Parser::EFormat::XML) {
            if (pp->source == Parser::ESource::EMBL)
                symbol = ',';
            else
                symbol = ':';
        }

        for (s = end_pages; s >= buf && *s != symbol;)
            s--;
        if (s < buf)
            s = buf;
    }

    if (*s == symbol && ss != year) {
        if (ss > s)
            ss = s + 1;
        end_volume = s;
        for (pages = s + 1; isspace(*pages) != 0;)
            pages++;
        end_tit = ss - 1;
        if (end_volume > ss) {
            volume = ss;
            if (*end_tit == '(')
                volume--;
        }
    } else {
        if (pre != CImprint::ePrepub_submitted)
            pre = CImprint::ePrepub_in_press;

        end_tit = end_pages;
    }

    if (*year == '0') {
        if (pages && fta_StartsWith(pages, "0-0"sv) &&
            pp->source == Parser::ESource::EMBL)
            *all_zeros = true;
        return cit_art;
    }

    tit = buf;
    if (*tit == '\0') {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "No journal title.");
        return cit_art;
    }

    cit_art.Reset(new CCit_art);
    CCit_jour& journal = cit_art->SetFrom().SetJournal();
    CImprint&  imp     = journal.SetImp();

    if (pre > 0)
        imp.SetPrepub(pre);

    *end_pages = '\0';
    if (pages && ! fta_StartsWith(pages, "0-0"sv)) {
        i = valid_pages_range(pages, tit, is_er, (pre == CImprint::ePrepub_in_press));
        if (i == 0)
            imp.SetPages(pages);
        else if (i == 1)
            end_tit = end_pages;
        else if (i == -1 && is_er > 0) {
            cit_art.Reset();
            return cit_art;
        }
    } else if (pre != CImprint::ePrepub_submitted)
        pre = CImprint::ePrepub_in_press;

    if (volume) {
        if (! get_parts(volume, end_volume, imp)) {
            cit_art.Reset();
            return cit_art;
        }

        if (pre != CImprint::ePrepub_submitted && ! imp.IsSetVolume()) {
            if (imp.IsSetPages()) {
                cit_art.Reset();
                return cit_art;
            }
            pre = CImprint::ePrepub_in_press;
        }
    } else if (is_er > 0 && pre != CImprint::ePrepub_in_press) {
        cit_art.Reset();
        return cit_art;
    }

    CRef<CDate> date;
    if (*year != '0')
        date = get_date(year);

    if (date.Empty()) {
        if (is_er == 0)
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "No date in journal reference");

        cit_art.Reset();
        return cit_art;
    }

    *end_tit = '\0';

    CRef<CTitle::C_E> journal_title(new CTitle::C_E);

    for (char* aux = end_tit - 1; aux > tit && *aux != '.' && *aux != ')' && ! isalnum(*aux); --aux)
        *aux = 0;

    journal_title->SetIso_jta(NStr::Sanitize(tit));
    journal.SetTitle().Set().push_back(journal_title);

    imp.SetDate(*date);
    if (pre > 0)
        imp.SetPrepub(pre);

    if ((is_er & 01) == 01) {
        if (er == 1)
            imp.SetPubstatus(ePubStatus_epublish);
        else
            imp.SetPubstatus(ePubStatus_aheadofprint);
    }

    /* check invalid "in-press"
     */
    if (pre == CImprint::ePrepub_in_press) {
        if (has_muid) {
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_InvalidInPress, "Reference flagged as In-press, but Medline UID exists, In-press ignored: {}", buf);
            imp.ResetPrepub();
        }

        if (imp.IsSetPages() && imp.IsSetVolume() && imp.IsSetDate()) {
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_InvalidInPress, "Reference flagged as In-press, but citation is complete, In-press ignored: {}", buf);
            imp.ResetPrepub();
        }
    }

    /* Title and authors are optional for cit_art
     */
    if (title)
        cit_art->SetTitle().Set().push_back(title);

    if (auth_list.NotEmpty())
        cit_art->SetAuthors(*auth_list);

    return cit_art;
}

/**********************************************************
 *
 *   static CitGenPtr get_unpub(bptr, eptr, auth, title):
 *
 *      Return a CitGen pointer.
 *
 *                                              11-14-93
 *
 **********************************************************/
static CRef<CCit_gen> get_unpub(char* bptr, char* eptr, CRef<CAuth_list>& auth_list, const Char* title)
{
    CRef<CCit_gen> cit_gen(new CCit_gen);

    char* s;
    char* str;

    if (bptr) {
        for (s = bptr; *s != '\0' && *s != '(';)
            s++;
        for (str = s - 1; str > bptr && isspace(*str) != 0;)
            str--;
        if (*s == '(')
            s += 6;

        if (s < eptr && *s != '\0' && auth_list.NotEmpty())
            auth_list->SetAffil().SetStr(NStr::Sanitize(s));

        cit_gen->SetCit(string(bptr, str + 1));
    }

    if (auth_list.NotEmpty())
        cit_gen->SetAuthors(*auth_list);

    if (title)
        cit_gen->SetTitle(title);

    return cit_gen;
}

/**********************************************************
 *
 *   static CitArtPtr get_book(bptr, auth, title, pre,
 *                             format, p):
 *
 *      Return a CitArt pointer (!!! that is an article
 *   from book!!).
 *
 *                                              11-14-93
 *
 **********************************************************/
static CRef<CCit_art> get_book(char* bptr, CRef<CAuth_list>& auth_list, CRef<CTitle::C_E>& title, CImprint::EPrepub pre, Parser::EFormat format, string_view jour)
{
    char* s;
    char* ss;
    char* tit;
    char* volume;
    char* pages;
    char* press;

    ERefFormat ref_fmt;
    bool  IS_AUTH = false;
    char* tbptr;
    char* p;
    Char  c;
    Int4  i;

    tit     = nullptr;
    ref_fmt = GB_REF;

    tbptr = bptr ? StringSave(bptr) : nullptr;

    switch (format) {
    case Parser::EFormat::EMBL:
        ref_fmt = EMBL_REF;
        break;
    case Parser::EFormat::GenBank:
        ref_fmt = GB_REF;
        break;
    case Parser::EFormat::SPROT:
        ref_fmt = SP_REF;
        break;
    default:
        break;
    }

    CRef<CCit_art> cit_art(new CCit_art);
    CCit_book&     cit_book = cit_art->SetFrom().SetBook();

    if (pre > 0)
        cit_book.SetImp().SetPrepub(pre);

    p = tbptr;
    CRef<CTitle::C_E> book_title(new CTitle::C_E);

    if (StringEquN(tbptr, "(in)", 4)) {
        for (s = tbptr + 4; *s == ' ';)
            s++;
        for (bptr = s; *s != ';' && *s != '(' && *s != '\0';)
            s++;
        if (StringEquNI(s, "(Eds.)", 6)) {
            tit     = s + 6;
            IS_AUTH = true;
        } else if (StringEquNI(s, "(Ed.)", 5)) {
            tit     = s + 5;
            IS_AUTH = true;
        } else if (*s == ';')
            tit = s;
        if (tit)
            while (*tit == ' ' || *tit == ';' || *tit == '\n')
                tit++;
        *s++ = '\0';
        if (IS_AUTH && *bptr != '\0') {
            CRef<CAuth_list> book_auth_list;
            get_auth(bptr, ref_fmt, jour, book_auth_list);
            if (book_auth_list.NotEmpty())
                cit_book.SetAuthors(*book_auth_list);
        } else {
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_UnusualBookFormat, "Cannot parse unusually formatted book reference (generating Cit-gen instead): {}", p);
            if (tbptr)
                MemFree(tbptr);

            cit_art.Reset();
            return cit_art;
        }

        ss = StringRChr(tit, ';');
        if (! ss)
            for (ss = tit; *ss != '\0';)
                ss++;
        for (s = ss; *s != ':' && s != tit;)
            s--;
        if (*s != ':')
            s = ss;
        c = *s;
        if (*s != '\0')
            *s++ = '\0';

        book_title->SetName("");
        if (*tit != '\0') {
            volume = check_book_tit(tit);
            if (volume)
                cit_book.SetImp().SetVolume(volume);

            book_title->SetName(NStr::Sanitize(tit));
        }

        if (c == ':') {
            for (pages = s; *s != '\0' && *s != ',' && *s != ';';)
                s++;
            if (*s != '\0')
                *s++ = '\0';

            while (*pages == ' ')
                pages++;

            if (fta_StartsWith(pages, "0-0"sv))
                cit_book.SetImp().SetPrepub(CImprint::ePrepub_in_press);
            else {
                bool is_in_press = cit_book.GetImp().IsSetPrepub() && cit_book.GetImp().GetPrepub() == CImprint::ePrepub_in_press;
                i                = valid_pages_range(pages, book_title->GetName().c_str(), 0, is_in_press);

                if (i == 0)
                    cit_book.SetImp().SetPages(NStr::Sanitize(pages));
                else if (i == 1) {
                    string new_title = book_title->GetName();
                    new_title += ": ";
                    new_title += pages;
                    book_title->SetName(new_title);
                }
            }
        }

        for (press = s; *s != '(' && *s != '\0';)
            s++;
        if (*s != '\0')
            *s++ = '\0';

        cit_book.SetImp().SetPub().SetStr(NStr::Sanitize(press));

        CRef<CDate> date = get_date(s);
        if (date.Empty()) {
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "No date in book reference");
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_Illegalreference, "Book format error (cit-gen created): {}", p);
            if (tbptr)
                MemFree(tbptr);

            cit_art.Reset();
            return cit_art;
        }

        cit_book.SetImp().SetDate(*date);
    }

    cit_book.SetTitle().Set().push_back(book_title);

    if (title.NotEmpty())
        cit_art->SetTitle().Set().push_back(title);

    if (auth_list.NotEmpty())
        cit_art->SetAuthors(*auth_list);

    if (tbptr)
        MemFree(tbptr);

    return cit_art;
}

/**********************************************************
 *
 *   static CitBookPtr get_thesis(bptr, auth, title, pre):
 *
 *      Return a CitBook pointer.
 *
 *                                              11-14-93
 *
 **********************************************************/
static CRef<CCit_let> get_thesis(char* bptr, CRef<CAuth_list>& auth_list, CRef<CTitle::C_E>& title, CImprint::EPrepub pre)
{
    CRef<CCit_let> cit_let(new CCit_let);

    cit_let->SetType(CCit_let::eType_thesis);

    CCit_book& book = cit_let->SetCit();

    if (pre > 0)
        book.SetImp().SetPrepub(pre);

    char* s;
    for (s = bptr; *s != '\0' && *s != '(';)
        s++;

    if (*s == '(') {
        CRef<CDate> date = get_date(s + 1);
        if (date.NotEmpty())
            book.SetImp().SetDate(*date);

        s = s + 6;
    }

    if (! book.GetImp().IsSetDate()) {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "Fail to parse thesis: missing date");

        cit_let.Reset();
        return cit_let;
    }

    if (*s != '\0')
        book.SetImp().SetPub().SetStr(NStr::Sanitize(s));

    if (title.NotEmpty())
        book.SetTitle().Set().push_back(title);
    else {
        FtaErrPost(SEV_WARNING, ERR_REFERENCE_Thesis, "Missing thesis title");

        CRef<CTitle::C_E> empty_title(new CTitle::C_E);
        empty_title->SetName("");
        book.SetTitle().Set().push_back(empty_title);
    }

    if (auth_list.NotEmpty())
        book.SetAuthors(*auth_list);
    return cit_let;
}

/**********************************************************
 *
 *   static CitBookPtr get_whole_book(bptr, auth, title,
 *                                    pre):
 *
 *      Return a CitBook pointer.
 *
 *                                              11-14-93
 *
 **********************************************************/
static CRef<CCit_book> get_whole_book(char* bptr, CRef<CAuth_list>& auth_list, CRef<CTitle::C_E>& title, CImprint::EPrepub pre)
{
    CRef<CCit_book> cit_book;

    char* s;

    for (bptr += 5; isspace(*bptr) != 0;)
        bptr++;


    for (s = bptr; *s != '\0' && *s != '(';)
        s++;

    if (*s != '(') {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "Fail to parse book: missing date");
        return cit_book;
    }

    cit_book.Reset(new CCit_book);

    if (pre > 0)
        cit_book->SetImp().SetPrepub(pre);

    CRef<CDate> date = get_date(s + 1);
    if (date.NotEmpty())
        cit_book->SetImp().SetDate(*date);

    *s = '\0';
    for (s = bptr; *s != '\0' && *s != '.';)
        s++;

    CRef<CTitle::C_E> book_title(new CTitle::C_E);
    book_title->SetName(string(bptr, s));
    cit_book->SetTitle().Set().push_back(book_title);

    if (*s == '.') {
        for (s++; isspace(*s) != 0;)
            s++;

        cit_book->SetImp().SetPub().SetStr(NStr::Sanitize(s));
    }

    if (auth_list.Empty() || ! auth_list->IsSetNames()) {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "Fail to parse thesis: missing thesis author");
        cit_book.Reset();
        return cit_book;
    }

    cit_book->SetAuthors(*auth_list);

    return cit_book;
}

/**********************************************************
 *
 *   static CitSubPtr get_sub(pp, bptr, auth):
 *
 *      Return a CitSub pointer.
 *
 **********************************************************/
static CRef<CCit_sub> get_sub(ParserPtr pp, char* bptr, CRef<CAuth_list>& auth_list)
{
    const char** b;
    char*        s;

    CCit_sub::TMedium medium = CCit_sub::eMedium_other;

    CRef<CCit_sub> ret;

    for (s = bptr; *s != '(' && *s != '\0';)
        s++;
    if (*s == '\0') {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "Fail to parse submission: missing date");
        return ret;
    }

    ret.Reset(new CCit_sub);
    CRef<CDate> date;

    if (pp && ! pp->entrylist.empty() &&
        IsNewAccessFormat(pp->entrylist[pp->curindx]->acnum) == 0 &&
        StringChr(ParFlat_LANL_AC, pp->entrylist[pp->curindx]->acnum[0]) &&
        isdigit((int)*(s + 1)) == 0) {
        date = get_lanl_date(s);
    } else {
        CRef<CDate_std> std_date = get_full_date(s + 1, true, pp->source);
        if (std_date) {
            date.Reset(new CDate);
            date->SetStd(*std_date);
        }
    }

    if (date.Empty())
        return ret;

    ret.Reset(new CCit_sub);
    ret->SetDate(*date);

    s = s + 13;
    if (StringStr(s, "E-mail"))
        medium = CCit_sub::eMedium_email;

    if (StringEquNI(s, " on tape", 8)) {
        medium = CCit_sub::eMedium_tape;
        for (s += 8; *s != '\0' && *s != ':';)
            s++;
    }
    if (*s != '\0' && *(s + 1) != '\0') {
        while (*s == ' ')
            s++;

        if (*s == ':')
            s++;
        for (;;) {
            for (b = strip_sub_str; *b; b++) {
                size_t l_str = StringLen(*b);
                if (StringEquN(s, *b, l_str)) {
                    for (s += l_str; *s == ' ' || *s == '.';)
                        s++;
                    break;
                }
            }
            if (! *b)
                break;
        }

        if (*s != '\0' && auth_list.NotEmpty()) {
            auth_list->SetAffil().SetStr(NStr::Sanitize(s));
        }
    }

    if (*s == '\0') {
        FtaErrPost(SEV_WARNING, ERR_REFERENCE_NoContactInfo, "Missing contact info : {}", bptr);
    }

    if (auth_list.Empty() || ! auth_list->IsSetNames()) {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "Direct submission: missing author (cit-gen created)");

        ret.Reset();
        return ret;
    }

    ret->SetAuthors(*auth_list);
    ret->SetMedium(medium);

    return ret;
}

/**********************************************************
 *
 *   static CitSubPtr get_sub_gsdb(bptr, auth, title, pp):
 *
 *      GSDB specific format for CitSub :
 *   REFERENCE   1  (bases 1 to 378)
 *     AUTHORS   Mundt,M.O.
 *     TITLE     Published by M.O. Mundt, Genomics LS-3,
 *               Los Alamos National Laboratory,
 *               Mail Stop M888, Los Alamos, NM, USA, 87545
 *     JOURNAL   Published in GSDB (11-OCT-1996)
 *
 **********************************************************/
static CRef<CCit_sub> get_sub_gsdb(char* bptr, CRef<CAuth_list>& auth_list, CRef<CTitle::C_E>& title, ParserPtr pp)
{
    CRef<CCit_sub> cit_sub;

    char* s;

    for (s = bptr; *s != '(' && *s != '\0';)
        s++;
    if (*s == '\0') {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "Fail to parse submission: missing date");
        return cit_sub;
    }

    CRef<CDate_std> std_date = get_full_date(s + 1, true, pp->source);
    if (std_date.Empty())
        return cit_sub;

    CRef<CDate> date;
    date->SetStd(*std_date);

    if (auth_list.Empty() || ! auth_list->IsSetNames()) {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "Direct submission: missing author (cit-gen created)");
        return cit_sub;
    }

    cit_sub.Reset(new CCit_sub);
    cit_sub->SetAuthors(*auth_list);
    cit_sub->SetDate(*date);

    if (title.NotEmpty()) {
        const Char* s     = title->GetName().c_str();
        size_t      l_str = StringLen("Published by");
        if (StringEquN(s, "Published by", l_str)) {
            s += l_str;
            while (*s == ' ')
                s++;
        }

        if (*s != '\0') {
            auth_list->SetAffil().SetStr(NStr::Sanitize(s));
        } else {
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_NoContactInfo, "Missing contact info : {}", bptr);
        }
    } else {
        FtaErrPost(SEV_WARNING, ERR_REFERENCE_NoContactInfo, "Missing contact info : {}", bptr);
    }

    return cit_sub;
}

/**********************************************************/
static CRef<CCit_gen> fta_get_citgen(char* bptr, CRef<CAuth_list>& auth_list, CRef<CTitle::C_E>& title)
{
    CRef<CCit_gen> cit_gen;

    char* p;
    char* q;
    char* r;
    Char  ch;
    Int2  year;

    if (! bptr || auth_list.Empty() || ! auth_list->IsSetNames() || title.Empty())
        return cit_gen;

    year = 0;
    p    = StringChr(bptr, '(');
    if (p) {
        for (p++; *p == ' ' || *p == '\t';)
            p++;
        for (q = p; *p >= '0' && *p <= '9';)
            p++;
        for (r = p; *p == ' ' || *p == '\t' || *p == ')';)
            p++;
        if (*p == '\n' || *p == '\0') {
            ch   = *r;
            *r   = '\0';
            year = atoi(q);
            if (year < 1900)
                *r = ch;
            else {
                for (q--; *q == ' ' || *q == '\t' || *q == '(';)
                    q--;
                *++q = '\0';
            }
        }
    }

    cit_gen.Reset(new CCit_gen);

    if (bptr)
        cit_gen->SetCit(bptr);

    cit_gen->SetAuthors(*auth_list);
    cit_gen->SetTitle(title->GetName());

    if (year >= 1900)
        cit_gen->SetDate().SetStd().SetYear(year);

    return cit_gen;
}

CRef<CPub> journal(ParserPtr pp, char* bptr, char* eptr, CRef<CAuth_list>& auth_list, CRef<CTitle::C_E>& title, bool has_muid, CRef<CCit_art>& cit_art, Int4 er)
{
    CImprint::EPrepub pre = static_cast<CImprint::EPrepub>(0);

    char* p;
    char* nearend;
    char* end;

    ERefRetType retval = ParFlat_MISSING_JOURNAL;

    CRef<CPub> ret(new CPub);
    if (! bptr) {
        const Char* title_str = title.Empty() ? nullptr : title->GetName().c_str();
        ret->SetGen(*get_unpub(bptr, eptr, auth_list, title_str));
        return ret;
    }

    p             = bptr;
    size_t my_len = StringLen(p);
    if (my_len > 7) {
        nearend = p + StringLen(p) - 1;
        while (*nearend == ' ' || *nearend == '\t' || *nearend == '.')
            *nearend-- = '\0';

        nearend -= 8;
        end = nearend + 2;
        if (StringEquNI(nearend + 1, "In press", 8)) {
            pre            = CImprint::ePrepub_in_press;
            *(nearend + 1) = '\0';
        }
        if (StringEquNI(nearend, "Submitted", 9)) {
            pre      = CImprint::ePrepub_submitted;
            *nearend = '\0';
        }
        if (pre == 0 && *end == '(' && isdigit(*(end + 1)) != 0) {
            for (nearend = end - 1; nearend > bptr && *nearend != ' ';)
                nearend--;
            if (StringEquNI(nearend + 1, "In press", 8)) {
                pre            = CImprint::ePrepub_in_press;
                *(nearend + 1) = '\0';
            }
        }
    }

    if (my_len >= 6 && *p == '(') {
        p += 6;
        if (fta_StartsWith(p, " In press"sv)) {
            retval = ParFlat_IN_PRESS;
            pre    = CImprint::ePrepub_in_press;
        }
    }

    p = bptr;
    if (fta_StartsWith(p, "Unpub"sv) || fta_StartsWith(p, "Unknown"sv)) {
        retval                = ParFlat_UNPUB_JOURNAL;
        const Char* title_str = title.Empty() ? nullptr : title->GetName().c_str();
        ret->SetGen(*get_unpub(bptr, eptr, auth_list, title_str));
    } else if (fta_StartsWith(p, "(in)"sv)) {
        retval = ParFlat_MONOGRAPH_NOT_JOURNAL;

        CRef<CCit_art> article = get_book(bptr, auth_list, title, pre, pp->format, p);

        if (article.Empty())
            ret->SetGen(*get_error(bptr, auth_list, title));
        else
            ret->SetArticle(*article);

    } else if (fta_StartsWith(p, "Thesis"sv)) {
        retval = ParFlat_THESIS_CITATION;

        CRef<CCit_let> cit_let = get_thesis(bptr, auth_list, title, pre);
        if (cit_let.Empty()) {
            ret.Reset();
            return ret;
        }
        ret->SetMan(*cit_let);
    } else if (fta_StartsWith(p, "Submi"sv)) {
        retval = ParFlat_SUBMITTED;

        CRef<CCit_sub> cit_sub = get_sub(pp, bptr, auth_list);
        if (cit_sub.Empty()) {
            ret.Reset();
            return ret;
        }

        ret->SetSub(*cit_sub);
    } else if (fta_StartsWith(p, "Published in GSDB"sv)) {
        FtaErrPost(SEV_WARNING, ERR_REFERENCE_GsdbRefDropped, "A published-in-gsdb reference was encountered and has been dropped [{}]", bptr);
        retval = ParFlat_SUBMITTED;

        CRef<CCit_sub> cit_sub = get_sub_gsdb(bptr, auth_list, title, pp);
        if (cit_sub.Empty()) {
            ret.Reset();
            return ret;
        }

        ret->SetSub(*cit_sub);
    } else if (fta_StartsWith(p, "Patent"sv) ||
               pp->source == Parser::ESource::USPTO) {
        retval = ParFlat_PATENT_CITATION;

        if (pp->seqtype == CSeq_id::e_Genbank || pp->seqtype == CSeq_id::e_Ddbj ||
            pp->seqtype == CSeq_id::e_Embl || pp->seqtype == CSeq_id::e_Other ||
            pp->seqtype == CSeq_id::e_Tpe || pp->seqtype == CSeq_id::e_Tpg ||
            pp->seqtype == CSeq_id::e_Tpd ||
            pp->source == Parser::ESource::USPTO) {
            CRef<CCit_pat> cit_pat = get_pat(pp, bptr, auth_list, title, eptr);
            if (cit_pat.Empty()) {
                ret.Reset();
                return ret;
            }

            ret->SetPatent(*cit_pat);
        } else {
            ret.Reset();
            return ret;
        }
    } else if (fta_StartsWith(p, "Book:"sv)) {
        retval = ParFlat_BOOK_CITATION;

        CRef<CCit_book> book = get_whole_book(bptr, auth_list, title, pre);
        if (book.Empty()) {
            ret.Reset();
            return ret;
        }

        ret->SetBook(*book);
    } else if (NStr::StartsWith(p, "Published Only in Database"sv, NStr::eNocase)) {
        retval                 = ParFlat_GEN_CITATION;
        CRef<CCit_gen> cit_gen = fta_get_citgen(bptr, auth_list, title);

        if (cit_gen.Empty()) {
            ret.Reset();
            return ret;
        }

        ret->SetGen(*cit_gen);
    } else if (NStr::StartsWith(p, "Online Publication"sv, NStr::eNocase)) {
        retval = ParFlat_ONLINE_CITATION;

        CRef<CCit_gen> cit_gen = fta_get_citgen(bptr, auth_list, title);

        if (cit_gen.Empty()) {
            ret.Reset();
            return ret;
        }

        ret->SetGen(*cit_gen);
    }

    if (retval == ParFlat_MISSING_JOURNAL) {
        if (cit_art.NotEmpty())
            ret->SetArticle(*cit_art);
        else {
            bool all_zeros;
            CRef<CCit_art> new_art = get_art(pp, bptr, auth_list, title, pre, has_muid, &all_zeros, er);
            if (new_art.Empty()) {
                if (! all_zeros && ! fta_StartsWith(bptr, "(er)"sv) && er == 0)
                    FtaErrPost(SEV_WARNING, ERR_REFERENCE_Illegalreference, "Journal format error (cit-gen created): {}", bptr);

                ret->SetGen(*get_error(bptr, auth_list, title));
            } else
                ret->SetArticle(*new_art);
        }
    }

    return ret;
}

/**********************************************************/
static char* FindBackSemicolon(char* pchStart, char* pchCurrent)
{
    if (! pchStart || ! pchCurrent || pchStart >= pchCurrent)
        return nullptr;

    for (pchCurrent--; pchCurrent >= pchStart; pchCurrent--) {
        if (isspace((int)*pchCurrent) != 0)
            continue;
        if (*pchCurrent == ';')
            return (pchCurrent);
        break;
    }

    return nullptr;
}

/**********************************************************/
static char* FindSemicolon(char* str)
{
    if (! str || *str == '\0')
        return nullptr;

    while (*str && std::isspace(*str))
        str++;

    if (*str == ';')
        return (str);

    return nullptr;
}

/**********************************************************/
static char* ExtractErratum(char* comm)
{
    char* start;
    char* pchNumber = nullptr;
    char* end;
    char* p;

    if (! comm)
        return nullptr;

    start = StringStr(comm, "Erratum:");
    if (! start)
        return (comm);

    end = StringChr(start, ']');
    if (! end)
        return (comm);

    pchNumber = end + 1;
    end       = FindSemicolon(pchNumber);
    if (end)
        pchNumber = end + 1;
    p = FindBackSemicolon(comm, start);
    if (p)
        start = p;
    fta_StringCpy(start, pchNumber);

    /* Check if the string after cutting signature is empty. If it's really
     * empty we have to ignore the whole string (comment).
     * Do you want to have a comment which contains nothing!? Probably no.
     */
    for (p = comm; *p == ' ' || *p == '\t' || *p == '\n';)
        p++;
    if (*p == '\0')
        *comm = '\0';

    return (comm);
}

/**********************************************************/
static void XMLGetXrefs(char* entry, const TXmlIndexList& xil, TQualVector& quals)
{
    if (! entry || xil.empty())
        return;

    for (const auto& xip : xil) {
        if (xip.subtags.empty())
            continue;

        CRef<CGb_qual> qual(new CGb_qual);

        for (const auto& xipqual : xip.subtags) {
            if (xipqual.tag == INSDXREF_DBNAME)
                qual->SetQual(*XMLGetTagValue(entry, xipqual));
            else if (xipqual.tag == INSDXREF_ID)
                qual->SetVal(*XMLGetTagValue(entry, xipqual));
        }

        if (qual->IsSetQual() && ! qual->GetQual().empty())
            quals.push_back(qual);
    }
}

/**********************************************************/
static void fta_add_article_ids(CPub& pub, const string& doi, const string& agricola)
{
    if (doi.empty() && agricola.empty())
        return;

    if (pub.IsArticle()) {
        CCit_art& cit_art = pub.SetArticle();

        if (! agricola.empty()) {
            CRef<CArticleId> id(new CArticleId);
            id->SetOther().SetDb("AGRICOLA");
            id->SetOther().SetTag().SetStr(agricola);

            cit_art.SetIds().Set().push_front(id);
        }

        if (! doi.empty()) {
            CRef<CArticleId> id(new CArticleId);
            id->SetDoi().Set(doi);

            cit_art.SetIds().Set().push_front(id);
        }
    }
}

/**********************************************************/
Int4 fta_remark_is_er(const string& str)
{
    const char** b;
    Int4         i;

    string s = str;
    ShrinkSpaces(s);

    for (i = 1, b = ERRemarks; *b; b++, i++) {
        if (StringIStr(s.c_str(), *b)) {
            if (i <= 6)
                return 1; // epublish
            else
                return 2; // aheadofprint
        }
    }

    return 0;
}

/**********************************************************/
static CRef<CPubdesc> XMLRefs(ParserPtr pp, const DataBlk& dbp, bool& no_auth, bool& rej)
{
    char*     p;
    char*     q;
    bool      is_online;
    TEntrezId pmid;

    Int4 er;

    CRef<CPubdesc> desc;

    if (! pp || ! dbp.mBuf.ptr || ! dbp.hasData())
        return desc;

    desc.Reset(new CPubdesc);

    p = StringSave(XMLFindTagValue(dbp.mBuf.ptr, dbp.GetXmlData(), INSDREFERENCE_REFERENCE));
    if (p && isdigit((int)*p) != 0) {
        desc->SetPub().Set().push_back(get_num(p));
    } else {
        FtaErrPost(SEV_WARNING, ERR_REFERENCE_Illegalreference, "No reference number.");
    }

    if (p)
        MemFree(p);

    p = StringSave(XMLFindTagValue(dbp.mBuf.ptr, dbp.GetXmlData(), INSDREFERENCE_MEDLINE));
    if (p) {
        rej = true;
        MemFree(p);
        desc.Reset();
        return desc;
    }

    pmid = ZERO_ENTREZ_ID;
    p    = StringSave(XMLFindTagValue(dbp.mBuf.ptr, dbp.GetXmlData(), INSDREFERENCE_PUBMED));
    if (p) {
        pmid = ENTREZ_ID_FROM(int, NStr::StringToInt(p, NStr::fAllowTrailingSymbols));
        MemFree(p);
    }

    CRef<CAuth_list> auth_list;

    p = StringSave(XMLConcatSubTags(dbp.mBuf.ptr, dbp.GetXmlData(), INSDREFERENCE_AUTHORS, ','));
    if (p) {
        if (pp->xml_comp) {
            q = StringRChr(p, '.');
            if (! q || q[1] != '\0') {
                string s = p;
                s.append(".");
                MemFree(p);
                p = StringSave(s);
                q = nullptr;
            }
        }
        for (q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;
        if (*q != '\0') {
            auto journal = XMLFindTagValue(dbp.mBuf.ptr, dbp.GetXmlData(), INSDREFERENCE_JOURNAL);
            char* r = StringChr(p, ',');
            if (r && ! StringChr(r + 1, '.'))
                *r = '|';
            get_auth(p, (pp->source == Parser::ESource::EMBL) ? EMBL_REF : GB_REF, journal ? *journal : string_view(), auth_list);
        }
        MemFree(p);
    }

    p = StringSave(XMLFindTagValue(dbp.mBuf.ptr, dbp.GetXmlData(), INSDREFERENCE_CONSORTIUM));
    if (p) {
        for (q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;

        if (*q != '\0')
            get_auth_consortium(p, auth_list);

        MemFree(p);
    }

    if (auth_list.Empty() || ! auth_list->IsSetNames())
        no_auth = true;

    p = StringSave(XMLFindTagValue(dbp.mBuf.ptr, dbp.GetXmlData(), INSDREFERENCE_TITLE));

    CRef<CTitle::C_E> title_art(new CTitle::C_E);
    if (p) {
        if (! fta_StartsWith(p, "Direct Submission"sv) &&
            *p != '\0' && *p != ';') {
            string title = clean_up(p);
            if (! title.empty()) {
                title_art->SetName(tata_save(title));
            }
        }
        MemFree(p);
    }

    is_online = false;
    p         = StringSave(XMLFindTagValue(dbp.mBuf.ptr, dbp.GetXmlData(), INSDREFERENCE_JOURNAL));
    if (! p) {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "No JOURNAL line, reference dropped");
        desc.Reset();
        return desc;
    }

    if (*p == '\0' || *p == ';') {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "JOURNAL line is empty, reference dropped");
        MemFree(p);
        desc.Reset();
        return desc;
    }

    if (NStr::EqualNocase(p, 0, 18, "Online Publication"))
        is_online = true;

    if (char* r = StringSave(XMLFindTagValue(dbp.mBuf.ptr, dbp.GetXmlData(), INSDREFERENCE_REMARK))) {
        string comm = NStr::Sanitize(ExtractErratum(r));
        MemFree(r);
        if (! is_online)
            normalize_comment(comm);
        desc->SetComment(comm);
    }

    er = desc->IsSetComment() ? fta_remark_is_er(desc->GetComment()) : 0;

    CRef<CCit_art> cit_art;
    if (pp->medserver == 1 && pmid > ZERO_ENTREZ_ID && (fta_StartsWith(p, "(er)"sv) || er > 0)) {
        cit_art = FetchPubPmId(pmid);
        if (cit_art.Empty())
            pmid = ZERO_ENTREZ_ID;
    }

    if (pmid > ZERO_ENTREZ_ID) {
        CRef<CPub> pub(new CPub);
        pub->SetPmid().Set(pmid);
        desc->SetPub().Set().push_back(pub);
    }

    CRef<CPub> pub_ref = journal(pp, p, p + StringLen(p), auth_list, title_art, false, cit_art, er);
    MemFree(p);

    TQualVector xrefs;
    for (const auto& xip : dbp.GetXmlData()) {
        if (xip.tag == INSDREFERENCE_XREF)
            XMLGetXrefs(dbp.mBuf.ptr, xip.subtags, xrefs);
    }

    string doi;
    string agricola;
    for (const auto& xref : xrefs) {
        if (! xref->IsSetQual())
            continue;

        if (NStr::EqualNocase(xref->GetQual(), "ARGICOLA") && agricola.empty())
            agricola = xref->GetVal();
        else if (NStr::EqualNocase(xref->GetQual(), "DOI") && doi.empty())
            doi = xref->GetVal();
    }

    fta_add_article_ids(*pub_ref, doi, agricola);

    if (pub_ref.Empty()) {
        desc.Reset();
        return desc;
    }

    if (dbp.mType == ParFlat_REF_NO_TARGET)
        desc->SetReftype(CPubdesc::eReftype_no_target);

    desc->SetPub().Set().push_back(pub_ref);

    return desc;
}

/**********************************************************/
static
CRef<CPubdesc> gb_refs_common(ParserPtr pp, DataBlk& dbp, Uint2 col_data, bool bParser, DataBlk*** ppInd, bool& no_auth)
{
    static DataBlk* ind[MAXKW + 1];

    bool      has_muid;
    char*     p;
    char*     q;
    char*     r;
    bool      is_online;
    TEntrezId pmid;
    Int4      er;

    CRef<CPubdesc> desc(new CPubdesc);

    p = dbp.mBuf.ptr + col_data;
    if (bParser) {
        /* This branch works when this function called in context of PARSER
         */
        if (*p >= '0' && *p <= '9')
            desc->SetPub().Set().push_back(get_num(p));
        else
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_Illegalreference, "No reference number.");
        ind_subdbp(dbp, ind, MAXKW, Parser::EFormat::GenBank);
    } else {
        /* This branch works when this function is called in context of GBDIFF
         */
        if (ppInd) {
            ind_subdbp(dbp, ind, MAXKW, Parser::EFormat::GenBank);
            *ppInd = &ind[0];

            return desc;
        }

        if (*p < '0' || *p > '9')
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_Illegalreference, "No reference number.");
    }

    has_muid = false;
    if (ind[ParFlat_MEDLINE]) {
        p              = ind[ParFlat_MEDLINE]->mBuf.ptr;
        CRef<CPub> pub = get_muid(p, Parser::EFormat::GenBank);
        if (pub.NotEmpty()) {
            has_muid = true;
            desc->SetPub().Set().push_back(get_num(p));
        }
    }

    pmid = ZERO_ENTREZ_ID;
    if (ind[ParFlat_PUBMED]) {
        p = ind[ParFlat_PUBMED]->mBuf.ptr;
        if (p)
            pmid = ENTREZ_ID_FROM(int, NStr::StringToInt(p, NStr::fAllowTrailingSymbols));
    }

    CRef<CAuth_list> auth_list;
    if (ind[ParFlat_AUTHORS]) {
        p = ind[ParFlat_AUTHORS]->mBuf.ptr;
        for (q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;

        if (*q != '\0') {
            if (ind[ParFlat_JOURNAL])
                q = ind[ParFlat_JOURNAL]->mBuf.ptr;

            get_auth(p, GB_REF, q, auth_list);
        }
    }

    if (ind[ParFlat_CONSRTM]) {
        p = ind[ParFlat_CONSRTM]->mBuf.ptr;
        for (q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;

        if (*q != '\0')
            get_auth_consortium(p, auth_list);
    }

    if (auth_list.Empty() || ! auth_list->IsSetNames())
        no_auth = true;

    CRef<CTitle::C_E> title_art;
    if (ind[ParFlat_TITLE]) {
        p = ind[ParFlat_TITLE]->mBuf.ptr;
        if (! fta_StartsWith(p, "Direct Submission"sv) &&
            *p != '\0' && *p != ';') {
            string title = clean_up(p);
            if (! title.empty()) {
                title_art.Reset(new CTitle::C_E);
                title_art->SetName(NStr::Sanitize(title));
            }
        }
    }

    if (! ind[ParFlat_JOURNAL]) {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "No JOURNAL line, reference dropped");

        desc.Reset();
        return desc;
    }

    p = ind[ParFlat_JOURNAL]->mBuf.ptr;
    if (*p == '\0' || *p == ';') {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "JOURNAL line is empty, reference dropped");

        desc.Reset();
        return desc;
    }

    is_online = NStr::StartsWith(p, "Online Publication"sv, NStr::eNocase);

    if (ind[ParFlat_REMARK]) {
        r = ind[ParFlat_REMARK]->mBuf.ptr;
        string comm = NStr::Sanitize(ExtractErratum(r));
        if (! is_online)
            normalize_comment(comm);
        desc->SetComment(comm);
    }

    er = desc->IsSetComment() ? fta_remark_is_er(desc->GetComment()) : 0;

    CRef<CCit_art> cit_art;
    if (pp->medserver == 1 && pmid > ZERO_ENTREZ_ID && (fta_StartsWith(p, "(er)"sv) || er > 0)) {
        cit_art = FetchPubPmId(pmid);
        if (! cit_art)
            pmid = ZERO_ENTREZ_ID;
    }

    if (pmid > ZERO_ENTREZ_ID) {
        CRef<CPub> pub(new CPub);
        pub->SetPmid().Set(pmid);
        desc->SetPub().Set().push_back(pub);
    }

    CRef<CPub> pub_ref = journal(pp, p, p + ind[ParFlat_JOURNAL]->mBuf.len, auth_list, title_art, has_muid, cit_art, er);

    if (pub_ref.Empty()) {
        desc.Reset();
        return desc;
    }

    if (dbp.mType == ParFlat_REF_NO_TARGET)
        desc->SetReftype(CPubdesc::eReftype_no_target);

    desc->SetPub().Set().push_back(pub_ref);

    return desc;
}

/**********************************************************
 *
 *   static PubdescPtr embl_refs(pp, dbp, col_data, no_auth):
 *
 *      Parse EMBL references. Return a Pubdesc pointer.
 *
 *                                              11-14-93
 *
 **********************************************************/
static CRef<CPubdesc> embl_refs(ParserPtr pp, DataBlk& dbp, Uint2 col_data, bool& no_auth)
{
    static DataBlk* ind[MAXKW + 1];
    
    char*     s;
    bool      has_muid;
    char*     p;
    char*     q;
    TEntrezId pmid;

    Int4 er;

    CRef<CPubdesc> desc(new CPubdesc);

    p = dbp.mBuf.ptr + col_data;
    while ((*p < '0' || *p > '9') && dbp.mBuf.len > 0)
        p++;
    if (*p >= '0' && *p <= '9')
        desc->SetPub().Set().push_back(get_num(p));
    else
        FtaErrPost(SEV_WARNING, ERR_REFERENCE_Illegalreference, "No reference number.");

    ind_subdbp(dbp, ind, MAXKW, Parser::EFormat::EMBL);

    has_muid = false;
    pmid     = ZERO_ENTREZ_ID;

    string doi;
    string agricola;

    if (ind[ParFlat_RC])
        desc->SetComment(NStr::Sanitize(ind[ParFlat_RC]->mBuf.ptr));

    er = desc->IsSetComment() ? fta_remark_is_er(desc->GetComment()) : 0;

    if (ind[ParFlat_RX]) {
        p              = ind[ParFlat_RX]->mBuf.ptr;
        CRef<CPub> pub = get_muid(p, Parser::EFormat::EMBL);

        char* id = get_embl_str_pub_id(p, "DOI;");
        if (id) {
            doi = id;
            MemFree(id);
        }

        id = get_embl_str_pub_id(p, "AGRICOLA;");
        if (id) {
            agricola = id;
            MemFree(id);
        }

        if (pub.NotEmpty()) {
            desc->SetPub().Set().push_back(pub);
            has_muid = true;
        }

        pmid = get_embl_pmid(p);
    }

    CRef<CAuth_list> auth_list;
    if (ind[ParFlat_RA]) {
        p = ind[ParFlat_RA]->mBuf.ptr;
        s = p + StringLen(p) - 1;
        if (*s == ';')
            *s = '\0';
        for (q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;
        if (*q != '\0') {
            if (ind[ParFlat_RL])
                q = ind[ParFlat_RL]->mBuf.ptr;

            get_auth(p, EMBL_REF, q, auth_list);
        }
    }

    if (ind[ParFlat_RG]) {
        p = ind[ParFlat_RG]->mBuf.ptr;
        s = p + StringLen(p) - 1;
        if (*s == ';')
            *s = '\0';

        for (q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;

        if (*q != '\0')
            get_auth_consortium(p, auth_list);
    }

    if (auth_list.Empty() || ! auth_list->IsSetNames())
        no_auth = true;

    CRef<CTitle::C_E> title_art;
    if (ind[ParFlat_RT]) {
        p = ind[ParFlat_RT]->mBuf.ptr;
        if (*p != '\0' && *p != ';') {
            string title = clean_up(p);
            if (! title.empty()) {
                title_art.Reset(new CTitle::C_E);
                title_art->SetName(NStr::Sanitize(title));
            }
        }
    }

    if (! ind[ParFlat_RL]) {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Illegalreference, "No JOURNAL line, reference dropped.");

        desc.Reset();
        return desc;
    }

    p = ind[ParFlat_RL]->mBuf.ptr;
    if (*p == '\0' || *p == ';') {
        FtaErrPost(SEV_ERROR, ERR_REFERENCE_Illegalreference, "JOURNAL line is empty, reference dropped.");

        desc.Reset();
        return desc;
    }

    CRef<CCit_art> cit_art;
    if (pp->medserver == 1 && pmid > ZERO_ENTREZ_ID && (fta_StartsWith(p, "(er)"sv) || er > 0)) {
        cit_art = FetchPubPmId(pmid);
        if (! cit_art)
            pmid = ZERO_ENTREZ_ID;
    }

    if (pmid > ZERO_ENTREZ_ID) {
        CRef<CPub> pub(new CPub);
        pub->SetPmid().Set(pmid);
        desc->SetPub().Set().push_back(pub);
    }

    CRef<CPub> pub_ref = journal(pp, p, p + ind[ParFlat_RL]->mBuf.len, auth_list, title_art, has_muid, cit_art, er);

    if (pub_ref.Empty()) {
        desc.Reset();
        return desc;
    }

    fta_add_article_ids(*pub_ref, doi, agricola);

    if (dbp.mType == ParFlat_REF_NO_TARGET)
        desc->SetReftype(CPubdesc::eReftype_no_target);

    desc->SetPub().Set().push_back(pub_ref);

    return desc;
}

/**********************************************************/
static void fta_sort_pubs(TPubList& pubs)
{
    for (TPubList::iterator pub = pubs.begin(); pub != pubs.end(); ++pub) {
        TPubList::iterator next_pub = pub;
        for (++next_pub; next_pub != pubs.end(); ++next_pub) {
            if ((*next_pub)->Which() > (*pub)->Which())
                continue;

            if ((*next_pub)->Which() == (*pub)->Which()) {
                if (! (*pub)->IsMuid() || (*pub)->GetMuid() >= (*next_pub)->GetMuid())
                    continue;
            }

            pub->Swap(*next_pub);
        }
    }
}

/**********************************************************/
static void fta_check_long_last_name(const CAuth_list& authors, bool soft_report)
{
    static const size_t MAX_LAST_NAME_LEN = 30;

    ErrSev sev;

    if (! authors.IsSetNames() || ! authors.GetNames().IsStd())
        return;

    for (const auto& author : authors.GetNames().GetStd()) {
        if (! author->IsSetName() || ! author->GetName().IsName())
            continue;

        const CName_std& name = author->GetName().GetName();

        if (name.IsSetLast() && name.GetLast().size() > MAX_LAST_NAME_LEN) {
            /* Downgrade severity of this error to WARNING
             * if in HTGS mode. As of 7/31/2002, very long
             * consortium names were treated as if
             * they were author last names, for HTGS data.
             * This can be reverted to ERROR after the
             * consortium name slot is available and utilized
             * in the ASN.1.
             */
            sev = (soft_report ? SEV_WARNING : SEV_ERROR);
            FtaErrPost(sev, ERR_REFERENCE_LongAuthorName, "Last name of author exceeds 30 characters in length. A format error in the reference data might have caused the author name to be parsed incorrectly. Name is \"{}\".", name.GetLast());
        }
    }
}

/**********************************************************/
static void fta_check_long_name_in_article(const CCit_art& cit_art, bool soft_report)
{
    if (cit_art.IsSetAuthors())
        fta_check_long_last_name(cit_art.GetAuthors(), soft_report);

    if (cit_art.IsSetFrom()) {
        const CCit_book* book = nullptr;
        if (cit_art.GetFrom().IsBook())
            book = &cit_art.GetFrom().GetBook();
        else if (cit_art.GetFrom().IsProc()) {
            if (cit_art.GetFrom().GetProc().IsSetBook())
                book = &cit_art.GetFrom().GetProc().GetBook();
        }

        if (book && book->IsSetAuthors())
            fta_check_long_last_name(book->GetAuthors(), soft_report);
    }
}

/**********************************************************/
static void fta_check_long_names(const CPub& pub, bool soft_report)
{
    if (pub.IsGen()) /* CitGen */
    {
        const CCit_gen& cit_gen = pub.GetGen();
        if (cit_gen.IsSetAuthors())
            fta_check_long_last_name(cit_gen.GetAuthors(), soft_report);
    } else if (pub.IsSub()) /* CitSub */
    {
        if (! soft_report) {
            const CCit_sub& cit_sub = pub.GetSub();
            if (cit_sub.IsSetAuthors())
                fta_check_long_last_name(cit_sub.GetAuthors(), soft_report);
        }
    } else if (pub.IsMedline()) /* Medline */
    {
        const CMedline_entry& medline = pub.GetMedline();
        if (medline.IsSetCit()) {
            fta_check_long_name_in_article(medline.GetCit(), soft_report);
        }
    } else if (pub.IsArticle()) /* CitArt */
    {
        fta_check_long_name_in_article(pub.GetArticle(), soft_report);
    } else if (pub.IsBook() || pub.IsProc() || pub.IsMan()) /* CitBook or CitProc or
                                                              CitLet */
    {
        const CCit_book* book = nullptr;

        if (pub.IsBook())
            book = &pub.GetBook();
        else if (pub.IsProc()) {
            if (pub.GetProc().IsSetBook())
                book = &pub.GetProc().GetBook();
        } else {
            if (pub.GetMan().IsSetCit())
                book = &pub.GetMan().GetCit();
        }

        if (book && book->IsSetAuthors())
            fta_check_long_last_name(book->GetAuthors(), soft_report);
    } else if (pub.IsPatent()) /* CitPat */
    {
        const CCit_pat& patent = pub.GetPatent();

        if (patent.IsSetAuthors())
            fta_check_long_last_name(patent.GetAuthors(), soft_report);

        if (patent.IsSetApplicants())
            fta_check_long_last_name(patent.GetApplicants(), soft_report);

        if (patent.IsSetAssignees())
            fta_check_long_last_name(patent.GetAssignees(), soft_report);
    } else if (pub.IsEquiv()) /* PubEquiv */
    {
        for (const auto& cur_pub : pub.GetEquiv().Get()) {
            fta_check_long_names(*cur_pub, soft_report);
        }
    }
}

/**********************************************************/
static void fta_propagate_pmid_muid(CPub_equiv& pub_equiv)
{
    TEntrezId pmid = ZERO_ENTREZ_ID;
    TEntrezId muid = ZERO_ENTREZ_ID;

    CCit_art* cit_art = nullptr;
    for (auto& pub : pub_equiv.Set()) {
        if (pub->IsMuid() && muid == ZERO_ENTREZ_ID)
            muid = pub->GetMuid();
        else if (pub->IsPmid() && pmid == ZERO_ENTREZ_ID)
            pmid = pub->GetPmid().Get();
        else if (pub->IsArticle() && ! cit_art)
            cit_art = &pub->SetArticle();
    }

    if (! cit_art || (muid == ZERO_ENTREZ_ID && pmid == ZERO_ENTREZ_ID))
        return;

    if (muid != ZERO_ENTREZ_ID) {
        CRef<CArticleId> id(new CArticleId);
        id->SetMedline().Set(muid);
        cit_art->SetIds().Set().push_front(id);
    }

    if (pmid != ZERO_ENTREZ_ID) {
        CRef<CArticleId> id(new CArticleId);
        id->SetPubmed().Set(pmid);
        cit_art->SetIds().Set().push_front(id);
    }
}

/**********************************************************
 *
 *   PubdescPtr DescrRefs(pp, dbp, col_data):
 *
 *      Return a Pubdesc pointer.
 *
 *                                              4-14-93
 *
 **********************************************************/
CRef<CPubdesc> DescrRefs(ParserPtr pp, DataBlk& dbp, Uint2 col_data)
{
    bool soft_report = false;

    bool rej     = false;
    bool no_auth = false;

    if (pp->mode == Parser::EMode::HTGS)
        soft_report = true;

    CRef<CPubdesc> desc;

    if (pp->format == Parser::EFormat::SPROT)
        desc = sp_refs(pp, dbp, col_data);
    else if (pp->format == Parser::EFormat::XML)
        desc = XMLRefs(pp, dbp, no_auth, rej);
    else if (pp->format == Parser::EFormat::GenBank)
        desc = gb_refs_common(pp, dbp, col_data, true, nullptr, no_auth);
    else if (pp->format == Parser::EFormat::EMBL)
        desc = embl_refs(pp, dbp, col_data, no_auth);

    if (desc && desc->IsSetComment()) {
        ShrinkSpaces(desc->SetComment());
    }

    if (no_auth) {
        if (pp->source == Parser::ESource::EMBL)
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_MissingAuthors, "Reference has no author names.");
        else {
            FtaErrPost(SEV_REJECT, ERR_REFERENCE_MissingAuthors, "Reference has no author names. Entry dropped.");
            pp->entrylist[pp->curindx]->drop = true;
        }
    }

    if (rej) {
        FtaErrPost(SEV_REJECT, ERR_REFERENCE_InvalidMuid, "Use of Medline ID in INSDSeq format is not alowed. Entry dropped.");
        pp->entrylist[pp->curindx]->drop = true;
    }

    if (desc.NotEmpty() && desc->IsSetPub()) {
        fta_sort_pubs(desc->SetPub().Set());

        for (const auto& pub : desc->GetPub().Get()) {
            fta_check_long_names(*pub, soft_report);
        }

        fta_propagate_pmid_muid(desc->SetPub());
    }

    return desc;
}

END_NCBI_SCOPE
