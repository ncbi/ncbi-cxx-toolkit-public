/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>

#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/mla/mla_client.hpp>
#include <objects/mla/Title_msg.hpp>
#include <objects/mla/Title_msg_list.hpp>
#include <objects/medline/Medline_entry.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>

#include "wgs_med.hpp"
#include "wgs_utils.hpp"

namespace wgsparse
{

CMLAClient& GetMLA()
{
    static CRef<CMLAClient> mla;

    if (mla.Empty()) {
        mla.Reset(new CMLAClient);
        mla->AskInit();
    }

    return *mla;
}

int GetPMID(const CPub& pub)
{
    int pmid = 0;
    
    try {
        pmid = GetMLA().AskCitmatchpmid(pub);
    }
    catch (const CException& ) {
        // Failed lookup is not an error
    }

    return pmid;
}

static void StripPubComment(string& comment)
{
    static const string PUB_STATUS[] = {
        "Publication Status: Available-Online prior to print",
        "Publication Status : Available-Online prior to print",
        "Publication_Status: Available-Online prior to print",
        "Publication_Status : Available-Online prior to print",
        "Publication-Status: Available-Online prior to print",
        "Publication-Status : Available-Online prior to print",
        "Publication Status: Online-Only",
        "Publication Status : Online-Only",
        "Publication_Status: Online-Only",
        "Publication_Status : Online-Only",
        "Publication-Status: Online-Only",
        "Publication-Status : Online-Only",
        "Publication Status: Available-Online",
        "Publication Status : Available-Online",
        "Publication_Status: Available-Online",
        "Publication_Status : Available-Online",
        "Publication-Status: Available-Online",
        "Publication-Status : Available-Online"
    };

    for (auto& cur_status : PUB_STATUS) {

        SIZE_TYPE pos = NStr::FindNoCase(comment, cur_status, 0);

        while (pos != NPOS) {

            size_t len = comment.size(),
                   offset = pos + cur_status.size();
            while (offset < len && (comment[offset] == ';' || comment[offset] == ' ')) {
                ++offset;
            }
            comment = comment.substr(0, pos) + comment.substr(offset);
            pos = NStr::FindNoCase(comment, cur_status, pos);
        }
    }

    if (NStr::StartsWith(comment, "Publication Status", NStr::eNocase) ||
        NStr::StartsWith(comment, "Publication-Status", NStr::eNocase) ||
        NStr::StartsWith(comment, "Publication_Status", NStr::eNocase)) {

        ERR_POST_EX(0, 0, Info << "An unusual Publication Status comment exists for this record: \"" << comment <<
                    "\". If it is a new variant of the special comments used to indicate ahead-of-print or online-only articles, then the comment must be added to the appropriate table of the parser.");
    }
}

static bool NeedToStripComment(const CCit_jour& journal)
{
    return journal.IsSetImp() && journal.GetImp().IsSetPubstatus() &&
        (journal.GetImp().GetPubstatus() == ePubStatus_epublish ||
        journal.GetImp().GetPubstatus() == ePubStatus_ppublish ||
        journal.GetImp().GetPubstatus() == ePubStatus_aheadofprint);
}

static void StripErRemarks(CPubdesc& pubdescr)
{
    if (pubdescr.IsSetPub() && pubdescr.GetPub().IsSet() && pubdescr.IsSetComment()) {

        auto& pubs = pubdescr.SetPub().Set();
        for (auto& pub : pubs) {

            if (pub->IsArticle() && pub->GetArticle().IsSetFrom() && pub->GetArticle().GetFrom().IsJournal()) {

                CCit_jour& journal = pub->SetArticle().SetFrom().SetJournal();
                if (NeedToStripComment(journal)) {
                    string& comment = pubdescr.SetComment();
                    StripPubComment(comment);
                    if (comment.empty()) {
                        pubdescr.ResetComment();
                    }
                    break;
                }
            }
        }
    }
}

static void MedlineToISO(CCit_art& cit_art)
{
    CAuth_list& auths = cit_art.SetAuthors();
    if (auths.IsSetNames()) {
        if (auths.GetNames().IsMl()) {
            auths.ConvertMlToStandard();
        }
        else if (auths.GetNames().IsStd())
        {
            for (auto& auth: auths.SetNames().SetStd())
            {
                if (auth->IsSetName() && auth->GetName().IsMl()) {

                    CRef<CAuthor> standard_auth = CAuthor::ConvertMlToStandard(*auth);
                    auth->Assign(*standard_auth);
                }
            }
        }
    }

    if (!cit_art.IsSetFrom() || !cit_art.GetFrom().IsJournal()) {

        // from a journal - get iso_jta
        CCit_jour& journal = cit_art.SetFrom().SetJournal();

        bool is_iso = false;

        if (journal.IsSetTitle()) {
            
            auto& titles = journal.GetTitle().Get();
            is_iso = find_if(titles.begin(), titles.end(), [](const CRef<CTitle::C_E>& title) { return title->IsIso_jta(); }) != titles.end();
        }

        if (!is_iso) {
            if (journal.IsSetTitle()) {

                CTitle::C_E& first_title = *journal.SetTitle().Set().front();

                const string& title_str = journal.GetTitle().GetTitle(first_title.Which());

                CRef<CTitle> title_new(new CTitle);
                CRef<CTitle::C_E> type_new(new CTitle::C_E);
                type_new->SetIso_jta(title_str);
                title_new->Set().push_back(type_new);

                CRef<CTitle_msg> msg_new(new CTitle_msg);
                msg_new->SetType(eTitle_type_iso_jta);
                msg_new->SetTitle(*title_new);

                CRef<CTitle_msg_list> msg_list_new;
                try {
                    msg_list_new = GetMLA().AskGettitle(*msg_new);
                }
                catch (exception &) {
                    msg_list_new.Reset();
                }

                if (msg_list_new.NotEmpty() && msg_list_new->IsSetTitles()) {
                    for (auto& title: msg_list_new->GetTitles())
                    {
                        const CTitle &cur_title = title->GetTitle();
                        if (cur_title.IsSet()) {

                            auto iso_title = find_if(cur_title.Get().begin(), cur_title.Get().end(), [](const CRef<CTitle::C_E>& item) { return item->IsIso_jta(); });
                            if (iso_title != cur_title.Get().end()) {
                                first_title.SetIso_jta((*iso_title)->GetIso_jta());
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (journal.IsSetImp()) {
            // remove Eng language
            if (journal.GetImp().IsSetLanguage() && journal.GetImp().GetLanguage() == "Eng")
                journal.SetImp().ResetLanguage();
        }
    }
}

static void SplitMedlineEntry(CPub_equiv::Tdata& medlines)
{
    CPub& pub = *medlines.front();
    CMedline_entry& medline = pub.SetMedline();
    if (!medline.IsSetCit() && medline.IsSetPmid() && medline.GetPmid() < 0)
        return;

    CRef<CPub> pmid;
    if (medline.GetPmid() > 0) {
        pmid.Reset(new CPub);
        pmid->SetPmid(medline.GetPmid());
    }

    CRef<CPub> cit_art;
    if (medline.IsSetCit()) {
        cit_art.Reset(new CPub);
        cit_art->SetArticle(medline.SetCit());
        MedlineToISO(cit_art->SetArticle());
    }

    medlines.clear();

    if (pmid.NotEmpty())
        medlines.push_back(pmid);

    if (cit_art.NotEmpty())
        medlines.push_back(cit_art);
}

static bool IsInpressSet(const ncbi::objects::CCit_art& cit_art)
{
    if (!cit_art.IsSetFrom())
        return false;

    bool ret = false;
    if (cit_art.GetFrom().IsJournal()) {
        const CCit_jour& journal = cit_art.GetFrom().GetJournal();
        ret = journal.IsSetImp() && journal.GetImp().IsSetPrepub() && journal.GetImp().GetPrepub() == CImprint::ePrepub_in_press;
    }
    else if (cit_art.GetFrom().IsBook() || cit_art.GetFrom().IsProc()) {
        const ncbi::objects::CCit_book& book = cit_art.GetFrom().GetBook();
        ret = book.IsSetImp() && book.GetImp().IsSetPrepub() && book.GetImp().GetPrepub() == CImprint::ePrepub_in_press;
            return true;
    }
    return false;
}

static CRef<CCit_art> FetchPubPmId(int pmid)
{
    CRef<CCit_art> cit_art;
    if (pmid < 0)
        return cit_art;

    CRef<CPub> pub;
    try {
        pub = GetMLA().AskGetpubpmid(CPubMedId(pmid));
    }
    catch (exception &) {
        pub.Reset();
    }

    if (pub.NotEmpty() && pub->IsArticle()) {
        cit_art.Reset(new CCit_art);
        cit_art->Assign(pub->GetArticle());
        MedlineToISO(*cit_art);
    }

    return cit_art;
}


static bool IsInPress(const CCit_art& cit_art)
{
    if (!cit_art.IsSetFrom() || !cit_art.GetFrom().IsJournal() || !cit_art.GetFrom().GetJournal().IsSetTitle())
        return true;

    const CCit_jour& journal = cit_art.GetFrom().GetJournal();
    if (!journal.IsSetImp() || !journal.GetImp().IsSetVolume() || !journal.GetImp().IsSetPages() || !journal.GetImp().IsSetDate())
        return true;

    return false;
}


static void PropagateInPress(bool inpress, CCit_art& cit_art)
{
    if (!inpress || !IsInPress(cit_art) || !cit_art.IsSetFrom())
        return;

    if (cit_art.GetFrom().IsJournal()) {
        if (cit_art.GetFrom().GetJournal().IsSetImp())
            cit_art.SetFrom().SetJournal().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    }
    else if (cit_art.GetFrom().IsBook() || cit_art.GetFrom().IsProc()) {
        if (cit_art.GetFrom().GetBook().IsSetImp())
            cit_art.SetFrom().SetBook().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    }
}


static void FixPubEquivAppendPmid(int muid, CPub_equiv::Tdata& pmids)
{
    int oldpmid = pmids.empty() ? 0 : pmids.front()->GetPmid(),
        newpmid = 0;

    try {
        newpmid = GetMLA().AskUidtopmid(muid);
    }
    catch (exception &) {
        newpmid = -1;
    }

    if (oldpmid < 1 && newpmid < 1)
        return;

    if (oldpmid > 0 && newpmid > 0 && oldpmid != newpmid) {
        ERR_POST_EX(0, 0, Error << "Old PMID (" << oldpmid << ") doesn't match lookup (" << newpmid << "). Keeping lookup.");
    }

    if (pmids.empty()) {
        CRef<CPub> pmid_pub(new CPub);
        pmids.push_back(pmid_pub);
    }

    pmids.front()->SetPmid().Set((newpmid > 0) ? newpmid : oldpmid);
}

static size_t ExtractConsortiums(const CAuth_list_Base::C_Names::TStd& names, set<string>& extracted)
{
    int num_of_names = 0;
    
    for(auto& auth: names)
    {
        if (!auth->IsSetName()) {
            continue;
        }

        if (auth->GetName().IsName()) {
            ++num_of_names;
        }
        else if (auth->GetName().IsConsortium()) {
            extracted.insert(auth->GetName().GetConsortium());
        }
    }

    return num_of_names;
}


static bool CheckAuthors(CCit_art& cit, ncbi::objects::CCit_art& new_cit)
{
    if (!new_cit.IsSetAuthors() || !new_cit.GetAuthors().IsSetNames()) {
        if (cit.IsSetAuthors()) {
            new_cit.SetAuthors(cit.SetAuthors());
            cit.ResetAuthors();
        }
        return true;
    }

    if (!cit.IsSetAuthors() || !cit.GetAuthors().IsSetNames() || cit.GetAuthors().GetNames().Which() != new_cit.GetAuthors().GetNames().Which()) {
        return true;
    }

    set<string> old_consortiums;
    size_t num_of_old_names = ExtractConsortiums(cit.GetAuthors().GetNames().GetStd(), old_consortiums);

    set<string> new_consortiums;
    size_t num_of_new_names = ExtractConsortiums(new_cit.GetAuthors().GetNames().GetStd(), new_consortiums);

    if (!old_consortiums.empty()) {
        if (new_consortiums.empty()) {

            string old_names_str = NStr::Join(old_consortiums.begin(), old_consortiums.end(), " ");
            ERR_POST_EX(0, 0, Warning << "Publication as returned by MedArch lacks consortium authors of the original publication: \"" << old_names_str << "\".");

            for_each(old_consortiums.begin(), old_consortiums.end(),
                [&new_cit] (const string& old_cons)
                {
                    CRef<CAuthor> auth(new CAuthor);
                    auth->SetName().SetConsortium(old_cons);

                    new_cit.SetAuthors().SetNames().SetStd().push_front(auth);

                });
        }
        else {
            if (old_consortiums != new_consortiums) {

                string old_names_str = NStr::Join(old_consortiums.begin(), old_consortiums.end(), " "),
                       new_names_str = NStr::Join(new_consortiums.begin(), new_consortiums.end(), " ");
                ERR_POST_EX(0, 0, Warning << "Consortium author names differ. Original is \"" << old_names_str << "\". MedArch's is \"" << new_names_str << "\".");
            }

        }

        if (num_of_old_names == 0) {
            return true;
        }
    }

    set<string> old_authors;
    for (auto& auth: new_cit.GetAuthors().GetNames().GetStd())
    {
        if (auth->IsSetName() && auth->GetName().IsName() && auth->GetName().GetName().IsSetLast()) {

            string last_name = auth->GetName().GetName().GetLast();
            old_authors.insert(NStr::ToLower(last_name));
        }
    }

    size_t match = 0;
    for (auto& auth: cit.GetAuthors().GetNames().GetStd())
    {
        if (auth->IsSetName() && auth->GetName().IsName() && auth->GetName().GetName().IsSetLast()) {
            string last_name = auth->GetName().GetName().GetLast();
            NStr::ToLower(last_name);

            if (old_authors.find(last_name) != old_authors.end()) {
                ++match;
            }
        }
    }

    if (num_of_old_names > 3 * match)
        return false;

    if (num_of_new_names == 0) {
        new_cit.SetAuthors(cit.SetAuthors());
        cit.ResetAuthors();
    }

    return true;
}


static void FixPubEquiv(CPub_equiv::Tdata& pub_list)
{
    CPub_equiv::Tdata muids,
        pmids,
        medlines,
        others,
        cit_arts;

    for (auto& pub: pub_list) {
        if (pub->IsMuid()) {
            muids.push_back(pub);
        }
        else if (pub->IsPmid()) {
            pmids.push_back(pub);
        }
        else if (pub->IsArticle()) {
            if (pub->GetArticle().IsSetFrom() && pub->GetArticle().GetFrom().IsBook())
                others.push_back(pub);
            else
                cit_arts.push_back(pub);
        }
        else if (pub->IsMedline())
            medlines.push_back(pub);
        else
            others.push_back(pub);
    }

    pub_list.clear();
    pub_list.splice(pub_list.end(), others);

    if (!medlines.empty()) {
        if (medlines.size() > 1) {
            ERR_POST_EX(0, 0, Warning << "More than one Medline entry in Pub-equiv.");
            medlines.resize(1);
        }

        SplitMedlineEntry(medlines);
        pub_list.splice(pub_list.end(), medlines);
    }

    int oldpmid = 0,
        oldmuid = 0;

    if (!pmids.empty()) {
        if (pmids.size() > 1) {
            oldpmid = pmids.front()->GetPmid();
            for (auto& cur_pub: pmids)
            {
                if (cur_pub->GetPmid() != oldpmid) {
                    ERR_POST_EX(0, 0, Warning << "Two different pmids in Pub - equiv [" << oldpmid << "] [" << cur_pub->GetPmid() << "]");
                }
            }
            pmids.resize(1);
        }
    }

    if (!muids.empty()) {
        if (muids.size() > 1) {
            int oldmuid = muids.front()->GetMuid();
            for (auto& cur_pub : muids) {
                if (cur_pub->GetMuid() != oldmuid) {
                    ERR_POST_EX(0, 0, Warning << "Two different muids in Pub - equiv [" << oldmuid << "] [" << cur_pub->GetMuid() << "]");
                }
            }
            muids.resize(1);
        }
    }

    if (!cit_arts.empty()) {
        if (cit_arts.size() > 1) {
            ERR_POST_EX(0, 0, Warning << "More than one Cit-art in Pub-equiv.");
            cit_arts.resize(1);
        }

        CCit_art* cit_art = &cit_arts.front()->SetArticle();
        MedlineToISO(*cit_art);

        bool inpress = IsInpressSet(*cit_art);

        CRef<CPub> new_pub(new CPub);
        new_pub->SetArticle(*cit_art);

        int pmid = GetPMID(*new_pub);
        if (pmid) {

            if (oldpmid && oldpmid != pmid) { // already had a pmid
                ERR_POST_EX(0, 0, Error << "Old PMID (" << oldpmid << ") doesn't match lookup (" << pmid << "). Keeping lookup.");
            }

            CRef<CCit_art> new_cit_art = FetchPubPmId(pmid);

            if (new_cit_art.NotEmpty()) {

                if (CheckAuthors(*cit_art, *new_cit_art)) {
                    if (pmids.empty()) {
                        pmids.push_back(CRef<CPub>(new CPub));
                    }

                    pmids.front()->SetPmid().Set(pmid);
                    pub_list.splice(pub_list.end(), pmids);

                    ncbi::CRef<ncbi::objects::CPub> cit_pub(new ncbi::objects::CPub);
                    cit_pub->SetArticle(*new_cit_art);
                    pub_list.push_back(cit_pub);

                    cit_arts.clear();
                    cit_arts.push_back(cit_pub);
                    cit_art = new_cit_art;
                }
                else {
                    pub_list.splice(pub_list.end(), cit_arts);
                }
            }
            else {

                ERR_POST_EX(0, 0, Error << "Failed to get pub from MedArch server for pmid = " << pmid << ". Input one is preserved.");

                if (pmids.empty()) {
                    CRef<CPub> pmid_pub(new CPub);
                    pmids.push_back(pmid_pub);
                }

                pmids.front()->SetPmid().Set(pmid);
                pub_list.splice(pub_list.end(), pmids);
                pub_list.splice(pub_list.end(), cit_arts);
            }

            PropagateInPress(inpress, *cit_art);
        }
        else {
            PropagateInPress(inpress, *cit_art);
            pub_list.splice(pub_list.end(), cit_arts);
        }
    }
    else {
        if (oldpmid != 0) { // have a pmid but no cit-art
            
            CRef<CCit_art> new_cit_art = FetchPubPmId(oldpmid);

            if (new_cit_art.NotEmpty()) {

                MedlineToISO(*new_cit_art);
                CRef<CPub> cit_pub(new CPub);
                cit_pub->SetArticle(*new_cit_art);
                pub_list.push_back(cit_pub);
                pub_list.splice(pub_list.end(), pmids);
                return;
            }
            else {
                ERR_POST_EX(0, 0, Error << "Cant find article for pmid [" << oldpmid << ".");
            }
        }

        if (oldpmid > 0) {
            pub_list.splice(pub_list.end(), pmids);
        }
        else if (oldmuid > 0) {
            pub_list.splice(pub_list.end(), muids);
            FixPubEquivAppendPmid(oldmuid, pmids);
            pub_list.splice(pub_list.end(), pmids);
        }
    }
}

void SinglePubLookup(CPubdesc& pubdescr)
{
    FixPubEquiv(pubdescr.SetPub().Set());
    StripErRemarks(pubdescr);
}

}
