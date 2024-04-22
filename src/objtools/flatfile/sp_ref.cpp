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
 * File Name: sp_ref.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Utility routines for parsing reference block of
 * SWISS-PROT flatfile.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/ArticleIdSet.hpp>
#include <objects/biblio/ArticleId.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Cit_let.hpp>
#include "sprot.h"
#include <objtools/flatfile/flatdefn.h>

#include "ftaerr.hpp"
#include "ftablock.h"
#include "asci_blk.h"
#include "utilref.h"
#include "add.h"
#include "utilfun.h"
#include "ref.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "sp_ref.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

struct ParRefBlk {
    Int4  refnum; /* REFERENCE for GenBank, RN for Embl
                                           and Swiss-Prot */
    Int4  muid;   /* RM for Swiss-Prot */
    TEntrezId pmid;
    char* doi;
    char* agricola;

    CRef<CAuth_list> authors; /* a linklist of the author's name,
                                                       AUTHORS for GenBank, RA for Embl
                                                       and Swiss-Prot */
    char*            title;   /* TITLE for GenBank */
    string           journal; /* JOURNAL for GenBank, RL for Embl
                                           and Swiss-Prot */
    char*            cit;     /* for cit-gen in Swiss-Prot */
    string           vol;
    string           pages;
    char*            year;
    string           affil;
    char*            country;
    string           comment; /* STANDARD for GenBank, RP for
                                           Swiss-Prot, RC for Embl */
    Uint1            reftype; /* 0 if ignore the reference,
                                           1 if non-parseable,
                                           2 if thesis,
                                           3 if article,
                                           4 submitted etc. */

    ParRefBlk() :
        refnum(0),
        muid(0),
        pmid(ZERO_ENTREZ_ID),
        doi(nullptr),
        agricola(nullptr),
        title(nullptr),
        cit(nullptr),
        year(nullptr),
        country(nullptr),
        reftype(ParFlat_ReftypeIgnore)
    {
    }
};
using ParRefBlkPtr = ParRefBlk*;

const char* ParFlat_SPRefRcToken[] = {
    "MEDLINE", "PLASMID", "SPECIES", "STRAIN", "TISSUE", "TRANSPOSON", nullptr
};


/**********************************************************
 *
 *   static bool NotName(name):
 *
 *      Trying to check that the token doesn't look
 *   like name.
 *
 **********************************************************/
static bool NotName(const char* name)
{
    ValNodePtr vnp;
    char*      tmp;
    const char* s;
    Int4       i;

    if (! name)
        return false;
    if (! StringChr(name, '.'))
        return true;
    tmp = StringSave(name);
    vnp = get_tokens(tmp, " ");
    if (! vnp)
        return true;
    for (i = 0; vnp->next; vnp = vnp->next)
        i++;
    if (i > 3)
        return true;

    s = vnp->data;
    if (StringLen(s) > 8)
        return true;

    while (isalpha(*s) != 0 || *s == '.')
        s++;
    if (*s != '\0')
        return true;

    MemFree(tmp);
    return false;
}

/**********************************************************
 *
 *   static Int4 GetDataFromRN(dbp, col_data):
 *
 *      RN line has a format:  RN   [1]
 *
 *                                              10-27-93
 *
 **********************************************************/
static Int4 GetDataFromRN(DataBlkPtr dbp, Int4 col_data)
{
    char* bptr;
    char* eptr;
    char* str;
    Int4  num = 0;

    bptr = dbp->mOffset + col_data;
    eptr = bptr + dbp->len;
    while (isdigit(*bptr) == 0 && bptr < eptr)
        bptr++;
    for (str = bptr; isdigit(*str) != 0 && str < eptr;)
        str++;

    num = NStr::StringToInt(string(bptr, str), NStr::fAllowTrailingSymbols);
    return (num);
}

/**********************************************************
 *
 *   static void CkSPComTopics(pp, str):
 *
 *      SWISS-PROT flat file Table of valid Reference Comment
 *   (RC line) tokens,
 *   s.t.  RC   STRAIN=SPRAGUE-DAWLEY; TISSUE=LIVER; STRAIN=ISOLATE JAPAN;
 *
 **********************************************************/
static void CkSPComTopics(ParserPtr pp, const char* str)
{
    const char* ptr1;

    for (ptr1 = str; *ptr1 != '\0';) {
        if (fta_StringMatch(ParFlat_SPRefRcToken, ptr1) == -1) {
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_UnkRefRcToken, "Unknown Reference Comment token (swiss-prot) w/ data, %s", ptr1);
        }

        while (*ptr1 != '\0' && *ptr1 != ';')
            ptr1++;

        while (*ptr1 == ';' || *ptr1 == ' ')
            ptr1++;
    }
}

