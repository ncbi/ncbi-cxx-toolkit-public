/* ref.c
 *
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
 * File Name:  ref.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 */
#include <ncbi_pch.hpp>

#include <objtools/flatfile/ftacpp.hpp>

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

#include <objtools/flatfile/index.h>
#include <objtools/flatfile/indx_blk.h>
#include <objtools/flatfile/utilfun.h>
#include <objtools/flatfile/genbank.h>
#include <objtools/flatfile/embl.h>
#include <objtools/flatfile/fta_xml.h>

#include <objtools/flatfile/ref.h>
#include <objtools/flatfile/utilref.h>
#include <objtools/flatfile/flatdefn.h>
#include <objtools/flatfile/asci_blk.h>
#include <objtools/flatfile/ftanet.h>

#include <objtools/flatfile/xutils.h>
#include <objtools/flatfile/xgbfeat.h>

#include "add.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "ref.cpp"

#define MAXKW 38

static const char *strip_sub_str[] = {
    "to the EMBL/GenBank/DDBJ databases",
    "to the EMBL/DDBJ/GenBank databases",
    "to the DDBJ/GenBank/EMBL databases",
    "to the DDBJ/EMBL/GenBank databases",
    "to the GenBank/DDBJ/EMBL databases",
    "to the GenBank/EMBL/DDBJ databases",
    "to the INSDC",
    NULL
};

static const char *ERRemarks[] = {
    "Publication Status: Online-Only",                          /*  1 */
    "Publication Status : Online-Only",                         /*  2 */
    "Publication_Status: Online-Only",                          /*  3 */
    "Publication_Status : Online-Only",                         /*  4 */
    "Publication-Status: Online-Only",                          /*  5 */
    "Publication-Status : Online-Only",                         /*  6 */
    "Publication Status: Available-Online",                     /*  7 */
    "Publication Status : Available-Online",                    /*  8 */
    "Publication_Status: Available-Online",                     /*  9 */
    "Publication_Status : Available-Online",                    /* 10 */
    "Publication-Status: Available-Online",                     /* 11 */
    "Publication-Status : Available-Online",                    /* 12 */
    "Publication Status: Available-Online prior to print",      /* 13 */
    "Publication Status : Available-Online prior to print",     /* 14 */
    "Publication_Status: Available-Online prior to print",      /* 15 */
    "Publication_Status : Available-Online prior to print",     /* 16 */
    "Publication-Status: Available-Online prior to print",      /* 17 */
    "Publication-Status : Available-Online prior to print",     /* 18 */
    NULL
};

/**********************************************************/
static void normalize_comment(std::string& comment)
{
    std::string new_comment = comment;
    char *q, *r;

    for(r = (char *) new_comment.c_str();;)
    {
        r = strstr(r, "; ");
        if(r == NULL)
            break;
        for(r += 2, q = r; *q == ' ' || *q == ';';)
            q++;
        if(q > r)
            fta_StringCpy(r, q);
    }

    comment = new_comment;
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
static ncbi::CRef<ncbi::objects::CDate> get_lanl_date(char* s)
{
    int            day = 0;
    int            month = 0;
    int            year;
    int            cal;

    const char     *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    ncbi::CRef<ncbi::objects::CDate> date(new ncbi::objects::CDate);
    for(cal = 0; cal < 12; cal++)
    {
        if(StringNICmp(s + 1, months[cal], 3) == 0)
        {
            month = cal + 1;
            break;
        }
    }
    day = atoi(s + 5);
    year = atoi(s + 8);
    if(year < 1900 || year > 1994)
    {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_IllegalDate,
                  "Illegal year: %d", year);
    }

    date->SetStd().SetYear(year);
    date->SetStd().SetMonth(month);
    date->SetStd().SetDay(day);

    if (XDateCheck(date->GetStd()) != 0)
    {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_IllegalDate,
                  "Illegal date: %s", s);
        date.Reset();
    }

    return(date);
}

/**********************************************************
 *
 *   static char* clean_up(str):
 *
 *      Deletes front and tail double or single quotes
 *   if any.
 *
 **********************************************************/
static char* clean_up(char* str)
{
    char* newp;
    char* s;

    if(str == NULL)
        return(NULL);

    s = str + StringLen(str) - 1;
    if(*s == ';')
        *s = '\0';

    while(*str == '\"' || *str == '\'')
        str++;

    newp = strdup(str);
    size_t size = StringLen(newp);
    while(size > 0 && (newp[size-1] == '\"' || newp[size-1] == '\''))
    {
        size--;
        newp[size] = '\0';
    }

    return(newp);
}

/**********************************************************
*
*   static ValNodePtr get_num(str):
*
*      Get gb serial number and put it to PUB_Gen.
*
*                                              12-4-93
*
**********************************************************/
static ncbi::CRef<ncbi::objects::CPub> get_num(char* str)
{
    int serial_num = ncbi::NStr::StringToInt(str, ncbi::NStr::fAllowTrailingSymbols);

    ncbi::CRef<ncbi::objects::CPub> ret(new ncbi::objects::CPub);
    ret->SetGen().SetSerial_number(serial_num);

    return ret;
}

/**********************************************************
 *
 *   static ValNodePtr get_muid(str, format):
 *
 *      Get gb MUID and put it to PUB_Gen.
 *
 *                                              12-4-93
 *
 **********************************************************/
static ncbi::CRef<ncbi::objects::CPub> get_muid(char* str, Uint1 format)
{
    char* p;
    Int4    i;

    ncbi::CRef<ncbi::objects::CPub> muid;

    if(str == NULL)
        return muid;

    if(format == ParFlat_GENBANK || format == ParFlat_XML)
        p = str;
    else if(format == ParFlat_EMBL)
    {
        p = StringIStr(str, "MEDLINE;");
        if(p == NULL)
            return muid;
        for(p += 8; *p == ' ';)
            p++;
    }
    else
        return muid;

    i = ncbi::NStr::StringToInt(p, ncbi::NStr::fAllowTrailingSymbols);
    if(i < 1)
        return muid;

    muid.Reset(new ncbi::objects::CPub);
    muid->SetMuid(ENTREZ_ID_FROM(int, i));
    return muid;
}

/**********************************************************/
static char* get_embl_str_pub_id(char* str, const Char *tag)
{
    char* p;
    char* q;
    char* ret;
    Char    ch;

    if(str == NULL || tag == NULL)
        return(NULL);

    p = StringIStr(str, tag);
    if(p == NULL)
        return(NULL);
    for(p += StringLen(tag); *p == ' ';)
        p++;

    ret = NULL;
    for(q = p; *q != ' ' && *q != '\0';)
        q++;
    q--;
    if(*q != '.')
        q++;
    ch = *q;
    *q = '\0';
    ret = StringSave(p);
    *q = ch;
    return(ret);
}

