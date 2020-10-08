/* ftamed.c
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
 * File Name:  ftamed.c
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 * -----------------
 *      MedArch lookup and post-processing utilities.
 * Mostly the same as medutil.c written by James Ostell.
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/mla/mla_client.hpp>
#include <objects/mla/Title_msg.hpp>
#include <objects/mla/Title_msg_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/ArticleIdSet.hpp>
#include <objects/biblio/ArticleId.hpp>
#include <objects/general/Dbtag.hpp>
#include <corelib/ncbi_url.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include "index.h"

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "utilref.h"
#include "ftamed.h"
#include "xmlmisc.h"
#include "ref.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "ftamed.cpp"

BEGIN_NCBI_SCOPE

// error definitions from C-toolkit (mdrcherr.h)
#define ERR_REFERENCE  1,0
#define ERR_REFERENCE_MuidNotFound  1,1
#define ERR_REFERENCE_SuccessfulMuidLookup  1,2
#define ERR_REFERENCE_OldInPress  1,3
#define ERR_REFERENCE_No_reference  1,4
#define ERR_REFERENCE_Multiple_ref  1,5
#define ERR_REFERENCE_Multiple_muid  1,6
#define ERR_REFERENCE_MedlineMatchIgnored  1,7
#define ERR_REFERENCE_MuidMissmatch  1,8
#define ERR_REFERENCE_NoConsortAuthors  1,9
#define ERR_REFERENCE_DiffConsortAuthors  1,10
#define ERR_REFERENCE_PmidMissmatch  1,11
#define ERR_REFERENCE_Multiple_pmid  1,12
#define ERR_REFERENCE_FailedToGetPub  1,13
#define ERR_REFERENCE_MedArchMatchIgnored  1,14
#define ERR_REFERENCE_SuccessfulPmidLookup  1,15
#define ERR_REFERENCE_PmidNotFound  1,16
#define ERR_REFERENCE_NoPmidJournalNotInPubMed  1,17
#define ERR_REFERENCE_PmidNotFoundInPress  1,18
#define ERR_REFERENCE_NoPmidJournalNotInPubMedInPress  1,19
#define ERR_PRINT  2,0
#define ERR_PRINT_Failed  2,1


using namespace ncbi;
using namespace objects;

static char *this_module = (char *) "medarch";
#ifdef THIS_MODULE
#undef THIS_MODULE
#endif

#define THIS_MODULE this_module

static CMLAClient mlaclient;

USING_SCOPE(objects);

/**********************************************************/
bool MedArchInit()
{
    CMla_back i;

    try
    {
        mlaclient.AskInit(&i);
    }
    catch(exception &)
    {
        return false;
    }

    return i.IsInit();
}

/**********************************************************/
void MedArchFini()
{
    try
    {
        mlaclient.AskFini();
    }
    catch(exception &)
    {
    }
}

/**********************************************************/
static void MUGetStringCallback(XmlObjPtr xop, XmlObjPtr parent,
                                Int2 level, void* userdata)
{
    char** strp;

    if(xop == NULL || userdata == NULL)
        return;

    strp = (char**) userdata;

    if (!StringHasNoText(xop->contents))
        *strp = StringSave(xop->contents);
}

/**********************************************************/
static void MUGetStringSetCallback(XmlObjPtr xop, XmlObjPtr parent,
                                   Int2 level, void* userdata)
{
    ValNodeBlockPtr vnbp;

    if(xop == NULL || userdata == NULL)
        return;

    vnbp = (ValNodeBlockPtr) userdata;
    if (!StringHasNoText(xop->contents))
        ValNodeCopyStrEx(&(vnbp->head), &(vnbp->tail), 0, xop->contents);
}

/**********************************************************/
static bool MULooksLikeISSN(char* str)
{
    Char ch;
    Int2 i;

    if (StringHasNoText(str) || StringLen(str) != 9 || str[4] != '-')
        return false;

    for(i = 0; i < 9; i++)
    {
        ch = str[i];
        if(IS_DIGIT(ch) != 0 || (ch == '-' && i == 4) || (ch == 'X' && i == 8))
            continue;
        return false;
    }

    return true;
}

const unsigned int DEFAULT_HTTP_TIMEOUT = 30;

static string fetch_url(const string& url) {
    string s;
    char buff[1024];
    CConn_HttpStream http(url);
    while (!http.fail()) {
        http.read(buff, 1024);
        s.append(buff, http.gcount());
    }
    return s;
}


static char* StringNCpy_0(char* to, const char* from, size_t max)
{
    if (!to || !max) {
        return to;
    }
    if (max > 0) {
        to[0] = '\0';
    }
    if (from) {
        StringNCat(to, from, max - 1);
    }
    return to;
}


char* TrimSpacesAroundString(char* str)
{
    unsigned char ch;
    char* dst;
    char* ptr;

    if (str != NULL && str[0] != '\0') {
        dst = str;
        ptr = str;
        ch = *ptr;
        while (ch != '\0' && ch <= ' ') {
            ptr++;
            ch = *ptr;
        }
        while (ch != '\0') {
            *dst = ch;
            dst++;
            ptr++;
            ch = *ptr;
        }
        *dst = '\0';
        dst = NULL;
        ptr = str;
        ch = *ptr;
        while (ch != '\0') {
            if (ch > ' ') {
                dst = NULL;
            }
            else if (dst == NULL) {
                dst = ptr;
            }
            ptr++;
            ch = *ptr;
        }
        if (dst != NULL) {
            *dst = '\0';
        }
    }
    return str;
}