/**********************************************************
 *
 *   static char* ParseYear(str):
 *
 *      Return first group of continuous 4 digits.
 *
 *                                              10-28-93
 *
 **********************************************************/
static char* ParseYear(const char* str)
{
    Int2  i;
    char* year;

    while (*str != '\0' && isdigit(*str) == 0)
        str++;

    if (*str == '\0')
        return nullptr;

    year = StringNew(4);
    for (i = 0; i < 4 && *str != '\0' && isdigit(*str) != 0; str++, i++)
        year[i] = *str;

    return (year);
}

/**********************************************************
 *
 *   static bool ParseJourLine(pp, prbp, str):
 *
 *      Return FALSE if str is not the format.
 *   str has the format: journal-name volume:pages(year).
 *
 *                                              10-28-93
 *
 **********************************************************/
static bool ParseJourLine(ParserPtr pp, ParRefBlkPtr prbp, const char* str)
{
    const char* ptr1;
    const char* ptr2;

    ptr1 = StringChr(str, ':');
    if (! ptr1)
        return false;

    for (ptr2 = ptr1; ptr2 != str && *ptr2 != ' ';)
        ptr2--;

    if (ptr2 == str)
        return false;

    prbp->journal.assign(str, ptr2);
    prbp->journal = NStr::TruncateSpaces(prbp->journal, NStr::eTrunc_End);

    ptr2++;
    prbp->vol.assign(ptr2, ptr1);

    ptr1++;
    ptr2 = StringChr(ptr1, '(');
    if (! ptr2)
        return false;

    prbp->pages.assign(ptr1, ptr2);

    std::vector<Char> pages(ptr1, ptr2);
    pages.push_back(0);
    if (valid_pages_range(&pages[0], prbp->journal.c_str(), 0, false) > 0)
        prbp->pages.assign(pages.begin(), pages.end());

    prbp->year = ParseYear(ptr2);
    return true;
}

/**********************************************************
 *
 *   static void ParseRLDataSP(pp, prbp, str):
 *
 *      swiss-prot
 *      - there are words within RL lines (first word)
 *        identifying citation that aren't parsed,
 *        ParFlat_SPRefRLNoParse
 *        - PUB_Gen, cit = "str"
 *          RL   SUBMITTED (MMM-year) to DATA BASE_NAME
 *          RL   THESIS (year), name-of-affiliation(s.t. university), country.
 *      - journal has format:
 *          RL   journal-name volume:pages(year).
 *
 *                                              10-28-93
 *
 **********************************************************/
static void ParseRLDataSP(ParserPtr pp, ParRefBlkPtr prbp, char* str)
{
    char* ptr1;
    char* ptr2;
    char* token;

    if (StringEquNI("UNPUBLISHED", str, 11)) {
        prbp->reftype = ParFlat_ReftypeUnpub;
        prbp->journal = str;
    } else if (StringEquNI("(IN)", str, 4)) {
        prbp->reftype = ParFlat_ReftypeBook;
        for (str += 4; *str == ' ';)
            str++;
        prbp->journal = str;
    } else if (StringEquNI("SUBMITTED", str, 9)) {
        prbp->reftype = ParFlat_ReftypeSubmit;
        prbp->journal = str;
    } else if (StringEquNI("PATENT NUMBER", str, 13)) {
        prbp->reftype = ParFlat_ReftypePatent;
        prbp->journal = str;
    } else if (StringEquNI("THESIS", str, 6)) {
        prbp->reftype = ParFlat_ReftypeThesis;

        ptr1       = str;
        ptr2       = PointToNextToken(ptr1);
        token      = GetTheCurrentToken(&ptr2); /* token = (year), */
        prbp->year = ParseYear(token);
        MemFree(token);

        ptr1 = StringChr(ptr2, ',');
        if (ptr1) /* university */
        {
            prbp->affil.assign(ptr2, ptr1);

            while (*ptr1 == ',' || *ptr1 == ' ')
                ptr1++;
            prbp->country = StringSave(ptr1);
            if (! StringEqu(prbp->country, "U.S.A."))
                CleanTailNoneAlphaChar(prbp->country);
        } else /* error */
        {
            prbp->reftype = ParFlat_ReftypeIgnore;
            prbp->journal = str;

            ErrPostEx(SEV_WARNING, ERR_REFERENCE_IllegalFormat, "Could not parse the thesis format (swiss-prot), %s, ref [%d].", str, prbp->refnum);
            prbp->cit = StringSave(str);
        }
    } else if (ParseJourLine(pp, prbp, str)) {
        if (prbp->year)
            prbp->reftype = ParFlat_ReftypeArticle;
        else {
            prbp->reftype = ParFlat_ReftypeIgnore;
            prbp->journal = str;
            prbp->cit     = StringSave(str);

            ErrPostEx(SEV_WARNING, ERR_REFERENCE_YearEquZero, "Article without year (swiss-prot), %s", str);
        }
    } else {
        prbp->reftype = ParFlat_ReftypeIgnore;
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_IllegalFormat, "Could not parse the article format (swiss-prot), %s, ref [%d].", str, (int)prbp->refnum);
        prbp->cit = StringSave(str);
    }
}