/**********************************************************/
static Int4 get_embl_pmid(char* str)
{
    char* p;
    Int4    i;

    if(str == NULL)
        return(0);

    p = StringIStr(str, "PUBMED;");
    if(p == NULL)
        return(0);
    for(p += 7; *p == ' ';)
        p++;
    i = (Int4) atol(p);
    if(i < 1)
        return(0);
    return(i);
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
    if(p == NULL)
        return(NULL);

    if(p[3] == '.')
        q = p + 4;
    else if(StringNCmp(p + 3, "ume", 3) == 0)
        q = p + 6;
    else
        return(NULL);

    while(*q == ' ' || *q == '\t')
        q++;
    for(r = q; *r >= '0' && *r <= '9';)
        r++;

    if(r == q || *r != '\0')
        return(NULL);

    if(p > title)
    {
        p--;
        if(*p != ' ' && *p != '\t' && *p != ',' && *p != ';' && *p != '.')
            return(NULL);

        while(*p == ' ' || *p == '\t' || *p == ',' || *p == ';' || *p == '.')
        {
            if(p == title)
                break;
            p--;
        }
        if(*p != ' ' && *p != '\t' && *p != ',' && *p != ';' && *p != '.')
            p++;
    }
    *p = '\0';

    return(q);
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
static ncbi::CRef<ncbi::objects::CCit_pat> get_pat(ParserPtr pp, char* bptr, ncbi::CRef<ncbi::objects::CAuth_list>& auth_list, ncbi::CRef<ncbi::objects::CTitle::C_E>& title, char* eptr)
{
    IndexblkPtr ibp;

    ncbi::CRef<ncbi::objects::CCit_pat> cit_pat;

    char*     country;
    char*     number;
    char*     type;
    char*     app;
    char*     s;
    char*     p;
    char*     q;
    char*     temp;

    ErrSev      sev;
    Char        ch;

    ibp = pp->entrylist[pp->curindx];

    temp = StringSave(bptr);

    ch = (pp->format == ParFlat_EMBL) ? '.' : ';';
    p = StringChr(temp, ch);
    if(p != NULL)
        *p = '\0';

    p = StringChr(bptr, ch);
    if(p != NULL)
        *p = '\0';

    if(ibp->is_pat && ibp->psip.NotEmpty())
    {
        ErrPostStr(SEV_ERROR, ERR_FORMAT_MultiplePatRefs,
                   "Too many patent references for patent sequence; ignoring all but the first.");
    }

    q = (pp->format == ParFlat_EMBL) ? (char*) "Patent number" :
                                       (char*) "Patent:";
    size_t len = StringLen(q);
    if(StringNICmp(q, bptr, len) != 0)
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Illegal format: \"%s\"", temp);
        MemFree(temp);
        return cit_pat;
    }

    for(s = bptr + len; *s == ' ';)
        s++;
    for(country = s, q = s; isalpha((int) *s) != 0 || *s == ' '; s++)
        if(*s != ' ')
            q = s;
    if(country == q)
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "No Patent Document Country: \"%s\"", temp);
        MemFree(temp);
        return cit_pat;
    }
    s = q + 1;

    if(pp->format != ParFlat_EMBL)
        *s++ = '\0';
    while(*s == ' ')
        s++;
    for(number = s, q = s; isdigit((int) *s) != 0 || *s == ','; s++)
        if(*s != ',')
            *q++ = *s;

    if(number == s)
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "No Patent Document Number: \"%s\"", temp);
        MemFree(temp);
        return cit_pat;
    }

    if(q != s)
        *q = '\0';

    if(*s == '-')
    {
        *s++ = '\0';
        for(type = s; *s != ' ' && *s != '/' && *s != '\0';)
            s++;
        if(type == s)
            type = NULL;
    }
    else
        type = NULL;
    if(*s != '\0')
        *s++ = '\0';

    if(type == NULL)
    {
        sev = (ibp->is_pat ? SEV_ERROR : SEV_WARNING);
        ErrPostEx(sev, ERR_REFERENCE_Fail_to_parse,
                  "No Patent Document Type: \"%s\"", temp);
    }

    for(app = s, q = s; *s >= '0' && *s <= '9';)
        s++;
    if(*s != '\0' && *s != ',' && *s != '.' && *s != ' ' && *s != ';' &&
       *s != '\n')
    {
        sev = (ibp->is_pat ? SEV_ERROR : SEV_WARNING);
        ErrPostEx(sev, ERR_REFERENCE_Fail_to_parse,
                  "No number of sequence in patent: \"%s\"", temp);
        app = NULL;
        s = q;
    }
    else if(*s != '\0')
        for(*s++ = '\0'; *s == ' ';)
            s++;

    ncbi::CRef<ncbi::objects::CDate_std> std_date;
    if(*s != '\0')
    {
        std_date = get_full_date(s, true, pp->source);
    }

    if (std_date.Empty())
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Illegal format: \"%s\"", temp);
        MemFree(temp);
        return cit_pat;
    }

    if(p != NULL)
        *p = ch;

    std::string msg = ncbi::NStr::Sanitize(number);
    if(pp->format == ParFlat_EMBL)
        *number = '\0';

    cit_pat.Reset(new ncbi::objects::CCit_pat);

    cit_pat->SetCountry(country);
    cit_pat->SetNumber(msg);

    cit_pat->SetDoc_type(type == NULL ? "" : type);
    cit_pat->SetDate_issue().SetStd(*std_date);
    cit_pat->SetTitle(title.Empty() ? "" : title->GetName());

    if (auth_list.Empty() || !auth_list->IsSetNames())
    {
        ncbi::objects::CAuth_list& pat_auth_list = cit_pat->SetAuthors();
        pat_auth_list.SetNames().SetStr().push_back("");
    }
    else
        cit_pat->SetAuthors(*auth_list);

    if (auth_list.NotEmpty())
    {
        ncbi::objects::CAffil& affil = auth_list->SetAffil();

        s += 13;
        if (s < eptr && *s != '\0')
            affil.SetStr(s);
        else
            affil.SetStr("");
    }

    if(ibp->is_pat && ibp->psip.Empty())
    {
        ibp->psip = new ncbi::objects::CPatent_seq_id;
        ibp->psip->SetCit().SetCountry(country);
        ibp->psip->SetCit().SetId().SetNumber(msg);
        ibp->psip->SetSeqid(app != NULL ? atoi(app) : 0);
    }

    MemFree(temp);
    return cit_pat;
}

