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

IMessageListener::EPostResult 
CPubFixMessageListener::PostMessage(const IMessage& message) 
{
    static const map<EDiagSev, ErrSev> sSeverityMap
               = {{eDiag_Trace, SEV_NONE},
                  {eDiag_Info, SEV_INFO},
                  {eDiag_Warning, SEV_WARNING},
                  {eDiag_Error, SEV_ERROR},
                  {eDiag_Critical, SEV_REJECT},
                  {eDiag_Fatal, SEV_FATAL}};

    ErrPostEx(sSeverityMap.at(message.GetSeverity()),
            message.GetCode(),
            message.GetSubCode(),
            message.GetText().c_str());

    return eHandled;
}

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

END_NCBI_SCOPE