/**********************************************************/
static void GetSprotIds(ParRefBlkPtr prbp, char* str)
{
    char* p;
    char* q;
    bool  dois;
    bool  muids;
    bool  pmids;
    bool  agricolas;

    if (! prbp || ! str || str[0] == '\0')
        return;

    prbp->muid     = 0;
    prbp->pmid     = ZERO_ENTREZ_ID;
    prbp->doi      = nullptr;
    prbp->agricola = nullptr;
    dois           = false;
    muids          = false;
    pmids          = false;
    agricolas      = false;
    for (p = str; p;) {
        while (*p == ' ' || *p == '\t' || *p == ';')
            p++;
        q = p;
        p = StringChr(p, ';');
        if (p)
            *p = '\0';

        if (StringEquNI(q, "MEDLINE=", 8)) {
            if (prbp->muid == 0)
                prbp->muid = atoi(q + 8);
            else
                muids = true;
        } else if (StringEquNI(q, "PUBMED=", 7)) {
            if (prbp->pmid == ZERO_ENTREZ_ID)
                prbp->pmid = ENTREZ_ID_FROM(int, atoi(q + 7));
            else
                pmids = true;
        } else if (StringEquNI(q, "DOI=", 4)) {
            if (! prbp->doi)
                prbp->doi = StringSave(q + 4);
            else
                dois = true;
        } else if (StringEquNI(q, "AGRICOLA=", 9)) {
            if (! prbp->agricola)
                prbp->agricola = StringSave(q + 9);
            else
                agricolas = true;
        }

        if (p)
            *p++ = ';';
    }

    if (muids) {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_MultipleIdentifiers, "Reference has multiple MEDLINE identifiers. Ignoring all but the first.");
    }
    if (pmids) {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_MultipleIdentifiers, "Reference has multiple PubMed identifiers. Ignoring all but the first.");
    }
    if (dois) {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_MultipleIdentifiers, "Reference has multiple DOI identifiers. Ignoring all but the first.");
    }
    if (agricolas) {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_MultipleIdentifiers, "Reference has multiple AGRICOLA identifiers. Ignoring all but the first.");
    }
}

/**********************************************************
 *
 *   static ParRefBlkPtr SprotRefString(pp, dbp, col_data):
 *
 *                                              10-27-93
 *
 **********************************************************/
static ParRefBlkPtr SprotRefString(ParserPtr pp, DataBlkPtr dbp, Int4 col_data)
{
    DataBlkPtr subdbp;
    char*      str;
    char*      s;
    char*      p;

    ParRefBlkPtr prbp = new ParRefBlk;

    /* parser RN line, get number only
     */
    prbp->refnum = GetDataFromRN(dbp, col_data);

#ifdef DIFF

    prbp->refnum = 0;

#endif

    for (subdbp = static_cast<DataBlk*>(dbp->mpData); subdbp; subdbp = subdbp->mpNext) {
        /* process REFERENCE subkeywords
         */
        str        = StringSave(GetBlkDataReplaceNewLine(string_view(subdbp->mOffset, subdbp->len), (Int2)col_data));
        size_t len = StringLen(str);
        while (len > 0 && str[len - 1] == ';') {
            str[len - 1] = '\0';
            len--;
        }

        switch (subdbp->mType) {
        case ParFlatSP_RP:
            prbp->comment = str;
            break;
        case ParFlatSP_RC:
            CkSPComTopics(pp, str);
            if (! prbp->comment.empty())
                prbp->comment += ";~";
            prbp->comment += NStr::Sanitize(str);
            break;
        case ParFlatSP_RM:
            break; /* old format for muid */
        case ParFlatSP_RX:
            if (StringEquNI(str, "MEDLINE;", 8)) {
                for (s = str + 8; *s == ' ';)
                    s++;
                prbp->muid = (Int4)atol(s);
            } else
                GetSprotIds(prbp, str);
            break;
        case ParFlatSP_RA:
            get_auth(str, SP_REF, nullptr, prbp->authors);
            break;
        case ParFlatSP_RG:
            get_auth_consortium(str, prbp->authors);
            break;
        case ParFlatSP_RT:
            for (s = str; *s == '\"' || *s == ' ' || *s == '\t' ||
                          *s == '\n' || *s == ';';)
                s++;
            if (*s == '\0')
                break;
            for (p = s; *p != '\0';)
                p++;
            for (p--; *p == '\"' || *p == ' ' || *p == '\t' ||
                      *p == '\n' || *p == ';';
                 p--) {
                if (p == s) {
                    *p = '\0';
                    break;
                }
            }
            p[1]        = '\0';
            prbp->title = StringSave(s);
            break;
        case ParFlatSP_RL:
            ParseRLDataSP(pp, prbp, str);
            break;
        default:
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_UnkRefSubType, "Unknown reference subtype (swiss-prot) w/ data, %s", str);
            break;
        } /* switch */

        MemFree(str);
    } /* for, subdbp */

    return (prbp);
}