/**********************************************************/
static void fta_get_part_sup(char* parts, ncbi::objects::CImprint& imp)
{
    char* start;
    char* end;
    char* p;
    char* q;
    Char    ch;
    Int4    i;
    Int4    j;

    if(parts == NULL || *parts == '\0')
        return;

    for(p = parts, i = 0, j = 0; *p != '\0'; p++)
    {
        if(*p == '(')
            i++;
        else if(*p == ')')
            j++;

        if(j > i || i - j > 1)
            break;
    }

    if(*p != '\0' || i < 2)
        return;

    start = StringChr(parts, '(');
    end = StringChr(start + 1, ')');

    for(p = start + 1; *p == ' ';)
        p++;
    if(p == end)
        return;

    for(q = end - 1; *q == ' ' && q > p;)
        q--;
    if(*q != ' ')
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
static bool get_parts(char* bptr, char* eptr, ncbi::objects::CImprint& imp)
{
    char* parts;
    char* p;
    char* q;
    Char    ch;
    Int4    bad;

    if(bptr == NULL || eptr == NULL)
        return false;

    ch = *eptr;
    *eptr = '\0';
    parts = StringSave(bptr);
    *eptr = ch;

    for(p = parts; *p != '\0'; p++)
        if(*p == '\t')
            *p = ' ';

    fta_get_part_sup(parts, imp);

    bad = 0;
    q = StringChr(parts, '(');
    p = StringChr(parts, ')');

    if(p != NULL && q != NULL)
    {
        if(p < q || StringChr(p + 1, ')') != NULL ||
           StringChr(q + 1, '(') != NULL)
            bad = 1;
    }
    else if(p != NULL || q != NULL)
        bad = 1;

    if(bad != 0)
    {
        MemFree(parts);
        return false;
    }

    if(q != NULL)
    {
        *q++ = '\0';
        *p = '\0';

        for(p = q; *p == ' ';)
            p++;
        for(q = p; *q != '\0' && *q != ' ';)
            q++;
        if(*q != '\0')
            *q++ = '\0';
        if(q > p)
            imp.SetIssue(p);
        for(p = q; *p == ' ';)
            p++;
        for(q = p; *q != '\0';)
            q++;
        if(q > p)
        {
            for(q--; *q == ' ';)
                q--;
            *++q = '\0';

            std::string supi(" ");
            supi += p;
            imp.SetPart_supi(supi);
        }

        const Char* issue_str = imp.IsSetIssue() ? imp.GetIssue().c_str() : NULL;
        if (imp.IsSetPart_supi() && issue_str != NULL &&
            (issue_str[0] == 'P' || issue_str[0] == 'p') && (issue_str[1] == 'T' || issue_str[1] == 't') &&
            issue_str[2] == '\0')
        {
            std::string& issue = imp.SetIssue();
            issue += imp.GetPart_supi();
            imp.ResetPart_supi();
        }
    }

    for(p = parts; *p == ' ';)
        p++;
    for(q = p; *q != '\0' && *q != ' ';)
        q++;
    if(*q != '\0')
        *q++ = '\0';
    if(q > p)
        imp.SetVolume(p);
    for(p = q; *p == ' ';)
        p++;
    for(q = p; *q != '\0';)
        q++;
    if(q > p)
    {
        for(q--; *q == ' ';)
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
static ncbi::CRef<ncbi::objects::CCit_art> get_art(ParserPtr pp, char* bptr, ncbi::CRef<ncbi::objects::CAuth_list>& auth_list, ncbi::CRef<ncbi::objects::CTitle::C_E>& title,
                         int pre, bool has_muid, bool* all_zeros, Int4 er)
{
    char*      eptr;
    char*      end_tit;
    char*      s;
    char*      p;
    char*      ss;
    char*      end_volume;
    char*      end_pages;
    char*      buf;
    char*      tit = NULL;
    char*      volume = NULL;
    char*      pages = NULL;
    char*      year;
    Char         symbol;

    Int4         i;
    Int4         is_er;

    *all_zeros = false;

    is_er = 0;
    if(er > 0)
        is_er |= 01;                    /* based on REMARKs */
    if(StringNCmp(bptr, "(er)", 4) == 0)
        is_er |= 02;

    ncbi::CRef<ncbi::objects::CCit_art> cit_art;

    if(pp->format == ParFlat_GENBANK || pp->format == ParFlat_PRF)
        symbol = ',';
    else if(pp->format == ParFlat_EMBL)
        symbol = ':';
    else if(pp->format == ParFlat_XML)
    {
        if(pp->source == ParFlat_EMBL)
            symbol = ':';
        else
            symbol = ',';
    }
    else
        return cit_art;

    end_volume = NULL;

    size_t len = StringLen(bptr);
    buf = (char*) MemNew(len + 1);
    StringCpy(buf, bptr);
    eptr = buf + len - 1;
    while(eptr > buf && (*eptr == ' ' || *eptr == '\t' || *eptr == '.'))
        *eptr-- = '\0';
    if(*eptr != ')')
    {
        MemFree(buf);
        return cit_art;
    }
    for(s = eptr - 1; s > buf && *s != '(';)
        s--;
    if(*s != '(')
    {
        MemFree(buf);
        return cit_art;
    }

    if(pp->format == ParFlat_PRF && s > buf &&
       (StringLen(s) != 6 || s[1] < '1' || s[1] > '2' || s[2] < '0' ||
        s[2] > '9' || s[3] < '0' || s[3] > '9' || s[4] < '0' || s[4] > '9'))
    {
        for(p = s - 1; p > buf && *p != '(';)
            p--;
        if(*p == '(' && p[5] == ')' && p[1] > '0' && p[1] < '3' &&
           p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9' &&
           p[4] >= '0' && p[4] <= '9')
        {
            *s = '\0';
            s = p;
        }
    }

    year = s + 1;
    for(s--; s >= buf && isspace((int) *s) != 0;)
        s--;
    if(s < buf)
        s = buf;
    end_pages = s + 1;
    if(buf[0] == 'G' && buf[1] == '3')
        ss = buf + 2;
    else
        ss = buf;
    for(i = 0; ss <= year; ss++)
    {
        if(*ss == '(')
            i++;
        else if(*ss == ')')
            i--;
        else if(*ss >= '0' && *ss <= '9' && i == 0)
            break;
    }

    for(s = end_pages; s >= buf && *s != symbol;)
        s--;
    if(s < buf)
        s = buf;
    if(*s != symbol)
    {
        /* try delimiter from other format
         */
        if(pp->format == ParFlat_GENBANK)
            symbol = ':';
        else if(pp->format == ParFlat_EMBL)
            symbol = ',';
        else if(pp->format == ParFlat_XML)
        {
            if(pp->source == ParFlat_EMBL)
                symbol = ',';
            else
                symbol = ':';
        }

        for(s = end_pages; s >= buf && *s != symbol;)
            s--;
        if(s < buf)
            s = buf;
    }

    if(*s == symbol && ss != year)
    {
        if(ss > s)
            ss = s + 1;
        end_volume = s;
        for(pages = s + 1; IS_WHITESP(*pages) != 0;)
            pages++;
        end_tit = ss - 1;
        if(end_volume > ss)
        {
            volume = ss;
            if(*end_tit == '(')
                volume--;
        }
    }
    else
    {
        if(pre != 1)
            pre = 2;

        end_tit = end_pages;
    }

    if(*year == '0')
    {
        if(pages != NULL && StringNCmp(pages, "0-0", 3) == 0 &&
           pp->source == ParFlat_EMBL)
            *all_zeros = true;
        MemFree(buf);
        return cit_art;
    }

    tit = buf;
    if(*tit == '\0')
    {
        ErrPostStr(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                   "No journal title.");
        MemFree(buf);
        return cit_art;
    }

    cit_art.Reset(new ncbi::objects::CCit_art);
    ncbi::objects::CCit_jour& journal = cit_art->SetFrom().SetJournal();
    ncbi::objects::CImprint& imp = journal.SetImp();

    if (pre > 0)
        imp.SetPrepub(static_cast<ncbi::objects::CImprint::EPrepub>(pre));

    *end_pages = '\0';
    if(pages != NULL && StringNCmp(pages, "0-0", 3) != 0)
    {
        i = valid_pages_range(pages, tit, is_er, (pre == 2));
        if(i == 0)
            imp.SetPages(pages);
        else if(i == 1)
            end_tit = end_pages;
        else if(i == -1 && is_er > 0)
        {
            MemFree(buf);
            cit_art.Reset();
            return cit_art;
        }
    }
    else if(pre != 1)
        pre = 2;

    if(volume != NULL)
    {
        if(!get_parts(volume, end_volume, imp))
        {
            MemFree(buf);
            cit_art.Reset();
            return cit_art;
        }

        if(pre != 1 && !imp.IsSetVolume())
        {
            if(imp.IsSetPages())
            {
                MemFree(buf);
                cit_art.Reset();
                return cit_art;
            }
            pre = 2;
        }
    }
    else if(is_er > 0 && pre != 2)
    {
        MemFree(buf);
        cit_art.Reset();
        return cit_art;
    }

    ncbi::CRef<ncbi::objects::CDate> date;
    if (*year != '0')
        date = get_date(year);

    if(date.Empty())
    {
        if(is_er == 0)
            ErrPostStr(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                       "No date in journal reference");

        MemFree(buf);
        cit_art.Reset();
        return cit_art;
    }

    *end_tit = '\0';

    ncbi::CRef<ncbi::objects::CTitle::C_E> journal_title(new ncbi::objects::CTitle::C_E);

    for (char* aux = end_tit - 1; aux > tit && *aux != '.' && *aux != ')' && !isalnum(*aux); --aux)
        *aux = 0;

    journal_title->SetIso_jta(ncbi::NStr::Sanitize(tit));
    journal.SetTitle().Set().push_back(journal_title);

    imp.SetDate(*date);
    if (pre > 0)
        imp.SetPrepub(static_cast<ncbi::objects::CImprint::EPrepub>(pre));

    if((is_er & 01) == 01)
    {
        if(er == 1)
            imp.SetPubstatus(3);         /* epublish     */
        else
            imp.SetPubstatus(10);        /* aheadofprint */
    }

    /* check invalid "in-press"
     */
    if(pre == 2)
    {
        if(has_muid)
        {
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_InvalidInPress,
                      "Reference flagged as In-press, but Medline UID exists, In-press ignored: %s",
                      buf);
            imp.ResetPrepub();
        }

        if(imp.IsSetPages() && imp.IsSetVolume() && imp.IsSetDate())
        {
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_InvalidInPress,
                      "Reference flagged as In-press, but citation is complete, In-press ignored: %s",
                      buf);
            imp.ResetPrepub();
        }
    }

    /* Title and authors are optional for cit_art
     */
    if(title != NULL)
        cit_art->SetTitle().Set().push_back(title);

    if (auth_list.NotEmpty())
        cit_art->SetAuthors(*auth_list);

    MemFree(buf);
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
static ncbi::CRef<ncbi::objects::CCit_gen> get_unpub(char* bptr, char* eptr, ncbi::CRef<ncbi::objects::CAuth_list>& auth_list,
                                                     const Char* title)
{
    ncbi::CRef<ncbi::objects::CCit_gen> cit_gen(new ncbi::objects::CCit_gen);

    char*   s;
    char*   str;

    if (bptr != NULL)
    {
        for(s = bptr; *s != '\0' && *s != '(';)
            s++;
        for(str = s - 1; str > bptr && IS_WHITESP(*str) != 0;)
            str--;
        if(*s == '(')
            s += 6;

        if (s < eptr && *s != '\0' && auth_list.NotEmpty())
            auth_list->SetAffil().SetStr(ncbi::NStr::Sanitize(s));

        cit_gen->SetCit(std::string(bptr, str + 1));
    }

    if (auth_list.NotEmpty())
        cit_gen->SetAuthors(*auth_list);

    if (title != NULL)
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
static ncbi::CRef<ncbi::objects::CCit_art> get_book(char* bptr, ncbi::CRef<ncbi::objects::CAuth_list>& auth_list, ncbi::CRef<ncbi::objects::CTitle::C_E>& title,
                                                    int pre, Int2 format, char* jour)
{
    char*    s;
    char*    ss;
    char*    tit;
    char*    volume;
    char*    pages;
    char*    press;

    Uint1      ref_fmt;
    bool       IS_AUTH = false;
    char*    tbptr;
    char*    p;
    Char       c;
    Int4       i;

    tit = NULL;
    ref_fmt = 0;

    tbptr = (bptr == NULL) ? NULL : StringSave(bptr);

    switch(format)
    {
        case ParFlat_EMBL:
            ref_fmt = EMBL_REF;
            break;
        case ParFlat_GENBANK:
            ref_fmt = GB_REF;
            break;
        case ParFlat_PIR:
            ref_fmt = PIR_REF;
            break;
        case ParFlat_SPROT:
            ref_fmt = SP_REF;
            break;
    }

    ncbi::CRef<ncbi::objects::CCit_art> cit_art(new ncbi::objects::CCit_art);
    ncbi::objects::CCit_book& cit_book = cit_art->SetFrom().SetBook();

    if (pre > 0)
        cit_book.SetImp().SetPrepub(static_cast<ncbi::objects::CImprint::EPrepub>(pre));

    p = tbptr;
    ncbi::CRef<ncbi::objects::CTitle::C_E> book_title(new ncbi::objects::CTitle::C_E);

    if(StringNCmp("(in)", tbptr, 4) == 0)
    {
        for(s = tbptr + 4; *s == ' ';)
            s++;
        for(bptr = s; *s != ';' && *s != '(' && *s != '\0';)
            s++;
        if(StringNICmp(s, "(Eds.)", 6) == 0)
        {
            tit = s + 6;
            IS_AUTH = true;
        }
        else if(StringNICmp(s, "(Ed.)", 5) == 0)
        {
            tit = s + 5;
            IS_AUTH = true;
        }
        else if(*s == ';')
            tit = s;
        if(tit != NULL)
            while(*tit == ' ' || *tit == ';' || *tit == '\n')
                tit++;
        c = *s;
        *s++ = '\0';
        if(IS_AUTH && *bptr != '\0')
        {
            ncbi::CRef<ncbi::objects::CAuth_list> book_auth_list;
            get_auth(bptr, ref_fmt, jour, book_auth_list);
            if (book_auth_list.NotEmpty())
                cit_book.SetAuthors(*book_auth_list);
        }
        else
        {
            ErrPostEx(SEV_ERROR, ERR_REFERENCE_UnusualBookFormat,
                      "Cannot parse unusually formatted book reference (generating Cit-gen instead): %s",
                      p);
            if(tbptr != NULL)
                MemFree(tbptr);

            cit_art.Reset();
            return cit_art;
        }

        ss = StringRChr(tit, ';');
        if(ss == NULL)
            for(ss = tit; *ss != '\0';)
                ss++;
        for(s = ss; *s != ':' && s != tit;)
            s--;
        if(*s != ':')
            s = ss;
        c = *s;
        if(*s != '\0')
            *s++ = '\0';

        book_title->SetName("");
        if(*tit != '\0')
        {
            volume = check_book_tit(tit);
            if(volume != NULL)
                cit_book.SetImp().SetVolume(volume);

            book_title->SetName(ncbi::NStr::Sanitize(tit));
        }

        if(c == ':')
        {
            for(pages = s; *s != '\0' && *s != ',' && *s != ';';)
                s++;
            if(*s != '\0')
                *s++ = '\0';

            while(*pages == ' ')
                pages++;

            if (StringNCmp(pages, "0-0",  3) == 0)
                cit_book.SetImp().SetPrepub(ncbi::objects::CImprint::ePrepub_in_press);
            else
            {
                bool is_in_press = cit_book.GetImp().IsSetPrepub() && cit_book.GetImp().GetPrepub() == ncbi::objects::CImprint::ePrepub_in_press;
                i = valid_pages_range(pages, book_title->GetName().c_str(), 0, is_in_press);

                if(i == 0)
                    cit_book.SetImp().SetPages(ncbi::NStr::Sanitize(pages));
                else if(i == 1)
                {
                    std::string new_title = book_title->GetName();
                    new_title += ": ";
                    new_title += pages;
                    book_title->SetName(new_title);
                }
            }
        }

        for(press = s; *s != '(' && *s != '\0';)
            s++;
        if(*s != '\0')
            *s++ = '\0';

        cit_book.SetImp().SetPub().SetStr(ncbi::NStr::Sanitize(press));

        ncbi::CRef<ncbi::objects::CDate> date = get_date(s);
        if (date.Empty())
        {
            ErrPostStr(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                       "No date in book reference");
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_Illegalreference,
                      "Book format error (cit-gen created): %s", p);
            if(tbptr != NULL)
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

    if(tbptr != NULL)
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
static ncbi::CRef<ncbi::objects::CCit_let> get_thesis(char* bptr, ncbi::CRef<ncbi::objects::CAuth_list>& auth_list,
                                                      ncbi::CRef<ncbi::objects::CTitle::C_E>& title, int pre)
{
    ncbi::CRef<ncbi::objects::CCit_let> cit_let(new ncbi::objects::CCit_let);

    cit_let->SetType(ncbi::objects::CCit_let::eType_thesis);

    ncbi::objects::CCit_book& book = cit_let->SetCit();

    if (pre > 0)
        book.SetImp().SetPrepub(static_cast<ncbi::objects::CImprint::EPrepub>(pre));

    char* s;
    for (s = bptr; *s != '\0' && *s != '(';)
        s++;

    if(*s == '(')
    {
        ncbi::CRef<ncbi::objects::CDate> date = get_date(s + 1);
        if (date.NotEmpty())
            book.SetImp().SetDate(*date);

        s = s + 6;
    }

    if (!book.GetImp().IsSetDate())
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Fail to parse thesis: missing date");

        cit_let.Reset();
        return cit_let;
    }

    if(*s != '\0')
        book.SetImp().SetPub().SetStr(ncbi::NStr::Sanitize(s));

    if (title.NotEmpty())
        book.SetTitle().Set().push_back(title);
    else
    {
        ErrPostStr(SEV_WARNING, ERR_REFERENCE_Thesis, "Missing thesis title");

        ncbi::CRef<ncbi::objects::CTitle::C_E> empty_title(new ncbi::objects::CTitle::C_E);
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
static ncbi::CRef<ncbi::objects::CCit_book> get_whole_book(char* bptr, ncbi::CRef<ncbi::objects::CAuth_list>& auth_list,
                                 ncbi::CRef<ncbi::objects::CTitle::C_E>& title, int pre)
{
    ncbi::CRef<ncbi::objects::CCit_book> cit_book;

    char*    s;

    for(bptr += 5; IS_WHITESP(*bptr) != 0;)
        bptr++;


    for(s = bptr; *s != '\0' && *s != '(';)
        s++;

    if(*s != '(')
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Fail to parse book: missing date");
        return cit_book;
    }

    cit_book.Reset(new ncbi::objects::CCit_book);

    if (pre > 0)
        cit_book->SetImp().SetPrepub(static_cast<ncbi::objects::CImprint::EPrepub>(pre));

    ncbi::CRef<ncbi::objects::CDate> date = get_date(s + 1);
    if (date.NotEmpty())
        cit_book->SetImp().SetDate(*date);

    *s = '\0';
    for(s = bptr; *s != '\0' && *s != '.';)
        s++;

    ncbi::CRef<ncbi::objects::CTitle::C_E> book_title(new ncbi::objects::CTitle::C_E);
    book_title->SetName(std::string(bptr, s));
    cit_book->SetTitle().Set().push_back(book_title);

    if(*s == '.')
    {
        for(s++; IS_WHITESP(*s) != 0;)
            s++;

        cit_book->SetImp().SetPub().SetStr(ncbi::NStr::Sanitize(s));
    }

    if (auth_list.Empty() || !auth_list->IsSetNames())
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Fail to parse thesis: missing thesis author");
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
static ncbi::CRef<ncbi::objects::CCit_sub> get_sub(ParserPtr pp, char* bptr, ncbi::CRef<ncbi::objects::CAuth_list>& auth_list)
{
    const char  **b;
    char*     s;
    Int2        medium = OTHER_MEDIUM;

    ncbi::CRef<ncbi::objects::CCit_sub> ret;

    for(s = bptr; *s != '(' &&  *s != '\0';)
        s++;
    if(*s == '\0')
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Fail to parse submission: missing date");
        return ret;
    }

    ret.Reset(new ncbi::objects::CCit_sub);
    ncbi::CRef<ncbi::objects::CDate> date;

    if(pp != NULL && pp->entrylist != NULL &&
       IsNewAccessFormat(pp->entrylist[pp->curindx]->acnum) == 0 &&
       (StringChr(ParFlat_LANL_AC,
                  pp->entrylist[pp->curindx]->acnum[0]) != NULL) &&
       isdigit((int) *(s + 1)) == 0)
    {
        date = get_lanl_date(s);
    }
    else
    {
        ncbi::CRef<ncbi::objects::CDate_std> std_date = get_full_date(s + 1, true, pp->source);
        date.Reset(new ncbi::objects::CDate);
        date->SetStd(*std_date);
    }

    if (date.Empty())
        return ret;

    ret.Reset(new ncbi::objects::CCit_sub);
    ret->SetDate(*date);

    s = s + 13;
    if(StringStr(s, "E-mail") != NULL)
        medium = EMAIL_MEDIUM;

    if(StringNICmp(" on tape", s, 8) == 0)
    {
        medium = TAPE_MEDIUM;
        for(s += 8; *s != '\0' && *s != ':';)
            s++;
    }
    if(*s != '\0' && *(s + 1) != '\0')
    {
        while(*s == ' ')
            s++;

        if(*s == ':')
            s++;
        for(;;)
        {
            for(b = strip_sub_str; *b != NULL; b++)
            {
                size_t l_str = StringLen(*b);
                if(StringNCmp(s, *b, l_str) == 0)
                {
                    for(s += l_str; *s == ' ' || *s == '.';)
                        s++;
                    break;
                }
            }
            if(*b == NULL)
                break;
        }

        if (*s != '\0' && auth_list.NotEmpty())
        {
            auth_list->SetAffil().SetStr(ncbi::NStr::Sanitize(s));
        }
    }

    if(*s == '\0')
    {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_NoContactInfo,
                  "Missing contact info : %s", bptr);
    }

    if (auth_list.Empty() || !auth_list->IsSetNames())
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Direct submission: missing author (cit-gen created)");

        ret.Reset();
        return ret;
    }

    ret->SetAuthors(*auth_list);
    ret->SetMedium(static_cast<ncbi::objects::CCit_sub::EMedium>(medium));

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
static ncbi::CRef<ncbi::objects::CCit_sub> get_sub_gsdb(char* bptr, ncbi::CRef<ncbi::objects::CAuth_list>& auth_list,
                              ncbi::CRef<ncbi::objects::CTitle::C_E>& title, ParserPtr pp)
{
    ncbi::CRef<ncbi::objects::CCit_sub> cit_sub;

    char*   s;

    for(s = bptr; *s != '(' && *s != '\0';)
        s++;
    if(*s == '\0')
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Fail to parse submission: missing date");
        return cit_sub;
    }

    ncbi::CRef<ncbi::objects::CDate_std> std_date = get_full_date(s + 1, true, pp->source);
    if(std_date.Empty())
        return cit_sub;

    ncbi::CRef<ncbi::objects::CDate> date;
    date->SetStd(*std_date);

    if (auth_list.Empty() || !auth_list->IsSetNames())
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Direct submission: missing author (cit-gen created)");
        return cit_sub;
    }

    cit_sub.Reset(new ncbi::objects::CCit_sub);
    cit_sub->SetAuthors(*auth_list);
    cit_sub->SetDate(*date);

    if (title.NotEmpty())
    {
        const Char* s = title->GetName().c_str();
        size_t l_str = StringLen("Published by");
        if(StringNCmp(s, "Published by", l_str) == 0)
        {
            s += l_str;
            while(*s == ' ')
                s++;
        }

        if(*s != '\0')
        {
            auth_list->SetAffil().SetStr(ncbi::NStr::Sanitize(s));
        }
        else
        {
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_NoContactInfo,
                      "Missing contact info : %s", bptr);
        }
    }
    else
    {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_NoContactInfo,
                  "Missing contact info : %s", bptr);
    }

    return cit_sub;
}