static char* CompressSpaces(char* str)
{
    char ch;
    char* dst;
    char last;
    char* ptr;

    if (str != NULL && str[0] != '\0') {
        dst = str;
        ptr = str;
        ch = *ptr;
        while (ch != '\0' && ch <= ' ') {
            ptr++;
            ch = *ptr;
        }
        while (ch != '\0') {
            *dst = ch;
            dst++;
            ptr++;
            last = ch;
            ch = *ptr;
            if (ch != '\0' && ch < ' ') {
                *ptr = ' ';
                ch = *ptr;
            }
            while (ch != '\0' && last <= ' ' && ch <= ' ') {
                ptr++;
                ch = *ptr;
            }
        }
        *dst = '\0';
        dst = NULL;
        ptr = str;
        ch = *ptr;
        while (ch != '\0') {
            if (ch != ' ') {
                dst = NULL;
            }
            else if (dst == NULL) {
                dst = ptr;
            }
            ptr++;
            ch = *ptr;
        }
        if (dst != NULL) {
            *dst = '\0';
        }
    }
    return str;
}


/**********************************************************/
static bool MUIsJournalIndexed(const Char* journal)
{
    ValNodeBlock blk;
    XmlObjPtr    xop;
    char*      count = NULL;
    char*      jids = NULL;
    char*      ptr;
    char*      status = NULL;
    bool         rsult = false;
    Char         ch;
    Char         title [512];

    if (StringHasNoText(journal))
        return false;

    StringNCpy_0(title, journal, sizeof(title));

    ptr = title;
    ch = *ptr;
    while(ch != '\0')
    {
        if(ch == '(' || ch == ')' || ch == '.')
            *ptr = ' ';
        ptr++;
        ch = *ptr;
    }
    TrimSpacesAroundString(title);
    CompressSpaces(title);

    std::string str;

    // TODO: Probably 'CFetchURL::Fetch' should be changed by higher level procedures (CHydraSearch ?)
    CDefaultUrlEncoder url_encoder;
    STimeout timeout = { DEFAULT_HTTP_TIMEOUT, 0 };

    const std::string base_url = "https://eutils.ncbi.nlm.nih.gov/entrez/eutils";
    const std::string nlmcatalog_url = base_url + "/esearch.fcgi?db=nlmcatalog&retmax=200&term=" + url_encoder.EncodeFragment(title);
    str = fetch_url(nlmcatalog_url + "%5Bissn%5D");

    if(MULooksLikeISSN(title))
        str = fetch_url(nlmcatalog_url + "%5Bissn%5D");
    if (str.empty())
        str = fetch_url(nlmcatalog_url + "%5Bmulti%5D+AND+ncbijournals%5Bsb%5D");
    if (str.empty())
        return false;

    xop = ParseXmlString(str.c_str());
    if(xop == NULL)
        return false;

    blk.head = NULL;
    blk.tail = NULL;
    VisitXmlNodes(xop, (void*) &blk, MUGetStringSetCallback, (char *) "Id",
                  NULL, NULL, NULL, 0);
    VisitXmlNodes(xop, (void*) &count, MUGetStringCallback, (char *) "Count",
                  (char*) "eSearchResult", NULL, NULL, 0);

    xop = FreeXmlObject(xop);

    if(StringCmp(count, "0") == 0 || blk.head == NULL)
    {
        count = (char *) MemFree(count);

        /* if [multi] failed, try [jour] Microbiology (Reading, Engl.)
         * is indexed in [multi] as microbiology reading, england
         * and in [jour] as microbiology reading, engl
         * microbiology reading, england
         */
        str = fetch_url(nlmcatalog_url + "%5Bjour%5D");

        if (str.empty())
            return false;

        xop = ParseXmlString(str.c_str());
        if(xop == NULL)
            return false;

        blk.head = NULL;
        blk.tail = NULL;
        VisitXmlNodes(xop, (void*) &blk, MUGetStringSetCallback,
                      (char *) "Id", NULL, NULL, NULL, 0);
        VisitXmlNodes(xop, (void*) &count, MUGetStringCallback,
                      (char *) "Count", (char *) "eSearchResult",
                      NULL, NULL, 0);

        xop = FreeXmlObject(xop);
    }

    count = (char *) MemFree(count);

    if(blk.head == NULL)
        return false;

    /* if more than one candidate, bail
     */
    if(blk.head->next != NULL)
    {
        ValNodeFreeData(blk.head);
        return false;
    }

    jids = ValNodeMergeStrsEx(blk.head, (char *) ",");
    ValNodeFreeData(blk.head);

    if(jids == NULL)
        return false;

    str = fetch_url(base_url + "/esummary.fcgi?&db=nlmcatalog&retmax=200&version=2.0&id=" + url_encoder.EncodeFragment(jids));

    MemFree(jids);
    if (str.empty())
        return false;

    xop = ParseXmlString(str.c_str());
    if(xop == NULL)
        return false;

    VisitXmlNodes(xop, (void*) &status, MUGetStringCallback,
                  (char *) "CurrentIndexingStatus", NULL, NULL, NULL, 0);

    xop = FreeXmlObject(xop);

    if(StringCmp(status, "Y") == 0)
        rsult = true;

    MemFree(status);

    return(rsult);
}

