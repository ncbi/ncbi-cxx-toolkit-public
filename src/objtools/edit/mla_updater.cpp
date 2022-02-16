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

CRef<CTitle_msg_list> CMLAUpdater::GetTitle(const CTitle_msg& msg)
{
    try {
        return m_mlac->AskGettitle(msg);
    } catch (CException&) {
    }

    return {};
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