/**********************************************************/
static ncbi::CRef<ncbi::objects::CCit_gen> fta_get_citgen(char* bptr, ncbi::CRef<ncbi::objects::CAuth_list>& auth_list,
                                                          ncbi::CRef<ncbi::objects::CTitle::C_E>& title)
{
    ncbi::CRef<ncbi::objects::CCit_gen> cit_gen;

    char*   p;
    char*   q;
    char*   r;
    Char      ch;
    Int2      year;

    if (bptr == NULL || auth_list.Empty() || !auth_list->IsSetNames() || title.Empty())
        return cit_gen;

    year = 0;
    p = StringChr(bptr, '(');
    if(p != NULL)
    {
        for(p++; *p == ' ' || *p == '\t';)
            p++;
        for(q = p; *p >= '0' && *p <= '9';)
            p++;
        for(r = p; *p == ' ' || *p == '\t' || *p == ')';)
            p++;
        if(*p == '\n' || *p == '\0')
        {
            ch = *r;
            *r = '\0';
            year = atoi(q);
            if(year < 1900)
                *r = ch;
            else
            {
                for(q--; *q == ' ' || *q == '\t' || *q == '(';)
                    q--;
                *++q = '\0';
            }
        }
    }

    cit_gen.Reset(new ncbi::objects::CCit_gen);

    if(bptr != NULL)
        cit_gen->SetCit(bptr);

    cit_gen->SetAuthors(*auth_list);
    cit_gen->SetTitle(title->GetName());

    if(year >= 1900)
        cit_gen->SetDate().SetStd().SetYear(year);

    return cit_gen;
}