/**********************************************************/
static void print_pub(const objects::CCit_art& cit_art, bool found, bool auth, TEntrezId muid)
{
    const Char*    s_title = NULL;

    const Char*    page = NULL;
    const Char*    vol = NULL;

    const Char*    last = NULL;
    const Char*    first = NULL;

    Int2       year = 0;

    first = "";
    last = "";
    if (!cit_art.IsSetAuthors())
        ErrPostEx(SEV_WARNING, ERR_PRINT_Failed, "Authors NULL");
    else
    {
        if (!cit_art.GetAuthors().IsSetNames())
            ErrPostEx(SEV_WARNING, ERR_PRINT_Failed, "Authors NULL");
        else
        {
            /* warning! processing only the first of the author name valnodes
             * should loop through them all in case first is uninformative
             */
            if (cit_art.GetAuthors().GetNames().IsStd())
            {
                const objects::CAuthor& first_author = *(*cit_art.GetAuthors().GetNames().GetStd().begin());

                if (first_author.IsSetName())
                {
                    if (first_author.GetName().IsName())
                    {
                        const objects::CName_std& namestd = first_author.GetName().GetName();
                        if (namestd.IsSetLast())
                            last = namestd.GetLast().c_str();
                        if (namestd.IsSetInitials())
                            first = namestd.GetInitials().c_str();
                    }
                    else if (first_author.GetName().IsConsortium())
                        last = first_author.GetName().GetConsortium().c_str();
                }
            }
            else
            {
                first = "";
                last = cit_art.GetAuthors().GetNames().GetStr().begin()->c_str();
            }
        }
    }

    const objects::CImprint* imprint = nullptr;
    const objects::CTitle* title = nullptr;

    if (cit_art.IsSetFrom())
    {
        if (cit_art.GetFrom().IsJournal())
        {
            const objects::CCit_jour& journal = cit_art.GetFrom().GetJournal();

            if (journal.IsSetTitle())
                title = &journal.GetTitle();

            if (journal.IsSetImp())
                imprint = &journal.GetImp();
        }
        else if (cit_art.GetFrom().IsBook())
        {
            const objects::CCit_book& book = cit_art.GetFrom().GetBook();

            if (book.IsSetTitle())
                title = &book.GetTitle();

            if (book.IsSetImp())
                imprint = &book.GetImp();
        }
    }

    if (title != nullptr && title->IsSet() && !title->Get().empty())
    {
        const objects::CTitle::C_E& first_title = *title->Get().front();
        const std::string& title_str = title->GetTitle(first_title.Which());

        if (!title_str.empty())
            s_title = title_str.c_str();
    }

    if(s_title == NULL)
        s_title = "journal unknown";

    bool in_press = false;
    if (imprint != nullptr)
    {
        vol = imprint->IsSetVolume() ? imprint->GetVolume().c_str() : NULL;
        page = imprint->IsSetPages() ? imprint->GetPages().c_str() : NULL;

        if (imprint->IsSetDate())
            year = imprint->GetDate().GetStd().GetYear();

        in_press = imprint->IsSetPrepub() && imprint->GetPrepub() == objects::CImprint::ePrepub_in_press;
    }

    if(page == NULL)
        page = "no page number";
    if(vol == NULL)
        vol = "no volume number";

    if(auth)
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_MedArchMatchIgnored,
                  "Too many author name differences: %ld|%s %s|%s|(%d)|%s|%s",
                  ENTREZ_ID_TO(long, muid), last, first, s_title, (int) year, vol, page);
        return;
    }

    if (in_press)
    {
        int cur_year = objects::CDate_std(CTime(CTime::eCurrent)).GetYear();

        if (year != 0 && cur_year - year > 2)
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_OldInPress,
                      "encountered in-press article more than 2 years old: %s %s|%s|(%d)|%s|%s",
                      last, first, s_title, (int) year, vol, page);
    }

    if (found)
    {
        ErrPostEx(SEV_INFO, ERR_REFERENCE_SuccessfulPmidLookup,
                  "%ld|%s %s|%s|(%d)|%s|%s", ENTREZ_ID_TO(long, muid), last, first,
                  s_title, (int)year, vol, page);
    }
    else if (MUIsJournalIndexed(s_title))
    {
        if(muid == ZERO_ENTREZ_ID)
        {
            if (in_press)
                ErrPostEx(SEV_WARNING, ERR_REFERENCE_PmidNotFoundInPress,
                          "%s %s|%s|(%d)|%s|%s", last, first, s_title,
                          (int) year, vol, page);
            else
                ErrPostEx(SEV_WARNING, ERR_REFERENCE_PmidNotFound,
                          "%s %s|%s|(%d)|%s|%s", last, first, s_title,
                          (int) year, vol, page);
        }
        else
        {
            if (in_press)
                ErrPostEx(SEV_WARNING, ERR_REFERENCE_PmidNotFoundInPress,
                          ">>%ld<<|%s %s|%s|(%d)|%s|%s", ENTREZ_ID_TO(long, muid), last,
                          first, s_title, (int) year, vol, page);
            else
                ErrPostEx(SEV_WARNING, ERR_REFERENCE_PmidNotFound,
                          ">>%ld<<|%s %s|%s|(%d)|%s|%s", ENTREZ_ID_TO(long, muid), last,
                          first, s_title, (int) year, vol, page);
        }
    }
    else
    {
        if(muid == ZERO_ENTREZ_ID)
        {
            if (in_press)
                ErrPostEx(SEV_WARNING, ERR_REFERENCE_NoPmidJournalNotInPubMedInPress,
                          "%s %s|%s|(%d)|%s|%s", last, first, s_title,
                          (int) year, vol, page);
            else
                ErrPostEx(SEV_WARNING, ERR_REFERENCE_NoPmidJournalNotInPubMed,
                          "%s %s|%s|(%d)|%s|%s", last, first, s_title,
                          (int) year, vol, page);
        }
        else
        {
            if (in_press)
                ErrPostEx(SEV_INFO, ERR_REFERENCE_NoPmidJournalNotInPubMedInPress,
                          ">>%ld<<|%s %s|%s|(%d)|%s|%s", ENTREZ_ID_TO(long, muid), last,
                          first, s_title, (int) year, vol, page);
            else
                ErrPostEx(SEV_INFO, ERR_REFERENCE_NoPmidJournalNotInPubMed,
                          ">>%ld<<|%s %s|%s|(%d)|%s|%s", ENTREZ_ID_TO(long, muid), last,
                          first, s_title, (int) year, vol, page);
        }
    }
}

