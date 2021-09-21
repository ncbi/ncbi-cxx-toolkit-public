/* ftanet.cpp
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
 * File Name:  ftanet.cpp
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 * -----------------
 *      Functions for real working with the servers and network.
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objects/seqfeat/Org_ref.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/biblio/ArticleIdSet.hpp>
#include <objects/biblio/ArticleId.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/edit/pub_fix.hpp>

#include <dbapi/driver/drivers.hpp>

#include "index.h"

#include <objtools/flatfile/flatdefn.h>
#include <objtools/flatfile/flatfile_parser.hpp>
#include <corelib/ncbi_message.hpp>

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "ftamed.h"
#include "utilfun.h"
#include "ref.h"
#include "flatfile_message_reporter.hpp"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "ftanet.cpp"

#define HEALTHY_ACC "U12345"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static KwordBlk PubStatus[] = {
    {"Publication Status: Available-Online prior to print", 51},
    {"Publication Status : Available-Online prior to print", 52},
    {"Publication_Status: Available-Online prior to print", 51},
    {"Publication_Status : Available-Online prior to print", 52},
    {"Publication-Status: Available-Online prior to print", 51},
    {"Publication-Status : Available-Online prior to print", 52},
    {"Publication Status: Online-Only", 31},
    {"Publication Status : Online-Only", 32},
    {"Publication_Status: Online-Only", 31},
    {"Publication_Status : Online-Only", 32},
    {"Publication-Status: Online-Only", 31},
    {"Publication-Status : Online-Only", 32},
    {"Publication Status: Available-Online", 36},
    {"Publication Status : Available-Online", 37},
    {"Publication_Status: Available-Online", 36},
    {"Publication_Status : Available-Online", 37},
    {"Publication-Status: Available-Online", 36},
    {"Publication-Status : Available-Online", 37},
    {NULL, 0}
};

/**********************************************************/
static char* fta_strip_pub_comment(char* comment, KwordBlkPtr kbp)
{
    char* p;
    char* q;

    ShrinkSpaces(comment);
    for(; kbp->str != NULL; kbp++)
    {
        for(;;)
        {
            p = StringIStr(comment, kbp->str);
            if(p == NULL)
                break;
            for(q = p + kbp->len; *q == ' ' || *q == ';';)
                q++;
            fta_StringCpy(p, q);
        }
    }

    ShrinkSpaces(comment);
    p = (*comment == '\0') ? NULL : StringSave(comment);
    MemFree(comment);

    if(p != NULL && (StringNICmp(p, "Publication Status", 18) == 0 ||
                     StringNICmp(p, "Publication_Status", 18) == 0 ||
                     StringNICmp(p, "Publication-Status", 18) == 0))
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_UnusualPubStatus,
                  "An unusual Publication Status comment exists for this record: \"%s\". If it is a new variant of the special comments used to indicate ahead-of-print or online-only articles, then the comment must be added to the appropriate table of the parser.",
                  p);

    return(p);
}

/**********************************************************/
static void fta_fix_last_initials(objects::CName_std &namestd,
                                  bool initials)
{
    char *str;
    char *p;

    if(initials)
    {
        if(!namestd.IsSetInitials())
            return;
        str = (char *) namestd.GetInitials().c_str();
    }
    else
    {
        if(!namestd.IsSetLast())
            return;
        str = (char *) namestd.GetLast().c_str();
    }

    size_t i = strlen(str);
    if(i > 5)
    {
        p = &str[i-5];
        if((*p == ' ' || *p == '.') && !strcmp(p + 1, "III."))
        {
            namestd.SetSuffix("III");
            if(*p == '.')
                p++;
            *p = '\0';
            if(initials)
                namestd.SetInitials(str);
            else
                namestd.SetLast(str);
            i = 0;
        }
    }
    if(i > 4)
    {
        p = &str[i-4];
        if((*p == ' ' || *p == '.') &&
           (!strcmp(p + 1, "III") || !strcmp(p + 1, "2nd") ||
            !strcmp(p + 1, "Jr.") || !strcmp(p + 1, "IV.")))
        {
            if(!strcmp(p + 1, "III"))
                namestd.SetSuffix("III");
            else if(!strcmp(p + 1, "2nd"))
                namestd.SetSuffix("II");
            else if(!strcmp(p + 1, "Jr."))
                namestd.SetSuffix("Jr.");
            else
                namestd.SetSuffix("IV");
            if(*p == '.')
                p++;
            *p = '\0';
            if(initials)
                namestd.SetInitials(str);
            else
                namestd.SetLast(str);
            i = 0;
        }
    }
    if(i > 3)
    {
        p = &str[i-3];
        if((*p == ' ' || *p == '.') &&
           (!strcmp(p + 1, "Jr") || !strcmp(p + 1, "IV") ||
            !strcmp(p + 1, "II")))
        {
            if(!strcmp(p + 1, "Jr"))
                namestd.SetSuffix("Jr.");
            else if(!strcmp(p + 1, "IV"))
                namestd.SetSuffix("IV");
            else
                namestd.SetSuffix("II");
            if(*p == '.')
                p++;
            *p = '\0';
            if(initials)
                namestd.SetInitials(str);
            else
                namestd.SetLast(str);
            i = 0;
        }
    }
}

