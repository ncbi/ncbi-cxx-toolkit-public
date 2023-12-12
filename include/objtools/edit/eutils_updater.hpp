#ifndef _EUTILS_UPDATER_HPP_
#define _EUTILS_UPDATER_HPP_

#include <objtools/eutils/api/eutils.hpp>
#include <objtools/edit/pubmed_updater.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
class CPubmed_entry;
BEGIN_SCOPE(edit)

using TPubInterceptor = std::function<void(CRef<CPub>&)>;

class NCBI_XOBJEDIT_EXPORT IPubmedUpdater
{
public:
    virtual ~IPubmedUpdater() {}
    virtual bool       Init() { return true; }
    virtual void       Fini() {}
    virtual TEntrezId  CitMatch(const CPub&, EPubmedError* = nullptr)      = 0;
    virtual TEntrezId  CitMatch(const SCitMatch&, EPubmedError* = nullptr) = 0;
    virtual CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr)     = 0;

    void SetPubInterceptor(TPubInterceptor f)
    {
        m_pub_interceptor = f;
    }

protected:
    TPubInterceptor m_pub_interceptor = nullptr;
};

class NCBI_XOBJEDIT_EXPORT CEUtilsUpdater : public IPubmedUpdater
{
public:
    enum class ENormalize { Off, On };

public:
    CEUtilsUpdater(ENormalize = ENormalize::Off);
    NCBI_DEPRECATED CEUtilsUpdater(bool norm) :
        CEUtilsUpdater(norm ? ENormalize::On : ENormalize::Off) {}

    TEntrezId  CitMatch(const CPub&, EPubmedError* = nullptr) override;
    TEntrezId  CitMatch(const SCitMatch&, EPubmedError* = nullptr) override;
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;

    // Hydra replacement using citmatch api; RW-1918,RW-1999
    static bool DoPubSearch(const std::vector<string>& query, std::vector<TEntrezId>& pmids);

protected:
    CRef<CPubmed_entry> x_GetPubmedEntry(TEntrezId pmid, EPubmedError*);
    CRef<CPub>          x_GetPub(TEntrezId pmid, EPubmedError*);

private:
    CRef<CEUtils_ConnContext> m_Ctx;
    ENormalize                m_Norm;
};

class NCBI_XOBJEDIT_EXPORT CEUtilsUpdaterWithCache : public CEUtilsUpdater
{
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;

public:
    CEUtilsUpdaterWithCache(ENormalize norm = ENormalize::Off) :
        CEUtilsUpdater(norm) {}
    void ReportStats(std::ostream&);
    void ClearCache();

private:
    map<TEntrezId, CConstRef<CPub>> m_cache;

    size_t m_num_requests = 0;
    size_t m_cache_hits   = 0;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // _EUTILS_UPDATER_HPP_