/**********************************************************/
static bool ten_authors_compare(objects::CCit_art& cit_old, objects::CCit_art& cit_new)
{
    const Char* namesnew[10];
    Int4       numold;
    Int4       numnew;
    Int4       match;
    Int4       i;

    if (!cit_old.IsSetAuthors() || !cit_old.GetAuthors().IsSetNames())
        return true;

    if (!cit_new.IsSetAuthors() || !cit_new.GetAuthors().IsSetNames())
        return false;

    if (cit_old.GetAuthors().GetNames().Which() != cit_new.GetAuthors().GetNames().Which() ||
        cit_old.GetAuthors().GetNames().IsStd())
        return true;

    const objects::CAuth_list::C_Names& old_names = cit_old.GetAuthors().GetNames();
    const objects::CAuth_list::C_Names& new_names = cit_new.GetAuthors().GetNames();

    for(i = 0; i < 10; i++)
        namesnew[i] = NULL;

    numnew = 0;

    ITERATE(objects::CAuth_list::C_Names::TStr, name, old_names.GetStr())
    {
        if (!name->empty())
            numnew++;
    }

    numold = 0;

    ITERATE(objects::CAuth_list::C_Names::TStr, name, new_names.GetStr())
    {
        if (!name->empty())
            namesnew[numold++] = name->c_str();
    }

    match = 0;

    ITERATE(objects::CAuth_list::C_Names::TStr, name, old_names.GetStr())
    {
        if (name->empty())
            continue;

        for (i = 0; i < numnew; i++)
        {
            if(StringICmp(name->c_str(), namesnew[i]) == 0)
            {
                match++;
                break;
            }
        }
    }

    i = (numold < numnew) ? numold : numnew;
    if(i > 3 * match)
        return false;

    if(numold > 10)
    {
        cit_new.SetAuthors(cit_old.SetAuthors());
        cit_old.ResetAuthors();
    }
    return true;
}

/**********************************************************/
static Int2 extract_names(const objects::CAuth_list_Base::C_Names::TStd& names, objects::CAuth_list_Base::C_Names::TStr& extracted, size_t& len)
{
    len = 1;
    Int2 num = 0;
    ITERATE(objects::CAuth_list_Base::C_Names::TStd, name, names)
    {
        const objects::CAuthor& auth = *(*name);
        if (auth.IsSetName() && auth.GetName().IsName())
            num++;
        else if (auth.IsSetName() && auth.GetName().IsConsortium())
        {
            const std::string& cur_consortium = auth.GetName().GetConsortium();
            len += cur_consortium.size() + 2;

            if (names.empty())
            {
                extracted.push_back(cur_consortium);
                continue;
            }

            bool inserted = false;
            NON_CONST_ITERATE(objects::CAuth_list_Base::C_Names::TStr, cur, extracted)
            {
                if (StringICmp(cur_consortium.c_str(), cur->c_str()) <= 0)
                {
                    extracted.insert(cur, cur_consortium);
                    inserted = true;
                    break;
                }
            }

            if (!inserted)
                extracted.push_back(cur_consortium);
        }
    }

    return num;
}

/**********************************************************/
static Char* cat_names(const objects::CAuth_list_Base::C_Names::TStr& names, size_t len)
{
    if (names.empty())
        return NULL;

    char* buf = (char *)MemNew(len);
    buf[0] = '\0';

    ITERATE(objects::CAuth_list_Base::C_Names::TStr, cur, names)
    {
        if (buf[0] != '\0')
            StringCat(buf, "; ");
        StringCat(buf, cur->c_str());
    }

    return buf;
}

/**********************************************************/
static bool ten_authors(objects::CCit_art& cit, objects::CCit_art& cit_tmp)
{
    char*    oldbuf;
    char*    newbuf;
    const Char* mu[10];

    Int2       num;
    Int2       numnew;
    Int2       numtmp;
    Int2       i;
    Int2       match;
    bool       got_names;

    got_names = false;
    if(cit_tmp.IsSetAuthors() && cit_tmp.GetAuthors().IsSetNames() &&
       cit_tmp.GetAuthors().CanGetNames())
    {
        const CAuth_list::TNames &names = cit_tmp.GetAuthors().GetNames();
        if(names.Which() == CAuth_list::TNames::e_Std ||
           names.Which() == CAuth_list::TNames::e_Str ||
           names.Which() == CAuth_list::TNames::e_Ml)
            got_names = true;
    }

    if(got_names == false)
    {
        if(cit.IsSetAuthors())
        {
            cit_tmp.SetAuthors(cit.SetAuthors());
            cit.ResetAuthors();
        }
        return(true);
    }

    if (!cit.IsSetAuthors() || !cit.GetAuthors().IsSetNames() ||
        cit.GetAuthors().GetNames().Which() != cit_tmp.GetAuthors().GetNames().Which())
        return true;

    if (!cit.GetAuthors().GetNames().IsStd())
        return(ten_authors_compare(cit, cit_tmp));

    oldbuf = NULL;

    objects::CAuth_list_Base::C_Names::TStr oldcon;

    size_t oldlen = 0;
    num = extract_names(cit.GetAuthors().GetNames().GetStd(), oldcon, oldlen);
    oldbuf = cat_names(oldcon, oldlen);

    objects::CAuth_list_Base::C_Names::TStr newcon;

    size_t newlen = 0;
    numtmp = extract_names(cit_tmp.GetAuthors().GetNames().GetStd(), newcon, newlen);
    newbuf = cat_names(newcon, newlen);

    if (!oldcon.empty())
    {
        if (newcon.empty())
        {
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_NoConsortAuthors,
                      "Publication as returned by MedArch lacks consortium authors of the original publication: \"%s\".",
                      oldbuf);

            ITERATE(objects::CAuth_list_Base::C_Names::TStr, cur, oldcon)
            {
                CRef<objects::CAuthor> auth(new objects::CAuthor);
                auth->SetName().SetConsortium(*cur);

                cit_tmp.SetAuthors().SetNames().SetStd().push_front(auth);
            }
        }
        else
        {
            if(StringICmp(oldbuf, newbuf) != 0)
                ErrPostEx(SEV_WARNING, ERR_REFERENCE_DiffConsortAuthors,
                          "Consortium author names differ. Original is \"%s\". MedArch's is \"%s\".",
                          oldbuf, newbuf);
            MemFree(newbuf);
            newbuf = NULL;

            oldcon.clear();
            newcon.clear();
        }

        MemFree(oldbuf);
        if(num == 0)
            return true;
    }

    newcon.clear();

    if (newbuf != NULL)
        MemFree(newbuf);

    numnew = 0;
    ITERATE(objects::CAuth_list_Base::C_Names::TStd, name, cit_tmp.GetAuthors().GetNames().GetStd())
    {
        const objects::CAuthor& auth = *(*name);
        if (!auth.IsSetName() || !auth.GetName().IsName())
            continue;

        if (auth.GetName().GetName().IsSetLast())
        {
            mu[numnew++] = auth.GetName().GetName().GetLast().c_str();
            if (numnew == sizeof(mu) / sizeof(mu[0]))
                break;
        }
    }

    match = 0;
    ITERATE(objects::CAuth_list_Base::C_Names::TStd, name, cit.GetAuthors().GetNames().GetStd())
    {
        const objects::CAuthor& auth = *(*name);
        if (!auth.IsSetName() || !auth.GetName().IsName())
            continue;

        if (auth.GetName().GetName().IsSetLast())
        {
            for (i = 0; i < numnew; i++)
            {
                if (StringICmp(auth.GetName().GetName().GetLast().c_str(), mu[i]) == 0)
                {
                    match++;
                    break;
                }
            }
        }
    }

    i = (num < numnew) ? num : numnew;
    if(i > 3 * match)
        return false;

    if(numtmp == 0 || (num > 10 && numtmp < 12) || (num > 25 && numtmp < 27))
    {
        cit_tmp.SetAuthors(cit.SetAuthors());
        cit.ResetAuthors();
    }

    return true;
}

