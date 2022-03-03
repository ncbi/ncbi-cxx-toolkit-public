#include <ncbi_pch.hpp>
#include <objtools/edit/mla_updater.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/mla/Title_msg.hpp>
#include <objects/mla/Title_msg_list.hpp>
#include <objects/mla/mla_client.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


CMLAUpdater::CMLAUpdater()
  : m_mlac(new CMLAClient())
{
}

bool CMLAUpdater::Init()
{
    CMla_back reply;
    try {
        m_mlac->AskInit(&reply);
    } catch (...) {
        return false;
    }
    return reply.IsInit();
}

void CMLAUpdater::Fini()
{
    try {
        m_mlac->AskFini();
    } catch (...) {
    }
}

void CMLAUpdater::SetClient(CMLAClient* mla)
{
    m_mlac.Reset(mla);
}

TEntrezId CMLAUpdater::CitMatch(const CPub& pub)
{
    try {
        return ENTREZ_ID_FROM(int, m_mlac->AskCitmatchpmid(pub));
    } catch (CException&) {
    }

    return ZERO_ENTREZ_ID;
}

CRef<CPub> CMLAUpdater::GetPub(TEntrezId pmid, EPubmedError* perr)
{
    CMLAClient::TReply reply;

    try {
        return m_mlac->AskGetpubpmid(CPubMedId(pmid), &reply);
    } catch (CException&) {
        if (perr) {
            EError_val mlaErrorVal = reply.GetError();
            *perr = mlaErrorVal;
        }
    }

    return {};
}

string CMLAUpdater::GetTitle(const string& title)
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
                const auto& title_list = cur_title.Get();
                auto jta_title = find_if(title_list.begin(), title_list.end(), is_jta);
                if (jta_title != title_list.end()) {
                    return (*jta_title)->GetIso_jta();
                }
            }
        }
    }

    return {};
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