/**********************************************************/
static CRef<CDate> get_s_date(const Char* str, bool bstring)
{
    CRef<CDate> ret;

    const Char*        s;
    Int2               year;
    Int2               month = 0;
    Int2               cal;
    static const char* months[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

    for (s = str; *s != '\0' && *s != ')';)
        s++;
    if (*s == '\0')
        return ret;

    ret.Reset(new CDate);
    if (bstring)
        ret->SetStr(string(str, s));
    else {
        for (cal = 0; cal < 12; cal++) {
            if (StringEquNI(str, months[cal], 3)) {
                month = cal + 1;
                break;
            }
        }

        if (cal == 12) {
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_IllegalDate, "Unrecognised month: %s", str);
            ret.Reset();
            return ret;
        }

        ret->SetStd().SetMonth(month);

        year = NStr::StringToInt(str + 4, NStr::fAllowTrailingSymbols);

        CTime     time(CTime::eCurrent);
        CDate_std now(time);

        int cur_year = now.GetYear();

        if (year < 1900 || year > cur_year) {
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_IllegalDate, "Illegal year: %d", year);
        }

        ret->SetStd().SetYear(year);
    }

    return ret;
}

/**********************************************************/
static void SetCitTitle(CTitle& title, const Char* title_str)
{
    if (! title_str)
        return;

    CRef<CTitle::C_E> new_title(new CTitle::C_E);
    new_title->SetName(title_str);
    title.Set().push_back(new_title);
}

/**********************************************************
 *
 *   static ImprintPtr GetImprintPtr(prbp):
 *
 *      For swiss-prot only.
 *
 **********************************************************/
static bool GetImprintPtr(ParRefBlkPtr prbp, CImprint& imp)
{
    if (! prbp->year)
        return false;

    imp.SetDate().SetStd().SetYear(NStr::StringToInt(prbp->year, NStr::fAllowTrailingSymbols));

    if (! prbp->vol.empty()) {
        if (prbp->vol[0] == '0')
            imp.SetPrepub(CImprint::ePrepub_in_press);
        else
            imp.SetVolume(prbp->vol);
    }

    if (! prbp->pages.empty()) {
        if (prbp->pages[0] == '0')
            imp.SetPrepub(CImprint::ePrepub_in_press);
        else
            imp.SetPages(prbp->pages);
    }
    return true;
}

/**********************************************************
 *
 *   static CitSubPtr GetCitSubmit(prbp):
 *
 *      Only for swiss-prot.
 *
 **********************************************************/
static bool GetCitSubmit(ParRefBlkPtr prbp, CCit_sub& sub)
{
    const Char* bptr;
    const Char* s;

    while (! prbp->journal.empty() && prbp->journal.back() == '.')
        NStr::TrimSuffixInPlace(prbp->journal, ".");

    bptr = prbp->journal.c_str();
    for (s = bptr; *s != '(' && *s != '\0';)
        s++;

    CRef<CDate> date;
    if (NStr::Equal(s + 1, 0, 3, "XXX")) {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_IllegalDate, "%s", s);
        date = get_s_date(s + 1, true);
    } else
        date = get_s_date(s + 1, false);

    /* date is required in Imprint structure
     */
    if (date.Empty())
        return false;

    sub.SetImp().SetDate(*date);
    if (prbp->authors.NotEmpty())
        sub.SetAuthors(*prbp->authors);

    for (s = bptr; *s != ')' && *s != '\0';)
        s++;