/**********************************************************/
static void MergeNonPubmedPubIds(objects::CCit_art& cit_new, objects::CCit_art& cit_old)
{
    if (!cit_old.IsSetIds())
        return;

    objects::CArticleIdSet& old_ids = cit_old.SetIds();

    for (objects::CArticleIdSet::Tdata::iterator cur = old_ids.Set().begin(); cur != old_ids.Set().end(); )
    {
        if (!(*cur)->IsDoi() && !(*cur)->IsOther())
        {
            ++cur;
            continue;
        }

        bool found = false;
        if (cit_new.IsSetIds())
        {
            ITERATE(objects::CArticleIdSet::Tdata, new_id, cit_new.GetIds().Get())
            {
                if ((*new_id)->Which() != (*cur)->Which())
                    continue;

                if ((*new_id)->IsDoi())
                {
                    found = true;
                    break;
                }

                if ((*new_id)->GetOther().GetDb() == (*cur)->GetOther().GetDb())
                {
                    found = true;
                    break;
                }
            }
        }

        if (found)
        {
            ++cur;
            continue;
        }

        cit_new.SetIds().Set().push_front(*cur);
        cur = old_ids.Set().erase(cur);
    }
}

/**********************************************************
 *
 *   MedlineToISO(tmp)
 *       converts a MEDLINE citation to ISO/GenBank style
 *
 **********************************************************/
static void MedlineToISO(objects::CCit_art& cit_art)
{
    bool     is_iso;

    objects::CAuth_list& auths = cit_art.SetAuthors();
    if (auths.IsSetNames())
    {
        if (auths.GetNames().IsMl())            /* ml style names */
        {
            objects::CAuth_list::C_Names& old_names = auths.SetNames();
            CRef<objects::CAuth_list::C_Names> new_names(new objects::CAuth_list::C_Names);

            ITERATE(objects::CAuth_list::C_Names::TMl, name, old_names.GetMl())
            {
                CRef<CAuthor> author = get_std_auth(name->c_str(), ML_REF);
                if (author.Empty())
                    continue;

                new_names->SetStd().push_back(author);
            }

            auths.ResetNames();
            auths.SetNames(*new_names);
        }
        else if (auths.GetNames().IsStd())       /* std style names */
        {
            NON_CONST_ITERATE(objects::CAuth_list::C_Names::TStd, auth, auths.SetNames().SetStd())
            {
                if (!(*auth)->IsSetName() || !(*auth)->GetName().IsMl())
                    continue;

                std::string cur_name = (*auth)->GetName().GetMl();
                GetNameStdFromMl((*auth)->SetName().SetName(), cur_name.c_str());
            }
        }
    }

    if (!cit_art.IsSetFrom() || !cit_art.GetFrom().IsJournal())
        return;

    /* from a journal - get iso_jta
     */
    objects::CCit_jour& journal = cit_art.SetFrom().SetJournal();

    is_iso = false;

    if (journal.IsSetTitle())
    {
        ITERATE(objects::CTitle::Tdata, title, journal.GetTitle().Get())
        {
            if ((*title)->IsIso_jta())      /* have it */
            {
                is_iso = true;
                break;
            }
        }
    }

    if (!is_iso)
    {
        if (journal.IsSetTitle())
        {
            objects::CTitle::C_E& first_title = *(*journal.SetTitle().Set().begin());
            
            const std::string& title_str = journal.GetTitle().GetTitle(first_title.Which());

            CRef<CTitle> title_new(new CTitle);
            CRef<CTitle::C_E> type_new(new CTitle::C_E);
            type_new->SetIso_jta(title_str);
            title_new->Set().push_back(type_new);
            
            CRef<CTitle_msg> msg_new(new CTitle_msg);
            msg_new->SetType(eTitle_type_iso_jta);
            msg_new->SetTitle(*title_new);

            CRef<CTitle_msg_list> msg_list_new;
            try
            {
                msg_list_new = mlaclient.AskGettitle(*msg_new);
            }
            catch(exception &)
            {
                msg_list_new = null;
            }

            if (msg_list_new != null && msg_list_new->IsSetTitles())
            {
                bool gotit = false;
                ITERATE(CTitle_msg_list::TTitles, item3, msg_list_new->GetTitles())
                {
                    const CTitle &cur_title = (*item3)->GetTitle();
                    ITERATE(CTitle::Tdata, item, cur_title.Get())
                    {
                        if((*item)->IsIso_jta())
                        {
                            gotit = true;
                            first_title.SetIso_jta((*item)->GetIso_jta());
                            break;
                        }
                    }
                    if (gotit)
                        break;
                }
            }
        }
    }

    if (journal.IsSetImp())
    {
        /* remove Eng language */
        if (journal.GetImp().IsSetLanguage() && journal.GetImp().GetLanguage() == "Eng")
            journal.SetImp().ResetLanguage();
    }
}