/**********************************************************/
static void fta_fix_affil(TPubList &pub_list, Parser::ESource source)
{
    bool got_pmid = false;

    NON_CONST_ITERATE(objects::CPub_equiv::Tdata, pub, pub_list)
    {
        if(!(*pub)->IsPmid())
            continue;
        got_pmid = true;
        break;
    }

    NON_CONST_ITERATE(objects::CPub_equiv::Tdata, pub, pub_list)
    {
        objects::CAuth_list *authors;
        if((*pub)->IsArticle())
        {
            objects::CCit_art &art = (*pub)->SetArticle();
            if(!art.IsSetAuthors() || !art.CanGetAuthors())
                continue;

            authors = &art.SetAuthors();
        }
        else if((*pub)->IsSub())
        {
            objects::CCit_sub &sub = (*pub)->SetSub();
            if(!sub.IsSetAuthors() || !sub.CanGetAuthors())
                continue;

            authors = &sub.SetAuthors();
        }
        else if((*pub)->IsGen())
        {
            objects::CCit_gen &gen = (*pub)->SetGen();
            if(!gen.IsSetAuthors() || !gen.CanGetAuthors())
                continue;

            authors = &gen.SetAuthors();
        }
        else if((*pub)->IsBook())
        {
            objects::CCit_book &book = (*pub)->SetBook();
            if(!book.IsSetAuthors() || !book.CanGetAuthors())
                continue;

            authors = &book.SetAuthors();
        }
        else if((*pub)->IsMan())
        {
            objects::CCit_let &man = (*pub)->SetMan();
            if(!man.IsSetCit() || !man.CanGetCit())
                continue;

            objects::CCit_book &book = man.SetCit();
            if(!book.IsSetAuthors() || !book.CanGetAuthors())
                continue;

            authors = &book.SetAuthors();
        }
        else if((*pub)->IsPatent())
        {
            objects::CCit_pat &pat = (*pub)->SetPatent();
            if(!pat.IsSetAuthors() || !pat.CanGetAuthors())
                continue;

            authors = &pat.SetAuthors();
        }
        else
            continue;


        if(authors->IsSetAffil() && authors->CanGetAffil() &&
           authors->GetAffil().Which() == objects::CAffil::e_Str)
        {
            objects::CAffil &affil = authors->SetAffil();
            char *aff = (char *) affil.GetStr().c_str();
            ShrinkSpaces(aff);
            affil.SetStr(aff);
        }

        if(authors->IsSetNames() && authors->CanGetNames() &&
           authors->GetNames().Which() == objects::CAuth_list::TNames::e_Std)
        {
            objects::CAuth_list::TNames &names = authors->SetNames();
            objects::CAuth_list::TNames::TStd::iterator it = (names.SetStd()).begin();
            objects::CAuth_list::TNames::TStd::iterator it_end = (names.SetStd()).end();
            for(; it != it_end; it++)
            {
                if((*it)->IsSetAffil() && (*it)->CanGetAffil() &&
                   (*it)->GetAffil().Which() == objects::CAffil::e_Str)
                {
                    objects::CAffil &affil = (*it)->SetAffil();
                    char *aff = (char *) affil.GetStr().c_str();
                    ShrinkSpaces(aff);
                    affil.SetStr(aff);
                }
                if((*it)->IsSetName() && (*it)->CanGetName() &&
                   (*it)->GetName().IsName())
                {
                    objects::CName_std &namestd = (*it)->SetName().SetName();
/* bsv: commented out single letter first name population*/
                    if(source != Parser::ESource::SPROT && source != Parser::ESource::PIR &&
                       !got_pmid)
                    {
                        if(!namestd.IsSetFirst() && namestd.IsSetInitials())
                        {
                            char *str = (char *) namestd.GetInitials().c_str();
                            if((strlen(str) == 1 || strlen(str) == 2) &&
                               (str[1] == '.' || str[1] == '\0'))
                            {
                                char *p = (char*) MemNew(2);
                                p[0] = str[0];
                                p[1] = '\0';
                                namestd.SetFirst(p);
                                MemFree(p);
                            }
                        }
                        if((*pub)->IsArticle())
                        {
                            objects::CCit_art &art1 = (*pub)->SetArticle();
                            if(art1.IsSetAuthors() && art1.CanGetAuthors())
                            {
                                objects::CAuth_list *authors1;
                                authors1 = &art1.SetAuthors();
                                if(authors1->IsSetNames() &&
                                   authors1->CanGetNames() &&
                                   authors1->GetNames().Which() == objects::CAuth_list::TNames::e_Std)
                                {
                                    objects::CAuth_list::TNames &names1 = authors1->SetNames();
                                    objects::CAuth_list::TNames::TStd::iterator it1 = (names1.SetStd()).begin();
                                    objects::CAuth_list::TNames::TStd::iterator it1_end = (names1.SetStd()).end();
                                    for(; it1 != it1_end; it1++)
                                    {
                                        if((*it1)->IsSetName() &&
                                           (*it1)->CanGetName() &&
                                           (*it1)->GetName().IsName())
                                        {
                                            objects::CName_std &namestd1 = (*it1)->SetName().SetName();
                                            if(!namestd1.IsSetFirst() &&
                                               namestd1.IsSetInitials())
                                            {
                                                char *str = (char *) namestd1.GetInitials().c_str();
                                                if((strlen(str) == 1 || strlen(str) == 2) &&
                                                   (str[1] == '.' || str[1] == '\0'))
                                                {
                                                    char *p = (char*) MemNew(2);
                                                    p[0] = str[0];
                                                    p[1] = '\0';
                                                    namestd1.SetFirst(p);
                                                    MemFree(p);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
/**/

                    if(namestd.IsSetSuffix())
                        continue;
                    fta_fix_last_initials(namestd, true);
                    if(!namestd.IsSetSuffix())
                        fta_fix_last_initials(namestd, false);
                }
            }
        }
    }
}

/**********************************************************/
static void fta_fix_imprint_language(TPubList &pub_list)
{
    NON_CONST_ITERATE(objects::CPub_equiv::Tdata, pub, pub_list)
    {
        if(!(*pub)->IsArticle())
            continue;

        objects::CCit_art &art = (*pub)->SetArticle();
        if(!art.IsSetFrom() || !art.GetFrom().IsJournal())
            continue;

        objects::CCit_jour &journal = art.SetFrom().SetJournal();

        if(journal.IsSetImp() && journal.GetImp().IsSetLanguage())
        {
            string language = journal.GetImp().GetLanguage();
            char *p;
            char *lang = (char *) language.c_str();
            for(p = lang; *p != '\0'; p++)
                if(*p >= 'A' && *p <= 'Z')
                     *p |= 040;
           journal.SetImp().SetLanguage(lang);
      }
    }
}

/**********************************************************/
static void fta_strip_er_remarks(objects::CPubdesc& pub_descr)
{
    if (!pub_descr.IsSetComment())
        return;

    ITERATE(objects::CPub_equiv::Tdata, pub, pub_descr.GetPub().Get())
    {
        if (!(*pub)->IsArticle())
            continue;

        const objects::CCit_art& art = (*pub)->GetArticle();
        if (!art.IsSetFrom() || !art.GetFrom().IsJournal())
            continue;

        const objects::CCit_jour& journal = art.GetFrom().GetJournal();
        
        int status = 0;
        if (journal.IsSetImp() && journal.GetImp().IsSetPubstatus())
            status = journal.GetImp().GetPubstatus();

        if (status == 3 ||          /* epublish */
            status == 4 ||          /* ppublish */
            status == 10)           /* aheadofprint */
        {
            char* comment = StringSave(pub_descr.GetComment().c_str());
            comment = fta_strip_pub_comment(comment, PubStatus);
            if (comment != NULL && comment[0] != 0)
                pub_descr.SetComment(comment);
            else
                pub_descr.ResetComment();

            MemFree(comment);
        }
    }
}

/**********************************************************/
static Uint1 fta_init_med_server(void)
{
    if(!MedArchInit())
        return(2);
    return(1);

}

/**********************************************************/
static Uint1 fta_init_tax_server(void)
{
    objects::CTaxon1 taxon_srv;
    if (!taxon_srv.Init())
        return(2);
    return(1);
}

/**********************************************************/
void fta_init_servers(ParserPtr pp)
{
    if(pp->taxserver != 0)
    {
        pp->taxserver = fta_init_tax_server();
        if(pp->taxserver == 2)
        {
            ErrPostEx(SEV_WARNING, ERR_SERVER_Failed,
                      "TaxArchInit call failed.");
        }
    }
    else
    {
        ErrPostEx(SEV_WARNING, ERR_SERVER_NoTaxLookup,
                  "No taxonomy lookup will be performed.");
    }

    if(pp->medserver != 0)
    {
        pp->medserver = fta_init_med_server();
        if(pp->medserver == 2)
        {
            ErrPostEx(SEV_ERROR, ERR_SERVER_Failed,
                      "MedArchInit call failed.");
        }
    }
    else
    {
        ErrPostEx(SEV_WARNING, ERR_SERVER_NoPubMedLookup,
                  "No medline lookup will be performed.");
    }
}

/**********************************************************/
void fta_fini_servers(ParserPtr pp)
{
    if (pp->medserver == 1)
        MedArchFini();
    /*    if(pp->taxserver == 1)
            tax1_fini();*/
}

#if 0 // RW-707
//std::shared_ptr<CPubseqAccess> s_pubseq;

/**********************************************************/
static Uint1 fta_init_pubseq(void)
{
    // C Toolkit's accpubseq.h library gets username/password from
    // the environment.
    // We are now using C++ Toolkit's cpubseq.hpp library which require
    // credentials during the construction of CPubseqAccess.  So read
    // the environment here and pass it along to the constructor.

    DBAPI_RegisterDriver_FTDS();
//    DBAPI_RegisterDriver_CTLIB();

    char* env_val = getenv("ALTER_OPEN_SERVER");
    string idserver = env_val ? env_val : "";

    env_val = getenv("ALTER_USER_NAME");
    string idusername = env_val ? env_val : "";

    env_val = getenv("ALTER_USER_PASSWORD");
    string idpassword = env_val ? env_val : "";

    s_pubseq.reset(new CPubseqAccess(idserver.empty() ? "PUBSEQ_OS_INTERNAL_GI64" : idserver.c_str(),
        idusername.empty() ? "anyone" : idusername.c_str(),
        idpassword.empty() ? "allowed" : idpassword.c_str()));

    if (s_pubseq == nullptr || !s_pubseq->CheckConnection())
        return(2);
    return(1);
    return 2;
}

/**********************************************************/
void fta_entrez_fetch_enable(ParserPtr pp)
{
    return; // RW-707

    if(pp->entrez_fetch != 0)
    {
        pp->entrez_fetch = fta_init_pubseq();
        if(pp->entrez_fetch == 2)
        {
            ErrPostEx(SEV_WARNING, ERR_SERVER_Failed,
                      "Failed to connect to PUBSEQ OS.");
        }
    }
    else
    {
        ErrPostEx(SEV_WARNING, ERR_SERVER_NotUsed,
                  "No PUBSEQ Bioseq fetch will be performed.");
    }
}

/**********************************************************/
void fta_entrez_fetch_disable(ParserPtr pp)
{
    if(pp->entrez_fetch == 1)
        s_pubseq.reset();
}
#endif

/**********************************************************/
void fta_fill_find_pub_option(ParserPtr pp, bool htag, bool rtag)
{
    pp->fpo.always_look = !htag;
    pp->fpo.replace_cit = !rtag;
    pp->fpo.merge_ids = true;
}


class CFindPub {

public:
    CFindPub(Parser* pp) : 
        m_pParser(pp),
        m_pPubFixListener(new CPubFixMessageListener()) {
            if (m_pParser) {
                const auto& findPubOptions = m_pParser->fpo;
                m_pPubFix.reset(new edit::CPubFix(
                            findPubOptions.always_look,
                            findPubOptions.replace_cit,
                            findPubOptions.merge_ids,
                            m_pPubFixListener.get()));
            }
        }

    using TEntryList = list<CRef<CSeq_entry>>;
    void Apply(TEntryList& entries);
private:
    void fix_pub_equiv(CPub_equiv& pub_equiv, Parser* pp, bool er);
    void fix_pub_annot(CPub& pub, Parser* pp, bool er);
    void find_pub(Parser* pp, list<CRef<CSeq_annot>>& annots, CSeq_descr& descrs);

    Parser* m_pParser;
    unique_ptr<CPubFixMessageListener> m_pPubFixListener;
    unique_ptr<edit::CPubFix> m_pPubFix = nullptr;
};



/**********************************************************/
static void fta_check_pub_ids(TPubList& pub_list)
{
    bool found = false;
    ITERATE(objects::CPub_equiv::Tdata, pub, pub_list)
    {
        if ((*pub)->IsArticle())
        {
            found = true;
            break;
        }
    }
        
    if (found)
        return;

    for (objects::CPub_equiv::Tdata::iterator pub = pub_list.begin(); pub != pub_list.end();)
    {
        if (!(*pub)->IsMuid() && !(*pub)->IsPmid())
        {
            ++pub;
            continue;
        }

        ErrPostEx(SEV_ERROR, ERR_REFERENCE_ArticleIdDiscarded,
                  "Article identifier was found for an unpublished, direct submission, book or unparsable article reference, and has been discarded : %s %d.",
                  (*pub)->IsMuid() ? "MUID" : "PMID", (*pub)->GetMuid());

        pub = pub_list.erase(pub);
    }
}


/**********************************************************/
void CFindPub::fix_pub_equiv(CPub_equiv& pub_equiv, ParserPtr pp, bool er)
{
    if (!pp)
        return;

    IndexblkPtr ibp = pp->entrylist[pp->curindx];


    list<CRef<CPub>> cit_arts;
    for (auto pPub : pub_equiv.Set()) 
    {
        if (!pPub->IsGen()) {
            continue;
        }
        const CCit_gen& cit_gen = pPub->SetGen();
        if (cit_gen.IsSetCit() &&
            (StringNCmp(cit_gen.GetCit().c_str(), "(er)", 4) == 0 || er))
        {
            cit_arts.push_back(pPub);
            break;
        }
    }

    if (cit_arts.empty())
    {
        fta_check_pub_ids(pub_equiv.Set());
        m_pPubFix->FixPubEquiv(pub_equiv);
        return;
    }

    auto cit_gen = cit_arts.front();

    list<CRef<CPub>> others;
    CRef<CPub> pMuid, pPmid;

    for (auto pPub : pub_equiv.Set())
    {
        if (cit_gen == pPub)
            continue;
        if (pPub->IsMuid() && !pMuid)
            pMuid = pPub;
        else if (pPub->IsPmid() && !pPmid)
            pPmid = pPub;
        else if (!pPub->IsArticle())
            others.push_back(pPub);
    }



    TEntrezId oldpmid = pPmid ? pPmid->GetPmid() : ZERO_ENTREZ_ID;
    TEntrezId oldmuid = pMuid ? pMuid->GetMuid() : ZERO_ENTREZ_ID;
    TEntrezId muid = ZERO_ENTREZ_ID;
    TEntrezId pmid = ZERO_ENTREZ_ID;

    CRef<CCit_art> new_cit_art;
    if(oldpmid > ZERO_ENTREZ_ID)
    {
        new_cit_art = FetchPubPmId(ENTREZ_ID_TO(Int4, oldpmid));
        if (new_cit_art.Empty())
        {
            ErrPostEx(SEV_REJECT, ERR_REFERENCE_InvalidPmid,
                      "MedArch failed to find a Cit-art for reference with pmid \"%d\".",
                      oldpmid);
            ibp->drop = 1;
        }
        else
        {
            if (new_cit_art->IsSetIds())
            {
                for (const auto& pId : new_cit_art->GetIds().Get())
                {
                    if (pId->IsPubmed()) {
                        pmid = pId->GetPubmed();
                    }
                    else if (pId->IsMedline()) {
                        muid = pId->GetMedline();
                    }
                }
            }

            if(pmid == ZERO_ENTREZ_ID)
            {
                ErrPostEx(SEV_REJECT, ERR_REFERENCE_CitArtLacksPmid,
                          "Cit-art returned by MedArch lacks pmid identifier in its ArticleIdSet.");
                ibp->drop = 1;
            }
            else if(pmid != oldpmid)
            {
                ErrPostEx(SEV_REJECT, ERR_REFERENCE_DifferentPmids,
                          "Pmid \"%d\" used for lookup does not match pmid \"%d\" in the ArticleIdSet of the Cit-art returned by MedArch.",
                          oldpmid, pmid);
                ibp->drop = 1;
            }
            if(muid > ZERO_ENTREZ_ID && oldmuid > ZERO_ENTREZ_ID && muid != oldmuid)
            {
                ErrPostEx(SEV_ERROR, ERR_REFERENCE_MuidPmidMissMatch,
                          "Reference has supplied Medline UI \"%d\" but it does not match muid \"%d\" in the Cit-art returned by MedArch.",
                          oldmuid, muid);
            }
        }
    }

    if (new_cit_art.NotEmpty() && !ibp->drop)
    {
        cit_arts.clear();
        CRef<objects::CPub> new_pub(new objects::CPub);
        new_pub->SetArticle(*new_cit_art);
        cit_arts.push_back(new_pub);

        if (pmid > ZERO_ENTREZ_ID && !pPmid)
        {
            pPmid = Ref(new CPub());
            pPmid->SetPmid().Set(pmid);
        }

        if(muid > ZERO_ENTREZ_ID && !pMuid)
        {
            pMuid = Ref(new CPub());
            pMuid->SetMuid(muid);
        }
    }

    auto& pub_list = pub_equiv.Set();
    pub_list = others;
    if (pPmid) {
        pub_list.push_back(pPmid);
    }
    if (pMuid && muid > ZERO_ENTREZ_ID) {
        pub_list.push_back(pMuid);
    }
    pub_list.splice(pub_list.end(), cit_arts);
}

/**********************************************************/
void CFindPub::fix_pub_annot(CPub& pub, ParserPtr pp, bool er)
{
    if (pp == NULL)
        return;

    if (pub.IsEquiv())
    {
        fix_pub_equiv(pub.SetEquiv(), pp, er);
        if(pp->qamode)
            fta_fix_imprint_language(pub.SetEquiv().Set());
        fta_fix_affil(pub.SetEquiv().Set(), pp->source);
        return;
    }

    m_pPubFix->FixPub(pub);
}


/**********************************************************/
void CFindPub::find_pub(ParserPtr pp, list<CRef<CSeq_annot>>& annots, CSeq_descr& descrs)
{
    bool er = any_of(begin(descrs.Get()), end(descrs.Get()),
            [](CRef<CSeqdesc> pDesc) {
                if (pDesc->IsPub()) {
                    const auto& pubdesc = pDesc->GetPub();
                    return (pubdesc.IsSetComment() &&
                            fta_remark_is_er(pubdesc.GetComment().c_str()));
                }
                return false;
            });


    for (auto pDescr : descrs.Set())
    {
        if (!pDescr->IsPub())
            continue;

        CPubdesc& pub_descr = pDescr->SetPub();
        fix_pub_equiv(pub_descr.SetPub(), pp, er);
        if(pp->qamode)
            fta_fix_imprint_language(pub_descr.SetPub().Set());
        fta_fix_affil(pub_descr.SetPub().Set(), pp->source);
        fta_strip_er_remarks(pub_descr);
    }

    for (auto pAnnot : annots)
    {
        if (!pAnnot->IsSetData() || !pAnnot->GetData().IsFtable())              /* feature table */
            continue;


        for (auto pFeat : pAnnot->SetData().SetFtable()) 
        {
            if (pFeat->IsSetData() && pFeat->GetData().IsPub())   /* pub feature */
            {
                fix_pub_equiv(pFeat->SetData().SetPub().SetPub(), pp, er);
                if(pp->qamode)
                    fta_fix_imprint_language(pFeat->SetData().SetPub().SetPub().Set());
                fta_fix_affil(pFeat->SetData().SetPub().SetPub().Set(), pp->source);
                fta_strip_er_remarks(pFeat->SetData().SetPub());
            }

            if (!pFeat->IsSetCit()) {
                continue;
            }

            for (auto pPub : pFeat->SetCit().SetPub()) {
                if (pPub) {
                    fix_pub_annot(*pPub, pp, er);
                }
            }
        }
    }
}

/**********************************************************/
//static void fta_find_pub(ParserPtr pp, TEntryList& seq_entries)
void CFindPub::Apply(list<CRef<CSeq_entry>>& seq_entries)
{
    for (auto pEntry : seq_entries) 
    {
        for (CTypeIterator<objects::CBioseq_set> bio_set(Begin(*pEntry)); bio_set; ++bio_set)
        {
            find_pub(m_pParser, bio_set->SetAnnot(), bio_set->SetDescr());

            if (bio_set->GetDescr().Get().empty())
                bio_set->ResetDescr();

            if (bio_set->SetAnnot().empty())
                bio_set->ResetAnnot();
        }

        for (CTypeIterator<objects::CBioseq> bioseq(Begin(*pEntry)); bioseq; ++bioseq)
        {
            find_pub(m_pParser, bioseq->SetAnnot(), bioseq->SetDescr());

            if (bioseq->GetDescr().Get().empty())
                bioseq->ResetDescr();

            if (bioseq->SetAnnot().empty())
                bioseq->ResetAnnot();
        }
    }
}

/**********************************************************/
void fta_find_pub_explore(ParserPtr pp, TEntryList& seq_entries)
{
    if(pp->medserver == 0)
        return;

    if(pp->medserver == 2)
        pp->medserver = fta_init_med_server();

    if (pp->medserver == 1)
    {
        CFindPub find_pub(pp);
        find_pub.Apply(seq_entries);
    }
}

/**********************************************************/
static void new_synonym(objects::COrg_ref& org_ref, objects::COrg_ref& tax_org_ref)
{
    if (!org_ref.CanGetSyn() || !tax_org_ref.CanGetSyn())
        return;

    ITERATE(objects::COrg_ref::TSyn, org_syn, org_ref.GetSyn())
    {
        bool found = false;
        ITERATE(objects::COrg_ref::TSyn, tax_syn, tax_org_ref.GetSyn())
        {
            if (*org_syn == *tax_syn)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            ErrPostEx(SEV_INFO, ERR_ORGANISM_NewSynonym,
                      "New synonym: %s for [%s].",
                      org_syn->c_str(), org_ref.GetTaxname().c_str());
        }
    }
}


#define TAX_SERVER_TIMEOUT 3
static const STimeout s_timeout = { TAX_SERVER_TIMEOUT, 0 };

static void fix_synonyms(objects::CTaxon1& taxon, objects::COrg_ref& org_ref)
{
    bool with_syns = taxon.SetSynonyms(false);
    if (!with_syns)
        org_ref.SetSyn().clear();
    else
        taxon.SetSynonyms(true);
}

/**********************************************************/
static CRef<objects::COrg_ref> fta_get_orgref_byid(ParserPtr pp, unsigned char* drop, Int4 taxid, bool isoh)
{
    CConstRef<objects::CTaxon2_data> taxdata;

    objects::CTaxon1 taxon;

    bool connection_failed = false;
    for (size_t i = 0; i < 3 && taxdata.Empty(); ++i)
    {
        if (taxon.Init(&s_timeout))
        {
            taxdata = taxon.GetById(TAX_ID_FROM(Int4, taxid));
        }
        else
        {
            connection_failed = true;
            break;
        }
    }

    CRef<objects::COrg_ref> ret;
    if (taxdata.Empty())
    {
        if (connection_failed)
        {
            ErrPostEx(SEV_FATAL, ERR_SERVER_TaxServerDown,
                      "Taxonomy lookup failed for taxid %d, apparently because the server is down. Cannot generate ASN.1 for this entry.",
                      taxid);
            *drop = 1;
        }
        else
        {
            ErrPostEx(SEV_ERROR, ERR_ORGANISM_TaxNameNotFound,
                      "Taxname not found: [taxid %d].", taxid);
        }
        return ret;
    }

    if (taxdata->GetIs_species_level() != 1 && !isoh)
    {
        ErrPostEx(SEV_WARNING, ERR_ORGANISM_TaxIdNotSpecLevel,
                  "Taxarch hit is not on species level: [taxid %d].", taxid);
    }

    ret.Reset(new objects::COrg_ref);
    ret->Assign(taxdata->GetOrg());
    fix_synonyms(taxon, *ret);

    if (ret->IsSetSyn() && ret->GetSyn().empty())
        ret->ResetSyn();

    return ret;
}

/**********************************************************/
CRef<objects::COrg_ref> fta_fix_orgref_byid(ParserPtr pp, Int4 taxid, unsigned char* drop, bool isoh)
{
    CRef<objects::COrg_ref> ret;

    if(taxid < 1 && pp->taxserver == 0)
        return ret;

    if(pp->taxserver == 2)
        pp->taxserver = fta_init_tax_server();

    if(pp->taxserver == 2)
    {
        ErrPostEx(SEV_FATAL, ERR_SERVER_TaxServerDown,
                  "Taxonomy lookup failed for taxid %d, because the server is down. Cannot generate ASN.1 for this entry.",
                  taxid);
        *drop = 1;
        return ret;
    }

    ret = fta_get_orgref_byid(pp, drop, taxid, isoh);
    if (ret.NotEmpty())
    {
        ErrPostEx(SEV_INFO, ERR_SERVER_TaxNameWasFound,
                  "Taxname _was_ found for taxid %d", taxid);
    }

    return ret;
}

/**********************************************************/
static CRef<objects::COrg_ref> fta_replace_org(ParserPtr pp, unsigned char* drop, objects::COrg_ref& org_ref,
                                                           const Char* pn, int merge, Int4 attempt)
{
    IndexblkPtr ibp = pp->entrylist[pp->curindx];

    CConstRef<objects::CTaxon2_data> taxdata;

    objects::CTaxon1 taxon;

    bool connection_failed = true;
    for (size_t i = 0; i < 3 && taxdata.Empty(); ++i)
    {
        if (taxon.Init(&s_timeout))
        {
            if (merge)
            {
                taxdata = taxon.LookupMerge(org_ref);
            }
            else
                taxdata = taxon.Lookup(org_ref);
            connection_failed = false;
            break;
        }
        else
            taxon.Fini();
    }

    CRef<objects::COrg_ref> ret;

    if (taxdata.Empty())
    {
        if(attempt == 1)
            return ret;

        if (connection_failed)
        {
            ErrPostEx(SEV_FATAL, ERR_SERVER_TaxServerDown,
                      "Taxonomy lookup failed for \"%s\", apparently because the server is down. Cannot generate ASN.1 for this entry.",
                      pn);
            *drop = 1;
        }
        else if(taxon.GetTaxIdByOrgRef(org_ref) < ZERO_TAX_ID)
        {
            if((pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::EMBL) &&
               ibp->is_pat && ibp->taxid > 0 && !ibp->organism.empty())
            {
                ret = fta_fix_orgref_byid(pp, ibp->taxid, &ibp->drop, true);
                if (ret.NotEmpty() && ret->IsSetTaxname() &&
                   ret->GetTaxname() == ibp->organism)
                {
                    ibp->no_gc_warning = true;
                    return ret;
                }
            }
            ErrPostEx(SEV_ERROR, ERR_ORGANISM_TaxIdNotUnique,
                      "Not an unique Taxonomic Id for [%s].", pn);
        }
        else
        {
            ErrPostEx(SEV_ERROR, ERR_ORGANISM_TaxNameNotFound,
                      "Taxon Id not found for [%s].", pn);
        }
        return ret;
    }

    if (taxdata->GetIs_species_level() != 1 && (ibp->is_pat == false ||
       (pp->source != Parser::ESource::EMBL && pp->source != Parser::ESource::DDBJ)))
    {
        ErrPostEx(SEV_WARNING, ERR_ORGANISM_TaxIdNotSpecLevel,
                  "Taxarch hit is not on species level for [%s].", pn);
    }

    ret.Reset(new objects::COrg_ref);

    if (merge)
        ret->Assign(org_ref);
    else
        ret->Assign(taxdata->GetOrg());

    return ret;
}

/**********************************************************/
void fta_fix_orgref(ParserPtr pp, objects::COrg_ref& org_ref, unsigned char* drop,
                    char* organelle)
{
    Int4      attempt;
    int       merge;

    if (org_ref.IsSetTaxname())
    {
        std::string taxname = org_ref.GetTaxname();

        size_t last_char = taxname.size();
        for (; last_char; --last_char)
        {
            if (!isspace(taxname[last_char]))
                break;
        }

        if (!isspace(taxname[last_char]))
            ++last_char;
        org_ref.SetTaxname(taxname.substr(0, last_char));
    }

    if(pp->taxserver == 0)
        return;

    if(pp->taxserver == 2)
        pp->taxserver = fta_init_tax_server();

    std::string old_taxname;
    if (organelle != NULL)
    {
        std::string taxname = org_ref.IsSetTaxname() ? org_ref.GetTaxname() : "",
                    organelle_str(organelle),
                    space(taxname.size() ? " " : "");

        old_taxname = taxname;
        taxname = organelle_str + space + taxname;
        org_ref.SetTaxname(taxname);
        attempt = 1;
    }
    else
    {
        attempt = 2;
    }

    std::string taxname = org_ref.IsSetTaxname() ? org_ref.GetTaxname() : "";
    if (pp->taxserver == 2)
    {
        ErrPostEx(SEV_FATAL, ERR_SERVER_TaxServerDown,
                  "Taxonomy lookup failed for \"%s\", because the server is down. Cannot generate ASN.1 for this entry.",
                  taxname.c_str());
        *drop = 1;
    }
    else
    {
        merge = (pp->format == Parser::EFormat::PIR) ? 0 : 1;

        CRef<objects::COrg_ref> new_org_ref = fta_replace_org(pp, drop, org_ref, taxname.c_str(), merge, attempt);
        if (new_org_ref.Empty() && attempt == 1)
        {
            org_ref.SetTaxname(old_taxname);
            old_taxname.clear();
            new_org_ref = fta_replace_org(pp, drop, org_ref, "", merge, 2);
        }

        if (new_org_ref.NotEmpty())
        {
            ErrPostEx(SEV_INFO, ERR_SERVER_TaxNameWasFound,
                      "Taxon Id _was_ found for [%s]", taxname.c_str());
            if(pp->format == Parser::EFormat::PIR)
                new_synonym(org_ref, *new_org_ref);

            org_ref.Assign(*new_org_ref);
        }
    }

    if (org_ref.IsSetSyn() && org_ref.GetSyn().empty())
        org_ref.ResetSyn();
}

/**********************************************************/
static TGi fta_get_gi_for_seq_id(const objects::CSeq_id& id)
{
    TGi gi = objects::sequence::GetGiForId(id, GetScope());
    if(gi > ZERO_GI)
        return(gi);


    objects::CSeq_id test_id;
    test_id.SetGenbank().SetAccession(HEALTHY_ACC);

    int i = 0;
    for (; i < 5; i++)
    {
        if (objects::sequence::GetGiForId(test_id, GetScope()) > ZERO_GI)
            break;
        SleepSec(3);
    }

    if(i == 5)
        return GI_CONST(-1);

    gi = objects::sequence::GetGiForId(id, GetScope());
    if (gi > ZERO_GI)
        return(gi);

    return ZERO_GI;
}

/**********************************************************
 * returns -1 if couldn't get division;
 *          1 if it's CON;
 *          0 if it's not CON.
 */
Int4 fta_is_con_div(ParserPtr pp, const objects::CSeq_id& id, const Char* acc)
{
    if(pp->entrez_fetch == 0)
        return(-1);
    //if (pp->entrez_fetch == 2)
    //    pp->entrez_fetch = fta_init_pubseq();
    if(pp->entrez_fetch == 2)
    {
        ErrPostEx(SEV_ERROR, ERR_ACCESSION_CannotGetDivForSecondary,
                  "Failed to determine division code for secondary accession \"%s\". Entry dropped.",
                  acc);
        pp->entrylist[pp->curindx]->drop = 1;
        return(-1);
    }

    TGi gi = fta_get_gi_for_seq_id(id);
    if(gi < ZERO_GI)
    {
        ErrPostEx(SEV_ERROR, ERR_ACCESSION_CannotGetDivForSecondary,
                  "Failed to determine division code for secondary accession \"%s\". Entry dropped.",
                  acc);
        pp->entrylist[pp->curindx]->drop = 1;
        return(-1);
    }

    if (gi == ZERO_GI)
        return(0);
#if 0 // RW-707
    CPubseqAccess::IdGiClass id_gi;
    CPubseqAccess::IdBlobClass id_blob;

    if (!s_pubseq->GetIdGiClass(gi, id_gi) || !s_pubseq->GetIdBlobClass(id_gi, id_blob) ||
        id_blob.div[0] == '\0')
    {
        ErrPostEx(SEV_ERROR, ERR_ACCESSION_CannotGetDivForSecondary,
                  "Failed to determine division code for secondary accession \"%s\". Entry dropped.",
                  acc);
        pp->entrylist[pp->curindx]->drop = 1;
        return(-1);
    }
    if (NStr::EqualNocase(id_blob.div, "CON"))
        return(1);
#endif
    return(0);
}

/**********************************************************/
CRef<objects::CCit_art> fta_citart_by_pmid(Int4 pmid, bool& done)
{
    CRef<objects::CCit_art> cit_art;

    done = true;
    if (pmid < 0)
        return cit_art;
    
    cit_art = FetchPubPmId(pmid);
    return cit_art;
}

/**********************************************************/
void fta_init_gbdataloader()
{
    objects::CGBDataLoader::RegisterInObjectManager(*objects::CObjectManager::GetInstance());
}

END_NCBI_SCOPE