#ifdef DIFF

    if (StringEquN(s + 1, " TO ", 4))
        s += 4;

#endif

    sub.SetImp().SetPub().SetStr(NStr::Sanitize(s + 1));
    sub.SetMedium(CCit_sub::eMedium_other);

    return true;
}

/**********************************************************/
static bool GetCitBookOld(ParRefBlkPtr prbp, CCit_art& article)
{
    const Char* s;
    const Char* vol;
    const Char* page;

    string title;

    ValNodePtr token;
    ValNodePtr vnp;
    ValNodePtr here;

    if (! prbp || prbp->journal.empty())
        return false;

    string bptr = prbp->journal;

    SIZE_TYPE ed_pos = NStr::FindNoCase(bptr, "ED.");
    if (ed_pos == NPOS)
        ed_pos = NStr::FindNoCase(bptr, "EDS.");
    if (ed_pos == NPOS)
        ed_pos = NStr::FindNoCase(bptr, "EDS,");

    if (ed_pos == NPOS) /* no authors found */
        return false;

    string      temp1 = bptr.substr(0, ed_pos);
    const char* temp2 = &bptr[ed_pos];
    if (*temp2 == 'S')
        temp2++;
    if (*temp2 == '.')
        temp2++;
    if (*temp2 == ',')
        temp2++;
    while (isspace(*temp2) != 0)
        temp2++;

    std::vector<Char> buf(temp1.begin(), temp1.end());
    buf.push_back(0);

    token = get_tokens(&buf[0], ", ");

    for (vnp = token, here = token; vnp; vnp = vnp->next) {
        if (NotName(vnp->data))
            here = vnp;
    }

    size_t len = 0;
    for (vnp = token; vnp; vnp = vnp->next) {
        len += StringLen(vnp->data) + 2;
        if (vnp == here)
            break;
    }

    title.reserve(len);
    for (vnp = token; vnp; vnp = vnp->next) {
        if (vnp != token)
            title.append(", ");
        title.append(vnp->data);
        if (vnp == here)
            break;
    }

    CRef<CAuth_list> authors;
    get_auth_from_toks(here->next, SP_REF, authors);
    if (authors.Empty() /* || title.empty() */) {
        return false;
    }

    CCit_book& book = article.SetFrom().SetBook();

    SetCitTitle(book.SetTitle(), title.c_str());
    book.SetAuthors(*authors);

    page = StringIStr(temp2, "PP.");
    if (page != 0)
        page += 4;
    else if ((page = StringIStr(temp2, "P.")) != 0)
        page += 3;
    else
        page = nullptr;

    if (page) {
        for (temp2 = page; *temp2 != '\0' && *temp2 != ',';)
            temp2++;

        book.SetImp().SetPages(string(page, temp2));
    }

    const Char* eptr = bptr.c_str() + bptr.size() - 1;
    if (*eptr == '.')
        eptr--;

    if (*eptr == ')')
        eptr--;

    if (isdigit(*eptr) == 0)
        return false;

    const Char* year = eptr;
    for (; isdigit(*year) != 0;)
        year--;

    CRef<CDate> date = get_date(year + 1);
    if (date.NotEmpty())
        book.SetImp().SetDate(*date);
    if (*year == '(')
        year--;

    while (*temp2 == ' ')
        temp2++;
    if (*temp2 != '(') {
        if (*temp2 == ',')
            temp2++;
        while (isspace(*temp2) != 0)
            temp2++;

        const char* p;
        for (p = year; p > temp2 && (*p == ' ' || *p == ',');)
            p--;
        if (p != year)
            p++;
        string affil = NStr::Sanitize(string(temp2, p));
        book.SetImp().SetPub().SetStr(affil);
    }

    SIZE_TYPE vol_pos = NStr::Find(bptr, "VOL.", NStr::eNocase);
    if (vol_pos != NPOS) {
        vol = bptr.c_str() + 4;
        for (s = vol; *s != '\0' && isdigit(*s) != 0;)
            s++;

        book.SetImp().SetVolume(string(vol, s));
    }

    if (prbp->authors.NotEmpty())
        article.SetAuthors(*prbp->authors);

    if (prbp->title && prbp->title[0] != '\0')
        SetCitTitle(article.SetTitle(), prbp->title);

    return true;
}

/**********************************************************
 *
 *   static CitArtPtr GetCitBook(prbp):
 *
 *      Only for swiss-prot.
 *
 *                                              10-28-93
 *
 **********************************************************/