/**********************************************************
 *
 *   ValNodePtr journal(pp, bptr, eptr, auth, title,
 *                      has_muid, cit_art, er):
 *
 *      Return a ValNodePtr.
 *
 **********************************************************/
ncbi::CRef<ncbi::objects::CPub> journal(ParserPtr pp, char* bptr, char* eptr, ncbi::CRef<ncbi::objects::CAuth_list>& auth_list,
                                        ncbi::CRef<ncbi::objects::CTitle::C_E>& title, bool has_muid, ncbi::CRef<ncbi::objects::CCit_art>& cit_art, Int4 er)
{
    int        pre = 0;
    char*    p;
    char*    nearend;
    char*    end;
    bool       all_zeros;
    int        retval = ParFlat_MISSING_JOURNAL;

    ncbi::CRef<ncbi::objects::CPub> ret(new ncbi::objects::CPub);
    if(bptr == NULL)
    {
        const Char* title_str = title.Empty() ? NULL : title->GetName().c_str();
        ret->SetGen(*get_unpub(bptr, eptr, auth_list, title_str));
        return ret;
    }

    p = bptr;
    size_t my_len = StringLen(p);
    if(my_len > 7)
    {
        nearend = p + StringLen(p) - 1;
        while(*nearend == ' ' || *nearend == '\t' || *nearend == '.')
            *nearend-- = '\0';

        nearend -= 8;
        end = nearend + 2;
        if(StringNICmp("In press", nearend + 1, 8) == 0)
        {
            pre = 2;
            *(nearend + 1) = '\0';
        }
        if(StringNICmp("Submitted", nearend, 9) == 0)
        {
            pre = 1;
            *nearend = '\0';
        }
        if(pre == 0 && *end == '(' && IS_DIGIT(*(end + 1)) != 0)
        {
            for(nearend = end - 1; nearend > bptr && *nearend != ' ';)
                nearend--;
            if(StringNICmp("In press", nearend + 1, 8) == 0)
            {
                pre = 2;
                *(nearend + 1) = '\0';
            }
        }
    }

    if(my_len >= 6 && *p == '(')
    {
        p += 6;
        my_len -= 6;
        if(StringNCmp(" In press", p, 9) == 0)
        {
            retval = ParFlat_IN_PRESS;
            pre = 2;
        }
    }

    p = bptr;
    my_len = StringLen(p);
    if(StringNCmp("Unpub", p, 5) == 0 || StringNCmp("Unknown", p, 7) == 0)
    {
        retval = ParFlat_UNPUB_JOURNAL;
        const Char* title_str = title.Empty() ? NULL : title->GetName().c_str();
        ret->SetGen(*get_unpub(bptr, eptr, auth_list, title_str));
    }
    else if(StringNCmp("(in)", p, 4) == 0)
    {
        retval = ParFlat_MONOGRAPH_NOT_JOURNAL;

        ncbi::CRef<ncbi::objects::CCit_art> article = get_book(bptr, auth_list, title, pre, pp->format, p);

        if (article.Empty())
            ret->SetGen(*get_error(bptr, auth_list, title));
        else
            ret->SetArticle(*article);

    }
    else if (StringNCmp("Thesis", p, 6) == 0)
    {
        retval = ParFlat_THESIS_CITATION;

        ncbi::CRef<ncbi::objects::CCit_let> cit_let = get_thesis(bptr, auth_list, title, pre);
        if (cit_let.Empty())
        {
            ret.Reset();
            return ret;
        }
        ret->SetMan(*cit_let);
    }
    else if (StringNCmp("Submi", p,  5) == 0)
    {
        retval = ParFlat_SUBMITTED;

        ncbi::CRef<ncbi::objects::CCit_sub> cit_sub = get_sub(pp, bptr, auth_list);
        if (cit_sub.Empty())
        {
            ret.Reset();
            return ret;
        }
        
        ret->SetSub(*cit_sub);
    }
    else if(StringNCmp("Published in GSDB", p,  17) == 0)
    {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_GsdbRefDropped,
                  "A published-in-gsdb reference was encountered and has been dropped [%s]",
                  bptr);
        retval = ParFlat_SUBMITTED;

        ncbi::CRef<ncbi::objects::CCit_sub> cit_sub = get_sub_gsdb(bptr, auth_list, title, pp);
        if (cit_sub.Empty())
        {
            ret.Reset();
            return ret;
        }

        ret->SetSub(*cit_sub);
    }
    else if(StringNCmp("Patent", p, 6) == 0)
    {
        retval = ParFlat_PATENT_CITATION;

        if (pp->seqtype == ncbi::objects::CSeq_id::e_Genbank || pp->seqtype == ncbi::objects::CSeq_id::e_Ddbj ||
            pp->seqtype == ncbi::objects::CSeq_id::e_Embl || pp->seqtype == ncbi::objects::CSeq_id::e_Other ||
            pp->seqtype == ncbi::objects::CSeq_id::e_Tpe || pp->seqtype == ncbi::objects::CSeq_id::e_Tpg ||
            pp->seqtype == ncbi::objects::CSeq_id::e_Tpd)
        {
            ncbi::CRef<ncbi::objects::CCit_pat> cit_pat = get_pat(pp, bptr, auth_list, title, eptr);
            if (cit_pat.Empty())
            {
                ret.Reset();
                return ret;
            }

            ret->SetPatent(*cit_pat);
        }
        else
        {
            ret.Reset();
            return ret;
        }
    }
    else if(StringNCmp("Book:", p, 5) == 0)
    {
        retval = ParFlat_BOOK_CITATION;

        ncbi::CRef<ncbi::objects::CCit_book> book = get_whole_book(bptr, auth_list, title, pre);
        if(book.Empty())
        {
            ret.Reset();
            return ret;
        }

        ret->SetBook(*book);
    }
    else if(StringNICmp("Published Only in Database", p, 26) == 0)
    {
        retval = ParFlat_GEN_CITATION;
        ncbi::CRef<ncbi::objects::CCit_gen> cit_gen = fta_get_citgen(bptr, auth_list, title);

        if (cit_gen.Empty())
        {
            ret.Reset();
            return ret;
        }

        ret->SetGen(*cit_gen);
    }
    else if(StringNICmp("Online Publication", p, 18) == 0)
    {
        retval = ParFlat_ONLINE_CITATION;

        ncbi::CRef<ncbi::objects::CCit_gen> cit_gen = fta_get_citgen(bptr, auth_list, title);

        if (cit_gen.Empty())
        {
            ret.Reset();
            return ret;
        }

        ret->SetGen(*cit_gen);
    }

    if(retval == ParFlat_MISSING_JOURNAL)
    {
        if(cit_art.NotEmpty())
            ret->SetArticle(*cit_art);
        else
        {
            ncbi::CRef<ncbi::objects::CCit_art> new_art = get_art(pp, bptr, auth_list, title, pre,
                                                                  has_muid, &all_zeros, er);
            if (new_art.Empty())
            {
                if(!all_zeros &&
                   StringNCmp(bptr, "(er)", 4) != 0 && er == 0)
                    ErrPostEx(SEV_WARNING, ERR_REFERENCE_Illegalreference,
                              "Journal format error (cit-gen created): %s",
                              bptr);

                ret->SetGen(*get_error(bptr, auth_list, title));
            }
            else
                ret->SetArticle(*new_art);
        }
    }

    return ret;
}

/**********************************************************/
static char* FindBackSemicolon(char* pchStart, char* pchCurrent)
{
    if(pchStart == NULL || pchCurrent == NULL || pchStart >= pchCurrent)
        return(NULL);

    for(pchCurrent--; pchCurrent >= pchStart; pchCurrent--)
    {
        if(isspace((int) *pchCurrent) != 0)
            continue;
        if(*pchCurrent == ';')
            return(pchCurrent);
        break;
    }

    return(NULL);
}

/**********************************************************/
static char* FindSemicolon(char* str)
{
    if(str == NULL || *str == '\0')
        return(NULL);

    str = SkipSpaces(str);

    if(*str == ';')
        return(str);

    return(NULL);
}

