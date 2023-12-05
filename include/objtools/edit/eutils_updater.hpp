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
    enum class ENormalize { Off, On };

public:
    CEUtilsUpdaterBase(ENormalize);
    TEntrezId  CitMatch(const CPub&, EPubmedError* = nullptr) override;
    TEntrezId  CitMatch(const SCitMatch&, EPubmedError* = nullptr) override;
    CRef<CPub> x_GetPub(TEntrezId pmid, EPubmedError*);

    // Hydra replacement using citmatch api; RW-1918,RW-1999
    static bool DoPubSearch(const std::vector<string>& query, std::vector<TEntrezId>& pmids);
    static void Normalize(CPub&);

private:
    CRef<CEUtils_ConnContext> m_Ctx;
    ENormalize                m_Norm;
};

class NCBI_XOBJEDIT_EXPORT CEUtilsUpdaterWithCache : public CEUtilsUpdaterBase
{
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;

public:
    CEUtilsUpdaterWithCache(ENormalize norm = ENormalize::Off) :
        CEUtilsUpdaterBase(norm) {}
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
    CEUtilsUpdater(ENormalize norm = ENormalize::Off) :
        CEUtilsUpdaterWithCache(norm) {}
    NCBI_DEPRECATED CEUtilsUpdater(bool norm) :
        CEUtilsUpdaterWithCache(norm ? ENormalize::On : ENormalize::Off) {}
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // _EUTILS_UPDATER_HPP_