static bool GetCitBook(ParRefBlkPtr prbp, CCit_art& article)
{
    char* publisher;
    char* year;
    char* title;
    char* pages;
    char* volume;

    char* ptr;
    char* p;
    char* q;
    char  ch;

    if (! prbp || prbp->journal.empty())
        return false;

    std::vector<Char> bptr(prbp->journal.begin(), prbp->journal.end());
    bptr.push_back(0);

    SIZE_TYPE eds_pos = NStr::Find(&bptr[0], "(EDS.)", NStr::eNocase);
    if (eds_pos == NPOS)
        return GetCitBookOld(prbp, article);

    ptr  = &bptr[0] + eds_pos;
    *ptr = '\0';
    CRef<CAuth_list> auth;
    get_auth(&bptr[0], SP_REF, nullptr, auth);
    *ptr = '(';
    if (auth.Empty())
        return false;

    for (ptr += 6; *ptr == ';' || *ptr == ' ' || *ptr == '\t';)
        ptr++;
    p = StringIStr(ptr, "PP.");
    if (! p || p == ptr) {
        return false;
    }

    ch = '\0';
    q  = p;
    for (p--; *p == ' ' || *p == '\t' || *p == ','; p--) {
        if (p != ptr)
            continue;
        ch = *p;
        *p = '\0';
        break;
    }
    if (p == ptr && *p == '\0') {
        *p = ch;
        return false;
    }

    ++p;
    title = StringSave(string_view(ptr, p - ptr));
    for (q += 3, p = q; *q >= '0' && *q <= '9';)
        q++;
    if (q == p || (*q != ':' && *q != '-')) {
        MemFree(title);
        return false;
    }

    if (*q == ':') {
        volume = StringSave(string_view(p, q - p));
        q++;
        for (p = q; *q >= '0' && *q <= '9';)
            q++;
        if (q == p || *q != '-') {
            MemFree(title);
            MemFree(volume);
            return false;
        }
        q++;
    } else {
        volume = nullptr;
        q++;
    }

    while (*q >= '0' && *q <= '9')
        q++;
    if (*(q - 1) == '-') {
        MemFree(title);
        if (volume)
            MemFree(volume);
        return false;
    }

    pages = StringSave(string_view(p, q - p));
    for (p = q; *q == ' ' || *q == ',';)
        q++;
    if (q == p || *q == '\0') {
        MemFree(pages);
        MemFree(title);
        if (volume)
            MemFree(volume);
        return false;
    }

    p = StringChr(q, '(');
    if (! p || p == q) {
        MemFree(pages);
        MemFree(title);
        if (volume)
            MemFree(volume);
        return false;
    }

    ch = '\0';
    for (p--; *p == ' ' || *p == '\t' || *p == ','; p--) {
        if (p != q)
            continue;
        ch = *p;
        *p = '\0';
        break;
    }
    if (p == q && *p == '\0') {
        *p = ch;
        MemFree(pages);
        MemFree(title);
        if (volume)
            MemFree(volume);
        return false;
    }
    ++p;
    publisher = StringSave(string_view(q, p - q));

    p = StringChr(q, '(');
    for (p++, q = p; *p >= '0' && *p <= '9';)
        p++;
    if (p - q != 4 || *p != ')') {
        MemFree(pages);
        MemFree(title);
        MemFree(publisher);
        if (volume)
            MemFree(volume);
        return false;
    }
    year = StringSave(string_view(q, p - q));

    CCit_book& book = article.SetFrom().SetBook();
    CImprint&  imp  = book.SetImp();

    imp.SetPub().SetStr(publisher);
    if (volume)
        imp.SetVolume(volume);
    imp.SetPages(pages);
    imp.SetDate().SetStd().SetYear(NStr::StringToInt(year, NStr::fAllowTrailingSymbols));
    MemFree(year);

    SetCitTitle(book.SetTitle(), title);
    book.SetAuthors(*auth);

    if (prbp->authors.NotEmpty())
        article.SetAuthors(*prbp->authors);

    if (prbp->title && prbp->title[0] != '\0')
        SetCitTitle(article.SetTitle(), prbp->title);

    return true;
}

/**********************************************************
 *
 *   static CitPatPtr GetCitPatent(prbp, source):
 *
 *      Only for swiss-prot.
 *
 *                                              10-28-93
 *
 **********************************************************/