/**********************************************************/
static char* ExtractErratum(char* comm)
{
    char* start;
    char* pchNumber = NULL;
    char* end;
    char* p;

    if(comm == NULL)
        return(NULL);

    start = StringStr(comm, "Erratum:");
    if(start == NULL)
        return(comm);

    end = StringChr(start, ']');
    if(end == NULL)
        return(comm);

    pchNumber = end + 1;
    end = FindSemicolon(pchNumber);
    if(end != NULL)
        pchNumber = end + 1;
    p = FindBackSemicolon(comm, start);
    if(p != NULL)
        start = p;
    fta_StringCpy(start, pchNumber);

    /* Check if the string after cutting signature is empty. If it's really
     * empty we have to ignore the whole string (comment).
     * Do you want to have a comment which contains nothing!? Probably no.
     */
    for(p = comm; *p == ' ' || *p == '\t' || *p == '\n';)
        p++;
    if(*p == '\0')
        *comm = '\0';

    return(comm);
}

/**********************************************************/
static void XMLGetXrefs(char* entry, XmlIndexPtr xip, TQualVector& quals)
{
    XmlIndexPtr xipqual;

    if(entry == NULL || xip == NULL)
        return;

    for (; xip != NULL; xip = xip->next)
    {
        if(xip->subtags == NULL)
            continue;

        ncbi::CRef<ncbi::objects::CGb_qual> qual(new ncbi::objects::CGb_qual);

        for(xipqual = xip->subtags; xipqual != NULL; xipqual = xipqual->next)
        {
            if (xipqual->tag == INSDXREF_DBNAME)
                qual->SetQual(XMLGetTagValue(entry, xipqual));
            else if(xipqual->tag == INSDXREF_ID)
                qual->SetVal(XMLGetTagValue(entry, xipqual));
        }

        if (qual->IsSetQual() && !qual->GetQual().empty())
            quals.push_back(qual);
    }
}

/**********************************************************/
static void fta_add_article_ids(ncbi::objects::CPub& pub, const std::string& doi, const std::string& agricola)
{
    if (doi.empty() && agricola.empty())
        return;

    if (pub.IsArticle())
    {
        ncbi::objects::CCit_art& cit_art = pub.SetArticle();

        if (!agricola.empty())
        {
            ncbi::CRef<ncbi::objects::CArticleId> id(new ncbi::objects::CArticleId);
            id->SetOther().SetDb("AGRICOLA");
            id->SetOther().SetTag().SetStr(agricola);

            cit_art.SetIds().Set().push_front(id);
        }

        if (!doi.empty())
        {
            ncbi::CRef<ncbi::objects::CArticleId> id(new ncbi::objects::CArticleId);
            id->SetDoi().Set(doi);

            cit_art.SetIds().Set().push_front(id);
        }
    }
}

/**********************************************************/
Int4 fta_remark_is_er(const Char* str)
{
    const char **b;
    char*    s;
    Int4       i;

    s = StringSave(str);
    ShrinkSpaces(s);
    for(i = 1, b = ERRemarks; *b != NULL; b++, i++)
        if(StringIStr(s, *b) != NULL)
            break;

    MemFree(s);
    if(*b == NULL)
        return(0);
    if(i < 7)
        return(1);                      /* epublish     */
    return(2);                          /* aheadofprint */
}

/**********************************************************/
static ncbi::CRef<ncbi::objects::CPubdesc> XMLRefs(ParserPtr pp, DataBlkPtr dbp, bool& no_auth, bool& rej)
{
    char*           title;

    char*           p;
    char*           q;
    char*           r;
    bool              is_online;
    Int4              pmid;
    bool              retstat;

    XmlIndexPtr       xip;

    Int4              er;

    ncbi::CRef<ncbi::objects::CPubdesc> desc;

    if(pp == NULL || dbp == NULL || dbp->offset == NULL || dbp->data == NULL)
        return desc;

    desc.Reset(new ncbi::objects::CPubdesc);

    p = XMLFindTagValue(dbp->offset, (XmlIndexPtr) dbp->data,
                        INSDREFERENCE_REFERENCE);
    if(p != NULL && isdigit((int) *p) != 0)
    {
        desc->SetPub().Set().push_back(get_num(p));
    }
    else
    {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_Illegalreference,
                  "No reference number.");
    }

    if(p != NULL)
        MemFree(p);

    p = XMLFindTagValue(dbp->offset, (XmlIndexPtr) dbp->data,
                        INSDREFERENCE_MEDLINE);
    if(p != NULL)
    {
        rej = true;
        MemFree(p);
        desc.Reset();
        return desc;
    }

    pmid = 0;
    p = XMLFindTagValue(dbp->offset, (XmlIndexPtr) dbp->data,
                        INSDREFERENCE_PUBMED);
    if(p != NULL)
    {
        pmid = ncbi::NStr::StringToInt(p, ncbi::NStr::fAllowTrailingSymbols);
        MemFree(p);
    }

    ncbi::CRef<ncbi::objects::CAuth_list> auth_list;

    p = XMLConcatSubTags(dbp->offset, (XmlIndexPtr) dbp->data,
                         INSDREFERENCE_AUTHORS, ',');
    if(p != NULL)
    {
        if(pp->xml_comp)
        {
            q = StringRChr(p, '.');
            if(q == NULL || q[1] != '\0')
            {
                q = (char*) MemNew(StringLen(p) + 2);
                StringCpy(q, p);
                StringCat(q, ".");
                MemFree(p);
                p = q;
                q = NULL;
            }
        }
        for(q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;
        if(*q != '\0')
        {
            q = XMLFindTagValue(dbp->offset, (XmlIndexPtr) dbp->data,
                                INSDREFERENCE_JOURNAL);
            get_auth(p, (pp->source == ParFlat_EMBL) ? EMBL_REF : GB_REF, q, auth_list);
            MemFree(q);
        }
        MemFree(p);
    }

    p = XMLFindTagValue(dbp->offset, (XmlIndexPtr) dbp->data,
                        INSDREFERENCE_CONSORTIUM);
    if(p != NULL)
    {
        for(q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;

        if (*q != '\0')
            get_auth_consortium(p, auth_list);

        MemFree(p);
    }

    if (auth_list.Empty() || !auth_list->IsSetNames())
        no_auth = true;

    p = XMLFindTagValue(dbp->offset, (XmlIndexPtr) dbp->data,
                        INSDREFERENCE_TITLE);

    ncbi::CRef<ncbi::objects::CTitle::C_E> title_art(new ncbi::objects::CTitle::C_E);
    if (p != NULL)
    {
        if(StringNCmp(p, "Direct Submission", 17) != 0 &&
           *p != '\0' && *p != ';')
        {
            title = clean_up(p);
            if(title != NULL)
            {
                title_art->SetName(title);
                MemFree(title);
            }
        }
        MemFree(p);
    }

    is_online = false;
    p = XMLFindTagValue(dbp->offset, (XmlIndexPtr) dbp->data,
                        INSDREFERENCE_JOURNAL);
    if(p == NULL)
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "No JOURNAL line, reference dropped");
        desc.Reset();
        return desc;
    }

    if(*p == '\0' || *p == ';')
    {
        ErrPostStr(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                   "JOURNAL line is empty, reference dropped");
        MemFree(p);
        desc.Reset();
        return desc;
    }

    if (ncbi::NStr::EqualNocase(p, 0, 18, "Online Publication"))
        is_online = true;

    r = XMLFindTagValue(dbp->offset, (XmlIndexPtr) dbp->data,
                        INSDREFERENCE_REMARK);
    if(r != NULL)
    {
        r = ExtractErratum(r);
        desc->SetComment(ncbi::NStr::Sanitize(r));
        MemFree(r);

        if(!is_online)
            normalize_comment(desc->SetComment());
    }

    er = fta_remark_is_er(desc->IsSetComment() ? desc->GetComment().c_str() : NULL);

    ncbi::CRef<ncbi::objects::CCit_art> cit_art;
    if((StringNCmp(p, "(er)", 4) == 0 || er > 0) &&
       pmid > 0 && pp->medserver == 1)
    {
        cit_art = fta_citart_by_pmid(pmid, retstat);
        if(retstat && cit_art.Empty())
            pmid = 0;
    }

    if (pmid > 0)
    {
        ncbi::CRef<ncbi::objects::CPub> pub(new ncbi::objects::CPub);
        pub->SetPmid().Set(ENTREZ_ID_FROM(int, pmid));
        desc->SetPub().Set().push_back(pub);
    }

    ncbi::CRef<ncbi::objects::CPub> pub_ref = journal(pp, p, p + StringLen(p), auth_list, title_art, false, cit_art, er);
    MemFree(p);

    TQualVector xrefs;
    for (xip = (XmlIndexPtr)dbp->data; xip != NULL; xip = xip->next)
    {
        if (xip->tag == INSDREFERENCE_XREF)
            XMLGetXrefs(dbp->offset, xip->subtags, xrefs);
    }

    std::string doi;
    std::string agricola;
    ITERATE(TQualVector, xref, xrefs)
    {
        if (!(*xref)->IsSetQual())
            continue;

        if (ncbi::NStr::EqualNocase((*xref)->GetQual(), "ARGICOLA") && agricola.empty())
            agricola = (*xref)->GetVal();
        else if (ncbi::NStr::EqualNocase((*xref)->GetQual(), "DOI") && doi.empty())
            doi = (*xref)->GetVal();
    }

    fta_add_article_ids(*pub_ref, doi, agricola);

    if (pub_ref.Empty())
    {
        desc.Reset();
        return desc;
    }

    if(dbp->type == ParFlat_REF_NO_TARGET)
        desc->SetReftype(3);

    desc->SetPub().Set().push_back(pub_ref);

    return desc;
}

