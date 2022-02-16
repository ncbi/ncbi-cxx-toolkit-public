#ifndef _MLA_UPDATER_HPP_
#define _MLA_UPDATER_HPP_

#include <objects/mla/mla_client.hpp>
#include <objtools/edit/pubmed_updater.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


class NCBI_XOBJEDIT_EXPORT CMLAUpdater : public IPubmedUpdater
{
public:
    CMLAUpdater();
    bool Init() override;
    void Fini() override;
    TEntrezId CitMatch(const CPub&) override;
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;
    CRef<CTitle_msg_list> GetTitle(const CTitle_msg&) override;

    void SetClient(CMLAClient*);
private:
    CRef<CMLAClient> m_mlac;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // _MLA_UPDATER_HPP_