static bool GetCitPatent(ParRefBlkPtr prbp, Parser::ESource source, CCit_pat& pat)
{
    string num;
    char*  p;
    char*  q;
    char   ch;
    char   country[3];

    if (! prbp || prbp->journal.empty())
        return (false);

    p = (char*)prbp->journal.c_str() + StringLen("PATENT NUMBER");
    while (*p == ' ')
        p++;
    for (q = p; *q != ' ' && *q != ',' && *q != '\0';)
        q++;
    if (q == p) {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Patent, "Incorrectly formatted patent reference: %s", prbp->journal.c_str());
        return (false);
    }

    ch  = *q;
    *q  = '\0';
    num = p;
    *q  = ch;

    for (char& c : num)
        if ('a' <= c && c <= 'z')
            c &= ~040; // toupper

    unsigned j = 0;
    for (char c : num) {
        if (! ('A' <= c && c <= 'Z'))
            break;
        j++;
    }
    if (j == 2) {
        country[0] = num[0];
        country[1] = num[1];
        country[2] = '\0';
    } else {
        country[0] = '\0';
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Patent, "Incorrectly formatted patent reference: %s", prbp->journal.c_str());
    }

    while (*q != '\0' && isdigit(*q) == 0)
        q++;
    if (*q == '\0') {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_Patent, "Missing date in patent reference: %s", prbp->journal.c_str());
        return (false);
    }

    CRef<CDate_std> std_date = get_full_date(q, true, source);
    if (! std_date || std_date.Empty()) {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_Patent, "Missing date in patent reference: %s", prbp->journal.c_str());
        return false;
    }

    pat.SetDate_issue().SetStd(*std_date);
    pat.SetCountry(country);

    if (prbp->title)
        pat.SetTitle(prbp->title);
    else
        pat.SetTitle("");

    pat.SetDoc_type("");
    if (! num.empty())
        pat.SetNumber(num);

    if (prbp->authors.NotEmpty())
        pat.SetAuthors(*prbp->authors);

    return (true);
}

/**********************************************************
 *
 *   static CitGenPtr GetCitGen(prbp):
 *
 *      Only for swiss-prot.
 *
 *                                              10-28-93
 *
 **********************************************************/
static bool GetCitGen(ParRefBlkPtr prbp, CCit_gen& cit_gen)
{
    bool is_set = false;
    if (! prbp->journal.empty()) {
        cit_gen.SetCit(prbp->journal);
        is_set = true;
    } else if (prbp->cit) {
        cit_gen.SetCit(prbp->cit);
        prbp->cit = nullptr;
        is_set    = true;
    }

    if (prbp->title && prbp->title[0] != '\0') {
        cit_gen.SetTitle(prbp->title);
        is_set = true;
    }

    if (prbp->authors.NotEmpty()) {
        cit_gen.SetAuthors(*prbp->authors);
        is_set = true;
    }

    return is_set;
}

/**********************************************************
 *
 *   static CitBookPtr GetCitLetThesis(prbp):
 *
 *      Only for swiss-prot.
 *
 *                                              10-28-93
 *
 **********************************************************/
static bool GetCitLetThesis(ParRefBlkPtr prbp, CCit_let& cit_let)
{
    CCit_book& book = cit_let.SetCit();

    if (prbp->title)
        SetCitTitle(book.SetTitle(), prbp->title);

    if (! GetImprintPtr(prbp, book.SetImp()))
        return false;

    if (prbp->authors.NotEmpty())
        book.SetAuthors(*prbp->authors);

    cit_let.SetType(CCit_let::eType_thesis);

    return true;
}

/**********************************************************
 *
 *   static CitArtPtr GetCitArticle(prbp):
 *
 *      For swiss-prot only (jta, because they all upper
 *   characters).
 *
 *                                              10-29-93
 *
 **********************************************************/
static bool GetCitArticle(ParRefBlkPtr prbp, CCit_art& article)
{
    if (prbp->title && prbp->title[0] != '\0')
        SetCitTitle(article.SetTitle(), prbp->title);

    if (prbp->authors.NotEmpty())
        article.SetAuthors(*prbp->authors);

    CCit_jour& journal = article.SetFrom().SetJournal();
    if (! GetImprintPtr(prbp, journal.SetImp()))
        journal.ResetImp();

    if (! prbp->journal.empty()) {
        CRef<CTitle::C_E> title(new CTitle::C_E);
        title->SetJta(prbp->journal);
        journal.SetTitle().Set().push_back(title);
    }

    if (prbp->agricola) {
        CRef<CArticleId> id(new CArticleId);

        id->SetOther().SetDb("AGRICOLA");
        id->SetOther().SetTag().SetStr(prbp->agricola);

        article.SetIds().Set().push_back(id);
    }

    if (prbp->doi) {
        CRef<CArticleId> id(new CArticleId);
        id->SetDoi().Set(prbp->doi);

        article.SetIds().Set().push_back(id);
    }

    return true;
}