/**********************************************************/
ncbi::CRef<ncbi::objects::CPubdesc> gb_refs_common(ParserPtr pp, DataBlkPtr dbp, Int4 col_data,
                                                   bool bParser, DataBlkPtr** ppInd, bool& no_auth)
{
    static DataBlkPtr ind[MAXKW+1];

    bool              has_muid;
    char*           p;
    char*           q;
    char*           r;
    bool              is_online;
    Int4              pmid;
    bool              retstat;
    Int4              er;

    ncbi::CRef<ncbi::objects::CPubdesc> desc(new ncbi::objects::CPubdesc);

    p = dbp->offset + col_data;
    if(bParser)
    {
        /* This branch works when this function called in context of PARSER
         */
        if(*p >= '0' && *p <= '9')
            desc->SetPub().Set().push_back(get_num(p));
        else
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_Illegalreference,
                      "No reference number.");
        ind_subdbp(dbp, ind, MAXKW, ParFlat_GENBANK);
    }
    else
    {
        /* This branch works when this function is called in context of GBDIFF
         */
        if(ppInd != NULL)
        {
            ind_subdbp(dbp, ind, MAXKW, ParFlat_GENBANK);
            *ppInd = &ind[0];

            return desc;
        }

        if(*p < '0' || *p > '9')
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_Illegalreference,
                      "No reference number.");
    }

    has_muid = false;
    if(ind[ParFlat_MEDLINE] != NULL)
    {
        p = ind[ParFlat_MEDLINE]->offset;
        ncbi::CRef<ncbi::objects::CPub> pub = get_muid(p, ParFlat_GENBANK);
        if (pub.NotEmpty())
        {
            has_muid = true;
            desc->SetPub().Set().push_back(get_num(p));
        }
    }

    pmid = 0;
    if(ind[ParFlat_PUBMED] != NULL)
    {
        p = ind[ParFlat_PUBMED]->offset;
        if(p != NULL)
            pmid = ncbi::NStr::StringToInt(p, ncbi::NStr::fAllowTrailingSymbols);
    }

    ncbi::CRef<ncbi::objects::CAuth_list> auth_list;
    if(ind[ParFlat_AUTHORS] != NULL)
    {
        p = ind[ParFlat_AUTHORS]->offset;
        for(q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;

        if(*q != '\0')
        {
            if(ind[ParFlat_JOURNAL] != NULL)
                q = ind[ParFlat_JOURNAL]->offset;

            get_auth(p, GB_REF, q, auth_list);
        }
    }

    if(ind[ParFlat_CONSRTM] != NULL)
    {
        p = ind[ParFlat_CONSRTM]->offset;
        for(q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;

        if (*q != '\0')
            get_auth_consortium(p, auth_list);
    }

    if (auth_list.Empty() || !auth_list->IsSetNames())
        no_auth = true;

    ncbi::CRef<ncbi::objects::CTitle::C_E> title_art;
    if(ind[ParFlat_TITLE] != NULL)
    {
        p = ind[ParFlat_TITLE]->offset;
        if(StringNCmp(p, "Direct Submission", 17) != 0 &&
           *p != '\0' && *p != ';')
        {
            q = clean_up(p);
            if(q != NULL)
            {
                title_art.Reset(new ncbi::objects::CTitle::C_E);
                title_art->SetName(ncbi::NStr::Sanitize(q));
                MemFree(q);
            }
        }
    }

    if(ind[ParFlat_JOURNAL] == NULL)
    {
        ErrPostStr(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                   "No JOURNAL line, reference dropped");

        desc.Reset();
        return desc;
    }

    p = ind[ParFlat_JOURNAL]->offset;
    if(*p == '\0' || *p == ';')
    {
        ErrPostStr(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                   "JOURNAL line is empty, reference dropped");

        desc.Reset();
        return desc;
    }

    is_online = (StringNICmp(p, "Online Publication", 18) == 0);

    if(ind[ParFlat_REMARK] != NULL)
    {
        r = ind[ParFlat_REMARK]->offset;
        r = ExtractErratum(r);
        desc->SetComment(ncbi::NStr::Sanitize(r));

        if(!is_online)
            normalize_comment(desc->SetComment());
    }

    er = fta_remark_is_er(desc->IsSetComment() ? desc->GetComment().c_str() : NULL);

    ncbi::CRef<ncbi::objects::CCit_art> cit_art;

    if(pp->medserver == 1 && pmid > 0 &&
       (StringNCmp(p, "(er)", 4) == 0 || er > 0))
    {
        cit_art = fta_citart_by_pmid(pmid, retstat);
        if(retstat && cit_art == NULL)
            pmid = 0;
    }

    if (pmid > 0)
    {
        ncbi::CRef<ncbi::objects::CPub> pub(new ncbi::objects::CPub);
        pub->SetPmid().Set(ENTREZ_ID_FROM(int, pmid));
        desc->SetPub().Set().push_back(pub);
    }

    ncbi::CRef<ncbi::objects::CPub> pub_ref = journal(pp, p, p + ind[ParFlat_JOURNAL]->len,
                                                      auth_list, title_art, has_muid, cit_art, er);

    if (pub_ref.Empty())
    {
        desc.Reset();
        return desc;
    }

    if(dbp->type == ParFlat_REF_NO_TARGET)
        desc->SetReftype(3);

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
static ncbi::CRef<ncbi::objects::CPubdesc> embl_refs(ParserPtr pp, DataBlkPtr dbp, Int4 col_data, bool& no_auth)
{
    static DataBlkPtr ind[MAXKW+1];
    char*           s;

    char*           title;
    bool              has_muid;
    char*           p;
    char*           q;
    Int4              pmid;

    bool              retstat;
    Int4              er;

    ncbi::CRef<ncbi::objects::CPubdesc> desc(new ncbi::objects::CPubdesc);

    p = dbp->offset + col_data;
    while((*p < '0' || *p > '9') && dbp->len > 0)
        p++;
    if(*p >= '0' && *p <= '9')
        desc->SetPub().Set().push_back(get_num(p));
    else
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_Illegalreference,
                  "No reference number.");

    ind_subdbp(dbp, ind, MAXKW, ParFlat_EMBL);

    has_muid = false;
    pmid = 0;
    
    std::string doi;
    std::string agricola;

    if(ind[ParFlat_RC] != NULL)
        desc->SetComment(ncbi::NStr::Sanitize(ind[ParFlat_RC]->offset));

    er = fta_remark_is_er(desc->IsSetComment() ? desc->GetComment().c_str() : NULL);

    if(ind[ParFlat_RX] != NULL)
    {
        p = ind[ParFlat_RX]->offset;
        ncbi::CRef<ncbi::objects::CPub> pub = get_muid(p, ParFlat_EMBL);

        const Char* id = get_embl_str_pub_id(p, "DOI;");
        if (id)
            doi = id;

        id = get_embl_str_pub_id(p, "AGRICOLA;");
        if (id)
            agricola = id;

        if (pub.NotEmpty())
        {
            desc->SetPub().Set().push_back(pub);
            has_muid = true;
        }

        pmid = get_embl_pmid(p);
    }

    ncbi::CRef<ncbi::objects::CAuth_list> auth_list;
    if(ind[ParFlat_RA] != NULL)
    {
        p = ind[ParFlat_RA]->offset;
        s = p + StringLen(p) - 1;
        if(*s == ';')
            *s = '\0';
        for(q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;
        if(*q != '\0')
        {
            if(ind[ParFlat_RL] != NULL)
                q = ind[ParFlat_RL]->offset;

            get_auth(p, EMBL_REF, q, auth_list);
        }
    }

    if(ind[ParFlat_RG] != NULL)
    {
        p = ind[ParFlat_RG]->offset;
        s = p + StringLen(p) - 1;
        if(*s == ';')
            *s = '\0';

        for(q = p; *q == ' ' || *q == '.' || *q == ',';)
            q++;

        if (*q != '\0')
            get_auth_consortium(p, auth_list);
    }

    if (auth_list.Empty() || !auth_list->IsSetNames())
        no_auth = true;

    ncbi::CRef<ncbi::objects::CTitle::C_E> title_art;
    if (ind[ParFlat_RT] != NULL)
    {
        p = ind[ParFlat_RT]->offset;
        if(*p != '\0' && *p != ';')
        {
            title = clean_up(p);
            if (title != NULL && title[0])
            {
                title_art.Reset(new ncbi::objects::CTitle::C_E);
                title_art->SetName(ncbi::NStr::Sanitize(title));
            }
            MemFree(title);
        }
    }

    if(ind[ParFlat_RL] == NULL)
    {
        ErrPostStr(SEV_ERROR, ERR_REFERENCE_Illegalreference,
                   "No JOURNAL line, reference dropped.");

        desc.Reset();
        return desc;
    }

    p = ind[ParFlat_RL]->offset;
    if(*p == '\0' || *p == ';')
    {
        ErrPostStr(SEV_ERROR, ERR_REFERENCE_Illegalreference,
                   "JOURNAL line is empty, reference dropped.");

        desc.Reset();
        return desc;
    }

    ncbi::CRef<ncbi::objects::CCit_art> cit_art;
    if ((StringNCmp(p, "(er)", 4) == 0 || er > 0) &&
        pmid > 0 && pp->medserver == 1)
    {
        cit_art = fta_citart_by_pmid(pmid, retstat);
        if(retstat && cit_art == NULL)
            pmid = 0;
    }

    if (pmid > 0)
    {
        ncbi::CRef<ncbi::objects::CPub> pub(new ncbi::objects::CPub);
        pub->SetPmid().Set(ENTREZ_ID_FROM(int, pmid));
        desc->SetPub().Set().push_back(pub);
    }

    ncbi::CRef<ncbi::objects::CPub> pub_ref = journal(pp, p, p + ind[ParFlat_RL]->len, auth_list,
                      title_art, has_muid, cit_art, er);

    if (pub_ref.Empty())
    {
        desc.Reset();
        return desc;
    }

    fta_add_article_ids(*pub_ref, doi, agricola);

    if(dbp->type == ParFlat_REF_NO_TARGET)
        desc->SetReftype(3);

    desc->SetPub().Set().push_back(pub_ref);

    return desc;
}

/**********************************************************/
static void fta_sort_pubs(TPubList& pubs)
{
    NON_CONST_ITERATE(TPubList, pub, pubs)
    {
        TPubList::iterator next_pub = pub;
        for (++next_pub; next_pub != pubs.end(); ++next_pub)
        {
            if ((*next_pub)->Which() > (*pub)->Which())
                continue;

            if ((*next_pub)->Which() == (*pub)->Which())
            {
                if (!(*pub)->IsMuid() || (*pub)->GetMuid() >= (*next_pub)->GetMuid())
                    continue;
            }

            pub->Swap(*next_pub);
        }
    }
}

/**********************************************************/
static void fta_check_long_last_name(const ncbi::objects::CAuth_list& authors, bool soft_report)
{
    static const size_t MAX_LAST_NAME_LEN = 30;

    ErrSev     sev;

    if (!authors.IsSetNames() || !authors.GetNames().IsStd())
        return;

    ITERATE(ncbi::objects::CAuth_list::C_Names::TStd, author, authors.GetNames().GetStd())
    {
        if (!(*author)->IsSetName() || !(*author)->GetName().IsName())
            continue;

        const ncbi::objects::CName_std& name = (*author)->GetName().GetName();

        if (name.IsSetLast() && name.GetLast().size() > MAX_LAST_NAME_LEN)
        {
            /* Downgrade severity of this error to WARNING
             * if in HTGS mode. As of 7/31/2002, very long
             * consortium names were treated as if
             * they were author last names, for HTGS data.
             * This can be reverted to ERROR after the
             * consortium name slot is available and utilized
             * in the ASN.1.
             */
            sev = (soft_report ? SEV_WARNING : SEV_ERROR);
            ErrPostEx(sev, ERR_REFERENCE_LongAuthorName,
                      "Last name of author exceeds 30 characters in length. A format error in the reference data might have caused the author name to be parsed incorrectly. Name is \"%s\".",
                      name.GetLast().c_str());
        }
    }
}

/**********************************************************/
static void fta_check_long_name_in_article(const ncbi::objects::CCit_art& cit_art, bool soft_report)
{
    if (cit_art.IsSetAuthors())
        fta_check_long_last_name(cit_art.GetAuthors(), soft_report);

    if (cit_art.IsSetFrom())
    {
        const ncbi::objects::CCit_book* book = nullptr;
        if (cit_art.GetFrom().IsBook())
            book = &cit_art.GetFrom().GetBook();
        else if (cit_art.GetFrom().IsProc())
        {
            if (cit_art.GetFrom().GetProc().IsSetBook())
                book = &cit_art.GetFrom().GetProc().GetBook();
        }

        if (book != nullptr && book->IsSetAuthors())
            fta_check_long_last_name(book->GetAuthors(), soft_report);
    }
}

/**********************************************************/
static void fta_check_long_names(const ncbi::objects::CPub& pub, bool soft_report)
{
    if (pub.IsGen())                        /* CitGen */
    {
        const ncbi::objects::CCit_gen& cit_gen = pub.GetGen();
        if (cit_gen.IsSetAuthors())
            fta_check_long_last_name(cit_gen.GetAuthors(), soft_report);
    }
    else if (pub.IsSub())                   /* CitSub */
    {
        if (!soft_report)
        {
            const ncbi::objects::CCit_sub& cit_sub = pub.GetSub();
            if (cit_sub.IsSetAuthors())
                fta_check_long_last_name(cit_sub.GetAuthors(), soft_report);
        }
    }
    else if (pub.IsMedline())                   /* Medline */
    {
        const ncbi::objects::CMedline_entry& medline = pub.GetMedline();
        if (medline.IsSetCit())
        {
            fta_check_long_name_in_article(medline.GetCit(), soft_report);
        }
    }
    else if (pub.IsArticle())                   /* CitArt */
    {
        fta_check_long_name_in_article(pub.GetArticle(), soft_report);
    }
    else if (pub.IsBook() || pub.IsProc() || pub.IsMan())  /* CitBook or CitProc or
                                                              CitLet */
    {
        const ncbi::objects::CCit_book* book = nullptr;

        if (pub.IsBook())
            book = &pub.GetBook();
        else if (pub.IsProc())
        {
            if (pub.GetProc().IsSetBook())
                book = &pub.GetProc().GetBook();
        }
        else
        {
            if (pub.GetMan().IsSetCit())
                book = &pub.GetMan().GetCit();
        }

        if (book != nullptr && book->IsSetAuthors())
            fta_check_long_last_name(book->GetAuthors(), soft_report);
    }
    else if (pub.IsPatent())                   /* CitPat */
    {
        const ncbi::objects::CCit_pat& patent = pub.GetPatent();

        if (patent.IsSetAuthors())
            fta_check_long_last_name(patent.GetAuthors(), soft_report);

        if (patent.IsSetApplicants())
            fta_check_long_last_name(patent.GetApplicants(), soft_report);

        if (patent.IsSetAssignees())
            fta_check_long_last_name(patent.GetAssignees(), soft_report);
    }
    else if (pub.IsEquiv())                  /* PubEquiv */
    {
        ITERATE(TPubList, cur_pub, pub.GetEquiv().Get())
        {
            fta_check_long_names(*(*cur_pub), soft_report);
        }
    }
}

/**********************************************************/
static void fta_propagate_pmid_muid(ncbi::objects::CPub_equiv& pub_equiv)
{
    Int4       pmid;
    Int4       muid;

    pmid = 0;
    muid = 0;

    ncbi::objects::CCit_art* cit_art = nullptr;
    NON_CONST_ITERATE(TPubList, pub, pub_equiv.Set())
    {
        if ((*pub)->IsMuid() && muid == 0)
            muid = ENTREZ_ID_TO(int, (*pub)->GetMuid());
        else if ((*pub)->IsPmid() && pmid == 0)
            pmid = ENTREZ_ID_TO(int, (*pub)->GetPmid().Get());
        else if ((*pub)->IsArticle() && cit_art == nullptr)
            cit_art = &(*pub)->SetArticle();
    }

    if (cit_art == NULL || (muid == 0 && pmid == 0))
        return;

    if(muid != 0)
    {
        ncbi::CRef<ncbi::objects::CArticleId> id(new ncbi::objects::CArticleId);
        id->SetMedline().Set(ENTREZ_ID_FROM(int, muid));
        cit_art->SetIds().Set().push_front(id);
    }

    if(pmid != 0)
    {
        ncbi::CRef<ncbi::objects::CArticleId> id(new ncbi::objects::CArticleId);
        id->SetPubmed().Set(ENTREZ_ID_FROM(int, pmid));
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
ncbi::CRef<ncbi::objects::CPubdesc> DescrRefs(ParserPtr pp, DataBlkPtr dbp, Int4 col_data)
{
    bool soft_report = false;

    bool rej = false;
    bool no_auth = false;

    if(pp->mode == FTA_HTGS_MODE)
        soft_report = true;

    ncbi::CRef<ncbi::objects::CPubdesc> desc;

    if (pp->format == ParFlat_SPROT)
        desc = sp_refs(pp, dbp, col_data);
    else if(pp->format == ParFlat_XML)
        desc = XMLRefs(pp, dbp, no_auth, rej);
    else if(pp->format == ParFlat_GENBANK)
        desc = gb_refs_common(pp, dbp, col_data, true, NULL, no_auth);
    else if(pp->format == ParFlat_EMBL)
        desc = embl_refs(pp, dbp, col_data, no_auth);

    if(desc && desc->IsSetComment())
    {
        char *comment = (char *) desc->GetComment().c_str();
        ShrinkSpaces(comment);
        desc->SetComment(comment);
    }

    if(no_auth)
    {
        if(pp->source == ParFlat_EMBL)
            ErrPostEx(SEV_ERROR, ERR_REFERENCE_MissingAuthors,
                      "Reference has no author names.");
        else
        {
            ErrPostEx(SEV_REJECT, ERR_REFERENCE_MissingAuthors,
                      "Reference has no author names. Entry dropped.");
            pp->entrylist[pp->curindx]->drop = 1;
        }
    }

    if(rej)
    {
        ErrPostEx(SEV_REJECT, ERR_REFERENCE_InvalidMuid,
                  "Use of Medline ID in INSDSeq format is not alowed. Entry dropped.");
        pp->entrylist[pp->curindx]->drop = 1;
    }

    if (desc.NotEmpty() && desc->IsSetPub())
    {
        fta_sort_pubs(desc->SetPub().Set());

        ITERATE(TPubList, pub, desc->GetPub().Get())
        {
            fta_check_long_names(*(*pub), soft_report);
        }

        fta_propagate_pmid_muid(desc->SetPub());
    }

    return desc;
}
