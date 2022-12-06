#ifndef _EUTILS_UPDATER_HPP_
#define _EUTILS_UPDATER_HPP_

#include <objtools/eutils/api/eutils.hpp>
#include <objtools/edit/pubmed_updater.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

class NCBI_XOBJEDIT_EXPORT CEUtilsUpdaterBase : public IPubmedUpdater
{
public:
    CEUtilsUpdaterBase(bool bNorm);
    TEntrezId  CitMatch(const CPub&, EPubmedError* = nullptr) override;
    TEntrezId  CitMatch(const SCitMatch&, EPubmedError* = nullptr) override;
    CRef<CPub> x_GetPub(TEntrezId pmid, EPubmedError*);
    string     GetTitle(const string&) override;

private:
    CRef<CEUtils_ConnContext> m_Ctx;
    bool                      m_bNorm;
};

class NCBI_XOBJEDIT_EXPORT CEUtilsUpdaterWithCache : public CEUtilsUpdaterBase
{
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;

public:
    CEUtilsUpdaterWithCache(bool bNorm = false) :
        CEUtilsUpdaterBase(bNorm) {}
    void ReportStats(std::ostream&);
    void ClearCache();

private:
    map<TEntrezId, CConstRef<CPub>> m_cache;
    size_t                     m_num_requests = 0;
    size_t                     m_cache_hits   = 0;
};

class NCBI_XOBJEDIT_EXPORT CEUtilsUpdater : public CEUtilsUpdaterWithCache
{
public:
    CEUtilsUpdater(bool bNorm = false) :
        CEUtilsUpdaterWithCache(bNorm) {}
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // _EUTILS_UPDATER_HPP_