/**********************************************************
 *
 *   SplitMedlineEntry(mep)
 *      splits a medline entry into 2 pubs (1 muid, 1 Cit-art)
 *      converts Cit-art to ISO/GenBank style
 *      returns pointer to first (chained) pub
 *      deletes original medline entry
 *
 **********************************************************/
static void SplitMedlineEntry(objects::CPub_equiv::Tdata& medlines)
{
    if (medlines.size() != 1)
        return;

    objects::CPub& pub = *medlines.front();
    objects::CMedline_entry& medline = pub.SetMedline();
    if (!medline.IsSetCit() && medline.IsSetPmid() && medline.GetPmid() < ZERO_ENTREZ_ID)
        return;

    CRef<objects::CPub> pmid;
    if (medline.GetPmid() > ZERO_ENTREZ_ID)
    {
        pmid.Reset(new objects::CPub);
        pmid->SetPmid(medline.GetPmid());
    }

    CRef<objects::CPub> cit_art;
    if (medline.IsSetCit())
    {
        cit_art.Reset(new objects::CPub);
        cit_art->SetArticle(medline.SetCit());
        MedlineToISO(cit_art->SetArticle());
    }

    medlines.clear();

    if (pmid.NotEmpty())
        medlines.push_back(pmid);

    if (cit_art.NotEmpty())
        medlines.push_back(cit_art);
}

/**********************************************************/
CRef<objects::CCit_art> FetchPubPmId(Int4 pmid)
{
    CRef<objects::CCit_art> cit_art;
    if (pmid < 0)
        return cit_art;

    CRef<objects::CPub> pub;
    try
    {
        pub = mlaclient.AskGetpubpmid(CPubMedId(ENTREZ_ID_FROM(Int4, pmid)));
    }
    catch(exception &)
    {
        pub.Reset();
    }

    if (pub.Empty() || !pub->IsArticle())
        return cit_art;

    cit_art.Reset(new objects::CCit_art);
    cit_art->Assign(pub->GetArticle());
    MedlineToISO(*cit_art);

    return cit_art;
}

/**********************************************************
 *
 *   FixPub(pub, fpop)
 *       Tries to make any Pub into muid/cit-art
 *
 **********************************************************/
void FixPub(TPubList& pub_list, TFindPubOptions& findPubOptions)
{
    TEntrezId       pmid;

    pub_list.resize(1);
    objects::CPub& first_pub = *(*pub_list.begin());

    switch (first_pub.Which())
    {
        case objects::CPub::e_Medline :               /* medline, just split to pubequiv */
            pub_list.resize(1);
            SplitMedlineEntry(pub_list);
            break;

        case objects::CPub::e_Article:
            {
            objects::CCit_art& cit_art = first_pub.SetArticle();
            if (cit_art.IsSetFrom() && cit_art.GetFrom().IsBook())
                return;

            findPubOptions.lookups_attempted++;

            CRef<CPub> cpub(new CPub());
            cpub->SetArticle(cit_art);

            try
            {
                pmid = ENTREZ_ID_FROM(int, mlaclient.AskCitmatchpmid(*cpub));
            }
            catch (exception &)
            {
                pmid = ZERO_ENTREZ_ID;
            }

            if (pmid > ZERO_ENTREZ_ID)                /* matched it */
            {
                print_pub(cit_art, true, false, pmid);
                findPubOptions.lookups_succeeded++;
                if (findPubOptions.replace_cit)
                {
                    findPubOptions.fetches_attempted++;
                    CRef<objects::CCit_art> new_cit_art = FetchPubPmId(ENTREZ_ID_TO(int, pmid));

                    if (new_cit_art.NotEmpty())
                    {
                        if (ten_authors(cit_art, *new_cit_art))
                        {
                            findPubOptions.fetches_succeeded++;

                            if (findPubOptions.merge_ids)
                                MergeNonPubmedPubIds(*new_cit_art, cit_art);

                            first_pub.Reset();

                            CRef<objects::CPub> pub(new objects::CPub);
                            pub->SetArticle(*new_cit_art);
                            first_pub.SetEquiv().Set().push_back(pub);

                            pub.Reset(new objects::CPub);
                            pub->SetPmid().Set(pmid);
                            first_pub.SetEquiv().Set().push_back(pub);
                        }
                        else
                        {
                            print_pub(cit_art, false, true, pmid);
                            MedlineToISO(cit_art);
                        }
                    }
                }
                else
                {
                    print_pub(cit_art, false, false, pmid);
                    MedlineToISO(cit_art);
                }
            }
            }
            break;
        case objects::CPub::e_Equiv:
            FixPubEquiv(pub_list, findPubOptions);
            break;
        default:
            break;
    }
}

/**********************************************************/
static bool if_inpress_set(const objects::CCit_art& cit_art)
{
    if (!cit_art.IsSetFrom())
        return false;

    if (cit_art.GetFrom().IsJournal())
    {
        const objects::CCit_jour& journal = cit_art.GetFrom().GetJournal();
        if (journal.IsSetImp() && journal.GetImp().IsSetPrepub() && journal.GetImp().GetPrepub() == objects::CImprint::ePrepub_in_press)
            return true;
    }
    else if (cit_art.GetFrom().IsBook() || cit_art.GetFrom().IsProc())
    {
        const objects::CCit_book& book = cit_art.GetFrom().GetBook();
        if (book.IsSetImp() && book.GetImp().IsSetPrepub() && book.GetImp().GetPrepub() == objects::CImprint::ePrepub_in_press)
            return true;
    }
    return false;
}

