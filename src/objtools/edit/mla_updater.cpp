#include <ncbi_pch.hpp>
#include <objtools/edit/mla_updater.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/mla/Title_msg.hpp>
#include <objects/mla/Title_msg_list.hpp>
#include <objects/mla/mla_client.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


CMLAUpdaterBase::CMLAUpdaterBase(bool bNorm) :
    m_mlac(new CMLAClient()), m_bNorm(bNorm)
{
}

bool CMLAUpdaterBase::Init()
{
    try {
        m_mlac->Connect();
        return true;
    } catch (...) {
    }
    return false;
}

void CMLAUpdaterBase::Fini()
{
    try {
        m_mlac->Disconnect();
    } catch (...) {
    }
}

void CMLAUpdaterBase::SetClient(CMLAClient* mla)
{
    m_mlac.Reset(mla);
}

TEntrezId CMLAUpdaterBase::CitMatch(const CPub& pub, EPubmedError* perr)
{
    CMLAClient::TReply reply;

    try {
        return ENTREZ_ID_FROM(int, m_mlac->AskCitmatchpmid(pub, &reply));
    } catch (CException&) {
    }

    if (perr) {
        EError_val mlaErrorVal = reply.GetError();
        *perr = mlaErrorVal;
    }
    return ZERO_ENTREZ_ID;
}

static void SetArticle(CCit_art& art, const SCitMatch& cm)
{
    CCit_jour& J = art.SetFrom().SetJournal();

    if (! cm.Journal.empty()) {
        CRef<CTitle::C_E> t(new CTitle::C_E);
        t->SetName(cm.Journal);
        J.SetTitle().Set().push_back(t);
    }

    if (! cm.Volume.empty()) {
        J.SetImp().SetVolume(cm.Volume);
    }

    if (! cm.Page.empty()) {
        J.SetImp().SetPages(cm.Page);
    }

    int year = 0;
    if (! cm.Year.empty()) {
        NStr::StringToNumeric(cm.Year, &year, NStr::fConvErr_NoThrow);
    }
    J.SetImp().SetDate().SetStd().SetYear(year);

    if (! cm.Author.empty()) {
        CRef<CAuthor> author(new CAuthor);
        author->SetName().SetMl(cm.Author);
        art.SetAuthors().SetNames().SetMl().push_back(cm.Author);
    }

    if (! cm.Issue.empty()) {
        J.SetImp().SetIssue(cm.Issue);
    }

    if (! cm.Title.empty()) {
        CRef<CTitle::C_E> title(new CTitle::C_E);
        title->SetName(cm.Title);
        art.SetTitle().Set().push_back(title);
    }

    if (cm.InPress) {
        J.SetImp().SetPrepub(CImprint::ePrepub_in_press);
    }
}

TEntrezId CMLAUpdaterBase::CitMatch(const SCitMatch& cm, EPubmedError* perr)
{
    CPub pub;
    SetArticle(pub.SetArticle(), cm);
    return this->CitMatch(pub, perr);
}

CRef<CPub> CMLAUpdaterBase::x_GetPub(TEntrezId pmid, EPubmedError* perr)
{
    CMLAClient::TReply reply;

    try {
        CRef<CPub> pub = m_mlac->AskGetpubpmid(CPubMedId(pmid), &reply);
        if (m_bNorm)
            Normalize(*pub);
        if (m_pub_interceptor)
            m_pub_interceptor(pub);
        return pub;
    } catch (CException&) {
    }

    if (perr) {
        EError_val mlaErrorVal = reply.GetError();
        *perr = mlaErrorVal;
    }
    return {};
}

string CMLAUpdaterBase::GetTitle(const string& title)
{
    CRef<CTitle> title_new(new CTitle);
    CRef<CTitle::C_E> type_new(new CTitle::C_E);
    type_new->SetIso_jta(title);
    title_new->Set().push_back(type_new);

    CRef<CTitle_msg> msg_new(new CTitle_msg);
    msg_new->SetType(eTitle_type_iso_jta);
    msg_new->SetTitle(*title_new);

    CRef<CTitle_msg_list> msg_list_new;
    try {
        msg_list_new = m_mlac->AskGettitle(*msg_new);
    } catch (CException&) {
        // msg_list_new stays empty
    }

    if (msg_list_new.NotEmpty() && msg_list_new->IsSetTitles()) {
        auto is_jta = [](const CRef<CTitle::C_E>& title) -> bool { return title->IsIso_jta(); };
        for (const auto& item : msg_list_new->GetTitles()) {
            const CTitle& cur_title = item->GetTitle();
            if (cur_title.IsSet()) {
                const CTitle::Tdata& title_list = cur_title.Get();
                auto jta_title = find_if(title_list.begin(), title_list.end(), is_jta);
                if (jta_title != title_list.end()) {
                    return (*jta_title)->GetIso_jta();
                }
            }
        }
    }

    return {};
}

CRef<CPub> CMLAUpdater::GetPub(TEntrezId pmid, EPubmedError* perr)
{
    return x_GetPub(pmid, perr);
}

CRef<CPub> CMLAUpdaterWithCache::GetPub(TEntrezId pmid, EPubmedError* perr)
{
    m_num_requests++;
    auto& pub = m_cache[pmid];
    if (pub.Empty()) {
        pub = x_GetPub(pmid, perr);
    } else {
        m_cache_hits++;
    }
    if (pub.Empty())
        return {};
    else {
        auto copied = new CPub();
        copied->Assign(*pub);
        return Ref(copied);
    }
}

void CMLAUpdaterWithCache::ReportStats(std::ostream& os)
{
    os << "CMLAUpdater: " << m_cache_hits << " cache_hits out of " << m_num_requests << " requests\n";
}

void CMLAUpdaterWithCache::ClearCache()
{
    m_cache.clear();
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
