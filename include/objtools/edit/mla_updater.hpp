#ifndef _MLA_UPDATER_HPP_
#define _MLA_UPDATER_HPP_

#include <objects/mla/mla_client.hpp>
#include <objtools/edit/pubmed_updater.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

class NCBI_XOBJEDIT_EXPORT CMLAUpdaterBase : public IPubmedUpdater
{
public:
    CMLAUpdaterBase(bool bNorm);
    bool       Init() override;
    void       Fini() override;
    TEntrezId  CitMatch(const CPub&, EPubmedError* = nullptr) override;
    TEntrezId  CitMatch(const SCitMatch&, EPubmedError* = nullptr) override;
    CRef<CPub> x_GetPub(TEntrezId pmid, EPubmedError*);
    string     GetTitle(const string&) override;

    void SetClient(CMLAClient*);

private:
    CRef<CMLAClient> m_mlac;
    bool             m_bNorm;
};

class NCBI_XOBJEDIT_EXPORT CMLAUpdaterWithCache : public CMLAUpdaterBase
{
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;

public:
    CMLAUpdaterWithCache(bool bNorm = false) :
        CMLAUpdaterBase(bNorm) {}
    void ReportStats(std::ostream&);
    void ClearCache();

private:
    map<TEntrezId, CConstRef<CPub>> m_cache;
    size_t                     m_num_requests = 0;
    size_t                     m_cache_hits   = 0;
};

class NCBI_XOBJEDIT_EXPORT CMLAUpdater : public CMLAUpdaterWithCache
{
public:
    CMLAUpdater(bool bNorm = false) :
        CMLAUpdaterWithCache(bNorm) {}
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // _MLA_UPDATER_HPP_