/**********************************************************/
static bool is_in_press(const objects::CCit_art& cit_art)
{
    if (!cit_art.IsSetFrom() || !cit_art.GetFrom().IsJournal() || !cit_art.GetFrom().GetJournal().IsSetTitle())
        return true;

    const objects::CCit_jour& journal = cit_art.GetFrom().GetJournal();
    if (!journal.IsSetImp())
        return true;

    if (!journal.GetImp().IsSetVolume() || !journal.GetImp().IsSetPages() || !journal.GetImp().IsSetDate())
        return true;

    return false;
}

/**********************************************************/
static void PropagateInPress(bool inpress, objects::CCit_art& cit_art)
{
    if (!inpress)
        return;

    if(!is_in_press(cit_art) || !cit_art.IsSetFrom())
        return;

    if (cit_art.GetFrom().IsJournal())
    {
        if (cit_art.GetFrom().GetJournal().IsSetImp())
            cit_art.SetFrom().SetJournal().SetImp().SetPrepub(objects::CImprint::ePrepub_in_press);
    }
    else if (cit_art.GetFrom().IsBook() || cit_art.GetFrom().IsProc())
    {
        if (cit_art.GetFrom().GetBook().IsSetImp())
            cit_art.SetFrom().SetBook().SetImp().SetPrepub(objects::CImprint::ePrepub_in_press);
    }
}

/**********************************************************/
static void FixPubEquivAppendPmid(Int4 muid, objects::CPub_equiv::Tdata& pmids)
{
    TEntrezId oldpmid;
    TEntrezId newpmid;

    oldpmid = pmids.empty() ? ZERO_ENTREZ_ID : (*pmids.begin())->GetPmid();

    try
    {
        newpmid = mlaclient.AskUidtopmid(muid);
    }
    catch(exception &)
    {
        newpmid = INVALID_ENTREZ_ID;
    }

    if(oldpmid < ENTREZ_ID_CONST(1) && newpmid < ENTREZ_ID_CONST(1))
        return;

    if(oldpmid > ZERO_ENTREZ_ID && newpmid > ZERO_ENTREZ_ID && oldpmid != newpmid)
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_PmidMissmatch,
                  "OldPMID=%ld doesn't match lookup (%ld). Keeping lookup.",
                  ENTREZ_ID_TO(long, oldpmid), ENTREZ_ID_TO(long, newpmid));

    if (pmids.empty())
    {
        CRef<objects::CPub> pmid_pub(new objects::CPub);
        pmids.push_back(pmid_pub);
    }

    (*pmids.begin())->SetPmid().Set((newpmid > ZERO_ENTREZ_ID) ? newpmid : oldpmid);
}