/**********************************************************
 *
 *   static void FreeParRefBlkPtr(prbp):
 *
 *                                              10-28-93
 *
 **********************************************************/
static void FreeParRefBlkPtr(ParRefBlkPtr prbp)
{
    if (prbp->doi)
        MemFree(prbp->doi);
    if (prbp->agricola)
        MemFree(prbp->agricola);
    if (prbp->title)
        MemFree(prbp->title);
    if (prbp->year)
        MemFree(prbp->year);
    if (prbp->country)
        MemFree(prbp->country);

    delete prbp;
}

/**********************************************************/
static void DisrootImprint(CCit_sub& sub)
{
    if (! sub.IsSetImp())
        return;

    if (! sub.IsSetDate() && sub.GetImp().IsSetDate())
        sub.SetDate(sub.SetImp().SetDate());

    if (sub.IsSetAuthors()) {
        if (! sub.GetAuthors().IsSetAffil() && sub.GetImp().IsSetPub())
            sub.SetAuthors().SetAffil(sub.SetImp().SetPub());
    }

    sub.ResetImp();
}

/**********************************************************
 *
 *   static PubdescPtr GetPubRef(prbp, source):
 *
 *      Assume prbp only contains one reference data.
 *
 **********************************************************/
static CRef<CPubdesc> GetPubRef(ParRefBlkPtr prbp, Parser::ESource source)
{
    CRef<CPubdesc> ret;

    const Char* msg;

    if (! prbp)
        return ret;

    ret.Reset(new CPubdesc);
    if (prbp->refnum > 0) {
        CRef<CPub> pub(new CPub);
        pub->SetGen().SetSerial_number(prbp->refnum);
        ret->SetPub().Set().push_back(pub);
    }

    if (prbp->muid > 0) {
        CRef<CPub> pub(new CPub);
        pub->SetMuid(ENTREZ_ID_FROM(int, prbp->muid));
        ret->SetPub().Set().push_back(pub);
    }

    if (prbp->pmid > ZERO_ENTREZ_ID) {
        CRef<CPub> pub(new CPub);
        pub->SetPmid().Set(prbp->pmid);
        ret->SetPub().Set().push_back(pub);
    }

    msg = nullptr;
    CRef<CPub> pub(new CPub);
    bool       is_set = false;

    if (prbp->reftype == ParFlat_ReftypeNoParse) {
        is_set = GetCitGen(prbp, pub->SetGen());
    } else if (prbp->reftype == ParFlat_ReftypeThesis) {
        is_set = GetCitLetThesis(prbp, pub->SetMan());
        if (! is_set)
            msg = "Thesis format error (CitGen created)";
    } else if (prbp->reftype == ParFlat_ReftypeArticle) {
        is_set = GetCitArticle(prbp, pub->SetArticle());
        if (! is_set)
            msg = "Article format error (CitGen created)";
    } else if (prbp->reftype == ParFlat_ReftypeSubmit) {
        is_set = GetCitSubmit(prbp, pub->SetSub());
        if (is_set)
            DisrootImprint(pub->SetSub());
        else
            msg = "Submission format error (CitGen created)";
    } else if (prbp->reftype == ParFlat_ReftypeBook) {
        is_set = GetCitBook(prbp, pub->SetArticle());
        if (! is_set) {
            msg           = "Book format error (CitGen created)";
            prbp->reftype = ParFlat_ReftypeNoParse;
        }
    } else if (prbp->reftype == ParFlat_ReftypePatent) {
        is_set = GetCitPatent(prbp, source, pub->SetPatent());
        if (! is_set)
            msg = "Patent format error (cit-gen created)";
    } else if (prbp->reftype == ParFlat_ReftypeUnpub ||
               prbp->reftype == ParFlat_ReftypeIgnore) {
        is_set = GetCitGen(prbp, pub->SetGen());
    }

    if (msg) {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse, "%s: %s", msg, prbp->journal.c_str());
        is_set = GetCitGen(prbp, pub->SetGen());
    }

    if (is_set)
        ret->SetPub().Set().push_back(pub);

    if (! prbp->comment.empty())
        ret->SetComment(prbp->comment);

    return ret;
}

/**********************************************************/
CRef<CPubdesc> sp_refs(ParserPtr pp, DataBlkPtr dbp, Int4 col_data)
{
    ParRefBlkPtr prbp = SprotRefString(pp, dbp, col_data);

    if (! prbp->title)
        prbp->title = StringSave("");

    CRef<CPubdesc> desc = GetPubRef(prbp, pp->source);
    FreeParRefBlkPtr(prbp);

    return desc;
}

END_NCBI_SCOPE
