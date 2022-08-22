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
    bool       Init() override;
    void       Fini() override;
    TEntrezId  CitMatch(const CPub&, EPubmedError* = nullptr) override;
    TEntrezId  CitMatch(const SCitMatch&, EPubmedError* = nullptr) override;
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;
    string     GetTitle(const string&) override;

    void SetClient(CMLAClient*);

private:
    CRef<CMLAClient> m_mlac;
};

class NCBI_XOBJEDIT_EXPORT CMLAUpdaterWithCache : public CMLAUpdater
{
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;

public:
    void ReportStats(std::ostream&);
    void ClearCache();

private:
    map<TEntrezId, CConstRef<CPub>> m_cache;
    size_t                     m_num_requests = 0;
    size_t                     m_cache_hits   = 0;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // _MLA_UPDATER_HPP_