/**********************************************************/
void FixPubEquiv(TPubList& pub_list, TFindPubOptions& findPubOptions)
{
    bool       inpress;
    TEntrezId  oldmuid = ZERO_ENTREZ_ID;
    TEntrezId  pmid = ZERO_ENTREZ_ID;
    TEntrezId  oldpmid = ZERO_ENTREZ_ID;

    objects::CPub_equiv::Tdata muids,
                                     pmids,
                                     medlines,
                                     others,
                                     cit_arts;

    NON_CONST_ITERATE(objects::CPub_equiv::Tdata, pub, pub_list)
    {
        if ((*pub)->IsMuid())
            muids.push_back(*pub);
        else if ((*pub)->IsPmid())
            pmids.push_back(*pub);
        else if ((*pub)->IsArticle())
        {
            if ((*pub)->GetArticle().GetFrom().IsBook())
                others.push_back(*pub);
            else
                cit_arts.push_back(*pub);
        }
        else if ((*pub)->IsMedline())
            medlines.push_back(*pub);
        else
            others.push_back(*pub);
    }

    pub_list.clear();

    if ((!muids.empty() || !pmids.empty()) &&  /* got a muid or pmid */
        !findPubOptions.always_look)
    {
        pub_list.splice(pub_list.end(), cit_arts);
        pub_list.splice(pub_list.end(), muids);
        pub_list.splice(pub_list.end(), pmids);
        pub_list.splice(pub_list.end(), medlines);
        pub_list.splice(pub_list.end(), others);
        return;                   /* no changes */
    }

    pub_list.splice(pub_list.end(), others); /* put back others */

    if (!medlines.empty())            /* have a medline entry,
                                         take it first */
    {
        if (medlines.size() > 1)
        {
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_Multiple_ref,
                      "More than one Medline entry in Pub-equiv");
            medlines.resize(1);
        }

        SplitMedlineEntry(medlines);
        pub_list.splice(pub_list.end(), medlines);
    }

    if (!pmids.empty())                 /* have a pmid */
    {
        if (pmids.size() > 1)       /* more than one, just take
                                       the first */
        {
            oldpmid = (*pmids.begin())->GetPmid();
            ITERATE(objects::CPub_equiv::Tdata, pub, pmids)
            {
                if ((*pub)->GetPmid() != oldpmid)
                {
                    ErrPostEx(SEV_WARNING, ERR_REFERENCE_Multiple_pmid,
                              "Two different pmids in Pub-equiv [%ld] [%ld]",
                              ENTREZ_ID_TO(long, oldpmid), ENTREZ_ID_TO(long, (*pub)->GetPmid().Get()));
                }
            }
            pmids.resize(1);
        }
    }

    if (!muids.empty())                 /* have an muid */
    {
        if (muids.size() > 1)       /* more than one, just take
                                    the first */
        {
            oldmuid = (*muids.begin())->GetMuid();
            ITERATE(objects::CPub_equiv::Tdata, pub, muids)
            {
                if ((*pub)->GetMuid() != oldmuid)
                {
                    ErrPostEx(SEV_WARNING, ERR_REFERENCE_Multiple_muid,
                              "Two different muids in Pub-equiv [%ld] [%ld]",
                              ENTREZ_ID_TO(long, oldmuid), ENTREZ_ID_TO(long, (*pub)->GetMuid()));
                }
            }
            muids.resize(1);
        }
    }

    if (!cit_arts.empty())
    {
        if (cit_arts.size() > 1)     /* ditch extras */
        {
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_Multiple_ref,
                      "More than one Cit-art in Pub-equiv");
            cit_arts.resize(1);
        }

        objects::CCit_art* cit_art = &cit_arts.front()->SetArticle();
        inpress = if_inpress_set(*cit_art);
        findPubOptions.lookups_attempted++;

        CRef<objects::CPub> new_pub(new objects::CPub);
        new_pub->SetArticle(*cit_art);

        pmid = ZERO_ENTREZ_ID;
        try
        {
            pmid = ENTREZ_ID_FROM(int, mlaclient.AskCitmatchpmid(*new_pub));
        }
        catch (exception &)
        {
            pmid = ZERO_ENTREZ_ID;
        }

        if (pmid != ZERO_ENTREZ_ID)                   /* success */
        {
            print_pub(*cit_art, true, false, pmid);
            findPubOptions.lookups_succeeded++;
            if (oldpmid > ZERO_ENTREZ_ID && oldpmid != pmid)  /* already had a pmid */
                ErrPostEx(SEV_ERROR, ERR_REFERENCE_PmidMissmatch,
                "OldPMID=%ld doesn't match lookup (%ld). Keeping lookup.",
                ENTREZ_ID_TO(long, oldpmid), ENTREZ_ID_TO(long, pmid));

            if (findPubOptions.replace_cit)
            {
                findPubOptions.fetches_attempted++;

                CRef<objects::CCit_art> new_cit_art = FetchPubPmId(ENTREZ_ID_TO(Int4, pmid));

                if (new_cit_art.NotEmpty())
                {
                    findPubOptions.fetches_succeeded++;

                    if (ten_authors(*cit_art, *new_cit_art))
                    {
                        if (pmids.empty())
                        {
                            CRef<objects::CPub> pmid_pub(new objects::CPub);
                            pmids.push_back(pmid_pub);
                        }

                        (*pmids.begin())->SetPmid().Set(pmid);
                        pub_list.splice(pub_list.end(), pmids);
                        
                        CRef<objects::CPub> cit_pub(new objects::CPub);
                        cit_pub->SetArticle(*new_cit_art);
                        pub_list.push_back(cit_pub);

                        if (findPubOptions.merge_ids)
                            MergeNonPubmedPubIds(cit_pub->SetArticle(), *cit_art);

                        cit_arts.clear();
                        cit_arts.push_back(cit_pub);
                        cit_art = new_cit_art;
                    }
                    else
                    {
                        pmids.clear();

                        print_pub(*cit_art, false, true, pmid);
                        pub_list.splice(pub_list.end(), cit_arts);
                    }
                }
                else
                {
                    ErrPostEx(SEV_ERROR, ERR_REFERENCE_FailedToGetPub,
                              "Failed to get pub from MedArch server for pmid = %ld. Input one is preserved.",
                              pmid);

                    if (pmids.empty())
                    {
                        CRef<objects::CPub> pmid_pub(new objects::CPub);
                        pmids.push_back(pmid_pub);
                    }

                    (*pmids.begin())->SetPmid().Set(pmid);
                    pub_list.splice(pub_list.end(), pmids);

                    MedlineToISO(*cit_art);

                    pub_list.splice(pub_list.end(), cit_arts);
                }
            }
            else
            {
                if (pmids.empty())
                {
                    CRef<objects::CPub> pmid_pub(new objects::CPub);
                    pmids.push_back(pmid_pub);
                }

                (*pmids.begin())->SetPmid().Set(pmid);
                pub_list.splice(pub_list.end(), pmids);

                MedlineToISO(*cit_art);

                pub_list.splice(pub_list.end(), cit_arts);
            }

            muids.clear();
            PropagateInPress(inpress, *cit_art);
            return;
        }

        print_pub(*cit_art, false, false, oldpmid);
        PropagateInPress(inpress, *cit_art);
        pub_list.splice(pub_list.end(), cit_arts);

        muids.clear();             /* ditch the mismatched muid */
        pmids.clear();             /* ditch the mismatched pmid */

        return;
    }

    if (oldpmid != ZERO_ENTREZ_ID)                    /* have a pmid but no cit-art */
    {
        findPubOptions.fetches_attempted++;

        CRef<objects::CCit_art> new_cit_art = FetchPubPmId(ENTREZ_ID_TO(int, oldpmid));

        if (new_cit_art.NotEmpty())
        {
            findPubOptions.fetches_succeeded++;
            if (findPubOptions.replace_cit)
            {
                MedlineToISO(*new_cit_art);
                CRef<objects::CPub> cit_pub(new objects::CPub);
                cit_pub->SetArticle(*new_cit_art);
                pub_list.push_back(cit_pub);
            }

            pub_list.splice(pub_list.end(), pmids);
            return;
        }
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_No_reference,
                  "Cant find article for pmid [%ld]", ENTREZ_ID_TO(long, oldpmid));
    }

    if (oldpmid > ZERO_ENTREZ_ID)
        pub_list.splice(pub_list.end(), pmids);
    else if (oldmuid > ZERO_ENTREZ_ID)
    {
        pub_list.splice(pub_list.end(), muids);
        FixPubEquivAppendPmid(ENTREZ_ID_TO(int, oldmuid), pmids);
        pub_list.splice(pub_list.end(), pmids);
    }
}

END_NCBI_SCOPE
